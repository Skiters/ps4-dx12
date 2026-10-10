// SPDX-License-Identifier: GPL-2.0-or-later
#include "ir_adapter.h"
#include "dxil_compiler.h"
#include "shader_recompiler/ir/program.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Shader;
using namespace Shader::IR;
using namespace Shader::Backend::DX12;

struct Fixture {
    // Destruction order keeps instructions alive while the intrusive block list exists.
    Common::ObjectPool<Inst> instructions{64};
    Common::ObjectPool<Block> blocks{4};
    Info info;
    Program program{info};
    Block* block = blocks.Create(instructions);
    Fixture() {
        program.blocks.push_back(block);
        AbstractSyntaxNode node{}; node.type = AbstractSyntaxNode::Type::Block; node.data.block = block;
        program.syntax_list.push_back(node);
        AbstractSyntaxNode ret{}; ret.type = AbstractSyntaxNode::Type::Return; program.syntax_list.push_back(ret);
    }
    Value Add(Opcode opcode, std::initializer_list<Value> arguments) {
        block->AppendNewInst(opcode, arguments);
        return Value{&block->back()};
    }
    void Export(Attribute attribute, Value value, uint32_t component) {
        Add(Opcode::SetAttribute, {Value{attribute}, value, Value{component}});
    }
};
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void Save(const char* path, const DxilResult& dxil) {
    std::ofstream stream(path,std::ios::binary);
    stream.write(reinterpret_cast<const char*>(dxil.bytecode.data()),dxil.bytecode.size());
    Check(static_cast<bool>(stream),"Cannot write DXIL fixture");
}
int wmain(int argc, wchar_t** argv) {
    try {
        Check(argc == 2,"Pass absolute dxcompiler.dll path");
        Fixture vertex;
        auto id = vertex.Add(Opcode::GetAttributeU32,{Value{Attribute::VertexId},Value{uint32_t{0}}});
        auto is_one = vertex.Add(Opcode::IEqual32,{id,Value{uint32_t{1}}});
        auto is_two = vertex.Add(Opcode::IEqual32,{id,Value{uint32_t{2}}});
        auto x = vertex.Add(Opcode::SelectF32,{is_one,Value{3.0f},Value{-1.0f}});
        auto y = vertex.Add(Opcode::SelectF32,{is_two,Value{3.0f},Value{-1.0f}});
        vertex.Export(Attribute::Position0,x,0); vertex.Export(Attribute::Position0,y,1);
        vertex.Export(Attribute::Position0,Value{0.0f},2); vertex.Export(Attribute::Position0,Value{1.0f},3);
        vertex.Export(Attribute::Param0,Value{1.0f},0);
        auto vh = EmitProgram(vertex.program,Stage::Vertex); Check(vh.supported,vh.error.c_str());
        auto vs = CompileDxil(argv[1],vh.source,Stage::Vertex); Check(vs.success,vs.diagnostics.c_str());
        Fixture pixel;
        auto input = pixel.Add(Opcode::GetAttribute,{Value{Attribute::Param0},Value{uint32_t{0}},Value{uint32_t{0}}});
        auto half = pixel.Add(Opcode::FPMul32,{input,Value{0.5f}});
        auto color = pixel.Add(Opcode::FPAdd32,{half,Value{0.25f}});
        pixel.Export(Attribute::RenderTarget0,color,0); pixel.Export(Attribute::RenderTarget0,Value{0.5f},1);
        pixel.Export(Attribute::RenderTarget0,Value{0.25f},2); pixel.Export(Attribute::RenderTarget0,Value{1.0f},3);
        auto ph = EmitProgram(pixel.program,Stage::Pixel); Check(ph.supported,ph.error.c_str());
        auto ps = CompileDxil(argv[1],ph.source,Stage::Pixel); Check(ps.success,ps.diagnostics.c_str());
        // Must reject control flow rather than flattening it into a different program.
        AbstractSyntaxNode branch{}; branch.type = AbstractSyntaxNode::Type::If;
        pixel.program.syntax_list.insert(pixel.program.syntax_list.begin(),branch);
        Check(!EmitProgram(pixel.program,Stage::Pixel).supported,"Structured branch was silently flattened");
        pixel.program.syntax_list.erase(pixel.program.syntax_list.begin());
        pixel.program.blocks.push_back(pixel.blocks.Create(pixel.instructions));
        Check(!EmitProgram(pixel.program,Stage::Pixel).supported,"Multiple blocks were silently flattened");
        Save("vertex.dxil",vs); Save("pixel.dxil",ps);
        std::ofstream("vertex.hlsl") << vh.source; std::ofstream("pixel.hlsl") << ph.source;
        std::cout << "PASS: original shadPS4 IR Block/Inst/Value -> EmitProgram -> real VS/PS DXIL; structured CFG and multiblock rejected\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << "\n"; return 1; }
}
