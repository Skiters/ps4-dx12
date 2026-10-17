// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
#include <algorithm>
#include <bit>
#include <optional>
#include <utility>

namespace NativeDX12 {
struct GuestImageOwnershipGeometry {
    GuestTextureDesc desc;
    GuestTextureLayout layout;
};

// Physical depth-plane transfer capability is independent from admitting the
// full image to independent ownership. Optional S keeps its own guest range.
inline bool IsGuestDepthPlaneStorage(const GuestTextureDesc& desc,const GuestTextureLayout& layout) {
    return desc.depth_target&&!desc.render_target&&!desc.writable&&!desc.volume_depth&&!desc.one_dimensional&&
        desc.samples&&std::has_single_bit(desc.samples)&&desc.samples<=16&&
        (desc.samples==1||desc.levels==1)&&layout.block_extent==1&&!layout.expanded_bytes_per_pixel&&
        ((desc.data_format==2&&desc.number_format==0&&layout.bytes_per_pixel==2&&layout.format==DXGI_FORMAT_R16_UNORM)||
         (desc.data_format==4&&desc.number_format==7&&layout.bytes_per_pixel==4&&layout.format==DXGI_FORMAT_R32_FLOAT));
}

// D16/D32 depth storage has one exact physical plane. The native depth
// attachment remains typed/readonly to shaders; only ownership uses a writable
// color-shaped description of the same guest words. Split stencil and MSAA
// need their own plane/sample transfer and must not enter this contract.
inline bool IsIndependentGuestDepthStorage(const GuestTextureDesc& desc) {
    return desc.depth_target&&!desc.render_target&&!desc.writable&&!desc.stencil_address&&
        desc.samples==1&&!desc.volume_depth&&!desc.one_dimensional&&
        ((desc.data_format==2&&desc.number_format==0)||(desc.data_format==4&&desc.number_format==7));
}
inline bool IsIndependentGuestDepthStorage(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout) {
    return IsIndependentGuestDepthStorage(desc)&&layout.block_extent==1&&!layout.expanded_bytes_per_pixel&&
        ((desc.data_format==2&&layout.bytes_per_pixel==2&&layout.format==DXGI_FORMAT_R16_UNORM)||
         (desc.data_format==4&&layout.bytes_per_pixel==4&&layout.format==DXGI_FORMAT_R32_FLOAT));
}

// Validate actual readonly BC storage independently of whether its entire mip
// chain can also be represented as a UINT64/128 image. No shader writes allowed.
inline bool IsNativeReadonlyGuestBlockStorage(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout) {
    if(!(desc.data_format>=35&&desc.data_format<=41&&!desc.writable&&
        !desc.render_target&&!desc.depth_target&&!desc.stencil_address&&
        !desc.volume_depth&&!desc.one_dimensional&&desc.samples==1&&
        desc.levels&&desc.levels<=15&&layout.mips.size()==desc.levels&&
        desc.tile_mode!=8&&desc.tile_mode!=31&&
        desc.width>=4&&desc.height>=4&&desc.pitch>=desc.width&&
        desc.width%4==0&&desc.height%4==0&&desc.pitch%4==0&&
        !desc.pow2_padding&&!desc.storage_height&&!desc.storage_slice_bytes&&
        layout.block_extent==4&&!layout.expanded_bytes_per_pixel&&
        (layout.bytes_per_pixel==8||layout.bytes_per_pixel==16)))return false;
    return desc.levels<=std::bit_width(std::max(desc.width,desc.height));
}

// Complete word-image correspondence is stricter than actual BC ownership.
inline bool IsIndependentGuestBlockStorage(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout) {
    if(!IsNativeReadonlyGuestBlockStorage(desc,layout))return false;
    const auto width=desc.width/4,height=desc.height/4;
    if(desc.levels>std::bit_width(std::max(width,height)))return false;
    for(uint32_t mip=0;mip<desc.levels;++mip){
        // Dividing the base extent is insufficient for non-power-of-two
        // chains: e.g. 96 texels need two BC blocks at mip4, not one.
        if((std::max(desc.width>>mip,1u)+3)/4!=std::max(width>>mip,1u)||
           (std::max(desc.height>>mip,1u)+3)/4!=std::max(height>>mip,1u))return false;
    }
    return true;
}

// Ownership tracks physical guest words independently of the current shader
// interpretation. It must not make a read-only format legal for IMAGE_WRITE.
inline std::optional<GuestImageOwnershipGeometry> DescribeGuestImageOwnership(
    const GuestTextureCache::Entry& image) {
    auto owner=image.desc;owner.writable=true;owner.render_target=false;
    if(image.desc.depth_target){
        if(!IsIndependentGuestDepthStorage(image.desc,image.layout))return std::nullopt;
        owner.depth_target=false;
    }
    const bool block=image.layout.block_extent==4;
    const bool block_words=block&&IsIndependentGuestBlockStorage(image.desc,image.layout);
    if(block&&!IsNativeReadonlyGuestBlockStorage(image.desc,image.layout))return std::nullopt;
    if(block&&!block_words)owner=image.desc; // Preserve tiny/partial BC-only mips as actual storage.
    if(block_words){
        // Validate only physical ownership using one UINT texel per BC block.
        // The real image stays readonly BC; no BC UAV is created or authorized.
        owner.width/=4;owner.height/=4;owner.pitch/=4;
        owner.data_format=image.layout.bytes_per_pixel==8?11:14;owner.number_format=4;
        owner.native_raw32=owner.native_packed10_raw=false;
    }
    if(image.desc.data_format==13&&!image.desc.writable&&!image.desc.render_target){
        // RGB32 has a true 96-bit guest layout. Its independent read-only
        // ownership must neither authorize image writes nor become R32/RGBA32.
        owner.writable=false;
    }
    if(image.desc.data_format==34&&image.desc.number_format==7&&
       !image.desc.writable&&!image.desc.render_target&&!image.desc.depth_target&&
       !image.desc.stencil_address){
        // Pinned Vulkan allows E5B9G9R9 sampled aliases in the 32-bit format
        // class, but its format table does not admit storage writes. Validate
        // only the owner's physical geometry as R32_UINT; keep the real image,
        // typed view, native cast list and descriptor unchanged.
        owner.data_format=4;owner.number_format=4;
        owner.native_raw32=owner.native_packed10_raw=false;
    }
    auto layout=LayoutGuestTexture(owner);
    const auto& source=image.layout;
    if(layout.bytes_per_pixel!=source.bytes_per_pixel||
       (block_words?(layout.block_extent!=1||source.block_extent!=4):layout.block_extent!=source.block_extent)||
       layout.guest_bytes!=source.guest_bytes||layout.logical_bytes!=source.logical_bytes||
       layout.mips.size()!=source.mips.size())return std::nullopt;
    for(size_t i=0;i<layout.mips.size();++i){
        const auto& a=layout.mips[i];const auto& b=source.mips[i];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||
           a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)
            return std::nullopt;
    }
    return GuestImageOwnershipGeometry{std::move(owner),std::move(layout)};
}
} // namespace NativeDX12
