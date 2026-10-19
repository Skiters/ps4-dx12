// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <stdexcept>

#include "video_core/amdgpu/owned_cmd_buffer.h"

int main() {
    std::array<u32, 5> first_source{0xc0016900, 0x292, 0, 0xc0016900, 0x293};
    std::array<u32, 3> first_ccb{0xc0011000, 7, 9};
    auto first = AmdGpu::OwnCmdBuffers(first_source, first_ccb);

    first_source.fill(0x1e9);
    first_ccb.fill(0x8);
    const std::array<u32, 5> expected_first{0xc0016900, 0x292, 0, 0xc0016900, 0x293};
    const std::array<u32, 3> expected_ccb{0xc0011000, 7, 9};
    if (!std::ranges::equal(first.dcb, expected_first) ||
        !std::ranges::equal(first.ccb, expected_ccb)) {
        throw std::runtime_error("owned command buffer changed with guest storage");
    }

    const std::array<u32, 4> second_source{0xc0026900, 0xb4, 0, 0x3f800000};
    auto second = AmdGpu::OwnCmdBuffers(second_source, {});
    auto first_owner = std::move(first.storage);
    second.storage.reset();
    if (!first_owner || !std::ranges::equal(first.dcb, expected_first) || !second.ccb.empty()) {
        throw std::runtime_error("command buffer ownership is shared across submissions");
    }
    return 0;
}
