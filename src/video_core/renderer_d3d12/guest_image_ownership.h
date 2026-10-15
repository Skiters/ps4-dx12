// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
#include <optional>
#include <utility>

namespace NativeDX12 {
struct GuestImageOwnershipGeometry {
    GuestTextureDesc desc;
    GuestTextureLayout layout;
};

// Ownership tracks physical guest words independently of the current shader
// interpretation. It must not make a read-only format legal for IMAGE_WRITE.
inline std::optional<GuestImageOwnershipGeometry> DescribeGuestImageOwnership(
    const GuestTextureCache::Entry& image) {
    auto owner=image.desc;owner.writable=true;owner.render_target=false;
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
    if(layout.bytes_per_pixel!=source.bytes_per_pixel||layout.block_extent!=source.block_extent||
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
