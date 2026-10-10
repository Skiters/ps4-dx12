// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <unordered_map>
namespace NativeDX12 {
// Mirror Vulkan's HTile clear masks: new depth surfaces start cleared,
// fill/clear shaders clear all slices, attachment use consumes one slice.
class GuestDepthMetadata {
    std::unordered_map<uint64_t,uint32_t> masks_;
public:
    void Register(uint64_t address){if(address)masks_.try_emplace(address,UINT32_MAX);}
    bool Contains(uint64_t address) const{return masks_.contains(address);}
    bool Clear(uint64_t address){auto it=masks_.find(address);if(it==masks_.end())return false;it->second=UINT32_MAX;return true;}
    bool Consume(uint64_t address,uint32_t slice){auto it=masks_.find(address);
        if(it==masks_.end()||slice>=32)return false;
        const auto bit=uint32_t(1)<<slice;const bool cleared=(it->second&bit)!=0;it->second&=~bit;return cleared;}
    void Remove(uint64_t address,uint64_t size){for(auto it=masks_.begin();it!=masks_.end();)
        if(it->first>=address&&it->first-address<size)it=masks_.erase(it);else ++it;}
};
}
