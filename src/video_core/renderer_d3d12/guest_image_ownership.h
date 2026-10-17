// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
#include "page_watch.h"
#include <algorithm>
#include <bit>
#include <optional>
#include <utility>

namespace NativeDX12 {
struct GuestImageOwnershipGeometry {
    GuestTextureDesc desc;
    GuestTextureLayout layout;
    std::optional<std::pair<GuestTextureDesc,GuestTextureLayout>> stencil;
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
// color-shaped description of the same guest words. This single-plane helper
// remains stricter than the complete multi-plane ownership description below.
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
        if(!IsGuestDepthPlaneStorage(image.desc,image.layout)||
           !GuestTextureCache::CanUploadGpuSnapshot(image.desc,image.layout))return std::nullopt;
        owner.depth_target=false;owner.stencil_address=0;
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
    GuestImageOwnershipGeometry result{std::move(owner),std::move(layout),{}};
    if(image.desc.stencil_address){
        if(!image.desc.depth_target)return std::nullopt;
        auto stencil=DescribeGuestStencil(image.desc);stencil.writable=true;
        auto stencil_layout=LayoutGuestTexture(stencil);
        if(!MatchesGuestMipLayout(stencil_layout,image.stencil_layout))return std::nullopt;
        result.stencil.emplace(std::move(stencil),std::move(stencil_layout));
    }
    return result;
}

inline bool CanRetainGuestImageOwnership(const GuestTextureCache& cache,
    const GuestImageOwnershipGeometry& owner){
    return cache.CanRetainIndependentImage(owner.desc,owner.layout)&&
        (!owner.stencil||cache.CanRetainIndependentImage(owner.stencil->first,owner.stencil->second));
}

inline std::vector<std::pair<uint64_t,uint64_t>> GuestImageProtectionRanges(
    const GuestTextureCache& cache,const GuestTextureCache::Entry* exclude=nullptr){
    std::vector<std::pair<uint64_t,uint64_t>> result;
    for(const auto& image:cache.Entries())if(image.get()!=exclude&&(image->gpu_owned||image->cpu_watched))
        for(const auto& plane:image->ProtectionPlanes())
            VisitGuestImageProtectedRanges(plane,[&](uint64_t address,uint64_t bytes){result.emplace_back(address,bytes);});
    return result;
}

// Shared production ownership operation: validate BOTH physical geometries,
// backing and native guards before publishing any new ownership flags.
template <class HasBacking,class StrongBuffers>
bool AcquireGuestImageOwnership(GuestTextureCache& cache,GuestTextureCache::Entry& image,
    PageWatch& watch,HasBacking&& has_backing,StrongBuffers&& strong_buffers,
    GuestTextureCache::Entry* replaced=nullptr){
    if(image.cache!=&cache)return false;
    const auto owner=DescribeGuestImageOwnership(image);
    if(!owner||!CanRetainGuestImageOwnership(cache,*owner))return false;
    if(image.HasUnprotectedCpuPages())
        throw std::runtime_error("Image ownership requires validated boundary witnesses");
    std::vector<std::pair<uint64_t,uint64_t>> planes,strong,transferred;
    if(replaced){
        if(replaced==&image||replaced->cache!=&cache)return false;
        if(replaced->gpu_owned&&!replaced->buffer_alias_version){
            if(replaced->HasUnprotectedCpuPages())return false;
            for(const auto& plane:replaced->ProtectionPlanes())if(plane.bytes){
                const auto pages=GuestProtectionPages(plane.address,plane.bytes);
                if(!pages||!has_backing(plane.address,plane.bytes))return false;
                transferred.emplace_back(pages->address,pages->size);
            }
        }
    }
    for(const auto& plane:image.ProtectionPlanes())if(plane.bytes){
        const auto pages=GuestProtectionPages(plane.address,plane.bytes);
        if(!pages||!has_backing(plane.address,plane.bytes))return false;
        planes.emplace_back(pages->address,pages->size);
        auto retained=strong_buffers(plane.address,plane.bytes);
        strong.insert(strong.end(),retained.begin(),retained.end());
    }
    if(!watch.TrackImageReadOnly(planes,image.gpu_owned&&!image.buffer_alias_version,strong,transferred))return false;
    image.cpu_watched=false;image.buffer_alias_version=true;image.SetGpuOwned(true);
    image.buffer_alias_watch=true;return true;
}

template <class Release>
void ReleaseGuestImageOwnership(GuestTextureCache& cache,GuestTextureCache::Entry& image,Release&& release){
    if(image.cache!=&cache)throw std::invalid_argument("Independent image retirement belongs to another cache");
    image.buffer_alias_version=image.buffer_alias_watch=false;image.SetGpuOwned(false);
    image.unprotected_cpu_pages.clear();image.stencil_unprotected_cpu_pages.clear();image.linear_download_pending=false;
    image.sampled_snapshot.clear();image.stencil_snapshot.clear();
    const auto retained=GuestImageProtectionRanges(cache,&image);
    for(const auto& plane:image.ProtectionPlanes())if(plane.bytes)release(plane.address,plane.bytes,retained);
}
} // namespace NativeDX12
