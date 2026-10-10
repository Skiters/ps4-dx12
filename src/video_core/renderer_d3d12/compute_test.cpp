#include "common/api_trace.h"
#include "compute_pipeline.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <dxgi1_6.h>
#include <fstream>
#include <cstdio>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("compute assertion failed");}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::invalid_argument("usage: compute-test fixture.spv converter.dll");
    std::ifstream f(argv[1],std::ios::binary|std::ios::ate);Require(bool(f));auto size=f.tellg();Require(size>0&&size%4==0);
    std::vector<uint32_t> spirv(size_t(size)/4);f.seekg(0);f.read(reinterpret_cast<char*>(spirv.data()),size);Require(bool(f));
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto converted=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);
    if(!converted.success||!converted.validated)throw std::runtime_error(converted.diagnostics);
    printf("requires_runtime_data=%d, needs_draw_sysvals=%d\n",converted.requires_runtime_data,converted.needs_draw_sysvals);printf("Mesa converted and validated/signed compute DXIL: %zu bytes\n",converted.bytecode.size());
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};DX12_API_CALL(a->GetDesc1(&d));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",d.Description);break;}}
    Require(device!=nullptr);ComputePipeline pipeline(device.Get(),converted.bytecode,0,0);
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> alloc;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&alloc))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,alloc.Get(),nullptr,IID_PPV_ARGS(&list))));
    constexpr UINT count=1024;auto output=UavBuffer(device.Get(),count*4);auto readback=Buffer(device.Get(),count*4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    pipeline.Dispatch(list.Get(),output,count/64);output.Transition(list.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    DX12_API_CALL(list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,count*4));Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());timeline.Wait(timeline.Submit(queue.Get(),list.Get()));
    void* data=nullptr;D3D12_RANGE range{0,count*4};Check(DX12_API_CALL(readback.handle->Map(0,&range,&data)));auto words=static_cast<const uint32_t*>(data);
    for(UINT i=0;i<count;++i)if(words[i]!=i*3+7)throw std::runtime_error("GPU result differs at index "+std::to_string(i)+": "+std::to_string(words[i]));
    D3D12_RANGE no_write{0,0};DX12_API_CALL(readback.handle->Unmap(0,&no_write));
    puts("PASS: GLSL -> SPIR-V -> Mesa signed DXIL -> native D3D12 compute PSO/root UAV -> Dispatch -> 1024 exact readback values");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}

