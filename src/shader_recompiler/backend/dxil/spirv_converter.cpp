#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "spirv_converter.h"
#include "common/runtime_module_trace.h"
#include "native_clock_compile.h"
#include "spirv_buffer_access.h"
#include <windows.h>
#include <wrl/client.h>
#include <dxcapi.h>
#define BITFIELD_MASK(b) ((1u << (b)) - 1u)
#define BITFIELD_RANGE(b,n) (BITFIELD_MASK(n) << (b))
#include <spirv_to_dxil.h>
#undef BITFIELD_MASK
#undef BITFIELD_RANGE
#include <cstring>
#include <limits>
#include <stdexcept>
namespace Shader::Backend::DX12 {
namespace {
using Microsoft::WRL::ComPtr;
struct Module {
    HMODULE handle{};
    explicit Module(const std::wstring& path) {
        handle = Common::RuntimeModuleTrace::Load(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!handle) throw std::runtime_error("Cannot load compiler DLL, Win32 " + std::to_string(GetLastError()));
    }
    ~Module() { if(handle) FreeLibrary(handle); }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    template<class T> T Symbol(const char* name) {
        auto address = GetProcAddress(handle,name);
        if(!address) throw std::runtime_error(std::string("Missing compiler export: ") + name);
        return reinterpret_cast<T>(address);
    }
};
void AppendLog(void* data,const char* message) noexcept {
    try { if(message) { auto& log=*static_cast<std::string*>(data); log += message; log += '\n'; } } catch(...) {}
}
dxil_spirv_shader_stage StageOf(SpirvStage stage) {
    switch(stage) {
    case SpirvStage::Vertex: return DXIL_SPIRV_SHADER_VERTEX;
    case SpirvStage::TessellationControl: return DXIL_SPIRV_SHADER_TESS_CTRL;
    case SpirvStage::TessellationEvaluation: return DXIL_SPIRV_SHADER_TESS_EVAL;
    case SpirvStage::Geometry: return DXIL_SPIRV_SHADER_GEOMETRY;
    case SpirvStage::Pixel: return DXIL_SPIRV_SHADER_FRAGMENT;
    case SpirvStage::Compute: return DXIL_SPIRV_SHADER_COMPUTE;
    }
    throw std::runtime_error("Invalid shader stage");
}
dxil_spirv_sysval_type SysvalOf(SystemValueMode mode) {
    switch(mode) {
    case SystemValueMode::Zero: return DXIL_SPIRV_SYSVAL_TYPE_ZERO;
    case SystemValueMode::Native: return DXIL_SPIRV_SYSVAL_TYPE_NATIVE;
    case SystemValueMode::RuntimeData: return DXIL_SPIRV_SYSVAL_TYPE_RUNTIME_DATA;
    }
    throw std::runtime_error("Invalid system-value mode");
}
}
uint64_t ConverterNativeFeatures(const std::wstring& converter_dll){
    Module module{converter_dll};
    const auto address=GetProcAddress(module.handle,"spirv_to_dxil_native_features");
    const auto selection=GetProcAddress(module.handle,"spirv_to_dxil_set_native_clock_backend");
    return address&&selection?reinterpret_cast<uint64_t(*)()>(address)():0;
}
SpirvConvertResult ConvertSpirv(const std::wstring& converter_dll,
                              std::span<const uint32_t> words,
                              const SpirvConvertOptions& options) {
    SpirvConvertResult result;
    try {
        // Hull/domain stages require linked tessellation metadata. The baseline
        // converter also corrupts its heap on unlinked guest hull shaders.
        // Keep standalone hull conversion disabled even with the allocator fix.
        if (options.stage == SpirvStage::TessellationControl)
            throw std::runtime_error("Tessellation control requires linked hull/domain compilation; standalone conversion disabled");
        if(words.size()<5 || words[0]!=0x07230203u) throw std::runtime_error("Invalid SPIR-V module header");
        if(options.entry_point.empty() || options.entry_point.find('\0') != std::string::npos) throw std::runtime_error("Invalid entry point");
        if(options.shader_model_minor>8 || options.validator_minor>8) throw std::runtime_error("Unsupported SM/validator version request");
        if(options.specializations.size()>std::numeric_limits<unsigned>::max()) throw std::runtime_error("Too many specialization constants");
        // Verify instruction framing before passing untrusted bytecode to the compiler.
        for(size_t offset=5; offset<words.size();) {
            size_t count=words[offset]>>16;
            if(!count || count>words.size()-offset) throw std::runtime_error("Truncated SPIR-V instruction");
            offset+=count;
        }
        if(options.stage==SpirvStage::Compute && options.compute_wave_size &&
           (options.shader_model_minor<6 || (options.compute_wave_size!=32&&options.compute_wave_size!=64)))
            throw std::invalid_argument("Native forced compute subgroup requires SM6.6 and 32 or 64 lanes");
        std::vector<uint32_t> unified_words;
        if(options.native_storage_buffer_uavs){unified_words=NativeStorageBufferUavs(words);words=unified_words;}
        auto slash=converter_dll.find_last_of(L"/\\");
        if(slash==std::wstring::npos) throw std::runtime_error("Converter path must include its directory");
        auto directory=converter_dll.substr(0,slash+1);
        Module validator_module{directory+L"dxil.dll"};
        Module dxc_module{directory+L"dxcompiler.dll"};
        Module converter_module{converter_dll};
        NativeClockCompileScope clock_scope(converter_module.handle,options.native_shader_clock_backend);
        const char* entry=options.stage!=SpirvStage::Compute||!options.compute_wave_size?"spirv_to_dxil":
            options.compute_wave_size==64?"spirv_to_dxil_compute_waves64":"spirv_to_dxil_compute_waves32";
        auto convert=converter_module.Symbol<decltype(&spirv_to_dxil)>(entry);
        auto free_output=converter_module.Symbol<decltype(&spirv_to_dxil_free)>("spirv_to_dxil_free");
        auto version=converter_module.Symbol<decltype(&spirv_to_dxil_get_version)>("spirv_to_dxil_get_version");
        result.converter_version=version();
        std::vector<dxil_spirv_specialization> constants(options.specializations.size());
        for(size_t i=0;i<constants.size();++i) {
            constants[i].id=options.specializations[i].id;
            constants[i].value.u64=options.specializations[i].raw_bits;
            constants[i].defined_on_module=options.specializations[i].defined_on_module;
        }
        dxil_spirv_runtime_conf config{};
        config.runtime_data_cbv={options.runtime_cbv_space,options.runtime_cbv_register};
        config.push_constant_cbv={options.push_cbv_space,options.push_cbv_register};
        config.first_vertex_and_base_instance_mode=SysvalOf(options.draw_sysvals);
        config.workgroup_id_mode=SysvalOf(options.workgroup_id);
        if(options.runtime_viewport_flips && (options.stage==SpirvStage::Vertex ||
           options.stage==SpirvStage::TessellationEvaluation || options.stage==SpirvStage::Geometry))
            config.yz_flip.mode=DXIL_SPIRV_YZ_FLIP_CONDITIONAL;
        config.declared_read_only_images_as_srvs=options.declared_read_only_images_as_srvs;
        config.inferred_read_only_images_as_srvs=options.inferred_read_only_images_as_srvs;
        config.force_sample_rate_shading=options.force_sample_rate_shading;
        config.lower_view_index=options.lower_view_index;
        config.lower_view_index_to_rt_layer=options.lower_view_index_to_rt_layer;
        config.shader_model_max=static_cast<dxil_shader_model>(SHADER_MODEL_6_0+options.shader_model_minor);
        dxil_spirv_debug_options debug{};
        dxil_spirv_logger logger{&result.diagnostics,AppendLog};
        dxil_spirv_object output{};
        struct OutputOwner { dxil_spirv_object* output; decltype(free_output) release; ~OutputOwner(){ release(output); } } owner{&output,free_output};
        if(!convert(words.data(),words.size(),constants.data(),static_cast<unsigned>(constants.size()),StageOf(options.stage),options.entry_point.c_str(),
                    static_cast<dxil_validator_version>(DXIL_VALIDATOR_1_0+options.validator_minor),&debug,&config,&logger,&output)) {
            result.diagnostics += "SPIR-V to DXIL conversion failed\n"; return result;
        }
        if(!output.binary.buffer || !output.binary.size || output.binary.size>std::numeric_limits<UINT32>::max()) throw std::runtime_error("Converter returned invalid/empty DXIL");
        result.converted=true;
        result.requires_runtime_data=output.metadata.requires_runtime_data;
        result.needs_draw_sysvals=output.metadata.needs_draw_sysvals;
        auto create_utils=dxc_module.Symbol<DxcCreateInstanceProc>("DxcCreateInstance");
        auto create_validator=validator_module.Symbol<DxcCreateInstanceProc>("DxcCreateInstance");
        ComPtr<IDxcUtils> utils; ComPtr<IDxcValidator> validator;
        if(FAILED(DX12_API_CALL(create_utils(CLSID_DxcUtils,IID_PPV_ARGS(&utils)))) || FAILED(DX12_API_CALL(create_validator(CLSID_DxcValidator,IID_PPV_ARGS(&validator))))) throw std::runtime_error("Cannot create DXIL validator");
        ComPtr<IDxcBlobEncoding> blob;
        if(FAILED(DX12_API_CALL(utils->CreateBlob(output.binary.buffer,static_cast<UINT32>(output.binary.size),DXC_CP_ACP,&blob)))) throw std::runtime_error("Cannot create DXIL validation blob");
        ComPtr<IDxcOperationResult> validation;
        if(FAILED(DX12_API_CALL(validator->Validate(blob.Get(),DxcValidatorFlags_InPlaceEdit,&validation))) || !validation) throw std::runtime_error("DXIL validation call failed");
        ComPtr<IDxcBlobEncoding> errors;
        if(SUCCEEDED(DX12_API_CALL(validation->GetErrorBuffer(&errors))) && errors && DX12_API_CALL(errors->GetBufferSize())) {
            const auto* text=static_cast<const char*>(DX12_API_CALL(errors->GetBufferPointer()));
            size_t length=DX12_API_CALL(errors->GetBufferSize());
            while(length && text[length-1]=='\0') --length;
            result.diagnostics.append(text,length);
        }
        HRESULT status=E_FAIL; DX12_API_CALL(validation->GetStatus(&status));
        if(FAILED(status)) { result.diagnostics += "DXIL validation rejected shader\n"; return result; }
        ComPtr<IDxcBlob> signed_blob;
        if(FAILED(DX12_API_CALL(validation->GetResult(&signed_blob))) || !signed_blob) throw std::runtime_error("Validator returned no signed DXIL");
        result.bytecode.resize(DX12_API_CALL(signed_blob->GetBufferSize()));
        std::memcpy(result.bytecode.data(),DX12_API_CALL(signed_blob->GetBufferPointer()),DX12_API_CALL(signed_blob->GetBufferSize()));
        result.validated=true; result.success=true;
    } catch(const std::exception& error) { result.diagnostics += error.what(); result.diagnostics += '\n'; }
    return result;
}
}
