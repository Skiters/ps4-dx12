// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Shader::Backend::DX12 {
enum class Stage { Vertex, Pixel };
// Operand text is produced only by the IR adapter, never from guest source text.
struct Instruction {
    std::string opcode;
    std::string result_type;
    std::vector<std::string> arguments;
    uint64_t flags{};
};
struct HlslResult {
    bool supported{};
    std::string source;
    std::string error;
};
HlslResult EmitStraightLine(Stage stage, const std::vector<Instruction>& instructions);
}
