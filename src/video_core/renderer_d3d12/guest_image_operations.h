// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
#include <algorithm>
#include <bit>
namespace NativeDX12 {
// Image clears affect texels and preserve guest padding. Whole-allocation
// copies additionally require a padding-free layout or explicit backing copy.
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
    a.storage_slice_bytes=b.storage_slice_bytes=0;
    a.first_mip=b.first_mip=a.first_layer=b.first_layer=0;a.view_layers=b.view_layers=1;
    a.swizzle=b.swizzle={4,5,6,7};
    if(a.data_format==10&&(a.number_format==0||a.number_format==9)&&(b.number_format==0||b.number_format==9))a.number_format=b.number_format=0;
    if(a!=b)return false;
    if(src.layout.mips.size()!=dst.layout.mips.size())return false;
    for(size_t i=0;i<src.layout.mips.size();++i){const auto& x=src.layout.mips[i];const auto& y=dst.layout.mips[i];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||x.macro!=y.macro||x.thickness!=y.thickness)return false;}
    const auto x=src.texture.handle->GetDesc(),y=dst.texture.handle->GetDesc();
    const auto family=[](DXGI_FORMAT f){
        if(f==DXGI_FORMAT_R32_TYPELESS||f==DXGI_FORMAT_R32_FLOAT||f==DXGI_FORMAT_D32_FLOAT||
           f==DXGI_FORMAT_R32_UINT||f==DXGI_FORMAT_R32_SINT)return DXGI_FORMAT_R32_TYPELESS;
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
    const bool float_multisample=e.desc.samples>1&&e.desc.data_format==4&&e.desc.number_format==7&&
        e.layout.format==DXGI_FORMAT_R32_FLOAT&&e.texture.handle&&
        (e.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    return e.layout.block_extent==1&&!e.layout.expanded_bytes_per_pixel&&
        !e.desc.stencil_address&&(e.desc.render_target||float_multisample)&&!e.desc.depth_target&&
        (e.desc.number_format==0||e.desc.number_format==1||e.desc.number_format==7||
         (e.desc.data_format==10&&e.desc.number_format==9));
}
inline GuestTextureDesc GuestImageClearView(GuestTextureDesc desc) {
    // Pinned compute image clears cover the resource's complete subresource
    // range, independently of the mip/layer view that populated the cache.
    desc.first_mip=desc.first_layer=0;desc.view_layers=desc.layers;return desc;
}
inline std::array<float,4> GuestNativeImageClearColor(const GuestTextureDesc& desc,const float (&color)[4]){
    std::array<float,4> native{color[0],color[1],color[2],color[3]};
    // Physical RGB32 has no alpha. Its native RGBA32 backing keeps the same
    // format-defined one as ordinary detile, for float and integer views.
    if(desc.data_format==13)native[3]=desc.number_format==4||desc.number_format==5?
        std::bit_cast<float>(uint32_t(1)):1.f;
    return native;
}
inline bool GuestStorageImageClearCompatible(const GuestTextureCache::Entry& e,const float (&color)[4]) {
    const bool zero=std::all_of(color,color+4,[](float value){return std::bit_cast<uint32_t>(value)==0;});
    const bool wide=e.desc.data_format==4||e.desc.data_format==11||e.desc.data_format==14;
    const bool typed=!e.desc.UsesRaw32()&&e.desc.number_format!=9&&
        !(e.desc.data_format>=16&&e.desc.data_format<=19);
    return e.layout.block_extent==1&&!e.desc.stencil_address&&e.desc.writable&&
        !e.desc.render_target&&!e.desc.depth_target&&e.desc.samples==1&&!e.layout.expanded_bytes_per_pixel&&
        (zero||wide||typed)&&
        (e.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
}
struct GuestStorageImageClearLease {
    Resource texture;
    GuestTextureDesc view;
    DXGI_FORMAT view_format;
    Descriptors visible,cpu;
    uint32_t mips;
    GuestStorageImageClearLease(ID3D12Device* device,const GuestTextureCache::Entry& e):texture(e.texture),view(GuestImageClearView(e.desc)),view_format(e.layout.format),
        visible(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,e.desc.levels,true),
        cpu(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,e.desc.levels,false),mips(e.desc.levels){
        const auto clear_view=GuestImageClearView(e.desc);
        for(uint32_t mip=0;mip<mips;++mip){auto view=e.Uav(clear_view,mip);
            // Integer clears write the already converted guest bit pattern.
            // Use the unsigned member of the same typeless family: passing a
            // sign-extended UINT to a narrow SINT clear can saturate instead
            // of retaining the guest's negative value on native drivers.
            switch(view.Format){
            case DXGI_FORMAT_R8_SINT:view.Format=DXGI_FORMAT_R8_UINT;break;
            case DXGI_FORMAT_R8G8_SINT:view.Format=DXGI_FORMAT_R8G8_UINT;break;
            case DXGI_FORMAT_R8G8B8A8_SINT:view.Format=DXGI_FORMAT_R8G8B8A8_UINT;break;
            case DXGI_FORMAT_R16_SINT:view.Format=DXGI_FORMAT_R16_UINT;break;
            case DXGI_FORMAT_R16G16_SINT:view.Format=DXGI_FORMAT_R16G16_UINT;break;
            case DXGI_FORMAT_R16G16B16A16_SINT:view.Format=DXGI_FORMAT_R16G16B16A16_UINT;break;
            case DXGI_FORMAT_R32_SINT:view.Format=DXGI_FORMAT_R32_UINT;break;
            case DXGI_FORMAT_R32G32_SINT:view.Format=DXGI_FORMAT_R32G32_UINT;break;
            case DXGI_FORMAT_R32G32B32A32_SINT:view.Format=DXGI_FORMAT_R32G32B32A32_UINT;break;
            default:break;
            }
            DX12_API_CALL(device->CreateUnorderedAccessView(texture.handle.Get(),nullptr,&view,visible.Cpu(mip)));
            DX12_API_CALL(device->CreateUnorderedAccessView(texture.handle.Get(),nullptr,&view,cpu.Cpu(mip)));
        }
    }
};
inline bool GuestConvertedStorageClearCompatible(const GuestTextureCache::Entry& e){
    const bool rgba8=e.desc.data_format==10&&(e.desc.UsesRaw32()||e.desc.number_format==9)&&
        (e.desc.number_format==0||e.desc.number_format==1||e.desc.number_format==4||e.desc.number_format==5||e.desc.number_format==9);
    const bool packed16=e.desc.data_format>=16&&e.desc.data_format<=19&&e.desc.number_format==0;
    const bool packed10=e.desc.data_format==9&&(e.desc.UsesRawPacked10()||HasExactPackedExpansion(e.desc,e.layout))&&
        (e.desc.number_format==0||e.desc.number_format==1||e.desc.number_format==4||e.desc.number_format==5);
    const bool typed=!e.desc.UsesRaw32()&&
        (e.desc.data_format==1||e.desc.data_format==2||e.desc.data_format==3||e.desc.data_format==4||
         e.desc.data_format==5||e.desc.data_format==6||e.desc.data_format==9||e.desc.data_format==10||
         e.desc.data_format==11||e.desc.data_format==12||e.desc.data_format==13||e.desc.data_format==14);
    return (rgba8||packed16||packed10||typed)&&
        e.layout.block_extent==1&&(!e.layout.expanded_bytes_per_pixel||HasExactPackedExpansion(e.desc,e.layout))&&!e.desc.stencil_address&&
        !e.desc.depth_target&&e.desc.samples==1;
}
inline bool GuestConvertedMultisampleClearCompatible(const GuestTextureCache::Entry& e,
                                                     const float (&color)[4]){
    // Clear one typed single-sample image, pack its guest word, and broadcast
    // that exact R32 word to every native sample. Packed16 keeps only its low
    // guest halfword; its host R32 representation has zero high bits.
    // SRGB uses the existing linear-color RTV conversion before this broadcast,
    // preserving encoded RGB and linear alpha. Narrow integer inputs must fit
    // their guest components; leave undefined overflow cases to the guest shader.
    const bool rgba8=e.desc.data_format==10&&e.desc.UsesRaw32();
    const bool packed10=e.desc.data_format==9&&e.desc.UsesRawPacked10();
    const bool packed16=e.desc.data_format>=16&&e.desc.data_format<=19&&
        e.desc.writable&&!e.desc.render_target&&e.desc.number_format==0;
    if((!rgba8&&!packed10&&!packed16)||
       !GuestTextureCache::CanTransferIndependentMultisample(e.desc,e.layout)||!e.texture.handle)
        return false;
    const auto native=e.texture.handle->GetDesc();
    if(native.SampleDesc.Count!=e.desc.samples||
       (native.Format!=DXGI_FORMAT_R32_TYPELESS&&native.Format!=DXGI_FORMAT_R32_UINT)||
       !(native.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))return false;
    for(uint32_t component=0;component<4;++component){
        const float value=color[component];
        const auto bits=packed10?(component==3?2u:10u):8u;
        if(e.desc.number_format==4){
            if(std::bit_cast<uint32_t>(value)>((1u<<bits)-1u))return false;
        }else if(e.desc.number_format==5){
            const auto integer=std::bit_cast<int32_t>(value),limit=int32_t(1u<<(bits-1));
            if(integer< -limit||integer>=limit)return false;
        }else if(e.desc.number_format==0||e.desc.number_format==1||
                 (rgba8&&e.desc.number_format==9)){
            if((std::bit_cast<uint32_t>(value)&0x7f800000u)==0x7f800000u)return false;
        }else return false;
    }
    return true;
}
inline std::shared_ptr<GuestStorageImageClearLease> RecordGuestStorageImageClear(ID3D12Device* device,
    GuestTextureCache::Entry& e,ID3D12GraphicsCommandList* list,const float (&color)[4],
    std::shared_ptr<GuestStorageImageClearLease> lease={}) {
    if(!GuestStorageImageClearCompatible(e,color))throw std::invalid_argument("Incompatible guest storage image clear");
    if(!lease){
        auto& cached=e.storage_clear_descriptors;
        if(!cached||cached->texture.handle!=e.texture.handle||cached->view!=GuestImageClearView(e.desc)||cached->view_format!=e.layout.format)
            cached=std::make_shared<GuestStorageImageClearLease>(device,e);
        lease=cached;
    }
    if(lease->texture.handle!=e.texture.handle||lease->view!=GuestImageClearView(e.desc)||lease->view_format!=e.layout.format)
        throw std::invalid_argument("Mismatched prepared guest storage image clear");
    e.texture.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    // ClearUAV is a compute operation, including when the state is unchanged.
    e.texture.UavBarrier(list);
    ID3D12DescriptorHeap* heaps[]{lease->visible.Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    // Pinned VkClearValue stores the input float vector's raw union bits for
    // integer images. R32 raw storage of a guest float also needs those exact
    // bits, including signed zero. Typed float views receive the float values.
    // Typed narrow views use the same native numeric clear. Packed/raw narrow
    // representations still require positive zero until exact packing exists.
    const auto native_color=GuestNativeImageClearColor(e.desc,color);
    UINT bits[4];for(unsigned component=0;component<4;++component)
        bits[component]=std::bit_cast<uint32_t>(native_color[component]);
    if(!e.desc.UsesRaw32()&&(e.desc.number_format==4||e.desc.number_format==5)){
        // Vulkan ClearImage casts integer union values to the target component
        // width. For representable SINT values this retains two's complement;
        // UINT also retains its low bits. Keep raw R32 guest words untouched.
        const auto data=e.desc.data_format;
        const UINT mask=data==1||data==3||data==10?0xffu:
            data==2||data==5||data==12?0xffffu:UINT_MAX;
        for(auto& component:bits)component&=mask;
    }
    const bool integer=e.desc.UsesRaw32()||e.desc.number_format==4||e.desc.number_format==5||
        (e.desc.data_format>=16&&e.desc.data_format<=19);
    for(uint32_t mip=0;mip<e.desc.levels;++mip){
        if(integer)DX12_API_CALL(list->ClearUnorderedAccessViewUint(lease->visible.Gpu(mip),lease->cpu.Cpu(mip),
            e.texture.handle.Get(),bits,0,nullptr));
        else DX12_API_CALL(list->ClearUnorderedAccessViewFloat(lease->visible.Gpu(mip),lease->cpu.Cpu(mip),
            e.texture.handle.Get(),native_color.data(),0,nullptr));
    }
    e.texture.UavBarrier(list);e.pending=true;return lease;
}
inline GuestTextureCache::Entry GuestMultisampleClearPlane(const GuestTextureCache::Entry& image,UINT sample){
    GuestTextureCache::Entry plane;plane.desc=image.desc;plane.desc.samples=1;
    plane.layout=image.layout;plane.texture=image.storage_samples.at(sample);return plane;
}
inline bool GuestMultisampleStorageClearCompatible(const GuestTextureCache::Entry& image,const float (&color)[4]){
    if(image.desc.samples<2||!image.desc.writable||image.desc.render_target||image.desc.depth_target||
       image.desc.stencil_address||image.storage_samples.size()!=image.desc.samples||!image.storage_linear.handle)
        return false;
    for(UINT sample=0;sample<image.desc.samples;++sample){
        if(!image.storage_samples[sample].handle||
           !GuestStorageImageClearCompatible(GuestMultisampleClearPlane(image,sample),color))return false;
    }
    return true;
}
struct GuestMultisampleStorageClearLease {
    std::vector<std::shared_ptr<GuestStorageImageClearLease>> clears;
    std::shared_ptr<GuestMultisample::StorageLease> transfer;
};
inline std::shared_ptr<GuestMultisampleStorageClearLease> RecordGuestMultisampleStorageClear(
    ID3D12Device* device,GuestTextureCache& cache,GuestTextureCache::Entry& image,
    ID3D12GraphicsCommandList* list,const float (&color)[4]){
    if(!GuestMultisampleStorageClearCompatible(image,color))
        throw std::invalid_argument("Incompatible guest MSAA storage clear");
    auto lease=std::make_shared<GuestMultisampleStorageClearLease>();lease->clears.reserve(image.desc.samples);
    for(UINT sample=0;sample<image.desc.samples;++sample){const auto plane=GuestMultisampleClearPlane(image,sample);
        lease->clears.push_back(std::make_shared<GuestStorageImageClearLease>(device,plane));}
    lease->transfer=cache.PrepareMultisampleStorage(image);
    // Retain every descriptor before the first recorded clear, including if
    // a later recording operation fails. The cache releases it at its fence.
    image.storage_clear_leases.push_back(lease);
    for(UINT sample=0;sample<image.desc.samples;++sample){
        // Resource copies retain storage, but their state trackers are local.
        // Transition the authoritative plane before building the clear view.
        image.storage_samples[sample].Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto plane=GuestMultisampleClearPlane(image,sample);
        RecordGuestStorageImageClear(device,plane,list,color,lease->clears[sample]);}
    lease->transfer->Record(list,image.texture,image.storage_samples,image.storage_linear,true);
    image.pending=true;return lease;
}
struct GuestImageClearLease {
    Resource texture;GuestTextureDesc view;DXGI_FORMAT view_format;ComPtr<ID3D12DescriptorHeap> heap;UINT step,mips;
    GuestImageClearLease(ID3D12Device* device,const GuestTextureCache::Entry& e):texture(e.texture),view(GuestImageClearView(e.desc)),view_format(e.layout.format),mips(e.desc.levels){
        D3D12_DESCRIPTOR_HEAP_DESC desc{};desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;desc.NumDescriptors=mips;
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&heap))));
        auto handle=heap->GetCPUDescriptorHandleForHeapStart();step=device->GetDescriptorHandleIncrementSize(desc.Type);
        GuestTextureCache::Entry target;target.desc=GuestImageClearView(e.desc);
        target.desc.render_target=true;target.layout=e.layout;target.texture.handle=texture.handle;
        for(UINT mip=0;mip<mips;++mip){const auto view=target.Rtv(mip);
            DX12_API_CALL(device->CreateRenderTargetView(texture.handle.Get(),&view,handle));handle.ptr+=step;}
    }
};
// The returned CPU descriptor heap must survive until submission retires.
inline ComPtr<ID3D12DescriptorHeap> RecordGuestImageClear(ID3D12Device* device,
    GuestTextureCache::Entry& e,ID3D12GraphicsCommandList* list,const float (&color)[4],
    std::shared_ptr<GuestImageClearLease> lease={}) {
    if(!GuestImageClearCompatible(e))throw std::invalid_argument("Incompatible guest image clear");
    if(!lease){
        auto& cached=e.render_clear_descriptors;
        if(!cached||cached->texture.handle!=e.texture.handle||cached->view!=GuestImageClearView(e.desc)||cached->view_format!=e.layout.format)
            cached=std::make_shared<GuestImageClearLease>(device,e);
        lease=cached;
    }
    if(lease->texture.handle!=e.texture.handle||lease->view!=GuestImageClearView(e.desc)||lease->view_format!=e.layout.format)
        throw std::invalid_argument("Mismatched prepared guest render target clear");
    auto handle=lease->heap->GetCPUDescriptorHandleForHeapStart();
    e.texture.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto native_color=GuestNativeImageClearColor(e.desc,color);
    for(uint32_t mip=0;mip<e.desc.levels;++mip){
        DX12_API_CALL(list->ClearRenderTargetView(handle,native_color.data(),0,nullptr));handle.ptr+=lease->step;
    }
    e.pending=true;return lease->heap;
}
}
