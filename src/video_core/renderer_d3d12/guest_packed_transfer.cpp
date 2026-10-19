// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "guest_image_operations.h"

namespace NativeDX12 {
namespace {
uint64_t BufferAllocationBytes(ID3D12Device* device,uint64_t bytes){
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width=(bytes+3)&~uint64_t(3);desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
    desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const auto info=device->GetResourceAllocationInfo(0,1,&desc);
    if(!info.SizeInBytes||info.SizeInBytes==UINT64_MAX)throw std::runtime_error("Invalid packed transfer buffer allocation");
    return info.SizeInBytes;
}
struct TransferFootprint {D3D12_PLACED_SUBRESOURCE_FOOTPRINT value;UINT64 bytes;};
TransferFootprint Footprint(ID3D12Device* device,const D3D12_RESOURCE_DESC& texture,
    const GuestTextureDesc& desc,const GuestTextureLayout& layout,UINT subresource){
    if(desc.samples>1){const auto fp=GuestMultisample::Footprint(desc,layout);return {fp,GuestMultisample::LinearBytes(desc,fp)};}
    TransferFootprint result{};device->GetCopyableFootprints(&texture,subresource,1,0,&result.value,nullptr,nullptr,&result.bytes);
    return result;
}
}
class GuestPackedTransfer {
    struct Slice {
        Resource source_linear,destination_linear;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT source_footprint,destination_footprint;
        UINT subresource;
        std::shared_ptr<GuestTiler::Lease> retile,detile;
        std::shared_ptr<GuestMultisample::Lease> read_samples,write_samples;
    };
    Resource source_,destination_,raw_,mask_;
    bool inherited_{};
    std::vector<Slice> slices_;
public:
    std::vector<Resource> retired;
    std::shared_ptr<std::vector<Resource>> retired_readback;
    std::shared_ptr<void> clear_lease;
    std::vector<std::shared_ptr<GuestMultisample::CopyLease>> clear_sample_copies;
    static uint64_t ScratchBytes(ID3D12Device* device,const Resource& source,
        const D3D12_RESOURCE_DESC& destination,const GuestTextureDesc& a,const GuestTextureLayout& al,
        const GuestTextureDesc& b,const GuestTextureLayout& bl,bool inherited){
        return ScratchBytes(device,source.handle->GetDesc(),destination,a,al,b,bl,inherited);
    }
    static uint64_t ScratchBytes(ID3D12Device* device,const D3D12_RESOURCE_DESC& original,
        const D3D12_RESOURCE_DESC& destination,const GuestTextureDesc& a,const GuestTextureLayout& al,
        const GuestTextureDesc& b,const GuestTextureLayout& bl,bool inherited){
        uint64_t bytes=inherited?0:BackingBytes(device,al);
        for(UINT sub=0;sub<a.layers*a.levels;++sub){
            if(!inherited)bytes+=BufferAllocationBytes(device,Footprint(device,original,a,al,sub).bytes);
            bytes+=BufferAllocationBytes(device,Footprint(device,destination,b,bl,sub).bytes);
        }
        return bytes;
    }
    static uint64_t BackingBytes(ID3D12Device* device,const GuestTextureLayout& layout){
        return BufferAllocationBytes(device,layout.guest_bytes)+
            BufferAllocationBytes(device,GuestTiler::WrittenBytes(layout.guest_bytes));
    }
    const Resource& Backing()const{return raw_;}
    const Resource& WrittenMask()const{return mask_;}
    GuestPackedTransfer(ID3D12Device* device,Resource& source,Resource destination,
        const GuestTextureDesc& a,const GuestTextureLayout& al,const GuestTextureDesc& b,const GuestTextureLayout& bl,
        GuestTiler& tiler,GuestMultisample& multisample,const Resource& backing,const Resource& written_mask):
        source_(source),destination_(std::move(destination)),
        raw_(backing.handle?backing:GuestTiler::Scratch(device,al.guest_bytes)),
        mask_(backing.handle?written_mask:GuestTiler::Scratch(device,GuestTiler::WrittenBytes(al.guest_bytes))),
        inherited_(bool(backing.handle)){
        slices_.reserve(a.layers*a.levels);const auto source_desc=source.handle->GetDesc(),destination_desc=destination_.handle->GetDesc();
        for(UINT layer=0;layer<a.layers;++layer)for(UINT mip=0;mip<a.levels;++mip){
            const auto sub=mip+layer*a.levels;
            const auto to=Footprint(device,destination_desc,b,bl,sub);
            auto& slice=slices_.emplace_back();slice.subresource=sub;
            slice.destination_footprint=to.value;
            slice.destination_linear=GuestTiler::Scratch(device,to.bytes);
            if(!inherited_){
                const auto from=Footprint(device,source_desc,a,al,sub);
                slice.source_footprint=from.value;slice.source_linear=GuestTiler::Scratch(device,from.bytes);
                slice.retile=tiler.Prepare(slice.source_linear,raw_,mask_,a,al,mip,layer,from.value,al.HostBytesPerPixel(),true,!sub);
                if(a.samples>1)slice.read_samples=multisample.Prepare(source,slice.source_linear,a,al,layer,0,false);
            }
            slice.detile=tiler.Prepare(raw_,slice.destination_linear,slice.destination_linear,b,bl,mip,layer,to.value,bl.HostBytesPerPixel(),false);
            if(b.samples>1)slice.write_samples=multisample.Prepare(destination_,slice.destination_linear,b,bl,layer,0,true);
        }
    }
    Resource Record(ID3D12GraphicsCommandList* list,Resource& source){
        if(!inherited_&&source.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)source.UavBarrier(list);
        if(!inherited_)for(auto& slice:slices_){
            if(slice.read_samples)slice.read_samples->Record(list,source,slice.source_linear);
            else{
                source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);slice.source_linear.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
                D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=source.handle.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=slice.subresource;
                D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=slice.source_linear.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=slice.source_footprint;
                DX12_API_CALL(list->CopyTextureRegion(&to,0,0,0,&from,nullptr));
            }
            slice.retile->Record(list,slice.source_linear,raw_,mask_);
        }
        if(!inherited_)raw_.UavBarrier(list);
        for(auto& slice:slices_){
            slice.detile->Record(list,raw_,slice.destination_linear,slice.destination_linear);
            slice.destination_linear.UavBarrier(list);
            if(slice.write_samples)slice.write_samples->Record(list,destination_,slice.destination_linear);
            else{
                slice.destination_linear.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);destination_.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
                D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=slice.destination_linear.handle.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=slice.destination_footprint;
                D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=destination_.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.SubresourceIndex=slice.subresource;
                DX12_API_CALL(list->CopyTextureRegion(&to,0,0,0,&from,nullptr));
            }
        }
        destination_.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return destination_;
    }
};
void GuestTextureCache::ClearRawStorageImage(Entry& entry,const float (&color)[4],
    ID3D12GraphicsCommandList* list,uint64_t completed){
    const bool broadcast=GuestConvertedMultisampleClearCompatible(entry,color);
    if(!list||(!GuestConvertedStorageClearCompatible(entry)&&!broadcast))
        throw std::invalid_argument("Incompatible converted guest storage image clear");
    auto destination_desc=entry.desc;
    if(broadcast)destination_desc.samples=1;
    const auto destination_layout=broadcast?LayoutGuestTexture(destination_desc):entry.layout;
    auto desc=GuestImageClearView(destination_desc);desc.native_raw32=false;
    const bool packed16=desc.data_format>=16&&desc.data_format<=19;
    const bool packed10=desc.data_format==9;
    const bool packed=packed16||packed10;
    const bool srgb=desc.data_format==10&&desc.number_format==9;
    auto clear_desc=desc;
    clear_desc.writable=true;clear_desc.render_target=false;
    if(srgb){clear_desc.writable=false;clear_desc.swizzle={4,5,6,7};}
    if(packed){clear_desc.data_format=14;
        clear_desc.number_format=packed10&&(desc.number_format==4||desc.number_format==5)?desc.number_format:7;}
    const auto clear_layout=LayoutGuestTexture(clear_desc);
    // The synthetic packed clear source has the single-sample guest layout.
    // Reusing the destination MSAA slice size here would retile layers into a
    // different allocation before broadcasting the resulting word.
    auto layout=packed?destination_layout:clear_layout;
    if(packed){layout.format=clear_layout.format;layout.expanded_bytes_per_pixel=16;}
    auto destination=entry.texture.handle->GetDesc();
    if(broadcast){destination.SampleDesc={1,0};destination.Alignment=0;
        destination.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;}
    auto source=destination;
    const auto family=[](UINT format){switch(format){
        case 1:return DXGI_FORMAT_R8_TYPELESS;case 2:return DXGI_FORMAT_R16_TYPELESS;
        case 3:return DXGI_FORMAT_R8G8_TYPELESS;case 4:return DXGI_FORMAT_R32_TYPELESS;
        case 5:return DXGI_FORMAT_R16G16_TYPELESS;case 10:return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case 11:return DXGI_FORMAT_R32G32_TYPELESS;case 12:return DXGI_FORMAT_R16G16B16A16_TYPELESS;
        case 13:case 14:return DXGI_FORMAT_R32G32B32A32_TYPELESS;default:return DXGI_FORMAT_UNKNOWN;}};
    source.Format=packed?DXGI_FORMAT_R32G32B32A32_TYPELESS:family(clear_desc.data_format);
    if(source.Format==DXGI_FORMAT_UNKNOWN)source.Format=clear_layout.format;
    source.Alignment=0;
    source.Flags=srgb?D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{clear_layout.format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
    const auto dimension=desc.volume_depth?D3D12_FORMAT_SUPPORT1_TEXTURE3D:
        desc.one_dimensional?D3D12_FORMAT_SUPPORT1_TEXTURE1D:D3D12_FORMAT_SUPPORT1_TEXTURE2D;
    if(!(support.Support1&dimension)||(srgb?!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET):
       (!(support.Support1&D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)||
        !(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))))
        throw std::runtime_error("Converted guest clear lacks native typed UAV store support");
    const auto info=texture_allocator_.AllocationInfo(source);
    if(!info.SizeInBytes||info.SizeInBytes==UINT64_MAX||!info.Alignment)
        throw std::runtime_error("Invalid converted guest clear allocation");
    const auto scratch=GuestPackedTransfer::ScratchBytes(device_.Get(),source,destination,
        desc,layout,destination_desc,destination_layout,false);
    const auto intermediate_bytes=broadcast?texture_allocator_.AllocationInfo(destination).SizeInBytes:0;
    if(broadcast&&(!intermediate_bytes||intermediate_bytes==UINT64_MAX))
        throw std::runtime_error("Invalid converted MSAA clear intermediate allocation");
    const bool keep_backing=HasExactPackedExpansion(entry.desc,entry.layout);
    const auto backing_bytes=keep_backing?GuestPackedTransfer::BackingBytes(device_.Get(),entry.layout):0;
    if(info.SizeInBytes>UINT64_MAX-intermediate_bytes||
       scratch>UINT64_MAX-info.SizeInBytes-intermediate_bytes)
        throw std::overflow_error("Converted guest clear reservation overflow");
    Collect(completed);
    ReserveMemory(info.SizeInBytes+intermediate_bytes+scratch,completed,&entry);
    // Pressure retirement may have submitted/closed the caller's old list.
    if(recording_list_)list=recording_list_();
    if(!list)throw std::runtime_error("Converted guest clear lost its recording list");
    if(!tiler_)tiler_=std::make_shared<GuestTiler>(device_.Get());
    if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
    Entry temporary;temporary.desc=clear_desc;temporary.layout=clear_layout;
    // This internal native clear target supports 1D/3D RTVs without changing
    // the guest cache's ordinary attachment geometry restrictions.
    temporary.desc.render_target=srgb;
    temporary.texture=AllocateTexture(&entry,source,D3D12_RESOURCE_STATE_COMMON);
    std::shared_ptr<GuestStorageImageClearLease> lease;
    std::shared_ptr<GuestImageClearLease> render_lease;
    if(srgb)render_lease=std::make_shared<GuestImageClearLease>(device_.Get(),temporary);
    else lease=std::make_shared<GuestStorageImageClearLease>(device_.Get(),temporary);
    auto output=broadcast?AllocateTexture(&entry,destination,D3D12_RESOURCE_STATE_COMMON):entry.texture;
    auto transfer=std::make_shared<GuestPackedTransfer>(device_.Get(),temporary.texture,output,
        desc,layout,destination_desc,destination_layout,*tiler_,*multisample_,Resource{},Resource{});
    if(broadcast){
        transfer->clear_sample_copies.reserve(entry.desc.layers);
        for(UINT layer=0;layer<entry.desc.layers;++layer)
            transfer->clear_sample_copies.push_back(multisample_->PrepareCopy(output,entry.texture,
                entry.desc,entry.layout,layer));
    }
    if(keep_backing){
        if(entry.expanded_guest_backing.handle)transfer->retired.push_back(entry.expanded_guest_backing);
        if(entry.expanded_written_mask.handle)transfer->retired.push_back(entry.expanded_written_mask);
    }
    transfer->clear_lease=srgb?std::static_pointer_cast<void>(render_lease):std::static_pointer_cast<void>(lease);
    entry.format_transfers.reserve(entry.format_transfers.size()+1);
    entry.format_transfers.push_back(transfer);
    const auto retired_bytes=info.SizeInBytes+intermediate_bytes+scratch-backing_bytes+
        (keep_backing?entry.expanded_backing_bytes:0);
    entry.allocation_bytes+=retired_bytes;
    entry.format_retirement_bytes+=retired_bytes;
    // Retain every referenced texture, heap and scratch buffer before commands.
    // Only the existing image changes; its buffer/CPU versions stay independent.
    entry.pending=true;
    if(srgb)RecordGuestImageClear(device_.Get(),temporary,list,color,std::move(render_lease));
    else RecordGuestStorageImageClear(device_.Get(),temporary,list,color,std::move(lease));
    auto converted=transfer->Record(list,temporary.texture);
    if(broadcast){
        for(const auto& copy:transfer->clear_sample_copies)copy->Record(list,converted,entry.texture);
        entry.texture.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        // Shader storage binds seed all sample planes from this authoritative
        // native texture before use; no physical buffer version is reconciled.
    }else entry.texture=std::move(converted);
    if(keep_backing){entry.expanded_guest_backing=transfer->Backing();entry.expanded_written_mask=transfer->WrittenMask();
        entry.expanded_backing_bytes=backing_bytes;}
    entry.last_use=++use_sequence_;Diagnostics::Count(packed16?"image-clear.packed16.gpu-conversions":
        packed10?"image-clear.packed10.gpu-conversions":srgb?"image-clear.srgb.gpu-conversions":"image-clear.raw-rgba8.gpu-conversions");
}
bool GuestTextureCache::ConvertPackedBacking(Entry& entry,const GuestTextureDesc& desc,
    const GuestTextureLayout& layout,ID3D12GraphicsCommandList* list){
    const auto source_desc=entry.texture.handle->GetDesc();
    const bool inherited=entry.layout.expanded_bytes_per_pixel!=0;
    if(inherited&&(!entry.expanded_guest_backing.handle||!entry.expanded_written_mask.handle||
       !entry.expanded_backing_bytes))return false;
    const bool keep_backing=HasExactPackedExpansion(desc,layout);
    if(((desc.volume_depth||entry.desc.volume_depth||desc.one_dimensional||entry.desc.one_dimensional)&&
        !(desc.data_format==9&&entry.desc.data_format==9)&&
        !(desc.data_format>=16&&desc.data_format<=19&&desc.data_format==entry.desc.data_format&&
          desc.number_format==0&&entry.desc.number_format==0)&&
        !((desc.data_format==4||desc.data_format==10)&&(entry.desc.data_format==4||entry.desc.data_format==10)))||
       source_desc.SampleDesc.Count!=entry.desc.samples||entry.layout.guest_bytes!=layout.guest_bytes||
       entry.layout.mips.size()!=layout.mips.size())return false;
    for(size_t i=0;i<layout.mips.size();++i){const auto& a=entry.layout.mips[i];const auto& b=layout.mips[i];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)return false;
    }
    D3D12_RESOURCE_DESC target=source_desc;target.Format=layout.format;target.Alignment=0;
    if(keep_backing)target.Format=DXGI_FORMAT_R32G32B32A32_TYPELESS;
    else if(desc.UsesRaw32())target.Format=DXGI_FORMAT_R32_TYPELESS;
    else if(desc.data_format==9)target.Format=DXGI_FORMAT_R10G10B10A2_TYPELESS;
    else if(desc.writable||desc.render_target){
        if(desc.data_format==4)target.Format=DXGI_FORMAT_R32_TYPELESS;
        else if(desc.data_format==10)target.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
    }
    target.Flags=desc.writable&&desc.samples==1?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
    if(desc.render_target||desc.samples>1){
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{layout.format};Check(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)));
        if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET))return false;
        target.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if(desc.samples>1){D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS support{layout.format,desc.samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
        Check(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&support,sizeof(support)));if(!support.NumQualityLevels)return false;
    }
    const auto target_info=device_->GetResourceAllocationInfo(0,1,&target);
    if(!target_info.SizeInBytes||target_info.SizeInBytes==UINT64_MAX)return false;
    uint64_t target_bytes=target_info.SizeInBytes;
    auto storage_desc=target;storage_desc.SampleDesc={1,0};storage_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    uint64_t storage_linear_bytes{};
    if(desc.writable&&desc.samples>1){
        const auto info=device_->GetResourceAllocationInfo(0,1,&storage_desc);
        if(!info.SizeInBytes||info.SizeInBytes==UINT64_MAX)return false;
        storage_linear_bytes=GuestMultisample::LinearBytes(desc,GuestMultisample::Footprint(desc,layout));
        target_bytes+=info.SizeInBytes*desc.samples+BufferAllocationBytes(device_.Get(),storage_linear_bytes);
    }
    const auto scratch_bytes=GuestPackedTransfer::ScratchBytes(device_.Get(),entry.texture,target,entry.desc,entry.layout,desc,layout,inherited);
    const auto backing_bytes=keep_backing?(inherited?entry.expanded_backing_bytes:
        GuestPackedTransfer::BackingBytes(device_.Get(),layout)):0;
    uint64_t readback_reserve=2*((layout.guest_bytes+3)&~uint64_t(3))+2*GuestTiler::WrittenBytes(layout.guest_bytes);
    for(UINT sub=0;sub<desc.layers*desc.levels;++sub)
        readback_reserve+=Footprint(device_.Get(),target,desc,layout,sub).bytes;
    RefreshMemoryBudget();const auto resident=ResidentBytes();
    if(target_bytes>resident_budget_||scratch_bytes>resident_budget_-target_bytes||
       readback_reserve>resident_budget_-target_bytes-scratch_bytes||
       resident>resident_budget_-target_bytes-scratch_bytes-readback_reserve)return false;
    if(!tiler_)tiler_=std::make_shared<GuestTiler>(device_.Get());
    if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
    auto destination=AllocateTexture(&entry,target,D3D12_RESOURCE_STATE_COMMON);
    auto transfer=std::make_shared<GuestPackedTransfer>(device_.Get(),entry.texture,std::move(destination),
        entry.desc,entry.layout,desc,layout,*tiler_,*multisample_,
        inherited?entry.expanded_guest_backing:Resource{},inherited?entry.expanded_written_mask:Resource{});
    std::vector<Resource> storage;Resource storage_linear;
    if(desc.writable&&desc.samples>1){storage.reserve(desc.samples);
        for(UINT sample=0;sample<desc.samples;++sample)storage.push_back(AllocateTexture(&entry,storage_desc,D3D12_RESOURCE_STATE_COMMON));
        storage_linear=GuestTiler::Scratch(device_.Get(),storage_linear_bytes);
    }
    uint64_t retired_pool_bytes{};
    const auto retain=[&](const std::vector<Resource>& resources){for(const auto& resource:resources){transfer->retired.push_back(resource);retired_pool_bytes+=resource.handle->GetDesc().Width;}};
    retain(entry.upload_pool);retain(entry.tiler_input_pool);retain(entry.tiler_output_pool);
    transfer->retired_readback=entry.readback_pool;
    if(entry.readback_pool)for(const auto& resource:*entry.readback_pool)retired_pool_bytes+=resource.handle->GetDesc().Width;
    for(const auto& [count,resource]:entry.sample_backings)transfer->retired.push_back(resource);
    transfer->retired.insert(transfer->retired.end(),entry.storage_samples.begin(),entry.storage_samples.end());
    if(entry.storage_linear.handle)transfer->retired.push_back(entry.storage_linear);
    auto new_desc=desc;auto new_layout=layout;
    entry.format_transfers.reserve(entry.format_transfers.size()+1);
    const auto retired_bytes=entry.allocation_bytes-entry.format_retirement_bytes+retired_pool_bytes+scratch_bytes-
        (keep_backing&&!inherited?backing_bytes:0)+(keep_backing?0:entry.expanded_backing_bytes);
    auto converted=transfer->Record(list,entry.texture);
    entry.expanded_guest_backing=keep_backing?transfer->Backing():Resource{};
    entry.expanded_written_mask=keep_backing?transfer->WrittenMask():Resource{};
    entry.expanded_backing_bytes=backing_bytes;
    entry.format_transfers.push_back(std::move(transfer));entry.texture=std::move(converted);
    entry.desc=std::move(new_desc);entry.layout=std::move(new_layout);
    entry.allocation_bytes=target_bytes+entry.format_retirement_bytes+retired_bytes;entry.format_retirement_bytes+=retired_bytes;
    entry.readback_reserve_bytes=readback_reserve;
    entry.upload_pool.clear();entry.tiler_input_pool.clear();entry.tiler_output_pool.clear();entry.readback_pool.reset();
    entry.sample_backings.clear();entry.requested_backing_samples=0;
    entry.storage_samples=std::move(storage);entry.storage_linear=std::move(storage_linear);
    // The replacement backing owns its new storage state, including when it
    // no longer needs sample planes. Do not restore the retired old domain.
    entry.storage_retired=false;
    entry.tiler=tiler_;entry.multisample=multisample_;entry.pending=true;entry.last_use=++use_sequence_;
    entry.sampled_snapshot.clear();entry.stencil_snapshot.clear();
    Diagnostics::Count("packed-backing.gpu-conversions");return true;
}
} // namespace NativeDX12
