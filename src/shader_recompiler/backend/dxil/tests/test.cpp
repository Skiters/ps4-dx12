// SPDX-License-Identifier: GPL-2.0-or-later
#include "hlsl_emitter.h"
#include "dxil_compiler.h"
#include <fstream>
#include <iostream>
using namespace Shader::Backend::DX12;
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    std::vector<Instruction> pixel{
        {"GetAttribute", "float", {"32", "0", "0"}},
        {"FPMul32", "float", {"v0", "0.5f"}},
        {"FPAdd32", "float", {"v1", "0.25f"}},
        {"SetAttribute", "void", {"0", "v2", "0"}},
        {"SetAttribute", "void", {"0", "0.5f", "1"}},
        {"SetAttribute", "void", {"0", "0.25f", "2"}},
        {"SetAttribute", "void", {"0", "1.0f", "3"}}};
    auto ps = EmitStraightLine(Stage::Pixel, pixel);
    if (!ps.supported) { std::cerr << ps.error; return 1; }
    auto compiled_ps = CompileDxil(argv[1], ps.source, Stage::Pixel);
    if (!compiled_ps.success) { std::cerr << compiled_ps.diagnostics; return 1; }
    std::vector<Instruction> vertex{
        {"GetAttributeU32", "uint", {"68", "0"}},
        {"IEqual32", "bool", {"v0", "1u"}},
        {"IEqual32", "bool", {"v0", "2u"}},
        {"SelectF32", "float", {"v1", "3.0f", "-1.0f"}},
        {"SelectF32", "float", {"v2", "3.0f", "-1.0f"}},
        {"SetAttribute", "void", {"12", "v3", "0"}},
        {"SetAttribute", "void", {"12", "v4", "1"}},
        {"SetAttribute", "void", {"12", "0.0f", "2"}},
        {"SetAttribute", "void", {"12", "1.0f", "3"}},
        {"SetAttribute", "void", {"32", "1.0f", "0"}}};
    auto vs = EmitStraightLine(Stage::Vertex, vertex);
    auto compiled_vs = CompileDxil(argv[1], vs.source, Stage::Vertex);
    if (!vs.supported || !compiled_vs.success) { std::cerr << vs.error << compiled_vs.diagnostics; return 1; }
    if (EmitStraightLine(Stage::Pixel, {{"ImageSampleImplicitLod", "float4", {}}}).supported) return 3;
    if (EmitStraightLine(Stage::Pixel, {{"Phi", "float", {}}}).supported) return 4;
    if (EmitStraightLine(Stage::Pixel, {{"FPAdd32", "float", {"1.0f", "2.0f"}, 1}}).supported) return 5;
    if (EmitStraightLine(Stage::Vertex, pixel).supported) return 6;
    std::ofstream("pixel.hlsl") << ps.source;
    std::ofstream("vertex.hlsl") << vs.source;
    std::ofstream("pixel.dxil", std::ios::binary).write(reinterpret_cast<const char*>(compiled_ps.bytecode.data()), compiled_ps.bytecode.size());
    std::ofstream("vertex.dxil", std::ios::binary).write(reinterpret_cast<const char*>(compiled_vs.bytecode.data()), compiled_vs.bytecode.size());
    std::cout << "PASS: pixel DXIL " << compiled_ps.bytecode.size() << " bytes, vertex DXIL " << compiled_vs.bytecode.size() << " bytes; unsupported resource/phi/flags/stage rejected\n";
    return 0;
}
