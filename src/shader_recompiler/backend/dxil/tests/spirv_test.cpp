// SPDX-License-Identifier: GPL-2.0-or-later
#include "spirv_converter.h"
#include <windows.h>
#include <d3d12shader.h>
#include <dxcapi.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Shader::Backend::DX12;
using Microsoft::WRL::ComPtr;

static uint64_t RequiresFlags(const std::wstring& converter_path,
                              const std::vector<uint8_t>& bytecode) {
    const auto separator = converter_path.find_last_of(L"/\\");
    if (separator == std::wstring::npos)
        throw std::runtime_error("Converter path must include its directory");
    const auto compiler_path = converter_path.substr(0, separator + 1) + L"dxcompiler.dll";
    const HMODULE compiler = LoadLibraryExW(compiler_path.c_str(), nullptr,
                                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                                LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!compiler)
        throw std::runtime_error("Cannot load sibling DXC for reflection");
    struct CompilerOwner {
        HMODULE module;
        ~CompilerOwner() { FreeLibrary(module); }
    } owner{compiler};
    const auto create = reinterpret_cast<DxcCreateInstanceProc>(
        GetProcAddress(compiler, "DxcCreateInstance"));
    if (!create)
        throw std::runtime_error("DXC reflection entry point is missing");
    ComPtr<IDxcUtils> utils;
    if (FAILED(create(CLSID_DxcUtils, IID_PPV_ARGS(&utils))))
        throw std::runtime_error("Cannot create DXC reflection utilities");
    const DxcBuffer buffer{bytecode.data(), bytecode.size(), DXC_CP_ACP};
    ComPtr<ID3D12ShaderReflection> reflection;
    if (FAILED(utils->CreateReflection(&buffer, IID_PPV_ARGS(&reflection))))
        throw std::runtime_error("Cannot reflect converted DXIL");
    return reflection->GetRequiresFlags();
}

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
        if(argc>=5 && std::wstring_view{argv[4]}==L"check-rov") {
            if(options.stage!=SpirvStage::Pixel)
                throw std::runtime_error("ROV metadata fixture must be a pixel shader");
            const auto converter=std::wstring{argv[1]};
            const auto normal=ConvertSpirv(converter,words,options);
            if(!normal.success)throw std::runtime_error("Baseline ROV fixture conversion failed: "+normal.diagnostics);
            if(RequiresFlags(converter,normal.bytecode)&D3D_SHADER_REQUIRES_ROVS)
                throw std::runtime_error("Baseline shader unexpectedly requires ROVs");
            options.raster_ordered_uav_space=0;
            options.raster_ordered_uav_register=1;
            options.raster_ordered_uav_count=1;
            const auto mismatch=ConvertSpirv(converter,words,options);
            if(!mismatch.success)throw std::runtime_error("Mismatched ROV selector conversion failed: "+mismatch.diagnostics);
            if(RequiresFlags(converter,mismatch.bytecode)&D3D_SHADER_REQUIRES_ROVS)
                throw std::runtime_error("Mismatched UAV binding unexpectedly requires ROVs");
            options.raster_ordered_uav_register=0;
            const auto ordered=ConvertSpirv(converter,words,options);
            if(!ordered.success)throw std::runtime_error("Selected ROV conversion failed: "+ordered.diagnostics);
            if(!(RequiresFlags(converter,ordered.bytecode)&D3D_SHADER_REQUIRES_ROVS))
                throw std::runtime_error("Selected UAV binding lacks the DXIL ROV requirement");
            options.raster_ordered_uav_count=0;
            const auto restored=ConvertSpirv(converter,words,options);
            if(!restored.success)throw std::runtime_error("Post-ROV baseline conversion failed: "+restored.diagnostics);
            if(RequiresFlags(converter,restored.bytecode)&D3D_SHADER_REQUIRES_ROVS)
                throw std::runtime_error("ROV selector leaked into the next compilation");
            options.raster_ordered_uav_count=1;
            options.stage=SpirvStage::Vertex;
            const auto invalid_stage=ConvertSpirv(converter,words,options);
            if(invalid_stage.success || invalid_stage.diagnostics.find("require a pixel shader")==std::string::npos)
                throw std::runtime_error("Non-pixel ROV selector was not rejected before compilation");
            std::cout << "PASS: exact UAV selector controls DXIL ROV metadata and rejects non-pixel use\n";
            return 0;
        }
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
