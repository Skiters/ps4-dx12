// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include "common/types.h"

namespace Libraries::GnmDriver::CommandWriterTrace {

struct Entry {
    static constexpr u32 SnapshotWords = 64;

    uintptr_t address{};
    u32 size_dw{};
    std::string_view function{};
    u64 serial{};
    u64 fingerprint{};
    u64 current_fingerprint{};
    std::array<u32, SnapshotWords> snapshot{};
    u32 snapshot_size_dw{};
};

inline constexpr size_t EntryCount = 64_KB;
inline constexpr u32 MaxFingerprintWords = 256;

[[nodiscard]] inline std::array<Entry, EntryCount>& Entries() {
    static const auto entries = std::make_unique<std::array<Entry, EntryCount>>();
    return *entries;
}

[[nodiscard]] inline std::mutex& EntriesMutex() {
    static std::mutex mutex;
    return mutex;
}

[[nodiscard]] inline u64& NextSerial() {
    static u64 serial{};
    return serial;
}

[[nodiscard]] inline u64 Fingerprint(const u32* address, u32 size_dw) {
    constexpr u64 OffsetBasis = 14695981039346656037ull;
    constexpr u64 Prime = 1099511628211ull;
    u64 hash = OffsetBasis;
    const u32 tracked_size = std::min(size_dw, MaxFingerprintWords);
    for (u32 index = 0; index < tracked_size; ++index) {
        hash ^= address[index];
        hash *= Prime;
    }
    return hash;
}

inline void RecordImpl(const char* function, const u32* address, u32 size_dw, bool written) {
    if (address == nullptr || size_dw == 0) {
        return;
    }
    std::scoped_lock lock{EntriesMutex()};
    auto& entries = Entries();
    const u64 serial = ++NextSerial();
    auto& entry = entries[(serial - 1) % entries.size()];
    entry = {
        .address = reinterpret_cast<uintptr_t>(address),
        .size_dw = size_dw,
        .function = function,
        .serial = serial,
    };
    if (written) {
        entry.fingerprint = Fingerprint(address, size_dw);
        entry.snapshot_size_dw = std::min(size_dw, Entry::SnapshotWords);
        std::copy_n(address, entry.snapshot_size_dw, entry.snapshot.begin());
    }
}

inline void Record(const char* function, const u32* address, u32 size_dw) {
    RecordImpl(function, address, size_dw, false);
}

inline void RecordWritten(const char* function, const u32* address, u32 size_dw) {
    RecordImpl(function, address, size_dw, true);
}

[[nodiscard]] inline std::vector<Entry> FindRecentCovering(uintptr_t address) {
    std::vector<Entry> matches;
    std::scoped_lock lock{EntriesMutex()};
    const auto& entries = Entries();
    for (const auto& entry : entries) {
        if (entry.serial == 0 || address < entry.address) {
            continue;
        }
        const uintptr_t byte_offset = address - entry.address;
        if (byte_offset < static_cast<uintptr_t>(entry.size_dw) * sizeof(u32)) {
            auto match = entry;
            if (entry.fingerprint != 0) {
                match.current_fingerprint =
                    Fingerprint(reinterpret_cast<const u32*>(entry.address), entry.size_dw);
            }
            matches.push_back(std::move(match));
        }
    }
    std::ranges::sort(matches, std::greater{}, &Entry::serial);
    if (matches.size() > 8) {
        matches.resize(8);
    }
    return matches;
}

// Construction capacity is context only; it is never a claim that its tail was written.
struct Construction {
    uintptr_t address{};
    u32 capacity_dw{};
    u32 output_dw{};
    uintptr_t caller{};
    u64 serial{};
};
inline std::array<Construction, 4096> constructions{};
inline u64 construction_serial{};
inline void RecordConstruction(const u32* address, u32 capacity_dw, u32 output_dw, uintptr_t caller = 0) {
    if (!address || !output_dw || output_dw > capacity_dw) return;
    std::scoped_lock lock{EntriesMutex()};
    const u64 serial = ++construction_serial;
    constructions[(serial - 1) % constructions.size()] = {
        reinterpret_cast<uintptr_t>(address), capacity_dw, output_dw, caller, serial};
}
[[nodiscard]] inline Construction FindConstruction(uintptr_t address) {
    std::scoped_lock lock{EntriesMutex()};
    Construction result{};
    for (const auto& entry : constructions) {
        if (entry.serial > result.serial && address >= entry.address &&
            address - entry.address < uintptr_t(entry.capacity_dw) * sizeof(u32)) result = entry;
    }
    return result;
}
class Scope {
public:
    Scope(const char* function_, const u32* address_, u32 size_dw_)
        : function{function_}, address{address_}, size_dw{size_dw_} {}

    ~Scope() {
        if (written) {
            RecordWritten(function, address, size_dw);

        }
    }

    void MarkWritten(u32 actual_size_dw) {
        size_dw = std::min(size_dw, actual_size_dw);
        written = true;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

private:
    const char* function;
    const u32* address;
    u32 size_dw;
    bool written{};
};

} // namespace Libraries::GnmDriver::CommandWriterTrace
