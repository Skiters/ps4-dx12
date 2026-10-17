// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include "preserved_buffer_budget_fixture.h"
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
    const bool ranges=argc>1&&std::string_view(argv[1])=="ranges";
    const bool preserved=argc>1&&std::string_view(argv[1])=="preserved";
    _putenv_s("SHADPS4_DX12_MEMORY_PERF","1");
    _putenv_s("SHADPS4_DX12_BUFFER_BUDGET_MB",(copy||ranges)?"256":(lru||preserved)?"4":"32");
    _putenv_s("SHADPS4_DX12_CPU_POOL_MB","32");
    MemoryPerfConfig config;config.enabled=true;config.requested=8192*MemoryMiB;
    const auto full=config.Budget(16*1024*MemoryMiB,2*1024*MemoryMiB,0);
    Require(full.cache==8192*MemoryMiB,"8 GiB cap on roomy adapter");
    const auto pressure=config.Budget(8*1024*MemoryMiB,7*1024*MemoryMiB,2*1024*MemoryMiB);
    Require(pressure.arena==1024*MemoryMiB&&pressure.cache<pressure.arena,"pressure preserves other allocations and reserve");
    Require(config.Budget(1024*MemoryMiB,2*1024*MemoryMiB,0).cache==0,"budget subtraction underflow");
    MemoryPerfConfig automatic;
    const auto roomy=automatic.Budget(16*1024*MemoryMiB,2*1024*MemoryMiB,0);
    Require(roomy.cache>2048*MemoryMiB&&roomy.cache<roomy.arena&&roomy.arena<=14*1024*MemoryMiB,
        "automatic residency cannot use the available adapter budget beyond 2 GiB");
    Require(automatic.Budget(1024*MemoryMiB,2*1024*MemoryMiB,0).cache==0,"automatic budget subtraction underflow");
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};a->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;}
    Require(device!=nullptr,"hardware device");
    if(ranges){
        Guest guest(4*MemoryMiB);Rasterizer raster(device.Get());raster.MapMemory(guest.Base(),guest.size);
        std::vector<Rasterizer::ShaderBufferRequest> requests;
        for(u64 i=0;i<1024;++i)requests.push_back({guest.Base()+i*4096,16});
        std::vector<VAddr> released;
        raster.SetBufferOwnership([](VAddr,u64){return false;},[](VAddr,std::span<const std::byte>){return false;},[&](VAddr a,u64){released.push_back(a);});
        raster.SetBufferWriteWatch([](VAddr,u64){return true;});
        raster.PrepareShaderBuffers(requests);raster.Finish();
        const auto candidates=raster.MemoryReuse().watch_candidates;
        const auto start=std::chrono::steady_clock::now();
        for(unsigned i=0;i<10000;++i)raster.ReleaseCpuBufferWatches(guest.Base()+512*4096+4,4);
        const auto visited=raster.MemoryReuse().watch_candidates-candidates;
        printf("watch_range mappings=1024 calls=10000 visited=%llu ms=%.3f\n",visited,
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        Require(released.size()==1&&released[0]==guest.Base()+512*4096,"watch predecessor or unrelated release");
        Require(visited==10000,"CPU watch invalidation scans unrelated resident mappings");
        raster.ReleaseCpuBufferWatches(guest.Base()+16,4096-16);
        Require(released.size()==1,"exclusive edge released neighbors");
        raster.UnmapMemory(guest.Base(),guest.size);raster.ValidateDebugMessages();return 0;
    }
    {
        PlacedBufferAllocator arena(device.Get(),256*MemoryMiB,512*MemoryMiB);arena.SetCompactLargePages(true);
        auto a=arena.Allocate(160*MemoryMiB),b=arena.Allocate(160*MemoryMiB),c=arena.Allocate(160*MemoryMiB);
        Require(arena.CommittedBytes()==480*MemoryMiB,"large backing fragmentation exceeds budget");
        arena.SetBudget(0);arena.Trim();Require(arena.CommittedBytes()==480*MemoryMiB,"trim discarded live leases");
        a={};b={};c={};arena.Trim();Require(arena.CommittedBytes()==0,"trim retained unleased heaps");
        bool rejected=false;try{arena.Allocate(65536);}catch(const BufferBudgetExhausted&){rejected=true;}
        Require(rejected,"zero pressure budget allowed new heap allocation");
    }
    {
        // Two live pages must share the configured budget. The second page
        // needs four MiB, not another complete eight-MiB page target.
        PlacedBufferAllocator arena(device.Get(),8*MemoryMiB,12*MemoryMiB);
        auto a=arena.Allocate(2*MemoryMiB),b=arena.Allocate(4*MemoryMiB),c=arena.Allocate(4*MemoryMiB);
        Require(arena.CommittedBytes()==12*MemoryMiB,"remaining placed-heap budget was not admitted");
        Rasterizer commands(device.Get());
        auto upload=Buffer(device.Get(),12,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto readback=Buffer(device.Get(),12,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const u32 expected[]{0x11223344,0x51525354,0x61626364};void* cpu{};D3D12_RANGE empty{0,0};
        Check(upload.handle->Map(0,&empty,&cpu));memcpy(cpu,expected,sizeof(expected));upload.handle->Unmap(0,nullptr);
        Resource* resources[]{&a,&b,&c};
        for(unsigned i=0;i<3;++i){auto& resource=*resources[i];resource.Transition(commands.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            commands.RecordingList()->CopyBufferRegion(resource.handle.Get(),0,upload.handle.Get(),i*4,4);
            resource.Transition(commands.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            commands.RecordingList()->CopyBufferRegion(readback.handle.Get(),i*4,resource.handle.Get(),0,4);}
        commands.Finish();D3D12_RANGE all{0,12};Check(readback.handle->Map(0,&all,&cpu));
        const bool valid=!memcmp(cpu,expected,sizeof(expected));readback.handle->Unmap(0,&empty);
        Require(valid,"second placed-heap page corrupted live resource contents");
        arena.SetBudget(0);arena.Trim();Require(arena.CommittedBytes()==12*MemoryMiB,"reduced budget destroyed live pages");
        bool rejected=false;try{arena.Allocate(16*MemoryMiB);}catch(const BufferBudgetExhausted&){rejected=true;}
        Require(rejected,"zero actual heap budget admitted another page");
        a={};b={};c={};arena.Trim();Require(arena.CommittedBytes()==0,"completed heap pages did not retire");
        commands.ValidateDebugMessages();
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
    if(preserved){RunPreservedBufferBudgetFixture(device.Get());puts("PASS: retained B1/selected generations, bounded reconstructible cache, semantic residency, CPU-store proofs and exact GPU visibility under pressure");return 0;}
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
