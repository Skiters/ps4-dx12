#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_shader_cache.h"
#include "shader_pipeline.h"
#include <dxcapi.h>
#include <d3d12shader.h>
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
using namespace NativeDX12;
static std::vector<u32> Read(const wchar_t* path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("Cannot read guest SPIR-V fixture");
    auto size=file.tellg();if(size<=0||size%4)throw std::runtime_error("Malformed SPIR-V fixture");
    std::vector<u32> words(size_t(size)/4);file.seekg(0);file.read(reinterpret_cast<char*>(words.data()),size);
    if(!file)throw std::runtime_error("Short SPIR-V fixture");return words;
}
static ComPtr<ID3D12Device> HardwareDevice() {
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> adapter;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(adapter->GetDesc1(&d)));
        if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;
    }
    if(!device)throw std::runtime_error("No hardware D3D12 device");return device;
}
int wmain(int argc,wchar_t** argv) {try {
    const auto cache_dir=std::filesystem::current_path()/("cache-test-"+std::to_string(GetCurrentProcessId()));
    _wputenv_s(L"SHADPS4_NATIVE_SHADER_CACHE",cache_dir.c_str());
    if(argc==4) {
        GuestShaderStage compute;compute.id=1;compute.info.sw_stage=Shader::SwStage::Compute;compute.spirv=Read(argv[3]);
        Shader::Backend::DX12::SpirvConvertOptions options;options.native_storage_buffer_uavs=true;
        GuestShaderCache cache(argv[1],options);const auto& dxil=cache.Compute(compute);
        if(&cache.Compute(compute)!=&dxil)throw std::runtime_error("Guest compute cache lost stable entry");
        GuestShaderCache warm(argv[1],options);
        if(warm.Compute(compute).bytecode!=dxil.bytecode||warm.PersistentHits()!=1)throw std::runtime_error("Persistent compute DXIL cache did not reload exact bytecode");
        auto device=HardwareDevice();ShaderPipeline pipeline(device.Get(),argv[2],dxil.bytecode);
        if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
        printf("PASS: actual guest compute -> signed DXIL -> reflected root layout -> hardware compute PSO; bindings=%zu; guest resources not executed\n",pipeline.Layout().Bindings().size());return 0;
    }
    if(argc!=5&&argc!=7&&argc!=8)throw std::invalid_argument("usage: guest-pipeline-test converter.dll dxcompiler.dll compute.spv OR pixel.spv|- vertex.spv [hull.spv|- domain.spv|- [geometry.spv]]");
    GuestShaderStage pixel,vertex;pixel.id=1;vertex.id=2;
    const bool has_pixel=std::wstring_view(argv[3])!=L"-";
    const bool has_tessellation=argc>=7&&std::wstring_view(argv[5])!=L"-";
    if(has_pixel){pixel.info.sw_stage=Shader::SwStage::Fragment;pixel.spirv=Read(argv[3]);}
    vertex.info.sw_stage=Shader::SwStage::Vertex;vertex.spirv=Read(argv[4]);
    GuestGraphicsStages guest;if(has_pixel)guest.stages[u32(Shader::SwStage::Fragment)]=&pixel;
    guest.stages[u32(Shader::SwStage::Vertex)]=&vertex;
    GuestShaderStage hull,domain;
    if(has_tessellation) {
        hull.id=3;domain.id=4;
        hull.info.sw_stage=Shader::SwStage::TessellationControl;hull.spirv=Read(argv[5]);
        domain.info.sw_stage=Shader::SwStage::TessellationEval;domain.spirv=Read(argv[6]);
        guest.stages[u32(Shader::SwStage::TessellationControl)]=&hull;
        guest.stages[u32(Shader::SwStage::TessellationEval)]=&domain;
    }
    GuestShaderStage geometry;
    if(argc==8) {
        geometry.id=5;geometry.info.sw_stage=Shader::SwStage::Geometry;geometry.spirv=Read(argv[7]);
        guest.stages[u32(Shader::SwStage::Geometry)]=&geometry;
    }
    Shader::Backend::DX12::SpirvConvertOptions options;options.runtime_viewport_flips=true;
    options.native_storage_buffer_uavs=true;
    GuestShaderCache cache(argv[1],options);const auto& dxil=cache.Graphics(guest);
    if(&cache.Graphics(guest)!=&dxil)throw std::runtime_error("Guest stage cache lost stable entry");
    GuestShaderCache warm(argv[1],options);const auto& reloaded=warm.Graphics(guest);
    if(warm.PersistentHits()!=1)throw std::runtime_error("Persistent linked DXIL cache missed");
    for(u32 index=0;index<Shader::MaxStageTypes;++index)
        if(reloaded.stages[index].bytecode!=dxil.stages[index].bytecode)throw std::runtime_error("Persistent linked DXIL stage mismatch");
    auto device=HardwareDevice();
    auto module=LoadLibraryExW(argv[2],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module)throw std::runtime_error("Cannot load DXC");
    struct ModuleRelease{HMODULE value;~ModuleRelease(){FreeLibrary(value);}} release{module};
    auto create=reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module,"DxcCreateInstance"));
    if(!create)throw std::runtime_error("Missing DXC reflection entry point");
    ComPtr<IDxcUtils> utils;Check(create(CLSID_DxcUtils,IID_PPV_ARGS(&utils)));
    auto reflect=[&](Shader::SwStage stage) {
        const auto& code=dxil.stages[u32(stage)].bytecode;DxcBuffer buffer{code.data(),code.size(),DXC_CP_ACP};
        ComPtr<ID3D12ShaderReflection> result;Check(utils->CreateReflection(&buffer,IID_PPV_ARGS(&result)));return result;
    };
    auto vs=reflect(Shader::SwStage::Vertex);
    auto ps=has_pixel?reflect(Shader::SwStage::Fragment):ComPtr<ID3D12ShaderReflection>{};
    D3D12_SHADER_DESC desc{};Check(DX12_API_CALL(vs->GetDesc(&desc)));
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
    // Fixture audit only: signature-compatible float/uint input storage. Guest
    // execution still needs formats, strides and divisors from the fetch shader.
    for(UINT i=0;i<desc.InputParameters;++i) {
        D3D12_SIGNATURE_PARAMETER_DESC p{};Check(vs->GetInputParameterDesc(i,&p));
        if(p.SystemValueType!=D3D_NAME_UNDEFINED)continue;
        const UINT components=p.Mask>=8?4:p.Mask>=4?3:p.Mask>=2?2:1;
        const DXGI_FORMAT floats[]{DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        const DXGI_FORMAT uints[]{DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_UINT,DXGI_FORMAT_R32G32B32_UINT,DXGI_FORMAT_R32G32B32A32_UINT};
        const DXGI_FORMAT ints[]{DXGI_FORMAT_R32_SINT,DXGI_FORMAT_R32G32_SINT,DXGI_FORMAT_R32G32B32_SINT,DXGI_FORMAT_R32G32B32A32_SINT};
        const auto format=p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?uints[components-1]:p.ComponentType==D3D_REGISTER_COMPONENT_SINT32?ints[components-1]:floats[components-1];
        inputs.push_back({p.SemanticName,p.SemanticIndex,format,UINT(inputs.size()),0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0});
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC fixed{};fixed.SampleMask=UINT_MAX;
    fixed.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;fixed.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    fixed.RasterizerState.DepthClipEnable=TRUE;fixed.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    if(has_tessellation)fixed.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    else if(argc==8) {
        auto gs=reflect(Shader::SwStage::Geometry);D3D12_SHADER_DESC geometry_desc{};Check(DX12_API_CALL(gs->GetDesc(&geometry_desc)));
        switch(geometry_desc.InputPrimitive) {
        case D3D_PRIMITIVE_POINT:fixed.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;break;
        case D3D_PRIMITIVE_LINE:case D3D_PRIMITIVE_LINE_ADJ:fixed.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;break;
        case D3D_PRIMITIVE_TRIANGLE:case D3D_PRIMITIVE_TRIANGLE_ADJ:break;
        default:throw std::runtime_error("Unexpected guest geometry input primitive");
        }
    }
    fixed.SampleDesc.Count=1;fixed.InputLayout={inputs.data(),UINT(inputs.size())};
    desc={};if(ps)Check(DX12_API_CALL(ps->GetDesc(&desc)));
    for(UINT i=0;i<desc.OutputParameters;++i) {
        D3D12_SIGNATURE_PARAMETER_DESC p{};Check(ps->GetOutputParameterDesc(i,&p));
        if(p.SystemValueType!=D3D_NAME_TARGET)continue;
        if(p.SemanticIndex>=8)throw std::runtime_error("Unsupported guest MRT semantic");
        fixed.NumRenderTargets=std::max(fixed.NumRenderTargets,p.SemanticIndex+1);
        fixed.RTVFormats[p.SemanticIndex]=p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?DXGI_FORMAT_R32G32B32A32_UINT:
            p.ComponentType==D3D_REGISTER_COMPONENT_SINT32?DXGI_FORMAT_R32G32B32A32_SINT:DXGI_FORMAT_R8G8B8A8_UNORM;
        fixed.BlendState.RenderTarget[p.SemanticIndex].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    // Gaps between MRTs must be valid even if this shader never writes them.
    for(UINT i=0;i<fixed.NumRenderTargets;++i)if(fixed.RTVFormats[i]==DXGI_FORMAT_UNKNOWN)fixed.RTVFormats[i]=DXGI_FORMAT_R8G8B8A8_UNORM;
    const auto& v=dxil.stages[u32(Shader::SwStage::Vertex)].bytecode;
    const auto& p=dxil.stages[u32(Shader::SwStage::Fragment)].bytecode;
    const auto& h=dxil.stages[u32(Shader::SwStage::TessellationControl)].bytecode;
    const auto& d=dxil.stages[u32(Shader::SwStage::TessellationEval)].bytecode;
    const auto& g=dxil.stages[u32(Shader::SwStage::Geometry)].bytecode;
    ShaderPipeline pipeline(device.Get(),argv[2],{v,p,h,d,g},fixed);
    if(!has_tessellation){
        auto extended=fixed;extended.DSVFormat=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        extended.DepthStencilState.StencilEnable=TRUE;
        extended.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_REPLACE,D3D12_COMPARISON_FUNC_ALWAYS};
        extended.DepthStencilState.BackFace=extended.DepthStencilState.FrontFace;
        ShaderPipeline separate(device.Get(),argv[2],{v,p,h,d,g},extended,false,{0x0f,0x03,0xf0,0xc0,true});
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
    printf("PASS: actual cached guest VS+PS -> linked signed DXIL -> reflected root layout -> hardware graphics PSO; inputs=%zu bindings=%zu; no guest frame claimed\n",inputs.size(),pipeline.Layout().Bindings().size());
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
