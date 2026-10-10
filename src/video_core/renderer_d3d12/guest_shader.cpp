// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_shader.h"
#include "shader_recompiler/runtime_info.h"

namespace NativeDX12 {
Shader::Backend::DX12::SpirvConvertResult CompileGuestShader(
    std::span<const u32> code, Shader::Pools& pools, Shader::Info& info,
    Shader::RuntimeInfo& runtime, const Shader::Profile& profile,
    Shader::Backend::Bindings& bindings, const std::wstring& converter_dll,
    const Shader::Backend::DX12::SpirvConvertOptions& options) {
    if (code.empty()) {
        Shader::Backend::DX12::SpirvConvertResult failure;
        failure.diagnostics = "Empty guest shader";
        return failure;
    }
    auto candidate_bindings = bindings;
    auto stage_options = options;
    using Stage = Shader::Backend::DX12::SpirvStage;
    switch (runtime.sw_stage) {
    case Shader::SwStage::Vertex: stage_options.stage = Stage::Vertex; break;
    case Shader::SwStage::Fragment: stage_options.stage = Stage::Pixel; break;
    case Shader::SwStage::Compute: stage_options.stage = Stage::Compute; break;
    case Shader::SwStage::Geometry: stage_options.stage = Stage::Geometry; break;
    case Shader::SwStage::TessellationControl: stage_options.stage = Stage::TessellationControl; break;
    case Shader::SwStage::TessellationEval: stage_options.stage = Stage::TessellationEvaluation; break;
    default: {
        Shader::Backend::DX12::SpirvConvertResult failure;
        failure.diagnostics = "Invalid guest logical shader stage";
        return failure;
    }
    }
    const auto program = Shader::TranslateProgram(code, pools, info, runtime, profile);
    const auto intermediate = Shader::Backend::SPIRV::EmitSPIRV(
        profile, runtime, program, candidate_bindings);
    auto result = Shader::Backend::DX12::ConvertSpirv(converter_dll, intermediate, stage_options);
    if (result.success) {
        bindings = candidate_bindings;
    }
    return result;
}
}
