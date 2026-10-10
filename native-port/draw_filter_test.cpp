// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include <cstdio>
using namespace NativeDX12;
int main() {
    using Mode = AmdGpu::ColorControl::OperationMode;
    using Op = GuestDrawOperation;
    AmdGpu::Regs regs{};
    regs.primitive_type = AmdGpu::PrimitiveType::None;
    regs.depth_render_override.force_z_dirty = true;
    regs.depth_render_override.force_z_valid = true;
    regs.depth_render_override.force_stencil_dirty = true;
    regs.depth_render_override.force_stencil_valid = true;
    // Expected decisions follow the pinned upstream FilterDraw ordering.
    // No-primitive draws must stop even when depth override flags remain set.
    const struct { Mode mode; Op expected; } cases[]{
        {Mode::Disable, Op::Skip}, {Mode::Normal, Op::Skip},
        {Mode::EliminateFastClear, Op::FastClear},
        {Mode::FmaskDecompress, Op::FastClear}, {Mode::Resolve, Op::Resolve}};
    for (const auto& test : cases) {
        regs.color_control.mode = test.mode;
        if (ClassifyGuestDraw(regs) != test.expected) return 1;
    }
    regs.primitive_type = AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode = Mode::Disable;
    if (ClassifyGuestDraw(regs) != Op::Rasterize) return 2;
    regs.color_control.mode = Mode::Normal;
    if (ClassifyGuestDraw(regs) != Op::Rasterize) return 3;
    puts("PASS: Vulkan special-operation and no-primitive filter precedence");
}
