// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include "core/backing_write_plan.h"

namespace {

struct PhysicalArea {
    PAddr base;
    u64 size;
};

struct VirtualArea {
    VAddr base;
    u64 size;
    std::map<u64, PhysicalArea> phys_areas;
    bool backed = true;
};

using VirtualMap = std::map<VAddr, VirtualArea>;

void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

auto Plan(const VirtualMap& areas, VAddr address, u64 size) {
    return Core::PlanBackingWrite(areas, address, size,
                                  [](const VirtualArea& area) { return area.backed; });
}

void RequireSpan(const Core::BackingWriteSpan& span, PAddr physical, u64 source, u64 size) {
    Require(span.physical_offset == physical && span.source_offset == source && span.size == size,
            "Incorrect physical address, source offset or span extent");
}

void Apply(const std::vector<Core::BackingWriteSpan>& plan, std::array<u8, 96>& backing,
           const std::array<u8, 16>& source) {
    for (const auto& span : plan) {
        Require(span.physical_offset <= backing.size() &&
                    span.size <= backing.size() - span.physical_offset &&
                    span.source_offset <= source.size() &&
                    span.size <= source.size() - span.source_offset,
                "Fixture span outside buffers");
        std::memcpy(backing.data() + span.physical_offset, source.data() + span.source_offset,
                    span.size);
    }
}

void TestSplitWrites() {
    const VirtualMap areas{
        {0x2000, {0x2000, 8, {{0, {32, 4}}, {4, {16, 4}}}}},
        {0x2008, {0x2008, 8, {{0, {48, 8}}}}},
    };
    const auto plan = Plan(areas, 0x2002, 10);
    Require(plan && plan->size() == 3, "Split VMA/physical range was not fully planned");
    RequireSpan((*plan)[0], 34, 0, 2);
    RequireSpan((*plan)[1], 16, 2, 4);
    RequireSpan((*plan)[2], 48, 6, 4);

    std::array<u8, 16> source{};
    for (u8 i = 0; i < source.size(); ++i) {
        source[i] = 0x40 + i;
    }
    std::array<u8, 96> backing;
    backing.fill(0xcd);
    auto expected = backing;
    std::memcpy(expected.data() + 34, source.data(), 2);
    std::memcpy(expected.data() + 16, source.data() + 2, 4);
    std::memcpy(expected.data() + 48, source.data() + 6, 4);
    Apply(*plan, backing, source);
    Require(backing == expected, "Split write changed padding or lost source progression");

    const auto final_boundary = Plan(areas, 0x200c, 4);
    Require(final_boundary && final_boundary->size() == 1, "Exact final VMA boundary rejected");
    RequireSpan(final_boundary->front(), 52, 0, 4);
}

void TestAliases() {
    const VirtualMap areas{
        {0x3000, {0x3000, 4, {{0, {8, 4}}}}},
        {0x3004, {0x3004, 4, {{0, {8, 4}}}}},
    };
    const auto plan = Plan(areas, 0x3000, 8);
    Require(plan && plan->size() == 2, "Physical aliases were dropped or coalesced");
    RequireSpan((*plan)[0], 8, 0, 4);
    RequireSpan((*plan)[1], 8, 4, 4);
    std::array<u8, 96> backing{};
    const std::array<u8, 16> source{1, 2, 3, 4, 5, 6, 7, 8};
    Apply(*plan, backing, source);
    Require(backing[8] == 5 && backing[9] == 6 && backing[10] == 7 && backing[11] == 8,
            "Overlapping physical aliases did not retain virtual traversal order");
    Require(backing[7] == 0 && backing[12] == 0, "Alias write changed surrounding physical bytes");
}

void RequireRejected(const VirtualMap& areas, VAddr address, u64 size) {
    // A failed plan must not expose a writable prefix to its caller.
    const auto plan = Plan(areas, address, size);
    Require(!plan, "Incomplete or overflowing backing write accepted");
}

void TestRejectedRanges() {
    VirtualMap areas{{0x1000, {0x1000, 8, {{0, {8, 8}}}}},
                     {0x1008, {0x1008, 8, {}, false}}};
    RequireRejected(areas, 0x1004, 8); // Backed prefix followed by Free/Reserved VMA.
    RequireRejected(areas, 0x1008, 1);
    areas.erase(0x1008);
    RequireRejected(areas, 0x1004, 5); // Ends beyond the final VMA.
    RequireRejected(areas, 0x0fff, 1);
    areas.emplace(0x100c, VirtualArea{0x100c, 4, {{0, {32, 4}}}});
    RequireRejected(areas, 0x1004, 12); // Virtual gap after an otherwise valid prefix.

    areas = {{0x1000, {0x1000, 8, {}}}};
    RequireRejected(areas, 0x1000, 1);
    areas.begin()->second.phys_areas = {{2, {8, 6}}};
    RequireRejected(areas, 0x1000, 4); // Missing physical prefix.
    areas.begin()->second.phys_areas = {{0, {8, 4}}, {6, {32, 2}}};
    RequireRejected(areas, 0x1000, 8); // Physical gap after a valid prefix.
    areas.begin()->second.phys_areas = {{0, {8, 4}}};
    RequireRejected(areas, 0x1000, 8); // Missing physical suffix.
    areas.begin()->second.phys_areas = {{0, {8, 0}}};
    RequireRejected(areas, 0x1000, 1);
    RequireRejected({}, 0x1000, 1);

    constexpr auto max_virtual = std::numeric_limits<VAddr>::max();
    constexpr auto max_physical = std::numeric_limits<PAddr>::max();
    RequireRejected(areas, max_virtual - 3, 4);
    RequireRejected({{max_virtual - 7, {max_virtual - 7, 16, {{0, {0, 16}}}}}},
                    max_virtual - 7, 1);
    RequireRejected({{0x1000, {0x1000, 4, {{0, {max_physical - 1, 4}}}}}}, 0x1000, 1);
    RequireRejected({{0, {0, max_virtual, {{max_virtual - 3, {0, 8}}}}}}, max_virtual - 3,
                    1);

    const auto empty = Plan({}, max_virtual, 0);
    Require(empty && empty->empty(), "Zero-byte plan should contain no accesses");
}

} // namespace

int main() {
    try {
        TestSplitWrites();
        TestAliases();
        TestRejectedRanges();
        std::puts("PASS: complete backing-write planning, physical aliases and rejected gaps/overflow");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
