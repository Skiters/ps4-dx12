// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <thread>
#include <atomic>
using namespace NativeDX12;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Guest {
    void* data;u64 size;
    explicit Guest(u64 bytes):data(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)),size(bytes){Require(data,"guest allocation");}
    ~Guest(){VirtualFree(data,0,MEM_RELEASE);}
    VAddr Base()const{return reinterpret_cast<VAddr>(data);}
};
int main(int argc,char** argv){try{
    const bool lru=argc>1&&std::string_view(argv[1])=="lru";
    const bool copy=argc>1&&std::string_view(argv[1])=="copy";
    _putenv_s("SHADPS4_DX12_MEMORY_PERF","1");
    _putenv_s("SHADPS4_DX12_BUFFER_BUDGET_MB",copy?"256":lru?"4":"32");
    _putenv_s("SHADPS4_DX12_CPU_POOL_MB","32");
    MemoryPerfConfig config;config.enabled=true;config.requested=8192*MemoryMiB;
    const auto full=config.Budget(16*1024*MemoryMiB,2*1024*MemoryMiB,0);
    Require(full.cache==8192*MemoryMiB,"8 GiB cap on roomy adapter");
    const auto pressure=config.Budget(8*1024*MemoryMiB,7*1024*MemoryMiB,2*1024*MemoryMiB);
    Require(pressure.arena==1024*MemoryMiB&&pressure.cache<pressure.arena,"pressure preserves other allocations and reserve");
    Require(config.Budget(1024*MemoryMiB,2*1024*MemoryMiB,0).cache==0,"budget subtraction underflow");
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};a->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;}
    Require(device!=nullptr,"hardware device");
    {
        PlacedBufferAllocator arena(device.Get(),256*MemoryMiB,512*MemoryMiB);arena.SetCompactLargePages(true);
        auto a=arena.Allocate(160*MemoryMiB),b=arena.Allocate(160*MemoryMiB),c=arena.Allocate(160*MemoryMiB);
        Require(arena.CommittedBytes()==480*MemoryMiB,"large backing fragmentation exceeds budget");
        arena.SetBudget(0);arena.Trim();Require(arena.CommittedBytes()==480*MemoryMiB,"trim discarded live leases");
        a={};b={};c={};arena.Trim();Require(arena.CommittedBytes()==0,"trim retained unleased heaps");
        bool rejected=false;try{arena.Allocate(65536);}catch(const BufferBudgetExhausted&){rejected=true;}
        Require(rejected,"zero pressure budget allowed new heap allocation");
    }
    if(argc>1&&std::string_view(argv[1])=="fence"){
        Guest guest(65536);Rasterizer raster(device.Get());raster.MapMemory(guest.Base(),guest.size);
        static_cast<u32*>(guest.data)[0]=0x12345678;
        const Rasterizer::ShaderBufferRequest request[]{{guest.Base(),guest.size}};
        auto view=raster.PrepareShaderBuffers(request);raster.Finish();
        ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
        raster.WaitExternalFence(gate.Get(),1);
        auto readback=Buffer(device.Get(),4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource source;source.handle=view[0].resource;source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        source.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        raster.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,source.handle.Get(),view[0].offset,4);
        const auto pending=raster.Flush();std::atomic<bool> started{},retired{};std::exception_ptr error;
        std::jthread worker([&]{started=true;try{raster.UnmapMemory(guest.Base(),guest.size);retired=true;}catch(...){error=std::current_exception();}});
        while(!started)std::this_thread::yield();std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const bool premature=retired||raster.CompletedFence()>=pending;
        Check(gate->Signal(1));worker.join();if(error)std::rethrow_exception(error);
        Require(!premature&&retired,"eviction did not wait for GPU fence");
        void* data{};D3D12_RANGE range{0,4};Check(readback.handle->Map(0,&range,&data));
        const auto word=*static_cast<u32*>(data);D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
        Require(word==0x12345678,"pending GPU copy lost its resource");
        raster.ValidateDebugMessages();puts("PASS: eviction blocks behind unfinished GPU fence and preserves readback");return 0;
    }
    if(copy){
        Guest guest(96*MemoryMiB);Rasterizer raster(device.Get());raster.MapMemory(guest.Base(),guest.size);
        const Rasterizer::ShaderBufferRequest request[]{{guest.Base(),80*MemoryMiB}};
        auto hot=raster.PrepareShaderBuffers(request);raster.Finish();
        auto* words=static_cast<u32*>(guest.data);words[80*MemoryMiB/4]=91;
        raster.CopyBuffer(guest.Base()+80*MemoryMiB+16,guest.Base()+80*MemoryMiB,4,false,false);raster.Finish();
        auto again=raster.PrepareShaderBuffers(request);
        Require(hot[0].resource.Get()==again[0].resource.Get(),"tiny DMA copy discarded unrelated 80 MiB resident backing");
        Require(words[80*MemoryMiB/4+4]==91,"DMA destination mismatch");
        raster.Finish();raster.ValidateDebugMessages();puts("PASS: tiny DMA retains unrelated hot cache and exact CPU result");return 0;
    }
    if(lru){
        Guest guest(6*MemoryMiB);Rasterizer raster(device.Get());raster.MapMemory(guest.Base(),guest.size);
        const auto bind=[&](u64 index){const Rasterizer::ShaderBufferRequest req[]{{guest.Base()+index*MemoryMiB,MemoryMiB}};
            return raster.PrepareShaderBuffers(req);};
        auto hot=bind(0);for(u64 i=1;i<4;++i)bind(i);bind(0);bind(4);
        const auto again=bind(0);
        Require(again[0].resource.Get()==hot[0].resource.Get(),"address-order eviction discarded the recently reused hot buffer");
        raster.Finish();raster.ValidateDebugMessages();puts("PASS: hot resident buffer survives cold eviction");return 0;
    }
    Guest guest(32*MemoryMiB);Rasterizer raster(device.Get());raster.EnableBufferArenas(true);
    raster.MapMemory(guest.Base(),guest.size);raster.NotifyHostMapping(guest.Base(),guest.size,true,true);
    const auto bind=[&](u64 offset,u64 size){const Rasterizer::ShaderBufferRequest req[]{{guest.Base()+offset,size}};
        return raster.PrepareShaderBuffers(req);};
    const auto verify=[&](const Rasterizer::ShaderBufferView& view,u32 expected){
        auto readback=Buffer(device.Get(),8,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource source;source.handle=view.resource;source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        source.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        raster.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,source.handle.Get(),view.offset,4);
        raster.RecordingList()->CopyBufferRegion(readback.handle.Get(),4,source.handle.Get(),view.offset+view.size-4,4);
        source.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        raster.Finish();void* data{};D3D12_RANGE range{0,8};Check(readback.handle->Map(0,&range,&data));
        const bool valid=static_cast<u32*>(data)[0]==expected&&static_cast<u32*>(data)[1]==expected;
        D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);Require(valid,"stale snapshot/staging GPU contents");
    };
    auto* words=static_cast<u32*>(guest.data);std::fill_n(words,guest.size/4,17u);
    auto a=bind(0,65536),b=bind(0,68*1024),c=bind(0,60*1024),d=bind(0,72*1024);
    Require(b[0].resource.Get()==c[0].resource.Get()&&b[0].resource.Get()==d[0].resource.Get(),"stable large envelopes");
    verify(d[0],17);a.clear();b.clear();c.clear();d.clear();
    raster.UnmapMemory(guest.Base(),guest.size);raster.MapMemory(guest.Base(),guest.size);
    raster.NotifyHostMapping(guest.Base(),guest.size,true,true);
    auto large=bind(0,8*MemoryMiB);verify(large[0],17);large.clear();
    raster.UnmapMemory(guest.Base(),8*MemoryMiB);
    std::fill_n(words+8*MemoryMiB/4,8*MemoryMiB/4,29u);
    auto next=bind(8*MemoryMiB,8*MemoryMiB);verify(next[0],29);
    words[8*MemoryMiB/4]=words[16*MemoryMiB/4-1]=43;
    raster.NotifyCpuWrite(guest.Base()+8*MemoryMiB,8*MemoryMiB);
    auto changed=bind(8*MemoryMiB,8*MemoryMiB);verify(changed[0],43);
    Require(raster.MemoryReuse().snapshots>0,"snapshot allocation not reused");
    Require(raster.MemoryReuse().staging>0,"large staging allocation not reused");
    next.clear();changed.clear();raster.UnmapMemory(guest.Base()+8*MemoryMiB,24*MemoryMiB);
    // Reserved, uncommitted neighbors must not be rounded into an envelope.
    Guest sparse(128*1024);Require(VirtualFree(reinterpret_cast<void*>(sparse.Base()+65536),65536,MEM_DECOMMIT),"decommit hole");
    raster.MapMemory(sparse.Base(),sparse.size);raster.NotifyHostMapping(sparse.Base(),65536,true,true);
    const Rasterizer::ShaderBufferRequest edge[]{{sparse.Base()+32768,32768}};
    auto edge_view=raster.PrepareShaderBuffers(edge);raster.Finish();
    Require(edge_view[0].offset+32768<=edge_view[0].resource->GetDesc().Width,"edge view bounds");
    bool rejected=false;try{const Rasterizer::ShaderBufferRequest hole[]{{sparse.Base()+32768,65536}};raster.PrepareShaderBuffers(hole);}catch(const std::exception&){rejected=true;}
    Require(rejected,"PRT hole accepted");raster.UnmapMemory(sparse.Base(),sparse.size);
    raster.ValidateDebugMessages();
    printf("PASS: budget pressure arithmetic, stable envelopes, snapshot reuse=%llu staging reuse=%llu, CPU refresh and PRT holes\n",raster.MemoryReuse().snapshots,raster.MemoryReuse().staging);
    return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
