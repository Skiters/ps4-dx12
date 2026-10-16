// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "video_core/amdgpu/regs.h"
#include <array>
namespace NativeDX12 {
struct GuestViewports {
    std::vector<D3D12_VIEWPORT> viewports;
    std::vector<D3D12_RECT> scissors;
    uint32_t flip_mask{};
    std::array<std::array<float,4>,16> clip_transforms{};
};
GuestViewports BuildGuestViewports(const AmdGpu::Regs&);
std::vector<D3D12_VIEWPORT> BuildGuestPointViewports(std::span<const D3D12_VIEWPORT>);
}
