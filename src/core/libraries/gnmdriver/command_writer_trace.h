// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <vector>

#include "common/types.h"

namespace Libraries::GnmDriver::CommandWriterTrace {

struct Entry {
    uintptr_t address{};
    u32 size_dw{};
    std::string_view function{};
    u64 serial{};
};

inline constexpr size_t EntryCount = 64_KB;
inline std::array<Entry, EntryCount> entries{};
inline std::mutex entries_mutex{};
inline u64 next_serial{};

inline void Record(const char* function, const u32* address, u32 size_dw) {
    if (address == nullptr || size_dw == 0) {
        return;
    }
    std::scoped_lock lock{entries_mutex};
    const u64 serial = ++next_serial;
    entries[(serial - 1) % entries.size()] = {
        .address = reinterpret_cast<uintptr_t>(address),
        .size_dw = size_dw,
        .function = function,
        .serial = serial,
    };
}

[[nodiscard]] inline std::vector<Entry> FindRecentCovering(uintptr_t address) {
    std::vector<Entry> matches;
    std::scoped_lock lock{entries_mutex};
    for (const auto& entry : entries) {
        if (entry.serial == 0 || address < entry.address) {
            continue;
        }
        const uintptr_t byte_offset = address - entry.address;
        if (byte_offset < static_cast<uintptr_t>(entry.size_dw) * sizeof(u32)) {
            matches.push_back(entry);
        }
    }
    std::ranges::sort(matches, std::greater{}, &Entry::serial);
    if (matches.size() > 8) {
        matches.resize(8);
    }
    return matches;
}

class Scope {
public:
    Scope(const char* function_, const u32* address_, u32 size_dw_)
        : function{function_}, address{address_}, size_dw{size_dw_} {}

    ~Scope() {
        Record(function, address, size_dw);
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    const char* function;
    const u32* address;
    u32 size_dw;
};

} // namespace Libraries::GnmDriver::CommandWriterTrace
