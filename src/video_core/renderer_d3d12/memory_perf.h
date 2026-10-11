// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <charconv>
#include <string_view>
#include <string>

namespace NativeDX12 {
inline constexpr u64 MemoryMiB=1024ull*1024;
struct MemoryPerfConfig {
    bool enabled{};
    u64 requested{}; // Zero preserves the existing automatic default policy.
    u64 cpu_pool=512*MemoryMiB;
    static u64 Megabytes(const char* name,u64 fallback,u64 maximum) {
        const auto* text=std::getenv(name);if(!text||!*text)return fallback;
        u64 value{};const auto end=text+std::strlen(text);const auto result=std::from_chars(text,end,value);
        if(result.ec!=std::errc{}||result.ptr!=end||!value||value>maximum)
            throw std::invalid_argument(std::string("Invalid memory budget: ")+name);
        return value*MemoryMiB;
    }
    static MemoryPerfConfig Environment() {
        MemoryPerfConfig c;const auto* mode=std::getenv("SHADPS4_DX12_MEMORY_PERF");
        c.enabled=mode&&std::strcmp(mode,"1")==0;
        c.requested=Megabytes("SHADPS4_DX12_BUFFER_BUDGET_MB",c.enabled?4096*MemoryMiB:0,8192);
        c.cpu_pool=Megabytes("SHADPS4_DX12_CPU_POOL_MB",512*MemoryMiB,4096);
        return c;
    }
    struct Limits {u64 cache,arena;};
    Limits Budget(u64 budget,u64 usage,u64 buffer_commitment)const {
        if(!enabled){
            auto cache=std::clamp<u64>(budget/8,128*MemoryMiB,2048*MemoryMiB);
            if(requested)cache=std::min(cache,requested);
            return {cache,std::clamp<u64>(budget/4,1024*MemoryMiB,8192*MemoryMiB)};
        }
        // Account for other process resources and leave at least 2 GiB spare.
        // No reservation: these are caps on lazy allocation, not allocations.
        const auto other=usage>buffer_commitment?usage-buffer_commitment:0;
        const auto reserve=std::max<u64>(2048*MemoryMiB,budget/8);
        const auto available=budget>other&&budget-other>reserve?budget-other-reserve:0;
        const auto arena=std::min<u64>(requested+requested/4,available);
        return {std::min(requested,arena*4/5),arena};
    }
};
}
