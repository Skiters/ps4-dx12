#include "common/api_trace.h"
#include "common/scope_exit.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "guest_image_ownership.h"
#include "guest_bc_decoder.h"
#include "video_core/amdgpu/resource.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/regs_depth.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "video_core/amdgpu/tiling.h"
#include "video_core/guest_image_layout.h"
#include "guest_tile_address.h"
#include "guest_subresource_rebase.h"
#include "guest_cmask_address.h"
#include "guest_color_clear.h"
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/libraries/kernel/process.h"
#endif
#include <bit>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <map>
#include <execution>
#include <numeric>
#include <thread>
namespace NativeDX12 {
namespace {
std::optional<std::pair<UINT,UINT>> ReadonlyDepthSubresource(
    const GuestTextureDesc&,const GuestTextureLayout&,const GuestTextureDesc&,const GuestTextureLayout&);
bool GuestAltTile(bool enabled){
#ifdef SHADPS4_NATIVE_DX12_CORE
    return enabled&&Libraries::Kernel::sceKernelIsNeoMode();
#else
    return false;
#endif
}
DXGI_FORMAT BlockStorageFormat(DXGI_FORMAT format){
    switch(format){
    case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:return DXGI_FORMAT_BC1_TYPELESS;
    case DXGI_FORMAT_BC2_UNORM:case DXGI_FORMAT_BC2_UNORM_SRGB:return DXGI_FORMAT_BC2_TYPELESS;
    case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:return DXGI_FORMAT_BC3_TYPELESS;
    case DXGI_FORMAT_BC4_UNORM:case DXGI_FORMAT_BC4_SNORM:return DXGI_FORMAT_BC4_TYPELESS;
    case DXGI_FORMAT_BC5_UNORM:case DXGI_FORMAT_BC5_SNORM:return DXGI_FORMAT_BC5_TYPELESS;
    case DXGI_FORMAT_BC6H_UF16:case DXGI_FORMAT_BC6H_SF16:return DXGI_FORMAT_BC6H_TYPELESS;
    case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:return DXGI_FORMAT_BC7_TYPELESS;
    default:return DXGI_FORMAT_UNKNOWN;
    }
}
bool CanBindReadonlyBcMipPrefix(const GuestTextureCache::Entry&,const GuestTextureDesc&,
    const GuestTextureLayout&);
bool CanBindReadonlyBcView(const GuestTextureCache::Entry&,const GuestTextureDesc&,
    const GuestTextureLayout&);
// PS4 number formats reinterpret the same texel bits. Allocate the DXGI
// typeless family, then bind a fully typed SRV/UAV for each guest descriptor.
DXGI_FORMAT GuestStorageFormat(uint32_t data,DXGI_FORMAT fallback){
    switch(data){
    case 1:return DXGI_FORMAT_R8_TYPELESS;
    case 2:return DXGI_FORMAT_R16_TYPELESS;
    case 3:return DXGI_FORMAT_R8G8_TYPELESS;
    case 4:return DXGI_FORMAT_R32_TYPELESS;
    case 5:return DXGI_FORMAT_R16G16_TYPELESS;
    case 9:return DXGI_FORMAT_R10G10B10A2_TYPELESS;
    case 10:return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    case 11:return DXGI_FORMAT_R32G32_TYPELESS;
    case 12:return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case 13:return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case 14:return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case 35:case 36:case 37:case 38:case 39:case 40:case 41:{
        const auto block=BlockStorageFormat(fallback);return block==DXGI_FORMAT_UNKNOWN?fallback:block;}
    default:return fallback;
    }
}
std::span<const DXGI_FORMAT> PackedFloatCasts(DXGI_FORMAT format,bool supported){
    static constexpr std::array r11{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_UINT};
    static constexpr std::array rgb9{DXGI_FORMAT_R9G9B9E5_SHAREDEXP,DXGI_FORMAT_R32_UINT};
    if(!supported)return {};
    if(format==DXGI_FORMAT_R11G11B10_FLOAT)return r11;
    if(format==DXGI_FORMAT_R9G9B9E5_SHAREDEXP)return rgb9;
    return {};
}
bool IsDirectReadonlyWordClass(const GuestTextureDesc& d){
    if(d.writable||d.render_target||d.depth_target||d.stencil_address||d.samples!=1||
       d.native_raw32)return false;
    if(d.native_packed10_raw)return d.data_format==9&&
        (d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5);
    switch(d.data_format){
    case 4:return d.number_format==4||d.number_format==5||d.number_format==7;
    case 5:return d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||d.number_format==7;
    case 6:case 34:return d.number_format==7;
    case 9:return d.number_format==0||d.number_format==4; // Signed packed10 is expanded.
    case 10:return d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||d.number_format==9;
    default:return false;
    }
}
std::span<const DXGI_FORMAT> ReadonlyWordCasts(const GuestTextureDesc& d,bool supported){
    // Pinned Vulkan _32BIT mutable class, restricted to direct readonly native
    // representations. An explicit format-list allocation retains the same I;
    // expanded signed packed10, depth, writers and MSAA have separate domains.
    static constexpr std::array formats{DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R16G16_UNORM,DXGI_FORMAT_R16G16_SNORM,DXGI_FORMAT_R16G16_UINT,DXGI_FORMAT_R16G16_SINT,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R9G9B9E5_SHAREDEXP,
        DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R10G10B10A2_UINT,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_SNORM,DXGI_FORMAT_R8G8B8A8_UINT,
        DXGI_FORMAT_R8G8B8A8_SINT,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB};
    if(!supported||!IsDirectReadonlyWordClass(d))return {};
    return formats;
}
bool IsReadonlyMultisampleWordClass(const GuestTextureDesc& d){
    return !d.writable&&!d.render_target&&!d.depth_target&&!d.stencil_address&&
        !d.one_dimensional&&!d.volume_depth&&d.levels==1&&
        (d.samples==2||d.samples==4||d.samples==8)&&
        (d.UsesRaw32()||(d.data_format==4&&
            (d.number_format==4||d.number_format==5||d.number_format==7)));
}
std::span<const DXGI_FORMAT> WritableWordCasts(const GuestTextureDesc& d,bool supported,bool r11_uav){
    // Original _32BIT storage remains mutable after a write. Keep typed R16G16
    // UAVs and raw R32 codecs on one allocation, without changing the guest I.
    static constexpr std::array formats{DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32_SINT,
        DXGI_FORMAT_R16G16_UNORM,DXGI_FORMAT_R16G16_SNORM,DXGI_FORMAT_R16G16_UINT,
        DXGI_FORMAT_R16G16_SINT,DXGI_FORMAT_R16G16_FLOAT,
        DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R10G10B10A2_UINT,
        DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_UINT,DXGI_FORMAT_R8G8B8A8_SINT,
        DXGI_FORMAT_R9G9B9E5_SHAREDEXP,DXGI_FORMAT_R11G11B10_FLOAT};
    const auto available=std::span(formats).first(formats.size()-size_t(!r11_uav));
    if(!supported||!d.writable||d.render_target||d.depth_target||d.stencil_address||d.samples!=1)return {};
    if(d.UsesRaw32())return available;
    if(d.data_format==4&&(d.number_format==4||d.number_format==5||d.number_format==7))return available;
    if(d.data_format==5&&(d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||d.number_format==7))return available;
    if(d.data_format==6&&d.number_format==7&&r11_uav)return available;
    if((d.data_format==9&&(d.number_format==0||d.number_format==4))||
       (d.data_format==10&&(d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5)))return available;
    return {};
}
bool IsOrderedScalarClass(const GuestTextureDesc& d){
    return (d.data_format==1||d.data_format==2)&&
        (d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||
         (d.data_format==2&&d.number_format==7)||(d.data_format==1&&d.number_format==9));
}
bool IsOrderedFloatWordClass(const GuestTextureDesc& d){
    return d.number_format==7&&(d.data_format==4||d.data_format==11||d.data_format==14);
}
bool IsOrderedIntegerClass(const GuestTextureDesc& d){
    if(d.number_format!=4&&d.number_format!=5)return false;
    switch(d.data_format){
    case 1:case 2:case 3:case 4:case 5:case 9:case 10:case 11:case 12:case 14:return true;
    default:return false;
    }
}
bool IsOrderedPacked16Class(const GuestTextureDesc& d){
    return d.number_format==0&&d.data_format>=16&&d.data_format<=19;
}
bool IsOrderedSrgbClass(const GuestTextureDesc& d){
    return d.data_format==10&&d.number_format==9;
}
DXGI_FORMAT OrderedColorWordFormat(const GuestTextureDesc& d){
    return d.data_format==1?DXGI_FORMAT_R8_UINT:(d.data_format==2||d.data_format==3||IsOrderedPacked16Class(d))?DXGI_FORMAT_R16_UINT:
        (d.data_format==11||d.data_format==12)?DXGI_FORMAT_R32G32_UINT:
        d.data_format==14?DXGI_FORMAT_R32G32B32A32_UINT:DXGI_FORMAT_R32_UINT;
}
DXGI_FORMAT OrderedColorWordStorage(const GuestTextureDesc& d){
    return d.data_format==1?DXGI_FORMAT_R8_TYPELESS:(d.data_format==2||d.data_format==3||IsOrderedPacked16Class(d))?DXGI_FORMAT_R16_TYPELESS:
        (d.data_format==11||d.data_format==12)?DXGI_FORMAT_R32G32_TYPELESS:
        d.data_format==14?DXGI_FORMAT_R32G32B32A32_TYPELESS:DXGI_FORMAT_R32_TYPELESS;
}
bool IsOrderedColorClass(const GuestTextureDesc& d) {
    return d.render_target&&!d.writable&&!d.depth_target&&!d.stencil_address&&
        !d.one_dimensional&&!d.volume_depth&&d.first_mip<d.levels&&
        (d.samples==1||(!d.first_mip&&d.levels==1))&&
        std::has_single_bit(d.samples)&&d.samples<=8&&
        (IsOrderedScalarClass(d)||IsOrderedIntegerClass(d)||IsOrderedPacked16Class(d)||(d.data_format==9&&(d.number_format==0||d.number_format==1))||
         (d.data_format==3&&(d.number_format==0||d.number_format==1||d.number_format==9))||
         (d.data_format==10&&(d.number_format==0||d.number_format==1||d.number_format==9))||
         ((d.data_format==5||d.data_format==12)&&(d.number_format==0||d.number_format==1||d.number_format==7))||
         IsOrderedFloatWordClass(d)||(d.data_format==6&&d.number_format==7));
}
bool IsOrderedMultisampleColorClass(const GuestTextureDesc& d) {
    return IsOrderedColorClass(d)&&d.samples>1;
}
std::span<const DXGI_FORMAT> OrderedColorCasts(const GuestTextureDesc& d,bool supported){
    static constexpr std::array packed10{DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R10G10B10A2_UINT,DXGI_FORMAT_R32_UINT};
    // An explicit list replaces the old typeless-family view permissions.
    // Preserve every ordinary family view, not just this draw's RTV and ROV.
    static constexpr std::array rgba8{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_SNORM,
        DXGI_FORMAT_R8G8B8A8_UINT,DXGI_FORMAT_R8G8B8A8_SINT,DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,DXGI_FORMAT_R32_UINT};
    static constexpr std::array rg16{DXGI_FORMAT_R16G16_UNORM,DXGI_FORMAT_R16G16_SNORM,
        DXGI_FORMAT_R16G16_UINT,DXGI_FORMAT_R16G16_SINT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R32_UINT};
    static constexpr std::array rg8{DXGI_FORMAT_R8G8_UNORM,DXGI_FORMAT_R8G8_SNORM,
        DXGI_FORMAT_R8G8_UINT,DXGI_FORMAT_R8G8_SINT,DXGI_FORMAT_R16_UINT};
    static constexpr std::array r11{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_UINT};
    static constexpr std::array rgb565{DXGI_FORMAT_B5G6R5_UNORM,DXGI_FORMAT_R16_UINT};
    static constexpr std::array rgba5551{DXGI_FORMAT_B5G5R5A1_UNORM,DXGI_FORMAT_R16_UINT};
    static constexpr std::array rgba4444{DXGI_FORMAT_B4G4R4A4_UNORM,DXGI_FORMAT_R16_UINT};
    static constexpr std::array rgba16{DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_SNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R16G16B16A16_UINT,DXGI_FORMAT_R16G16B16A16_SINT,
        DXGI_FORMAT_R32G32_UINT};
    if(!supported||!IsOrderedColorClass(d))return {};
    if(d.data_format==9)return packed10;
    if(d.data_format==10)return rgba8;
    if(d.data_format==5)return rg16;
    if(d.data_format==12)return rgba16;
    if(d.data_format==6)return r11;
    if(d.data_format==3)return rg8;
    if(d.data_format==16)return rgb565;
    if(d.data_format==17||d.data_format==18)return rgba5551;
    if(d.data_format==19)return rgba4444;
    return {};
}
bool CanBindNativeWordFormat(const GuestTextureCache::Entry& entry,const GuestTextureDesc& view){
    if(!entry.texture.handle)return false;
    const auto format=LayoutGuestTexture(view).format;
    const auto native=entry.texture.handle->GetDesc().Format;
    if(entry.texture.castable_formats)return entry.texture.AllowsCast(format);
    if(native==format||entry.texture.AllowsCast(format))return true;
    return native==(view.UsesRaw32()?DXGI_FORMAT_R32_TYPELESS:GuestStorageFormat(view.data_format,format));
}
bool CanUseMultisampleStorageView(const GuestTextureCache::Entry& entry,const GuestTextureDesc& view){
    const bool ordered=IsOrderedMultisampleColorClass(entry.desc)&&
        IsOrderedMultisampleColorClass(view)&&CompatibleGuestAttachmentStorage(entry.desc,view);
    if(!entry.texture.handle||(!view.writable&&!ordered)||entry.desc.samples<2||entry.desc.samples>16||
       !std::has_single_bit(entry.desc.samples)||view.samples!=entry.desc.samples||
       entry.desc.levels!=1||view.levels!=1||entry.desc.depth_target||
       (!entry.desc.writable&&!IsReadonlyMultisampleWordClass(entry.desc)&&!ordered))return false;
    const auto native=entry.texture.handle->GetDesc();
    if(native.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       native.SampleDesc.Count!=entry.desc.samples||native.MipLevels!=1||
       native.Width!=entry.desc.width||native.Height!=entry.desc.height||
       native.DepthOrArraySize!=entry.desc.layers||
       !(native.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
       (native.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))return false;
    auto written_source=entry.desc;written_source.writable=true;
    return (ordered||(entry.desc.writable&&entry.desc==view)||
        CompatibleGuestWritableStorage(written_source,view))&&
        MatchesGuestMipLayout(entry.layout,LayoutGuestTexture(view))&&
        (ordered||CanBindNativeWordFormat(entry,view));
}
bool IsReadonlyRawR32Class(const GuestTextureDesc& d){
    if(d.writable||d.render_target||d.depth_target||d.stencil_address||
       (d.native_raw32&&!(d.data_format==34&&d.samples>1))||
       !std::has_single_bit(d.samples)||d.samples>16||
       (d.samples>1&&(d.levels!=1||d.volume_depth||d.one_dimensional)))return false;
    if(d.data_format==4)return d.number_format==4||d.number_format==5||d.number_format==7;
    if(d.data_format==34)return d.native_raw32&&d.number_format==7;
    return d.UsesRawPacked10()&&(d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5);
}
bool CanBindReadonlyWordSrv(const GuestTextureCache::Entry& entry,const GuestTextureDesc& view){
    if(!CompatibleGuestReadonlyWordStorage(entry.desc,view)||!entry.texture.handle)return false;
    const auto format=LayoutGuestTexture(view).format;
    if(entry.texture.AllowsCast(format))return true;
    if(entry.texture.castable_formats)return false;
    // A real R32 typeless resource needs no relaxed format-list feature for
    // its R32 views. Fully typed resources without a list retain exact format.
    const auto native=entry.texture.handle->GetDesc().Format;
    const bool r32=format==DXGI_FORMAT_R32_UINT||format==DXGI_FORMAT_R32_SINT||format==DXGI_FORMAT_R32_FLOAT;
    return r32&&(native==DXGI_FORMAT_R32_TYPELESS||native==format);
}
std::span<const DXGI_FORMAT> ResourceCasts(const Resource& resource){
    return resource.castable_formats?std::span<const DXGI_FORMAT>(*resource.castable_formats):std::span<const DXGI_FORMAT>{};
}
}
std::span<const DXGI_FORMAT> GuestTextureCache::OrderedColorFormats(const GuestTextureDesc& desc,bool supported){
    return OrderedColorCasts(desc,supported);
}
bool GuestTextureCache::CanTransferIndependentMultisample(const GuestTextureDesc& d,const GuestTextureLayout& layout){
    if((d.samples!=2&&d.samples!=4&&d.samples!=8)||d.levels!=1||
       d.depth_target||d.stencil_address||d.volume_depth||d.one_dimensional||layout.block_extent!=1)return false;
    if(layout.expanded_bytes_per_pixel){
        // Read-only signed packed10 and packed19 retain original guest words
        // plus exact written-byte coverage on upload/conversion. Retile uses
        // that backing, never reconstruction from the expanded float values.
        if(d.writable||d.render_target||!HasExactPackedExpansion(d,layout)||
           layout.HostBytesPerPixel()!=16)return false;
        if(d.data_format==9)return (d.number_format==1||d.number_format==5)&&
            layout.bytes_per_pixel==4&&layout.format==(d.number_format==1?
                DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_R32G32B32A32_SINT);
        return d.data_format==19&&d.number_format==0&&layout.bytes_per_pixel==2&&
            layout.format==DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    if(d.data_format==6){
        // Packed float components must never round-trip through float loads:
        // the allocation's explicit R32_UINT cast preserves every guest bit.
        // Upload and GuestMultisample still validate actual format-list support,
        // the resource's cast list and the requested sample/view capabilities.
        return d.number_format==7&&layout.format==DXGI_FORMAT_R11G11B10_FLOAT&&
            layout.bytes_per_pixel==4&&layout.HostBytesPerPixel()==4;
    }
    if(d.data_format==34){
        // PS4 RGB9E5 is sampled-only. Its independent snapshot keeps typed
        // sampling while raw R32 views move the original shared-exponent word.
        // Ownership is described separately; this does not permit IMAGE_WRITE.
        return (!d.writable||d.UsesRaw32())&&!d.render_target&&d.number_format==7&&
            layout.format==(d.UsesRaw32()?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R9G9B9E5_SHAREDEXP)&&
            layout.bytes_per_pixel==4&&layout.HostBytesPerPixel()==4;
    }
    if(d.data_format==13){
        // RGB32 retains its 96-bit guest geometry. UINT4 moves the three
        // payload DWORDs and a synthetic host alpha; retile stores only RGB.
        // Pinned guest usage permits sampled images, not IMAGE_WRITE or MRT.
        const auto format=d.number_format==4?DXGI_FORMAT_R32G32B32A32_UINT:
            d.number_format==5?DXGI_FORMAT_R32G32B32A32_SINT:
            d.number_format==7?DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_UNKNOWN;
        return !d.writable&&!d.render_target&&format!=DXGI_FORMAT_UNKNOWN&&
            layout.format==format&&layout.bytes_per_pixel==12&&layout.HostBytesPerPixel()==16;
    }
    if(d.data_format==9){
        // Packed10 storage is either one raw R32 word or an exact four-lane
        // R10G10B10A2 UINT transfer. Never reconstruct signed sampled expansion
        // from normalized values; that path needs its original words and mask.
        const bool raw=d.UsesRawPacked10()&&layout.format==DXGI_FORMAT_R32_UINT&&
            (d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5);
        const bool native=!d.UsesRawPacked10()&&
            ((d.number_format==0&&layout.format==DXGI_FORMAT_R10G10B10A2_UNORM)||
             ((d.number_format==4||(d.render_target&&(d.number_format==1||d.number_format==5)))&&
              layout.format==DXGI_FORMAT_R10G10B10A2_UINT));
        return (raw||native)&&layout.bytes_per_pixel==4&&layout.HostBytesPerPixel()==4;
    }
    if(d.data_format>=16&&d.data_format<=19){
        // Writable packed16 uses an R32 UINT plane holding the guest word in
        // its low half. Existing tiling stores exactly two bytes through masked
        // writes, preserving the neighbour, row tail, sample layout and padding.
        if(d.number_format!=0||layout.bytes_per_pixel!=2)return false;
        const bool raw=d.writable&&!d.render_target&&
            layout.format==DXGI_FORMAT_R32_UINT&&layout.HostBytesPerPixel()==4;
        // Native packed sample loads recover each small UNORM integer code
        // through PACKED_MODE1/2/3. Original18's rotation and19's guest nibble
        // order remain in the tiler. Actual RTV/sample support is checked when
        // allocating/preparing the native attachment; unavailable B4 must not
        // authorize a float approximation or silently change the guest usage.
        const auto native_format=d.data_format==16?DXGI_FORMAT_B5G6R5_UNORM:
            d.data_format==19?DXGI_FORMAT_B4G4R4A4_UNORM:DXGI_FORMAT_B5G5R5A1_UNORM;
        const bool native=!d.writable&&layout.HostBytesPerPixel()==2&&layout.format==native_format;
        return raw||native;
    }
    // These typeless 8/16/32-bit families have exact UINT buffer, MSAA SRV and
    // RTV transfers. Sub-DWORD texels use the tiler's byte-masked retile,
    // preserving neighbouring texels and row padding. sRGB transfers use UINT
    // views and a linear copy footprint, without gamma conversion.
    // Typed access and sample quality remain adapter-validated by Upload and
    // GuestMultisample::Prepare; RGB32's distinct guest stride is handled above.
    const UINT bytes8=d.data_format==1?1:d.data_format==3?2:d.data_format==10?4:0;
    const UINT bytes16=d.data_format==2?2:d.data_format==5?4:d.data_format==12?8:0;
    const UINT bytes=bytes8?bytes8:bytes16?bytes16:d.data_format==4?4:d.data_format==11?8:d.data_format==14?16:0;
    const bool number=d.number_format==4||d.number_format==5||(!bytes8&&d.number_format==7)||
        ((bytes8||bytes16)&&(d.number_format==0||d.number_format==1))||
        (bytes8&&d.number_format==9);
    // Raw RGBA8 moves its already-packed DWORD through R32_UINT views;
    // ordinary RGBA8 retains its four-component integer transfer.
    return (!(d.data_format==10&&d.UsesRaw32())||layout.format==DXGI_FORMAT_R32_UINT)&&
        bytes&&number&&layout.bytes_per_pixel==bytes&&layout.HostBytesPerPixel()==bytes;
}
void GuestTextureCache::ValidateWritableFormat(DXGI_FORMAT format){
    if(writable_formats_.contains(format))return;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
    if(!(support.Support1&D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)||
       !(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)||!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
        throw std::runtime_error("Native guest writable format="+std::to_string(format)+" lacks typed UAV load/store");
    writable_formats_.insert(format);
}
GuestTextureCache::GuestTextureCache(ID3D12Device* device,std::wstring converter):device_(device),texture_allocator_(device),converter_(std::move(converter)) {
    // An optional typed UAV format must not become a requirement for every
    // ordinary word allocation merely because another view can use its bits.
    D3D12_FEATURE_DATA_FORMAT_SUPPORT r11{DXGI_FORMAT_R11G11B10_FLOAT};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&r11,sizeof(r11))));
    if((r11.Support1&D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)&&
       (r11.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)&&(r11.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
        writable_formats_.insert(DXGI_FORMAT_R11G11B10_FLOAT);
    ComPtr<IDXGIFactory4> factory;
    if(SUCCEEDED(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))))
        DX12_API_CALL(factory->EnumAdapterByLuid(DX12_API_CALL(device->GetAdapterLuid()),IID_PPV_ARGS(&budget_adapter_)));
    if(budget_adapter_){
        budget_event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!budget_event_||FAILED(DX12_API_CALL(budget_adapter_->RegisterVideoMemoryBudgetChangeNotificationEvent(
            budget_event_,&budget_cookie_)))){
            if(budget_event_)CloseHandle(budget_event_);budget_event_=nullptr;budget_cookie_=0;
        }
    }
    RefreshMemoryBudget();
}
GuestTextureCache::~GuestTextureCache(){
    if(budget_adapter_&&budget_cookie_)budget_adapter_->UnregisterVideoMemoryBudgetChangeNotification(budget_cookie_);
    if(budget_event_)CloseHandle(budget_event_);
}
void GuestTextureCache::Entry::SetGpuOwned(bool value){
    if(!value)linear_download_pending=false;
    if(gpu_owned==value)return;
    if(cache)cache->guest_owned_overlap_ranges_dirty_=true;
    gpu_owned=value;
}
bool GuestTextureCache::HasGuestRangeOverlap(uint64_t address,uint64_t size,bool gpu_owned_only)const{
    if(!size)return false;
    if(address>UINT64_MAX-size)throw std::out_of_range("Guest image overlap range overflow");
    auto& index=gpu_owned_only?guest_owned_overlap_ranges_:guest_overlap_ranges_;
    auto& dirty=gpu_owned_only?guest_owned_overlap_ranges_dirty_:guest_overlap_ranges_dirty_;
    if(dirty){
        std::map<uint64_t,uint64_t> ranges;
        const auto add=[&](uint64_t begin,uint64_t bytes){
            if(!bytes)return;
            if(begin>UINT64_MAX-bytes)throw std::logic_error("Cached guest image extent overflow");
            auto end=begin+bytes;
            auto it=ranges.lower_bound(begin);
            if(it!=ranges.begin()&&std::prev(it)->second>=begin)it=std::prev(it);
            while(it!=ranges.end()&&it->first<=end){begin=std::min(begin,it->first);end=std::max(end,it->second);it=ranges.erase(it);}
            ranges.emplace(begin,end);
        };
        for(const auto& entry:entries_){if(gpu_owned_only&&!entry->gpu_owned)continue;
            add(entry->desc.address,entry->layout.guest_bytes);
            if(entry->desc.stencil_address)add(entry->desc.stencil_address,entry->stencil_layout.guest_bytes);}
        index=std::move(ranges);dirty=false;
    }
    const auto next=index.upper_bound(address);
    return (next!=index.begin()&&std::prev(next)->second>address)||
        (next!=index.end()&&next->first<address+size);
}
void GuestTextureCache::RefreshMemoryBudget() {
    const auto now=GetTickCount64();
    // DXGI signals this event when Windows changes the process budget.  Keep a
    // slow fallback poll for drivers that fail to deliver notifications.
    if(budget_checked_at_&&now-budget_checked_at_<10000&&
       (!budget_event_||WaitForSingleObject(budget_event_,0)!=WAIT_OBJECT_0))return;
    budget_checked_at_=now;
    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
    if(!budget_adapter_||FAILED(DX12_API_CALL(budget_adapter_->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory)))||!memory.Budget)return;
    // The original renderer derives cache pressure from the adapter budget.
    // The cache accounts CPU snapshots and transfer pools as well as GPU
    // images. A fixed 4 GiB cap therefore evicts considerably less than 4 GiB
    // of actual VRAM on large adapters. Follow the current Windows budget,
    // reserving a quarter for buffers/presentation and other native resources.
    const auto budget=std::clamp<uint64_t>(memory.Budget/4*3,64*1024*1024,12ULL*1024*1024*1024);
    if(Diagnostics::Enabled()&&budget!=resident_budget_)fprintf(stderr,
        "DX12 texture budget: cache_bytes=%llu adapter_budget=%llu process_usage=%llu\n",
        (unsigned long long)budget,(unsigned long long)memory.Budget,(unsigned long long)memory.CurrentUsage);
    resident_budget_=budget;
    texture_allocator_.SetBudget(budget);
}
struct GuestTextureCopyPlans {
    struct Plan {
        uint32_t pieces{};
        std::array<std::array<uint32_t,2>,64> pixels{};
        std::vector<uint64_t> bases;
    };
    std::mutex mutex;
    std::map<std::pair<uint32_t,uint32_t>,std::shared_ptr<const Plan>> subresources;
};
// View-only differences do not change physical PS4 storage. Borrow only retired,
// CPU-coherent, single-plane resources; stencil/format reinterpretation needs
// separate plane and typeless-view support and is deliberately excluded here.
bool CompatibleGuestSampledStorage(GuestTextureDesc source,GuestTextureDesc sampled) {
    if(source.writable||sampled.writable||sampled.stencil_address||
       sampled.render_target||sampled.depth_target)return false;
    if(source.stencil_address&&!source.depth_target)return false;
    if(source.render_target&&source.data_format==9&&(source.number_format==1||source.number_format==5))return false;
    source.stencil_address=0;
    source.render_target=source.depth_target=false;
    source.sampled_depth=sampled.sampled_depth=false;
    source.storage_height=sampled.storage_height=0;
    source.storage_slice_bytes=sampled.storage_slice_bytes=0;
    source.pow2_padding=sampled.pow2_padding=false;
    source.first_mip=sampled.first_mip=0;
    source.min_lod=sampled.min_lod=0;
    source.first_layer=sampled.first_layer=0;
    source.view_layers=sampled.view_layers=1;
    source.swizzle=sampled.swizzle={4,5,6,7};
    return source==sampled;
}
bool CompatibleGuestWritableStorage(GuestTextureDesc source,GuestTextureDesc view) {
    if(!source.writable||!view.writable||source.stencil_address||view.stencil_address||
       source.render_target||view.render_target||source.depth_target||view.depth_target)return false;
    source.first_mip=view.first_mip=0;
    source.min_lod=view.min_lod=0;
    source.first_layer=view.first_layer=0;
    source.view_layers=view.view_layers=1;
    // UAVs have no component mapping; guest storage swizzles are lowered in
    // the shader and do not require separate physical images.
    source.swizzle=view.swizzle={4,5,6,7};
    const auto word_format=[](uint32_t format){return format==4||format==5||format==6||format==9||format==10||format==34;};
    // Ordinary R32 UINT/SINT/FLOAT already use the same four-byte typeless
    // allocation with typed views. Numeric interpretation belongs to the view;
    // requiring a history-dependent raw flag retires valid I and loses its bits.
    const auto raw_word=[](const GuestTextureDesc& d){return d.UsesRaw32()||
        (d.data_format==4&&(d.number_format==4||d.number_format==5||d.number_format==7))||
        (d.data_format==5&&(d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||d.number_format==7))||
        (d.data_format==9&&(d.number_format==0||d.number_format==4))||
        (d.data_format==10&&(d.number_format==0||d.number_format==1||d.number_format==4||d.number_format==5||d.number_format==9))||
        ((d.data_format==6||d.data_format==34)&&d.number_format==7);};
    if(raw_word(source)&&raw_word(view)&&word_format(source.data_format)&&word_format(view.data_format)){
        // Only comparison copies change. Both actual layouts are four-byte
        // words; typed R16G16 uses an actual resource cast, checked at binding.
        // The original image class, codecs and ownership survive.
        source.native_packed10_raw=view.native_packed10_raw=false;
        source.data_format=view.data_format=4;source.number_format=view.number_format=4;
        source.native_raw32=view.native_raw32=true;
    }
    if(source.UsesRaw32()!=view.UsesRaw32())return false;
    source.native_raw32=view.native_raw32=false;
    if(source.data_format==9&&view.data_format==9){
        if(source.UsesRawPacked10()!=view.UsesRawPacked10())return false;
        source.native_packed10_raw=view.native_packed10_raw=false;
    }
    if(source.data_format==view.data_format&&
       GuestStorageFormat(source.data_format,DXGI_FORMAT_UNKNOWN)!=DXGI_FORMAT_UNKNOWN)
        source.number_format=view.number_format=0;
    return source==view;
}
bool CompatibleGuestShaderStorage(GuestTextureDesc source,GuestTextureDesc view) {
    source.sampled_depth=view.sampled_depth=false;
    const bool depth_storage=source.depth_target||view.depth_target;
    if(source.depth_target||view.depth_target){
        // A sampled view can use the same exact single depth plane. This does
        // not make the depth allocation UAV-capable or admit a color writer.
        if(source.writable||view.writable||
           (source.depth_target&&!IsIndependentGuestDepthStorage(source))||
           (view.depth_target&&!IsIndependentGuestDepthStorage(view)))return false;
        source.depth_target=view.depth_target=false;
    }
    if(((source.data_format==9&&(source.number_format==1||source.number_format==5||
        view.number_format==1||view.number_format==5)&&!(source.UsesRawPacked10()&&view.UsesRawPacked10()))||(source.data_format>=16&&source.data_format<=19))&&
       source.writable!=view.writable)
        return false;
    // Ordinary color allocations already use their DXGI typeless family even
    // without a guest writer. Original mutable-format readonly views therefore
    // share those bits, rather than creating a converted snapshot. Expanded
    // signed packed10 and depth retain their distinct representation/aspects.
    if(!source.writable&&!view.writable&&source.number_format!=view.number_format){
        const bool expanded_packed=source.data_format==9&&(source.number_format==1||
            source.number_format==5||view.number_format==1||view.number_format==5)&&
            !(source.UsesRawPacked10()&&view.UsesRawPacked10());
        if(depth_storage||expanded_packed||source.data_format!=view.data_format||
           GuestStorageFormat(source.data_format,DXGI_FORMAT_UNKNOWN)==DXGI_FORMAT_UNKNOWN)return false;
    }
    source.writable=view.writable=true;
    source.swizzle=view.swizzle={4,5,6,7};
    return CompatibleGuestWritableStorage(source,view);
}
bool GuestTextureCache::CanBindReadonlyWordView(const Entry& image,const GuestTextureDesc& view)const{
    return CanBindReadonlyWordSrv(image,view);
}
bool GuestTextureCache::CanRetainReadonlyWordStorage(const GuestTextureDesc& desc)const{
    // Sharing tests deliberately exclude an identical descriptor. Retention
    // asks whether this original image can have a real mutable-word backing.
    return IsReadonlyMultisampleWordClass(desc)||
        !ReadonlyWordCasts(desc,texture_allocator_.SupportsFormatCasting()).empty();
}
bool GuestTextureCache::HasOwnedShaderStorage(const GuestTextureDesc& desc)const{
    const auto [first,last]=address_index_.equal_range(desc.address);
    for(auto it=first;it!=last;++it)if(it->second->gpu_owned&&
        (CompatibleGuestShaderStorage(it->second->desc,desc)||CanBindReadonlyWordView(*it->second,desc)||
         (CompatibleGuestColorSubview(it->second->desc,it->second->layout,desc,LayoutGuestTexture(desc))&&
          CanBindNativeWordFormat(*it->second,desc))||
         (it->second->desc.depth_target&&CompatibleGuestDepthSubview(it->second->desc,it->second->layout,
             desc,LayoutGuestTexture(desc)))||CompatibleGuestPackedStorage(it->second->desc,desc)))return true;
    return false;
}
bool CompatibleGuestReadonlyWordStorage(GuestTextureDesc source,GuestTextureDesc view){
    // Readonly binds can retain an already GPU-written R32 allocation. This
    // normalizes only the comparison; source permissions/history never change.
    const bool raw=source.UsesRaw32();
    if(source.writable&&(source.data_format==4||raw)){
        source.writable=false;source.native_raw32=false;
        if(source.data_format==9&&raw)source.native_packed10_raw=true;
    }
    const bool raw_family=IsReadonlyRawR32Class(source)&&IsReadonlyRawR32Class(view);
    if((source.data_format==view.data_format&&source.native_packed10_raw==view.native_packed10_raw)||
       (!raw_family&&(!IsDirectReadonlyWordClass(source)||!IsDirectReadonlyWordClass(view))))return false;
    source.data_format=view.data_format=4;source.number_format=view.number_format=4;
    source.native_raw32=view.native_raw32=false;
    source.native_packed10_raw=view.native_packed10_raw=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
bool CompatibleGuestImageMipGeometry(GuestTextureDesc a,const GuestTextureLayout& al,
    GuestTextureDesc b,const GuestTextureLayout& bl){
    if(a.depth_target||b.depth_target){
        if(a.writable||b.writable||
           (a.depth_target&&!IsIndependentGuestDepthStorage(a,al))||
           (b.depth_target&&!IsIndependentGuestDepthStorage(b,bl)))return false;
        a.depth_target=b.depth_target=false;
    }
    if(a.samples!=1||b.samples!=1||al.block_extent!=1||bl.block_extent!=1||
       a.render_target||b.render_target||a.depth_target||b.depth_target||a.stencil_address||b.stencil_address||
       (al.expanded_bytes_per_pixel&&!HasExactPackedExpansion(a,al))||
       (bl.expanded_bytes_per_pixel&&!HasExactPackedExpansion(b,bl))||
       al.bytes_per_pixel!=bl.bytes_per_pixel||al.mips.size()!=a.levels||bl.mips.size()!=b.levels)
        return false;
    const auto count=std::min(a.levels,b.levels);
    if((a.levels<=b.levels?al.guest_bytes:bl.guest_bytes)>
       (a.levels<=b.levels?bl.guest_bytes:al.guest_bytes))return false;
    a.levels=b.levels=std::max(a.levels,b.levels);
    auto writable_a=a,writable_b=b;writable_a.writable=writable_b.writable=true;
    if(!CompatibleGuestShaderStorage(writable_a,writable_b)&&!CompatibleGuestPackedStorage(a,b)&&
       !CompatibleGuestReadonlyWordStorage(a,b))return false;
    // Mips are stored mip-major, including all layers/depth slices. Equal
    // offsets and slice geometry prove the complete prefix, not just texel0.
    for(UINT mip=0;mip<count;++mip){const auto& x=al.mips[mip];const auto& y=bl.mips[mip];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||
           x.macro!=y.macro||x.thickness!=y.thickness)return false;
    }
    return true;
}
bool CompatibleGuestShaderMipPrefixes(GuestTextureDesc a,const GuestTextureLayout& al,
    GuestTextureDesc b,const GuestTextureLayout& bl){
    if(al.expanded_bytes_per_pixel||bl.expanded_bytes_per_pixel||
       !CompatibleGuestImageMipGeometry(a,al,b,bl))return false;
    a.levels=b.levels=std::max(a.levels,b.levels);
    return CompatibleGuestShaderStorage(a,b);
}
bool CompatibleGuestColorSampledMapping(const GuestTextureDesc& target,const GuestTextureLayout& physical,
    const GuestTextureDesc& sampled,const GuestTextureLayout& sampled_layout){
    if(!target.render_target||target.depth_target||sampled.render_target||
       !CompatibleGuestColorSubview(target,physical,sampled,sampled_layout)||
       target.first_mip>=target.levels||sampled.first_mip>=sampled.levels||
       !target.view_layers||!sampled.view_layers||target.first_layer>=target.layers||sampled.first_layer>=sampled.layers||
       target.view_layers>target.layers-target.first_layer||sampled.view_layers>sampled.layers-sampled.first_layer)return false;
    // Compatible native views can normalize pow2/storage hints. A disjoint
    // guest-memory proof additionally needs the actual physical mip mapping.
    if(physical.guest_bytes!=sampled_layout.guest_bytes||
       physical.bytes_per_pixel!=sampled_layout.bytes_per_pixel||physical.block_extent!=sampled_layout.block_extent||
       physical.expanded_bytes_per_pixel!=sampled_layout.expanded_bytes_per_pixel||physical.mips.size()!=sampled_layout.mips.size())return false;
    for(size_t i=0;i<physical.mips.size();++i){const auto& a=physical.mips[i];const auto& b=sampled_layout.mips[i];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||
           a.macro!=b.macro||a.thickness!=b.thickness)return false;}
    return true;
}
bool DisjointGuestColorSampledViews(const GuestTextureDesc& target,const GuestTextureLayout& physical,
    const GuestTextureDesc& sampled,const GuestTextureLayout& sampled_layout){
    if(!CompatibleGuestColorSampledMapping(target,physical,sampled,sampled_layout))return false;
    return target.first_mip<sampled.first_mip||target.first_mip>=sampled.levels||
        target.first_layer>=sampled.first_layer+sampled.view_layers||
        sampled.first_layer>=target.first_layer+target.view_layers;
}
bool CompatibleGuestAttachmentStorage(GuestTextureDesc source,GuestTextureDesc view){
    if((!source.render_target&&!source.depth_target)||(!view.render_target&&!view.depth_target))return false;
    source.first_mip=view.first_mip=0;
    source.first_layer=view.first_layer=0;
    source.view_layers=view.view_layers=1;
    return source==view;
}
bool CompatibleGuestDepthSubview(const GuestTextureDesc& source,const GuestTextureLayout& physical,
    const GuestTextureDesc& view,const GuestTextureLayout& view_layout){
    if(ReadonlyDepthSubresource(source,physical,view,view_layout))return true;
    // ResolveDepthOverlap returns the existing image when a smaller binding
    // needs no aspect/format recreation. Keep its full mip-major guest layout:
    // fewer requested layers change guest mip offsets but NOT the cached I.
    if(!IsGuestDepthPlaneStorage(source,physical)||view.writable||view.render_target||
       view.volume_depth||view.one_dimensional||view.address!=source.address||
       view.width!=source.width||view.height!=source.height||view.pitch!=source.pitch||
       view.data_format!=source.data_format||view.number_format!=source.number_format||
       view.samples!=source.samples||!view.levels||view.levels>source.levels||
       !view.layers||view.layers>source.layers||view.first_mip>=view.levels||
       view.first_layer>=view.layers||!view.view_layers||view.view_layers>view.layers-view.first_layer||
       view_layout.block_extent!=1||view_layout.expanded_bytes_per_pixel||
       view_layout.bytes_per_pixel!=physical.bytes_per_pixel||view_layout.mips.size()!=view.levels)
        return false;
    // An actual depth attachment must keep matching aspects. A sampled color
    // view can omit S; its native SRV still uses the parent's depth plane format.
    return view.depth_target?view.stencil_address==source.stencil_address:!view.stencil_address;
}
bool CompatibleGuestColorSubview(const GuestTextureDesc& source,const GuestTextureLayout& physical,
    const GuestTextureDesc& view,const GuestTextureLayout& requested){
    // Pinned ResolveOverlap keeps the existing compatible image when the
    // requested resource extent is smaller. CB slice_max bounds the view; it
    // must not split a sampled array into independent per-face allocations.
    if(source.writable||view.writable||source.depth_target||view.depth_target||
       source.stencil_address||view.stencil_address||source.volume_depth||view.volume_depth||
       source.one_dimensional||view.one_dimensional||!view.layers||view.layers>source.layers||
       !view.levels||view.levels>source.levels||view.first_mip>=view.levels||
       view.first_layer>=view.layers||!view.view_layers||view.view_layers>view.layers-view.first_layer||
       physical.block_extent!=1||requested.block_extent!=1||physical.expanded_bytes_per_pixel||
       requested.expanded_bytes_per_pixel||physical.bytes_per_pixel!=requested.bytes_per_pixel||
       physical.mips.size()<view.levels||requested.mips.size()!=view.levels)return false;
    for(UINT mip=0;mip<view.levels;++mip){const auto& a=physical.mips[mip];const auto& b=requested.mips[mip];
        if(a.pitch!=b.pitch||a.height!=b.height||a.slice_bytes!=b.slice_bytes||
           a.macro!=b.macro||a.thickness!=b.thickness)return false;
    }
    auto owner=source,target=view;owner.layers=target.layers;owner.levels=target.levels;
    owner.render_target=target.render_target=false;
    owner.storage_height=target.storage_height=0;
    owner.storage_slice_bytes=target.storage_slice_bytes=0;
    owner.pow2_padding=target.pow2_padding=false;
    // Mutable numeric views share the physical bits. Keep expanded encodings
    // excluded above and validate the actual native cast permission at binding.
    // Normalizing only these comparison copies preserves the owner's codec,
    // guest role and allocation lifetime.
    return CompatibleGuestSampledStorage(owner,target)||CompatibleGuestShaderStorage(owner,target);
}
void MergeGuestColorAttachmentLayers(std::span<GuestTextureDesc> attachments){
    // CB slice_max bounds a view as well as the requested storage. The original
    // cache grows one compatible backing to the maximum requested layer count.
    // Preserve each view's active slices while giving simultaneous views the
    // same allocation description before any resource or descriptor is made.
    for(size_t i=0;i<attachments.size();++i){auto& a=attachments[i];
        if(!a.render_target||a.depth_target)continue;
        for(size_t j=0;j<i;++j){auto& b=attachments[j];
            if(!b.render_target||b.depth_target||a.address!=b.address)continue;
            auto x=a,y=b;const auto layers=std::max(x.layers,y.layers);
            x.layers=y.layers=layers;
            if(CompatibleGuestAttachmentStorage(x,y))a.layers=b.layers=layers;
        }
    }
}
bool CompatibleGuestPackedStorage(GuestTextureDesc source,GuestTextureDesc view){
    const auto packed_number=[](UINT value){return value==0||value==1||value==4||value==5;};
    const bool packed10=source.data_format==9&&view.data_format==9&&
        packed_number(source.number_format)&&packed_number(view.number_format);
    const bool raw32=(source.data_format==4||source.data_format==10)&&
        (view.data_format==4||view.data_format==10)&&
        (source.native_raw32||view.native_raw32||source.data_format!=view.data_format);
    const bool packed16=source.data_format>=16&&source.data_format<=19&&source.data_format==view.data_format&&
        source.number_format==0&&view.number_format==0&&source.writable!=view.writable;
    if((!packed10&&!packed16&&!raw32)||source.depth_target||view.depth_target||source.stencil_address||view.stencil_address)return false;
    const auto a=LayoutGuestTexture(source),b=LayoutGuestTexture(view);
    if(a.guest_bytes!=b.guest_bytes||a.mips.size()!=b.mips.size())return false;
    for(size_t i=0;i<a.mips.size();++i){const auto& x=a.mips[i];const auto& y=b.mips[i];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||
           x.macro!=y.macro||x.thickness!=y.thickness)return false;
    }
    source.writable=view.writable=source.render_target=view.render_target=false;
    if(packed10)source.number_format=view.number_format=0;
    if(raw32){source.data_format=view.data_format=4;source.number_format=view.number_format=4;}
    source.native_packed10_raw=view.native_packed10_raw=false;
    source.native_raw32=view.native_raw32=false;
    source.storage_height=view.storage_height=0;source.storage_slice_bytes=view.storage_slice_bytes=0;
    source.pow2_padding=view.pow2_padding=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
