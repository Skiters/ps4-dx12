// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
namespace NativeDX12 {
// Captures each mip/layer using D3D12's actual row footprints, then restores
// PS4 tiling without overwriting padding. Owner must retain until fence retires.
class GuestImageReadback {
    struct Slice {Resource buffer;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT mip,layer,plane;};
    GuestTextureDesc desc_;
    GuestTextureLayout layout_;
    GuestTextureLayout stencil_layout_;
    std::vector<Slice> slices_;
    struct TiledPlane {Resource buffer;uint64_t mask_offset;UINT plane;};
    std::vector<TiledPlane> tiled_planes_;
    Resource expanded_backing_,expanded_mask_;
    std::vector<std::shared_ptr<GuestTiler::Lease>> tiler_leases_;
    std::vector<std::shared_ptr<GuestMultisample::Lease>> multisample_leases_;
    std::shared_ptr<std::vector<Resource>> storage_;
    std::vector<std::byte> readonly_compressed_source_;
    uint64_t fence_{};
    void RecordTiled(ID3D12Device*,GuestTextureCache::Entry&,ID3D12GraphicsCommandList*);
    void RecordExpandedBacking(ID3D12Device*,GuestTextureCache::Entry&,ID3D12GraphicsCommandList*);
public:
    GuestImageReadback(ID3D12Device*,GuestTextureCache::Entry&,ID3D12GraphicsCommandList*);
    void Submitted(uint64_t fence);
    // Plane bits: 1 = color/depth, 2 = stencil. The default restores all planes.
    void Resolve(uint64_t completed,std::span<std::byte> guest,std::span<std::byte> stencil={},
                 uint32_t plane_mask=3);
};
// Pinned DownloadImageMemory copies mip zero into a compact pitch-based CPU
// span, independently of the full guest mip/layer allocation layout.
class GuestLinearImageReadback {
    struct Slice {Resource buffer;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT layer,native_bytes;};
    Resource source_;
    Resource packed_source_,packed_readback_;
    GuestTextureDesc desc_;
    GuestTextureLayout layout_;
    std::vector<Slice> slices_;
    uint64_t bytes_{},allocation_bytes_{},fence_{};
public:
    GuestLinearImageReadback(ID3D12Device*,GuestTextureCache::Entry&,ID3D12GraphicsCommandList*);
    uint64_t Bytes()const{return bytes_;}
    uint64_t AllocationBytes()const{return allocation_bytes_;}
    void ReleaseSource(uint64_t completed){
        if(!fence_||completed<fence_)throw std::logic_error("Linear download source is still in flight");
        source_={};packed_source_={};
    }
    void Submitted(uint64_t fence){if(!fence)throw std::invalid_argument("Empty linear image readback fence");fence_=fence;}
    void Resolve(uint64_t completed,std::span<std::byte> guest);
};
}
