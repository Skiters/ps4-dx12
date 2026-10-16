// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shader_recompiler/runtime_info.h"
#include "shader_recompiler/profile.h"
#include <array>
namespace AmdGpu { struct Liverpool; union Regs; struct ComputeProgram; }
namespace Shader { struct Info; }
namespace VideoCore {
struct GuestRuntimeContext {
    const Shader::Profile& profile;
    std::array<const Shader::Info*, Shader::MaxStageTypes> stage_infos{};
    std::array<Shader::PsColorBuffer, Shader::MaxColorBuffers> color_buffers{};
    struct { u16 vertex_sgpr_offset{}; u32 instance_sgpr_offset{}; bool is_indirect{}; } indirect;
    bool depth_clip_control{};
};
// Reuses original guest register interpretation without referencing a host API.
Shader::RuntimeInfo BuildGuestRuntimeInfo(const AmdGpu::Regs&, const AmdGpu::ComputeProgram*,
    Shader::HwStage, Shader::SwStage, const GuestRuntimeContext&);
Shader::RuntimeInfo BuildGuestRuntimeInfo(AmdGpu::Liverpool&, Shader::HwStage,
    Shader::SwStage, const GuestRuntimeContext&);
}
