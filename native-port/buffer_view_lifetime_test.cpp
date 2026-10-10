// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    constexpr u64 Bytes=65536;
    struct Allocation {void* memory;~Allocation(){if(memory)VirtualFree(memory,0,MEM_RELEASE);}} allocation{
        VirtualAlloc(nullptr,3*Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!allocation.memory)throw std::runtime_error("Cannot allocate guest view fixture");
    auto* words=static_cast<u32*>(allocation.memory);
    for(u32 i=0;i<3;++i)std::fill_n(words+i*Bytes/4,Bytes/4,0xabcd0000+i);
    Rasterizer rasterizer(device.Get());const auto base=reinterpret_cast<VAddr>(words);rasterizer.MapMemory(base,3*Bytes);
    const auto prepare=[&](u32 index){const Rasterizer::ShaderBufferRequest request[]{{base+index*Bytes,Bytes,false,false}};
        auto views=rasterizer.PrepareShaderBuffers(request);rasterizer.Finish();return views;};
    auto first=prepare(0);rasterizer.UnmapMemory(base,Bytes);
    auto second=prepare(1);
    auto readback=Buffer(device.Get(),8,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    const Rasterizer::ShaderBufferView* selected[]{&first[0],&second[0]};
    for(u32 i=0;i<2;++i){Resource source;source.handle=selected[i]->resource;
        source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        source.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        rasterizer.RecordingList()->CopyBufferRegion(readback.handle.Get(),i*4,source.handle.Get(),selected[i]->offset,4);
        source.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    rasterizer.Finish();void* data{};D3D12_RANGE range{0,8};Check(readback.handle->Map(0,&range,&data));
    u32 values[2];memcpy(values,data,8);D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
    if(values[0]!=0xabcd0000||values[1]!=0xabcd0001)
        throw std::runtime_error("A live buffer view was overwritten by a different guest mapping");
    if(first[0].resource.Get()==second[0].resource.Get())throw std::runtime_error("Buffer pool reused a pinned view");
    // Once the last view releases its pin, this exact resource can be reused.
    auto* retired_identity=first[0].resource.Get();first.clear();
    auto third=prepare(2);
    if(third[0].resource.Get()!=retired_identity)throw std::runtime_error("Released buffer view prevented pool reuse");
    rasterizer.ValidateDebugMessages();puts("PASS: retained guest buffer view survives remapping; pool reuse resumes after its last pin retires");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
