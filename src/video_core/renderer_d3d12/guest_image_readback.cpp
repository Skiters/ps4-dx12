#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include <algorithm>
#include <cstring>
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
void GuestImageReadback::Submitted(uint64_t fence) {
    if(!fence||fence_)throw std::invalid_argument("Invalid native image readback retirement");fence_=fence;
}
void GuestImageReadback::Resolve(uint64_t completed,std::span<std::byte> guest,std::span<std::byte> stencil) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Readback,"validate");
    diagnostic.Detail("address=%llx size=%ux%u tile=%u bytes=%llu slices=%zu",
        (unsigned long long)desc_.address,desc_.width,desc_.height,desc_.tile_mode,
        (unsigned long long)layout_.guest_bytes,slices_.size());
    if(!fence_||completed<fence_)throw std::logic_error("Native image readback still in flight");
    if(guest.size()<layout_.guest_bytes)throw std::invalid_argument("Native image readback guest backing too short");
    // Build the complete result transactionally: failed mapping/decoding cannot
    // leave a partially updated guest texture.
    auto& result=readback_scratch.color;result.resize(size_t(layout_.guest_bytes));
    memcpy(result.data(),guest.data(),result.size());
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    auto& stencil_result=readback_scratch.stencil;stencil_result.clear();
    if(desc_.stencil_address){stencil_desc=DescribeGuestStencil(desc_);stencil_layout=stencil_layout_;
        if(stencil.size()<stencil_layout.guest_bytes)throw std::invalid_argument("Short guest stencil readback");
        stencil_result.resize(size_t(stencil_layout.guest_bytes));
        memcpy(stencil_result.data(),stencil.data(),stencil_result.size());}
    diagnostic.Stage("cpu-retile");
    for(auto& slice:slices_) {
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
    memcpy(guest.data(),result.data(),result.size());
    if(!stencil_result.empty())memcpy(stencil.data(),stencil_result.data(),stencil_result.size());
}
}