uint64_t GuestTextureCache::Entry::ResidentBytes()const{
    uint64_t bytes=allocation_bytes+expanded_backing_bytes+sampled_snapshot.size()+stencil_snapshot.size();
    bytes+=buffer_sync_allocation_bytes;
    for(const auto& buffer:upload_pool)bytes+=buffer.handle->GetDesc().Width;
    for(const auto& buffer:tiler_input_pool)bytes+=buffer.handle->GetDesc().Width;
    for(const auto& buffer:tiler_output_pool)bytes+=buffer.handle->GetDesc().Width;
    uint64_t readback{};
    if(readback_pool)for(const auto& buffer:*readback_pool)readback+=buffer.handle->GetDesc().Width;
    return bytes+std::max(readback_reserve_bytes,readback);
}
namespace {
uint32_t Align(uint32_t n,uint32_t a){return (n+a-1)&~(a-1);}
uint32_t Pow2(uint32_t n){return std::bit_ceil(n);}
std::pair<DXGI_FORMAT,uint32_t> Format(uint32_t data,uint32_t number){
    // Same unsupported-format fallback as pinned Vulkan Instance::GetSupportedFormat:
    // D3D12 has no R8/RG8 sRGB view. Preserve bytes and use UNORM for reads/writes.
    if(data==1){switch(number){case 0:case 9:return {DXGI_FORMAT_R8_UNORM,1};case 1:return {DXGI_FORMAT_R8_SNORM,1};case 4:return {DXGI_FORMAT_R8_UINT,1};case 5:return {DXGI_FORMAT_R8_SINT,1};}}
    if(data==3){switch(number){case 0:case 9:return {DXGI_FORMAT_R8G8_UNORM,2};case 1:return {DXGI_FORMAT_R8G8_SNORM,2};case 4:return {DXGI_FORMAT_R8G8_UINT,2};case 5:return {DXGI_FORMAT_R8G8_SINT,2};}}
    if(data==10){switch(number){case 0:return {DXGI_FORMAT_R8G8B8A8_UNORM,4};case 1:return {DXGI_FORMAT_R8G8B8A8_SNORM,4};case 4:return {DXGI_FORMAT_R8G8B8A8_UINT,4};case 5:return {DXGI_FORMAT_R8G8B8A8_SINT,4};case 9:return {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,4};}}
    if(data==4){switch(number){case 7:return {DXGI_FORMAT_R32_FLOAT,4};case 4:return {DXGI_FORMAT_R32_UINT,4};case 5:return {DXGI_FORMAT_R32_SINT,4};}}
    if(data==2){switch(number){case 0:return {DXGI_FORMAT_R16_UNORM,2};case 1:return {DXGI_FORMAT_R16_SNORM,2};case 4:return {DXGI_FORMAT_R16_UINT,2};case 5:return {DXGI_FORMAT_R16_SINT,2};case 7:return {DXGI_FORMAT_R16_FLOAT,2};}}
    if(data==5){switch(number){case 0:return {DXGI_FORMAT_R16G16_UNORM,4};case 1:return {DXGI_FORMAT_R16G16_SNORM,4};case 4:return {DXGI_FORMAT_R16G16_UINT,4};case 5:return {DXGI_FORMAT_R16G16_SINT,4};case 7:return {DXGI_FORMAT_R16G16_FLOAT,4};}}
    if(data==6&&number==7)return {DXGI_FORMAT_R11G11B10_FLOAT,4};
    if(data==9){if(number==0)return {DXGI_FORMAT_R10G10B10A2_UNORM,4};if(number==4)return {DXGI_FORMAT_R10G10B10A2_UINT,4};
        if(number==1)return {DXGI_FORMAT_R32G32B32A32_FLOAT,4};if(number==5)return {DXGI_FORMAT_R32G32B32A32_SINT,4};}
    if(data==34&&number==7)return {DXGI_FORMAT_R9G9B9E5_SHAREDEXP,4};
    if(data==11){switch(number){case 4:return {DXGI_FORMAT_R32G32_UINT,8};case 5:return {DXGI_FORMAT_R32G32_SINT,8};case 7:return {DXGI_FORMAT_R32G32_FLOAT,8};}}
    if(data==12){switch(number){case 0:return {DXGI_FORMAT_R16G16B16A16_UNORM,8};case 1:return {DXGI_FORMAT_R16G16B16A16_SNORM,8};case 4:return {DXGI_FORMAT_R16G16B16A16_UINT,8};case 5:return {DXGI_FORMAT_R16G16B16A16_SINT,8};case 7:return {DXGI_FORMAT_R16G16B16A16_FLOAT,8};}}
    if(data==13){switch(number){case 4:return {DXGI_FORMAT_R32G32B32A32_UINT,12};case 5:return {DXGI_FORMAT_R32G32B32A32_SINT,12};case 7:return {DXGI_FORMAT_R32G32B32A32_FLOAT,12};}}
    if(data==14){switch(number){case 4:return {DXGI_FORMAT_R32G32B32A32_UINT,16};case 5:return {DXGI_FORMAT_R32G32B32A32_SINT,16};case 7:return {DXGI_FORMAT_R32G32B32A32_FLOAT,16};}}
    if(number==0){
        if(data==16)return {DXGI_FORMAT_B5G6R5_UNORM,2};
        if(data==17||data==18)return {DXGI_FORMAT_B5G5R5A1_UNORM,2};
        if(data==19)return {DXGI_FORMAT_B4G4R4A4_UNORM,2};
    }
    if(data==35){if(number==0)return {DXGI_FORMAT_BC1_UNORM,8};if(number==9)return {DXGI_FORMAT_BC1_UNORM_SRGB,8};}
    if(data==36){if(number==0)return {DXGI_FORMAT_BC2_UNORM,16};if(number==9)return {DXGI_FORMAT_BC2_UNORM_SRGB,16};}
    if(data==37){if(number==0)return {DXGI_FORMAT_BC3_UNORM,16};if(number==9)return {DXGI_FORMAT_BC3_UNORM_SRGB,16};}
    if(data==38){if(number==0)return {DXGI_FORMAT_BC4_UNORM,8};if(number==1)return {DXGI_FORMAT_BC4_SNORM,8};}
    if(data==39){if(number==0)return {DXGI_FORMAT_BC5_UNORM,16};if(number==1)return {DXGI_FORMAT_BC5_SNORM,16};}
    if(data==40){if(number==0||number==7)return {DXGI_FORMAT_BC6H_UF16,16};if(number==1)return {DXGI_FORMAT_BC6H_SF16,16};}
    if(data==41){if(number==0)return {DXGI_FORMAT_BC7_UNORM,16};if(number==9)return {DXGI_FORMAT_BC7_UNORM_SRGB,16};}
    throw std::runtime_error("unsupported native guest texture format data="+std::to_string(data)+
        " number="+std::to_string(number));
}
std::vector<std::byte> ExpandSignedPacked10(std::span<const std::byte> source,uint32_t number){
    if((number!=1&&number!=5)||source.size()%4||source.size()>SIZE_MAX/4)
        throw std::invalid_argument("Invalid signed packed sampled data");
    std::vector<std::byte> result(source.size()*4);
    for(size_t pixel=0;pixel<source.size()/4;++pixel){
        uint32_t packed;memcpy(&packed,source.data()+pixel*4,4);
        for(uint32_t component=0;component<4;++component){
            const uint32_t bits=component==3?2:10;
            const uint32_t raw=(packed>>(component*10))&((1u<<bits)-1);
            const int32_t value=int32_t(raw)-((raw&(1u<<(bits-1)))?int32_t(1u<<bits):0);
            if(number==5)memcpy(result.data()+pixel*16+component*4,&value,4);
            else {const float normalized=std::max(float(value)/float((1u<<(bits-1))-1),-1.f);
                memcpy(result.data()+pixel*16+component*4,&normalized,4);}
        }
    }
    return result;
}
UINT Mapping(const std::array<uint32_t,4>& s){UINT result=D3D12_SHADER_COMPONENT_MAPPING_ALWAYS_SET_BIT_AVOIDING_ZEROMEM_MISTAKES;for(UINT i=0;i<4;++i){UINT c;if(s[i]<=1)c=s[i]+4;else if(s[i]>=4&&s[i]<=7)c=s[i]-4;else throw std::runtime_error("invalid guest component swizzle");result|=c<<(i*D3D12_SHADER_COMPONENT_MAPPING_SHIFT);}return result;}
}
GuestTextureDesc DescribeGuestTexture(const AmdGpu::Image& image){
    const bool one_dimensional=image.GetBaseType()==AmdGpu::ImageType::Color1D;
    const bool volume=image.type==uint64_t(AmdGpu::ImageType::Color3D);
    const bool cube=image.GetType()==AmdGpu::ImageType::Cube;
    const bool msaa=image.GetType()==AmdGpu::ImageType::Color2DMsaa||image.GetType()==AmdGpu::ImageType::Color2DMsaaArray;
    if(!one_dimensional&&!volume&&!cube&&!msaa&&image.type!=uint64_t(AmdGpu::ImageType::Color2D)&&image.type!=uint64_t(AmdGpu::ImageType::Color2DArray))
        throw std::runtime_error("Native image type="+std::to_string(image.type)+" format="+
            std::to_string(image.data_format)+" address="+std::to_string(image.Address())+
            " dimensions="+std::to_string(image.width+1)+"x"+std::to_string(image.height+1)+
            "x"+std::to_string(image.depth+1)+" tile="+std::to_string(image.tiling_index)+
            " needs native resource/view support");
    GuestTextureDesc d;d.address=image.base_address<<8;d.width=uint32_t(image.width)+1;d.height=uint32_t(image.height)+1;d.pitch=uint32_t(image.pitch)+1;
    d.alt_tile=GuestAltTile(image.alt_tile_mode);d.bank_swizzle=image.GetBankSwizzle();
    d.one_dimensional=one_dimensional;if(one_dimensional)d.height=1;
    if(volume)d.volume_depth=uint32_t(image.depth)+1;
    d.levels=image.NumLevels();d.samples=image.NumSamples();
    d.min_lod=uint32_t(image.min_lod);
    d.layers=image.NumLayers();
    d.pow2_padding=image.pow2pad;d.data_format=uint32_t(image.GetDataFmt());d.number_format=uint32_t(image.GetNumberFmt());d.tile_mode=uint32_t(image.tiling_index);
    d.first_mip=msaa?0:uint32_t(image.base_level);d.first_layer=std::min(uint32_t(image.base_array),d.layers-1);d.view_layers=image.NumViewLayers(true);
    if(volume){d.first_layer=0;d.view_layers=1;}
    if(d.pow2_padding)d.layers=Pow2(d.layers);
    const auto mapping=image.DstSelect();
    d.swizzle={uint32_t(mapping.r),uint32_t(mapping.g),uint32_t(mapping.b),uint32_t(mapping.a)};
    LayoutGuestTexture(d);return d;
}
GuestTextureDesc DescribeGuestColorBuffer(const AmdGpu::ColorBuffer& buffer,AmdGpu::CbDbExtent extent) {
    if(!buffer||buffer.info.compression||buffer.info.dcc_enable)
        throw std::runtime_error("Native guest color buffer needs valid uncompressed tiling: address="+
            std::to_string(buffer.Address())+" samples="+std::to_string(buffer.NumSamples())+
            " compression="+std::to_string(buffer.info.compression)+" dcc="+std::to_string(buffer.info.dcc_enable)+
            " alt="+std::to_string(buffer.info.alt_tile_mode)+" linear="+std::to_string(buffer.info.linear_general)+
            " fast_clear="+std::to_string(buffer.info.fast_clear)+" cmask="+std::to_string(buffer.CmaskAddress())+
            " format="+std::to_string(buffer.info.format)+" number="+std::to_string(buffer.info.number_type));
    GuestTextureDesc desc;desc.address=buffer.Address();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=buffer.NumSlices();
    desc.storage_height=buffer.Height();
    desc.storage_slice_bytes=buffer.GetColorSliceSize();
    desc.samples=buffer.NumSamples();
    desc.alt_tile=GuestAltTile(buffer.info.alt_tile_mode);
    desc.first_layer=buffer.BaseSlice();
    if(desc.first_layer>=desc.layers)throw std::out_of_range("Invalid guest color buffer layer view");
    desc.view_layers=desc.layers-desc.first_layer;desc.data_format=uint32_t(buffer.GetDataFmt());
    desc.number_format=uint32_t(buffer.GetNumberFmt());desc.tile_mode=uint32_t(buffer.GetTileMode());desc.render_target=true;
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=uint64_t(buffer.slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers*desc.samples)
        throw std::runtime_error("Native guest color buffer padded storage extent needs explicit layout");
    return desc;
}
// Uncompressed GCN CMASK: 0 selects CB_CLEAR_WORD, F selects expanded color.
uint64_t GuestCmaskBytes(const AmdGpu::ColorBuffer& b) {
    return (uint64_t(b.cmask_slice.tile_max)+1)*128*b.NumSlices();
}
namespace {
struct CmaskLayout {
    GuestCmaskAddress address;
    explicit CmaskLayout(const AmdGpu::ColorBuffer& b,bool alt_tile)
        : address(b.Pitch(),b.Height(),alt_tile?AmdGpu::GetAltPipeConfig(b.GetTileMode()):
            AmdGpu::GetPipeConfig(b.GetTileMode()),b.info.cmask_is_linear) {
        if(b.info.cmask_addr_type)
            throw std::runtime_error("Native TC-compatible CMASK requires FMASK-derived addressing");
        if(address.slice_bytes!=(uint64_t(b.cmask_slice.tile_max)+1)*128)
            throw std::runtime_error("Native CMASK descriptor does not match padded metadata slice");
    }
    GuestCmaskTile At(uint32_t layer,uint32_t x,uint32_t y) const {
        const auto [byte,shift]=address.At(layer,x,y);
        return {layer,x,y,byte,shift};
    }
};
}
GuestColorClearPlan PlanGuestColorClear(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<const std::byte> cmask) {
    GuestColorClearPlan plan;
    if(!b.info.fast_clear||!b.cmask_base_address)return plan;
    if(b.info.compression||b.info.dcc_enable||l.block_extent!=1)
        throw std::runtime_error("Native CMASK requires uncompressed color");
    if(cmask.size()<GuestCmaskBytes(b)||d.first_layer>=b.NumSlices()||
       !d.view_layers||d.view_layers>b.NumSlices()-d.first_layer)
        throw std::out_of_range("Native CMASK storage/view too short");
    const auto slice_bytes=GuestCmaskBytes(b)/b.NumSlices();
    for(uint32_t layer=d.first_layer;layer<d.first_layer+d.view_layers;++layer) {
        const auto slice=cmask.subspan(size_t(slice_bytes*layer),size_t(slice_bytes));
        if(std::all_of(slice.begin(),slice.end(),[](auto v){return v==std::byte{0xff};}))continue;
        if(std::all_of(slice.begin(),slice.end(),[](auto v){return v==std::byte{0};})) {
            plan.uniform_layers.push_back(layer);continue;
        }
        const CmaskLayout layout(b,d.alt_tile);
        for(uint32_t y=0;y<d.height;y+=8)for(uint32_t x=0;x<d.width;x+=8) {
            const auto tile=layout.At(layer,x,y);
            const auto code=(std::to_integer<unsigned>(cmask[tile.byte])>>tile.shift)&15;
            if(code!=0&&code!=15)
                throw std::runtime_error("Native single-sample CMASK unsupported tile code="+std::to_string(code)+
                    " address="+std::to_string(b.CmaskAddress())+" x="+std::to_string(x)+" y="+std::to_string(y));
            if(code==0)plan.tiles.push_back(tile);
        }
    }
    return plan;
}
bool GuestColorClearPending(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<const std::byte> cmask) {
    return PlanGuestColorClear(b,d,l,cmask).Pending();
}
bool MaterializeGuestColorClear(const AmdGpu::ColorBuffer& b,const GuestTextureDesc& d,
    const GuestTextureLayout& l,std::span<std::byte> color,std::span<std::byte> cmask) {
    // Validate every selected layer and tile before modifying either plane.
    const auto plan=PlanGuestColorClear(b,d,l,cmask);
    if(!plan.Pending())return false;
    if(color.size()<l.guest_bytes)throw std::out_of_range("Native CMASK color storage span too short");
    const uint32_t words[]{b.clear_word0,b.clear_word1};
    const uint32_t wide[]{b.clear_word0,b.clear_word0,b.clear_word0,b.clear_word1};
    const auto decoded=DecodeGuestColorClear(b);
    if(decoded&&(decoded->data_format!=d.data_format||decoded->number_format!=d.number_format||
                 decoded->pixel_bytes!=l.bytes_per_pixel))
        throw std::invalid_argument("CMASK typed clear does not match color storage");
    // Formats with an established reference codec share CPU/GPU component and
    // numeric conversion. Preserve the historical raw behavior for remaining
    // ambiguous formats; a storage mapping alone is not a typed-clear oracle.
    const auto* pixel=decoded?decoded->pixel.data():
        reinterpret_cast<const std::byte*>(l.bytes_per_pixel>=12?wide:words);
    const auto slice_bytes=GuestCmaskBytes(b)/b.NumSlices();
    if(!plan.uniform_layers.empty()) {
        std::vector<std::byte> linear(size_t(d.width)*d.height*d.samples*l.bytes_per_pixel);
        for(size_t offset=0;offset<linear.size();offset+=l.bytes_per_pixel)memcpy(linear.data()+offset,pixel,l.bytes_per_pixel);
        for(auto layer:plan.uniform_layers) {
            EncodeGuestSubresource(d,l,color,linear,0,layer);
            std::fill_n(cmask.begin()+size_t(slice_bytes*layer),size_t(slice_bytes),std::byte{0xff});
        }
    }
    for(const auto& tile:plan.tiles) {
        for(uint32_t y=tile.y;y<std::min(tile.y+8,d.height);++y)
        for(uint32_t x=tile.x;x<std::min(tile.x+8,d.width);++x)
        for(uint32_t sample=0;sample<d.samples;++sample) {
            const auto offset=GuestPixelOffset(d,l,0,tile.layer,x,y,sample);
            memcpy(color.data()+offset,pixel,l.bytes_per_pixel);
        }
        cmask[tile.byte]|=std::byte(15u<<tile.shift);
    }
    return true;
}
GuestTextureLayout LayoutGuestTexture(const GuestTextureDesc& d,std::source_location caller){
    if(!std::has_single_bit(d.samples)||d.samples>16)
        throw std::invalid_argument("Invalid guest sample count");
    if(d.samples>1&&(d.levels!=1||d.volume_depth||d.one_dimensional||d.data_format>=35))
        throw std::invalid_argument("Invalid multisample guest image dimensions/format");
    if(d.volume_depth&&(d.width>2048||d.height>2048||d.volume_depth>2048||d.layers!=1||d.one_dimensional||d.render_target||d.depth_target))
        throw std::invalid_argument("Invalid native 3D texture dimensions/usage");
    if(d.one_dimensional&&(d.height!=1||d.render_target||d.depth_target))
        throw std::invalid_argument("Invalid native 1D texture extent/attachment usage");
    const auto spatial_extent=std::max({d.width,d.height,d.volume_depth});
    if(!d.width||!d.height||d.width>16384||d.height>16384||d.pitch<d.width||d.pitch>32768||!d.layers||d.layers>2048||!d.levels||d.levels>15||d.levels>uint32_t(std::bit_width(spatial_extent)))
        throw std::runtime_error("Invalid guest dimensions/mips/layers");
    if(d.storage_height&&(d.storage_height<d.height||d.storage_height>32768||d.levels!=1))
        throw std::out_of_range("Invalid guest attachment physical height");
    if(d.tile_mode>26&&d.tile_mode!=31)throw std::invalid_argument("Invalid guest tile mode");
    if(d.first_mip>=d.levels||d.first_layer>=d.layers||!d.view_layers||d.view_layers>d.layers-d.first_layer)
        throw std::runtime_error("Invalid guest texture view range: address="+std::to_string(d.address)+
            " first_mip="+std::to_string(d.first_mip)+" levels="+std::to_string(d.levels)+
            " first_layer="+std::to_string(d.first_layer)+" layers="+std::to_string(d.layers)+
            " view_layers="+std::to_string(d.view_layers)+" caller="+caller.function_name()+
            ":"+std::to_string(caller.line()));
    Mapping(d.swizzle);
    const bool signed_packed10=d.data_format==9&&(d.number_format==1||d.number_format==5);
    if(signed_packed10&&d.depth_target)
        throw std::invalid_argument("Signed packed 10-bit attachment storage requires exact native packing");
    const auto [format,bpp]=Format(d.data_format,d.writable&&d.number_format==9?0:d.number_format);
    GuestTextureLayout layout;layout.format=format;layout.bytes_per_pixel=bpp;
    if(signed_packed10&&!d.writable&&!d.render_target&&!d.UsesRawPacked10())layout.expanded_bytes_per_pixel=16;
    // Sampled B4 MSAA uses exact packed expansion on adapters without native
    // B4 attachment support. Render targets retain native4-bit quantization
    // and blending, subject to the actual format/sample capability checks.
    if(d.data_format>=16&&d.data_format<=19&&
       (d.volume_depth||d.one_dimensional||(d.data_format==19&&d.samples>1&&!d.render_target))&&
       !d.writable&&d.number_format==0){
        layout.expanded_bytes_per_pixel=16;layout.format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    }
    // Integer physical lanes preserve untouched SNORM endpoint encodings.
    // Fragment export quantizes before fixed-function writes/logic operations;
    // normalized blending needs a separate ordered post-blend path.
    if(signed_packed10&&d.render_target)layout.format=DXGI_FORMAT_R10G10B10A2_UINT;
    if(d.UsesRaw32()||(d.writable&&d.data_format>=16&&d.data_format<=19))layout.format=DXGI_FORMAT_R32_UINT;
    if(d.data_format>=35&&d.data_format<=41){
        if(d.writable||d.render_target||d.depth_target)
            throw std::invalid_argument("Native BC texture requires a sampled image");
        layout.block_extent=4;
        if(d.volume_depth||d.one_dimensional||(d.width%4)||(d.height%4)){
            layout.expanded_bytes_per_pixel=d.data_format>=38&&d.data_format<=40?16:4;
            layout.format=layout.expanded_bytes_per_pixel==16?DXGI_FORMAT_R32G32B32A32_FLOAT:
                d.number_format==9?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }
    const auto mode=AmdGpu::TileMode(d.tile_mode);
    const auto array=AmdGpu::GetArrayMode(mode);
    const bool macro_image=AmdGpu::IsMacroTiled(array);
    const auto thickness=AmdGpu::GetMicroTileThickness(array);
    if(d.storage_slice_bytes&&d.levels!=1)throw std::invalid_argument("Explicit CB/DB allocation must have one mip");
    for(uint32_t mip=0;mip<d.levels;++mip){
        uint32_t pitch=std::max(d.pitch>>mip,1u),height=std::max((d.storage_height?d.storage_height:d.height)>>mip,1u);
        pitch=(pitch+layout.block_extent-1)/layout.block_extent;
        height=(height+layout.block_extent-1)/layout.block_extent;
        if(d.pow2_padding){pitch=Pow2(pitch);height=Pow2(height);}
        bool macro=false;uint32_t mip_thickness=1;
        if(d.storage_slice_bytes){
            // The pinned ImageInfo attachment constructors retain the declared
            // CB/DB pitch, height and slice size instead of recomputing a
            // sampled-image allocation from the active render area.
            macro=macro_image;mip_thickness=thickness;
        }else if(array==AmdGpu::ArrayMode::ArrayLinearGeneral){
            // Native VideoOut already uses linear-general images; retain their
            // declared stride instead of forcing the upstream UpdateSize assert.
        }else if(array==AmdGpu::ArrayMode::ArrayLinearAligned){
            const auto pa=std::max(8u,64u/bpp),sa=std::max(64u,256u/bpp);
            pitch=Align(pitch,pa);
            while((uint64_t(pitch)*height*d.samples)%sa)pitch+=pa;
        }else{
            uint32_t mw=8,mh=8;
            if(macro_image){
                if(d.samples<=8)std::tie(mw,mh)=VideoCore::GetMacroTileExtents(mode,bpp*8,d.samples,d.alt_tile);
                else{
                    const auto mt=AmdGpu::CalculateMacrotileMode(mode,bpp*8,d.samples);
                    const auto banks=d.alt_tile?AmdGpu::GetAltNumBanks(mt):AmdGpu::GetNumBanks(mt);
                    const auto bh=d.alt_tile?AmdGpu::GetAltBankHeight(mt):AmdGpu::GetBankHeight(mt);
                    const auto aspect=d.alt_tile?AmdGpu::GetAltMacrotileAspect(mt):AmdGpu::GetMacrotileAspect(mt);
                    const auto pipes=AmdGpu::GetPipeCount(d.alt_tile?AmdGpu::GetAltPipeConfig(mode):AmdGpu::GetPipeConfig(mode));
                    mw=8*pipes*AmdGpu::GetBankWidth(mt)*aspect;mh=8*banks*bh/aspect;
                }
                if(!mw||!mh)throw std::logic_error("Invalid guest macro tile extents");
                macro=mip==0||(pitch>=mw&&height>=mh);
            }
            mip_thickness=macro?thickness:std::min(thickness,4u);
            pitch=Align(pitch,macro?mw:8);height=Align(height,macro?mh:8);
            if(!macro)while((uint64_t(pitch)*height*bpp*d.samples*mip_thickness)%256)pitch+=8;
        }
        uint32_t depth=d.volume_depth?std::max(d.volume_depth>>mip,1u):1;
        if(d.pow2_padding)depth=Pow2(depth);
        const auto slices=Align(depth*d.layers,mip_thickness);
        const uint64_t minimum_slice=uint64_t(pitch)*height*bpp*d.samples;
        const uint64_t slice=d.storage_slice_bytes?d.storage_slice_bytes:minimum_slice;
        if(slice<minimum_slice)throw std::invalid_argument("CB/DB slice is shorter than its physical extent");
        layout.mips.push_back({pitch,height,layout.guest_bytes,slice,macro,mip_thickness});
        layout.guest_bytes+=slice*slices;
        layout.logical_bytes+=uint64_t((std::max(d.width>>mip,1u)+layout.block_extent-1)/layout.block_extent)*
            ((std::max(d.height>>mip,1u)+layout.block_extent-1)/layout.block_extent)*bpp*
            (d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers)*d.samples;
    }
    if(d.address>UINT64_MAX-layout.guest_bytes)throw std::out_of_range("Guest texture address overflow");
    layout.copy_plans=std::make_shared<GuestTextureCopyPlans>();return layout;
}
GuestTextureDesc DescribeGuestDepthBuffer(const AmdGpu::DepthBuffer& buffer,
    const AmdGpu::DepthView& view,AmdGpu::CbDbExtent extent) {
    if(!buffer.DepthValid()||buffer.z_info.tile_mode_index>7||view.slice_start>=view.NumSlices())
        throw std::runtime_error("Native guest depth needs valid depth tiling");
    GuestTextureDesc desc;desc.address=buffer.DepthAddress();desc.width=extent.Valid()?extent.width:buffer.Pitch();
    desc.height=extent.Valid()?extent.height:buffer.Height();desc.pitch=buffer.Pitch();desc.layers=view.NumSlices();
    desc.storage_height=buffer.Height();
    desc.storage_slice_bytes=buffer.GetDepthSliceSize();
    desc.samples=buffer.NumSamples();
    desc.first_layer=view.slice_start;desc.view_layers=desc.layers-desc.first_layer;desc.depth_target=true;
    if(buffer.StencilValid())desc.stencil_address=buffer.StencilAddress();
    desc.tile_mode=buffer.z_info.tile_mode_index;
    if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z32Float){desc.data_format=4;desc.number_format=7;}
    else if(buffer.z_info.format==AmdGpu::DepthBuffer::ZFormat::Z16){desc.data_format=2;desc.number_format=0;}
    else throw std::runtime_error("Unsupported native guest depth format");
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes!=u64(buffer.depth_slice.tile_max+1)*64*layout.bytes_per_pixel*desc.layers*desc.samples)
        throw std::runtime_error("Native guest depth allocation differs from declared slice stride");
    return desc;
}
GuestTextureDesc DescribeGuestStencil(const GuestTextureDesc& depth) {
    if(!depth.depth_target||!depth.stencil_address)throw std::invalid_argument("Missing guest stencil plane");
    auto stencil=depth;stencil.address=depth.stencil_address;stencil.stencil_address=0;
    stencil.depth_target=false;stencil.data_format=1;stencil.number_format=4;
    if(stencil.storage_slice_bytes)stencil.storage_slice_bytes/=Format(depth.data_format,depth.number_format).second;
    return stencil;
}
uint64_t GuestPixelOffset(const GuestTextureDesc& d,const GuestTextureLayout& l,uint32_t mip,uint32_t layer,uint32_t x,uint32_t y,uint32_t sample){
    if(sample>=d.samples||mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers)||
       x>=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent||
       y>=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent)
        throw std::out_of_range("Guest pixel/block coordinates");
    const auto& m=l.mips.at(mip);
    if(d.tile_mode==8||d.tile_mode==31)
        return m.offset+uint64_t(layer)*m.slice_bytes+(uint64_t(sample)*m.pitch*m.height+uint64_t(y)*m.pitch+x)*l.bytes_per_pixel;
    const GuestTileAddress address(AmdGpu::TileMode(d.tile_mode),l.bytes_per_pixel,d.samples);
    const auto offset=m.macro?address.Macro(x,y,layer,m.pitch,m.height,sample,d.bank_swizzle):
        address.Micro(x,y,layer,m.pitch,m.height,sample,m.thickness);
    const auto indexed=m.offset+offset;
    // Upstream's scalar-layout u32vec3 buffer indexes the complete tiled byte offset.
    return l.bytes_per_pixel==12?(indexed/12)*12:indexed;
}
namespace {
void CopyTexel(std::byte* dest,const std::byte* source,uint32_t bytes){
    switch(bytes){case 1:*dest=*source;break;case 2:memcpy(dest,source,2);break;case 4:memcpy(dest,source,4);break;
        case 8:memcpy(dest,source,8);break;case 12:memcpy(dest,source,12);break;case 16:memcpy(dest,source,16);break;default:throw std::logic_error("Invalid texel/block size");}
}
template<size_t Bytes,bool Encode>
void CopyPlannedTiles(const GuestTextureCopyPlans::Plan& plan,uint32_t width,uint32_t height,
    const std::byte* source,std::byte* dest){
    const auto rows=(height+7)/8,columns=(width+7)/8;
    static const unsigned workers=std::min(14u,std::max(1u,std::thread::hardware_concurrency()/2>2?
        std::thread::hardware_concurrency()/2-2:1u));
    const auto jobs=uint64_t(width)*height*Bytes>=1024*1024?std::min(workers,rows):1u;
    std::array<unsigned,14> indices{};std::iota(indices.begin(),indices.end(),0u);
    auto copy_rows=[&](unsigned job){
    for(uint32_t tile_row=rows*job/jobs;tile_row<rows*(job+1)/jobs;++tile_row){
    const auto ty=tile_row*8;
    for(uint32_t tx=0;tx<width;tx+=8){
        const auto tile=size_t(tile_row)*columns+tx/8;
        const auto* bases=plan.bases.data()+tile*plan.pieces;
        for(uint32_t y=0;y<std::min(8u,height-ty);++y){
            const auto row=(size_t(ty+y)*width+tx)*Bytes;
            for(uint32_t x=0;x<std::min(8u,width-tx);++x){
                const auto& pixel=plan.pixels[y*8+x];
                const auto guest=size_t(bases[pixel[0]])+pixel[1],linear=row+x*Bytes;
                if constexpr(Encode)memcpy(dest+guest,source+linear,Bytes);
                else memcpy(dest+linear,source+guest,Bytes);
            }
        }
    }
    }
    };
    // Only disjoint texels are written; layout, padding and mip/layer offsets
    // remain identical. Use the persistent Windows STL pool for large surfaces.
    if(jobs==1)copy_rows(0);
    else std::for_each(std::execution::par,indices.begin(),indices.begin()+jobs,copy_rows);
}
void CopyGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,uint32_t mip,uint32_t layer,
    const std::byte* source,std::byte* dest,bool encode){
    const auto width=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto height=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    const auto bpp=l.bytes_per_pixel;const auto& ml=l.mips.at(mip);
    if(d.samples>1){
        for(uint32_t sample=0;sample<d.samples;++sample)
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
            const auto guest=GuestPixelOffset(d,l,mip,layer,x,y,sample);
            const auto linear=((uint64_t(sample)*height+y)*width+x)*bpp;
            if(guest+bpp>l.guest_bytes)throw std::out_of_range("Guest MSAA texel exceeds backing");
            CopyTexel(dest+(encode?guest:linear),source+(encode?linear:guest),bpp);
        }
        return;
    }
    if(d.tile_mode==8||d.tile_mode==31){
        for(uint32_t y=0;y<height;++y){const auto guest=ml.offset+layer*ml.slice_bytes+uint64_t(y)*ml.pitch*bpp;
            const auto linear=uint64_t(y)*width*bpp;
            if(guest+uint64_t(width)*bpp>l.guest_bytes)throw std::out_of_range("Linear guest row exceeds backing");
            memcpy(dest+(encode?guest:linear),source+(encode?linear:guest),size_t(width)*bpp);}
        return;
    }
    if(ml.thickness>1||bpp==12){
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
            const auto guest=GuestPixelOffset(d,l,mip,layer,x,y),linear=(uint64_t(y)*width+x)*bpp;
            if(guest+bpp>l.guest_bytes)throw std::out_of_range("Guest volume texel exceeds backing");
            CopyTexel(dest+(encode?guest:linear),source+(encode?linear:guest),bpp);}
        return;
    }
    GuestTextureDesc tile;tile.width=tile.height=tile.pitch=8;tile.tile_mode=d.tile_mode;
    GuestTextureLayout local;local.bytes_per_pixel=bpp;local.mips.push_back({8,8,0,uint64_t(64)*bpp,false});
    std::array<uint32_t,64> offsets{};
    for(uint32_t y=0;y<8;++y)for(uint32_t x=0;x<8;++x)offsets[y*8+x]=uint32_t(GuestPixelOffset(tile,local,0,0,x,y));
    // A piece cannot exceed the microtile itself. R8/R16 microtiles occupy
    // 64/128 bytes even when the macro tile split is 256 bytes.
    const auto chunk=ml.macro?std::min({64*bpp,256u,GuestTileAddress(AmdGpu::TileMode(d.tile_mode),bpp,d.samples).split}):64*bpp;
    auto build_plan=[&]{
        auto plan=std::make_shared<GuestTextureCopyPlans::Plan>();plan->pieces=(64*bpp+chunk-1)/chunk;
        for(uint32_t i=0;i<64;++i)plan->pixels[i]={offsets[i]/chunk,offsets[i]%chunk};
        plan->bases.resize(size_t((width+7)/8)*((height+7)/8)*plan->pieces);
        size_t tile_index{};
        for(uint32_t ty=0;ty<height;ty+=8)for(uint32_t tx=0;tx<width;tx+=8,++tile_index){
            std::array<bool,16> ready{};
            for(uint32_t y=0;y<std::min(8u,height-ty);++y)for(uint32_t x=0;x<std::min(8u,width-tx);++x){
                const auto& pixel=plan->pixels[y*8+x];const auto piece=pixel[0],within=pixel[1];
                if(!ready[piece]){
                    const auto base=GuestPixelOffset(d,l,mip,layer,tx+x,ty+y)-within;
                    if(base>l.guest_bytes||chunk>l.guest_bytes-base)throw std::out_of_range("Guest tile exceeds backing");
                    plan->bases[tile_index*plan->pieces+piece]=base;ready[piece]=true;
                }
            }
        }
        return plan;
    };
    std::shared_ptr<const GuestTextureCopyPlans::Plan> plan;
    if(l.copy_plans){
        std::lock_guard lock(l.copy_plans->mutex);
        auto& stored=l.copy_plans->subresources[{mip,layer}];if(!stored)stored=build_plan();plan=stored;
    }else plan=build_plan();
    switch(bpp){
    case 1:if(encode)CopyPlannedTiles<1,true>(*plan,width,height,source,dest);else CopyPlannedTiles<1,false>(*plan,width,height,source,dest);break;
    case 2:if(encode)CopyPlannedTiles<2,true>(*plan,width,height,source,dest);else CopyPlannedTiles<2,false>(*plan,width,height,source,dest);break;
    case 4:if(encode)CopyPlannedTiles<4,true>(*plan,width,height,source,dest);else CopyPlannedTiles<4,false>(*plan,width,height,source,dest);break;
    case 8:if(encode)CopyPlannedTiles<8,true>(*plan,width,height,source,dest);else CopyPlannedTiles<8,false>(*plan,width,height,source,dest);break;
    case 16:if(encode)CopyPlannedTiles<16,true>(*plan,width,height,source,dest);else CopyPlannedTiles<16,false>(*plan,width,height,source,dest);break;
    default:throw std::logic_error("Invalid planned texel/block size");
    }
}
}
std::vector<std::byte> DecodeGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,std::span<const std::byte> bytes,uint32_t mip,uint32_t layer){
    if(bytes.size()<l.guest_bytes)throw std::invalid_argument("short guest texture backing");
    if(mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers))throw std::out_of_range("guest texture subresource");
    uint32_t w=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent,h=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    std::vector<std::byte> linear(uint64_t(w)*h*l.bytes_per_pixel*d.samples);
    CopyGuestSubresource(d,l,mip,layer,bytes.data(),linear.data(),false);return linear;
}
D3D12_SHADER_RESOURCE_VIEW_DESC GuestTextureCache::Entry::Srv()const{
    if(IsDirectReadonlyWordClass(desc)&&texture.handle&&!CanBindNativeWordFormat(*this,desc))
        throw std::invalid_argument("Native readonly word view requires its actual resource cast list");
    // Raw readonly packed10 is decoded and swizzled per texel in the shader.
    // The SRV must expose its unmodified R32 word even for forced guest channels.
    auto swizzle=desc.UsesRawPacked10()||desc.UsesRaw32()?std::array<uint32_t,4>{4,5,6,7}:desc.swizzle;
    if(desc.data_format==13)for(auto& component:swizzle)if(component==7)component=1;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=layout.format;view.Shader4ComponentMapping=Mapping(swizzle);
    const float minimum=std::min(float(desc.min_lod)/256.0f,float(desc.levels-1));
    if(desc.stencil_address)view.Format=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    if(desc.samples>1){
        if(desc.layers==1)view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.volume_depth){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;view.Texture3D={desc.first_mip,desc.levels-desc.first_mip,minimum};return view;}
    if(desc.one_dimensional) {
        if(desc.layers==1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1D;view.Texture1D.MostDetailedMip=desc.first_mip;view.Texture1D.ResourceMinLODClamp=minimum;view.Texture1D.MipLevels=desc.levels-desc.first_mip;}
        else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray.MostDetailedMip=desc.first_mip;view.Texture1DArray.ResourceMinLODClamp=minimum;view.Texture1DArray.MipLevels=desc.levels-desc.first_mip;view.Texture1DArray.FirstArraySlice=desc.first_layer;view.Texture1DArray.ArraySize=desc.view_layers;}
        return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Texture2D.MostDetailedMip=desc.first_mip;view.Texture2D.ResourceMinLODClamp=minimum;view.Texture2D.MipLevels=desc.levels-desc.first_mip;}
    else{view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MostDetailedMip=desc.first_mip;view.Texture2DArray.ResourceMinLODClamp=minimum;view.Texture2DArray.MipLevels=desc.levels-desc.first_mip;view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}return view;
}
D3D12_SHADER_RESOURCE_VIEW_DESC GuestTextureCache::Entry::Srv(const GuestTextureDesc& sampled_view)const{
    if(sampled_view==desc)return Srv();
    if(auto sub=ReadonlyDepthSubresource(desc,layout,sampled_view,LayoutGuestTexture(sampled_view))){
        auto native=desc;
        native.first_mip=sub->first;native.levels=sub->first+1;
        native.first_layer=sub->second+sampled_view.first_layer;
        native.view_layers=sampled_view.view_layers;
        native.min_lod=sub->first*256+sampled_view.min_lod;native.swizzle=sampled_view.swizzle;
        return Srv(native);
    }
    const bool depth_alias=desc.depth_target&&
        CompatibleGuestDepthSubview(desc,layout,sampled_view,LayoutGuestTexture(sampled_view));
    const bool color_alias=CompatibleGuestColorSubview(desc,layout,sampled_view,LayoutGuestTexture(sampled_view));
    const bool shader_alias=!sampled_view.writable&&
        (CompatibleGuestShaderStorage(desc,sampled_view)||
         CanBindReadonlyWordSrv(*this,sampled_view)||
         (desc.levels>=sampled_view.levels&&CompatibleGuestShaderMipPrefixes(desc,layout,
             sampled_view,LayoutGuestTexture(sampled_view))));
    const bool block_alias=layout.block_extent==4&&!sampled_view.writable&&
        CanBindReadonlyBcView(*this,sampled_view,LayoutGuestTexture(sampled_view));
    if((!CompatibleGuestSampledStorage(desc,sampled_view)&&!shader_alias&&!block_alias&&!depth_alias&&!color_alias)||sampled_view.first_mip>=sampled_view.levels||
       sampled_view.first_layer>=sampled_view.layers||!sampled_view.view_layers||
       sampled_view.view_layers>sampled_view.layers-sampled_view.first_layer)
        throw std::invalid_argument("Incompatible native sampled view storage/range");
    if(((shader_alias&&desc.data_format!=sampled_view.data_format)||
        (color_alias&&layout.format!=LayoutGuestTexture(sampled_view).format))&&
       !CanBindNativeWordFormat(*this,sampled_view))
        throw std::invalid_argument("Native image word alias requires a compatible resource format list");
    Entry view;view.desc=sampled_view;view.desc.stencil_address=desc.stencil_address;
    if(color_alias)view.desc.layers=desc.layers;
    view.layout=shader_alias||block_alias||depth_alias||color_alias?LayoutGuestTexture(sampled_view):layout;return view.Srv();
}
void EncodeGuestSubresource(const GuestTextureDesc& d,const GuestTextureLayout& l,
    std::span<std::byte> guest,std::span<const std::byte> linear,uint32_t mip,uint32_t layer) {
    if(mip>=d.levels||layer>=(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers))throw std::out_of_range("guest encode texture subresource");
    auto width=(std::max(d.width>>mip,1u)+l.block_extent-1)/l.block_extent,height=(std::max(d.height>>mip,1u)+l.block_extent-1)/l.block_extent;
    if(guest.size()<l.guest_bytes||linear.size()!=u64(width)*height*l.bytes_per_pixel*d.samples)
        throw std::invalid_argument("invalid guest texture encode backing");
    CopyGuestSubresource(d,l,mip,layer,linear.data(),guest.data(),true);
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::Uav(uint32_t mip)const {
    if(!desc.writable||mip<desc.first_mip||mip>=desc.levels)throw std::out_of_range("invalid guest texture UAV mip/access");
    if(texture.handle&&!(texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Native texture resource does not allow UAV views");
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=layout.format;
    if(desc.volume_depth){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE3D;view.Texture3D={mip,0,std::max(desc.volume_depth>>mip,1u)};return view;}
    if(desc.one_dimensional) {
        if(desc.layers==1){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1D;view.Texture1D.MipSlice=mip;}
        else {view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE1DARRAY;view.Texture1DArray={mip,desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;}
    else {view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    return view;
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::Uav(const GuestTextureDesc& writable_view,uint32_t mip)const {
    auto written_source=desc;written_source.writable=true;
    const bool mutable_readonly=IsDirectReadonlyWordClass(desc)&&writable_view.writable&&
        texture.handle&&(texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)&&
        CompatibleGuestWritableStorage(written_source,writable_view)&&CanBindNativeWordFormat(*this,writable_view);
    if(writable_view==desc)return Uav(mip);
    if((!mutable_readonly&&!CompatibleGuestWritableStorage(desc,writable_view)&&
        !(desc.writable&&writable_view.writable&&desc.levels>=writable_view.levels&&
          CompatibleGuestShaderMipPrefixes(desc,layout,writable_view,LayoutGuestTexture(writable_view))))||
       writable_view.first_layer>=writable_view.layers||!writable_view.view_layers||
       writable_view.view_layers>writable_view.layers-writable_view.first_layer)
        throw std::invalid_argument("Incompatible native writable view storage/range");
    if(desc.data_format!=writable_view.data_format&&!CanBindNativeWordFormat(*this,writable_view))
        throw std::invalid_argument("Native writable word alias requires a compatible resource format list");
    if(texture.handle&&!(texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Native texture resource does not allow UAV views");
    Entry view;view.desc=writable_view;view.layout=LayoutGuestTexture(writable_view);return view.Uav(mip);
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::OrderedColorUav(
    const GuestTextureDesc& attachment)const {
    if((!CompatibleGuestAttachmentStorage(desc,attachment)&&
        !CompatibleGuestColorSubview(desc,layout,attachment,LayoutGuestTexture(attachment)))||!attachment.render_target||
       attachment.writable||attachment.depth_target||attachment.stencil_address||attachment.samples!=1||
       !(IsOrderedScalarClass(attachment)||IsOrderedIntegerClass(attachment)||IsOrderedPacked16Class(attachment)||(attachment.data_format==9&&(attachment.number_format==0||attachment.number_format==1))||
         (attachment.data_format==3&&(attachment.number_format==0||attachment.number_format==1||attachment.number_format==9))||
         (attachment.data_format==10&&(attachment.number_format==0||attachment.number_format==1||attachment.number_format==9))||
         ((attachment.data_format==5||attachment.data_format==12)&&
          (attachment.number_format==0||attachment.number_format==1||attachment.number_format==7))||
         IsOrderedFloatWordClass(attachment)||(attachment.data_format==6&&attachment.number_format==7))||
       attachment.first_mip>=attachment.levels||!attachment.view_layers||
       attachment.first_layer>=attachment.layers||
       attachment.view_layers>attachment.layers-attachment.first_layer)
        throw std::invalid_argument("Invalid native ordered color view");
    if(!texture.handle||!(texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)||
       (!texture.AllowsCast(OrderedColorWordFormat(attachment))&&texture.handle->GetDesc().Format!=OrderedColorWordStorage(attachment)))
        throw std::runtime_error("Native ordered color requires matching UINT format casting");
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=OrderedColorWordFormat(attachment);
    if(cache)cache->ValidateWritableFormat(view.Format);
    view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    view.Texture2DArray={attachment.first_mip,attachment.first_layer,attachment.view_layers,0};
    return view;
}
D3D12_UNORDERED_ACCESS_VIEW_DESC GuestTextureCache::Entry::StorageUav(const GuestTextureDesc& writable_view,uint32_t sample)const{
    if(!CanUseMultisampleStorageView(*this,writable_view)||sample>=storage_samples.size()||
       storage_samples.size()!=desc.samples||writable_view.first_layer>=writable_view.layers||
       !writable_view.view_layers||writable_view.view_layers>writable_view.layers-writable_view.first_layer)
        throw std::invalid_argument("Incompatible native multisample storage writer view/sample");
    const auto& storage=storage_samples[sample];
    if(!storage.handle)throw std::invalid_argument("Native MSAA storage plane has no resource");
    const auto native=storage.handle->GetDesc();
    if(native.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||native.SampleDesc.Count!=1||
       native.MipLevels!=1||native.Width!=desc.width||native.Height!=desc.height||
       native.DepthOrArraySize!=desc.layers||!(native.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Native MSAA storage plane lacks its exact writable geometry");
    if(IsOrderedMultisampleColorClass(writable_view)){
        if(!storage.AllowsCast(OrderedColorWordFormat(writable_view))&&native.Format!=OrderedColorWordStorage(writable_view))
            throw std::invalid_argument("Native ordered MSAA plane lacks matching UINT format casting");
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=OrderedColorWordFormat(writable_view);
        if(cache)cache->ValidateWritableFormat(view.Format);
        view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        view.Texture2DArray={0,writable_view.first_layer,writable_view.view_layers,0};
        return view;
    }
    // Build the UAV from the actual single-sample plane. The authoritative
    // MSAA texture is not UAV-capable; its original readonly class stays put.
    Entry plane;plane.desc=writable_view;plane.desc.samples=1;
    plane.layout=LayoutGuestTexture(plane.desc);plane.texture=storage;
    if(!CanBindNativeWordFormat(plane,plane.desc))
        throw std::invalid_argument("Native MSAA storage plane lacks the requested format cast");
    return plane.Uav(0);
}
D3D12_RENDER_TARGET_VIEW_DESC GuestTextureCache::Entry::Rtv(uint32_t mip)const {
    if(!desc.render_target||mip<desc.first_mip||mip>=desc.levels)throw std::out_of_range("Invalid guest texture RTV mip/access");
    D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=layout.format;
    if(texture.handle&&texture.handle->GetDesc().SampleDesc.Count>1){
        if(desc.layers==1)view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
        return view;
    }
    if(desc.volume_depth){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE3D;
        view.Texture3D={mip,0,std::max(desc.volume_depth>>mip,1u)};return view;}
    if(desc.one_dimensional){
        if(desc.layers==1){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE1D;view.Texture1D.MipSlice=mip;}
        else{view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
            view.Texture1DArray={mip,desc.first_layer,desc.view_layers};}return view;
    }
    if(desc.layers==1){view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=mip;}
    else {view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    return view;
}
D3D12_RENDER_TARGET_VIEW_DESC GuestTextureCache::Entry::Rtv(const GuestTextureDesc& attachment,uint32_t mip)const{
    if(!CompatibleGuestAttachmentStorage(desc,attachment)&&
       !CompatibleGuestColorSubview(desc,layout,attachment,LayoutGuestTexture(attachment)))
        throw std::invalid_argument("Incompatible native render target view");
    if(!texture.handle||!(texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
        throw std::invalid_argument("Native color parent lacks render target capability");
    if(!CanBindNativeWordFormat(*this,attachment))
        throw std::invalid_argument("Native color parent lacks the requested render target format cast");
    Entry view;view.desc=attachment;view.layout=LayoutGuestTexture(attachment);view.texture.handle=texture.handle;
    view.desc.layers=desc.layers;
    return view.Rtv(mip);
}
D3D12_DEPTH_STENCIL_VIEW_DESC GuestTextureCache::Entry::Dsv(bool read_only_depth,bool read_only_stencil)const {
    if(!desc.depth_target)throw std::logic_error("Guest image is not a depth attachment");
    D3D12_DEPTH_STENCIL_VIEW_DESC view{};
    view.Format=desc.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        layout.bytes_per_pixel==4?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM;
    if(desc.samples>1){
        if(desc.layers==1)view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMS;
        else{view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={desc.first_layer,desc.view_layers};}
    }else if(desc.layers==1){view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;view.Texture2D.MipSlice=desc.first_mip;}
    else {view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        view.Texture2DArray.MipSlice=desc.first_mip;
        view.Texture2DArray.FirstArraySlice=desc.first_layer;view.Texture2DArray.ArraySize=desc.view_layers;}
    if(read_only_depth)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    if(read_only_stencil&&desc.stencil_address)view.Flags|=D3D12_DSV_FLAG_READ_ONLY_STENCIL;
    return view;
}
D3D12_DEPTH_STENCIL_VIEW_DESC GuestTextureCache::Entry::Dsv(const GuestTextureDesc& attachment,bool read_only_depth,bool read_only_stencil)const{
    if(!CompatibleGuestAttachmentStorage(desc,attachment)&&
        !CompatibleGuestDepthSubview(desc,layout,attachment,LayoutGuestTexture(attachment)))
        throw std::invalid_argument("Incompatible native depth target view");
    Entry view;view.desc=attachment;view.layout=layout;
    return view.Dsv(read_only_depth,read_only_stencil);
}
void GuestTextureCache::Entry::RememberSnapshot(std::span<const std::byte> bytes,std::span<const std::byte> stencil){
    if(desc.writable||bytes.size()<layout.guest_bytes||
       (desc.stencil_address&&stencil.size()<stencil_layout.guest_bytes))
        throw std::invalid_argument("Invalid coherent texture snapshot backing");
    sampled_snapshot.Assign(bytes.first(size_t(layout.guest_bytes)));
    if(desc.stencil_address)stencil_snapshot.Assign(stencil.first(size_t(stencil_layout.guest_bytes)));
}
void GuestTextureCache::SetBackingSamples(Entry& entry,UINT samples,ID3D12GraphicsCommandList* list,bool copy){
    if(!list||!entry.texture.handle||!samples||(samples&(samples-1))||samples>16)
        throw std::invalid_argument("Invalid native image backing sample count");
    auto pin=PinBinding(entry);
    const auto current=entry.texture.handle->GetDesc();
    const auto old_samples=entry.RequestedBackingSamples();
    if(!std::has_single_bit(old_samples)||old_samples>16||
       !std::has_single_bit(current.SampleDesc.Count)||current.SampleDesc.Count>old_samples||
       (old_samples==entry.desc.samples&&current.SampleDesc.Count!=entry.desc.samples))
        throw std::invalid_argument("Invalid native requested/physical backing identity");
    if(old_samples==samples)return;
    if(entry.desc.depth_target||entry.desc.levels!=1||entry.desc.volume_depth||entry.desc.one_dimensional)
        throw std::invalid_argument("Sample backing conversion requires a single-mip 2D color image");
    if(current.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||current.MipLevels!=1||
       current.Width!=entry.desc.width||current.Height!=entry.desc.height||current.DepthOrArraySize!=entry.desc.layers)
        throw std::invalid_argument("Native sample backing geometry differs from its owner");
    // Stage map nodes and resources before recording. Failed allocation, format
    // admission or a resumed-context failure cannot publish a new identity.
    auto backings=entry.sample_backings;
    auto& destination=backings[samples];backings.try_emplace(old_samples);
    uint64_t added_bytes{};
    if(!destination.handle){
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{entry.layout.format};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
        if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET))
            throw std::runtime_error("Native sample backing format lacks render-target support");
        const auto physical=GuestMultisample::SelectBackingSamples(device_.Get(),entry.layout.format,samples);
        // Alternate backings may use the pinned native fallback. The original
        // guest sample payload still needs an exact resource for upload/SRV/
        // storage/readback; never admit count loss at that boundary.
        if(physical!=samples&&(samples==entry.desc.samples||physical<std::min(samples,entry.desc.samples)))
            throw std::runtime_error("Native fallback cannot represent the original guest sample payload");
        auto description=current;description.SampleDesc={physical,0};description.Alignment=0;
        description.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if(physical>1)description.Flags&=~D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        const auto formats=ResourceCasts(entry.texture);
        const auto allocation=texture_allocator_.AllocationInfo(description,formats);
        if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX||
           entry.allocation_bytes>UINT64_MAX-allocation.SizeInBytes)
            throw std::runtime_error("Invalid native sample backing allocation size");
        ReserveMemory(allocation.SizeInBytes,collected_fence_,&entry);
        destination=AllocateTexture(&entry,description,D3D12_RESOURCE_STATE_COMMON,nullptr,formats);
        added_bytes=allocation.SizeInBytes;
    }
    const auto target=destination.handle->GetDesc();
    if(target.Dimension!=current.Dimension||target.Width!=current.Width||target.Height!=current.Height||
       target.MipLevels!=1||target.DepthOrArraySize!=current.DepthOrArraySize||target.Format!=current.Format||
       !std::has_single_bit(target.SampleDesc.Count)||target.SampleDesc.Count>samples||
       (samples==entry.desc.samples&&target.SampleDesc.Count!=entry.desc.samples))
        throw std::invalid_argument("Cached native sample backing differs from its requested identity");
    std::vector<std::shared_ptr<GuestMultisample::CopyLease>> prepared;
    if(copy){
        if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
        entry.multisample=multisample_;
        prepared.reserve(entry.desc.layers);
        for(UINT layer=0;layer<entry.desc.layers;++layer)
            prepared.push_back(multisample_->PrepareCopy(entry.texture,destination,entry.desc,entry.layout,layer));
        entry.sample_copy_leases.reserve(entry.sample_copy_leases.size()+prepared.size());
        // Reservation may have waited/submitted the caller's old list. All
        // descriptors and resources survive; record only on its current list.
        list=recording_list_?recording_list_():list;
        if(!list)throw std::runtime_error("Native sample backing copy has no current recording list");
        entry.sample_copy_leases.insert(entry.sample_copy_leases.end(),prepared.begin(),prepared.end());
        entry.pending=true;
        for(const auto& lease:prepared)lease->Record(list,entry.texture,destination);
    }
    entry.storage_clear_descriptors.reset();entry.render_clear_descriptors.reset();
    backings[old_samples]=std::move(entry.texture);entry.texture=std::move(destination);
    entry.sample_backings.swap(backings);entry.requested_backing_samples=samples;
    entry.allocation_bytes+=added_bytes;entry.last_use=++use_sequence_;
}
void GuestTextureCache::EnsureMultisampleStorage(Entry& entry){
    EnsureMultisampleStorage(entry,entry.desc);
}
void GuestTextureCache::EnsureMultisampleStorage(Entry& entry,const GuestTextureDesc& writable_view){
    if(!CanUseMultisampleStorageView(entry,writable_view))
        throw std::invalid_argument("Native MSAA shader storage has no compatible writer view");
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("MSAA storage has no owning cache");
    if(entry.storage_samples.size()==entry.desc.samples&&entry.storage_linear.handle){
        entry.storage_retired=false;return;
    }
    const bool initial_readonly=IsReadonlyMultisampleWordClass(entry.desc)&&
        !entry.storage_retired&&entry.storage_samples.empty()&&!entry.storage_linear.handle;
    // Float word attachments normally use native blending. Allocate sample
    // UAVs only when ordered blending is actually requested, not for every RT.
    const bool initial_ordered=IsOrderedMultisampleColorClass(entry.desc)&&
        (IsOrderedScalarClass(entry.desc)||IsOrderedIntegerClass(entry.desc)||IsOrderedFloatWordClass(entry.desc)||IsOrderedPacked16Class(entry.desc)||IsOrderedSrgbClass(entry.desc)||entry.desc.data_format==3||entry.desc.data_format==5||entry.desc.data_format==6||entry.desc.data_format==12)&&
        !entry.storage_retired&&entry.storage_samples.empty()&&!entry.storage_linear.handle;
    if((!entry.storage_retired&&!initial_readonly&&!initial_ordered)||!entry.storage_samples.empty()||entry.storage_linear.handle||
       !CanTransferIndependentMultisample(entry.desc,entry.layout))
        throw std::invalid_argument("Native MSAA shader storage is not reconstructible");
    auto description=entry.texture.handle->GetDesc();
    if(description.SampleDesc.Count!=entry.desc.samples)
        throw std::invalid_argument("Retired MSAA storage needs matching native sample backing");
    description.SampleDesc={1,0};description.Alignment=0;
    description.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const auto ordered_formats=OrderedColorFormats(entry.desc,texture_allocator_.SupportsFormatCasting());
    const auto formats=ordered_formats.empty()?ResourceCasts(entry.texture):ordered_formats;
    const auto texture_bytes=texture_allocator_.AllocationInfo(description,formats).SizeInBytes;
    const auto linear_bytes=GuestMultisample::LinearBytes(entry.desc,GuestMultisample::Footprint(entry.desc,entry.layout));
    D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width=(linear_bytes+3)&~uint64_t(3);buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
    buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    const auto buffer_bytes=device_->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
    if(!texture_bytes||texture_bytes==UINT64_MAX||!buffer_bytes||buffer_bytes==UINT64_MAX||
       texture_bytes>(UINT64_MAX-buffer_bytes)/entry.desc.samples)
        throw std::overflow_error("Invalid restored MSAA storage allocation size");
    const auto bytes=texture_bytes*entry.desc.samples+buffer_bytes;
    if(entry.allocation_bytes>UINT64_MAX-bytes)throw std::overflow_error("Restored MSAA storage accounting overflow");
    ReserveMemory(bytes,collected_fence_,&entry);
    // Allocate the complete replacement before publishing any of it. The
    // existing StorageLease read pass initializes every sample before use.
    std::vector<Resource> samples;samples.reserve(entry.desc.samples);
    for(UINT sample=0;sample<entry.desc.samples;++sample)
        samples.push_back(AllocateTexture(&entry,description,D3D12_RESOURCE_STATE_COMMON,nullptr,formats));
    auto linear=GuestTiler::Scratch(device_.Get(),linear_bytes);
    entry.storage_samples=std::move(samples);entry.storage_linear=std::move(linear);
    entry.allocation_bytes+=bytes;entry.storage_retired=false;
    Diagnostics::Count("image-msaa.storage-restored.bytes",bytes);
}
bool GuestTextureCache::RetireMultisampleStorage(Entry& entry,uint64_t completed){
    if(!entry.gpu_owned||!entry.buffer_alias_version||(!entry.desc.writable&&!IsReadonlyMultisampleWordClass(entry.desc))||
       !CanTransferIndependentMultisample(entry.desc,entry.layout)||entry.BindingPinned()||
       entry.pending||entry.fence>completed||entry.storage_samples.size()!=entry.desc.samples||
       !entry.storage_linear.handle||!entry.texture.handle||
       entry.texture.handle->GetDesc().SampleDesc.Count!=entry.desc.samples||
       !entry.storage_leases.empty()||!entry.storage_clear_leases.empty()||!entry.format_transfers.empty())return false;
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("MSAA pressure storage has no owning cache");
    const auto buffer=entry.storage_linear.handle->GetDesc();
    auto bytes=device_->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
    if(!bytes||bytes==UINT64_MAX)throw std::overflow_error("Invalid retired MSAA linear allocation");
    for(const auto& sample:entry.storage_samples){
        // Descriptor bindings retain the placed allocation independently of
        // the cache. Do not claim their bytes freed while such a lease exists.
        if(!sample.handle||!sample.allocation||sample.allocation.use_count()!=1)return false;
        const auto sample_bytes=texture_allocator_.AllocationInfo(sample.handle->GetDesc(),ResourceCasts(sample)).SizeInBytes;
        if(!sample_bytes||sample_bytes==UINT64_MAX||bytes>UINT64_MAX-sample_bytes)
            throw std::overflow_error("Invalid retired MSAA sample allocation");
        bytes+=sample_bytes;
    }
    if(bytes>entry.allocation_bytes)throw std::logic_error("MSAA storage exceeds its accounted allocation");
    entry.storage_samples.clear();entry.storage_linear={};entry.storage_retired=true;
    entry.allocation_bytes-=bytes;
    // The authoritative multisample texture, image UID and CPU-write guards
    // remain intact. Never resolve an independent image into physical memory.
    Diagnostics::Count("image-msaa.storage-retired.bytes",bytes);return true;
}
std::shared_ptr<GuestMultisample::StorageLease> GuestTextureCache::PrepareMultisampleStorage(Entry& entry){
    return PrepareMultisampleStorage(entry,entry.desc);
}
std::shared_ptr<GuestMultisample::StorageLease> GuestTextureCache::PrepareMultisampleStorage(Entry& entry,const GuestTextureDesc& writable_view){
    EnsureMultisampleStorage(entry,writable_view);
    if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
    auto lease=multisample_->PrepareStorage(entry.texture,entry.storage_samples,entry.storage_linear,entry.desc,entry.layout);
    entry.storage_leases.push_back(lease);entry.pending=true;return lease;
}
void GuestTextureCache::RegisterMeta(Entry& entry,MetaType type,uint64_t address,uint32_t initial_mask){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("Metadata owner is not a live native image");
    auto& previous=entry.metadata_addresses.at(size_t(type));
    if(previous!=address){
        const auto old=surface_metas_.find(previous);
        if(old!=surface_metas_.end()&&old->second.owner_uid==entry.image_uid)surface_metas_.erase(old);
        previous=address;
    }
    if(!address)return;
    const MetaDataInfo replacement{type,entry.image_uid,entry.desc.samples,initial_mask};
    const auto [it,inserted]=surface_metas_.try_emplace(address,replacement);
    if(!inserted){
        if(it->second.type!=type||it->second.owner_uid!=entry.image_uid)it->second=replacement;
        else it->second.num_samples=entry.desc.samples;
    }
}
std::optional<GuestTextureCache::MetaDataInfo> GuestTextureCache::GetMetaInfo(uint64_t address)const{
    const auto it=surface_metas_.find(address);
    if(it==surface_metas_.end())return std::nullopt;
    const auto owner=image_uids_.find(it->second.owner_uid);
    if(owner==image_uids_.end()||owner->second->metadata_addresses.at(size_t(it->second.type))!=address)
        return std::nullopt;
    return it->second;
}
bool GuestTextureCache::ClearMeta(uint64_t address){
    if(!GetMetaInfo(address))return false;
    surface_metas_.at(address).clear_mask=UINT32_MAX;return true;
}
bool GuestTextureCache::ConsumeMeta(uint64_t address,uint32_t slice){
    if(slice>=32||!GetMetaInfo(address))return false;
    auto& mask=surface_metas_.at(address).clear_mask;
    const uint32_t bit=uint32_t(1)<<slice;
    const bool clear=(mask&bit)!=0;mask&=~bit;return clear;
}
void GuestTextureCache::RemoveMeta(uint64_t address,uint64_t size){
    std::erase_if(surface_metas_,[&](const auto& meta){return meta.first>=address&&meta.first-address<size;});
}
std::shared_ptr<void> GuestTextureCache::PinBinding(Entry& entry){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry)
        throw std::invalid_argument("Native image binding is not owned by this cache");
    if(!entry.binding_pins)entry.binding_pins=std::make_shared<uint32_t>();
    if(*entry.binding_pins==UINT32_MAX)throw std::overflow_error("Native image binding pin overflow");
    ++*entry.binding_pins;
    auto counter=entry.binding_pins;
    // shared_ptr invokes the deleter if control-block allocation fails as well.
    return std::shared_ptr<void>(counter.get(),[counter](void*){--*counter;});
}
GuestTextureCache::PressureSourceLease GuestTextureCache::RetainPressureSource(Entry& entry){
    const auto owner=image_uids_.find(entry.image_uid);
    if(entry.cache!=this||owner==image_uids_.end()||owner->second!=&entry||!entry.texture.handle)
        throw std::invalid_argument("Pressure source is not owned by this image cache");
    const auto description=entry.texture.handle->GetDesc();
    auto bytes=texture_allocator_.AllocationInfo(description,ResourceCasts(entry.texture)).SizeInBytes;
    if(!bytes||bytes==UINT64_MAX)
        throw std::overflow_error("Invalid pressure source allocation size");
    Resource packed;
    if(entry.layout.expanded_bytes_per_pixel){
        if(!HasExactPackedExpansion(entry.desc,entry.layout)||!entry.expanded_guest_backing.handle)
            throw std::invalid_argument("Expanded pressure source has no exact packed backing");
        packed=entry.expanded_guest_backing;const auto buffer=packed.handle->GetDesc();
        const auto packed_bytes=device_->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
        if(!packed_bytes||packed_bytes==UINT64_MAX||bytes>UINT64_MAX-packed_bytes)
            throw std::overflow_error("Invalid packed pressure source allocation size");
        bytes+=packed_bytes;
    }
    if(PressureRetirementBytes()>UINT64_MAX-bytes)
        throw std::overflow_error("Pressure source residency overflow");
    auto source=entry.texture;auto counter=pressure_retirement_bytes_;counter->fetch_add(bytes);
    auto lease=std::shared_ptr<void>(counter.get(),[counter,bytes,source=std::move(source),packed=std::move(packed)](void*)mutable{
        source={};packed={};counter->fetch_sub(bytes);
    });
    return {std::move(lease),bytes};
}
uint64_t GuestTextureCache::ResidentBytes()const{
    uint64_t total=PressureRetirementBytes();
    for(const auto& e:entries_){const auto count=e->ResidentBytes();
        if(total>UINT64_MAX-count)throw std::overflow_error("Texture residency total overflow");total+=count;
    }
    return total;
}
std::shared_ptr<void> GuestTextureCache::PinPressureEntry(Entry* entry){
    if(!entry)return {};
    const auto owner=image_uids_.find(entry->image_uid);
    // A new entry can already own local resources before it is indexed. It is
    // not an eviction candidate; registered callers need a transitive pin.
    if(owner==image_uids_.end())return {};
    if(owner->second!=entry)throw std::logic_error("Pressure preserve identity mismatch");
    return PinBinding(*entry);
}
uint64_t GuestTextureCache::ReleaseRetiredStorage(Entry& entry,uint64_t completed){
    if(entry.BindingPinned()||entry.pending||entry.fence>completed||!entry.tiler_leases.empty()||!entry.multisample_leases.empty()||
       (entry.readback_pool&&entry.readback_pool.use_count()!=1))return 0;
    const auto before=entry.ResidentBytes();
    entry.upload_pool.clear();entry.tiler_input_pool.clear();entry.tiler_output_pool.clear();entry.readback_pool.reset();
    // Runtime::SetBackingSamples(copy_backing=false) can restore an inactive
    // backing before a partial image copy. Its uncovered pixels are observable
    // history, not scratch reconstructible from the active sample domain.
    // Retain these allocations with their original image, as pinned Vulkan
    // does; whole-image eviction still follows the normal ownership contract.
    RetireMultisampleStorage(entry,completed);
    return before-entry.ResidentBytes();
}
bool GuestTextureCache::EvictPressureVictim(uint64_t completed,Entry* preserve){
    auto victim=entries_.end();
    for(auto it=entries_.begin();it!=entries_.end();++it)
        if(it->get()!=preserve&&!(*it)->BindingPinned()&&!(*it)->pending&&(*it)->fence<=completed&&
           // Pinned GC skips tiled SafeToDownload images, but frees Dirty I
           // without a download. An invalid independent I cannot replace B.
           !((*it)->gpu_owned&&(*it)->buffer_alias_version&&
             (*it)->HasValidLogicalImage()&&
             ((*it)->desc.tile_mode!=uint32_t(AmdGpu::TileMode::DisplayLinearAligned)||(*it)->desc.samples!=1))&&
           (!(*it)->readback_pool||(*it)->readback_pool.use_count()==1)&&
           (victim==entries_.end()||(*it)->last_use<(*victim)->last_use))victim=it;
    if(victim==entries_.end())return false;
    auto* chosen=victim->get();const auto uid=chosen->image_uid;
    auto victim_pin=PinBinding(*chosen);
    if(chosen->gpu_owned){
        if(chosen->buffer_alias_version&&!chosen->HasValidLogicalImage()){
            // The physical buffer domain has already invalidated this I. Drop
            // only its ownership/witnesses, retaining sibling B/I guards and
            // pending real CPU-store markers. Never enqueue stale pixels.
            if(!retire_independent_)throw std::runtime_error("Invalid independent image has no retirement callback");
            retire_independent_(*chosen);
            if(chosen->gpu_owned||chosen->buffer_alias_version)
                throw std::logic_error("Invalid independent image retained ownership during eviction");
            Diagnostics::Count("image-pressure.discard-invalid-independent");
        }else{
            const auto& retire=retire_pressure_?retire_pressure_:resolve_owned_;
            if(!retire)throw std::runtime_error("GPU-owned image has no retirement resolver");retire(*chosen);
        }
    }
    if(chosen->cpu_watched&&release_immutable_)release_immutable_(*chosen);
    // A resolver can restore another sample backing, allocate recursively and
    // erase other victims. Cache objects stay pinned; vector iterators do not.
    const auto owner=image_uids_.find(uid);
    if(owner==image_uids_.end()||owner->second!=chosen)
        throw std::logic_error("Pressure victim identity changed during retirement");
    victim=std::find_if(entries_.begin(),entries_.end(),[chosen](const auto& image){return image.get()==chosen;});
    if(victim==entries_.end())throw std::logic_error("Pressure victim disappeared during retirement");
    victim_pin.reset();
    if(chosen->BindingPinned())throw std::runtime_error("Pressure victim acquired another binding during retirement");
    entries_.erase(victim);RebuildIndex();return true;
}
Resource GuestTextureCache::AllocateColorFeedbackCopy(Entry& entry,uint64_t completed,Resource* sampled_backing){
    if(entry.cache!=this||!entry.texture.handle||entry.desc.depth_target)
        throw std::invalid_argument("Invalid native color feedback source");
    auto pin=PinBinding(entry);
    auto& source=sampled_backing?*sampled_backing:entry.texture;
    const bool sample_plane=sampled_backing&&std::ranges::any_of(entry.storage_samples,
        [&](const auto& plane){return &plane==sampled_backing;});
    if((sampled_backing!=nullptr&&sampled_backing!=&entry.texture&&!sample_plane)||!source.handle)
        throw std::invalid_argument("Feedback backing does not belong to its image");
    const auto desc=source.handle->GetDesc();
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       (desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)||
       !(desc.Flags&(sample_plane?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)))
        throw std::invalid_argument("Feedback source is not a color attachment");
    const auto formats=source.castable_formats?std::span<const DXGI_FORMAT>(*source.castable_formats):std::span<const DXGI_FORMAT>{};
    const auto bytes=texture_allocator_.AllocationInfo(desc,formats).SizeInBytes;
    if(!bytes||bytes==UINT64_MAX)throw std::runtime_error("Invalid color feedback allocation");
    ReserveMemory(bytes,completed,&entry);
    auto copy=AllocateTexture(&entry,desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,formats);
    struct Account {
        std::shared_ptr<void> allocation;
        std::shared_ptr<std::atomic<uint64_t>> counter;uint64_t bytes{};
        ~Account(){if(counter)counter->fetch_sub(bytes);}
    };
    auto owner=std::make_shared<Account>();owner->allocation=copy.allocation;
    if(bytes>UINT64_MAX-pressure_retirement_bytes_->load())throw std::overflow_error("Feedback accounting overflow");
    owner->counter=pressure_retirement_bytes_;owner->bytes=bytes;
    pressure_retirement_bytes_->fetch_add(bytes);copy.allocation=std::move(owner);
    return copy;
}
Resource GuestTextureCache::AllocateTexture(Entry* preserve,const D3D12_RESOURCE_DESC& description,
    D3D12_RESOURCE_STATES initial,const D3D12_CLEAR_VALUE* clear,std::span<const DXGI_FORMAT> formats){
    auto preserve_pin=PinPressureEntry(preserve);
    for(;;){
        try{return texture_allocator_.Allocate(description,initial,clear,formats);}
        catch(const TextureBudgetExhausted&){
            // Resource-byte admission alone cannot predict placed-heap class,
            // alignment or fragmentation. Retry actual heap admission only after
            // one real eviction or completed in-flight source reclamation.
            const auto preceding_budget=resident_budget_;
            const auto preceding_commitment=texture_allocator_.CommittedBytes();
            RefreshMemoryBudget();
            if(resident_budget_>preceding_budget||
               texture_allocator_.CommittedBytes()<preceding_commitment)continue;
            bool released=false;
            for(auto& image:entries_)if(image.get()!=preserve)
                released|=ReleaseRetiredStorage(*image,collected_fence_)!=0;
            if(released)continue;
            if(EvictPressureVictim(collected_fence_,preserve))continue;
            const auto retired=PressureRetirementBytes();
            if(retired&&wait_pressure_){
                const auto completed=wait_pressure_();Collect(completed);
                if(PressureRetirementBytes()<retired)continue;
            }
            throw;
        }
    }
}
bool GuestTextureCache::ReclaimResidentMemory(uint64_t target,uint64_t completed,Entry* preserve){
    auto preserve_pin=PinPressureEntry(preserve);
    auto resident=ResidentBytes();
    if(resident>target)for(auto& entry:entries_){
        if(entry.get()==preserve)continue;
        resident-=ReleaseRetiredStorage(*entry,completed);
        if(resident<=target)break;
    }
    while(resident>target){
        if(!EvictPressureVictim(completed,preserve))return false;
        resident=ResidentBytes();
    }
    return true;
}
void GuestTextureCache::ReserveMemory(uint64_t bytes,uint64_t completed,Entry* preserve){
    auto preserve_pin=PinPressureEntry(preserve);
    RefreshMemoryBudget();
    for(;;){
        const auto total=ResidentBytes();
        if(total>UINT64_MAX-bytes)throw std::overflow_error("Texture reservation total overflow");
        const auto target=bytes<=resident_budget_?resident_budget_-bytes:0;
        if(total<=target&&bytes<=resident_budget_)return;
        const auto preceding_budget=resident_budget_;
        ReclaimResidentMemory(target,completed,preserve);
        // A resolver can allocate recursively and refresh the DXGI budget.
        // Recheck the current limit rather than admitting against an old target.
        if(bytes<=resident_budget_&&ResidentBytes()<=resident_budget_-bytes)return;
        if(resident_budget_!=preceding_budget)continue;
        const auto retired=PressureRetirementBytes();
        if(retired&&wait_pressure_){
            completed=std::max(completed,wait_pressure_());Collect(completed);
            if(bytes<=resident_budget_&&ResidentBytes()<=resident_budget_-bytes)return;
            if(PressureRetirementBytes()<retired)continue;
        }
        // Converted clears and other transfer leases remain attached to
        // pending entries until their ordinary submission retires. Eviction
        // cannot reclaim them before that fence, even if the GPU is idle.
        if(retire_pending_&&std::ranges::any_of(entries_,[&](const auto& image){
            return image->pending||image->fence>completed;})){
            const auto preceding_resident=ResidentBytes(),preceding_completed=completed;
            retire_pending_();completed=std::max(completed,collected_fence_);Collect(completed);
            if(ResidentBytes()<preceding_resident||completed>preceding_completed)continue;
        }
        throw std::runtime_error("Native texture batch exceeds adapter-derived cache budget including transfer pools and in-flight sources");
    }
}
bool GuestTextureCache::CanUploadGpuSnapshot(const GuestTextureDesc& d,const GuestTextureLayout& layout,bool writable_snapshot){
    const bool depth=d.depth_target&&!d.writable&&!d.render_target&&!d.volume_depth&&!d.one_dimensional&&
        ((d.data_format==2&&d.number_format==0&&layout.format==DXGI_FORMAT_R16_UNORM)||
         (d.data_format==4&&d.number_format==7&&layout.format==DXGI_FORMAT_R32_FLOAT));
    return (!d.writable||writable_snapshot)&&(!d.stencil_address||(depth&&d.stencil_address%4==0))&&
        (d.samples==1||CanTransferIndependentMultisample(d,layout)||
         (depth&&d.levels==1&&std::has_single_bit(d.samples)&&d.samples<=16))&&d.address%4==0&&
        GuestTiler::CanAddressGuestBytes(layout.guest_bytes)&&
        (!layout.expanded_bytes_per_pixel||HasExactPackedExpansion(d,layout));
}
bool MatchesGuestMipLayout(const GuestTextureLayout& source,const GuestTextureLayout& target){
    if(source.guest_bytes!=target.guest_bytes||source.bytes_per_pixel!=target.bytes_per_pixel||
       source.block_extent!=target.block_extent||source.logical_bytes!=target.logical_bytes||
       source.mips.size()!=target.mips.size())return false;
    for(size_t mip=0;mip<target.mips.size();++mip){const auto& a=source.mips[mip];const auto& b=target.mips[mip];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||
           a.macro!=b.macro||a.thickness!=b.thickness)return false;
    }
    return true;
}
namespace {
bool HasPossibleReferenceSubresourceBase(const GuestTextureDesc& child,
    const GuestTextureLayout& child_layout,const GuestTextureDesc& parent,const GuestTextureLayout& physical){
    // ImageInfo::MipOf first applies IsCompatible, including the promoted
    // depth format class. A raw color word at a depth slice boundary is still
    // a chance overlap; equal texel bits alone cannot make it a depth view.
    const bool child_depth=child.depth_target||child.sampled_depth;
    const bool parent_depth=parent.depth_target||parent.sampled_depth;
    if((child_depth||parent_depth)&&(child_depth!=parent_depth||
       child.data_format!=parent.data_format||child.number_format!=parent.number_format||
       bool(child.stencil_address)!=bool(parent.stencil_address)))return false;
    if(child.tile_mode<=26&&parent.tile_mode<=26&&
       AmdGpu::GetArrayMode(AmdGpu::TileMode(child.tile_mode))!=
           AmdGpu::GetArrayMode(AmdGpu::TileMode(parent.tile_mode)))return false;
    // A necessary condition from pinned ImageInfo::MipOf: a one-level
    // subresource starts exactly at a physical mip/slice boundary. Keep
    // unclassified volume/1D domains on the existing explicit transfer path.
    if(child.volume_depth||parent.volume_depth||child.one_dimensional||parent.one_dimensional||
       physical.block_extent!=1||child_layout.block_extent!=1||physical.mips.size()!=parent.levels)return true;
    if(child.samples!=parent.samples||child_layout.bytes_per_pixel!=physical.bytes_per_pixel)return false;
    if(child.levels!=1||child.address<parent.address)return false;
    const auto delta=child.address-parent.address;
    for(size_t mip=0;mip<physical.mips.size();++mip){
        const auto& part=physical.mips[mip];
        if(!part.slice_bytes)return true;
        const auto end=mip+1<physical.mips.size()?physical.mips[mip+1].offset:physical.guest_bytes;
        if(delta>=part.offset&&delta<end&&(delta-part.offset)%part.slice_bytes==0&&
           child.width==std::max(parent.width>>mip,1u)&&child.height==std::max(parent.height>>mip,1u)&&
           child.pitch==(mip?part.pitch:parent.pitch)&&child_layout.guest_bytes&&
           child_layout.guest_bytes%part.slice_bytes==0&&
           (delta-part.offset)%child_layout.guest_bytes==0)return true;
    }
    return false;
}
bool IsDirectDepthWordClass(const GuestTextureDesc& d,const GuestTextureLayout& l){
    // CopyColorAndDepth preserves texel bits. These native color layouts have
    // guest 16/32-bit words after explicitly reversing any SOURCE native
    // packed-channel order, without expansion or a numeric SRV conversion.
    if(l.block_extent!=1||l.expanded_bytes_per_pixel||
       (l.bytes_per_pixel!=2&&l.bytes_per_pixel!=4))return false;
    switch(d.data_format){
    case 2:case 3:case 4:case 5:case 10:return true;
    case 6:return d.number_format==7;
    case 16:case 17:case 18:case 19:return d.number_format==0;
    case 9:return d.number_format==0||d.number_format==4;
    default:return false;
    }
}
bool MatchesReferenceColorImage(GuestTextureDesc source,const GuestTextureLayout& source_layout,
    GuestTextureDesc view,const GuestTextureLayout& view_layout){
    // Pinned texture_cache/host_compatibility.cpp groups ordinary uncompressed
    // color formats by texel bits (8/16/32/64/96/128 here). Equal-sized BC blocks
    // and depth/stencil are separate classes and must not enter this predicate.
    if(source.render_target||view.render_target||source.depth_target||view.depth_target||
       source.stencil_address||view.stencil_address||source_layout.block_extent!=1||view_layout.block_extent!=1||
       !MatchesGuestMipLayout(source_layout,view_layout))return false;
    switch(source_layout.bytes_per_pixel){case 1:case 2:case 4:case 8:case 12:case 16:break;default:return false;}
    source.data_format=view.data_format=4;source.number_format=view.number_format=4;
    source.writable=view.writable=false;
    source.native_packed10_raw=view.native_packed10_raw=source.native_raw32=view.native_raw32=false;
    source.storage_height=view.storage_height=0;source.storage_slice_bytes=view.storage_slice_bytes=0;
    source.pow2_padding=view.pow2_padding=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
bool MatchesReferenceDepthImage(GuestTextureDesc source,const GuestTextureLayout& source_layout,
    GuestTextureDesc view,const GuestTextureLayout& view_layout){
    if(!source.depth_target&&!view.depth_target)return false;
    if((source.depth_target&&!IsGuestDepthPlaneStorage(source,source_layout))||
       (view.depth_target&&!IsGuestDepthPlaneStorage(view,view_layout)))return false;
    // CopyColorAndDepth moves logical depth/color words, not a depth UAV. Keep
    // exact physical/sample geometry here; other overlap domains need their
    // explicit mapping plan rather than a byte-prefix reinterpretation.
    source.depth_target=view.depth_target=false;source.render_target=view.render_target=false;
    source.sampled_depth=view.sampled_depth=false;
    source.stencil_address=view.stencil_address=0;
    return MatchesReferenceColorImage(source,source_layout,view,view_layout);
}
bool MatchesReferenceOverlapClass(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& target,const GuestTextureLayout& target_layout){
    // Pinned ResolveOverlap checks ImageInfo::BlockDim() and num_bits*samples
    // BEFORE ResolveDepthOverlap. A recycled address with another physical
    // block geometry is a separate I initialized from B, not a blit source.
    return source_layout.block_extent&&target_layout.block_extent&&
        source.pitch/source_layout.block_extent==target.pitch/target_layout.block_extent&&
        source.height/source_layout.block_extent==target.height/target_layout.block_extent&&
        uint64_t(source_layout.bytes_per_pixel)*source.samples==
            uint64_t(target_layout.bytes_per_pixel)*target.samples;
}
std::optional<std::pair<UINT,UINT>> ReadonlyDepthSubresource(
    const GuestTextureDesc& source,const GuestTextureLayout& physical,
    const GuestTextureDesc& view,const GuestTextureLayout& local){
    // Pinned ResolveOverlap's right-overlap path retains the parent and
    // returns MipOf/SliceOf. Bind that native subresource without converting,
    // reading a standalone child envelope or replacing the parent owner.
    if(!IsGuestDepthPlaneStorage(source,physical)||source.stencil_address||
       !view.sampled_depth||view.data_format!=source.data_format||view.number_format!=source.number_format||
       view.depth_target||view.render_target||view.writable||
       view.stencil_address||source.volume_depth||view.volume_depth||source.one_dimensional||view.one_dimensional||
       view.address<=source.address||view.levels!=1||view.first_mip||!view.layers||
       view.first_layer>=view.layers||!view.view_layers||view.view_layers>view.layers-view.first_layer||
       view.samples!=source.samples||physical.mips.size()!=source.levels||local.mips.size()!=1||
       !IsDirectDepthWordClass(view,local)||physical.bytes_per_pixel!=local.bytes_per_pixel||
       !MatchesGuestMipLayout(LayoutGuestTexture(view),local)||
       source.tile_mode>26||view.tile_mode>26||
       AmdGpu::GetArrayMode(AmdGpu::TileMode(source.tile_mode))!=
           AmdGpu::GetArrayMode(AmdGpu::TileMode(view.tile_mode)))return std::nullopt;
    const auto delta=view.address-source.address;
    for(UINT mip=0;mip<source.levels;++mip){const auto& part=physical.mips[mip];
        if(!part.slice_bytes||delta<part.offset||
           view.width!=std::max(source.width>>mip,1u)||view.height!=std::max(source.height>>mip,1u)||
           view.pitch!=(mip?part.pitch:source.pitch)||
           local.guest_bytes%part.slice_bytes)continue;
        const auto relative=delta-part.offset;
        // SliceOf also aligns the address to the complete child extent.
        if(!local.guest_bytes||relative%local.guest_bytes)continue;
        const auto layer=relative/part.slice_bytes;
        if(layer>=source.layers||view.layers>source.layers-layer)continue;
        return std::pair(mip,UINT(layer));
    }
    return std::nullopt;
}
bool CanMapIndependentDepthImage(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& target,const GuestTextureLayout& target_layout){
    if(source.address==target.address&&MatchesReferenceOverlapClass(source,source_layout,target,target_layout)&&
       GuestMultisample::CanReinterpretColorAsMsDepth(source,source_layout,target,target_layout))return true;
    if(MatchesReferenceDepthImage(source,source_layout,target,target_layout))return true;
    // ResolveOverlap admits equal-address depth recreation by BlockDim
    // (pitch/height) and texel bits, not by guest tiling or logical width.
    // CopyImage/CopyImageWithBuffer copy the SOURCE extent at (0,0); do not
    // invent clipped copies for a smaller destination. Each native source
    // plane is retiled into the destination's own physical addressing below.
    if(source.address==target.address&&source.samples==1&&target.samples==1&&
       (source.depth_target||target.depth_target)&&
       !source.volume_depth&&!target.volume_depth&&!source.one_dimensional&&!target.one_dimensional&&
       source.width&&source.width<=target.width&&source.levels&&target.levels&&source.layers&&target.layers&&
       MatchesReferenceOverlapClass(source,source_layout,target,target_layout)&&
       (!source.depth_target||IsGuestDepthPlaneStorage(source,source_layout))&&
       (!target.depth_target||IsGuestDepthPlaneStorage(target,target_layout))&&
       IsDirectDepthWordClass(source,source_layout)&&IsDirectDepthWordClass(target,target_layout)&&
       MatchesGuestMipLayout(LayoutGuestTexture(source),source_layout)&&
       MatchesGuestMipLayout(LayoutGuestTexture(target),target_layout))return true;
    // Original CopyImage/CopyImageWithBuffer copy the mip/layer intersection
    // after lexicographic resource recreation. More mips can mean fewer layers.
    // Byte prefixes are NOT physical mip prefixes when layer counts differ:
    // map source-native footprints to the target's own guest offsets/strides.
    if((!source.depth_target&&!target.depth_target)||source.samples!=target.samples||
       !source.levels||!source.layers||!target.levels||!target.layers||
       !IsDirectDepthWordClass(source,source_layout)||
       !IsDirectDepthWordClass(target,target_layout)||
       !MatchesGuestMipLayout(LayoutGuestTexture(target),target_layout))return false;
    auto prefix=target;prefix.levels=source.levels;prefix.layers=source.layers;
    prefix.first_mip=prefix.first_layer=prefix.min_lod=0;prefix.view_layers=source.layers;
    const auto prefix_layout=LayoutGuestTexture(prefix);
    return MatchesReferenceDepthImage(source,source_layout,prefix,prefix_layout);
}
bool CanMapIndependentColorGrowth(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& target,const GuestTextureLayout& target_layout){
    if(source.address!=target.address||source.samples!=1||target.samples!=1||
       source.levels>target.levels||source.layers>target.layers||
       (source.levels==target.levels&&source.layers==target.layers)||
       !MatchesGuestMipLayout(LayoutGuestTexture(target),target_layout))return false;
    auto prefix=target;prefix.levels=source.levels;prefix.layers=source.layers;
    prefix.first_mip=prefix.first_layer=prefix.min_lod=0;prefix.view_layers=source.layers;
    // ColorSubview proves the exact common slice geometry, native word class
    // and source extent. Layer growth moves later mip offsets, so the copy
    // must map logical texels into the destination layout, never copy bytes.
    return CompatibleGuestColorSubview(source,source_layout,prefix,LayoutGuestTexture(prefix));
}
bool MatchesReferenceBlockStorage(GuestTextureDesc source,const GuestTextureLayout& source_layout,
    GuestTextureDesc view,const GuestTextureLayout& view_layout){
    const bool source_block=IsIndependentGuestBlockStorage(source,source_layout);
    const bool view_block=IsIndependentGuestBlockStorage(view,view_layout);
    if(!source_block&&!view_block)return false;
    const auto word=[](const GuestTextureDesc& d,const GuestTextureLayout& l){
        return !d.render_target&&!d.depth_target&&!d.stencil_address&&!d.volume_depth&&
            !d.one_dimensional&&d.samples==1&&d.levels&&d.levels<=15&&
            l.mips.size()==d.levels&&!d.pow2_padding&&
            !d.storage_height&&!d.storage_slice_bytes&&!l.expanded_bytes_per_pixel&&
            l.block_extent==1&&d.number_format==4&&
            ((d.data_format==11&&l.bytes_per_pixel==8)||(d.data_format==14&&l.bytes_per_pixel==16));
    };
    if((!source_block&&!word(source,source_layout))||(!view_block&&!word(view,view_layout))||
       (source_block&&view_block&&source.data_format!=view.data_format)||
       source_layout.bytes_per_pixel!=view_layout.bytes_per_pixel||
       source_layout.guest_bytes!=view_layout.guest_bytes||
       source_layout.logical_bytes!=view_layout.logical_bytes||source.levels!=view.levels||
       source_layout.mips.size()!=source.levels||view_layout.mips.size()!=view.levels)return false;
    for(uint32_t mip=0;mip<source.levels;++mip){
        const auto& a=source_layout.mips[mip];const auto& b=view_layout.mips[mip];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||
           a.macro!=b.macro||a.thickness!=b.thickness)return false;
        const auto width=[](uint32_t extent,uint32_t level,bool block){
            const auto texels=std::max(extent>>level,1u);return block?(texels+3)/4:texels;
        };
        if(width(source.width,mip,source_block)!=width(view.width,mip,view_block)||
           width(source.height,mip,source_block)!=width(view.height,mip,view_block))return false;
    }
    // Pinned ResolveOverlap/ExpandImage copies uncompressed words to BC
    // blocks. Its block-texel view class also permits the reverse UINT view.
    // Same-codec BC numeric views keep those exact bytes; different codecs do
    // not become compatible just because their blocks have the same size.
    if(source_block){source.width/=4;source.height/=4;source.pitch/=4;}
    if(view_block){view.width/=4;view.height/=4;view.pitch/=4;}
    source.data_format=view.data_format=4;source.number_format=view.number_format=4;
    source.writable=view.writable=false;
    source.native_packed10_raw=view.native_packed10_raw=source.native_raw32=view.native_raw32=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
bool MatchesReferenceBlockImage(GuestTextureDesc source,const GuestTextureLayout& source_layout,
    GuestTextureDesc view,const GuestTextureLayout& view_layout){
    // Complete ownership replacement must not discard unrelated BC parent mips.
    if(source_layout.block_extent==4&&view_layout.block_extent!=4&&source.levels!=1)return false;
    return MatchesReferenceBlockStorage(source,source_layout,view,view_layout);
}
// A block-texel view selects one mip, not necessarily a one-mip allocation.
// Materialize a private readonly prefix through the selected mip; the full BC
// chain remains the sole authoritative image, including a one-level parent.
// This is not an ownership match: even a complete readonly word view must not
// retire a BC parent already bound by another slot in the same dispatch.
bool MatchesReadonlyBlockMipPrefix(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& view,const GuestTextureLayout& view_layout){
    if(view.writable||!view.levels||view.levels>source.levels||
       view.first_mip!=view.levels-1||
       !IsNativeReadonlyGuestBlockStorage(source,source_layout)||view_layout.block_extent!=1)return false;
    auto prefix=source;prefix.levels=view.levels;prefix.first_mip=0;
    const auto prefix_layout=LayoutGuestTexture(prefix);
    if(prefix_layout.guest_bytes>source_layout.guest_bytes)return false;
    // Guest arrays are mip-major. Only an identical complete physical prefix
    // can be clipped from the parent's private retile without remapping layers.
    for(uint32_t mip=0;mip<prefix.levels;++mip){
        const auto& actual=source_layout.mips[mip];const auto& part=prefix_layout.mips[mip];
        if(actual.pitch!=part.pitch||actual.height!=part.height||actual.offset!=part.offset||
           actual.slice_bytes!=part.slice_bytes||actual.macro!=part.macro||actual.thickness!=part.thickness)
            return false;
    }
    return MatchesReferenceBlockStorage(prefix,prefix_layout,view,view_layout);
}
bool MatchesReadonlyBcMipPrefix(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& view,const GuestTextureLayout& view_layout){
    // A native BC SRV can select a physical prefix without a UINT representation
    // of terminal mips. Numeric variants must keep the same BC codec family;
    // actual resource casting and ownership are checked separately below.
    if(!IsNativeReadonlyGuestBlockStorage(source,source_layout)||
       !IsNativeReadonlyGuestBlockStorage(view,view_layout)||view.levels>source.levels||
       view.first_mip>=view.levels||source.data_format!=view.data_format||
       BlockStorageFormat(source_layout.format)==DXGI_FORMAT_UNKNOWN||
       BlockStorageFormat(source_layout.format)!=BlockStorageFormat(view_layout.format))return false;
    auto prefix=source;prefix.levels=view.levels;prefix.first_mip=0;
    auto physical_view=view;physical_view.number_format=source.number_format;
    if(!CompatibleGuestSampledStorage(prefix,physical_view))return false;
    const auto physical=LayoutGuestTexture(prefix);
    if(!MatchesGuestMipLayout(physical,view_layout)||physical.guest_bytes>source_layout.guest_bytes)return false;
    for(uint32_t mip=0;mip<prefix.levels;++mip){
        const auto& a=source_layout.mips[mip];const auto& b=physical.mips[mip];
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||
           a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)return false;
    }
    return true;
}
bool CanBindReadonlyBcMipPrefix(const GuestTextureCache::Entry& source,const GuestTextureDesc& view,
    const GuestTextureLayout& layout){
    if(!source.texture.handle||!MatchesReadonlyBcMipPrefix(source.desc,source.layout,view,layout))return false;
    // Logical compatibility alone cannot authorize a typed cast of a legacy
    // resource. Numeric reinterpretation retains the complete GPU owner/watch
    // contract; ordinary CPU-coherent reuse checks complete physical bytes.
    if(source.gpu_owned&&source.desc.number_format!=view.number_format&&
       (!source.cache||!source.buffer_alias_version||!source.buffer_alias_watch||
        !source.cache->CanRetainIndependentImage(source.desc,source.layout)))return false;
    const auto format=DX12_API_CALL(source.texture.handle->GetDesc()).Format;
    return format==layout.format||format==BlockStorageFormat(layout.format);
}
bool IsExpandedReadonlyBcRgba8(const GuestTextureDesc& desc,const GuestTextureLayout& layout){
    // Mesa expands these codecs to the same encoded RGBA bytes for UNORM and
    // sRGB. BC4/5/6H decode signed/unsigned values into floats and cannot share
    // the decoded pixels just because their compressed block sizes agree.
    return !desc.writable&&!desc.render_target&&!desc.depth_target&&!desc.stencil_address&&
        desc.samples==1&&((desc.data_format>=35&&desc.data_format<=37)||desc.data_format==41)&&
        (desc.number_format==0||desc.number_format==9)&&layout.block_extent==4&&
        layout.expanded_bytes_per_pixel==4&&
        (layout.format==DXGI_FORMAT_R8G8B8A8_UNORM||layout.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
}
bool MatchesExpandedReadonlyBc(const GuestTextureDesc& source,const GuestTextureLayout& source_layout,
    const GuestTextureDesc& view,const GuestTextureLayout& view_layout){
    if(!IsExpandedReadonlyBcRgba8(source,source_layout)||!IsExpandedReadonlyBcRgba8(view,view_layout)||
       source.data_format!=view.data_format||!MatchesGuestMipLayout(source_layout,view_layout))return false;
    auto physical_view=view;physical_view.number_format=source.number_format;
    return CompatibleGuestSampledStorage(source,physical_view);
}
bool CanBindReadonlyBcView(const GuestTextureCache::Entry& source,const GuestTextureDesc& view,
    const GuestTextureLayout& layout){
    if(CanBindReadonlyBcMipPrefix(source,view,layout))return true;
    // Expanded BC has no independent GPU-written compressed owner/retile path.
    // Admit only the complete immutable CPU snapshot; upload reuse still checks
    // every compressed byte, including physical padding, before using this view.
    if(!source.texture.handle||source.gpu_owned||
       !MatchesExpandedReadonlyBc(source.desc,source.layout,view,layout))return false;
    const auto format=DX12_API_CALL(source.texture.handle->GetDesc()).Format;
    return format==layout.format||format==DXGI_FORMAT_R8G8B8A8_TYPELESS;
}
// Bounded pinned ExpandImage copy: all source words fit target BC blocks,
// and complete source physical storage is the same mip-major target prefix.
bool MatchesReferencePartialBlockExpansion(GuestTextureDesc source,const GuestTextureLayout& a,
    GuestTextureDesc view,const GuestTextureLayout& b){
    if(!IsNativeReadonlyGuestBlockStorage(view,b)||a.block_extent!=1||a.expanded_bytes_per_pixel||
       source.render_target||source.depth_target||source.stencil_address||source.volume_depth||
       source.one_dimensional||source.samples!=1||source.pow2_padding||source.storage_height||
       source.storage_slice_bytes||source.number_format!=4||
       !((source.data_format==11&&a.bytes_per_pixel==8)||(source.data_format==14&&a.bytes_per_pixel==16))||
       a.bytes_per_pixel!=b.bytes_per_pixel||!source.levels||source.levels>view.levels||
       a.mips.size()!=source.levels||a.guest_bytes>b.guest_bytes||source.layers!=view.layers)return false;
    for(uint32_t mip=0;mip<source.levels;++mip){
        const auto& x=a.mips[mip];const auto& y=b.mips[mip];
        if(x.pitch!=y.pitch||x.height!=y.height||x.offset!=y.offset||x.slice_bytes!=y.slice_bytes||
           x.macro!=y.macro||x.thickness!=y.thickness||
           uint64_t(std::max(source.width>>mip,1u))*4>std::max(view.width>>mip,1u)||
           uint64_t(std::max(source.height>>mip,1u))*4>std::max(view.height>>mip,1u))return false;
    }
    view.width/=4;view.height/=4;view.pitch/=4;view.levels=source.levels;
    source.data_format=view.data_format=4;source.number_format=view.number_format=4;
    source.writable=view.writable=false;
    source.native_raw32=view.native_raw32=source.native_packed10_raw=view.native_packed10_raw=false;
    source.first_mip=view.first_mip=source.first_layer=view.first_layer=source.min_lod=view.min_lod=0;
    source.view_layers=view.view_layers=1;source.swizzle=view.swizzle={4,5,6,7};
    return source==view;
}
bool MatchesGuestUploadStorage(const GuestTextureCache::Entry& entry,const GuestTextureDesc& desc,
    const GuestTextureLayout& layout){
    if(CompatibleGuestColorSubview(entry.desc,entry.layout,desc,layout)&&
       CanBindNativeWordFormat(entry,desc)&&
       (!desc.render_target||(entry.texture.handle&&
        (entry.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))))return true;
    if(CompatibleGuestDepthSubview(entry.desc,entry.layout,desc,layout))return true;
    return (entry.desc==desc||CompatibleGuestAttachmentStorage(entry.desc,desc)||
        CompatibleGuestWritableStorage(entry.desc,desc)||
        ((entry.desc.writable||!desc.writable||
          CanUseMultisampleStorageView(entry,desc)||
          (IsDirectReadonlyWordClass(entry.desc)&&entry.texture.handle&&
           (entry.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)&&
           CanBindNativeWordFormat(entry,desc)))&&CompatibleGuestShaderStorage(entry.desc,desc))||
        CanBindReadonlyWordSrv(entry,desc)||
        (!desc.writable&&!desc.render_target&&!desc.depth_target&&CompatibleGuestSampledStorage(entry.desc,desc))||
        CanBindReadonlyBcMipPrefix(entry,desc,layout))&&
        MatchesGuestMipLayout(entry.layout,layout);
}
}
bool CompatibleGuestBlockSampledViews(const GuestTextureDesc& a,const GuestTextureLayout& al,
    const GuestTextureDesc& b,const GuestTextureLayout& bl){
    return MatchesReadonlyBcMipPrefix(a,al,b,bl)||MatchesReadonlyBcMipPrefix(b,bl,a,al)||
        MatchesExpandedReadonlyBc(a,al,b,bl);
}
bool GuestTextureCache::CanRecreateColorDepthAttachments(const GuestTextureDesc& color,
    const GuestTextureLayout& color_layout,const GuestTextureDesc& depth,const GuestTextureLayout& depth_layout){
    return color.render_target&&!color.writable&&!color.depth_target&&!color.stencil_address&&
        depth.depth_target&&!depth.render_target&&!depth.writable&&
        color.address==depth.address&&CanMapIndependentDepthImage(color,color_layout,depth,depth_layout);
}
bool GuestTextureCache::SeparateColorSampledImages(const GuestTextureDesc& color,
    const GuestTextureLayout& color_layout,const GuestTextureDesc& sampled,const GuestTextureLayout& sampled_layout){
    // ResolveOverlap treats equal addresses with different BlockDim or total
    // bits/sample as pool reuse, before format/depth conversion. Physical byte
    // overlap alone must not publish the old attachment into the new image's B.
    return color.render_target&&!color.writable&&!color.depth_target&&!color.stencil_address&&
        !sampled.writable&&!sampled.render_target&&!sampled.depth_target&&!sampled.stencil_address&&
        color.address==sampled.address&&color_layout.block_extent&&sampled_layout.block_extent&&
        !MatchesReferenceOverlapClass(color,color_layout,sampled,sampled_layout);
}
bool GuestTextureCache::SeparateColorDepthAttachments(const GuestTextureDesc& color,
    const GuestTextureLayout& color_layout,const GuestTextureDesc& depth,const GuestTextureLayout& depth_layout){
    // ResolveOverlap checks pool-reuse geometry before ResolveDepthOverlap,
    // including when both images were selected as attachments in this draw.
    return color.render_target&&!color.writable&&!color.depth_target&&!color.stencil_address&&
        depth.depth_target&&!depth.render_target&&!depth.writable&&!depth.stencil_address&&
        color.address==depth.address&&color_layout.block_extent&&depth_layout.block_extent&&
        !MatchesReferenceOverlapClass(color,color_layout,depth,depth_layout);
}
bool CompatibleGuestBlockWordStorage(const GuestTextureDesc& a,const GuestTextureLayout& al,
    const GuestTextureDesc& b,const GuestTextureLayout& bl){
    if((al.block_extent==4)==(bl.block_extent==4))return false;
    auto readonly_a=a,readonly_b=b;readonly_a.writable=readonly_b.writable=false;
    return MatchesReadonlyBlockMipPrefix(readonly_a,al,readonly_b,bl)||
        MatchesReadonlyBlockMipPrefix(readonly_b,bl,readonly_a,al);
}
GuestTextureCache::Entry* GuestTextureCache::FindValidIndependentImage(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    Entry* result{};bool ambiguous=false;auto [first,last]=address_index_.equal_range(desc.address);
    for(auto indexed=first;indexed!=last;++indexed){auto* entry=indexed->second;
        if(!entry->gpu_owned||!entry->buffer_alias_version||entry->logical_dirty||
           !MatchesGuestUploadStorage(*entry,desc,layout))continue;
        // Separate D3D12 allocations may preserve preceding sampled/attachment
        // snapshots for commands already recorded. They are not separate
        // original images when RememberNativeImageStorage has advanced this
        // physical overlap class. Bind its current allocation even if a stale
        // snapshot happens to match the requested view descriptor exactly.
        if(!entry->cached_native_current){
            const bool superseded=std::any_of(first,last,[&](const auto& indexed_current){
                const auto& current=*indexed_current.second;
                return current.cached_native_current&&current.gpu_owned&&current.buffer_alias_version&&
                    !current.logical_dirty&&current.desc.width==entry->desc.width&&
                    MatchesReferenceOverlapClass(entry->desc,entry->layout,current.desc,current.layout)&&
                    MatchesGuestUploadStorage(current,desc,layout);
            });
            if(superseded)continue;
        }
        if(entry->HasUnprotectedCpuPages())
            throw std::runtime_error("Independent image lookup requires boundary validation");
        if(result){
            const bool exact=entry->desc==desc,previous_exact=result->desc==desc;
            if(previous_exact&&!exact)continue;
            if(exact==previous_exact){
                if(entry->texture.handle.Get()!=result->texture.handle.Get()||
                   entry->expanded_guest_backing.handle.Get()!=result->expanded_guest_backing.handle.Get()||
                   entry->expanded_written_mask.handle.Get()!=result->expanded_written_mask.handle.Get())
                    ambiguous=true;
                continue; // Keep stable order; a later exact match can resolve ambiguity.
            }
        }
        result=entry;ambiguous=false;
    }
    if(ambiguous){
        const auto report=[](const char* kind,const GuestTextureDesc& d,uint64_t bytes,
            const Entry* entry){
            std::fprintf(stderr,"DX12 independent ambiguity %s address=%llx bytes=%llu geometry=%ux%u pitch=%u layers=%u levels=%u format=%u/%u tile=%u rt=%u depth=%u writable=%u uid=%llu current=%u writer=%llx resource=%p\n",
                kind,(unsigned long long)d.address,(unsigned long long)bytes,d.width,d.height,d.pitch,
                d.layers,d.levels,d.data_format,d.number_format,d.tile_mode,unsigned(d.render_target),
                unsigned(d.depth_target),unsigned(d.writable),entry?(unsigned long long)entry->image_uid:0,
                entry?unsigned(entry->cached_native_current):0,entry?(unsigned long long)entry->diagnostic_writer:0,
                entry?static_cast<void*>(entry->texture.handle.Get()):nullptr);
        };
        report("request",desc,layout.guest_bytes,nullptr);
        for(auto indexed=first;indexed!=last;++indexed){const auto* entry=indexed->second;
            report("indexed",entry->desc,entry->layout.guest_bytes,entry);
            std::fprintf(stderr,"DX12 independent identity uid=%llu owned=%u alias=%u dirty=%u exact=%u match=%u samples=%u storage_height=%u slice=%llu first=%u/%u view_layers=%u\n",
                (unsigned long long)entry->image_uid,unsigned(entry->gpu_owned),unsigned(entry->buffer_alias_version),
                unsigned(entry->logical_dirty),unsigned(entry->desc==desc),unsigned(MatchesGuestUploadStorage(*entry,desc,layout)),
                entry->desc.samples,entry->desc.storage_height,(unsigned long long)entry->desc.storage_slice_bytes,
                entry->desc.first_mip,entry->desc.first_layer,entry->desc.view_layers);
        }
        std::fflush(stderr);
        throw std::runtime_error("Ambiguous clean independent image binding");
    }
    return result;
}
void GuestTextureCache::RememberNativeImageStorage(Entry& entry){
    // Every refreshed native allocation has a cached class/identity, including
    // CPU-initialized color. This witness survives dirtying, not replacement.
    auto [first,last]=address_index_.equal_range(entry.desc.address);
    for(auto indexed=first;indexed!=last;++indexed){auto& previous=*indexed->second;
        if(previous.cached_native_current&&previous.desc.width==entry.desc.width&&
           MatchesReferenceOverlapClass(previous.desc,previous.layout,entry.desc,entry.layout))
            previous.cached_native_current=false;
    }
    entry.cached_native_current=true;
}
GuestTextureCache::Entry* GuestTextureCache::FindDepthViewParent(const GuestTextureDesc& requested,
    const GuestTextureLayout& layout)const{
    if(requested.writable||requested.render_target)return nullptr;
    if(auto* live=FindValidIndependentImage(requested,layout)){
        if(CompatibleGuestDepthSubview(live->desc,live->layout,requested,layout))return live;
        // A current independent version at the requested base cannot be
        // replaced by a broader cached parent's older contents.
        return nullptr;
    }
    // Dirtying invalidates contents, not the cached allocation's extent. Keep
    // that extent for full-parent B refresh and existing descriptor lifetimes.
    // A different current render/storage class clears this geometry identity.
    Entry* parent{};auto [first,last]=address_index_.equal_range(requested.address);
    for(auto indexed=first;indexed!=last;++indexed){auto* candidate=indexed->second;
        if(candidate->cached_native_current&&CompatibleGuestDepthSubview(candidate->desc,candidate->layout,requested,layout)&&
           (!parent||std::pair(candidate->desc.levels,candidate->desc.layers)>
                     std::pair(parent->desc.levels,parent->desc.layers)))parent=candidate;
    }
    if(!parent&&requested.sampled_depth)for(const auto& candidate:entries_){
        if(!candidate->cached_native_current||
           !ReadonlyDepthSubresource(candidate->desc,candidate->layout,requested,layout))continue;
        if(parent&&parent->texture.handle.Get()!=candidate->texture.handle.Get())
            throw std::runtime_error("Ambiguous shifted depth view parent");
        parent=candidate.get();
    }
    return parent;
}
bool GuestTextureCache::HasCurrentNativeContents(const Entry& image)const{
    const auto found=image_uids_.find(image.image_uid);
    return image.cache==this&&found!=image_uids_.end()&&found->second==&image&&
        (image.HasValidLogicalImage()||image.cached_native_current)&&image.texture.handle&&image.gpu_owned&&!image.logical_dirty&&
        !image.HasUnprotectedCpuPages();
}
bool GuestTextureCache::HasCurrentDepthContents(const Entry& image)const{
    return HasCurrentNativeContents(image)&&IsGuestDepthPlaneStorage(image.desc,image.layout);
}
bool GuestTextureCache::IsCachedDepthRecreationSource(const Entry& image,const GuestTextureDesc& target,
    const GuestTextureLayout& layout)const{
    const auto found=image_uids_.find(image.image_uid);
    const bool depth_target=IsGuestDepthPlaneStorage(target,layout);
    const bool color_target=(target.render_target||target.writable)&&!target.depth_target&&
        !target.stencil_address&&image.desc.depth_target&&
        IsDirectDepthWordClass(target,layout);
    if(image.cache!=this||found==image_uids_.end()||found->second!=&image||
       !image.cached_native_current||!image.texture.handle||
       image.desc.samples!=1||target.samples!=1||
       (!depth_target&&!color_target)||
       (!image.desc.depth_target&&(image.desc.stencil_address||
        !IsDirectDepthWordClass(image.desc,image.layout)))||
       (image.desc.depth_target&&!IsGuestDepthPlaneStorage(image.desc,image.layout))||
       std::pair(target.levels,target.layers)<std::pair(image.desc.levels,image.desc.layers))return false;
    // ResolveDepthOverlap recreates on resource growth, color -> depth or changed S presence.
    // Same-resource CPU/B dirtying must instead refresh D from current B.
    const bool recreate=std::pair(target.levels,target.layers)>std::pair(image.desc.levels,image.desc.layers)||
        !image.desc.depth_target||color_target||bool(target.stencil_address)!=bool(image.desc.stencil_address);
    // Differing-aspect CopyImageWithBuffer asserts a one-mip intersection.
    if((target.depth_target!=image.desc.depth_target||bool(target.stencil_address)!=bool(image.desc.stencil_address))&&
       std::min(target.levels,image.desc.levels)!=1)return false;
    return recreate&&CanMapIndependentDepthImage(image.desc,image.layout,target,layout);
}
GuestTextureCache::Entry* GuestTextureCache::FindCachedDepthRecreationSource(const GuestTextureDesc& target,
    const GuestTextureLayout& layout)const{
    Entry* source{};auto [first,last]=address_index_.equal_range(target.address);
    for(auto indexed=first;indexed!=last;++indexed){auto* candidate=indexed->second;
        if(candidate->gpu_owned&&candidate->buffer_alias_version&&candidate->HasValidLogicalImage())continue;
        if(!IsCachedDepthRecreationSource(*candidate,target,layout))continue;
        if(source&&source->texture.handle.Get()!=candidate->texture.handle.Get())
            throw std::runtime_error("Ambiguous initialized cached-depth recreation source");
        source=candidate;
    }
    // A current independent version takes precedence over older cached native
    // history. Query it only when a cached candidate exists (usual binds do not).
    return source&&FindIndependentUploadSource(target,layout)?nullptr:source;
}
void GuestTextureCache::ForgetRecreatedSourceMeta(Entry& source){
    // Original FreeImage unregisters metadata, even while queued descriptors
    // still keep the old native allocation alive.
    for(auto& address:source.metadata_addresses){
        const auto found=surface_metas_.find(address);
        if(found!=surface_metas_.end()&&found->second.owner_uid==source.image_uid)surface_metas_.erase(found);
        address=0;
    }
}
GuestTextureCache::Entry* GuestTextureCache::FindImageRecreationBindingSource(const GuestTextureDesc& target,
    const GuestTextureLayout& layout)const{
    if(auto* source=FindIndependentUploadSource(target,layout)){
        const bool recreate=target.depth_target!=source->desc.depth_target||
            bool(target.stencil_address)!=bool(source->desc.stencil_address)||
            std::pair(target.levels,target.layers)>std::pair(source->desc.levels,source->desc.layers);
        if(recreate&&(target.depth_target||source->desc.depth_target)&&
           CanMapIndependentDepthImage(source->desc,source->layout,target,layout))return source;
    }
    return FindCachedDepthRecreationSource(target,layout);
}
std::shared_ptr<const GuestTextureCache::CachedImageBindingPlan> GuestTextureCache::PlanCachedImageBinding(
    const GuestTextureDesc& target,const GuestTextureLayout& layout,bool current_buffer_source){
    auto* source=FindImageRecreationBindingSource(target,layout);
    if(!source&&!current_buffer_source)return {};
    // A CPU-watched current I remains authoritative after an interior B write.
    // Absence of GPU ownership does not make that image dirty or select B.
    if(!source){auto [first,last]=address_index_.equal_range(target.address);
        for(auto it=first;it!=last;++it){const auto& cached=*it->second;
            if(cached.cpu_watched&&cached.cached_native_current&&!cached.logical_dirty&&
               MatchesGuestMipLayout(cached.layout,layout)&&
               (CompatibleGuestShaderStorage(cached.desc,target)||CanBindReadonlyWordView(cached,target)))return {};
        }
    }
    if(!prepare_image_buffer_||!current_image_buffer_||!CanUploadGpuSnapshot(target,layout,target.writable))
        throw std::runtime_error("Cached image binding has no prepared current-B source");
    auto plan=std::shared_ptr<CachedImageBindingPlan>(new CachedImageBindingPlan);
    plan->cache_=this;plan->storage_=target;if(source)plan->source_pin_=PinBinding(*source);
    // Retain the ALREADY published I before raw B reservation can reconcile
    // an exclusive owner. This creates no future target class or metadata.
    if(source&&HasCurrentNativeContents(*source)&&source->HasValidLogicalImage()&&
       !source->buffer_alias_version&&!RevalidateReadonly(*source))
        throw std::runtime_error("Cannot retain original image before recreation capacity reservation");
    // Capacity only. No new image, native copy, I validity, metadata, ownership
    // or deferred semantic residency is published before shader bindings.
    prepare_image_buffer_(target.address,layout.guest_bytes);
    if(target.stencil_address)prepare_image_buffer_(target.stencil_address,
        LayoutGuestTexture(DescribeGuestStencil(target)).guest_bytes);
    return plan;
}
GuestTextureCache::Entry& GuestTextureCache::BindPlannedImage(const CachedImageBindingPlan& plan,
    ID3D12GraphicsCommandList* list,uint64_t completed){
    if(plan.cache_!=this||!list||!current_image_buffer_)
        throw std::invalid_argument("Image binding plan belongs to another cache or has no source");
    struct Scope {bool& flag;bool old;~Scope(){flag=old;}} scope{planned_image_bind_,planned_image_bind_};
    planned_image_bind_=true;
    // Earlier shader binds may refresh or replace the source class. Select
    // CURRENT original I/B at the real bind, not a copied prewarm version.
    const auto target=ResolveDepthViewStorage(plan.storage_);const auto layout=LayoutGuestTexture(target);
    if(!CanUploadGpuSnapshot(target,layout,target.writable))throw std::runtime_error("Planned image binding lost its exact transfer domain");
    auto* parent=FindDepthViewParent(target,layout);
    if(FindIndependentUploadSource(target,layout)||FindCachedDepthRecreationSource(target,layout)||
       (parent&&!parent->logical_dirty&&(parent->gpu_owned||parent->cpu_watched))){
        const auto depth=std::span(reinterpret_cast<const std::byte*>(target.address),size_t(layout.guest_bytes));
        const auto stencil=target.stencil_address?std::span(reinterpret_cast<const std::byte*>(target.stencil_address),
            size_t(LayoutGuestTexture(DescribeGuestStencil(target)).guest_bytes)):std::span<const std::byte>{};
        auto& result=Upload(target,depth,list,completed,stencil);
        if(!result.gpu_owned&&(!acquire_independent_||!acquire_independent_(result)))
            throw std::runtime_error("Actual planned depth binding cannot retain I ownership");
        return result;
    }
    auto parent_pin=parent?PinBinding(*parent):std::shared_ptr<void>{};
    const bool retain_s=parent&&target.stencil_address&&parent->texture.handle&&
        CompatibleGuestDepthSubview(parent->desc,parent->layout,target,layout)&&
        CompatibleGuestDepthSubview(target,layout,parent->desc,parent->layout);
    auto depth=current_image_buffer_(target.address,layout.guest_bytes);
    auto stencil=target.stencil_address&&!retain_s?current_image_buffer_(target.stencil_address,
        LayoutGuestTexture(DescribeGuestStencil(target)).guest_bytes):BufferImageSource{};
    auto& result=Upload(target,{},list,completed,{},true,&depth.snapshot,target.writable,true,
        UploadPurpose::Preparation,stencil.snapshot.handle?&stencil.snapshot:nullptr);
    if(!acquire_independent_||!acquire_independent_(result))
        throw std::runtime_error("Actual current-B depth binding cannot retain I ownership");
    if(parent&&parent!=&result){
        if(retire_independent_)retire_independent_(*parent);parent->MarkLogicalDirty();
        result.htile_initial_mask=parent->htile_initial_mask;
        for(size_t i=0;i<parent->metadata_addresses.size();++i){const auto address=parent->metadata_addresses[i];
            const auto found=surface_metas_.find(address);
            if(found!=surface_metas_.end()&&found->second.owner_uid==parent->image_uid){
                found->second.owner_uid=result.image_uid;result.metadata_addresses[i]=address;
                parent->metadata_addresses[i]=0;
            }
        }
        result.pending_buffer_residency=std::move(parent->pending_buffer_residency);
    }
    // An actual current-B refresh initializes I even with original GpuModified0.
    // Keep that history separate from validity/ownership and real later writes.
    result.logical_dirty=false;RememberNativeImageStorage(result);
    if(depth.resident)result.pending_buffer_residency.emplace_back(target.address,layout.guest_bytes);
    if(stencil.resident)result.pending_buffer_residency.emplace_back(target.stencil_address,
        LayoutGuestTexture(DescribeGuestStencil(target)).guest_bytes);
    PublishBufferResidency(result);return result;
}
GuestTextureCache::Entry& GuestTextureCache::RecreateCachedDepthAtBind(Entry& image,const GuestTextureDesc& target,
    const GuestTextureLayout& layout,ID3D12GraphicsCommandList* list,uint64_t completed){
    auto* source=&image;
    if(!list||!read_cached_depth_||!acquire_independent_||!retire_independent_||
       !CanUploadGpuSnapshot(source->desc,source->layout)||!CanUploadGpuSnapshot(target,layout,target.writable))
        throw std::runtime_error("Cached depth recreation has no exact native source/seed/ownership transfer");
    auto pin=PinBinding(*source);const bool modified=source->logical_gpu_modified;
    auto residency=source->pending_buffer_residency;
    auto captured=read_cached_depth_(*source,target,layout);
    if(bool(captured.stencil.handle)!=bool(target.stencil_address))
        throw std::runtime_error("Cached depth recreation lost its target stencil seed");
    residency.insert(residency.end(),captured.seed_residency.begin(),captured.seed_residency.end());
    auto& result=UploadVersion(target,{},list,completed,{},true,&captured.depth,target.writable,false,nullptr,
        UploadPurpose::Preparation,captured.stencil.handle?&captured.stencil:nullptr,true);
    // An actual copy initialized the NEW native I. Do not revive the dirty old
    // I or invent original GpuModified history for a CPU-initialized source.
    result.htile_initial_mask=0;
    result.pending_buffer_residency=std::move(residency);
    if(!(acquire_independent_replacement_?acquire_independent_replacement_(result,*source):acquire_independent_(result)))
        throw std::runtime_error("Recreated cached depth could not retain target ownership");
    retire_independent_(*source);source->MarkLogicalDirty();
    ForgetRecreatedSourceMeta(*source);
    result.logical_gpu_modified=modified;result.logical_dirty=false;
    RememberNativeImageStorage(result);
    PublishBufferResidency(result);
    return result;
}
GuestTextureDesc GuestTextureCache::ResolveColorViewStorage(const GuestTextureDesc& requested)const{
    if(!requested.render_target||requested.depth_target||requested.writable||requested.stencil_address)
        return requested;
    const auto layout=LayoutGuestTexture(requested);
    auto* parent=FindValidIndependentImage(requested,layout);
    if(!parent)if(auto rebased=RebaseColorSubresource(requested,layout,true))return rebased->view;
    if(!parent||!CompatibleGuestColorSubview(parent->desc,parent->layout,requested,layout))
        return requested;
    auto storage=parent->desc;
    storage.render_target=true;
    storage.data_format=requested.data_format;storage.number_format=requested.number_format;
    storage.native_raw32=requested.native_raw32;storage.native_packed10_raw=requested.native_packed10_raw;
    storage.first_mip=requested.first_mip;storage.first_layer=requested.first_layer;
    storage.view_layers=requested.view_layers;storage.min_lod=requested.min_lod;storage.swizzle=requested.swizzle;
    // A native view alone does not prove a guest address mapping. Keep every
    // parent mip offset/slice and the complete memory envelope unchanged.
    const auto physical=LayoutGuestTexture(storage);
    if(!MatchesGuestMipLayout(parent->layout,physical)||
       !MatchesGuestUploadStorage(*parent,storage,physical))return requested;
    return storage;
}
GuestTextureDesc GuestTextureCache::GrowColorAttachmentStorage(const GuestTextureDesc& target,
    const GuestTextureDesc& sampled){
    if(!target.render_target||sampled.render_target)return target;
    const auto prefix_layout=LayoutGuestTexture(target),full_layout=LayoutGuestTexture(sampled);
    if(!CanMapIndependentColorGrowth(target,prefix_layout,sampled,full_layout))return target;
    auto storage=sampled;storage.render_target=true;
    storage.data_format=target.data_format;storage.number_format=target.number_format;
    storage.native_raw32=target.native_raw32;storage.native_packed10_raw=target.native_packed10_raw;
    storage.first_mip=target.first_mip;storage.first_layer=target.first_layer;
    storage.view_layers=target.view_layers;storage.min_lod=target.min_lod;storage.swizzle=target.swizzle;
    if(!MatchesGuestMipLayout(full_layout,LayoutGuestTexture(storage)))return target;
    return storage;
}
GuestTextureDesc GuestTextureCache::ResolveDepthViewStorage(const GuestTextureDesc& requested)const{
    const auto layout=LayoutGuestTexture(requested);
    if(!IsDirectDepthWordClass(requested,layout))return requested;
    auto* parent=FindDepthViewParent(requested,layout);
    if(parent){
        auto storage=parent->desc;
        storage.first_mip=requested.first_mip;storage.first_layer=requested.first_layer;
        storage.view_layers=requested.view_layers;storage.min_lod=requested.min_lod;storage.swizzle=requested.swizzle;
        if(auto sub=ReadonlyDepthSubresource(parent->desc,parent->layout,requested,layout)){
            storage.first_mip+=sub->first;storage.first_layer+=sub->second;
            storage.min_lod+=sub->first*256;
        }
        return storage;
    }
    if(!requested.depth_target&&!requested.render_target&&!requested.writable)return requested;
    // ResolveDepthOverlap recreates the requested depth/aspect image with
    // max(requested.resources, cached.resources). SubresourceExtent orders
    // levels first, then layers; independently maximizing both is different.
    // Resolve BEFORE caller source/prewrite captures, not inside Upload after
    // it has already been given a smaller guest byte envelope.
    Entry* preceding{};bool preceding_independent=false;auto resolved=requested;
    auto [first,last]=address_index_.equal_range(requested.address);
    for(auto indexed=first;indexed!=last;++indexed){auto* candidate=indexed->second;
        // ResolveDepthOverlap rejects color/color pairs before comparing
        // resource extents. A lexicographic mip/layer maximum from an unrelated
        // color image can otherwise manufacture an invalid requested view.
        if(!candidate->desc.depth_target&&!requested.depth_target)continue;
        auto storage=requested;
        if(std::pair(candidate->desc.levels,candidate->desc.layers)>std::pair(storage.levels,storage.layers)){
            storage.levels=candidate->desc.levels;storage.layers=candidate->desc.layers;
        }
        if(storage==requested||(storage.samples>1&&storage.levels!=1))continue;
        const auto physical=LayoutGuestTexture(storage);
        if(!CanMapIndependentDepthImage(candidate->desc,candidate->layout,storage,physical))continue;
        const bool independent=candidate->gpu_owned&&candidate->buffer_alias_version&&candidate->HasValidLogicalImage();
        const bool cached=IsCachedDepthRecreationSource(*candidate,storage,physical);
        if(!independent&&!cached)continue;
        if(!cached&&candidate->HasUnprotectedCpuPages())
            throw std::runtime_error("Depth recreation extent requires boundary validation");
        if(preceding&&preceding_independent!=independent){
            if(!independent)continue;
            preceding=nullptr;
        }
        if(preceding&&candidate->texture.handle.Get()!=preceding->texture.handle.Get())
            throw std::runtime_error("Ambiguous depth recreation extent source");
        preceding=candidate;preceding_independent=independent;resolved=storage;
    }
    return resolved;
}
GuestTextureCache::Entry* GuestTextureCache::FindIndependentUploadSource(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    if(auto* direct=FindValidIndependentImage(desc,layout))return direct;
    Entry* result{};auto [first,last]=address_index_.equal_range(desc.address);
    for(auto indexed=first;indexed!=last;++indexed){auto* entry=indexed->second;
        if(!entry->gpu_owned||!entry->buffer_alias_version||entry->logical_dirty)continue;
        if(entry->HasUnprotectedCpuPages())
            throw std::runtime_error("Independent upload source requires boundary validation");
        const bool block=MatchesReferenceBlockImage(entry->desc,entry->layout,desc,layout)||
            MatchesReadonlyBlockMipPrefix(entry->desc,entry->layout,desc,layout)||
            (entry->buffer_alias_watch&&CanRetainIndependentImage(entry->desc,entry->layout)&&
             CanBindReadonlyBcMipPrefix(*entry,desc,layout))||
            MatchesReferencePartialBlockExpansion(entry->desc,entry->layout,desc,layout);
        // A valid smaller mip chain is still the preceding I source. The
        // caller must expand it privately before final buffer selection; a
        // failed lookup here would instead seed BindTextures from unrelated B.
        const bool color_growth=CanMapIndependentColorGrowth(entry->desc,entry->layout,desc,layout);
        const bool mip_growth=entry->desc.levels<desc.levels&&
            CompatibleGuestShaderMipPrefixes(entry->desc,entry->layout,desc,layout);
        const bool depth_transfer=CanMapIndependentDepthImage(entry->desc,entry->layout,desc,layout);
        const bool depth_view=CompatibleGuestDepthSubview(entry->desc,entry->layout,desc,layout);
        if(!block&&!mip_growth&&!color_growth&&!depth_transfer&&!depth_view&&(!MatchesGuestMipLayout(entry->layout,layout)||
           (!CompatibleGuestPackedStorage(entry->desc,desc)&&
            !MatchesReferenceColorImage(entry->desc,entry->layout,desc,layout)&&
            !CanMapIndependentDepthImage(entry->desc,entry->layout,desc,layout))))continue;
        if(result&&(entry->texture.handle.Get()!=result->texture.handle.Get()||
            entry->expanded_guest_backing.handle.Get()!=result->expanded_guest_backing.handle.Get()||
            entry->expanded_written_mask.handle.Get()!=result->expanded_written_mask.handle.Get()))
            throw std::runtime_error("Ambiguous independent image interpretation source");
        if(!result)result=entry;
    }
    return result;
}
GuestTextureCache::Entry* GuestTextureCache::FindWritableBlockParent(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    return desc.writable?FindBlockViewParent(desc,layout):nullptr;
}
GuestTextureCache::Entry* GuestTextureCache::FindReadonlyBlockParent(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    // Direct BC readers keep the actual parent resource. A selected prefix
    // proves its physical storage without shortening the parent's allocation.
    if(!IsNativeReadonlyGuestBlockStorage(desc,layout))return nullptr;
    auto* parent=FindValidIndependentImage(desc,layout);
    if(parent&&IsNativeReadonlyGuestBlockStorage(parent->desc,parent->layout))return parent;
    parent=nullptr;
    auto [first,last]=address_index_.equal_range(desc.address);
    for(auto it=first;it!=last;++it){auto* candidate=it->second;
        if(!candidate->gpu_owned||!candidate->buffer_alias_version||!candidate->buffer_alias_watch||
           candidate->logical_dirty||!CanBindReadonlyBcMipPrefix(*candidate,desc,layout)||
           !CanRetainIndependentImage(candidate->desc,candidate->layout))continue;
        if(parent&&parent!=candidate)throw std::runtime_error("Ambiguous readonly BC prefix parent");
        parent=candidate;
    }
    return parent;
}
bool GuestTextureCache::CanTransferDependentBlockView(const Entry& parent,const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    // Only the complete parent owns CPU protection. A private prefix needs
    // word-aligned GPU transfers, not a separate page-aligned ownership range.
    if(parent.cache!=this||!parent.texture.handle||!parent.gpu_owned||
       !parent.buffer_alias_version||!parent.buffer_alias_watch||parent.logical_dirty||
       !desc.writable||desc.address!=parent.desc.address||!layout.guest_bytes||
       layout.guest_bytes>parent.layout.guest_bytes||
       parent.layout.guest_bytes>UINT64_MAX-parent.desc.address||
       !CanUploadGpuSnapshot(desc,layout,true))return false;
    auto view=desc;view.writable=false;
    return MatchesReadonlyBlockMipPrefix(parent.desc,parent.layout,view,layout)&&
        CanRetainIndependentImage(parent.desc,parent.layout);
}
bool GuestTextureCache::IsLiveBlockParent(const WritableBlockAlias& alias,const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    if(!alias.parent||!alias.parent_pin)return false;
    const auto owner=image_uids_.find(alias.parent_uid);
    if(owner==image_uids_.end()||owner->second!=alias.parent)return false;
    return alias.parent->image_uid==alias.parent_uid&&alias.parent->BindingPinned()&&
        CanTransferDependentBlockView(*alias.parent,desc,layout);
}
GuestTextureCache::Entry* GuestTextureCache::FindBlockViewParent(const GuestTextureDesc& desc,
    const GuestTextureLayout& layout)const{
    // Querying a readonly image must not reinterpret BC/depth/expanded formats
    // as writable just to test retention. Only raw UINT block-word views enter.
    if(layout.block_extent!=1||layout.expanded_bytes_per_pixel||desc.number_format!=4||
       !((desc.data_format==11&&layout.bytes_per_pixel==8)||(desc.data_format==14&&layout.bytes_per_pixel==16))||
       desc.render_target||desc.depth_target||desc.stencil_address||
       desc.first_layer>=desc.layers||!desc.view_layers||
       desc.view_layers>desc.layers-desc.first_layer)return nullptr;
    // A layer subset is a view of full-layer transient storage, not another
    // owner of a shortened guest range. Retain and transfer all original layers.
    auto storage=desc;storage.writable=true;storage.first_layer=0;storage.view_layers=storage.layers;
    Entry* parent{};auto [first,last]=address_index_.equal_range(desc.address);
    for(auto it=first;it!=last;++it){auto* candidate=it->second;
        if(!CanTransferDependentBlockView(*candidate,storage,layout))continue;
        if(parent&&parent!=candidate)throw std::runtime_error("Ambiguous BC block-view parent");
        parent=candidate;
    }
    return parent;
}
bool GuestTextureCache::IsLiveWritableBlockAlias(const WritableBlockAlias& alias)const{
    if(!alias.parent||!alias.image||alias.parent==alias.image||!alias.parent_pin||!alias.image_pin)return false;
    const auto& image=*alias.image;
    if(!IsLiveBlockParent(alias,image.desc,image.layout))return false;
    const auto owner=image_uids_.find(image.image_uid);
    if(image.cache!=this||owner==image_uids_.end()||owner->second!=&image||!image.BindingPinned()||
       image.gpu_owned||image.buffer_alias_version||image.cpu_watched||!image.logical_dirty||
       !image.desc.writable||image.desc.first_layer||image.desc.view_layers!=image.desc.layers)return false;
    return true;
}
std::optional<GuestTextureDesc> GuestTextureCache::RebaseBlockSubresource(
    const GuestTextureDesc& desc,const GuestTextureLayout& layout,bool array)const{
    // A standalone small/odd BC view may need an expanded host allocation,
    // while the existing parent's selected native BC mip needs no allocation.
    // Classify the guest block role here; validate the native typed layout only
    // after proving the original physical addresses and rebasing the descriptor.
    const bool native_bc=!desc.writable&&desc.data_format>=35&&desc.data_format<=41&&
        layout.block_extent==4&&(layout.bytes_per_pixel==8||layout.bytes_per_pixel==16);
    // Pinned ImageInfo::MipOf/SliceOf recognizes a one-level image starting
    // at a physical mip/slice boundary. Rebase it before grouping, so an alias
    // and an ordinary selected-mip view share one child and one parent commit.
    if(desc.levels!=1||desc.first_mip||!desc.layers||desc.first_layer>=desc.layers||
       !desc.view_layers||desc.view_layers>desc.layers-desc.first_layer||
       desc.render_target||desc.depth_target||desc.stencil_address||desc.one_dimensional||
       desc.volume_depth||desc.samples!=1||desc.pow2_padding||desc.storage_height||
       desc.storage_slice_bytes||layout.mips.size()!=1||desc.address%4||
       !GuestTiler::CanAddressGuestBytes(layout.guest_bytes)||
       (!native_bc&&(layout.expanded_bytes_per_pixel||layout.block_extent!=1||desc.number_format!=4||
       !((desc.data_format==11&&layout.bytes_per_pixel==8)||
         (desc.data_format==14&&layout.bytes_per_pixel==16))||
       !CanUploadGpuSnapshot(desc,layout,true))))return std::nullopt;
    // An existing independent version is a separate authority. Never silently
    // replace it with another overlapping image's contents.
    if(FindValidIndependentImage(desc,layout))return std::nullopt;
    const auto& local=layout.mips[0];
    if(local.offset||!local.slice_bytes)return std::nullopt;
    Entry* owner{};std::optional<GuestTextureDesc> result;
    for(const auto& candidate:entries_){const auto& parent=*candidate;
        if(!parent.gpu_owned||!parent.buffer_alias_version||!parent.buffer_alias_watch||
           parent.logical_dirty||desc.address<=parent.desc.address||
           !IsNativeReadonlyGuestBlockStorage(parent.desc,parent.layout)||
           (native_bc&&desc.data_format!=parent.desc.data_format)||
           (!array&&parent.desc.layers!=1)||parent.desc.width%4||
           parent.desc.height%4||parent.desc.pitch%4)continue;
        const auto delta=desc.address-parent.desc.address;
        if(delta>=parent.layout.guest_bytes||layout.guest_bytes>parent.layout.guest_bytes-delta)continue;
        for(uint32_t mip=0;mip<parent.desc.levels;++mip){const auto& physical=parent.layout.mips[mip];
            if(!physical.slice_bytes||
               physical.pitch!=local.pitch||physical.height!=local.height||
               physical.slice_bytes!=local.slice_bytes||u64(desc.pitch)!=u64(physical.pitch)*layout.block_extent||
               delta<physical.offset)continue;
            const auto layer_bytes=delta-physical.offset;
            if(layer_bytes%physical.slice_bytes)continue;
            const auto layer=layer_bytes/physical.slice_bytes;
            if(layer>=parent.desc.layers||desc.layers>parent.desc.layers-layer)continue;
            if(!MatchesGuestSliceAddressing(parent.desc,parent.layout,mip,desc,layout,uint32_t(layer)))continue;
            auto view=desc;view.address=parent.desc.address;
            // The local bank swizzle has been proved against slice rotation.
            // Canonical storage must use the full parent's coordinates/swizzle.
            view.bank_swizzle=parent.desc.bank_swizzle;
            const auto divisor=native_bc?1u:4u;
            view.width=parent.desc.width/divisor;view.height=parent.desc.height/divisor;view.pitch=parent.desc.pitch/divisor;
            if(mip>=std::bit_width(std::max(view.width,view.height))||
               desc.width!=std::max(view.width>>mip,1u)||desc.height!=std::max(view.height>>mip,1u))continue;
            view.levels=mip+1;view.first_mip=mip;view.layers=parent.desc.layers;
            view.first_layer=uint32_t(layer)+desc.first_layer;
            // A standalone one-level SRV clamps to its mip0. After rebasing,
            // that is the parent's selected mip, not the parent's mip0.
            view.min_lod=mip*256;
            if(native_bc){
                if(!CanBindReadonlyBcMipPrefix(parent,view,LayoutGuestTexture(view))||
                   !CanRetainIndependentImage(parent.desc,parent.layout))continue;
            }else{
                auto storage=view;storage.writable=true;storage.first_layer=0;storage.view_layers=storage.layers;
                if(!CanTransferDependentBlockView(parent,storage,LayoutGuestTexture(storage)))continue;
            }
            if(owner&&owner!=&parent)throw std::runtime_error("Ambiguous shifted BC block-view parent");
            owner=candidate.get();result=view;
        }
    }
    return result;
}
std::optional<GuestTextureCache::ColorSubresource> GuestTextureCache::RebaseColorSubresource(
    const GuestTextureDesc& desc,const GuestTextureLayout& layout,bool array)const{
    if(desc.writable||desc.depth_target||desc.stencil_address||
       desc.one_dimensional||desc.volume_depth||desc.samples!=1||desc.levels!=1||
       desc.first_mip||!desc.layers||desc.first_layer>=desc.layers||!desc.view_layers||
       desc.view_layers>desc.layers-desc.first_layer||layout.mips.size()!=1||
       layout.block_extent!=1||layout.expanded_bytes_per_pixel||desc.pow2_padding||
       (!desc.render_target&&(desc.storage_height||desc.storage_slice_bytes)))return std::nullopt;
    if(FindValidIndependentImage(desc,layout))return std::nullopt;
    const auto& local=layout.mips[0];
    if(local.offset||!local.slice_bytes)return std::nullopt;
    std::optional<ColorSubresource> result;
    for(const auto& candidate:entries_){auto& parent=*candidate;
        if(!parent.gpu_owned||parent.logical_dirty||parent.desc.writable||
           parent.desc.depth_target||parent.desc.stencil_address||parent.desc.samples!=1||
           parent.desc.volume_depth||parent.desc.one_dimensional||
           parent.layout.block_extent!=1||parent.layout.expanded_bytes_per_pixel||
           parent.layout.bytes_per_pixel!=layout.bytes_per_pixel||
           desc.address<=parent.desc.address||(!array&&parent.desc.layers!=1))continue;
        const auto delta=desc.address-parent.desc.address;
        if(delta>=parent.layout.guest_bytes||layout.guest_bytes>parent.layout.guest_bytes-delta)continue;
        for(UINT mip=0;mip<parent.desc.levels;++mip){const auto& part=parent.layout.mips[mip];
            if(!part.slice_bytes||delta<part.offset||desc.width!=std::max(parent.desc.width>>mip,1u)||
               desc.height!=std::max(parent.desc.height>>mip,1u)||desc.pitch!=part.pitch)continue;
            const auto relative=delta-part.offset;
            if(!layout.guest_bytes||relative%layout.guest_bytes||relative%part.slice_bytes)continue;
            const auto layer=relative/part.slice_bytes;
            if(layer>=parent.desc.layers||desc.layers>parent.desc.layers-layer||
               !MatchesGuestSliceAddressing(parent.desc,parent.layout,mip,desc,layout,UINT(layer)))continue;
            auto view=parent.desc;view.render_target=desc.render_target;
            view.data_format=desc.data_format;view.number_format=desc.number_format;
            view.native_raw32=desc.native_raw32;view.native_packed10_raw=desc.native_packed10_raw;
            view.sampled_depth=desc.sampled_depth;view.swizzle=desc.swizzle;
            view.first_mip=mip;view.levels=desc.render_target?parent.desc.levels:mip+1;view.min_lod=mip*256;
            view.first_layer=UINT(layer)+desc.first_layer;view.view_layers=desc.view_layers;
            if(!CompatibleGuestColorSubview(parent.desc,parent.layout,view,LayoutGuestTexture(view))||
               !CanBindNativeWordFormat(parent,view)||
               (desc.render_target&&!(parent.texture.handle->GetDesc().Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)))continue;
            if(parent.HasUnprotectedCpuPages())
                throw std::runtime_error("Color subresource lookup requires boundary validation");
            if(result&&result->parent!=&parent){
                if(result->parent->cached_native_current&&!parent.cached_native_current)continue;
                if(result->parent->cached_native_current==parent.cached_native_current)
                    throw std::runtime_error("Ambiguous shifted color-view parent");
            }
            result=ColorSubresource{&parent,view};
        }
    }
    return result;
}
std::optional<GuestTextureCache::WritableBlockAlias> GuestTextureCache::BeginWritableBlockAlias(
    const GuestTextureDesc& desc,ID3D12GraphicsCommandList* list,uint64_t completed){
    const auto layout=LayoutGuestTexture(desc);auto* parent=FindWritableBlockParent(desc,layout);
    if(!parent)return std::nullopt;
    if(!read_independent_||!acquire_independent_||!retire_independent_||!list)
        throw std::runtime_error("Writable BC alias has no parent transfer");
    WritableBlockAlias alias;alias.parent=parent;alias.parent_uid=parent->image_uid;
    alias.parent_pin=PinBinding(*parent);
    PublishBufferResidency(*parent);
    auto snapshot=read_independent_(*parent,desc.address,layout.guest_bytes);
    auto storage=desc;storage.first_layer=0;storage.view_layers=storage.layers;
    auto& child=UploadVersion(storage,{},list,completed,{},true,&snapshot,true,false,&alias,UploadPurpose::Binding);
    // Snapshot Upload cannot reuse an owned or binding-pinned image. Do not
    // convert any preexisting ownership state into a detached child by clearing it.
    if(child.gpu_owned||child.buffer_alias_version||child.cpu_watched)
        throw std::runtime_error("Writable BC child unexpectedly retained independent ownership");
    if(transfer_submission_)transfer_submission_();
    child.MarkLogicalDirty();alias.image=&child;alias.image_pin=PinBinding(child);
    if(!IsLiveWritableBlockAlias(alias))throw std::runtime_error("Writable BC alias initialization lost its parent");
    return alias;
}
GuestTextureCache::Entry& GuestTextureCache::CommitWritableBlockAlias(WritableBlockAlias& alias,
    ID3D12GraphicsCommandList* list,uint64_t completed){
    if(!IsLiveWritableBlockAlias(alias)||!read_independent_||!acquire_independent_||!retire_independent_)
        throw std::runtime_error("Writable BC alias lost its parent before writeback");
    auto& parent=*alias.parent;auto& image=*alias.image;
    // All untouched blocks/mips come from CURRENT parent I. The private child
    // may overwrite only its proved prefix and never acquires a physical watch.
    auto seed=read_independent_(parent,parent.desc.address,parent.layout.guest_bytes);
    auto prefix=RetileImageVersion(image,seed,list,completed,&alias);
    prefix.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    seed.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyBufferRegion(seed.handle.Get(),0,prefix.handle.Get(),0,image.layout.guest_bytes));
    seed.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    auto& replacement=Upload(parent.desc,{},list,completed,{},true,&seed);
    if(transfer_submission_)transfer_submission_();
    if(!acquire_independent_(replacement))
        throw std::runtime_error("Writable BC replacement could not acquire parent ownership");
    replacement.NoteLogicalImageWrite();
    retire_independent_(parent);
    alias.parent=nullptr; // A transaction cannot be committed twice.
    Diagnostics::Count("image-version.writable-block-view");
    return replacement;
}
void GuestTextureCache::PublishBufferResidency(Entry& entry){
    if(entry.pending_buffer_residency.empty())return;
    if(!publish_buffer_residency_)throw std::logic_error("Prepared image B residency has no publisher");
    // Keep the receipt on failure; a retry is idempotent. No cache allocation,
    // resource recreation or descriptor replacement belongs in the publisher.
    publish_buffer_residency_(entry.pending_buffer_residency);
    entry.pending_buffer_residency.clear();
}
GuestTextureCache::Entry& GuestTextureCache::Upload(const GuestTextureDesc& d,std::span<const std::byte> bytes,ID3D12GraphicsCommandList* list,uint64_t completed,std::span<const std::byte> stencil_bytes,bool guest_snapshot,const Resource* gpu_snapshot,bool writable_snapshot,bool current_buffer_snapshot,UploadPurpose purpose,const Resource* stencil_gpu_snapshot){
    return UploadVersion(d,bytes,list,completed,stencil_bytes,guest_snapshot,gpu_snapshot,
        writable_snapshot,current_buffer_snapshot,nullptr,purpose,stencil_gpu_snapshot);
}
GuestTextureCache::Entry& GuestTextureCache::UploadVersion(const GuestTextureDesc& d,
    std::span<const std::byte> bytes,ID3D12GraphicsCommandList* list,uint64_t completed,
    std::span<const std::byte> stencil_bytes,bool guest_snapshot,const Resource* gpu_snapshot,
    bool writable_snapshot,bool current_buffer_snapshot,const WritableBlockAlias* dependency,UploadPurpose purpose,
    const Resource* stencil_gpu_snapshot,bool fresh_depth_recreation){
    // A protected guest memcpy or a cache callback may Finish while preparing
    // this upload. A command-list pointer captured by the caller is then closed
    // (and the scheduler may select a different context). Keep those completed
    // operations in place and record remaining commands on the current context.
    Entry* recording_entry=nullptr;
    const auto recording=[this,list,&recording_entry]{
        auto* current=recording_list_?recording_list_():list;
        if(!current)throw std::invalid_argument("Texture upload has no recording command list");
        // A reentrant Finish also consumed the old pending receipt. The next
        // commands and their pools belong to the resumed submission fence.
        if(recording_entry)recording_entry->pending=true;
        return current;
    };
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Upload,"layout-and-cache-lookup");
    diagnostic.Detail("address=%llx size=%ux%u levels=%u layers=%u tile=%u bytes=%zu",
        (unsigned long long)d.address,d.width,d.height,d.levels,d.layers,d.tile_mode,bytes.size());
    auto layout=LayoutGuestTexture(d);
    const bool gpu_snapshot_any=gpu_snapshot||stencil_gpu_snapshot;
    const bool dependent=dependency&&d.writable&&guest_snapshot&&gpu_snapshot&&writable_snapshot&&
        !current_buffer_snapshot&&stencil_bytes.empty()&&!dependency->image&&!dependency->image_pin&&
        !d.first_layer&&d.view_layers==d.layers&&IsLiveBlockParent(*dependency,d,layout);
    if(dependency&&!dependent)throw std::invalid_argument("Invalid dependent block image upload lease");
    if(current_buffer_snapshot&&!guest_snapshot)
        throw std::invalid_argument("Current buffer image source must be an explicit snapshot");
    const bool refresh_current=!guest_snapshot||current_buffer_snapshot;
    Resource retained_stencil;std::shared_ptr<void> retained_stencil_pin;Entry* stencil_parent{};
    if(refresh_current&&d.depth_target&&d.stencil_address&&d.samples==1){
        auto* parent=FindDepthViewParent(d,layout);
        if(parent&&parent->texture.handle&&
           CompatibleGuestDepthSubview(parent->desc,parent->layout,d,layout)&&
           CompatibleGuestDepthSubview(d,layout,parent->desc,parent->layout)){
            // Pinned RefreshImage uploads DEPTH ONLY into an existing image.
            // Dirty CPU/B data does not replace its native stencil contents.
            stencil_parent=parent;retained_stencil_pin=PinBinding(*parent);retained_stencil=parent->texture;
        }
    }
    bool inherited_modified=false;
    std::vector<std::shared_ptr<void>> history_pins;
    if(refresh_current){
        auto [first,last]=address_index_.equal_range(d.address);
        for(auto indexed=first;indexed!=last;++indexed){auto& previous=*indexed->second;
            const bool block=MatchesReferenceBlockImage(previous.desc,previous.layout,d,layout)||
                MatchesReferencePartialBlockExpansion(previous.desc,previous.layout,d,layout)||
                MatchesReadonlyBcMipPrefix(previous.desc,previous.layout,d,layout)||
                MatchesExpandedReadonlyBc(previous.desc,previous.layout,d,layout);
            if(!previous.logical_gpu_modified||(current_buffer_snapshot&&!previous.logical_dirty)||
                (!block&&!MatchesGuestMipLayout(previous.layout,layout)&&
                 !CompatibleGuestImageMipGeometry(previous.desc,previous.layout,d,layout)&&
                 !CanMapIndependentDepthImage(previous.desc,previous.layout,d,layout)&&
                 !CanMapIndependentColorGrowth(previous.desc,previous.layout,d,layout)))continue;
            if(previous.desc!=d&&!CompatibleGuestShaderStorage(previous.desc,d)&&
               !CompatibleGuestPackedStorage(previous.desc,d)&&
               !CompatibleGuestAttachmentStorage(previous.desc,d)&&
               !CompatibleGuestSampledStorage(previous.desc,d)&&
                !MatchesReferenceColorImage(previous.desc,previous.layout,d,layout)&&
                !CanMapIndependentDepthImage(previous.desc,previous.layout,d,layout)&&
                !CanMapIndependentColorGrowth(previous.desc,previous.layout,d,layout)&&
                !CompatibleGuestImageMipGeometry(previous.desc,previous.layout,d,layout)&&!block)continue;
            // ReserveMemory may evict cache entries. Keep the logical identity
            // witness alive until the replacement's refresh is published.
            history_pins.push_back(PinBinding(previous));inherited_modified=true;
        }
    }
    const auto finish_refresh=[&](Entry& entry)->Entry&{
        if(refresh_current&&(!current_buffer_snapshot||inherited_modified)){
            entry.logical_gpu_modified|=inherited_modified;entry.logical_dirty=false;
        }else{
            // A private prewrite/feedback version is not proof that this
            // logical image has refreshed from its current buffer domain.
            entry.MarkLogicalDirty();
        }
        if(purpose==UploadPurpose::Binding)PublishBufferResidency(entry);
        if(refresh_current)RememberNativeImageStorage(entry);
        return entry;
    };
    const bool preserve_expanded=d.samples>1&&(d.data_format==9||d.data_format==19)&&
        !d.writable&&!d.render_target&&!d.depth_target&&!d.stencil_address&&
        HasExactPackedExpansion(d,layout);
    uint64_t expanded_snapshot_bytes{};
    if(preserve_expanded){
        for(const auto length:{layout.guest_bytes,GuestTiler::WrittenBytes(layout.guest_bytes)}){
            D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width=(length+3)&~uint64_t(3);buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
            buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            const auto allocation=device_->GetResourceAllocationInfo(0,1,&buffer).SizeInBytes;
            if(!allocation||allocation==UINT64_MAX||expanded_snapshot_bytes>UINT64_MAX-allocation)
                throw std::overflow_error("Invalid exact expanded snapshot backing allocation");
            expanded_snapshot_bytes+=allocation;
        }
    }
    if(gpu_snapshot){
        if(!guest_snapshot||!gpu_snapshot->handle||!CanUploadGpuSnapshot(d,layout,writable_snapshot)||
            gpu_snapshot->handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
            gpu_snapshot->handle->GetDesc().Width<layout.guest_bytes||
            gpu_snapshot->state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            throw std::invalid_argument("Invalid GPU guest image snapshot");
    }else if(bytes.size()<layout.guest_bytes)throw std::invalid_argument("short guest texture upload");
    Collect(completed);
    if(guest_snapshot&&d.writable&&!writable_snapshot)throw std::invalid_argument("Guest image snapshot must be read-only");
    if(writable_snapshot&&(!guest_snapshot||(!dependent&&!CanRetainBufferAlias(d,layout))))
        throw std::invalid_argument("Writable image snapshot has no independent cache ownership");
    if(d.writable)ValidateWritableFormat(layout.format);
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    if(d.stencil_address){stencil_desc=DescribeGuestStencil(d);stencil_layout=LayoutGuestTexture(stencil_desc);
        if(!retained_stencil.handle&&!stencil_gpu_snapshot&&stencil_bytes.size()<stencil_layout.guest_bytes)throw std::invalid_argument("Short guest stencil upload");}
    if(stencil_gpu_snapshot&&(!guest_snapshot||!d.stencil_address||!d.depth_target||
       !CanUploadGpuSnapshot(d,layout,writable_snapshot)||!stencil_gpu_snapshot->handle||
       stencil_gpu_snapshot->handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       stencil_gpu_snapshot->handle->GetDesc().Width<stencil_layout.guest_bytes||
       stencil_gpu_snapshot->state!=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
        throw std::invalid_argument("Invalid GPU guest stencil snapshot");
    if(!guest_snapshot)if(auto* source=FindCachedDepthRecreationSource(d,layout)){
        if(purpose!=UploadPurpose::Binding)
            throw std::logic_error("Cached native prewarm must use a private binding plan");
        return RecreateCachedDepthAtBind(*source,d,layout,recording(),completed);
    }
    auto address_entries=[&](){return address_index_.equal_range(d.address);};
    auto [owned_first,owned_last]=address_entries();
    auto* independent_source=guest_snapshot?nullptr:FindIndependentUploadSource(d,layout);
    auto* independent_reuse=independent_source&&
        (MatchesGuestUploadStorage(*independent_source,d,layout)||
         CanBindReadonlyBcMipPrefix(*independent_source,d,layout))?
        independent_source:nullptr;
    Entry* reuse=independent_reuse;
    for(auto indexed=owned_first;indexed!=owned_last;++indexed){auto* existing=indexed->second;
      if(guest_snapshot&&existing->cpu_watched){
        if(release_immutable_)release_immutable_(*existing);existing->cpu_watched=false;
      }
      const bool pending_coherent=!gpu_snapshot_any&&!existing->gpu_owned&&!existing->cpu_watched&&existing->pending&&existing->sampled_snapshot.size()==layout.guest_bytes&&
        !memcmp(existing->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes))&&
         (!d.stencil_address||(retained_stencil.handle?
          existing->texture.handle.Get()==retained_stencil.handle.Get():
          (existing->stencil_snapshot.size()==stencil_layout.guest_bytes&&
           !memcmp(existing->stencil_snapshot.data(),stencil_bytes.data(),size_t(stencil_layout.guest_bytes)))));
      if((pending_coherent||(!guest_snapshot&&!existing->logical_dirty&&(existing->gpu_owned||existing->cpu_watched)))&&
        MatchesGuestUploadStorage(*existing,d,layout)){
        if(!independent_source&&(!reuse||(existing->desc==d&&reuse->desc!=d)))reuse=existing;
      }
    }
    if(reuse){
        // Page watchers prove that no CPU access can have changed these bytes.
        // Keep the original native image through subsequent draws/sampling.
        recording_entry=reuse;
        reuse->last_use=++use_sequence_;reuse->pending=true;
        if(reuse->texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)reuse->texture.UavBarrier(recording());
        reuse->texture.Transition(recording(),d.writable&&d.samples==1?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return finish_refresh(*reuse);
    }
    // A different view (in particular a compute UAV) may overlap an owned
    // render target without qualifying for direct GPU reuse. Resolve ALL
    // overlapping backing on the caller before any comparison or parallel
    // detile can fault on a worker waiting for the caller's execution mutex.
    for(auto indexed=owned_first;indexed!=owned_last;++indexed){auto& existing=*indexed->second;
        if(!guest_snapshot&&(!independent_source||&existing==independent_source)&&
           existing.gpu_owned&&!existing.logical_dirty&&CompatibleGuestPackedStorage(existing.desc,d)){
            existing.MarkLogicalDirty();
            if(ConvertPackedBacking(existing,d,layout,recording()))return finish_refresh(existing);
            // A rejected conversion has not mutated its original resource.
            existing.logical_dirty=false;
        }
    }
    const auto overlaps=[](uint64_t a,uint64_t as,uint64_t b,uint64_t bs){
        return as&&bs&&(a<=b?b-a<as:a-b<bs);
    };
    // A prewrite snapshot already owns complete independent contents. Current
    // shader writers may have acquired guest ownership after capture; resolving
    // them here would retire their initialization before their recorded writes.
    // Preserve the same clean-I source that made the caller omit a B snapshot,
    // including a reference-compatible view requiring an independent transfer,
    // or when an in-place packed conversion declines for capacity/device reasons.
    std::vector<Entry*> resolve_entries;
    if(!guest_snapshot){
        if(independent_source)resolve_entries.push_back(independent_source);
        for(auto& existing:entries_)if(existing.get()!=independent_source&&existing->gpu_owned)
            resolve_entries.push_back(existing.get());
    }
    for(auto* existing:resolve_entries)if(!guest_snapshot&&existing->gpu_owned&&
        (overlaps(d.address,layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes)||
         (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes))||
         (existing->desc.stencil_address&&(overlaps(d.address,layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)||
           (d.stencil_address&&overlaps(d.stencil_address,stencil_layout.guest_bytes,existing->desc.stencil_address,existing->stencil_layout.guest_bytes)))))){
        if(existing->buffer_alias_version){
            if(existing->logical_dirty){
                // A B write invalidates current I without destroying its native
                // allocation history. RefreshImage obtains current B upstream;
                // stale I must neither seed a new view nor download over B.
                // Retire only its ownership, retaining pinned native resources
                // for recorded commands and cached depth recreation.
                if(!retire_independent_)
                    throw std::runtime_error("Dirty independent image has no ownership retirement");
                retire_independent_(*existing);
                if(existing->gpu_owned||existing->buffer_alias_version)
                    throw std::runtime_error("Dirty independent image remains owned after retirement");
                continue;
            }
            if(existing->desc.address!=d.address&&
               overlaps(d.address,layout.guest_bytes,existing->desc.address,existing->layout.guest_bytes)&&
               !(d.address>existing->desc.address?
                 HasPossibleReferenceSubresourceBase(d,layout,existing->desc,existing->layout):
                 HasPossibleReferenceSubresourceBase(existing->desc,existing->layout,d,layout))){
                // ResolveOverlap leaves chance overlaps independent. The new
                // image refreshes from B; a recent/pinned old I stays intact.
                // A possible mip/slice is NOT ignored: its mapping must still
                // be handled/proved by the normal subresource/transfer path.
                continue;
            }
            if(existing->desc.address==d.address&&
               !MatchesReferenceOverlapClass(existing->desc,existing->layout,d,layout))continue;
            const bool private_block_view=MatchesReadonlyBlockMipPrefix(existing->desc,existing->layout,d,layout);
            const bool partial_block=!MatchesReferenceBlockImage(existing->desc,existing->layout,d,layout)&&
                MatchesReferencePartialBlockExpansion(existing->desc,existing->layout,d,layout);
            if((existing->layout.block_extent==4||layout.block_extent==4)&&
               !MatchesReferenceBlockImage(existing->desc,existing->layout,d,layout)&&
               !private_block_view&&!partial_block&&!MatchesGuestUploadStorage(*existing,d,layout)){
                // A different BC codec is a distinct image class. Refresh it
                // from B while retaining the old codec's independent I.
                if(existing->layout.block_extent==4&&layout.block_extent==4&&
                   existing->desc.data_format!=d.data_format)continue;
                throw std::runtime_error("Independent block image view has no proven word/layout transfer");
            }
            const bool depth_transfer=existing->desc.depth_target||d.depth_target;
            const bool color_layer_growth=existing->desc.layers!=d.layers&&
                CanMapIndependentColorGrowth(existing->desc,existing->layout,d,layout);
            const bool color_ms_depth=existing->desc.address==d.address&&
                GuestMultisample::CanReinterpretColorAsMsDepth(existing->desc,existing->layout,d,layout);
            if((depth_transfer||color_layer_growth?!read_independent_image_:(partial_block?!expand_block_:!read_independent_))||
               !acquire_independent_||!retire_independent_||!CanUploadGpuSnapshot(d,layout,d.writable))
                throw std::runtime_error("Independent image view has no domain-preserving GPU transfer");
            if(depth_transfer&&!CanMapIndependentDepthImage(existing->desc,existing->layout,d,layout)){
                const auto& source=existing->desc;
                std::fprintf(stderr,"DX12 depth overlap rejected uid=%llu valid=%u source=%llx/%ux%u/p%u/l%u/m%u/f%u-%u/t%u/d%u/s%u/stencil%llx/bytes%llu target=%llx/%ux%u/p%u/l%u/m%u/f%u-%u/t%u/d%u/s%u/stencil%llx/bytes%llu\n",
                    (unsigned long long)existing->image_uid,unsigned(existing->HasValidLogicalImage()),
                    (unsigned long long)source.address,source.width,source.height,source.pitch,source.layers,source.levels,
                    source.data_format,source.number_format,source.tile_mode,unsigned(source.depth_target),source.samples,
                    (unsigned long long)source.stencil_address,(unsigned long long)existing->layout.guest_bytes,
                    (unsigned long long)d.address,d.width,d.height,d.pitch,d.layers,d.levels,
                    d.data_format,d.number_format,d.tile_mode,unsigned(d.depth_target),d.samples,
                    (unsigned long long)d.stencil_address,(unsigned long long)layout.guest_bytes);
                std::fflush(stderr);
                throw std::runtime_error("Independent depth/color overlap needs an exact physical transfer plan");
            }
            const auto samples_supported=[](const auto& desc,const auto& physical){
                return IsGuestDepthPlaneStorage(desc,physical)||CanTransferIndependentMultisample(desc,physical);};
            if(!color_ms_depth&&(existing->desc.samples>1||d.samples>1)&&
               (existing->desc.samples!=d.samples||
                !samples_supported(existing->desc,existing->layout)||!samples_supported(d,layout)))
                throw std::runtime_error("Independent MSAA image transfer needs matching raw-component sample domains");
            auto* source=existing;auto pin=PinBinding(*source);
            auto residency=source->pending_buffer_residency;
            Resource snapshot,stencil_snapshot;
            if(depth_transfer||color_layer_growth){
                auto captured=read_independent_image_(*source,d,layout);
                snapshot=std::move(captured.depth);stencil_snapshot=std::move(captured.stencil);
                residency.insert(residency.end(),captured.seed_residency.begin(),captured.seed_residency.end());
                if(bool(stencil_snapshot.handle)!=bool(d.stencil_address))
                    throw std::runtime_error("Independent depth conversion lost its stencil input");
            }else if(partial_block){
                auto expanded=expand_block_(*source,d,layout);
                snapshot=std::move(expanded.snapshot);
                residency.insert(residency.end(),expanded.residency.begin(),expanded.residency.end());
            }else snapshot=read_independent_(*source,d.address,layout.guest_bytes);
            // Source ownership and its CPU-write guard remain live until the
            // new interpretation has acquired its own guard. Pending copies
            // retain the old allocation/descriptors through normal cache fences.
            // First attach the complete dependency chain, then publish only at
            // the actual bind. Recursive upload cannot consume it prematurely.
            auto& converted=UploadVersion(d,{},recording(),completed,{},true,&snapshot,d.writable,false,nullptr,
                UploadPurpose::Preparation,stencil_snapshot.handle?&stencil_snapshot:nullptr,d.depth_target);
            converted.pending_buffer_residency=std::move(residency);
            if(color_ms_depth){
                // Pinned CopyColorAndDepth/ReinterpretColorAsMsDepth samples
                // typed source channels into target layer zero. All other
                // initialized layers/S stay intact. Register both participants
                // before recording so internal submission retains allocations.
                if(!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
                auto lease=multisample_->PrepareColorAsMsDepth(source->texture,converted.texture,
                    source->desc,source->layout,converted.desc,converted.layout);
                converted.color_depth_leases.push_back(lease);
                source->pending=converted.pending=true;
                if(transfer_submission_)transfer_submission_();
                recording_entry=&converted;
                lease->Record(recording(),source->texture,converted.texture);
            }
            if(private_block_view){
                // Do not publish a second mutable owner or retire the BC parent:
                // the selected mip is only a binding snapshot of its private I.
                // Keeping it dirty also prevents RevalidateReadonly from acquiring
                // ownership and prevents reuse after the parent changes. Upload's
                // pending resources and the binding pin retain it through fences.
                converted.MarkLogicalDirty();
                if(purpose==UploadPurpose::Binding)PublishBufferResidency(converted);
                Diagnostics::Count("image-version.private-block-mip-view");
                return converted;
            }
            if(!(acquire_independent_replacement_?acquire_independent_replacement_(converted,*source):acquire_independent_(converted)))
                throw std::runtime_error("Converted independent image could not retain ownership");
            if(d.depth_target)converted.htile_initial_mask=0;
            retire_independent_(*source);
            // The retained object now serves queued descriptors/fences, not a
            // live logical I version after the original image was replaced.
            source->MarkLogicalDirty();
            if(depth_transfer&&(d.depth_target||d.writable||d.render_target||
               std::pair(d.levels,d.layers)>std::pair(source->desc.levels,source->desc.layers)))
                ForgetRecreatedSourceMeta(*source);
            Diagnostics::Count("image-version.gpu-reinterpret");
            return finish_refresh(converted);
        }
        if(!resolve_owned_)throw std::runtime_error("GPU-owned upload backing has no resolver");
        if(Diagnostics::Enabled()){
            static std::atomic<unsigned> retirement_pairs{};
            if(retirement_pairs.fetch_add(1)<32){
                const auto& source=existing->desc;
                fprintf(stderr,"DX12 retirement pair source=%llx/%ux%u/fmt%u-%u/tile%u/levels%u/layers%u/rt%u/depth%u/write%u target=%llx/%ux%u/fmt%u-%u/tile%u/levels%u/layers%u/rt%u/depth%u/write%u storage_match=%u logical_dirty=%u pending=%u dirty_origin=%s\n",
                    (unsigned long long)source.address,source.width,source.height,source.data_format,source.number_format,source.tile_mode,source.levels,source.layers,unsigned(source.render_target),unsigned(source.depth_target),unsigned(source.writable),
                    (unsigned long long)d.address,d.width,d.height,d.data_format,d.number_format,d.tile_mode,d.levels,d.layers,unsigned(d.render_target),unsigned(d.depth_target),unsigned(d.writable),
                    unsigned(MatchesGuestUploadStorage(*existing,d,layout)),unsigned(existing->logical_dirty),unsigned(existing->pending),existing->logical_dirty_origin);
                fflush(stderr);
            }
        }
        resolve_owned_(*existing);
        if(existing->gpu_owned)throw std::runtime_error("Upload backing remains GPU-owned after resolution");
        // The resolver retires the image and reopens the recording list. Its
        // submission may be newer than the fence supplied by the caller.
        completed=std::max(completed,existing->fence);
    }
    // A depth/color attachment can be sampled by a different T# view. Reusing
    // its native texture avoids a 64MiB CPU detile/upload for 4096² shadows.
    // Compare the complete backing, including padding: CPU changes fall back to
    // the original checked upload path. No guest readback or fence is skipped.
    if(!gpu_snapshot_any&&!d.writable&&!d.render_target&&!d.depth_target&&!d.stencil_address){
        static unsigned large_traces{};
        if(Diagnostics::Enabled()&&d.width>=4096&&large_traces++<8){
            auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed){auto* existing=indexed->second;
                const auto& s=existing->desc;
                fprintf(stderr,"DX12 sampled candidate address=%llx requested=%ux%u fmt=%u/%u tile=%u pow2=%u existing=%ux%u fmt=%u/%u tile=%u depth=%u stencil=%llx pow2=%u bytes=%llu snapshot=%zu pending=%u\n",
                    (unsigned long long)d.address,d.width,d.height,d.data_format,d.number_format,d.tile_mode,unsigned(d.pow2_padding),
                    s.width,s.height,s.data_format,s.number_format,s.tile_mode,unsigned(s.depth_target),(unsigned long long)s.stencil_address,
                    unsigned(s.pow2_padding),(unsigned long long)existing->layout.guest_bytes,existing->sampled_snapshot.size(),unsigned(existing->pending));
            }
        }
        auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed){auto* existing=indexed->second;
            const bool immutable_bc=!existing->gpu_owned&&!existing->logical_dirty&&
                IsExpandedReadonlyBcRgba8(d,layout)&&CanBindReadonlyBcView(*existing,d,layout);
            // An identical immutable upload may be read by another typed view
            // while pending: no staging or texture bytes are overwritten, and
            // its original copy precedes both reads on the same scheduler.
            if(((existing->pending||existing->fence>completed)&&!immutable_bc)||
               (!CompatibleGuestSampledStorage(existing->desc,d)&&!CanBindReadonlyBcView(*existing,d,layout))||
               existing->layout.guest_bytes!=layout.guest_bytes||
               existing->sampled_snapshot.size()!=layout.guest_bytes)continue;
            bool same_layout=existing->layout.mips.size()==layout.mips.size();
            for(size_t i=0;same_layout&&i<layout.mips.size();++i){
                const auto& a=existing->layout.mips[i];const auto& b=layout.mips[i];
                same_layout=a.pitch==b.pitch&&a.height==b.height&&a.offset==b.offset&&
                    a.slice_bytes==b.slice_bytes&&a.macro==b.macro&&a.thickness==b.thickness;
            }
            if(!same_layout||memcmp(existing->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes)))continue;
            diagnostic.Stage("coherent-native-view-reuse");
            static unsigned reuse_traces{};
            if(Diagnostics::Enabled()&&reuse_traces++<16){
                fprintf(stderr,"DX12 coherent sampled reuse address=%llx size=%ux%u depth=%u color=%u bytes=%llu\n",
                    (unsigned long long)d.address,d.width,d.height,unsigned(existing->desc.depth_target),
                    unsigned(existing->desc.render_target),(unsigned long long)layout.guest_bytes);
                fflush(stderr);
            }
            existing->last_use=++use_sequence_;existing->pending=true;
            recording_entry=existing;
            existing->texture.Transition(recording(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            return finish_refresh(*existing);
        }
    }
    // A dependent block child belongs to exactly one parent transaction. An
    // older unowned/unpinned child may have compatible dimensions yet contains
    // a previous parent's snapshot and cannot be rebound to this lease.
    Entry* entry=nullptr;if(!fresh_depth_recreation&&!dependent){auto [first,last]=address_entries();for(auto indexed=first;indexed!=last;++indexed)
        if(!(guest_snapshot&&(indexed->second->gpu_owned||indexed->second->BindingPinned()||
             (!current_buffer_snapshot&&indexed->second->cached_native_current)))&&
           (indexed->second->desc==d||CompatibleGuestAttachmentStorage(indexed->second->desc,d)||
            CompatibleGuestWritableStorage(indexed->second->desc,d))){entry=indexed->second;break;}}
    const bool immutable_view=!d.writable;
    if(entry&&immutable_view&&!gpu_snapshot_any&&
       ((!entry->pending&&entry->fence<=completed)||(!d.render_target&&!d.depth_target))&&
       entry->sampled_snapshot.size()==layout.guest_bytes&&
       !memcmp(entry->sampled_snapshot.data(),bytes.data(),size_t(layout.guest_bytes))&&
        (!d.stencil_address||(retained_stencil.handle?
            entry->texture.handle.Get()==retained_stencil.handle.Get():
            (entry->stencil_snapshot.size()==stencil_layout.guest_bytes&&
             !memcmp(entry->stencil_snapshot.data(),stencil_bytes.data(),size_t(stencil_layout.guest_bytes)))))){
        entry->last_use=++use_sequence_;entry->pending=true;
        recording_entry=entry;
        entry->texture.Transition(recording(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return finish_refresh(*entry);
    }
    if(entry&&(entry->pending||entry->fence>completed)) {
        // Changed CPU data must not overwrite staging used by queued copies.
        // The caller retires the packet and reopens this command-list object.
        if(retire_pending_) {retire_pending_();completed=std::max(completed,entry->fence);}
        if(entry->pending||entry->fence>completed)
            throw std::runtime_error("guest texture upload still in flight");
    }
    const bool gpu_detile=gpu_snapshot_any||HasExactPackedExpansion(d,layout)||
        (!layout.expanded_bytes_per_pixel&&(d.data_format==13||(d.data_format>=16&&d.data_format<=19)||d.samples>1||(d.tile_mode!=8&&d.tile_mode!=31)||
        (d.depth_target&&d.stencil_address&&layout.bytes_per_pixel==2)));
    const Resource* snapshots[]{gpu_snapshot,stencil_gpu_snapshot};
    if(entry)for(size_t plane=0;plane<std::size(snapshots);++plane)if(const auto* snapshot=snapshots[plane]){
        const auto previous=plane<entry->tiler_input_pool.size()?entry->tiler_input_pool[plane].handle->GetDesc().Width:0;
        const auto incoming=snapshot->handle->GetDesc().Width;
        if(incoming>previous)ReserveMemory(incoming-previous,completed,entry);
    }
    if(layout.expanded_bytes_per_pixel&&d.data_format>=35&&!bc_decoder_)bc_decoder_=std::make_unique<GuestBcDecoder>(converter_);
    if(gpu_detile&&!tiler_)tiler_=std::make_shared<GuestTiler>(device_.Get());
    if(d.samples>1&&!multisample_)multisample_=std::make_shared<GuestMultisample>(device_.Get());
    auto cast_formats=PackedFloatCasts(layout.format,texture_allocator_.SupportsFormatCasting());
    const auto word_casts=d.writable?WritableWordCasts(d,texture_allocator_.SupportsFormatCasting(),
        writable_formats_.contains(DXGI_FORMAT_R11G11B10_FLOAT)):
        ReadonlyWordCasts(d,texture_allocator_.SupportsFormatCasting());
    if(!word_casts.empty())cast_formats=word_casts;
    const auto ordered_color_casts=OrderedColorFormats(d,texture_allocator_.SupportsFormatCasting());
    if(!ordered_color_casts.empty()&&d.samples==1)cast_formats=ordered_color_casts;
    const auto storage_cast_formats=!ordered_color_casts.empty()?ordered_color_casts:cast_formats;
    if(d.samples>1&&(d.data_format==6||d.data_format==34)&&!d.UsesRaw32()&&cast_formats.empty())
        throw std::runtime_error("Native packed-float MSAA requires bit-preserving D3D12 format casting");
    bool new_resource=false;
    if(!entry){
        diagnostic.Stage("resource-allocation");
        if(!next_image_uid_)throw std::overflow_error("Native image identity exhausted");
        auto new_entry=std::make_unique<Entry>();new_entry->desc=d;new_entry->layout=layout;new_entry->cache=this;
        new_entry->image_uid=next_image_uid_++;
        if(d.depth_target&&std::ranges::any_of(entries_,[&](const auto& prior){return
            prior->desc.address==d.address&&!prior->desc.depth_target&&
            prior->layout.bytes_per_pixel==layout.bytes_per_pixel&&
            prior->layout.guest_bytes==layout.guest_bytes&&prior->desc.samples==d.samples;}))
            new_entry->htile_initial_mask=0;
        if(d.stencil_address)new_entry->stencil_layout=stencil_layout;
        D3D12_RESOURCE_DESC r{};r.Dimension=d.volume_depth?D3D12_RESOURCE_DIMENSION_TEXTURE3D:d.one_dimensional?D3D12_RESOURCE_DIMENSION_TEXTURE1D:D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=d.width;r.Height=d.height;r.DepthOrArraySize=UINT16(d.volume_depth?d.volume_depth:d.layers);r.MipLevels=UINT16(d.levels);r.Format=layout.format;r.SampleDesc.Count=d.samples;
        if(layout.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)r.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        if(IsExpandedReadonlyBcRgba8(d,layout))r.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
        if(!d.depth_target&&!layout.expanded_bytes_per_pixel)r.Format=GuestStorageFormat(d.data_format,layout.format);
        if(HasExactPackedExpansion(d,layout))r.Format=DXGI_FORMAT_R32G32B32A32_TYPELESS;
        if(d.UsesRaw32())r.Format=DXGI_FORMAT_R32_TYPELESS;
        if(!word_casts.empty())r.Format=layout.format;
        if(d.depth_target) {
            if(d.writable||d.render_target||(d.samples>1&&d.levels!=1)||
               (layout.format!=DXGI_FORMAT_R32_FLOAT&&layout.format!=DXGI_FORMAT_R16_UNORM))
                throw std::invalid_argument("Invalid guest depth resource usage/format");
            r.Format=d.stencil_address?DXGI_FORMAT_R32G8X24_TYPELESS:
                layout.bytes_per_pixel==4?DXGI_FORMAT_R32_TYPELESS:DXGI_FORMAT_R16_TYPELESS;
            r.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if(d.writable) {
            r.Format=d.UsesRaw32()?
                DXGI_FORMAT_R32_TYPELESS:GuestStorageFormat(d.data_format,layout.format);
            if(d.samples==1)r.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if(!word_casts.empty())r.Format=layout.format;
        // Pinned Vulkan non-block images always retain storage usage. Physical
        // mutable-word capability is independent of the original readonly
        // shader descriptor and does not publish writes, residency or ownership.
        if(!d.writable&&!word_casts.empty()){
            r.Format=DXGI_FORMAT_R32_UINT;
            r.Flags|=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        const bool color_attachment_capable=!d.depth_target&&
            !d.one_dimensional&&!d.volume_depth&&layout.block_extent==1;
        if(d.render_target||(d.samples>1&&!d.depth_target)||color_attachment_capable) {
            D3D12_FEATURE_DATA_FORMAT_SUPPORT support{layout.format};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
            if(!(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET)&&(d.render_target||d.samples>1))
                throw std::runtime_error("Native guest color format lacks render-target support");
            if(d.samples>1&&(d.data_format==6||d.data_format==34)&&
               !(support.Support1&D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD))
                throw std::runtime_error("Native packed-float format lacks typed multisample loads");
            // Pinned color images retain attachment usage independently of
            // their first readonly/storage T#. A future MSAA->1x blit must be
            // able to restore THIS original allocation, without recreating I.
            if(support.Support1&D3D12_FORMAT_SUPPORT1_RENDER_TARGET)r.Flags|=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if((!ordered_color_casts.empty()||(IsOrderedColorClass(d)&&(IsOrderedScalarClass(d)||IsOrderedFloatWordClass(d)||
            (IsOrderedIntegerClass(d)&&(d.data_format==4||d.data_format==11||d.data_format==14)))))&&d.samples==1)
            r.Flags|=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if(d.samples>1){
            const auto typed=d.depth_target?(d.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
                layout.bytes_per_pixel==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT):layout.format;
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{typed,d.samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
            if(!quality.NumQualityLevels)throw std::runtime_error("Native adapter does not support the guest attachment sample count");
        }
        // Account for persistent upload/readback storage as well as texture
        // backing and CPU snapshots; pools must not silently bypass the bound.
        uint64_t transfer_bytes{};
        for(UINT sub=0;sub<d.layers*d.levels*(d.stencil_address?2u:1u);++sub){
            UINT64 bytes{};
            if(d.samples>1){const auto& physical=sub>=d.layers?stencil_layout:layout;
                bytes=GuestMultisample::LinearBytes(d,GuestMultisample::Footprint(d,physical));}
            else DX12_API_CALL(device_->GetCopyableFootprints(&r,sub,1,0,nullptr,nullptr,nullptr,&bytes));
            transfer_bytes+=bytes;
        }
        const auto allocation=texture_allocator_.AllocationInfo(r,cast_formats);
        if(allocation.SizeInBytes==UINT64_MAX)throw std::runtime_error("Native guest image allocation information is invalid");
        new_entry->allocation_bytes=allocation.SizeInBytes;
        auto storage_description=r;storage_description.SampleDesc={1,0};storage_description.Alignment=0;
        storage_description.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        uint64_t storage_linear_bytes{};
        const bool multisample_storage=d.samples>1&&(d.writable||
            (IsOrderedMultisampleColorClass(d)&&!IsOrderedIntegerClass(d)&&!IsOrderedScalarClass(d)&&!IsOrderedFloatWordClass(d)&&!IsOrderedPacked16Class(d)&&!IsOrderedSrgbClass(d)&&d.data_format!=3&&d.data_format!=5&&d.data_format!=6&&d.data_format!=12));
        if(multisample_storage){
            const auto storage_allocation=texture_allocator_.AllocationInfo(storage_description,storage_cast_formats);
            storage_linear_bytes=GuestMultisample::LinearBytes(d,GuestMultisample::Footprint(d,layout));
            D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width=(storage_linear_bytes+3)&~uint64_t(3);buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
            buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            const auto linear_allocation=device_->GetResourceAllocationInfo(0,1,&buffer);
            if(!storage_allocation.SizeInBytes||storage_allocation.SizeInBytes==UINT64_MAX||
               !linear_allocation.SizeInBytes||linear_allocation.SizeInBytes==UINT64_MAX||
               new_entry->allocation_bytes>UINT64_MAX-linear_allocation.SizeInBytes||
               storage_allocation.SizeInBytes>(UINT64_MAX-new_entry->allocation_bytes-linear_allocation.SizeInBytes)/d.samples)
                throw std::runtime_error("Invalid native MSAA shader storage allocation size");
            new_entry->allocation_bytes+=storage_allocation.SizeInBytes*d.samples+linear_allocation.SizeInBytes;
        }
        const uint64_t physical_bytes=(gpu_snapshot?gpu_snapshot->handle->GetDesc().Width:((layout.guest_bytes+3)&~uint64_t(3)))+
            (d.stencil_address?(stencil_gpu_snapshot?stencil_gpu_snapshot->handle->GetDesc().Width:
                ((stencil_layout.guest_bytes+3)&~uint64_t(3))):0);
        const uint64_t masks=GuestTiler::WrittenBytes(layout.guest_bytes)+
            (d.stencil_address?GuestTiler::WrittenBytes(stencil_layout.guest_bytes):0);
        if(d.writable||d.render_target||d.depth_target)
            new_entry->readback_reserve_bytes=transfer_bytes+(gpu_detile?physical_bytes*2+masks*2:0);
        const uint64_t required=new_entry->allocation_bytes+expanded_snapshot_bytes+
            (immutable_view&&!gpu_snapshot_any?layout.guest_bytes+(d.stencil_address?stencil_layout.guest_bytes:0):0)+
            transfer_bytes+(gpu_detile?physical_bytes:0)+new_entry->readback_reserve_bytes;
        ReserveMemory(required,completed);
        // Copy queues require COMMON on entry and decay back to COMMON at
        // submission completion. Depth/stencil cannot use implicit promotion,
        // so keep its upload and explicit barriers on the direct list.
        new_entry->texture=AllocateTexture(new_entry.get(),r,submit_copy_&&!d.depth_target&&!gpu_detile?
            D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_COPY_DEST,nullptr,cast_formats);
        if(storage_linear_bytes){
            new_entry->storage_samples.reserve(d.samples);
            for(UINT sample=0;sample<d.samples;++sample)
                new_entry->storage_samples.push_back(AllocateTexture(new_entry.get(),storage_description,D3D12_RESOURCE_STATE_COMMON,nullptr,storage_cast_formats));
            new_entry->storage_linear=GuestTiler::Scratch(device_.Get(),storage_linear_bytes);
        }
        entry=new_entry.get();entries_.push_back(std::move(new_entry));Index(*entry);new_resource=true;}
    // No failure after backing mutation may leave an old valid-I witness.
    entry->MarkLogicalDirty();
    recording_entry=entry;
    SetBackingSamples(*entry,d.samples,recording(),false);
    if(preserve_expanded){
        // An immutable GPU snapshot owns exact packed bits independently of the
        // expanded texture. Allocate the complete retained pair before replacing
        // old completed backing or recording any new upload commands.
        ReserveMemory(expanded_snapshot_bytes,completed,entry);
        auto original=GuestTiler::Scratch(device_.Get(),layout.guest_bytes);
        auto written=GuestTiler::Scratch(device_.Get(),GuestTiler::WrittenBytes(layout.guest_bytes));
        entry->expanded_guest_backing=std::move(original);entry->expanded_written_mask=std::move(written);
        entry->expanded_backing_bytes=expanded_snapshot_bytes;
    }else{entry->expanded_guest_backing={};entry->expanded_written_mask={};entry->expanded_backing_bytes=0;}
    const auto deferred_size=[](uint64_t size){return size>=64*1024&&size<=64*1024*1024;};
    const bool prepare_raw_snapshot=immutable_view&&!gpu_snapshot_any&&!retained_stencil.handle&&
        gpu_detile&&host_buffer_copy_&&(deferred_size(layout.guest_bytes)||
            (d.stencil_address&&deferred_size(stencil_layout.guest_bytes)));
    CpuBufferSnapshot raw_snapshot,raw_stencil_snapshot;
    if(prepare_raw_snapshot){
        // This is the existing coherence snapshot's one source copy, moved
        // before staging. Publish it into Entry only at the old success point.
        raw_snapshot=std::move(entry->sampled_snapshot);
        raw_stencil_snapshot=std::move(entry->stencil_snapshot);
        raw_snapshot.Assign(bytes.first(size_t(layout.guest_bytes)));
        if(d.stencil_address)raw_stencil_snapshot.Assign(stencil_bytes.first(size_t(stencil_layout.guest_bytes)));
    }
    entry->last_use=++use_sequence_;entry->sampled_snapshot.clear();entry->stencil_snapshot.clear();
    entry->pending_buffer_residency.clear();
    const bool copy_upload=new_resource&&submit_copy_&&!d.depth_target&&!gpu_detile;
    entry->pending=true;
    if(!copy_upload)entry->texture.Transition(recording(),D3D12_RESOURCE_STATE_COPY_DEST);
    auto resource_desc=DX12_API_CALL(entry->texture.handle->GetDesc());
    struct PendingTextureCopy {D3D12_TEXTURE_COPY_LOCATION destination,source;};
    std::vector<PendingTextureCopy> copy_batch;
    if(gpu_detile){
        diagnostic.Stage("gpu-detile-upload");
        entry->tiler=tiler_;entry->tiler_leases.clear();
        entry->multisample=multisample_;entry->multisample_leases.clear();
        const auto planes=d.stencil_address&&!retained_stencil.handle?2u:1u;
        entry->tiler_leases.reserve(size_t(planes)*d.layers*d.levels*(preserve_expanded?3u:1u));
        entry->tiler_input_pool.reserve(planes);
        entry->tiler_output_pool.reserve(size_t(planes)*d.layers*d.levels);
        entry->multisample_leases.reserve(size_t(planes)*d.layers);
        for(uint32_t plane=0;plane<planes;++plane){
            const auto& physical=plane?stencil_layout:layout;
            const auto backing=plane?stencil_bytes:bytes;
            if(physical.guest_bytes>UINT32_MAX-3)throw std::out_of_range("Native tiling upload exceeds guest buffer addressing");
            const auto size=(physical.guest_bytes+3)&~uint64_t(3);
            if(const auto* snapshot=snapshots[plane]){
                if(plane==entry->tiler_input_pool.size())entry->tiler_input_pool.push_back(*snapshot);
                else entry->tiler_input_pool.at(plane)=*snapshot;
                continue;
            }
            if(plane<entry->tiler_input_pool.size()){
                D3D12_HEAP_PROPERTIES heap{};D3D12_HEAP_FLAGS flags{};
                Check(entry->tiler_input_pool[plane].handle->GetHeapProperties(&heap,&flags));
                if(heap.Type!=D3D12_HEAP_TYPE_UPLOAD)entry->tiler_input_pool[plane]=
                    Buffer(device_.Get(),size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            }
            if(plane==entry->tiler_input_pool.size())entry->tiler_input_pool.push_back(
                Buffer(device_.Get(),size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            auto& raw=entry->tiler_input_pool.at(plane);
            if(raw.handle->GetDesc().Width<size)throw std::logic_error("Native raw tiling upload pool mismatch");
            void* mapped{};D3D12_RANGE empty{};Check(DX12_API_CALL(raw.handle->Map(0,&empty,&mapped)));
            if(size>physical.guest_bytes)memset(static_cast<std::byte*>(mapped)+physical.guest_bytes,0,size_t(size-physical.guest_bytes));
            bool deferred{};const auto mapped_resource=raw.handle;
            SCOPE_EXIT {if(!deferred)DX12_API_CALL(mapped_resource->Unmap(0,nullptr));};
            const auto& snapshot=plane?raw_stencil_snapshot:raw_snapshot;
            if(prepare_raw_snapshot&&deferred_size(physical.guest_bytes)){
                host_buffer_copy_(raw.handle.Get(),mapped,snapshot.Share(),size_t(physical.guest_bytes));
                deferred=true;
            }else memcpy(mapped,prepare_raw_snapshot?snapshot.data():backing.data(),size_t(physical.guest_bytes));
        }
        size_t output_index{};
        for(uint32_t plane=0;plane<planes;++plane)
        for(uint32_t layer=0;layer<d.layers;++layer)for(uint32_t mip=0;mip<d.levels;++mip){
            const auto& physical=plane?stencil_layout:layout;
            const auto& interpreted=plane?stencil_desc:d;
            const UINT sub=mip+layer*d.levels+plane*d.levels*d.layers;
            UINT rows{};UINT64 row_bytes{},total{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
            if(d.samples>1){fp=GuestMultisample::Footprint(interpreted,physical);
                rows=d.height;row_bytes=uint64_t(d.width)*physical.HostBytesPerPixel();
                total=GuestMultisample::LinearBytes(interpreted,fp);}
            else DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,sub,1,0,&fp,&rows,&row_bytes,&total));
            const auto width=(std::max(d.width>>mip,1u)+physical.block_extent-1)/physical.block_extent;
            if(!width||row_bytes%width)throw std::logic_error("Native tiling format footprint mismatch");
            if(output_index==entry->tiler_output_pool.size())entry->tiler_output_pool.push_back(GuestTiler::Scratch(device_.Get(),total));
            auto& linear=entry->tiler_output_pool.at(output_index++);
            if(linear.handle->GetDesc().Width<total)throw std::logic_error("Native linear tiling pool mismatch");
            auto tiling=tiler_->Prepare(entry->tiler_input_pool.at(plane),linear,linear,
                interpreted,physical,mip,d.volume_depth?0:layer,fp,uint32_t(row_bytes/width),false);
            std::shared_ptr<GuestTiler::Lease> exact_detile,exact_retile;
            if(preserve_expanded){
                // Reuse this slice's existing scratch before expanding it. The
                // raw view copies original words and marks only logical texels;
                // SNORM encodings, packed19 nibbles and padding stay exact.
                auto raw_desc=d;raw_desc.writable=true;raw_desc.render_target=false;
                raw_desc.native_packed10_raw=d.data_format==9;
                const auto raw_layout=LayoutGuestTexture(raw_desc);
                if(raw_layout.guest_bytes!=layout.guest_bytes||
                   raw_layout.bytes_per_pixel!=layout.bytes_per_pixel||
                   raw_layout.format!=DXGI_FORMAT_R32_UINT||raw_layout.HostBytesPerPixel()!=4)
                    throw std::logic_error("Expanded snapshot changed its physical word layout");
                auto raw_fp=fp;raw_fp.Footprint.Format=DXGI_FORMAT_R32_UINT;
                exact_detile=tiler_->Prepare(entry->tiler_input_pool.at(plane),linear,linear,
                    raw_desc,raw_layout,mip,layer,raw_fp,4,false);
                exact_retile=tiler_->Prepare(linear,entry->expanded_guest_backing,entry->expanded_written_mask,
                    raw_desc,raw_layout,mip,layer,raw_fp,4,true,plane==0&&layer==0&&mip==0);
                entry->tiler_leases.push_back(exact_detile);entry->tiler_leases.push_back(exact_retile);
            }
            entry->tiler_leases.push_back(tiling);
            if(d.samples>1){
                auto transfer=multisample_->Prepare(entry->texture,linear,d,layout,layer,plane,true);
                entry->multisample_leases.push_back(transfer);
                if(exact_detile){
                    exact_detile->Record(recording(),entry->tiler_input_pool.at(plane),linear,linear);
                    exact_retile->Record(recording(),linear,entry->expanded_guest_backing,entry->expanded_written_mask);
                }
                tiling->Record(recording(),entry->tiler_input_pool.at(plane),linear,linear);
                transfer->Record(recording(),entry->texture,linear);continue;
            }
            tiling->Record(recording(),entry->tiler_input_pool.at(plane),linear,linear);
            linear.Transition(recording(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=linear.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;
            D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=entry->texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;
            DX12_API_CALL(recording()->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
        }
    }else{
    size_t upload_index{};
    const auto expand_sampled=[&](std::vector<std::byte> linear,uint32_t mip){
        if(!layout.expanded_bytes_per_pixel)return linear;
        if(d.data_format==9)return ExpandSignedPacked10(linear,d.number_format);
        return bc_decoder_->Expand(d.data_format,d.number_format,
            std::max(d.width>>mip,1u),std::max(d.height>>mip,1u),linear);
    };
    for(uint32_t plane=0;plane<(d.stencil_address&&!retained_stencil.handle?2u:1u);++plane)
    for(uint32_t layer=0;layer<d.layers;++layer)for(uint32_t mip=0;mip<d.levels;++mip){
        diagnostic.Stage("cpu-detile");
        auto linear=plane?DecodeGuestSubresource(stencil_desc,entry->stencil_layout,stencil_bytes,mip,layer):DecodeGuestSubresource(d,entry->layout,bytes,mip,layer);
        linear=expand_sampled(std::move(linear),mip);
        if(!plane&&d.stencil_address&&layout.bytes_per_pixel==2){
            std::vector<std::byte> converted(linear.size()*2);
            for(size_t i=0;i<linear.size()/2;++i){uint16_t v;memcpy(&v,linear.data()+i*2,2);float f=float(v)/65535.f;memcpy(converted.data()+i*4,&f,4);}
            linear=std::move(converted);
        }
        diagnostic.Stage("upload-allocation-and-copy");
        UINT sub=mip+layer*d.levels+plane*d.levels*d.layers,rows;UINT64 row_bytes,total;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,sub,1,0,&fp,&rows,&row_bytes,&total));
        if(upload_index==entry->upload_pool.size())entry->upload_pool.push_back(
            Buffer(device_.Get(),total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
        auto upload=entry->upload_pool.at(upload_index++);
        if(DX12_API_CALL(upload.handle->GetDesc()).Width<total)throw std::logic_error("Native upload pool footprint mismatch");
        void* mapped=nullptr;D3D12_RANGE empty{0,0};Check(DX12_API_CALL(upload.handle->Map(0,&empty,&mapped)));
        bool deferred{};
        const auto mapped_resource=upload.handle;
        SCOPE_EXIT {if(!deferred)DX12_API_CALL(mapped_resource->Unmap(0,nullptr));};
        std::shared_ptr<std::vector<std::vector<std::byte>>> slices;
        if(host_texture_copy_&&total>=64*1024&&total<=64*1024*1024){
            slices=std::make_shared<std::vector<std::vector<std::byte>>>();
            slices->reserve(fp.Footprint.Depth);}
        for(UINT z=0;z<fp.Footprint.Depth;++z){
            if(d.volume_depth&&z){
                linear=DecodeGuestSubresource(d,layout,bytes,mip,z);
                linear=expand_sampled(std::move(linear),mip);
            }
            if(slices)slices->push_back(std::move(linear));
            else for(UINT row=0;row<rows;++row)memcpy(static_cast<std::byte*>(mapped)+fp.Offset+
                (UINT64(z)*rows+row)*fp.Footprint.RowPitch,linear.data()+UINT64(row)*row_bytes,size_t(row_bytes));
        }
        if(slices){host_texture_copy_(upload.handle.Get(),mapped,std::move(slices),fp,rows,row_bytes);deferred=true;}
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=entry->texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=sub;
        if(copy_upload)copy_batch.push_back({dst,src});
        else DX12_API_CALL(recording()->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
        entry->uploads.push_back(std::move(upload));}
    }
    if(!copy_batch.empty())submit_copy_([copies=std::move(copy_batch)](ID3D12GraphicsCommandList* copy_list){
        for(const auto& copy:copies)DX12_API_CALL(copy_list->CopyTextureRegion(
            &copy.destination,0,0,0,&copy.source,nullptr));
    });
    if(retained_stencil.handle&&retained_stencil.handle.Get()!=entry->texture.handle.Get()){
        const auto from=retained_stencil.handle->GetDesc(),to=entry->texture.handle->GetDesc();
        if(from.Dimension!=to.Dimension||from.Width!=to.Width||from.Height!=to.Height||
           from.DepthOrArraySize!=to.DepthOrArraySize||from.MipLevels!=to.MipLevels||
           from.Format!=to.Format||from.SampleDesc.Count!=1||to.SampleDesc.Count!=1)
            throw std::logic_error("Depth refresh lost its native stencil allocation geometry");
        // A bound old allocation can outlive the B refresh. Initialize the
        // replacement S from that native I, not unrelated guest B stencil bytes.
        entry->native_plane_sources.push_back({retained_stencil,retained_stencil_pin});
        stencil_parent->pending=entry->pending=true;if(transfer_submission_)transfer_submission_();
        const auto old_state=retained_stencil.state;
        retained_stencil.Transition(recording(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        entry->texture.Transition(recording(),D3D12_RESOURCE_STATE_COPY_DEST);
        const UINT plane_base=UINT(d.levels)*d.layers;
        for(UINT sub=0;sub<plane_base;++sub){
            D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=retained_stencil.handle.Get();
            source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=plane_base+sub;
            D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=entry->texture.handle.Get();
            destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;destination.SubresourceIndex=plane_base+sub;
            DX12_API_CALL(recording()->CopyTextureRegion(&destination,0,0,0,&source,nullptr));
        }
        // Resource is a retained value copy. Restore the old Entry's tracked
        // state so later bound-view refreshes cannot emit a stale barrier.
        retained_stencil.Transition(recording(),old_state);
    }
    diagnostic.Stage("cpu-coherence-snapshot");
    if(prepare_raw_snapshot){
        entry->sampled_snapshot=std::move(raw_snapshot);
        if(d.stencil_address)entry->stencil_snapshot=std::move(raw_stencil_snapshot);
    }else if(immutable_view&&!gpu_snapshot_any&&!retained_stencil.handle)entry->RememberSnapshot(bytes,stencil_bytes);
    if(immutable_view&&!guest_snapshot&&!d.render_target&&!d.depth_target&&!d.stencil_address&&watch_immutable_)watch_immutable_(*entry);
    entry->texture.Transition(recording(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);entry->pending=true;return finish_refresh(*entry);
}
void GuestTextureCache::Submitted(uint64_t fence){if(!fence)throw std::invalid_argument("zero texture fence");for(auto& e:entries_)if(e->pending){e->pending=false;e->fence=fence;}}
void GuestTextureCache::Used(Entry& entry,uint64_t fence){if(!fence||entry.pending||fence<entry.fence)throw std::invalid_argument("invalid texture use fence");entry.fence=fence;}
Resource GuestTextureCache::RetileBufferVersion(Entry& image,Resource& initial,
    ID3D12GraphicsCommandList* list,uint64_t completed){
    if(image.desc.depth_target){
        const auto owner=DescribeGuestImageOwnership(image);
        if(!image.gpu_owned||!image.buffer_alias_version||!owner||!CanRetainGuestImageOwnership(*this,*owner))
            throw std::invalid_argument("Formatted depth transfer has no complete independent owner");
        // Native D16/S8 needs a float32 -> guest16 transfer. The formatted
        // contract remains depth-only; private plane capture does not publish B.
        return CaptureDepthPlane(image,0,initial,list,completed);
    }
    return RetileImageVersion(image,initial,list,completed,nullptr);
}
GuestTextureCache::IndependentImageVersion GuestTextureCache::CaptureIndependentImageVersion(
    Entry& image,const GuestTextureDesc& target,const GuestTextureLayout& physical,
    ID3D12GraphicsCommandList* list,uint64_t completed,
    const std::function<Resource(uint64_t,uint64_t)>& capture_buffer){
    const auto owner=DescribeGuestImageOwnership(image);
    // RefreshImage can initialize current native I from B without setting the
    // original GpuModified flag. Depth reinterpretation copies those contents
    // too; it must not require a guest GPU write or invent one to permit copy.
    // Registered current allocation, dirty/boundary and ownership guards still
    // reject stale I before capturing any target initialization bytes.
    if(!HasCurrentNativeContents(image)||!image.buffer_alias_version||
       image.HasUnprotectedCpuPages()||!owner||!CanRetainGuestImageOwnership(*this,*owner)||
       !list||!capture_buffer||!CanUploadGpuSnapshot(target,physical,target.writable)||
       (!CanMapIndependentDepthImage(image.desc,image.layout,target,physical)&&
        !CanMapIndependentColorGrowth(image.desc,image.layout,target,physical))){
        std::fprintf(stderr,"DX12 depth conversion rejected uid=%llu cache=%u gpu=%u alias=%u valid=%u boundary=%u retained=%u upload=%u mapping=%u source=%llx/%ux%u/p%u/l%u/m%u/f%u-%u/t%u/d%u/s%u target=%llx/%ux%u/p%u/l%u/m%u/f%u-%u/t%u/d%u/s%u\n",
            (unsigned long long)image.image_uid,unsigned(image.cache==this),unsigned(image.gpu_owned),
            unsigned(image.buffer_alias_version),unsigned(image.HasValidLogicalImage()),unsigned(image.HasUnprotectedCpuPages()),
            unsigned(owner&&CanRetainGuestImageOwnership(*this,*owner)),
            unsigned(CanUploadGpuSnapshot(target,physical,target.writable)),
            unsigned(CanMapIndependentDepthImage(image.desc,image.layout,target,physical)),
            (unsigned long long)image.desc.address,image.desc.width,image.desc.height,image.desc.pitch,image.desc.layers,
            image.desc.levels,image.desc.data_format,image.desc.number_format,image.desc.tile_mode,unsigned(image.desc.depth_target),image.desc.samples,
            (unsigned long long)target.address,target.width,target.height,target.pitch,target.layers,target.levels,
            target.data_format,target.number_format,target.tile_mode,unsigned(target.depth_target),target.samples);
        std::fflush(stderr);
        throw std::invalid_argument("Independent depth conversion has no live physical image mapping");
    }
    GuestTextureLayout stencil;
    if(target.stencil_address){
        stencil=LayoutGuestTexture(DescribeGuestStencil(target));
    }
    auto pin=PinBinding(image);
    auto depth_seed=capture_buffer(target.address,physical.guest_bytes);
    auto stencil_seed=target.stencil_address?capture_buffer(target.stencil_address,stencil.guest_bytes):Resource{};
    IndependentImageVersion result;
    const bool color_ms_depth=image.desc.address==target.address&&
        GuestMultisample::CanReinterpretColorAsMsDepth(image.desc,image.layout,target,physical);
    const bool mapped=!MatchesGuestMipLayout(image.layout,physical);
    result.depth=color_ms_depth?std::move(depth_seed):mapped?RetileImageVersion(image,depth_seed,list,completed,nullptr,0,
        image.desc.depth_target,&target,&physical):RetileBufferVersion(image,depth_seed,list,completed);
    result.depth.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if(target.stencil_address){
        // BOTH pinned overlap paths, CopyImage and CopyImageWithBuffer, exclude
        // S. Explicit CopyDepthStencil is a separate operation. Preserve the
        // target's initialization seed instead of importing the old native S.
        result.stencil=std::move(stencil_seed);
        result.stencil.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    return result;
}
GuestTextureCache::IndependentImageVersion GuestTextureCache::CaptureCachedDepthRecreationVersion(
    Entry& image,const GuestTextureDesc& target,const GuestTextureLayout& layout,
    ID3D12GraphicsCommandList* list,uint64_t completed,
    const std::function<Resource(uint64_t,uint64_t)>& capture_buffer){
    if(!list||!capture_buffer||!IsCachedDepthRecreationSource(image,target,layout)||
       !CanUploadGpuSnapshot(image.desc,image.layout)||!CanUploadGpuSnapshot(target,layout,target.writable))
        throw std::invalid_argument("Cached depth recreation has no initialized native allocation mapping");
    auto pin=PinBinding(image);
    auto seed=capture_buffer(target.address,layout.guest_bytes);
    auto stencil=target.stencil_address?capture_buffer(target.stencil_address,
        LayoutGuestTexture(DescribeGuestStencil(target)).guest_bytes):Resource{};
    IndependentImageVersion result;
    result.depth=RetileImageVersion(image,seed,list,completed,nullptr,0,true,&target,&layout,
        NativeReadPurpose::CachedDepthRecreation);
    result.depth.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    result.stencil=std::move(stencil);
    if(result.stencil.handle)result.stencil.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    return result;
}
Resource GuestTextureCache::CaptureDepthPlane(Entry& image,UINT plane,Resource& initial,
    ID3D12GraphicsCommandList* list,uint64_t completed){
    if(image.cache!=this)throw std::invalid_argument("Depth snapshot source belongs to another cache");
    auto pin=PinBinding(image);
    auto result=RetileImageVersion(image,initial,list,completed,nullptr,plane,true);
    result.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    return result;
}
Resource GuestTextureCache::RetileImageVersion(Entry& image,Resource& initial,
    ID3D12GraphicsCommandList* list,uint64_t completed,const WritableBlockAlias* alias,
    UINT plane,bool private_depth_snapshot,const GuestTextureDesc* mapped_desc,const GuestTextureLayout* mapped_layout,
    NativeReadPurpose read_purpose){
    const bool cached=read_purpose==NativeReadPurpose::CachedDepthRecreation;
    if(cached&&(!mapped_desc||!mapped_layout||!private_depth_snapshot||plane||
       !IsCachedDepthRecreationSource(image,*mapped_desc,*mapped_layout)))
        throw std::invalid_argument("Cached native depth read lost its initialized recreation source");
    // Cached recreation reads retained native allocation contents, not current
    // guest memory. CPU dirtying/boundary equality cannot certify independent I
    // reuse, but does not destroy these old native contents either.
    if(!cached&&image.HasUnprotectedCpuPages())
        throw std::runtime_error("Independent image transfer requires boundary validation");
    const bool dependent=alias&&alias->image==&image&&IsLiveWritableBlockAlias(*alias);
    if(image.logical_dirty&&!dependent&&!cached)throw std::runtime_error("Independent image transfer requires a valid logical version");
    if(private_depth_snapshot){
        if(alias||(!cached&&!image.desc.depth_target)||(!cached&&(!image.gpu_owned||
           (!image.HasValidLogicalImage()&&!HasCurrentDepthContents(image))))||
           plane>1||(plane&&!image.desc.stencil_address)||!CanUploadGpuSnapshot(image.desc,image.layout))
            throw std::invalid_argument("Unsupported native depth snapshot source/plane");
    }else if(plane)throw std::invalid_argument("Formatted image transfer cannot publish a stencil plane");
    else if(!dependent){
        const auto ownership=DescribeGuestImageOwnership(image);
        if(!image.gpu_owned||!image.buffer_alias_version||!ownership||
           !CanRetainGuestImageOwnership(*this,*ownership))
            throw std::invalid_argument("Unsupported independent image ownership for formatted transfer");
    }
    if(bool(mapped_desc)!=bool(mapped_layout)||(mapped_desc&&(alias||plane||
       (!CanMapIndependentDepthImage(image.desc,image.layout,*mapped_desc,*mapped_layout)&&
        !CanMapIndependentColorGrowth(image.desc,image.layout,*mapped_desc,*mapped_layout)))))
        throw std::invalid_argument("Independent image transfer has no physical destination mapping");
    const auto source_interpreted=plane?DescribeGuestStencil(image.desc):image.desc;
    const auto& source_physical=plane?image.stencil_layout:image.layout;
    auto interpreted=mapped_desc?*mapped_desc:source_interpreted;
    const auto& physical=mapped_layout?*mapped_layout:source_physical;
    if(!list||!image.texture.handle||
       !initial.handle||initial.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       initial.handle->GetDesc().Width<physical.guest_bytes)
        throw std::invalid_argument("Unsupported independent image formatted-buffer geometry");
    const auto resource_desc=image.texture.handle->GetDesc();
    const bool multisampled=image.desc.samples>1;
    if(resource_desc.SampleDesc.Count!=image.desc.samples||
       (multisampled&&!private_depth_snapshot&&!CanTransferIndependentMultisample(image.desc,image.layout)))
        throw std::invalid_argument("Independent image sample backing has no exact transfer");
    const auto linear_bytes=source_physical.HostBytesPerPixel();
    const bool inherited=!plane&&image.layout.expanded_bytes_per_pixel!=0;
    if(inherited&&(!HasExactPackedExpansion(image.desc,image.layout)||
       !image.expanded_guest_backing.handle||!image.expanded_written_mask.handle||!image.expanded_backing_bytes))
        throw std::invalid_argument("Independent expanded image lost its exact packed backing");
    struct Linear {D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT64 bytes;UINT mip,layer,native_bytes;Resource buffer;
        std::shared_ptr<GuestTiler::Lease> dispatch;std::shared_ptr<GuestMultisample::Lease> samples;};
    const UINT copy_mips=mapped_desc?std::min(image.desc.levels,mapped_desc->levels):image.desc.levels;
    const UINT copy_layers=mapped_desc?std::min(image.desc.layers,mapped_desc->layers):image.desc.layers;
    std::vector<Linear> linear;linear.reserve(size_t(copy_mips)*copy_layers);
    if(!inherited)for(UINT layer=0;layer<copy_layers;++layer)for(UINT mip=0;mip<copy_mips;++mip){
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{},row_bytes{};
        if(multisampled){
            footprint=GuestMultisample::Footprint(source_interpreted,source_physical);
            row_bytes=u64(image.desc.width)*linear_bytes;
            bytes=GuestMultisample::LinearBytes(source_interpreted,footprint);
        }else DX12_API_CALL(device_->GetCopyableFootprints(&resource_desc,mip+layer*image.desc.levels+plane*image.desc.levels*image.desc.layers,
            1,0,&footprint,nullptr,&row_bytes,&bytes));
        const auto blocks=(std::max(image.desc.width>>mip,1u)+source_physical.block_extent-1)/source_physical.block_extent;
        if(!blocks||row_bytes%blocks||!bytes||bytes>UINT32_MAX||row_bytes/blocks>16)
            throw std::logic_error("Independent image transfer footprint changed");
        if(row_bytes/blocks!=linear_bytes&&!(private_depth_snapshot&&!plane&&
           image.desc.data_format==2&&linear_bytes==2&&row_bytes/blocks==4))
            throw std::logic_error("Independent image transfer needs an explicit plane conversion");
        linear.push_back({footprint,bytes,mip,layer,UINT(row_bytes/blocks),{},{},{}});
    }
    const auto mask_bytes=GuestTiler::WrittenBytes(physical.guest_bytes);
    const auto allocation_bytes=[&](uint64_t length,bool uav){
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width=length;desc.Height=desc.DepthOrArraySize=desc.MipLevels=1;
        desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags=uav?D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS:D3D12_RESOURCE_FLAG_NONE;
        const auto allocation=DX12_API_CALL(device_->GetResourceAllocationInfo(0,1,&desc));
        if(!allocation.SizeInBytes||allocation.SizeInBytes==UINT64_MAX)
            throw std::runtime_error("Invalid formatted image transfer allocation requirements");
        return allocation.SizeInBytes;
    };
    // Private image inputs are retained by a later entry's input pool, whose
    // residency account uses native buffer width. Expose the actual committed
    // buffer granularity rather than hiding a 64-KiB allocation behind a tiny
    // logical stencil extent. Guest tiling still uses physical.guest_bytes.
    const auto output_bytes=private_depth_snapshot?
        (physical.guest_bytes+D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1)&
            ~u64(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1):physical.guest_bytes;
    auto reservation=allocation_bytes(output_bytes,true)+
        (inherited?0:allocation_bytes(mask_bytes,true))+allocation_bytes(initial.handle->GetDesc().Width,false);
    for(const auto& slice:linear)reservation+=allocation_bytes((slice.bytes+3)&~u64(3),multisampled);
    ReserveMemory(reservation,completed,&image);
    if(!image.tiler)image.tiler=std::make_shared<GuestTiler>(device_.Get());
    if(multisampled&&!image.multisample)image.multisample=std::make_shared<GuestMultisample>(device_.Get());
    auto output=GuestTiler::Scratch(device_.Get(),output_bytes);
    auto mask=inherited?image.expanded_written_mask:GuestTiler::Scratch(device_.Get(),mask_bytes);
    auto merge=inherited?image.tiler->PrepareMaskedMerge(image.expanded_guest_backing,output,mask,image.layout.guest_bytes):
        std::shared_ptr<GuestTiler::Lease>{};
    for(auto& slice:linear){
        slice.buffer=multisampled?GuestTiler::Scratch(device_.Get(),slice.bytes):
            Buffer(device_.Get(),(slice.bytes+3)&~u64(3),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        const auto source_format=GuestTiler::DescribeNativeFormat(source_interpreted,source_physical);
        // The pinned copy uses source width/height, leaving the target-only
        // region initialized from its B seed. Keep TARGET pitch, mip offsets,
        // tiling and layer strides while bounding work by the source extent.
        auto copied=interpreted;
        if(mapped_desc){copied.width=source_interpreted.width;copied.height=source_interpreted.height;}
        slice.dispatch=image.tiler->Prepare(slice.buffer,output,mask,copied,physical,
            slice.mip,slice.layer,slice.footprint,slice.native_bytes,true,false,&source_format);
        if(multisampled)slice.samples=image.multisample->Prepare(image.texture,slice.buffer,
            image.desc,image.layout,slice.layer,plane,false);
    }
    // Retain all resources/descriptors and account transient bytes before recording.
    image.buffer_sync_resources.reserve(image.buffer_sync_resources.size()+linear.size()+3+(inherited?1:0));
    image.tiler_leases.reserve(image.tiler_leases.size()+linear.size()+(inherited?1:0));
    image.multisample_leases.reserve(image.multisample_leases.size()+(multisampled?linear.size():0));
    image.buffer_sync_resources.insert(image.buffer_sync_resources.end(),{output,mask,initial});
    if(inherited){image.buffer_sync_resources.push_back(image.expanded_guest_backing);image.tiler_leases.push_back(merge);}
    for(const auto& slice:linear){image.buffer_sync_resources.push_back(slice.buffer);
        image.tiler_leases.push_back(slice.dispatch);
        if(slice.samples)image.multisample_leases.push_back(slice.samples);}
    image.buffer_sync_allocation_bytes+=reservation;
    image.pending=true;
    // ReserveMemory/recursive Upload may submit internally. Register this
    // pending lease before recording commands, not only at outer dispatch end.
    if(transfer_submission_)transfer_submission_();
    if(image.texture.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)image.texture.UavBarrier(list);
    if(!multisampled)image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    initial.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    const auto seed_bytes=private_depth_snapshot?std::min(output_bytes,initial.handle->GetDesc().Width):physical.guest_bytes;
    DX12_API_CALL(list->CopyBufferRegion(output.handle.Get(),0,initial.handle.Get(),0,seed_bytes));
    if(inherited)merge->Record(list,image.expanded_guest_backing,output,image.expanded_written_mask);
    for(auto& slice:linear){
        // MSAA textures cannot be copied into a placed buffer footprint. Load
        // every sample through the existing raw integer transfer, without resolve,
        // then retile only logical texels into the physical-seeded private copy.
        if(slice.samples)slice.samples->Record(list,image.texture,slice.buffer);
        else{
            D3D12_TEXTURE_COPY_LOCATION source{};source.pResource=image.texture.handle.Get();
            source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;source.SubresourceIndex=slice.mip+slice.layer*image.desc.levels+plane*image.desc.levels*image.desc.layers;
            D3D12_TEXTURE_COPY_LOCATION destination{};destination.pResource=slice.buffer.handle.Get();
            destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=slice.footprint;
            DX12_API_CALL(list->CopyTextureRegion(&destination,0,0,0,&source,nullptr));
        }
        slice.dispatch->Record(list,slice.buffer,output,mask);
    }
    output.UavBarrier(list);
    return output;
}
void GuestTextureCache::Collect(uint64_t completed){
    // Completed fence usually stays constant across thousands of bindings in
    // one PS4 packet. Do the O(cache entries) retirement scan once per advance.
    if(completed<=collected_fence_)return;collected_fence_=completed;
    for(auto& e:entries_)if(!e->pending&&e->fence<=completed){
        e->uploads.clear();e->tiler_leases.clear();e->multisample_leases.clear();e->color_depth_leases.clear();e->native_plane_sources.clear();e->sample_copy_leases.clear();e->storage_leases.clear();
        e->storage_clear_leases.clear();
        e->buffer_sync_resources.clear();
        e->buffer_sync_allocation_bytes=0;
        e->format_transfers.clear();e->allocation_bytes-=e->format_retirement_bytes;e->format_retirement_bytes=0;
    }
}
Resource* GuestTextureCache::CoherentDisplayTexture(uint64_t address,UINT width,UINT height,UINT pitch,uint64_t completed){
    auto [first,last]=address_index_.equal_range(address);for(auto indexed=first;indexed!=last;++indexed){auto& e=*indexed->second;
        const auto& d=e.desc;
        if(d.address!=address||d.width!=width||d.height!=height||d.pitch!=pitch||
           d.tile_mode!=10||d.data_format!=10||(d.number_format!=0&&d.number_format!=9)||
           d.writable||d.depth_target||d.stencil_address||d.volume_depth||d.one_dimensional||
           d.levels!=1||d.layers!=1||d.samples!=1||d.swizzle!=std::array<uint32_t,4>{4,5,6,7}||
           e.pending||e.fence>completed||(!e.gpu_owned&&e.sampled_snapshot.size()!=e.layout.guest_bytes))continue;
        if(!e.gpu_owned&&memcmp(e.sampled_snapshot.data(),reinterpret_cast<const void*>(address),size_t(e.layout.guest_bytes)))continue;
        e.last_use=++use_sequence_;return &e.texture;
    }
    return nullptr;
}
void GuestTextureCache::Invalidate(uint64_t a,uint64_t s,uint64_t completed){if(!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid texture invalidation");
    const auto matches=[&](const Entry& e){return (e.desc.address<a+s&&a<e.desc.address+e.layout.guest_bytes)||
        (e.desc.stencil_address&&e.desc.stencil_address<a+s&&a<e.desc.stencil_address+e.stencil_layout.guest_bytes);};
    for(const auto& e:entries_)if(matches(*e)&&(e->BindingPinned()||e->pending||e->fence>completed))throw std::runtime_error("texture invalidation before binding/GPU retirement");
    const auto before=entries_.size();std::erase_if(entries_,[&](const auto& e){const bool remove=matches(*e);
        if(remove&&e->cpu_watched&&release_immutable_)release_immutable_(*e);return remove;});
    if(entries_.size()!=before)RebuildIndex();}
}


