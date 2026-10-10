// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "guest_texture_cache.h"
namespace NativeDX12 {
// CPU attachment readbacks are a mandatory coherence boundary, including
// when the draw returned with deferred descriptor/temporary-buffer leases.
inline void RetireGuestAttachmentReadbacks(Rasterizer& rasterizer,
    GuestTextureCache& textures,u64 fence,
    std::span<GuestTextureCache::Entry* const> targets) {
    rasterizer.Finish();
    textures.Submitted(fence);
    for(auto* target:targets)if(target)textures.Used(*target,fence);
}
}
