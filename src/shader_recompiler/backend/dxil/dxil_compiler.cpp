#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "dxil_compiler.h"
#include "common/runtime_module_trace.h"
#include <windows.h>
#include <wrl/client.h>
#include <dxcapi.h>
#include <array>
#include <cstring>

namespace Shader::Backend::DX12 {
using Microsoft::WRL::ComPtr;
static DxilResult CompileShaderProfile(const std::wstring& compiler_dll, const std::string& hlsl, LPCWSTR profile) {
    DxilResult result;
    const auto separator = compiler_dll.find_last_of(L"/\\");
    if (separator == std::wstring::npos) { result.diagnostics = "DXC path must include its directory"; return result; }
    const auto validator_path = compiler_dll.substr(0,separator + 1) + L"dxil.dll";
    const HMODULE validator = Common::RuntimeModuleTrace::Load(validator_path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!validator) { result.diagnostics = "Cannot load DXIL validator: Win32 " + std::to_string(GetLastError()); return result; }
    const HMODULE module = Common::RuntimeModuleTrace::Load(compiler_dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) { result.diagnostics = "Cannot load DXC: Win32 " + std::to_string(GetLastError()); FreeLibrary(validator); return result; }
    // COM objects must be released before the DLL is unloaded.
    {
        auto create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module, "DxcCreateInstance"));
        ComPtr<IDxcCompiler3> compiler;
        if (!create || FAILED(DX12_API_CALL(create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler))))) result.diagnostics = "DXC compiler creation failed";
        else {
            DxcBuffer buffer{hlsl.data(), hlsl.size(), DXC_CP_UTF8};
            std::array<LPCWSTR, 8> args{L"-E", L"main", L"-T", profile, L"-Ges", L"-WX", L"-O3", L"-Gis"};
            ComPtr<IDxcResult> compiled;
            HRESULT hr = DX12_API_CALL(compiler->Compile(&buffer, args.data(), static_cast<UINT32>(args.size()), nullptr, IID_PPV_ARGS(&compiled)));
            if (FAILED(hr) || !compiled) result.diagnostics = "DXC Compile call failed";
            else {
                ComPtr<IDxcBlobUtf8> errors;
                DX12_API_CALL(compiled->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
                if (errors && DX12_API_CALL(errors->GetStringLength())) result.diagnostics.assign(DX12_API_CALL(errors->GetStringPointer()), DX12_API_CALL(errors->GetStringLength()));
                HRESULT status = E_FAIL;
                DX12_API_CALL(compiled->GetStatus(&status));
                ComPtr<IDxcBlob> object;
                if (SUCCEEDED(status) && SUCCEEDED(DX12_API_CALL(compiled->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr))) && object) {
                    result.bytecode.resize(DX12_API_CALL(object->GetBufferSize()));
                    std::memcpy(result.bytecode.data(), DX12_API_CALL(object->GetBufferPointer()), DX12_API_CALL(object->GetBufferSize()));
                    result.success = true;
                }
            }
        }
    }
    FreeLibrary(module);
    FreeLibrary(validator);
    return result;
}
DxilResult CompileDxil(const std::wstring& compiler_dll,const std::string& hlsl,Stage stage){
    return CompileShaderProfile(compiler_dll,hlsl,stage==Stage::Pixel?L"ps_6_0":L"vs_6_0");
}
DxilResult CompileComputeDxil(const std::wstring& compiler_dll,const std::string& hlsl){
    return CompileShaderProfile(compiler_dll,hlsl,L"cs_6_0");
}
}
