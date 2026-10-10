#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "rasterizer.h"
#include "shader_pipeline.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int wmain(int argc,wchar_t**argv){try {
    if(argc!=4)throw std::invalid_argument("usage: guest-buffer-dispatch fixture.spv converter.dll dxc.dll");
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot read fixture");
    auto bytes=input.tellg();if(bytes<=0||bytes%4)throw std::runtime_error("Invalid fixture");
    std::vector<u32> code(size_t(bytes)/4);input.seekg(0);input.read(reinterpret_cast<char*>(code.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],code,options);if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);Rasterizer rasterizer(device.Get());
    std::vector<u32> guest(1024,0xdeadbeef);auto address=reinterpret_cast<VAddr>(guest.data());rasterizer.MapMemory(address,guest.size()*4);
    Rasterizer::ShaderBufferRequest requests[]{{address,4096,true,true},{address+4,4092,false,true}};
    auto views=rasterizer.PrepareShaderBuffers(requests);
    if(views[0].resource.Get()!=views[1].resource.Get()||views[1].offset!=4)throw std::runtime_error("Overlapping guest buffers did not share backing");
    ResourceBindings bindings(device.Get(),pipeline.Layout());
    auto runtime=Buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};D3D12_RANGE empty{};Check(DX12_API_CALL(runtime.handle->Map(0,&empty,&mapped)));memset(mapped,0,256);
    const UINT counts[]{16,1,1};memcpy(mapped,counts,sizeof(counts));DX12_API_CALL(runtime.handle->Unmap(0,nullptr));
    for(auto binding:pipeline.Layout().Bindings()) {
        if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV&&binding.reg==0&&binding.space==0) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;
            view.Buffer.FirstElement=views[0].offset/4;view.Buffer.NumElements=UINT(views[0].size/4);view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            bindings.Uav(0,0,views[0].resource.Get(),view);
        }else if(binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_CBV&&binding.reg==0&&binding.space==31)bindings.Cbv(0,31,runtime.handle.Get(),0,256);
        else throw std::runtime_error("Unexpected fixture resource");
    }
    rasterizer.ExecuteCompute(pipeline,bindings,16,1,1);
    for(UINT i=0;i<guest.size();++i)if(guest[i]!=i*3+7)throw std::runtime_error("Native shader result not reconciled to guest memory");
    requests[1].unordered_access=false;bool rejected=false;
    try{rasterizer.PrepareShaderBuffers(requests);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Incompatible aliased SRV/UAV states accepted");
    puts("PASS: guest mapped buffers -> coalesced native UAV descriptors -> compute dispatch -> guest CPU memory exact 1024 results; mixed-state alias rejection");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
