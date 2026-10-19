// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "guest_texture_cache.h"
namespace NativeDX12 {
// One logical draw may use several replay submissions. Select its native
// attachments once; a later slice must not upload the original guest/depth
// snapshot over pixels already produced by an earlier slice.
class GuestReplayAttachments {
    std::array<GuestTextureCache::Entry*,9> entries_{};
    std::array<std::shared_ptr<void>,9> pins_{};
    bool beginning_{true},closed_{};
public:
    GuestReplayAttachments()=default;
    GuestReplayAttachments(const GuestReplayAttachments&)=delete;
    GuestReplayAttachments& operator=(const GuestReplayAttachments&)=delete;
    bool Beginning()const{return beginning_;}
    template<class Initialize>
    GuestTextureCache::Entry& Select(GuestTextureCache& textures,u32 slot,Initialize&& initialize){
        if(closed_||slot>=entries_.size())throw std::logic_error("Invalid logical replay attachment selection");
        if(entries_[slot])return *entries_[slot];
        if(!beginning_)throw std::logic_error("Replay continuation introduced a new attachment");
        auto& entry=initialize();auto pin=textures.PinBinding(entry);
        entries_[slot]=&entry;pins_[slot]=std::move(pin);return entry;
    }
    void Submitted(){
        if(closed_)throw std::logic_error("Replay attachment scope is already closed");
        beginning_=false;
    }
    const std::shared_ptr<void>& BindingPin(u32 slot)const{
        if(closed_||slot>=pins_.size()||!pins_[slot])throw std::logic_error("Replay attachment was not selected");
        return pins_[slot];
    }
    // Share one pin control block across submissions. Release CPU selection
    // pins after readback retirement, before guest-memory reconciliation.
    void ReleasePins(){pins_.fill({});closed_=true;}
};
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
