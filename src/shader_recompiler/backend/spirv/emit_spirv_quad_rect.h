// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <span>
#include "common/types.h"

namespace Shader {
struct HwFragmentRuntimeInfo;
struct NativePointOutput;
}

namespace Shader::Backend::SPIRV {

enum class AuxShaderType : u32 {
    RectListTCS,
    QuadListTCS,
    PassthroughTES,
};

[[nodiscard]] std::vector<u32> EmitAuxilaryTessShader(AuxShaderType type,
                                                      const HwFragmentRuntimeInfo& fs_info,
                                                      u32 input_vertices = 32,
                                                      std::span<const NativePointOutput> transport = {},
                                                      u32 input_location_offset = 0);

} // namespace Shader::Backend::SPIRV
