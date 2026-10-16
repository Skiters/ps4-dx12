// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace NativeDX12 {
struct GuestProtectionRange {
    uint64_t address, size;
};
inline constexpr std::optional<GuestProtectionRange> GuestProtectionPages(uint64_t address,
                                                                       uint64_t size) {
    if (!size || address > UINT64_MAX - size) return std::nullopt;
    const auto first = address & ~uint64_t(4095), last = (address + size - 1) | uint64_t(4095);
    if (last == UINT64_MAX) return std::nullopt;
    return GuestProtectionRange{first, last - first + 1};
}
inline constexpr bool ShareGuestProtectionPages(uint64_t a, uint64_t as, uint64_t b, uint64_t bs) {
    if (!as || !bs) return false;
    if (a > UINT64_MAX - as || b > UINT64_MAX - bs)
        throw std::out_of_range("Guest protection range overflow");
    return (a >> 12) <= ((b + bs - 1) >> 12) && (b >> 12) <= ((a + as - 1) >> 12);
}

using GuestImageBoundaryPages = std::map<uint64_t, std::vector<std::byte>>;

// Pinned MaybeCpuDirty preserves I when an outside CPU store releases a shared
// boundary page. Keep complete physical B segments (at most two pages), rather
// than a partial pixel hash. Repeated outside stores retain the earliest proof.
template <class Read>
bool RememberGuestImageBoundary(GuestImageBoundaryPages& saved, uint64_t image, uint64_t bytes,
                                uint64_t address, uint64_t size, Read&& read) {
    if (!ShareGuestProtectionPages(image, bytes, address, size) ||
        (address < image + bytes && image < address + size)) return false;
    bool added = false;
    const auto first = image & ~uint64_t(4095), last = (image + bytes - 1) & ~uint64_t(4095);
    for (const auto page : {first, last}) {
        if (!ShareGuestProtectionPages(address, size, page, 4096) || saved.contains(page)) continue;
        const auto begin = std::max(page, image), end = std::min(page + 4096, image + bytes);
        std::vector<std::byte> snapshot(size_t(end - begin));
        if (!read(begin, std::span<std::byte>(snapshot)))
            throw std::runtime_error("Independent image boundary lost its physical backing");
        saved.emplace(page, std::move(snapshot));
        added = true;
    }
    return added;
}

template <class Read, class Protect>
std::vector<uint64_t> ValidateGuestImageBoundary(GuestImageBoundaryPages& saved, uint64_t image,
                                                Read&& read, Protect&& protect) {
    // Restore every guard before reading B: concurrent CPU stores must fault
    // and wait for the renderer lock, closing the read/reprotect race.
    for (const auto& [page, snapshot] : saved)
        if (!protect(page, 4096))
            throw std::runtime_error("Cannot restore an independent image boundary witness");
    std::vector<uint64_t> changed;
    for (const auto& [page, snapshot] : saved) {
        std::vector<std::byte> current(snapshot.size());
        if (!read(std::max(page, image), std::span<std::byte>(current)))
            throw std::runtime_error("Independent image boundary validation lost backing");
        if (current != snapshot) changed.push_back(page);
    }
    saved.clear();
    return changed;
}
} // namespace NativeDX12
