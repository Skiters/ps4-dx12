// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <limits>
#include <optional>
#include <vector>
#include "common/types.h"

namespace Core {

struct BackingWriteSpan {
    PAddr physical_offset;
    u64 source_offset;
    u64 size;
};

// The caller must keep the memory map locked until the returned writes finish.
// Plan the entire range first: a hole must not leave a successfully written prefix.
// Keep virtual traversal order, including writes through overlapping physical aliases.
template <typename VMAMap, typename HasBacking>
std::optional<std::vector<BackingWriteSpan>> PlanBackingWrite(const VMAMap& vmas, VAddr address,
                                                            u64 size, HasBacking has_backing) {
    std::vector<BackingWriteSpan> writes;
    if (size == 0) {
        return writes;
    }
    if (size > std::numeric_limits<VAddr>::max() - address) {
        return std::nullopt;
    }

    auto vma_it = vmas.upper_bound(address);
    if (vma_it == vmas.begin()) {
        return std::nullopt;
    }
    --vma_it;

    VAddr cursor = address;
    u64 remaining = size;
    while (remaining != 0) {
        if (vma_it == vmas.end()) {
            return std::nullopt;
        }
        const auto& vma = vma_it->second;
        if (vma.size > std::numeric_limits<VAddr>::max() - vma.base || cursor < vma.base ||
            cursor - vma.base >= vma.size || !has_backing(vma)) {
            return std::nullopt;
        }
        u64 offset = cursor - vma.base;
        u64 in_vma = std::min(remaining, vma.size - offset);
        auto physical_it = vma.phys_areas.upper_bound(offset);
        if (physical_it == vma.phys_areas.begin()) {
            return std::nullopt;
        }
        --physical_it;

        while (in_vma != 0) {
            if (physical_it == vma.phys_areas.end()) {
                return std::nullopt;
            }
            const auto& [physical_start, physical] = *physical_it;
            if (physical.size > std::numeric_limits<u64>::max() - physical_start ||
                physical.size > std::numeric_limits<PAddr>::max() - physical.base ||
                offset < physical_start || offset - physical_start >= physical.size) {
                return std::nullopt;
            }
            const u64 physical_offset = offset - physical_start;
            const u64 bytes = std::min(in_vma, physical.size - physical_offset);
            writes.push_back({physical.base + physical_offset, cursor - address, bytes});
            cursor += bytes;
            offset += bytes;
            remaining -= bytes;
            in_vma -= bytes;
            ++physical_it;
        }
        ++vma_it;
    }
    return writes;
}

} // namespace Core
