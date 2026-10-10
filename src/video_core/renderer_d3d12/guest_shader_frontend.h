// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/guest_shader_runtime.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/specialization.h"
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace NativeDX12 {
struct GuestShaderStage {
    u64 id{};
    Shader::Info info;
    Shader::RuntimeInfo runtime{};
    Shader::Backend::Bindings bindings_start{};
    Shader::Backend::Bindings bindings_end{};
    Shader::StageSpecialization specialization;
    std::vector<u32> spirv;
};
struct GuestGraphicsStages {
    std::array<GuestShaderStage*, Shader::MaxStageTypes> stages{};
    u32 mrt_mask{};
};
// Owns original guest reflection and specialization. Stage objects have stable
// addresses because StageSpecialization retains an Info pointer.
// GPU command thread only; outputs remain owned by this frontend.
class GuestShaderFrontend {
    Shader::Profile profile_;
    Shader::Pools pools_;
    using Key = std::tuple<u64, Shader::HwStage, Shader::SwStage>;
    std::map<Key, std::vector<std::unique_ptr<GuestShaderStage>>> programs_;
    std::map<std::vector<u32>, std::unique_ptr<GuestShaderStage>> auxiliary_;
    u64 next_id_ = 1;
    GuestShaderStage& Translate(const AmdGpu::Regs&, const AmdGpu::ComputeProgram*, Shader::HwStage, Shader::SwStage,
        Shader::ShaderParams, const VideoCore::GuestRuntimeContext&, Shader::Backend::Bindings&,
        std::span<const Shader::NativeImageWrite> writes = {});
public:
    explicit GuestShaderFrontend(const Shader::Profile& profile) : profile_(profile) {}
    GuestGraphicsStages BuildGraphics(AmdGpu::Liverpool&, u16 vertex_sgpr_offset = 0,
        u32 instance_sgpr_offset = 0);
    GuestGraphicsStages BuildGraphics(const AmdGpu::Regs&, u16 vertex_sgpr_offset = 0,
        u32 instance_sgpr_offset = 0);
    GuestShaderStage& BuildCompute(AmdGpu::Liverpool&);
};
}
