// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "hlsl_emitter.h"
namespace Shader::IR { struct Program; }
namespace Shader::Backend::DX12 {
HlslResult EmitProgram(const IR::Program& program, Stage stage);
}
