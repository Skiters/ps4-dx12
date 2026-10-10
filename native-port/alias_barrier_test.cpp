// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(int argc,char**){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    const bool enhanced=argc>1;D3D12_FEATURE_DATA_D3D12_OPTIONS12 options{};
    if(enhanced&&(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,&options,sizeof(options)))||!options.EnhancedBarriersSupported))return 77;
    constexpr UINT Bytes=4096;
    D3D12_HEAP_DESC heap_desc{};heap_desc.SizeInBytes=65536;heap_desc.Properties.Type=D3D12_HEAP_TYPE_DEFAULT;
    heap_desc.Flags=D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;ComPtr<ID3D12Heap> heap;
    Check(device->CreateHeap(&heap_desc,IID_PPV_ARGS(&heap)));
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=Bytes;
    desc.Height=desc.DepthOrArraySize=desc.MipLevels=desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    Resource first,second;
    Check(device->CreatePlacedResource(heap.Get(),0,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&first.handle)));
    Check(device->CreatePlacedResource(heap.Get(),0,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&second.handle)));
    auto upload=Buffer(device.Get(),Bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    auto readback=Buffer(device.Get(),Bytes*2,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    void* memory{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&memory));
    for(UINT i=0;i<Bytes/4;++i)static_cast<UINT*>(memory)[i]=0x12340000+i;upload.handle->Unmap(0,nullptr);
    Rasterizer renderer(device.Get());auto* list=renderer.RecordingList();
    if(enhanced){BarrierBatch prime;prime.Transition(second,D3D12_RESOURCE_STATE_COPY_SOURCE);prime.Flush(list);}
    {BarrierBatch initial;initial.Aliasing(nullptr,&first);initial.Transition(first,D3D12_RESOURCE_STATE_COPY_DEST);initial.Flush(list);}
    list->CopyBufferRegion(first.handle.Get(),0,upload.handle.Get(),0,Bytes);
    const auto prepare=[&](Resource& resource){BarrierBatch batch;
        if(!enhanced)batch.Transition(resource,D3D12_RESOURCE_STATE_COPY_SOURCE);
        batch.Transition(resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        // Force ordered legacy transitions even if COPY_SOURCE was already set.
        if(!enhanced){batch.Transition(resource,D3D12_RESOURCE_STATE_COPY_DEST);batch.Transition(resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
        batch.Flush(list);
        if(resource.enhanced_barriers!=enhanced)throw std::runtime_error("Wrong alias fixture barrier model");};
    prepare(first);
    {BarrierBatch transfer;transfer.Uav(&first);transfer.Aliasing(&first,&second);
        transfer.Transition(second,D3D12_RESOURCE_STATE_COPY_SOURCE);transfer.Flush(list);}
    if(enhanced&&(first.enhanced_barriers||first.state!=D3D12_RESOURCE_STATE_COMMON))
        throw std::runtime_error("Aliasing did not retire enhanced source through COMMON");
    if(second.state!=D3D12_RESOURCE_STATE_COPY_SOURCE)
        throw std::runtime_error("Alias bridge discarded a requested return to the prior state");
    list->CopyBufferRegion(readback.handle.Get(),0,second.handle.Get(),0,Bytes);
    prepare(second);
    {BarrierBatch transfer;transfer.Aliasing(&second,&first);transfer.Transition(first,D3D12_RESOURCE_STATE_COPY_SOURCE);
        transfer.Aliasing();transfer.Flush(list);}
    if(enhanced&&(second.enhanced_barriers||second.state!=D3D12_RESOURCE_STATE_COMMON))
        throw std::runtime_error("Reverse alias transfer lost COMMON state");
    list->CopyBufferRegion(readback.handle.Get(),Bytes,first.handle.Get(),0,Bytes);
    renderer.Finish();D3D12_RANGE read{0,Bytes*2};Check(readback.handle->Map(0,&read,&memory));
    bool valid=true;for(UINT i=0;i<Bytes*2/4;++i)valid&=static_cast<const UINT*>(memory)[i]==0x12340000+i%(Bytes/4);
    readback.handle->Unmap(0,&empty);
    if(!valid)throw std::runtime_error("Aliased buffer contents were not inherited in command order");
    renderer.ValidateDebugMessages();puts("PASS: placed buffer alias transfers preserve bits and barrier model interoperability in both directions");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
