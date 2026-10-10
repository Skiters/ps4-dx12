// SPDX-License-Identifier: GPL-2.0-or-later
#include "spirv_converter.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Shader::Backend::DX12;
std::vector<uint32_t> Read(const wchar_t* filename){
    std::ifstream input(std::filesystem::path{filename},std::ios::binary|std::ios::ate);
    auto bytes=input.tellg();if(!input || bytes<=0 || bytes%4)throw std::runtime_error("Cannot read linked SPIR-V fixture");
    std::vector<uint32_t> words(static_cast<size_t>(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(words.data()),bytes);
    if(!input)throw std::runtime_error("Truncated linked fixture file");return words;
}
int wmain(int argc,wchar_t**argv){
    try{
        if(argc!=4 && argc!=5)throw std::runtime_error("Usage: linked-test converter.dll hull.spv domain.spv [output-directory]");
        auto hull=Read(argv[2]),domain=Read(argv[3]);
        // Reverse input order exercises the C API's stage ordering and output mapping.
        const LinkedSpirvStage stages[]{
            {SpirvStage::TessellationEvaluation,domain},
            {SpirvStage::TessellationControl,hull},
        };
        SpirvConvertOptions options;
        auto result=ConvertLinkedStages(argv[1],stages,options);
        if(!result.success || result.stages.size()!=2){std::cerr<<result.diagnostics;return 1;}
        for(const auto& stage:result.stages)if(!stage.success || !stage.validated || stage.bytecode.empty())throw std::runtime_error("Linked stage not validated");
        if(argc==5){
            std::filesystem::path directory{argv[4]};std::filesystem::create_directories(directory);
            for(size_t i=0;i<result.stages.size();++i){std::ofstream output(directory/(i==0?"domain.dxil":"hull.dxil"),std::ios::binary);const auto& bytes=result.stages[i].bytecode;output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!output)throw std::runtime_error("Cannot write linked DXIL output");}
        }
        auto missing=ConvertLinkedStages(argv[1],std::span{stages}.first(1),options);
        if(missing.success || !missing.stages.empty())throw std::runtime_error("Unpaired DS accepted");
        const LinkedSpirvStage duplicate[]{stages[0],stages[0]};
        auto repeated=ConvertLinkedStages(argv[1],duplicate,options);
        if(repeated.success || !repeated.stages.empty())throw std::runtime_error("Duplicate stage accepted");
        auto invalid=stages[0];const uint32_t invalid_words[]{0,0,0,0,0};invalid.words=invalid_words;
        const LinkedSpirvStage bad[]{invalid,stages[1]};auto rejected=ConvertLinkedStages(argv[1],bad,options);
        if(rejected.success || !rejected.stages.empty())throw std::runtime_error("Invalid linked stage committed partially");
        std::cout<<"PASS: linked CAPI HS+DS -> all stages signed/validated; DSbytes="<<result.stages[0].bytecode.size()<<" HSbytes="<<result.stages[1].bytecode.size()<<"; reverse-input mapping and missing/duplicate/invalid transactional rejection passed\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 2;}
}
