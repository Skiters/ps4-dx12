// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "hlsl_emitter.h"
namespace Shader::Backend::DX12 {
struct DxilResult { bool success{}; std::vector<uint8_t> bytecode; std::string diagnostics; };
// Explicit DLL path keeps this portable and avoids changing the installed SDK/driver.
DxilResult CompileDxil(const std::wstring& compiler_dll, const std::string& hlsl, Stage stage);
}
