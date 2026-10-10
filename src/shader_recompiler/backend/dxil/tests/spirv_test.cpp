// SPDX-License-Identifier: GPL-2.0-or-later
#include "spirv_converter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Shader::Backend::DX12;
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc<4 || argc>6) throw std::runtime_error("Usage: test converter.dll shader.spv stage[vertex/pixel/compute/geometry/hull/domain] [output.dxil] [expect-reject]");
        SpirvConvertOptions options;
        std::wstring stage=argv[3];
        if(stage==L"vertex") options.stage=SpirvStage::Vertex;
        else if(stage==L"pixel" || stage==L"fragment") options.stage=SpirvStage::Pixel;
        else if(stage==L"compute") options.stage=SpirvStage::Compute;
        else if(stage==L"geometry") options.stage=SpirvStage::Geometry;
        else if(stage==L"hull") options.stage=SpirvStage::TessellationControl;
        else if(stage==L"domain") options.stage=SpirvStage::TessellationEvaluation;
        else throw std::runtime_error("Unknown shader stage");
        std::ifstream input(std::filesystem::path{argv[2]},std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot read SPIR-V fixture");
        auto size=input.tellg();
        if(size<=0 || size%4 || size>std::numeric_limits<int32_t>::max()) throw std::runtime_error("Invalid SPIR-V byte size");
        std::vector<uint32_t> words(static_cast<size_t>(size)/4);
        input.seekg(0);input.read(reinterpret_cast<char*>(words.data()),size);
        if(!input) throw std::runtime_error("Cannot read complete SPIR-V fixture");
        auto result=ConvertSpirv(argv[1],words,options);
        if(argc==6 && std::wstring{argv[5]}==L"expect-reject") {
            if(result.success || !result.bytecode.empty()) throw std::runtime_error("Unsupported fixture was accepted");
            std::cout << "PASS: unsupported SPIR-V rejected; converted=" << result.converted << "\n" << result.diagnostics;
            return 0;
        }
        if(!result.success) { std::cerr << result.diagnostics; return 1; }
        if(!result.converted || !result.validated || result.bytecode.empty()) throw std::runtime_error("Incomplete compiler success");
        if(argc>=5) {
            std::ofstream output(std::filesystem::path{argv[4]},std::ios::binary);
            output.write(reinterpret_cast<const char*>(result.bytecode.data()),result.bytecode.size());
            if(!output) throw std::runtime_error("Cannot write converted shader");
        }
        std::cout << "PASS: actual SPIR-V -> signed DXIL bytes=" << result.bytecode.size()
                  << " runtime_data=" << result.requires_runtime_data << " draw_sysvals=" << result.needs_draw_sysvals
                  << " converter_version=" << result.converter_version << "\n";
        // Invalid SPIR-V must fail before compiler invocation and never return usable DXIL.
        auto invalid=ConvertSpirv(argv[1],std::vector<uint32_t>{0,0,0,0,0},options);
        if(invalid.success || !invalid.bytecode.empty()) throw std::runtime_error("Invalid module accepted");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << "\n"; return 2; }
}
