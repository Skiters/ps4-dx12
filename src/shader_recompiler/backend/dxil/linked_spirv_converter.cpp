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
#include <spirv_to_dxil_linked.h>
#undef BITFIELD_MASK
#undef BITFIELD_RANGE
#include <array>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
namespace Shader::Backend::DX12 {
namespace {
using Microsoft::WRL::ComPtr;
struct Module {
    HMODULE handle{};
    explicit Module(const std::wstring& path) {
        handle=Common::RuntimeModuleTrace::Load(path.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!handle) throw std::runtime_error("Cannot load linked compiler DLL, Win32 "+std::to_string(GetLastError()));
    }
    ~Module(){ if(handle) FreeLibrary(handle); }
    Module(const Module&)=delete;
    template<class T> T Symbol(const char* name){
        auto symbol=GetProcAddress(handle,name);
        if(!symbol) throw std::runtime_error(std::string("Missing linked compiler export: ")+name);
        return reinterpret_cast<T>(symbol);
    }
};
void Log(void* target,const char* message) noexcept {
    try { if(message){auto& text=*static_cast<std::string*>(target);text+=message;text+='\n';} } catch(...) {}
}
dxil_spirv_shader_stage Stage(SpirvStage stage) {
    switch(stage){
    case SpirvStage::Vertex:return DXIL_SPIRV_SHADER_VERTEX;
    case SpirvStage::TessellationControl:return DXIL_SPIRV_SHADER_TESS_CTRL;
    case SpirvStage::TessellationEvaluation:return DXIL_SPIRV_SHADER_TESS_EVAL;
    case SpirvStage::Geometry:return DXIL_SPIRV_SHADER_GEOMETRY;
    case SpirvStage::Pixel:return DXIL_SPIRV_SHADER_FRAGMENT;
    case SpirvStage::Compute:return DXIL_SPIRV_SHADER_COMPUTE;
    }
    throw std::runtime_error("Invalid linked stage");
}
dxil_spirv_sysval_type Sysval(SystemValueMode mode){
    switch(mode){
    case SystemValueMode::Zero:return DXIL_SPIRV_SYSVAL_TYPE_ZERO;
    case SystemValueMode::Native:return DXIL_SPIRV_SYSVAL_TYPE_NATIVE;
    case SystemValueMode::RuntimeData:return DXIL_SPIRV_SYSVAL_TYPE_RUNTIME_DATA;
    }
    throw std::runtime_error("Invalid linked system value mode");
}
}
LinkedSpirvResult ConvertLinkedStages(const std::wstring& converter_dll,
                                    std::span<const LinkedSpirvStage> stages,
                                    const SpirvConvertOptions& options){
    LinkedSpirvResult result;
    try {
        if(stages.empty() || stages.size()>6) throw std::runtime_error("Linked shader stage count must be 1..6");
        if(options.shader_model_minor>8 || options.validator_minor>8) throw std::runtime_error("Invalid linked SM/validator version");
        if(options.runtime_cbv_space>31 || options.push_cbv_space>31) throw std::runtime_error("Mesa linked CBV descriptor spaces must be 0..31");
        std::array<bool,6> seen{};
        std::vector<dxil_spirv_linked_stage> inputs(stages.size());
        std::vector<std::vector<uint32_t>> unified_words(stages.size());
        std::vector<std::vector<dxil_spirv_specialization>> specializations(stages.size());
        for(size_t i=0;i<stages.size();++i){
            auto stage=Stage(stages[i].stage);
            if(seen[static_cast<size_t>(stage)]) throw std::runtime_error("Duplicate linked shader stage");
            seen[static_cast<size_t>(stage)]=true;
            auto words=stages[i].words;
            if(words.size()<5 || words[0]!=0x07230203u) throw std::runtime_error("Invalid linked SPIR-V header");
            for(size_t offset=5;offset<words.size();){
                size_t count=words[offset]>>16;
                if(!count || count>words.size()-offset) throw std::runtime_error("Truncated linked SPIR-V instruction");
                offset+=count;
            }
            if(stages[i].entry_point.empty() || stages[i].entry_point.find('\0')!=std::string::npos) throw std::runtime_error("Invalid linked entry point");
            if(stages[i].specializations.size()>std::numeric_limits<unsigned>::max()) throw std::runtime_error("Too many linked specializations");
            auto& constants=specializations[i];constants.resize(stages[i].specializations.size());
            for(size_t j=0;j<constants.size();++j){constants[j].id=stages[i].specializations[j].id;constants[j].value.u64=stages[i].specializations[j].raw_bits;constants[j].defined_on_module=stages[i].specializations[j].defined_on_module;}
            if(options.native_storage_buffer_uavs){unified_words[i]=NativeStorageBufferUavs(words);words=unified_words[i];}
            inputs[i]={words.data(),words.size(),stage,stages[i].entry_point.c_str(),constants.data(),static_cast<unsigned>(constants.size())};
        }
        if(seen[DXIL_SPIRV_SHADER_TESS_CTRL]!=seen[DXIL_SPIRV_SHADER_TESS_EVAL]) throw std::runtime_error("Linked hull/domain stages must be supplied together");
        if(seen[DXIL_SPIRV_SHADER_COMPUTE] && stages.size()!=1) throw std::runtime_error("Compute cannot join a graphics shader chain");
        auto slash=converter_dll.find_last_of(L"/\\");
        if(slash==std::wstring::npos) throw std::runtime_error("Linked converter path must include its directory");
        auto directory=converter_dll.substr(0,slash+1);
        Module validator_module{directory+L"dxil.dll"},dxc_module{directory+L"dxcompiler.dll"},converter_module{converter_dll};
        NativeClockCompileScope clock_scope(converter_module.handle,options.native_shader_clock_backend);
        auto convert=converter_module.Symbol<decltype(&spirv_to_dxil_linked)>("spirv_to_dxil_linked");
        auto release=converter_module.Symbol<decltype(&spirv_to_dxil_free)>("spirv_to_dxil_free");
        result.converter_version=converter_module.Symbol<decltype(&spirv_to_dxil_get_version)>("spirv_to_dxil_get_version")();
        dxil_spirv_runtime_conf config{};
        config.runtime_data_cbv={options.runtime_cbv_space,options.runtime_cbv_register};
        config.push_constant_cbv={options.push_cbv_space,options.push_cbv_register};
        config.first_vertex_and_base_instance_mode=Sysval(options.draw_sysvals);
        config.workgroup_id_mode=Sysval(options.workgroup_id);
        if(options.runtime_viewport_flips)
            config.yz_flip.mode=DXIL_SPIRV_YZ_FLIP_CONDITIONAL;
        config.declared_read_only_images_as_srvs=options.declared_read_only_images_as_srvs;
        config.inferred_read_only_images_as_srvs=options.inferred_read_only_images_as_srvs;
        config.force_sample_rate_shading=options.force_sample_rate_shading;
        config.lower_view_index=options.lower_view_index;config.lower_view_index_to_rt_layer=options.lower_view_index_to_rt_layer;
        config.shader_model_max=static_cast<dxil_shader_model>(SHADER_MODEL_6_0+options.shader_model_minor);
        dxil_spirv_debug_options debug{};dxil_spirv_logger logger{&result.diagnostics,Log};
        std::vector<dxil_spirv_object> outputs(stages.size());
        struct Owner{std::vector<dxil_spirv_object>& objects;decltype(release) free_object;~Owner(){for(auto& object:objects)free_object(&object);}} owner{outputs,release};
        if(!convert(inputs.data(),static_cast<unsigned>(inputs.size()),static_cast<dxil_validator_version>(DXIL_VALIDATOR_1_0+options.validator_minor),&debug,&config,&logger,outputs.data())) throw std::runtime_error("Linked SPIR-V/NIR/DXIL compilation failed");
        ComPtr<IDxcUtils> utils;ComPtr<IDxcValidator> validator;
        auto create_utils=dxc_module.Symbol<DxcCreateInstanceProc>("DxcCreateInstance");
        auto create_validator=validator_module.Symbol<DxcCreateInstanceProc>("DxcCreateInstance");
        if(FAILED(DX12_API_CALL(create_utils(CLSID_DxcUtils,IID_PPV_ARGS(&utils)))) || FAILED(DX12_API_CALL(create_validator(CLSID_DxcValidator,IID_PPV_ARGS(&validator))))) throw std::runtime_error("Cannot create linked DXIL validator");
        std::vector<SpirvConvertResult> pending(stages.size());
        for(size_t i=0;i<outputs.size();++i){
            const auto& output=outputs[i];auto& stage=pending[i];
            if(!output.binary.buffer || !output.binary.size || output.binary.size>std::numeric_limits<UINT32>::max()) throw std::runtime_error("Linked compiler returned empty/invalid bytecode");
            ComPtr<IDxcBlobEncoding> blob;
            if(FAILED(DX12_API_CALL(utils->CreateBlob(output.binary.buffer,static_cast<UINT32>(output.binary.size),DXC_CP_ACP,&blob)))) throw std::runtime_error("Cannot create linked validation blob");
            ComPtr<IDxcOperationResult> checked;
            if(FAILED(DX12_API_CALL(validator->Validate(blob.Get(),DxcValidatorFlags_InPlaceEdit,&checked))) || !checked) throw std::runtime_error("Linked DXIL validation call failed");
            ComPtr<IDxcBlobEncoding> errors;
            if(SUCCEEDED(DX12_API_CALL(checked->GetErrorBuffer(&errors))) && errors && DX12_API_CALL(errors->GetBufferSize())){
                auto text=static_cast<const char*>(DX12_API_CALL(errors->GetBufferPointer()));size_t length=DX12_API_CALL(errors->GetBufferSize());while(length && !text[length-1])--length;
                stage.diagnostics.assign(text,length);
                if(length) result.diagnostics+="Stage "+std::to_string(i)+": "+stage.diagnostics;
            }
            HRESULT status=E_FAIL;DX12_API_CALL(checked->GetStatus(&status));
            if(FAILED(status)) throw std::runtime_error("Linked DXIL stage "+std::to_string(i)+" rejected by validator");
            ComPtr<IDxcBlob> signed_blob;
            if(FAILED(DX12_API_CALL(checked->GetResult(&signed_blob))) || !signed_blob || !DX12_API_CALL(signed_blob->GetBufferSize())) throw std::runtime_error("Linked validator returned empty signed bytecode");
            stage.bytecode.resize(DX12_API_CALL(signed_blob->GetBufferSize()));std::memcpy(stage.bytecode.data(),DX12_API_CALL(signed_blob->GetBufferPointer()),DX12_API_CALL(signed_blob->GetBufferSize()));
            stage.requires_runtime_data=output.metadata.requires_runtime_data;stage.needs_draw_sysvals=output.metadata.needs_draw_sysvals;
            stage.converter_version=result.converter_version;stage.success=stage.converted=stage.validated=true;
        }
        result.stages=std::move(pending);result.success=true;
    }catch(const std::exception& error){result.diagnostics+=error.what();result.diagnostics+='\n';}
    return result;
}
}
