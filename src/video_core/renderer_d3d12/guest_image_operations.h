// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
namespace NativeDX12 {
// Buffer shaders operate on the whole PS4 allocation. RTV clears require a
// padding-free allocation; copies can preserve padding through physical backing.
inline bool WholeGuestImage(const GuestTextureCache::Entry& e) {
    return e.layout.block_extent==1&&e.layout.logical_bytes==e.layout.guest_bytes&&
        !e.desc.stencil_address&&!e.desc.volume_depth&&!e.desc.one_dimensional;
}
inline bool GuestImageCopyCompatible(const GuestTextureCache::Entry& src,
                                     const GuestTextureCache::Entry& dst) {
    if(&src==&dst||src.desc.stencil_address||dst.desc.stencil_address||
       src.desc.volume_depth||dst.desc.volume_depth||src.desc.one_dimensional||dst.desc.one_dimensional||
       src.layout.block_extent!=1||dst.layout.block_extent!=1||
       src.layout.guest_bytes!=dst.layout.guest_bytes||src.layout.logical_bytes!=dst.layout.logical_bytes)return false;
    auto a=src.desc,b=dst.desc;a.address=b.address=0;
    a.render_target=b.render_target=false;
    a.depth_target=b.depth_target=false;
    a.storage_height=b.storage_height=0;a.pow2_padding=b.pow2_padding=false;
    a.first_mip=b.first_mip=a.first_layer=b.first_layer=0;a.view_layers=b.view_layers=1;
    a.swizzle=b.swizzle={4,5,6,7};
    if(a.data_format==10&&(a.number_format==0||a.number_format==9)&&(b.number_format==0||b.number_format==9))a.number_format=b.number_format=0;
    if(a!=b)return false;
    if(src.layout.mips.size()!=dst.layout.mips.size())return false;
    for(size_t i=0;i<src.layout.mips.size();++i){const auto& x=src.layout.mips[i];const auto& y=dst.layout.mips[i];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||x.macro!=y.macro)return false;}
    const auto x=src.texture.handle->GetDesc(),y=dst.texture.handle->GetDesc();
    const auto family=[](DXGI_FORMAT f){
        if(f==DXGI_FORMAT_R32_TYPELESS||f==DXGI_FORMAT_R32_FLOAT||f==DXGI_FORMAT_D32_FLOAT)return DXGI_FORMAT_R32_TYPELESS;
        if(f==DXGI_FORMAT_R8G8B8A8_TYPELESS||f==DXGI_FORMAT_R8G8B8A8_UNORM||f==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        return f;};
    return x.Dimension==y.Dimension&&x.Width==y.Width&&x.Height==y.Height&&
        x.DepthOrArraySize==y.DepthOrArraySize&&x.MipLevels==y.MipLevels&&
        family(x.Format)==family(y.Format)&&x.SampleDesc.Count==y.SampleDesc.Count&&
        x.SampleDesc.Quality==y.SampleDesc.Quality;
}
inline void RecordGuestImageCopy(GuestTextureCache::Entry& src,
                                 GuestTextureCache::Entry& dst,ID3D12GraphicsCommandList* list,bool backing_prepared=false) {
    if(!GuestImageCopyCompatible(src,dst))throw std::invalid_argument("Incompatible guest image copy");
    if(!WholeGuestImage(src)&&!backing_prepared)throw std::invalid_argument("Padded guest image copy requires source padding backing");
    src.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    dst.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyResource(dst.texture.handle.Get(),src.texture.handle.Get()));
    src.pending=dst.pending=true;
}
inline bool GuestImageClearCompatible(const GuestTextureCache::Entry& e) {
    return WholeGuestImage(e)&&e.desc.render_target&&!e.desc.depth_target&&
        !e.desc.first_mip&&!e.desc.first_layer&&e.desc.view_layers==e.desc.layers&&
        (e.desc.number_format==0||e.desc.number_format==1||e.desc.number_format==7)&&
        e.desc.swizzle==std::array<uint32_t,4>{4,5,6,7};
}
// The returned CPU descriptor heap must survive until submission retires.
inline ComPtr<ID3D12DescriptorHeap> RecordGuestImageClear(ID3D12Device* device,
    GuestTextureCache::Entry& e,ID3D12GraphicsCommandList* list,const float (&color)[4]) {
    if(!GuestImageClearCompatible(e))throw std::invalid_argument("Incompatible guest image clear");
    D3D12_DESCRIPTOR_HEAP_DESC desc{};desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;desc.NumDescriptors=e.desc.levels;
    ComPtr<ID3D12DescriptorHeap> heap;Check(device->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&heap)));
    auto handle=heap->GetCPUDescriptorHandleForHeapStart();
    const auto step=device->GetDescriptorHandleIncrementSize(desc.Type);
    e.texture.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
    for(uint32_t mip=0;mip<e.desc.levels;++mip){
        auto view=e.Rtv(mip);device->CreateRenderTargetView(e.texture.handle.Get(),&view,handle);
        DX12_API_CALL(list->ClearRenderTargetView(handle,color,0,nullptr));handle.ptr+=step;
    }
    e.pending=true;return heap;
}
}
