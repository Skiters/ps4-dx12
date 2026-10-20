// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <algorithm>
#include <stdexcept>

namespace NativeDX12 {
struct GuestIndexRange {
    u64 address,bytes;
    static GuestIndexRange Make(u64 base,u32 first,u32 count,u32 maximum,u32 element,bool indirect){
        if(element!=2&&element!=4)throw std::invalid_argument("Invalid guest index element size");
        const u64 offset=u64(first)*element;
        if(base>UINT64_MAX-offset)throw std::out_of_range("Guest index address overflow");
        // Vulkan BindIndexBuffer clamps starting at the shifted address. The
        // indirect maximum is a size, not an absolute end index.
        const u64 bytes=u64(indirect?maximum:std::min(count,maximum))*element;
        const u64 address=base+offset;
        // The caller clamps bytes to mapped memory before checking the final
        // extent. An oversized requested range must not reject a valid prefix.
        return {address,bytes};
    }
};
} // namespace NativeDX12
