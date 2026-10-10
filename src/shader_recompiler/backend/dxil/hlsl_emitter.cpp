// SPDX-License-Identifier: GPL-2.0-or-later
#include "hlsl_emitter.h"
#include <array>
#include <map>
#include <stdexcept>
#include <string_view>

namespace Shader::Backend::DX12 {
HlslResult EmitStraightLine(Stage stage, const std::vector<Instruction>& instructions) {
    HlslResult result;
    std::string body;
    std::map<unsigned, bool> inputs, outputs;
    bool position = false;
    try {
        for (size_t index = 0; index < instructions.size(); ++index) {
            const auto& inst = instructions[index];
            const auto& op = inst.opcode;
            auto a = [&](size_t n) -> const std::string& { return inst.arguments.at(n); };
            auto integer = [&](size_t n) {
                size_t end = 0;
                unsigned value = static_cast<unsigned>(std::stoul(a(n), &end));
                if (end != a(n).size()) throw std::runtime_error("nonconstant attribute index");
                return value;
            };
            if (inst.flags) throw std::runtime_error("instruction flags require lowering: " + op);
            if (op == "Prologue" || op == "Epilogue" || op == "Void") continue;
            if (op == "Discard") { if (stage != Stage::Pixel) throw std::runtime_error("vertex discard"); body += "discard;\n"; continue; }
            if (op == "DiscardCond") { if (stage != Stage::Pixel) throw std::runtime_error("vertex discard"); body += "if(" + a(0) + ") discard;\n"; continue; }
            std::string expression;
            const std::map<std::string, std::string> binary{
                {"IAdd32", "+"}, {"ISub32", "-"}, {"IMul32", "*"}, {"IEqual32", "=="}, {"INotEqual32", "!="},
                {"FPAdd32", "+"}, {"FPSub32", "-"}, {"FPMul32", "*"}, {"FPDiv32", "/"},
                {"BitwiseAnd32", "&"}, {"BitwiseOr32", "|"}, {"BitwiseXor32", "^"},
                {"LogicalAnd", "&&"}, {"LogicalOr", "||"}, {"LogicalXor", "!="}};
            const std::map<std::string, std::string> unary{
                {"FPAbs32", "abs"}, {"FPSqrt", "sqrt"}, {"FPRecipSqrt32", "rsqrt"},
                {"FPExp2", "exp2"}, {"FPLog2", "log2"}, {"FPFloor32", "floor"},
                {"FPCeil32", "ceil"}, {"FPTrunc32", "trunc"},
                {"BitCastU32F32", "asuint"}, {"BitCastF32U32", "asfloat"},
                {"FPFract32", "frac"}, {"FPSaturate32", "saturate"}};
            if (auto found = binary.find(op); found != binary.end()) expression = "(" + a(0) + found->second + a(1) + ")";
            else if (auto unary_found = unary.find(op); unary_found != unary.end()) expression = unary_found->second + "(" + a(0) + ")";
            else if (op == "FPNeg32") expression = "(-" + a(0) + ")";
            else if (op == "FPRecip32") expression = "(1.0f/" + a(0) + ")";
            else if (op == "FPMin32" || op == "FPMax32") expression = std::string(op == "FPMin32" ? "min(" : "max(") + a(0) + "," + a(1) + ")";
            else if (op == "SelectU1" || op == "SelectU32" || op == "SelectF32") expression = "(" + a(0) + "?" + a(1) + ":" + a(2) + ")";
            else if (op == "GetAttributeU32") {
                if (stage != Stage::Vertex || integer(0) != 68 || integer(1) != 0)
                    throw std::runtime_error("unsupported integer input attribute");
                expression = "input.vertexId";
            } else if (op == "GetAttribute") {
                unsigned attr = integer(0), component = integer(1);
                if (stage != Stage::Pixel || attr < 32 || attr > 63 || component > 3 || integer(2) != 0) throw std::runtime_error("unsupported input attribute");
                inputs[attr - 32] = true;
                expression = "input.p" + std::to_string(attr - 32) + "[" + std::to_string(component) + "]";
            } else if (op == "SetAttribute") {
                unsigned attr = integer(0), component = integer(2);
                if (component > 3) throw std::runtime_error("attribute component out of range");
                std::string target;
                if (stage == Stage::Pixel && attr <= 7) { outputs[attr] = true; target = "output.c" + std::to_string(attr); }
                else if (stage == Stage::Vertex && attr == 12) { position = true; target = "output.position"; }
                else if (stage == Stage::Vertex && attr >= 32 && attr <= 63) { outputs[attr - 32] = true; target = "output.p" + std::to_string(attr - 32); }
                else throw std::runtime_error("unsupported output attribute");
                body += target + "[" + std::to_string(component) + "]=" + a(1) + ";\n";
                continue;
            } else if (op.starts_with("CompositeConstruct") && (inst.result_type == "float2" || inst.result_type == "float3" || inst.result_type == "float4" || inst.result_type == "uint2" || inst.result_type == "uint3" || inst.result_type == "uint4")) {
                if (inst.arguments.size() != static_cast<size_t>(inst.result_type.back() - '0')) throw std::runtime_error("unsupported composite constructor");
                expression = inst.result_type + "(";
                for (size_t n = 0; n < inst.arguments.size(); ++n) { if(n) expression += ","; expression += a(n); }
                expression += ")";
            } else if (op.starts_with("CompositeExtract")) expression = a(0) + "[" + a(1) + "]";
            else throw std::runtime_error("unsupported IR opcode: " + op + " at " + std::to_string(index));
            static constexpr std::array<std::string_view, 9> types{"bool", "uint", "float", "uint2", "uint3", "uint4", "float2", "float3", "float4"};
            bool valid_type = false;
            for (auto type : types) valid_type |= inst.result_type == type;
            if (!valid_type) throw std::runtime_error("unsupported result type: " + inst.result_type);
            body += "precise " + inst.result_type + " v" + std::to_string(index) + "=" + expression + ";\n";
        }
        std::string source = "struct Input {\n";
        // Always use a real system input so empty structs are never emitted.
        source += stage == Stage::Pixel ? "float4 position:SV_Position;\n" : "uint vertexId:SV_VertexID;\n";
        for (auto [slot, used] : inputs) source += "float4 p" + std::to_string(slot) + ":TEXCOORD" + std::to_string(slot) + ";\n";
        source += "};\nstruct Output {\n";
        if (stage == Stage::Vertex) { if (!position) throw std::runtime_error("vertex shader exports no position"); source += "float4 position:SV_Position;\n"; }
        if (stage == Stage::Pixel && outputs.empty()) throw std::runtime_error("pixel shader exports no render target");
        for (auto [slot, used] : outputs) source += "float4 " + std::string(stage == Stage::Pixel ? "c" : "p") + std::to_string(slot) + ":" + std::string(stage == Stage::Pixel ? "SV_Target" : "TEXCOORD") + std::to_string(slot) + ";\n";
        source += "};\nOutput main(Input input) { Output output=(Output)0;\n" + body + "return output;\n}\n";
        result.supported = true;
        result.source = std::move(source);
    } catch (const std::exception& error) { result.error = error.what(); }
    return result;
}
}
