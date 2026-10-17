#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include "guest_image_ownership.h"
#include <algorithm>
#include <cstring>
#include <bit>
namespace NativeDX12 {
namespace {
struct ReadbackScratch {
    std::vector<std::byte> color,stencil,linear,converted;
};
// Resolution runs serially on each caller thread after the GPU fence. Reuse
// staging allocations across images/draws, while retaining transactional writes.
thread_local ReadbackScratch readback_scratch;
}
GuestImageReadback::GuestImageReadback(ID3D12Device* device,GuestTextureCache::Entry& image,
    ID3D12GraphicsCommandList* list):desc_(image.desc),layout_(image.layout),stencil_layout_(image.stencil_layout) {
    if(!device||!list)throw std::invalid_argument("Null native image readback device/list");
    if(layout_.expanded_bytes_per_pixel){
        if(HasExactPackedExpansion(desc_,layout_)&&image.gpu_owned&&
           image.expanded_guest_backing.handle&&image.expanded_written_mask.handle){
            RecordExpandedBacking(device,image,list);return;
        }
        if(desc_.writable||desc_.render_target||desc_.depth_target||image.gpu_owned||
           image.sampled_snapshot.size()!=layout_.guest_bytes)
            throw std::logic_error("Expanded sampled image has no immutable guest source");
        readonly_compressed_source_=image.sampled_snapshot;return;
    }
    if(image.texture.handle->GetDesc().SampleDesc.Count!=desc_.samples){
        if(!image.cache)throw std::runtime_error("Sample backing readback has no owning cache");
        image.cache->SetBackingSamples(image,desc_.samples,list);
    }
    if(desc_.data_format==13||(desc_.data_format>=16&&desc_.data_format<=19)||desc_.samples>1||(desc_.tile_mode!=8&&desc_.tile_mode!=31)||
       (desc_.depth_target&&desc_.stencil_address&&layout_.bytes_per_pixel==2)){
        RecordTiled(device,image,list);return;
    }
    const auto desc=DX12_API_CALL(image.texture.handle->GetDesc());
    const bool reusable=image.readback_pool&&image.readback_pool.use_count()==1;
    storage_=reusable?image.readback_pool:std::make_shared<std::vector<Resource>>();
    // Allocate before recording references so allocation failure leaves no
    // commands pointing at resources belonging to a half-built object.
    for(UINT plane=0;plane<(desc_.stencil_address?2u:1u);++plane)
    for(UINT layer=0;layer<desc_.layers;++layer)for(UINT mip=0;mip<desc_.levels;++mip) {
        UINT64 total{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        DX12_API_CALL(device->GetCopyableFootprints(&desc,mip+layer*desc_.levels+plane*desc_.levels*desc_.layers,1,0,&fp,nullptr,nullptr,&total));
        if(!reusable)storage_->push_back(Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST));
        const auto index=slices_.size();
        if(index>=storage_->size()||DX12_API_CALL((*storage_)[index].handle->GetDesc()).Width<total)
            throw std::logic_error("Native readback pool has incompatible footprints");
        slices_.push_back({(*storage_)[index],fp,mip,layer,plane});
    }
    image.readback_pool=storage_;
    if(image.texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)image.texture.UavBarrier(list);
    image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    for(auto& slice:slices_) {
        D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=image.texture.handle.Get();
        source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=slice.mip+slice.layer*desc_.levels+slice.plane*desc_.levels*desc_.layers;
        D3D12_TEXTURE_COPY_LOCATION dest{};dest.pResource=slice.buffer.handle.Get();
        dest.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dest.PlacedFootprint=slice.footprint;
        DX12_API_CALL(list->CopyTextureRegion(&dest,0,0,0,&source,nullptr));
    }
}
GuestLinearImageReadback::GuestLinearImageReadback(ID3D12Device* device,GuestTextureCache::Entry& image,
    ID3D12GraphicsCommandList* list):source_(image.texture),desc_(image.desc),layout_(image.layout){
    if(!device||!list||(desc_.depth_target&&!IsGuestDepthPlaneStorage(desc_,layout_))||
       (desc_.stencil_address&&!desc_.depth_target)||desc_.samples!=1||
       layout_.block_extent!=1||!source_.handle)
        throw std::invalid_argument("Unsupported compact linear image readback");
    bytes_=uint64_t(desc_.pitch)*desc_.height*std::max(desc_.volume_depth,1u)*desc_.layers*layout_.bytes_per_pixel;
    if(!bytes_||bytes_>layout_.guest_bytes||bytes_>64*1024*1024)
        throw std::out_of_range("Invalid compact linear image download size");
    const auto texture=source_.handle->GetDesc();
    if(texture.SampleDesc.Count!=1)throw std::invalid_argument("Compact linear readback requires single-sample backing");
    if(layout_.expanded_bytes_per_pixel){
        if(!HasExactPackedExpansion(desc_,layout_)||!image.gpu_owned||
           !image.expanded_guest_backing.handle||!image.expanded_written_mask.handle||
           layout_.guest_bytes>64*1024*1024||
           image.expanded_guest_backing.handle->GetDesc().Width<layout_.guest_bytes)
            throw std::invalid_argument("Compact expanded image has no exact packed GPU source");
        packed_source_=image.expanded_guest_backing;
        packed_readback_=Buffer(device,layout_.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const auto buffer=packed_readback_.handle->GetDesc();
        allocation_bytes_=device->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
        if(!allocation_bytes_||allocation_bytes_==UINT64_MAX)
            throw std::runtime_error("Invalid expanded compact readback allocation size");
        if(image.expanded_guest_backing.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            image.expanded_guest_backing.UavBarrier(list);
        image.expanded_guest_backing.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        DX12_API_CALL(list->CopyBufferRegion(packed_readback_.handle.Get(),0,packed_source_.handle.Get(),0,layout_.guest_bytes));
        image.pending=true;return;
    }
    slices_.reserve(desc_.layers);
    for(UINT layer=0;layer<desc_.layers;++layer){
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 total{},row_bytes{};
        DX12_API_CALL(device->GetCopyableFootprints(&texture,layer*desc_.levels,1,0,&fp,nullptr,&row_bytes,&total));
        if(!fp.Footprint.Width||row_bytes%fp.Footprint.Width)
            throw std::invalid_argument("Invalid compact image native plane footprint");
        const auto native_bytes=row_bytes/fp.Footprint.Width;
        // Guest D16/S8 uses a native D32/S8 allocation. DownloadImageMemory
        // still emits only compact guest D16 depth bytes, never stencil.
        const bool widened_depth=desc_.depth_target&&desc_.stencil_address&&
            layout_.bytes_per_pixel==2&&native_bytes==4;
        if(native_bytes!=layout_.HostBytesPerPixel()&&!widened_depth)
            throw std::invalid_argument("Compact image native/guest plane stride mismatch");
        slices_.push_back({Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST),fp,layer,UINT(native_bytes)});
        const auto buffer=slices_.back().buffer.handle->GetDesc();
        const auto allocation=device->GetResourceAllocationInfo(0,1,&buffer);
        if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX||
           allocation_bytes_>UINT64_MAX-allocation.SizeInBytes)
            throw std::runtime_error("Invalid linear readback allocation size");
        allocation_bytes_+=allocation.SizeInBytes;
    }
    if(image.texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)image.texture.UavBarrier(list);
    image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    for(const auto& slice:slices_){
        D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=source_.handle.Get();
        source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=slice.layer*desc_.levels;
        D3D12_TEXTURE_COPY_LOCATION dest{};dest.pResource=slice.buffer.handle.Get();
        dest.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dest.PlacedFootprint=slice.footprint;
        DX12_API_CALL(list->CopyTextureRegion(&dest,0,0,0,&source,nullptr));
    }
    image.pending=true;
}
void GuestLinearImageReadback::Resolve(uint64_t completed,std::span<std::byte> guest){
    if(!fence_||completed<fence_)throw std::logic_error("Compact linear image readback still in flight");
    if(guest.size()<bytes_)throw std::invalid_argument("Short compact linear image backing");
    auto& result=readback_scratch.color;result.assign(guest.begin(),guest.begin()+size_t(bytes_));
    const auto physical=layout_.bytes_per_pixel;
    if(packed_readback_.handle){
        void* mapped{};const D3D12_RANGE range{0,SIZE_T(layout_.guest_bytes)};
        Check(DX12_API_CALL(packed_readback_.handle->Map(0,&range,&mapped)));
        struct Unmap {ID3D12Resource* resource;~Unmap(){const D3D12_RANGE empty{};
            DX12_API_CALL(resource->Unmap(0,&empty));}} unmap{packed_readback_.handle.Get()};
        const auto depth=std::max(desc_.volume_depth,1u);
        for(UINT layer=0;layer<desc_.layers;++layer)for(UINT z=0;z<depth;++z)
        for(UINT row=0;row<desc_.height;++row)for(UINT x=0;x<desc_.width;++x){
            const auto offset=GuestPixelOffset(desc_,layout_,0,desc_.volume_depth?z:layer,x,row);
            auto* to=result.data()+((uint64_t(layer)*depth+z)*desc_.height+row)*desc_.pitch*physical+uint64_t(x)*physical;
            memcpy(to,static_cast<const std::byte*>(mapped)+offset,physical);
        }
        memcpy(guest.data(),result.data(),result.size());return;
    }
    for(const auto& slice:slices_){void* mapped{};
        const auto host=slice.native_bytes;
        const D3D12_RANGE range{0,SIZE_T(slice.buffer.handle->GetDesc().Width)};
        Check(DX12_API_CALL(slice.buffer.handle->Map(0,&range,&mapped)));
        struct Unmap {ID3D12Resource* resource;~Unmap(){const D3D12_RANGE empty{};
            DX12_API_CALL(resource->Unmap(0,&empty));}} unmap{slice.buffer.handle.Get()};
        const auto depth=std::max(desc_.volume_depth,1u);
        for(UINT z=0;z<depth;++z)for(UINT row=0;row<desc_.height;++row)for(UINT x=0;x<desc_.width;++x){
            const auto* from=static_cast<const std::byte*>(mapped)+slice.footprint.Offset+
                (uint64_t(z)*desc_.height+row)*slice.footprint.Footprint.RowPitch+uint64_t(x)*host;
            auto* to=result.data()+((uint64_t(slice.layer)*depth+z)*desc_.height+row)*desc_.pitch*physical+uint64_t(x)*physical;
            if(desc_.depth_target&&physical==2&&host==4){
                float value{};memcpy(&value,from,4);
                // Match the GPU tiler's saturate before UNORM16 conversion;
                // a native float NaN must not reach an undefined integer cast.
                const auto normalized=value>0.f?std::min(value,1.f):0.f;
                const auto word=uint16_t(normalized*65535.f+0.5f);
                memcpy(to,&word,2);
            }else if(host==2&&(desc_.data_format==18||desc_.data_format==19)){
                uint16_t word;memcpy(&word,from,2);
                if(desc_.data_format==18)word=uint16_t((word<<1)|(word>>15));
                else word=uint16_t(((word&15u)<<12)|((word&240u)<<4)|((word&3840u)>>4)|((word&61440u)>>12));
                memcpy(to,&word,2);
            }else memcpy(to,from,physical);
        }
    }
    memcpy(guest.data(),result.data(),result.size());
}
void GuestImageReadback::RecordExpandedBacking(ID3D12Device* device,GuestTextureCache::Entry& image,
    ID3D12GraphicsCommandList* list){
    const auto mask_offset=(layout_.guest_bytes+3)&~uint64_t(3);
    const auto bytes=mask_offset+GuestTiler::WrittenBytes(layout_.guest_bytes);
    const bool reusable=image.readback_pool&&image.readback_pool.use_count()==1;
    storage_=reusable?image.readback_pool:std::make_shared<std::vector<Resource>>();
    if(!reusable)storage_->push_back(Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST));
    if(storage_->size()!=1||(*storage_)[0].handle->GetDesc().Width<bytes)
        throw std::logic_error("Expanded packed readback pool footprint mismatch");
    tiled_planes_.push_back({(*storage_)[0],mask_offset,0});
    expanded_backing_=image.expanded_guest_backing;expanded_mask_=image.expanded_written_mask;
    image.readback_pool=storage_;
    image.expanded_guest_backing.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    image.expanded_written_mask.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    DX12_API_CALL(list->CopyBufferRegion((*storage_)[0].handle.Get(),0,
        image.expanded_guest_backing.handle.Get(),0,layout_.guest_bytes));
    DX12_API_CALL(list->CopyBufferRegion((*storage_)[0].handle.Get(),mask_offset,
        image.expanded_written_mask.handle.Get(),0,GuestTiler::WrittenBytes(layout_.guest_bytes)));
}
void GuestImageReadback::RecordTiled(ID3D12Device* device,GuestTextureCache::Entry& image,
                                    ID3D12GraphicsCommandList* list){
    const auto resource_desc=image.texture.handle->GetDesc();
    const bool reusable=image.readback_pool&&image.readback_pool.use_count()==1;
    storage_=reusable?image.readback_pool:std::make_shared<std::vector<Resource>>();
    if(!image.tiler)image.tiler=std::make_shared<GuestTiler>(device);
    if(desc_.samples>1&&!image.multisample)image.multisample=std::make_shared<GuestMultisample>(device);
    struct Linear {size_t index;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;UINT mip,layer,plane,bytes;};
    struct Plane {size_t output,mask,readback;uint64_t mask_offset;};
    std::vector<Linear> linear;std::vector<Plane> planes;
    size_t next{};
    const auto allocate=[&](uint64_t bytes,bool uav,D3D12_HEAP_TYPE heap){
        const auto index=next++;
        if(!reusable)storage_->push_back(uav?GuestTiler::Scratch(device,bytes):
            Buffer(device,(bytes+3)&~uint64_t(3),heap,D3D12_RESOURCE_STATE_COPY_DEST));
        if(index>=storage_->size()||(*storage_)[index].handle->GetDesc().Width<bytes)
            throw std::logic_error("Native GPU retile pool footprint mismatch");
        return index;
    };
    const auto num_planes=desc_.stencil_address?2u:1u;
    for(UINT plane=0;plane<num_planes;++plane){
        const auto& physical=plane?stencil_layout_:layout_;
        const auto output=allocate(physical.guest_bytes,true,D3D12_HEAP_TYPE_DEFAULT);
        const auto mask=allocate(GuestTiler::WrittenBytes(physical.guest_bytes),true,D3D12_HEAP_TYPE_DEFAULT);
        const auto mask_offset=(physical.guest_bytes+3)&~uint64_t(3);
        const auto readback=allocate(mask_offset+GuestTiler::WrittenBytes(physical.guest_bytes),false,D3D12_HEAP_TYPE_READBACK);
        planes.push_back({output,mask,readback,mask_offset});
        for(UINT layer=0;layer<desc_.layers;++layer)for(UINT mip=0;mip<desc_.levels;++mip){
            UINT64 total{},row_bytes{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
            const UINT sub=mip+layer*desc_.levels+plane*desc_.levels*desc_.layers;
            if(desc_.samples>1){fp=GuestMultisample::Footprint(desc_,physical);
                row_bytes=uint64_t(desc_.width)*physical.HostBytesPerPixel();
                total=GuestMultisample::LinearBytes(desc_,fp);}
            else DX12_API_CALL(device->GetCopyableFootprints(&resource_desc,sub,1,0,&fp,nullptr,&row_bytes,&total));
            const auto width=(std::max(desc_.width>>mip,1u)+physical.block_extent-1)/physical.block_extent;
            if(!width||row_bytes%width)throw std::logic_error("Native GPU retile numeric footprint mismatch");
            linear.push_back({allocate(total,desc_.samples>1,D3D12_HEAP_TYPE_DEFAULT),fp,mip,layer,plane,UINT(row_bytes/width)});
        }
    }
    if(reusable&&next!=storage_->size())throw std::logic_error("Native GPU retile pool shape mismatch");
    tiler_leases_.reserve(linear.size());tiled_planes_.reserve(num_planes);
    multisample_leases_.reserve(desc_.samples>1?linear.size():0);
    const auto stencil_desc=desc_.stencil_address?DescribeGuestStencil(desc_):GuestTextureDesc{};
    for(const auto& slice:linear){
        auto& input=(*storage_)[slice.index];
        const auto& plane=planes[slice.plane];
        tiler_leases_.push_back(image.tiler->Prepare(input,(*storage_)[plane.output],(*storage_)[plane.mask],
            slice.plane?stencil_desc:desc_,slice.plane?stencil_layout_:layout_,slice.mip,
            desc_.volume_depth?0:slice.layer,slice.fp,slice.bytes,true,slice.mip==0&&slice.layer==0));
        if(desc_.samples>1)multisample_leases_.push_back(image.multisample->Prepare(
            image.texture,input,desc_,layout_,slice.layer,slice.plane,false));
    }
    // Every resource, descriptor, PSO and tiler lease exists before the first
    // command references it. Failure above leaves the recording list untouched.
    image.readback_pool=storage_;
    if(image.texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)image.texture.UavBarrier(list);
    for(size_t index=0;index<linear.size();++index){
        const auto& slice=linear[index];auto& input=(*storage_)[slice.index];
        if(desc_.samples>1)multisample_leases_[index]->Record(list,image.texture,input);
        else{
            image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            input.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=image.texture.handle.Get();
            source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex=slice.mip+slice.layer*desc_.levels+slice.plane*desc_.levels*desc_.layers;
            D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=input.handle.Get();
            destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=slice.fp;
            DX12_API_CALL(list->CopyTextureRegion(&destination,0,0,0,&source,nullptr));
        }
        const auto& plane=planes[slice.plane];
        tiler_leases_[index]->Record(list,input,(*storage_)[plane.output],(*storage_)[plane.mask]);
    }
    for(UINT plane=0;plane<num_planes;++plane){
        const auto& indices=planes[plane];const auto& physical=plane?stencil_layout_:layout_;
        auto& output=(*storage_)[indices.output];auto& mask=(*storage_)[indices.mask];
        auto& readback=(*storage_)[indices.readback];
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);mask.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        DX12_API_CALL(list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,physical.guest_bytes));
        DX12_API_CALL(list->CopyBufferRegion(readback.handle.Get(),indices.mask_offset,mask.handle.Get(),0,GuestTiler::WrittenBytes(physical.guest_bytes)));
        tiled_planes_.push_back({readback,indices.mask_offset,plane});
    }
}
void GuestImageReadback::Submitted(uint64_t fence) {
    if(!fence||fence_)throw std::invalid_argument("Invalid native image readback retirement");fence_=fence;
}
void GuestImageReadback::Resolve(uint64_t completed,std::span<std::byte> guest,std::span<std::byte> stencil,uint32_t plane_mask) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Readback,"validate");
    diagnostic.Detail("address=%llx size=%ux%u tile=%u bytes=%llu slices=%zu",
        (unsigned long long)desc_.address,desc_.width,desc_.height,desc_.tile_mode,
        (unsigned long long)layout_.guest_bytes,slices_.size());
    if(!fence_||completed<fence_)throw std::logic_error("Native image readback still in flight");
    if(plane_mask&~3u)throw std::invalid_argument("Invalid native image readback plane mask");
    plane_mask&=desc_.stencil_address?3u:1u;
    if(!plane_mask)throw std::invalid_argument("Native image readback selects no present plane");
    if((plane_mask&1)&&guest.size()<layout_.guest_bytes)throw std::invalid_argument("Native image readback guest backing too short");
    if(!readonly_compressed_source_.empty()){
        memcpy(guest.data(),readonly_compressed_source_.data(),readonly_compressed_source_.size());return;
    }
    // Build the complete result transactionally: failed mapping/decoding cannot
    // leave a partially updated guest texture.
    auto& result=readback_scratch.color;result.clear();
    if(plane_mask&1){result.resize(size_t(layout_.guest_bytes));memcpy(result.data(),guest.data(),result.size());}
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    auto& stencil_result=readback_scratch.stencil;stencil_result.clear();
    if(plane_mask&2){stencil_desc=DescribeGuestStencil(desc_);stencil_layout=stencil_layout_;
        if(stencil.size()<stencil_layout.guest_bytes)throw std::invalid_argument("Short guest stencil readback");
        stencil_result.resize(size_t(stencil_layout.guest_bytes));
        memcpy(stencil_result.data(),stencil.data(),stencil_result.size());}
    if(!tiled_planes_.empty()){
        diagnostic.Stage("gpu-retile-mask-validation");
        for(const auto& plane:tiled_planes_){
            if(!(plane_mask&(1u<<plane.plane)))continue;
            const auto& physical=plane.plane?stencil_layout_:layout_;
            auto& destination=plane.plane?stencil_result:result;
            void* mapped{};D3D12_RANGE range{0,SIZE_T(plane.mask_offset+GuestTiler::WrittenBytes(physical.guest_bytes))};
            Check(DX12_API_CALL(plane.buffer.handle->Map(0,&range,&mapped)));
            struct Unmap {
                ID3D12Resource* buffer;
                ~Unmap(){const D3D12_RANGE empty{};DX12_API_CALL(buffer->Unmap(0,&empty));}
            } unmap{plane.buffer.handle.Get()};
            const auto* data=static_cast<const std::byte*>(mapped);
            uint64_t written{};
            for(uint64_t offset=0;offset<GuestTiler::WrittenBytes(physical.guest_bytes);offset+=4){
                uint32_t bits{};memcpy(&bits,data+plane.mask_offset+offset,4);written+=std::popcount(bits);
            }
            if(written!=physical.logical_bytes)throw std::runtime_error("Native GPU retile lost logical contents: expected="+
                std::to_string(physical.logical_bytes)+" written="+std::to_string(written));
            for(uint64_t base=0;base<physical.guest_bytes;base+=32){
                uint32_t bits{};memcpy(&bits,data+plane.mask_offset+(base/32)*4,4);
                for(uint32_t group=0;group<8;++group){
                    const auto position=base+group*4;
                    const auto mask=(bits>>(group*4))&15u;
                    if(mask==15&&position+4<=physical.guest_bytes)memcpy(destination.data()+position,data+position,4);
                    else for(uint32_t byte=0;byte<4;++byte)if((mask&(1u<<byte))&&position+byte<physical.guest_bytes)
                        destination[position+byte]=data[position+byte];
                }
            }
        }
    }
    diagnostic.Stage(tiled_planes_.empty()?"cpu-linear-copy":"commit-gpu-retile");
    for(auto& slice:slices_) {
        if(!(plane_mask&(1u<<slice.plane)))continue;
        const auto width=(std::max(desc_.width>>slice.mip,1u)+layout_.block_extent-1)/layout_.block_extent,
            height=(std::max(desc_.height>>slice.mip,1u)+layout_.block_extent-1)/layout_.block_extent;
        const uint64_t row_bytes=uint64_t(width)*(slice.plane?1:desc_.stencil_address?4:layout_.bytes_per_pixel);
        auto& linear=readback_scratch.linear;linear.resize(size_t(row_bytes*height));void* mapped{};
        D3D12_RANGE range{SIZE_T(slice.footprint.Offset),SIZE_T(DX12_API_CALL(slice.buffer.handle->GetDesc()).Width)};
        Check(DX12_API_CALL(slice.buffer.handle->Map(0,&range,&mapped)));
        for(UINT z=0;z<slice.footprint.Footprint.Depth;++z) {
            for(UINT row=0;row<height;++row)memcpy(linear.data()+row*row_bytes,
                static_cast<const std::byte*>(mapped)+slice.footprint.Offset+(uint64_t(z)*height+row)*slice.footprint.Footprint.RowPitch,size_t(row_bytes));
            if(slice.plane)EncodeGuestSubresource(stencil_desc,stencil_layout,stencil_result,linear,slice.mip,slice.layer);
            else if(desc_.stencil_address&&layout_.bytes_per_pixel==2){
                auto& converted=readback_scratch.converted;converted.resize(uint64_t(width)*height*2);
                for(size_t i=0;i<converted.size()/2;++i){float f;memcpy(&f,linear.data()+i*4,4);
                    const auto v=uint16_t(std::clamp(f,0.f,1.f)*65535.f+0.5f);memcpy(converted.data()+i*2,&v,2);}
                EncodeGuestSubresource(desc_,layout_,result,converted,slice.mip,slice.layer);
            }else EncodeGuestSubresource(desc_,layout_,result,linear,slice.mip,desc_.volume_depth?z:slice.layer);
        }
        D3D12_RANGE empty{};DX12_API_CALL(slice.buffer.handle->Unmap(0,&empty));
    }
    diagnostic.Stage("commit-guest-memory");
    if(!result.empty())memcpy(guest.data(),result.data(),result.size());
    if(!stencil_result.empty())memcpy(stencil.data(),stencil_result.data(),stencil_result.size());
}
}
