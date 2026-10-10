#include "common/api_trace.h"
#include "rasterizer.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("assertion failed");}
int main(){try{
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 info{};DX12_API_CALL(a->GetDesc1(&info));if(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))){wprintf(L"Hardware: %ls\n",info.Description);break;}}
    Require(device!=nullptr);
    {
        Rasterizer arenas(device.Get());arenas.EnableBufferArenas(true);
        constexpr size_t bytes=256*1024;
        auto* memory=static_cast<u32*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Require(memory!=nullptr);
        const auto base=reinterpret_cast<VAddr>(memory);arenas.MapMemory(base,bytes);arenas.NotifyHostMapping(base,bytes,true,true);
        for(size_t i=0;i<bytes/4;++i)memory[i]=u32(i);
        const Rasterizer::ShaderBufferRequest a[]{{base+16,65536,false,false}},b[]{{base+32,65536,false,false}};
        auto first=arenas.PrepareShaderBuffers(a);arenas.Finish();
        auto second=arenas.PrepareShaderBuffers(b);Require(first[0].resource.Get()==second[0].resource.Get()&&second[0].offset==32);
        memory[9]=123456;Require(arenas.InvalidateMemory(base+36,4));
        auto edited=arenas.PrepareShaderBuffers(b);Require(edited[0].resource.Get()==first[0].resource.Get());
        auto rb=Buffer(device.Get(),65536,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource source;source.handle=edited[0].resource;source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        source.Transition(arenas.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        arenas.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,source.handle.Get(),edited[0].offset,65536);
        source.Transition(arenas.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        arenas.Finish();void* data{};D3D12_RANGE range{0,65536};Check(rb.handle->Map(0,&range,&data));
        Require(memcmp(data,memory+8,65536)==0);D3D12_RANGE none{};rb.handle->Unmap(0,&none);
        arenas.UnmapMemory(base,bytes);VirtualFree(memory,0,MEM_RELEASE);
    }
    {
        // A stream page must survive Finish before its later shader use, and
        // distinct in-flight CPU versions must not overwrite each other.
        Rasterizer stream(device.Get());stream.EnableBufferStreaming(true);
        std::array<u32,1024> words{};words.fill(7);
        const auto base=reinterpret_cast<VAddr>(words.data());stream.MapMemory(base,sizeof(words));
        const Rasterizer::ShaderBufferRequest request[]{{base,sizeof(words),false,false}};
        auto first=stream.PrepareShaderBuffers(request);Require(first[0].lease&&stream.CachedBufferBytes()==0);
        stream.Finish();words.fill(11);auto second=stream.PrepareShaderBuffers(request);
        Require(first[0].resource.Get()!=second[0].resource.Get());
        auto output=Buffer(device.Get(),2*sizeof(words),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        stream.RecordingList()->CopyBufferRegion(output.handle.Get(),0,first[0].resource.Get(),first[0].offset,sizeof(words));
        stream.RecordingList()->CopyBufferRegion(output.handle.Get(),sizeof(words),second[0].resource.Get(),second[0].offset,sizeof(words));
        auto held=first[0].resource.Get();stream.RetainUntilFinish([lease=first](u64){});first.clear();
        words.fill(19);auto third=stream.PrepareShaderBuffers(request);Require(third[0].resource.Get()!=held);
        stream.Finish();void* data{};D3D12_RANGE all{0,2*sizeof(words)};Check(output.handle->Map(0,&all,&data));
        for(size_t i=0;i<words.size();++i){Require(static_cast<u32*>(data)[i]==7);Require(static_cast<u32*>(data)[words.size()+i]==11);}
        D3D12_RANGE none{};output.handle->Unmap(0,&none);
        auto reused=stream.PrepareShaderBuffers(request);Require(reused[0].resource.Get()==held);
        stream.Finish();stream.UnmapMemory(base,sizeof(words));
    }
    {
        Rasterizer pool(device.Get());std::array<u32,4> data{123,456,789,999};
        auto first=pool.AcquireTemporaryBuffer(sizeof(data),data.data());
        auto second=pool.AcquireTemporaryBuffer(sizeof(data),data.data());
        const auto address=reinterpret_cast<VAddr>(first->words.data());
        Require(first!=second&&pool.IsMapped(address,sizeof(data)));
        const Rasterizer::BufferCopy copies[]{{reinterpret_cast<VAddr>(second->words.data()),address,sizeof(data)}};
        pool.CopyBufferBatch(copies);Require(second->words==first->words);
        first.reset();data={7,8,9,10};
        auto reused=pool.AcquireTemporaryBuffer(sizeof(data),data.data());
        Require(reinterpret_cast<VAddr>(reused->words.data())==address);
        const Rasterizer::BufferCopy next[]{{reinterpret_cast<VAddr>(second->words.data()),address,sizeof(data)}};
        pool.CopyBufferBatch(next);Require(second->words==std::vector<u32>(data.begin(),data.end()));
    }
    std::array<u32,1024> a{},b{};Rasterizer raster(device.Get());
    auto aa=reinterpret_cast<VAddr>(a.data()),bb=reinterpret_cast<VAddr>(b.data());
    raster.RegisterMemory(aa,sizeof(a));raster.RegisterMemory(bb,sizeof(b));
    Require(raster.CachedBufferBytes()==0);Require(!raster.IsMapped(aa,sizeof(a)));
    raster.MapMemory(aa,sizeof(a));raster.MapMemory(bb,sizeof(b));
    constexpr VAddr huge=0x10000000000ull;raster.MapMemory(huge,64ull*1024*1024*1024);raster.RegisterMemory(huge,64ull*1024*1024*1024);Require(raster.CachedBufferBytes()==0);raster.UnmapMemory(huge+4096,4096);Require(raster.IsMapped(huge,4096));Require(!raster.IsMapped(huge+4096,4096));Require(raster.IsMapped(huge+8192,4096));bool sparse_rejected=false;try{raster.FillBuffer(huge,4,1,false);}catch(const std::runtime_error&){sparse_rejected=true;}Require(sparse_rejected);Require(raster.CachedBufferBytes()==0);raster.UnmapMemory(huge,64ull*1024*1024*1024);
    Require(raster.IsMapped(aa,sizeof(a)));Require(!raster.IsMapped(0,4));
    raster.FillBuffer(aa,sizeof(a),0x12345678,false);raster.CopyBuffer(bb,aa,sizeof(a),false,false);
    auto fence=raster.Flush();Require(fence>0);raster.Finish();
    for(auto value:a)Require(value==0x12345678);Require(a==b);
    raster.FillBuffer(8,16,0xaabbccdd,true);raster.CopyBuffer(bb+16,8,16,false,true);raster.Finish();
    Require(raster.ReadDataFromGds(8)==0xaabbccdd);for(unsigned i=4;i<8;++i)Require(b[i]==0xaabbccdd);
    raster.CopyBuffer(bb+4,bb,32,false,false);raster.Finish();Require(b[1]==b[0]);
    a[0]=0xdeadbeef;Require(raster.InvalidateMemory(aa,4));raster.CopyBuffer(bb,aa,4,false,false);raster.Finish();Require(b[0]==0xdeadbeef);
    bool unsupported=false;try{raster.Draw(false);}catch(const std::runtime_error&){unsupported=true;}Require(unsupported);
    {
        Rasterizer invalidation(device.Get());std::array<u32,64> words{};
        const auto base=reinterpret_cast<VAddr>(words.data());
        invalidation.MapMemory(base,sizeof(words));
        // A fresh file-read destination must remain CPU-only until a GPU use.
        words[0]=17;Require(invalidation.InvalidateMemory(base,sizeof(words)));
        Require(invalidation.CachedBufferBytes()==0);
        invalidation.FillBuffer(base,sizeof(words),5,false);
        Require(invalidation.CachedBufferBytes()==0);for(auto value:words)Require(value==5);
        // A draw may own the execution lock while a fresh file buffer is being
        // loaded. The CPU-only range must complete without releasing that lock.
        std::atomic<bool> fresh_completed{};
        std::unique_lock rendering(invalidation.ExecutionMutex());
        std::jthread loader([&]{Require(invalidation.InvalidateMemory(base,sizeof(words)));fresh_completed=true;});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!fresh_completed&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
        const bool independent=fresh_completed.load();rendering.unlock();loader.join();Require(independent);
        // Invalidate a CPU-written subrange while the surrounding GPU write is
        // still pending. Both ownership domains must survive lazy eviction.
        const Rasterizer::ShaderBufferRequest pending_write[]{{base,sizeof(words),true,true}};
        invalidation.PrepareShaderBuffers(pending_write);
        invalidation.FillBuffer(base,sizeof(words),9,false);
        words[8]=23;Require(invalidation.InvalidateMemory(base+32,4));
        Require(invalidation.CachedBufferBytes()==sizeof(words));
        for(unsigned i=0;i<words.size();++i)Require(words[i]==(i==8?23:9));
        invalidation.CopyBuffer(base+36,base+32,4,false,false);
        invalidation.Finish();Require(words[9]==23);
        invalidation.UnmapMemory(base,sizeof(words));
    }
    raster.FillBuffer(aa+128,16,0xcafebabe,false);
    raster.UnmapMemory(aa+128,16);Require(a[32]==0xcafebabe);
    Require(raster.IsMapped(aa,128));Require(!raster.IsMapped(aa+128,16));Require(raster.IsMapped(aa+144,sizeof(a)-144));
    raster.MapMemory(aa+128,16);Require(raster.IsMapped(aa,sizeof(a)));
    // Memory callbacks may arrive from file-loading threads while the guest
    // GPU thread records work. Exercise shared command-list/cache ownership.
    std::atomic<bool> failed=false;
    std::vector<std::jthread> concurrent;
    for(unsigned worker=0;worker<4;++worker)concurrent.emplace_back([&,worker]{
        try {for(unsigned iteration=0;iteration<32;++iteration){
            const auto address=aa+512+worker*128;
            raster.FillBuffer(address,128,(worker<<16)|iteration,false);
            if(!raster.ReadMemory(address,128))failed=true;
            raster.Finish();
        }}catch(...){failed=true;}
    });
    concurrent.clear();Require(!failed);raster.Finish();
    for(unsigned worker=0;worker<4;++worker)for(unsigned word=0;word<32;++word)
        Require(a[128+worker*32+word]==((worker<<16)|31));
    raster.UnmapMemory(aa,sizeof(a));raster.UnmapMemory(bb,sizeof(b));
    {
        Rasterizer split(device.Get());std::array<u32,64> words{};
        for(u32 i=0;i<words.size();++i)words[i]=i+100;
        const auto base=reinterpret_cast<VAddr>(words.data());split.MapMemory(base,sizeof(words));
        const Rasterizer::ShaderBufferRequest large[]{{base,128,false,false}};
        split.PrepareShaderBuffers(large);split.Finish();Require(split.CachedBufferBytes()==128);
        // Later disjoint requests partially intersect one old larger mapping.
        // Neither request may keep an erased resource or inherit mixed states.
        const Rasterizer::ShaderBufferRequest slices[]{{base,32,false,false},{base+96,64,true,true}};
        auto views=split.PrepareShaderBuffers(slices);Require(views[0].resource.Get()!=views[1].resource.Get());
        Require(views[0].size==32&&views[1].size==64&&split.CachedBufferBytes()==96);
        split.Finish();split.CopyBuffer(base+96,base,32,false,false);split.Finish();
        for(u32 i=0;i<8;++i)Require(words[24+i]==words[i]);
        for(u32 i=32;i<64;++i)Require(words[i]==i+100);
        split.UnmapMemory(base,sizeof(words));Require(split.CachedBufferBytes()==0);
    }
    {
        Rasterizer coherence(device.Get());std::array<u32,64> words{};
        const auto address=reinterpret_cast<VAddr>(words.data());coherence.MapMemory(address,sizeof(words));
        const Rasterizer::ShaderBufferRequest read[]{{address,sizeof(words),false,true}};
        coherence.PrepareShaderBuffers(read);coherence.Finish();
        coherence.FillBuffer(address,sizeof(words),9,false);coherence.Finish();
        words.fill(0); // Return CPU memory to the old snapshot without notification.
        auto views=coherence.PrepareShaderBuffers(read);
        Resource source;source.handle=views[0].resource;source.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        auto result=Buffer(device.Get(),4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        auto* list=coherence.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        DX12_API_CALL(list->CopyBufferRegion(result.handle.Get(),0,source.handle.Get(),0,4));
        source.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);coherence.Finish();
        void* data{};D3D12_RANGE range{0,4};Check(DX12_API_CALL(result.handle->Map(0,&range,&data)));
        const auto actual=*static_cast<u32*>(data);D3D12_RANGE none{};DX12_API_CALL(result.handle->Unmap(0,&none));
        Require(actual==0);
    }
    {
        // A writable buffer's retired GPU version must survive the next
        // binding, while CPU edits on separate 4 KiB blocks reach the GPU.
        Rasterizer writable(device.Get());std::array<u32,3073> words{};words.fill(7);
        const auto address=reinterpret_cast<VAddr>(words.data());writable.MapMemory(address,sizeof(words));
        const Rasterizer::ShaderBufferRequest request[]{{address,sizeof(words),true,true}};
        auto views=writable.PrepareShaderBuffers(request);
        auto value=Buffer(device.Get(),4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE empty{};Check(value.handle->Map(0,&empty,&mapped));
        *static_cast<u32*>(mapped)=1234;value.handle->Unmap(0,nullptr);
        Resource source;source.handle=views[0].resource;source.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        source.Transition(writable.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        writable.RecordingList()->CopyBufferRegion(source.handle.Get(),views[0].offset+4096,value.handle.Get(),0,4);
        source.Transition(writable.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        writable.Finish();Require(words[1024]==1234);
        for(unsigned pass=0;pass<3;++pass){
            words[0]=40+pass;words[2048]=60+pass;words.back()=80+pass;
            auto next=writable.PrepareShaderBuffers(request);Require(next[0].resource.Get()==views[0].resource.Get());
            Resource current;current.handle=next[0].resource;current.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            auto rb=Buffer(device.Get(),sizeof(words),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            current.Transition(writable.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            writable.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,current.handle.Get(),next[0].offset,sizeof(words));
            current.Transition(writable.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            writable.Finish();D3D12_RANGE range{0,sizeof(words)};Check(rb.handle->Map(0,&range,&mapped));
            Require(memcmp(mapped,words.data(),sizeof(words))==0&&words[1024]==1234);
            rb.handle->Unmap(0,&empty);
        }
    }
    {
        // Large unchanged shader batches must retain resident GPU resources.
        // Exercise the actual adapter-derived budget, including real eviction,
        // rather than assuming the old fixed 512-MiB cap.
        struct Allocation{void* p{};~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} owner,extra_owner;
        Rasterizer reuse(device.Get());const auto budget=reuse.BufferCacheBudget();
        const u64 slice=std::min<u64>(160ULL*1024*1024,budget/4)&~u64(65535);
        const u64 count=budget/slice,bytes=count*slice;
        auto* storage=static_cast<u32*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        Require(storage!=nullptr);
        owner.p=storage;
        const auto address=reinterpret_cast<VAddr>(storage);
        reuse.MapMemory(address,bytes);
        std::vector<Rasterizer::ShaderBufferRequest> batch;
        for(u64 i=0;i<count;++i)batch.push_back({address+i*slice,slice,false,true});
        auto first=reuse.PrepareShaderBuffers(batch);reuse.Finish();
        auto second=reuse.PrepareShaderBuffers(batch);reuse.Finish();
        Require(first[0].resource.Get()==second[0].resource.Get()&&first[1].resource.Get()==second[1].resource.Get());
        Require(reuse.CachedBufferBytes()==bytes);
        storage[0]=0x11223344;storage[bytes/4-1]=0xaabbccdd;
        auto changed=reuse.PrepareShaderBuffers(batch);reuse.Finish();
        Require(changed[0].resource.Get()==first[0].resource.Get()&&changed[1].resource.Get()==first[1].resource.Get());
        // Completed command views must retire before testing inactive eviction.
        // Keep the first view pinned: the pressure batch still needs that resource.
        first.resize(1);second.clear();changed.clear();
        const u64 extra_bytes=slice*3/2;
        auto* extra=VirtualAlloc(nullptr,extra_bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);Require(extra!=nullptr);
        extra_owner.p=extra;const auto extra_address=reinterpret_cast<VAddr>(extra);reuse.MapMemory(extra_address,extra_bytes);
        const Rasterizer::ShaderBufferRequest pressure[]{{address,slice,false,true},{extra_address,extra_bytes,false,true}};
        auto retained=reuse.PrepareShaderBuffers(pressure);reuse.Finish();
        Require(retained[0].resource.Get()==first[0].resource.Get());
        const auto pressure_budget=reuse.BufferCacheBudget();
        const auto evicted=bytes+extra_bytes>pressure_budget?(bytes+extra_bytes-pressure_budget+slice-1)/slice:0;
        Require(evicted>0&&reuse.CachedBufferBytes()==bytes+extra_bytes-evicted*slice);
        std::array<u32,4> output{};const auto destination=reinterpret_cast<VAddr>(output.data());reuse.MapMemory(destination,sizeof(output));
        const Rasterizer::BufferCopy copies[]{{destination,address,4},{destination+4,address+bytes-4,4}};
        reuse.CopyBufferBatch(copies);
        Require(output[0]==0x11223344&&output[1]==0xaabbccdd&&output[2]==0&&output[3]==0);
    }
    {
        // Two CPU versions of a read-only guest buffer must reach the GPU in
        // order even when both copies are recorded before a single retirement.
        Rasterizer batch(device.Get());std::array<u32,3073> words{};words.fill(7);
        const auto address=reinterpret_cast<VAddr>(words.data());batch.MapMemory(address,sizeof(words));
        constexpr unsigned passes=24;
        auto output=Buffer(device.Get(),passes*sizeof(words),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const Rasterizer::ShaderBufferRequest request[]{{address,sizeof(words),false,false}};
        for(unsigned pass=0;pass<passes;++pass){
            words[0]=pass+20;words[2048]=pass+40;words.back()=pass+60;
            auto views=batch.PrepareShaderBuffers(request);
            Resource source;source.handle=views[0].resource;
            source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            auto* list=batch.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            DX12_API_CALL(list->CopyBufferRegion(output.handle.Get(),pass*sizeof(words),source.handle.Get(),0,sizeof(words)));
            source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            batch.Flush();
        }
        batch.Finish();void* mapped{};D3D12_RANGE range{0,passes*sizeof(words)};Check(DX12_API_CALL(output.handle->Map(0,&range,&mapped)));
        const auto* values=static_cast<const u32*>(mapped);
        for(unsigned pass=0;pass<passes;++pass)for(unsigned word=0;word<words.size();++word)
            Require(values[pass*words.size()+word]==(word==0?pass+20:word==2048?pass+40:word==words.size()-1?pass+60:7));
        D3D12_RANGE none{};DX12_API_CALL(output.handle->Unmap(0,&none));
    }
    Require(GetModuleHandleW(L"vulkan-1.dll")==nullptr);
    puts("PASS: native D3D12 mapped guest fill/copy/readback, GDS, same-resource copy, CPU invalidation, unsupported Draw explicit");return 0;
}catch(const std::exception&e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}



