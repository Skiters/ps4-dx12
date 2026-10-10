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
    std::shared_ptr<std::vector<Resource>> storage_;
    uint64_t fence_{};
public:
    GuestImageReadback(ID3D12Device*,GuestTextureCache::Entry&,ID3D12GraphicsCommandList*);
    void Submitted(uint64_t fence);
    void Resolve(uint64_t completed,std::span<std::byte> guest,std::span<std::byte> stencil={});
};
}
