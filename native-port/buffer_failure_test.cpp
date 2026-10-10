// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>

// This executable alone replaces allocation. Disarm before throwing so that
// exception handling, retirement and retries can allocate normally.
thread_local size_t allocation_index{}, failure_index{};
thread_local bool count_allocations{};
void* operator new(size_t bytes) {
    if(count_allocations && ++allocation_index==failure_index){
        count_allocations=false;throw std::bad_alloc{};
    }
    if(void* result=std::malloc(bytes?bytes:1))return result;
    throw std::bad_alloc{};
}
void operator delete(void* pointer) noexcept {std::free(pointer);}
void* operator new[](size_t bytes){return ::operator new(bytes);}
void operator delete[](void* pointer) noexcept {::operator delete(pointer);}

using namespace NativeDX12;
int main(){try{
    ComPtr<ID3D12Debug> debug;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){
        ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 description{};adapter->GetDesc1(&description);
        if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    constexpr u64 Bytes=5ull*1024*1024;
    struct Allocation{void* data=VirtualAlloc(nullptr,Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        ~Allocation(){if(data)VirtualFree(data,0,MEM_RELEASE);}} memory;
    if(!memory.data)throw std::bad_alloc{};
    const auto address=reinterpret_cast<VAddr>(memory.data);
    auto* words=static_cast<u32*>(memory.data);
    const Rasterizer::ShaderBufferRequest request{address,Bytes,false,false};
    size_t failures=0;
    for(unsigned phase=0;phase<3;++phase){
    const bool refresh=phase!=0;
    size_t allocations=0;
    for(size_t fail=0;fail<=allocations;++fail){
        Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,Bytes);
        std::memset(memory.data,0,size_t(Bytes));
        words[0]=0x12345678;words[Bytes/4-1]=0x87654321;
        if(refresh){
            auto initial=rasterizer.PrepareShaderBuffers(std::span(&request,1));
            rasterizer.Finish();
        }
        // Retry the exact data present during the failed operation: changing
        // it afterwards could hide a prematurely updated CPU snapshot.
        if(phase==2)std::memset(memory.data,0x5a,size_t(Bytes));
        words[0]=0xabcd0000+u32(fail);words[Bytes/4-1]=0xface0000+u32(fail);
        allocation_index=0;failure_index=fail;count_allocations=true;
        bool failed=false;
        try{auto initial=rasterizer.PrepareShaderBuffers(std::span(&request,1));}
        catch(const std::bad_alloc&){failed=true;}
        count_allocations=false;
        if(fail==0){allocations=allocation_index;printf("%s allocation sites: %zu\n",phase==2?"Full refresh":refresh?"Partial refresh":"Initial",allocations);}
        else if(!failed)throw std::runtime_error("Requested allocation failure was not reached");
        failures+=failed;
        const auto views=rasterizer.PrepareShaderBuffers(std::span(&request,1));
        auto rb=Buffer(device.Get(),Bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource source;source.handle=views[0].resource;
        source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        auto* list=rasterizer.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(rb.handle.Get(),0,source.handle.Get(),views[0].offset,Bytes);
        source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        rasterizer.Finish();void* data{};D3D12_RANGE range{0,Bytes};Check(rb.handle->Map(0,&range,&data));
        const bool valid=std::memcmp(data,memory.data,size_t(Bytes))==0;
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
        if(!valid||rasterizer.CachedBufferBytes()!=Bytes){
            fprintf(stderr,"Failed allocation site: %zu, phase=%u\n",fail,phase);
            throw std::runtime_error("Buffer retry lost data or cache accounting");
        }
        rasterizer.ValidateDebugMessages();
    }
    }
    printf("PASS: %zu injected allocation failures, complete retry GPU contents and cache accounting\n",failures);
    return 0;
}catch(const std::exception& error){count_allocations=false;fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
