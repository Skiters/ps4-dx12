// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <fmt/format.h>
#include "common/types.h"
#include "shader_recompiler/runtime_info.h"

namespace Shader {

inline std::string GetShaderName(HwStage stage, u64 hash, std::optional<size_t> perm = {}) {
    if (perm) {
        return fmt::format("{}_{:#018x}_{}", stage, hash, *perm);
    }
    return fmt::format("{}_{:#018x}", stage, hash);
}

} // namespace Shader
