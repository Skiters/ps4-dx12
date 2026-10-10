// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/recompiler.h"

namespace NativeDX12 {
// The caller supplies the guest's actual resource metadata and feature profile.
// Binding allocation is transactional: failed DXIL conversion must not consume slots.
Shader::Backend::DX12::SpirvConvertResult CompileGuestShader(
    std::span<const u32> code, Shader::Pools& pools, Shader::Info& info,
    Shader::RuntimeInfo& runtime, const Shader::Profile& profile,
    Shader::Backend::Bindings& bindings, const std::wstring& converter_dll,
    const Shader::Backend::DX12::SpirvConvertOptions& options);
}
