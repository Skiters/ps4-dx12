// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
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

template <class Pages = GuestImageBoundaryPages>
struct GuestImageBoundaryPlane {
    uint64_t address, bytes;
    Pages* saved;
};

// Each physical plane has its own bounded equality witnesses. Sharing a host
// page does not make depth's saved byte segment a witness for stencil's bytes.
template <class Pages, class Visit>
void VisitGuestImageProtectedRanges(const GuestImageBoundaryPlane<Pages>& plane, Visit&& visit) {
    if (!plane.bytes) return;
    const auto pages = GuestProtectionPages(plane.address, plane.bytes);
    if (!pages) throw std::runtime_error("Invalid image plane protection envelope");
    auto cursor = pages->address;
    for (const auto& [page, snapshot] : *plane.saved) {
        if (page < cursor || page >= pages->address + pages->size || page % 4096)
            throw std::runtime_error("Invalid image plane boundary witness");
        if (cursor < page) visit(cursor, page - cursor);
        cursor = page + 4096;
    }
    if (cursor < pages->address + pages->size)
        visit(cursor, pages->address + pages->size - cursor);
}

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

template <class Planes, class Read, class Protect>
std::vector<uint64_t> ValidateGuestImageBoundaries(Planes&& planes, Read&& read, Protect&& protect) {
    // Restore every guard before reading B: concurrent CPU stores must fault
    // and wait for the renderer lock, closing the read/reprotect race. Rearm
    // ALL planes before any reads; keep all proofs on a protection/read error.
    std::vector<uint64_t> pages;
    for (const auto& plane : planes) if (plane.bytes)
        for (const auto& [page, snapshot] : *plane.saved) pages.push_back(page);
    std::sort(pages.begin(), pages.end());
    pages.erase(std::unique(pages.begin(), pages.end()), pages.end());
    for (const auto page : pages)
        if (!protect(page, 4096))
            throw std::runtime_error("Cannot restore an independent image boundary witness");
    std::vector<uint64_t> changed;
    for (const auto& plane : planes) if (plane.bytes)
        for (const auto& [page, snapshot] : *plane.saved) {
            std::vector<std::byte> current(snapshot.size());
            if (!read(std::max(page, plane.address), std::span<std::byte>(current)))
                throw std::runtime_error("Independent image boundary validation lost backing");
            if (current != snapshot) changed.push_back(page);
        }
    for (const auto& plane : planes) if (plane.bytes) plane.saved->clear();
    std::sort(changed.begin(), changed.end());
    changed.erase(std::unique(changed.begin(), changed.end()), changed.end());
    return changed;
}

template <class Read, class Protect>
std::vector<uint64_t> ValidateGuestImageBoundary(GuestImageBoundaryPages& saved, uint64_t image,
                                                Read&& read, Protect&& protect) {
    // Legacy one-plane callers keep their original segment extent in saved.
    const std::array planes{GuestImageBoundaryPlane{image, uint64_t{1}, &saved}};
    return ValidateGuestImageBoundaries(planes, read, protect);
}
} // namespace NativeDX12
