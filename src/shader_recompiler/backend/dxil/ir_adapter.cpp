// SPDX-License-Identifier: GPL-2.0-or-later
#include "ir_adapter.h"
#include "shader_recompiler/ir/program.h"
#include <bit>
#include <map>
#include <stdexcept>

namespace Shader::Backend::DX12 {
HlslResult EmitProgram(const IR::Program& program, Stage stage) {
    using namespace IR;
    try {
        // Phi/branches need structured control-flow lowering, never flatten them.
        if (program.blocks.size() != 1) throw std::runtime_error("DX12: multiblock shader lowering not implemented");
        for (const auto& node : program.syntax_list)
            if (node.type != AbstractSyntaxNode::Type::Block && node.type != AbstractSyntaxNode::Type::Return)
                throw std::runtime_error("DX12: structured control flow not implemented");
        std::vector<Instruction> instructions;
        std::map<const Inst*, size_t> definitions;
        auto operand = [&](const Value& value) -> std::string {
            if (!value.IsImmediate()) {
                const auto found = definitions.find(value.Inst());
                if (found == definitions.end()) throw std::runtime_error("DX12: unresolved SSA operand");
                return "v" + std::to_string(found->second);
            }
            switch (value.Type()) {
            case Type::U1: return value.U1() ? "true" : "false";
            case Type::U32: return std::to_string(value.U32()) + "u";
            case Type::F32: return "asfloat(" + std::to_string(std::bit_cast<uint32_t>(value.F32())) + "u)";
            case Type::Attribute: return std::to_string(static_cast<unsigned>(value.Attribute()));
            default: throw std::runtime_error("DX12: unsupported immediate type");
            }
        };
        for (const auto& inst : program.blocks.front()->Instructions()) {
            Instruction lowered;
            lowered.opcode = NameOf(inst.GetOpcode());
            lowered.flags = inst.Flags<uint64_t>();
            switch (inst.Type()) {
            case Type::Void: lowered.result_type = "void"; break;
            case Type::U1: lowered.result_type = "bool"; break;
            case Type::U32: lowered.result_type = "uint"; break;
            case Type::F32: lowered.result_type = "float"; break;
            case Type::U32x2: lowered.result_type = "uint2"; break;
            case Type::U32x3: lowered.result_type = "uint3"; break;
            case Type::U32x4: lowered.result_type = "uint4"; break;
            case Type::F32x2: lowered.result_type = "float2"; break;
            case Type::F32x3: lowered.result_type = "float3"; break;
            case Type::F32x4: lowered.result_type = "float4"; break;
            default: throw std::runtime_error("DX12: unsupported IR result type");
            }
            for (size_t arg = 0; arg < inst.NumArgs(); ++arg) {
                // Attribute components/vertex index are literal metadata, not HLSL uint expressions.
                if (((lowered.opcode == "GetAttribute" || lowered.opcode == "GetAttributeU32") && arg > 0) || (lowered.opcode == "SetAttribute" && arg == 2)) {
                    if (!inst.Arg(arg).IsImmediate() || inst.Arg(arg).Type() != Type::U32) throw std::runtime_error("DX12: dynamic attribute index");
                    lowered.arguments.push_back(std::to_string(inst.Arg(arg).U32()));
                } else lowered.arguments.push_back(operand(inst.Arg(arg)));
            }
            definitions[&inst] = instructions.size();
            instructions.push_back(std::move(lowered));
        }
        return EmitStraightLine(stage, instructions);
    } catch (const std::exception& error) { return {false, {}, error.what()}; }
}
}
