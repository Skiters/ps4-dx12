// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_buffer_access.h"
#include "common/scope_exit.h"
#include "buffer_envelope_fixture.h"
#include "semantic_buffer_residency_fixture.h"
#include "video_core/renderer_d3d12/guest_indirect_arguments.h"
#include <cstdio>
#include <future>
using namespace NativeDX12;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Fixture final : public GuestBufferAccess {
    std::vector<std::pair<VAddr,u64>> retained_images_;
    std::vector<std::pair<VAddr,u64>> RetainedImageProtectionRanges()const override{return retained_images_;}
    std::function<void()> before_read_;
    bool opaque_{};
    std::function<bool(VAddr,std::span<std::byte>)> physical_;
    bool HasGpuImageOverlap(VAddr,u64)const override{return opaque_;}
    bool ReadDmaBacking(VAddr address,std::span<std::byte> bytes)override{
        return physical_?physical_(address,bytes):!opaque_&&Rasterizer::ReadDmaBacking(address,bytes);
    }
    void ReleaseBufferPageWatch(VAddr address,u64 bytes)override{
        ++guard_releases;GuestBufferAccess::ReleaseBufferPageWatch(address,bytes);
    }
public:
    void RetainedImages(std::vector<std::pair<VAddr,u64>> ranges){retained_images_=std::move(ranges);}
    unsigned guard_releases{};
    using GuestBufferAccess::GuestBufferAccess;
    using GuestBufferAccess::SetBufferBackingAccess;
    bool TrackReadOnly(VAddr address,u64 bytes){return page_watch_->TrackReadOnly(address,bytes);}
    bool TrackOpaque(VAddr address,u64 bytes){return page_watch_->Track(address,bytes);}
    void PhysicalBackingChanged(VAddr address,u64 bytes){page_watch_->InvalidateBackingEpoch(address,bytes);}
    void Opaque(bool value,std::function<bool(VAddr,std::span<std::byte>)> physical={}){
        opaque_=value;physical_=std::move(physical);
    }
    void Untrack(VAddr address,u64 bytes){page_watch_->Untrack(address,bytes);}
    void BeforeRead(std::function<void()> callback){before_read_=std::move(callback);}
    void PhysicalReader(std::function<bool(VAddr,std::span<std::byte>)> read){physical_=std::move(read);}
    void BeforeWatch(std::function<void(VAddr,u64)> hook){
        SetBufferWriteWatch([this,hook=std::move(hook)](VAddr address,u64 bytes){
            hook(address,bytes);
            return address%4096==0&&bytes%4096==0&&has_backing_(address,bytes)&&page_watch_->TrackReadOnly(address,bytes);
        });
    }
    bool ReadMemory(VAddr a,u64 s,bool assume=false) override {
        if(before_read_){auto callback=std::move(before_read_);before_read_={};callback();}
        return Rasterizer::ReadMemory(a,s,assume);
    }
    bool PrepareCpuUpload(VAddr a,u64 s) override {
        if(before_read_){auto callback=std::move(before_read_);before_read_={};callback();}
        return Rasterizer::PrepareCpuUpload(a,s);
    }
};
static u64 Counter(const char* name){auto& hub=Diagnostics::Hub::Get();std::lock_guard lock(hub.mutex);
    for(const auto& c:hub.counters)if(c.name&&std::string_view(c.name)==name)return c.value;return 0;}
static void FrameBufferReadback(ID3D12Device* device,std::byte* guest,std::byte* physical,VAddr base){
    constexpr u64 total=65536,page=4096;
    Fixture renderer(device);renderer.MapMemory(base,total);renderer.NotifyHostMapping(base,total,true,true);
    renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=total&&n<=total-(a-base);},
        [&](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
    const Rasterizer::ShaderBufferRequest requests[]{{base,total,false,true,true},
        {base+page,page,true,true,true},{base+8*page,page,true,true,true}};
    const auto arm=[&]{
        renderer.ResolveGpuBuffers(base,total);renderer.NotifyCpuWrite(base,total);memset(physical,0x4d,total);
        (void)renderer.PrepareShaderBuffers(requests);
        renderer.FillPreparedShaderBuffer(base+page,page,0x11111111,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillPreparedShaderBuffer(base+8*page,page,0x22222222,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.MarkShaderBufferWrites(std::span(requests+1,2));
    };
    const auto wait_copy=[&]{
        const auto fence=renderer.SubmittedFence();Require(fence!=0,"Frame copy did not submit");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(renderer.CompletedFence()<fence){
            Require(std::chrono::steady_clock::now()<deadline,"Frame copy did not complete");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };
    const auto check=[&](unsigned first,unsigned second){
        for(u64 i=0;i<total;++i){const auto expected=i>=page&&i<2*page?first:
            i>=8*page&&i<9*page?second:0x4d;
            Require(physical[i]==std::byte(expected),"Frame readback lost current bytes or physical padding");}
    };
    arm();const auto before_bytes=Counter("buffer-readback.frame-prefetched.bytes");
    const auto before_consumed=Counter("buffer-readback.frame-consumed");
    renderer.OnFrame();wait_copy();check(0x4d,0x4d);
    Require(Counter("buffer-readback.frame-prefetched.bytes")==before_bytes+2*page,
        "Frame prefetch did not compact exactly the two GPU-modified islands");
    auto submitted=std::make_shared<bool>(false);renderer.NotifyOnSubmission([submitted](u64){*submitted=true;});
    renderer.ResolveGpuBuffers(base+page,page);check(0x11,0x4d);
    Require(!*submitted&&renderer.IsGpuBufferModified(base+8*page,page),
        "Ready prefetch submitted/waited unrelated GPU work or retired the other island");
    renderer.ResolveGpuBuffers(base+8*page,page);check(0x11,0x22);
    // The first trusted commit may invalidate a coarser physical watch node.
    // A later page then conservatively follows the ordinary current readback.
    Require(Counter("buffer-readback.frame-consumed")>=before_consumed+1&&
        Counter("buffer-readback.frame-consumed")<=before_consumed+2,
        "Ready compact readback was not consumed");
    renderer.Finish(); // Drain the unused submission observer before its stack lifetime ends.
    arm();renderer.OnFrame();wait_copy();const auto second_first=Counter("buffer-readback.frame-consumed");
    renderer.ResolveGpuBuffers(base+8*page,page);check(0x4d,0x22);
    Require(Counter("buffer-readback.frame-consumed")==second_first+1,"Second compact staging offset did not retain exact bytes");
    arm();renderer.OnFrame();wait_copy();
    renderer.FillPreparedShaderBuffer(base+page,page,0x33333333,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.MarkShaderBufferWrites(std::span(requests+1,1));
    auto consumed=Counter("buffer-readback.frame-consumed");
    renderer.ResolveGpuBuffers(base+page,page);check(0x33,0x4d);
    Require(Counter("buffer-readback.frame-consumed")==consumed,"Late same-range GPU write consumed a stale frame copy");
    arm();renderer.OnFrame();wait_copy();renderer.ResolveGpuBuffers(base+page,page);
    *reinterpret_cast<volatile unsigned char*>(guest+page)=0x7f;
    consumed=Counter("buffer-readback.frame-consumed");renderer.ResolveGpuBuffers(base+8*page,page);
    Require(physical[page]==std::byte{0x7f}&&physical[8*page]==std::byte{0x22}&&
        Counter("buffer-readback.frame-consumed")==consumed,"CPU winner retained stale frame staging or was overwritten");
    arm();renderer.OnFrame();wait_copy();physical[page]=std::byte{0x99};
    renderer.PhysicalBackingChanged(base+page,1);consumed=Counter("buffer-readback.frame-consumed");
    renderer.ResolveGpuBuffers(base+page,page);check(0x11,0x4d);
    Require(Counter("buffer-readback.frame-consumed")==consumed,"Changed physical backing epoch retained stale staging");
    arm();renderer.OnFrame();wait_copy();
    const std::vector<std::byte> image(page,std::byte{0x66});
    renderer.CommitImageBacking(base+page,image,
        [&](VAddr a,std::span<std::byte> out){memcpy(out.data(),physical+a-base,out.size());return true;},
        [&](VAddr a,std::span<const std::byte> in){memcpy(physical+a-base,in.data(),in.size());return true;});
    consumed=Counter("buffer-readback.frame-consumed");renderer.ResolveGpuBuffers(base+8*page,page);check(0x66,0x22);
    Require(Counter("buffer-readback.frame-consumed")==consumed,"Physical image publication retained stale buffer staging");
    arm();renderer.OnFrame();wait_copy();const auto captured=Counter("buffer-readback.frame-prefetched.bytes");
    renderer.OnFrame();renderer.OnFrame();renderer.OnFrame();
    Require(Counter("buffer-readback.frame-prefetched.bytes")==captured,"Aged unused staging recopied an unchanged version");
    consumed=Counter("buffer-readback.frame-consumed");renderer.ResolveGpuBuffers(base+page,page);check(0x11,0x4d);
    Require(Counter("buffer-readback.frame-consumed")==consumed,"Expired frame staging bypassed the ordinary readback");
    arm();ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
    SCOPE_EXIT {gate->Signal(1);};
    renderer.WaitExternalFence(gate.Get(),1);renderer.OnFrame();
    Require(renderer.CompletedFence()<renderer.SubmittedFence(),"Pending-copy negative lost its GPU gate");
    consumed=Counter("buffer-readback.frame-consumed");
    // Only the ordinary fallback submission may release this GPU gate; no
    // timing assumption about worker/desktop scheduling can turn it ready.
    renderer.NotifyOnSubmission([gate](u64){Check(gate->Signal(1));});
    renderer.ResolveGpuBuffers(base+page,page);check(0x11,0x4d);
    Require(Counter("buffer-readback.frame-consumed")==consumed,"Incomplete frame copy was mapped as completed data");
    renderer.ResolveGpuBuffers(base,total);renderer.ValidateDebugMessages();
    std::puts("PASS: compact frame readback, demand publication, no ready-copy submit, GPU/CPU/image winners, expiry and pending fallback");
}
static void ProtectedDmaBacking(ID3D12Device* device,std::byte* guest,std::byte* physical,VAddr base){
    constexpr u64 bytes=4096;std::vector<std::byte> original(physical,physical+bytes);
    Fixture renderer(device);renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
    renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
        [&](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
    SCOPE_EXIT {renderer.Finish();renderer.Opaque(false);renderer.Untrack(base,bytes);memcpy(physical,original.data(),bytes);};
    const Rasterizer::ShaderBufferRequest ranges[]{{base,bytes,false,true,true}};
    const auto views=renderer.PrepareShaderBuffers(ranges);
    renderer.FillPreparedShaderBuffer(base,bytes,0x6b6b6b6b,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.ReleaseCpuBufferWatches(base,bytes);
    u64 physical_reads{};
    renderer.Opaque(true,[&](VAddr a,std::span<std::byte> out){
        if(a<base||a-base>bytes||out.size()>bytes-(a-base))return false;
        ++physical_reads;
        memcpy(out.data(),physical+a-base,out.size());return true;});
    Require(renderer.TrackOpaque(base,bytes),"opaque image guard setup");
    const auto probe=[&](bool cpu_store,bool synchronize=true){
        if(synchronize)renderer.SynchronizePreparedDmaBuffers(ranges,views);
        MEMORY_BASIC_INFORMATION state{};VirtualQuery(guest,&state,sizeof(state));
        Require(state.Protect==PAGE_NOACCESS,"DMA CPU synchronization resolved an opaque I guard");
        auto snapshot=renderer.CapturePreparedShaderBuffer(base,bytes);
        auto readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,bytes);
        renderer.Finish();void* data{};D3D12_RANGE all{0,size_t(bytes)};Check(readback.handle->Map(0,&all,&data));
        SCOPE_EXIT {D3D12_RANGE none{};readback.handle->Unmap(0,&none);};
        if(cpu_store)Require(!memcmp(data,physical,bytes),"DMA did not upload the actual CPU winner through physical backing");
        else for(u64 i=0;i<bytes;++i)Require(static_cast<std::byte*>(data)[i]==std::byte{0x6b},
            "DMA replaced GPU-only B with an unchanged physical CPU snapshot");
    };
    probe(false);const auto established=physical_reads;Require(established,"Missing initial physical comparison");
    probe(false);(void)renderer.PrepareShaderBuffers(ranges);probe(false);
    Require(physical_reads==established,"Warm protected B repeated full physical reads beside opaque I");
    renderer.ReleaseCpuBufferWatches(base,bytes);probe(false);
    Require(physical_reads==established,"Non-owning B proof retired or rescanned the opaque image guard");
    physical[17]=std::byte{0x88};renderer.PhysicalBackingChanged(base+17,1);probe(true);
    Require(physical_reads>established,"Physical image publication retained a stale B epoch");
    renderer.FillPreparedShaderBuffer(base,bytes,0x6b6b6b6b,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.NotifyCpuWrite(base+17,1);probe(true); // Same-value store still restores the CPU page.
    const auto before_rearm=physical_reads;
    renderer.Untrack(base,bytes);physical[23]=std::byte{0x39};
    Require(renderer.TrackOpaque(base,bytes),"opaque guard rearm");probe(true);
    Require(physical_reads>before_rearm,"Retired/rearmed image guard reused an obsolete physical proof");
    bool published=false;
    renderer.BeforeWatch([&](VAddr,u64){if(!published){published=true;
        physical[29]=std::byte{0xc7};renderer.PhysicalBackingChanged(base+29,1);
    }});
    (void)renderer.PrepareShaderBuffers(ranges);probe(true,false);
    Require(published,"Missing reentrant physical publication during guard acquisition");
    puts("PASS: opaque-I physical B epoch, warm direct/DMA zero rereads, independent GPU contents, host publication, same-value store and guard rearm");
    renderer.ValidateDebugMessages();
}
static void UnalignedPageWitness(ID3D12Device* device,std::byte* guest,std::byte* physical,VAddr base){
    constexpr u64 offset=64,bytes=8192-128;memset(physical,0x4d,65536);
    {
    Fixture renderer(device);renderer.MapMemory(base,65536);renderer.NotifyHostMapping(base,65536,true,true);
    renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=65536&&n<=65536-(a-base);},
        [&](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
    const Rasterizer::ShaderBufferRequest request[]{{base+offset,bytes,false,true,true}};
    (void)renderer.PrepareShaderBuffers(request);MEMORY_BASIC_INFORMATION state{};VirtualQuery(guest,&state,sizeof(state));
    Require(state.Protect==PAGE_READONLY,"Unaligned B did not establish its complete physical-page CPU witness");
    const auto comparisons=Counter("buffer-snapshot.bytes");
    (void)renderer.PrepareShaderBuffers(request);
    Require(Counter("buffer-snapshot.bytes")==comparisons,"Warm unaligned B compared its full logical envelope");
    renderer.FillPreparedShaderBuffer(base+offset,bytes,0x6b6b6b6b,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const auto verify=[&](bool tail){
        (void)renderer.PrepareShaderBuffers(request);
        auto buffer=renderer.CapturePreparedShaderBuffer(base+offset,bytes);
        auto readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        buffer.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,buffer.handle.Get(),0,bytes);
        renderer.Finish();void* data{};Check(readback.handle->Map(0,nullptr,&data));
        bool equal=true;for(u64 i=0;i<bytes;++i)equal&=static_cast<const std::byte*>(data)[i]==
            (tail||i<4096-offset?std::byte{0x4d}:std::byte{0x6b});
        readback.handle->Unmap(0,nullptr);Require(equal,"Neighbour CPU store lost page-granular B ownership or untouched GPU bytes");
    };
    // These are genuine CPU stores OUTSIDE the logical mapping but inside its
    // watched pages. Both must fault, dirty the page and retire the right proof.
    *reinterpret_cast<volatile unsigned char*>(guest+17)=0xe7;verify(false);
    *reinterpret_cast<volatile unsigned char*>(guest+8192-17)=0x92;verify(true);
    renderer.ReleaseCpuBufferWatches(base+17,1);VirtualQuery(guest,&state,sizeof(state));
    Require(state.Protect==PAGE_READWRITE,"Neighbour watch retirement left an unowned protected page");
    renderer.ValidateDebugMessages();
    }
    {
        memset(physical,0x4d,65536);Fixture renderer(device);renderer.MapMemory(base,65536);
        renderer.NotifyHostMapping(base,65536,true,true);
        renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=65536&&n<=65536-(a-base);},
            [&](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
        const Rasterizer::ShaderBufferRequest requests[]{{base+64,512,false,true,true},{base+2048,512,false,true,true}};
        auto views=renderer.PrepareShaderBuffers(requests);
        Require(views[0].resource.Get()!=views[1].resource.Get(),"Shared-page fixture needs two separate logical B mappings");
        for(const auto& request:requests)renderer.FillPreparedShaderBuffer(request.address,request.size,
            0x6b6b6b6b,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        *reinterpret_cast<volatile unsigned char*>(guest+1024)=0xa9;
        (void)renderer.PrepareShaderBuffers(requests);
        auto readback=Buffer(device,1024,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        for(unsigned i=0;i<2;++i){auto buffer=renderer.CapturePreparedShaderBuffer(requests[i].address,512);
            buffer.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),i*512,buffer.handle.Get(),0,512);}
        renderer.Finish();void* data{};Check(readback.handle->Map(0,nullptr,&data));
        bool equal=true;for(unsigned i=0;i<1024;++i)equal&=static_cast<const std::byte*>(data)[i]==std::byte{0x4d};
        readback.handle->Unmap(0,nullptr);Require(equal,"One neighbour store did not dirty both logical B witnesses on its physical page");
        renderer.ReleaseCpuBufferWatches(base+1024,1);MEMORY_BASIC_INFORMATION state{};VirtualQuery(guest,&state,sizeof(state));
        Require(state.Protect==PAGE_READWRITE,"Shared-page retirement retained a stale neighbour proof");
        renderer.ValidateDebugMessages();
    }
    {
        Fixture renderer(device);renderer.MapMemory(base+64,512);renderer.NotifyHostMapping(base+64,512,true,true);
        renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base+64&&a-(base+64)<=512&&n<=512-(a-(base+64));},
            [](VAddr,std::span<const std::byte>){return false;});
        const Rasterizer::ShaderBufferRequest request{base+64,512,false,true,true};
        (void)renderer.PrepareShaderBuffers(std::span(&request,1));MEMORY_BASIC_INFORMATION state{};
        VirtualQuery(guest,&state,sizeof(state));Require(state.Protect==PAGE_READWRITE,
            "Rounded witness claimed physical neighbours outside authorized guest backing");
        renderer.Finish();renderer.ValidateDebugMessages();
    }
    puts("PASS: unaligned readonly B, zero warm full comparisons, real neighbouring CPU stores, page winners and untouched GPU bytes");
}
static void IndirectArguments(ID3D12Device* device,std::byte* guest,std::byte* physical,VAddr base){
    constexpr u64 total=1024*1024,page=4096;
    Require(base%(512*1024)==0,"Indirect fixture needs deterministic CPU download windows");
    Fixture renderer(device);renderer.MapMemory(base,total);renderer.NotifyHostMapping(base,total,true,true);
    renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=total&&n<=total-(a-base);},
        [&](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
    const std::array<u32,9> initial{1,3,2,7,9,8,4,11,13};
    memcpy(physical,initial.data(),sizeof(initial));
    const Rasterizer::ShaderBufferRequest unrelated{base+768*1024,page,true,true,true};
    (void)renderer.PrepareShaderBuffers(std::span(&unrelated,1));
    renderer.FillPreparedShaderBuffer(unrelated.address,page,0x55555555,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.MarkShaderBufferWrites(std::span(&unrelated,1));
    bool submitted{};renderer.NotifyOnSubmission([&](u64){submitted=true;});
    Require(renderer.ReadMemory(base,4),"Clean indirect count read failed");
    u32 count{};memcpy(&count,guest,4);
    const auto clean=GuestIndirectLayout::Make(false,base,4,16,count);
    Require(renderer.ReadMemory(clean.address,clean.bytes),"Clean indirect command read failed");
    const auto before=clean.Snapshot({guest+4,size_t(clean.bytes)});
    Require(!submitted&&before.size()==1&&before[0].count==3&&before[0].first_instance==9,
        "Clean indirect snapshot submitted unrelated GPU work or changed CPU arguments");
    const Rasterizer::ShaderBufferRequest arguments{base,page,true,true,true};
    (void)renderer.PrepareShaderBuffers(std::span(&arguments,1));
    // Actual GPU writes, not a CPU replacement for a GPU-produced count/list.
    const std::array<u32,9> generated{2,12,5,17,19,23,6,29,31};
    for(u32 word=0;word<generated.size();++word)
        renderer.FillPreparedShaderBuffer(base+word*4,4,generated[word],D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.MarkShaderBufferWrites(std::span(&arguments,1));
    Require(renderer.ReadMemory(base,4),"GPU indirect count reconciliation failed");
    memcpy(&count,guest,4);count=std::min(count,3u);
    const auto layout=GuestIndirectLayout::Make(false,base,4,16,count);
    Require(renderer.ReadMemory(layout.address,layout.bytes),"GPU indirect argument reconciliation failed");
    const auto snapshot=layout.Snapshot({guest+4,size_t(layout.bytes)});
    Require(snapshot.size()==2&&snapshot[0].count==12&&snapshot[0].instances==5&&snapshot[0].first_vertex==17&&
        snapshot[0].first_instance==19&&snapshot[1].count==23&&snapshot[1].instances==6&&
        snapshot[1].first_vertex==29&&snapshot[1].first_instance==31,
        "Indirect snapshot lost GPU count, payload, first vertex or instance");
    Require(renderer.IsGpuBufferModified(unrelated.address,page),"Indirect read published an unrelated download window");
    Require(!memcmp(physical,generated.data(),sizeof(generated)),"GPU argument readback did not reach guest backing");
    Require(before[0].count==3&&before[0].first_instance==9,"Later GPU arguments mutated the earlier snapshot");
    renderer.Finish();renderer.ValidateDebugMessages();
    puts("PASS: clean indirect reads avoid queue submission; GPU count/arguments reconcile with immutable snapshots and unrelated ownership");
}
static void CpuDownloadWindow(ID3D12Device* device,std::byte* guest,std::byte* physical,VAddr base){
    constexpr u64 window=512*1024,total=2*window,page=4096;
    Require(base%window==0,"CPU download fixture needs a deterministic original arena window");
    Fixture renderer(device);renderer.MapMemory(base,total);renderer.NotifyHostMapping(base,total,true,true);
    u64 committed{};bool reject_second{};
    const Rasterizer::ShaderBufferRequest requests[]{{base+page,page,true,true,true},
        {base+window/2,page,true,true,true},{base+window+page,page,true,true,true},
        {base+window+window/2,page,true,true,true}};
    renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=total&&n<=total-(a-base);},
        [&](VAddr a,std::span<const std::byte> data){
            if(reject_second&&a==requests[1].address)return false;
            memcpy(physical+a-base,data.data(),data.size());committed+=data.size();return true;
        });
    const auto arm=[&]{
        renderer.ResolveGpuBuffers(base,total);renderer.NotifyCpuWrite(base,total);memset(physical,0x4d,total);
        (void)renderer.PrepareShaderBuffers(requests);
        for(unsigned i=0;i<4;++i)renderer.FillPreparedShaderBuffer(requests[i].address,page,
            0x11111111u*(i+1),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.MarkShaderBufferWrites(requests);
        committed=0;
    };
    const auto check_first_window=[&]{
        Require(committed==2*page,"CPU read did not download exactly the two modified islands in its512KiB window");
        std::vector<std::byte> expected(total,std::byte{0x4d});
        for(unsigned i=0;i<2;++i)memset(expected.data()+requests[i].address-base,0x11*(i+1),page);
        Require(!memcmp(physical,expected.data(),total),"CPU window lost GPU contents, sparse holes or out-of-window physical bytes");
        for(unsigned i=0;i<4;++i){
            Require(renderer.IsGpuBufferModified(requests[i].address,page)==(i>=2),"CPU window retired the wrong GPU-modified domain");
            MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(requests[i].address),&state,sizeof(state));
            Require(state.Protect==(i>=2?PAGE_NOACCESS:PAGE_READONLY),"CPU window changed an unrelated protection owner");
        }
    };
    // Whole native envelopes may be strongly owned although only one sharp's
    // pages are GPU-modified. A CPU store to a clean guarded page still has
    // to retire that page's owner, without downloading the dirty neighbour.
    const Rasterizer::ShaderBufferRequest envelope[]{{base,total,false,true,true},{base,page,true,true,true}};
    (void)renderer.PrepareShaderBuffers(envelope);
    renderer.FillPreparedShaderBuffer(base,page,0x55555555,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    renderer.MarkShaderBufferWrites(std::span(envelope+1,1));
    const auto clean=base+window+17;MEMORY_BASIC_INFORMATION guard{};
    VirtualQuery(reinterpret_cast<void*>(clean),&guard,sizeof(guard));
    Require(guard.Protect==PAGE_NOACCESS&&!renderer.IsGpuBufferModified(clean,1),
        "Clean-page regression did not establish a strong unmodified owner");
    bool submitted=false;renderer.NotifyOnSubmission([&](u64){submitted=true;});
    committed=0;*reinterpret_cast<volatile unsigned char*>(guest+window+17)=0x91;
    Require(physical[window+17]==std::byte{0x91}&&committed==0&&renderer.IsGpuBufferModified(base,page),
        "Clean-page CPU store failed to retire its exact guard or drained unrelated GPU data");
    Require(!submitted,"Clean-page CPU store submitted and waited for unrelated GPU commands");
    const Rasterizer::ShaderBufferRequest read_envelope[]{{base,total,false,true,true}};
    (void)renderer.PrepareShaderBuffers(read_envelope);
    auto retained=renderer.CapturePreparedShaderBuffer(base,total);
    auto readback=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    retained.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,retained.handle.Get(),0,total);
    renderer.Finish();void* contents{};Check(readback.handle->Map(0,nullptr,&contents));
    std::vector<std::byte> clean_expected(total,std::byte{0x4d});
    memset(clean_expected.data(),0x55,page);clean_expected[window+17]=std::byte{0x91};
    const bool clean_contents=!memcmp(contents,clean_expected.data(),total);
    readback.handle->Unmap(0,nullptr);
    Require(submitted&&clean_contents,"Deferred clean-page CPU winner lost GPU data, ordering or padding");
    arm();Require(renderer.ReadMemory(requests[0].address,4),"Explicit CPU read failed");check_first_window();
    renderer.ResolveGpuBuffers(requests[2].address,4);
    Require(renderer.IsGpuBufferModified(requests[3].address,page),"Internal exact-page GPU resolution was widened");
    Require(renderer.ReadMemory(requests[2].address,4)&&!renderer.IsGpuBufferModified(requests[3].address,page),
        "Explicit CPU read of a clean island missed its modified arena-window neighbour");
    arm();const auto observed=*reinterpret_cast<volatile u32*>(guest+page);
    Require(observed==0x11111111u,"Real protected CPU read lost GPU contents");check_first_window();
    arm();*reinterpret_cast<volatile unsigned char*>(guest+page+17)=0x99;
    Require(committed==2*page&&physical[page+17]==std::byte{0x99}&&physical[window/2]==std::byte{0x22},
        "Pre-store fault did not download its window before preserving the actual CPU winner");
    Require(renderer.IsGpuBufferModified(requests[2].address,page)&&renderer.IsGpuBufferModified(requests[3].address,page),
        "CPU store retired GPU data outside its original window");
    arm();reject_second=true;bool rejected=false;
    try{renderer.ReadMemory(requests[0].address,4);}catch(const std::runtime_error&){rejected=true;}
    Require(rejected&&physical[window/2]==std::byte{0x4d}&&renderer.IsGpuBufferModified(requests[1].address,page),
        "Failed CPU-window commit falsely published a GPU island");
    reject_second=false;Require(renderer.ReadMemory(requests[0].address,4),"CPU-window retry failed");
    Require(physical[window/2]==std::byte{0x22},"CPU-window retry lost the uncommitted GPU island");
    const auto before=committed;
    Require(!renderer.ReadMemory(base+total,4)&&!renderer.ReadMemory(UINT64_MAX-1,4)&&committed==before,
        "Invalid CPU-window request read outside mapped guest memory");
    renderer.Finish();renderer.ValidateDebugMessages();
    puts("PASS:512KiB CPU download window, explicit/fault/read/write paths, sparse islands, exact GPU primitive, outside ownership and failed-commit retry");
}
void Run(ID3D12Device* device,bool f2){
    constexpr u64 bytes=65536;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);
    Require(section,"section");
    auto* guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    auto* physical=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    Require(guest&&physical,"aliases");const auto base=reinterpret_cast<VAddr>(guest);
    for(u64 i=0;i<bytes;++i)physical[i]=std::byte(i*19+7);
    ProtectedDmaBacking(device,guest,physical,base);
    RunBufferEnvelopeFixture<Fixture>(device,physical,base,bytes);
    RunSemanticBufferResidencyFixture<Fixture>(device);
    {
        u64 commits{},committed{};bool fail{};
        Fixture renderer(device);renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
        renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
            [&](VAddr a,std::span<const std::byte> data){if(fail){fail=false;return false;}
                ++commits;committed+=data.size();memcpy(physical+a-base,data.data(),data.size());return true;});
        Require(renderer.IsDmaReadable(base,4096),"guest-backed memory was rejected by DMA");
        const auto host_only=renderer.AcquireTemporaryBuffer(4096);
        Require(host_only&&!renderer.IsDmaReadable(reinterpret_cast<VAddr>(host_only->words.data()),4096),
            "renderer-only host temporary escaped into the guest DMA address table");
        const Rasterizer::ShaderBufferRequest writable[]{{base,4096,true,true}};
        if(f2){
            Require(renderer.TrackReadOnly(base+49152,12288),"image-only protected range setup");
            const std::pair<VAddr,u64> retained_image[]{std::pair{base+53248,u64(4096)}};
            renderer.ReleaseUnusedReadOnlyWatches(base+49152,12288,retained_image);
            for(u32 page=0;page<3;++page){MEMORY_BASIC_INFORMATION protection{};
                VirtualQuery(guest+49152+page*4096,&protection,sizeof(protection));
                Require(protection.Protect==(page==1?PAGE_READONLY:PAGE_READWRITE),
                    "image retirement released another image witness or retained an unused gap");
            }
            renderer.Untrack(base+53248,4096);
            const Rasterizer::ShaderBufferRequest formatted_arena[]{{base+32768,16384,false,true,true}};
            renderer.PrepareShaderBuffers(formatted_arena);
            renderer.FillPreparedShaderBuffer(base+32768,16384,0xa5a5a5a5,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            const Rasterizer::ShaderBufferRequest partial_writer[]{
                {base+32768,4,true,true},{base+32768,16384,false,true,true}};
            renderer.PrepareShaderBuffers(partial_writer);
            // A read fault reconciles the four logical GPU-written bytes only.
            // The remaining GPU-only metadata arena must survive that read.
            Require(*reinterpret_cast<volatile unsigned char*>(guest+32768)==0xa5,
                "formatted arena read did not reconcile its logical GPU span");
            const auto store_uploads=Counter("buffer-snapshot.cpu-store-upload.bytes");
            renderer.PrepareShaderBuffers(formatted_arena);
            Require(Counter("buffer-snapshot.cpu-store-upload.bytes")==store_uploads,
                "CPU read incorrectly created actual-store upload markers");
            const auto read_arena=[&](u32 cpu_pages){
                auto arena=renderer.CapturePreparedShaderBuffer(base+32768,16384);
                auto readback=Buffer(device,16384,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                arena.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,arena.handle.Get(),0,16384);
                renderer.Finish();void* contents{};D3D12_RANGE all{0,16384};
                Check(readback.handle->Map(0,&all,&contents));
                SCOPE_EXIT {D3D12_RANGE none{};readback.handle->Unmap(0,&none);};
                for(u64 byte=0;byte<16384;++byte){
                    const auto expected=cpu_pages&(1u<<u32(byte/4096))?physical[32768+byte]:std::byte{0xa5};
                    Require(static_cast<const std::byte*>(contents)[byte]==expected,
                        "actual-store upload changed an untouched GPU-only page or missed its CPU page");
                }
            };
            read_arena(0);
            const auto old_byte=std::to_integer<unsigned char>(physical[32768+4096+9]);
            *reinterpret_cast<volatile unsigned char*>(guest+32768+4096+9)=old_byte;
            renderer.PrepareShaderBuffers(formatted_arena);
            Require(Counter("buffer-snapshot.cpu-store-upload.bytes")-store_uploads==4096,
                "same-value CPU store did not upload precisely its CPU page");
            read_arena(1u<<1);
            renderer.NotifyCpuWrite(base+32768+4096+9,1);
            renderer.BeforeRead([&]{renderer.NotifyCpuWrite(base+32768+8192+11,1);});
            renderer.PrepareShaderBuffers(formatted_arena);
            read_arena((1u<<1)|(1u<<2));
            // A GPU image download changes physical CPU bytes without becoming
            // a CPU store into the independently resident raw buffer arena.
            std::vector<std::byte> image_bytes(4096,std::byte{0x6c});
            const auto image_read=[&](VAddr a,std::span<std::byte> data){
                memcpy(data.data(),physical+a-base,data.size());return true;};
            const auto image_write=[&](VAddr a,std::span<const std::byte> data){
                memcpy(physical+a-base,data.data(),data.size());return true;};
            bool image_failed{};
            try{renderer.CommitImageBacking(base+32768+12288,image_bytes,image_read,
                [](VAddr,std::span<const std::byte>){return false;});}
            catch(const std::runtime_error&){image_failed=true;}
            Require(image_failed,"failed image backing commit did not report failure");
            read_arena((1u<<1)|(1u<<2));
            renderer.CommitImageBacking(base+32768+12288,image_bytes,image_read,image_write);
            Require(memcmp(physical+32768+12288,image_bytes.data(),4096)==0,"image backing download was lost");
            Require(!renderer.IsGpuBufferModified(base+32768+12288,4096),
                "physical image download or read-only arena synthesis became a guest GPU buffer modification");
            const auto image_store_uploads=Counter("buffer-snapshot.cpu-store-upload.bytes");
            renderer.PrepareShaderBuffers(formatted_arena);
            Require(Counter("buffer-snapshot.cpu-store-upload.bytes")==image_store_uploads,
                "image download became a CPU buffer store");
            read_arena((1u<<1)|(1u<<2));
            *reinterpret_cast<volatile unsigned char*>(guest+32768+12288+9)=0x6c;
            renderer.PrepareShaderBuffers(formatted_arena);
            Require(Counter("buffer-snapshot.cpu-store-upload.bytes")-image_store_uploads==4096,
                "same-value CPU store after image download did not upload its page");
            read_arena((1u<<1)|(1u<<2)|(1u<<3));
            // Remove the intentionally divergent arena before the existing
            // ordinary CPU-copy/proof cases create their own resident version.
            Require(renderer.InvalidateMemory(base+32768,16384),"formatted arena cleanup failed");
            const Rasterizer::ShaderBufferRequest paged[]{{base+32768,16384,false,true,true}};
            renderer.PrepareShaderBuffers(paged);
            // Queue an actual GPU readback while the resident arena still
            // equals CPU backing. A same-value CPU store before its deferred
            // image commit must remain an upload marker after rebasing.
            auto image_upload=Buffer(device,4096,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            auto image_download=Buffer(device,4096,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            void* image_mapped{};D3D12_RANGE image_none{};
            Check(image_upload.handle->Map(0,&image_none,&image_mapped));memset(image_mapped,0x59,4096);
            image_upload.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(image_download.handle.Get(),0,image_upload.handle.Get(),0,4096);
            renderer.Finish();
            const auto deferred_stores=Counter("buffer-snapshot.cpu-store-upload.bytes");
            {
                std::lock_guard lock(renderer.ExecutionMutex());
                renderer.DeferImageBacking(renderer.CompletedFence(),base+32768,4096,65536,image_read,image_write,
                    [image_download](u64,std::span<std::byte> data){void* mapped{};D3D12_RANGE all{0,4096};
                        Check(image_download.handle->Map(0,&all,&mapped));memcpy(data.data(),mapped,data.size());
                        D3D12_RANGE none{};image_download.handle->Unmap(0,&none);});
                Require(renderer.PendingImageCommitBytes()==4096+65536,"priority backing budget was not retained");
                renderer.ReleaseUnusedReadOnlyWatches(base+32768,16384);
                MEMORY_BASIC_INFORMATION retained_proof{};
                VirtualQuery(guest+32768,&retained_proof,sizeof(retained_proof));
                Require(retained_proof.Protect==PAGE_READONLY,"image retirement removed the live CPU buffer witness");
                const auto value=std::to_integer<unsigned char>(physical[32768+9]);
                *reinterpret_cast<volatile unsigned char*>(guest+32768+9)=value;
            }
            renderer.DrainPriorityOperations();
            Require(!renderer.PendingImageCommitBytes(),"priority image backing budget did not retire");
            // A submitted copy may still be blocked on an external GPU fence.
            // Physical backing replacement must cancel its eventual CPU write,
            // even when the same guest address is immediately mapped again.
            for(u32 replacement=0;replacement<4;++replacement){
                std::vector<std::byte> backing(4096,std::byte{0x93});
                const auto target=reinterpret_cast<VAddr>(backing.data());
                Rasterizer queued(device);queued.MapMemory(target,backing.size());
                auto readback=Buffer(device,4096,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
                SCOPE_EXIT {gate->Signal(1);};
                u32 reads{},writes{},resolves{};
                {
                    std::lock_guard lock(queued.ExecutionMutex());
                    queued.WaitExternalFence(gate.Get(),1);
                    queued.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,image_upload.handle.Get(),0,4096);
                    const auto fence=queued.Flush();
                    Require(queued.CompletedFence()<fence,"external GPU gate did not retain the submitted copy");
                    queued.DeferImageBacking(fence,target,4096,65536,
                        [&](VAddr,std::span<std::byte> data){++reads;memcpy(data.data(),backing.data(),data.size());return true;},
                        [&](VAddr,std::span<const std::byte> data){++writes;memcpy(backing.data(),data.data(),data.size());return true;},
                        [&,readback](u64,std::span<std::byte> data){++resolves;void* mapped{};D3D12_RANGE all{0,4096};
                            Check(readback.handle->Map(0,&all,&mapped));memcpy(data.data(),mapped,data.size());
                            D3D12_RANGE none{};readback.handle->Unmap(0,&none);});
                    Require(queued.PendingImageCommitBytes()==4096+65536,"in-flight GPU copy lost its priority budget");
                    if(replacement==0)queued.RegisterMemory(target,4096);
                    else if(replacement==1)queued.MapMemory(target,4096);
                    else if(replacement==2){queued.UnmapMemory(target,4096);queued.MapMemory(target,4096);}
                    else queued.NotifyHostMapping(target,4096,true,true);
                    Check(gate->Signal(1));
                }
                queued.DrainPriorityOperations();
                Require(!reads&&!writes&&!resolves&&!queued.PendingImageCommitBytes(),
                    "retired GPU image wrote into replacement physical backing");
                for(const auto byte:backing)Require(byte==std::byte{0x93},"replacement physical bytes were changed");
            }
            for(u32 byte=0;byte<4096;++byte)Require(physical[32768+byte]==std::byte{0x59},"priority GPU readback commit contents");
            renderer.PrepareShaderBuffers(paged);
            Require(Counter("buffer-snapshot.cpu-store-upload.bytes")-deferred_stores==4096,
                "pending image commit erased a preceding same-value CPU store");
            const Rasterizer::BufferCopy deferred_copy{base+24576+317,base+32768+9,1};
            renderer.CopyBufferBatch(std::span(&deferred_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+317)==0x59,
                "rebased CPU store did not reach the independent raw arena");
            const auto cp_cached=renderer.CachedBufferBytes(),cp_fence=renderer.SubmittedFence();
            renderer.CopyBuffer(base+32768+4096+5,base+64,35,false,false);
            Require(memcmp(physical+32768+4096+5,physical+64,35)==0,"cold CP copy contents");
            Require(renderer.CachedBufferBytes()==cp_cached&&renderer.SubmittedFence()==cp_fence,
                "cold CP copy allocated or submitted a GPU buffer");
            MEMORY_BASIC_INFORMATION cp_neighbour{};VirtualQuery(guest+32768,&cp_neighbour,sizeof(cp_neighbour));
            Require(cp_neighbour.Protect==PAGE_READONLY,"cold CP copy released a clean neighbouring page");
            const auto cp_compared=Counter("buffer-snapshot.bytes");
            renderer.PrepareShaderBuffers(paged);
            Require(Counter("buffer-snapshot.bytes")-cp_compared==4096,
                "cold CP copy lost the remaining page equality proof");
            const Rasterizer::BufferCopy cp_readback{base+24576+128,base+32768+4096+5,35};
            renderer.CopyBufferBatch(std::span(&cp_readback,1));
            Require(memcmp(guest+24576+128,physical+64,35)==0,"cold CP copy stayed stale in the GPU cache");
            *reinterpret_cast<volatile unsigned char*>(guest+32768+4096+9)=0x67;
            MEMORY_BASIC_INFORMATION neighbour{};VirtualQuery(guest+32768,&neighbour,sizeof(neighbour));
            Require(neighbour.Protect==PAGE_READONLY,"one CPU store released the neighbouring clean page");
            const auto compared_before=Counter("buffer-snapshot.bytes");
            renderer.PrepareShaderBuffers(paged);
            Require(Counter("buffer-snapshot.bytes")-compared_before==4096,"dirty proof rescanned clean pages");
            const Rasterizer::BufferCopy page_copy{base+24576+300,base+32768+4096+9,1};
            renderer.CopyBufferBatch(std::span(&page_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+300)==0x67,"dirty page upload missed a CPU store");
            renderer.NotifyCpuWrite(base+32768+8192+9,1);physical[32768+8192+9]=std::byte{0x92};
            const auto alias_before=Counter("buffer-snapshot.bytes");renderer.PrepareShaderBuffers(paged);
            Require(Counter("buffer-snapshot.bytes")-alias_before==4096,"physical alias notification discarded clean proof");
            const Rasterizer::BufferCopy alias_copy{base+24576+301,base+32768+8192+9,1};
            renderer.CopyBufferBatch(std::span(&alias_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+301)==0x92,"physical alias data stayed stale");
            *reinterpret_cast<volatile unsigned char*>(guest+32768+4096+10)=0x41;
            renderer.BeforeRead([&]{renderer.ReleaseCpuBufferWatches(base+32768,16384);physical[32768+11]=std::byte{0xab};});
            renderer.PrepareShaderBuffers(paged);
            const Rasterizer::BufferCopy reentrant_copy{base+24576+302,base+32768+11,1};
            renderer.CopyBufferBatch(std::span(&reentrant_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+302)==0xab,"reentrant partial-proof invalidation hid another page");
            *reinterpret_cast<volatile unsigned char*>(guest+32768+4096+12)=0x74;
            const Rasterizer::ShaderBufferRequest paged_writable[]{{base+32768,16384,true,true,true}};
            const auto transfer_before=Counter("buffer-snapshot.bytes");renderer.PrepareShaderBuffers(paged_writable);
            Require(Counter("buffer-snapshot.bytes")-transfer_before==4096,"dirty-page ownership upgrade rescanned clean pages");
            VirtualQuery(guest+32768+8192,&neighbour,sizeof(neighbour));
            Require(neighbour.Protect==PAGE_NOACCESS,"fragmented proof did not transfer GPU ownership atomically");
            renderer.ResolveGpuBuffers(base+32768,16384);
            const Rasterizer::ShaderBufferRequest read_only[]{{base+8192,4096,false,true}};
            const auto establish=Counter("buffer-watch.establish-compare.bytes");
            renderer.PrepareShaderBuffers(read_only);
            Require(Counter("buffer-watch.establish-compare.bytes")==establish,
                "read-only proof repeated the comparison after protecting the snapshot");
            *reinterpret_cast<volatile unsigned char*>(guest+8192+19)=0xb7;
            renderer.PrepareShaderBuffers(read_only);
            Require(Counter("buffer-watch.establish-compare.bytes")==establish,
                "changed read-only binding repeated its protected comparison");
            const Rasterizer::BufferCopy protected_copy{base+24576+19,base+8192+19,1};
            renderer.CopyBufferBatch(std::span(&protected_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+19)==0xb7,
                "protected comparison hid a real CPU store from the GPU");
            // Physical-alias writers must invalidate even a pending watch;
            // this callback models reconciliation while Upload reads backing.
            *reinterpret_cast<volatile unsigned char*>(guest+8192+20)=0xa4;
            renderer.BeforeRead([&]{
                renderer.ReleaseCpuBufferWatches(base+8192,4096);
                physical[8192+19]=std::byte{0xc1};
            });
            renderer.PrepareShaderBuffers(read_only);
            renderer.CopyBufferBatch(std::span(&protected_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+19)==0xc1,
                "reentrant alias reconciliation retained a pending proof");
            *reinterpret_cast<volatile unsigned char*>(guest+8192+20)=0xd2;
            renderer.BeforeRead([]{throw std::runtime_error("injected upload read failure");});
            bool failed{};try{renderer.PrepareShaderBuffers(read_only);}catch(const std::runtime_error&){failed=true;}
            MEMORY_BASIC_INFORMATION failed_region{};VirtualQuery(guest+8192,&failed_region,sizeof(failed_region));
            Require(failed&&failed_region.Protect==PAGE_READWRITE,"failed upload retained its pending watch");
            *reinterpret_cast<volatile unsigned char*>(guest+8192+19)=0xe3;
            renderer.PrepareShaderBuffers(read_only);renderer.CopyBufferBatch(std::span(&protected_copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+24576+19)==0xe3,
                "retry after failed proof reused stale contents");
            renderer.PrepareShaderBuffers(writable);renderer.ResolveGpuBuffers(base,4096);
            const auto before=Counter("buffer-snapshot.bytes");
            renderer.PrepareShaderBuffers(writable);
            const auto compared=Counter("buffer-snapshot.bytes")-before;
            printf("unchanged watched writable snapshot_compared_bytes=%llu\n",compared);
            Require(compared==0,"unchanged watched writable buffer performed snapshot comparison");
            renderer.ResolveGpuBuffers(base,4096);
            *reinterpret_cast<volatile unsigned char*>(guest+17)=0xa9; // real write fault
            const Rasterizer::BufferCopy copy{base+16384+17,base+17,1};
            renderer.CopyBufferBatch(std::span(&copy,1));
            Require(*reinterpret_cast<volatile unsigned char*>(guest+16384+17)==0xa9,"CPU store did not reach GPU/readback");
            // A CPU store already approaching a watched page cannot slip
            // through while GPU preparation transfers READONLY to NOACCESS.
            for(unsigned iteration=0;iteration<16;++iteration){
                renderer.PrepareShaderBuffers(writable);renderer.ResolveGpuBuffers(base,4096);
                std::promise<void> started;auto ready=started.get_future();std::atomic<bool> stored{};std::jthread writer;
                bool blocked{},owned{};
                {
                    std::unique_lock lock(renderer.ExecutionMutex());
                    writer=std::jthread([&]{started.set_value();*reinterpret_cast<volatile unsigned char*>(guest+18)=0x5a;stored=true;});
                    ready.wait();renderer.PrepareShaderBuffers(writable);
                    MEMORY_BASIC_INFORMATION region{};VirtualQuery(guest,&region,sizeof(region));owned=region.Protect==PAGE_NOACCESS;
                    blocked=!stored.load();renderer.Flush();
                }
                writer.join();Require(blocked&&owned&&stored,"CPU store crossed ownership transfer");
                const Rasterizer::BufferCopy observed{base+16384+18,base+18,1};
                renderer.CopyBufferBatch(std::span(&observed,1));
                Require(*reinterpret_cast<volatile unsigned char*>(guest+16384+18)==0x5a,"queued store lost after rearm");
            }
        }else{
            const Rasterizer::BufferCopy first{base+16384+64,base+32,32};
            renderer.CopyBufferBatch(std::span(&first,1));Require(commits==0,"first GPU copy committed eagerly");
            const Rasterizer::ShaderBufferRequest index_request[]{{base+16384,4096,false,false,true}};
            auto index_views=renderer.PrepareShaderBuffers(index_request);
            auto indices=renderer.PreparedIndexBuffer(base+16384+64,32,DXGI_FORMAT_R16_UINT);
            Require(commits==0&&indices.resource->handle.Get()==index_views[0].resource.Get(),"GPU-written indices left residency");
            auto index_readback=Buffer(device,32,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            indices.resource->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(index_readback.handle.Get(),0,indices.resource->handle.Get(),indices.offset,32);
            indices.resource->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            renderer.Finish();Require(commits==0,"resident index use reconciled GPU source to CPU");
            void* mapped{};D3D12_RANGE used{0,32};Check(index_readback.handle->Map(0,&used,&mapped));
            Require(!memcmp(mapped,physical+32,32),"resident index view did not see ordered GPU writes");
            D3D12_RANGE unused{};index_readback.handle->Unmap(0,&unused);
            const Rasterizer::ShaderBufferRequest uav_indices[]{{base+16384,4096,false,true,true}};
            renderer.PrepareShaderBuffers(uav_indices);
            auto held_index=renderer.PreparedIndexBuffer(base+16384+64,32,DXGI_FORMAT_R16_UINT);
            auto* held_identity=held_index.resource->handle.Get();renderer.Finish();
            auto second_index=renderer.PreparedIndexBuffer(base+16384+64,32,DXGI_FORMAT_R16_UINT);
            Require(second_index.resource->handle.Get()!=held_identity,"index-copy pool reused a live IA lease");
            held_index={};renderer.Finish();
            auto reused_index=renderer.PreparedIndexBuffer(base+16384+64,32,DXGI_FORMAT_R16_UINT);
            Require(reused_index.resource->handle.Get()==held_identity&&commits==0,"retired index-copy pool did not reuse GPU-only storage");
            const auto cpu_checks=Counter("buffer-access.cpu-reconcile");
            renderer.ValidateGpuBacking(base+16384+64,32);
            Require(Counter("buffer-access.cpu-reconcile")==cpu_checks,"legality check entered CPU path");
            // A separate unaligned writable binding remains CPU-visible.
            renderer.PrepareShaderBuffers(std::array<Rasterizer::ShaderBufferRequest,1>{{{base+49152+4,16,true,true}}});
            Require(commits==0,"unrelated CPU-visible buffer reconciled owned A");
            const Rasterizer::BufferCopy second{base+32768+64,base+16384+64,32};
            renderer.CopyBufferBatch(std::span(&second,1));
            printf("resident GPU A->B commits_before_cpu=%llu\n",commits);
            Require(commits==0,"GPU validation reconciled resident source to CPU");
            Require(*reinterpret_cast<volatile unsigned char*>(guest+32768+64)==std::to_integer<unsigned char>(physical[32]),"GPU chain data mismatch");
            renderer.ResolveGpuBuffers(base,bytes);
            Require(memcmp(physical+32768+64,physical+32,32)==0,"GPU chain content mismatch");
            std::vector<std::byte> expected(physical,physical+bytes);
            const Rasterizer::BufferCopy prior{base+16384+4032,base+4000,96};
            renderer.CopyBufferBatch(std::span(&prior,1));memmove(expected.data()+prior.destination-base,expected.data()+prior.source-base,prior.bytes);
            const Rasterizer::BufferCopy chain[]{{base+16384+4050,base+16384+4032,64},
                {base+32768+4080,base+16384+4032,96},{base+32768+4096,base+32768+4080,64}};
            for(const auto& c:chain)memmove(expected.data()+c.destination-base,expected.data()+c.source-base,c.bytes);
            renderer.CopyBufferBatch(chain);renderer.ResolveGpuBuffers(base,bytes);
            Require(memcmp(physical,expected.data(),bytes)==0,"partial/scratch/chained copy lost prior dirty bytes");
            const Rasterizer::BufferCopy retry{base+32768+128,base+192,32};renderer.CopyBufferBatch(std::span(&retry,1));
            fail=true;const auto before=commits;bool rejected{};
            try{renderer.ResolveGpuBuffers(base+32768,4096);}catch(const std::runtime_error&){rejected=true;}
            MEMORY_BASIC_INFORMATION region{};VirtualQuery(guest+32768,&region,sizeof(region));
            Require(rejected&&commits==before&&region.Protect==PAGE_NOACCESS,"failed commit released protection");
            renderer.ResolveGpuBuffers(base+32768,4096);Require(memcmp(physical+32768+128,physical+192,32)==0,"commit retry data");
            const auto completed=commits;renderer.ResolveGpuBuffers(base,bytes);Require(commits==completed,"commit repeated");
            renderer.ReleaseCpuBufferWatches(base,bytes);
            DWORD old{};Require(VirtualProtect(guest+49152,4096,PAGE_READONLY,&old),"protect guest read-only");
            renderer.NotifyHostMapping(base+49152,4096,true,false);
            const auto saved=std::vector<std::byte>(physical,physical+bytes);
            const Rasterizer::BufferCopy invalid[]{{base+16384+64,base+100,32},{base+49152+16,base,16}};
            rejected=false;try{renderer.CopyBufferBatch(invalid);}catch(const std::exception&){rejected=true;}
            Require(rejected&&memcmp(physical,saved.data(),bytes)==0,"invalid batch changed data or ignored rights");
            Require(VirtualProtect(guest+49152,4096,PAGE_READWRITE,&old),"restore rights");renderer.NotifyHostMapping(base+49152,4096,true,true);
            renderer.UnmapMemory(base+32768,8192);renderer.MapMemory(base+32768,8192);renderer.NotifyHostMapping(base+32768,8192,true,true);
            memset(physical+32768,0x3c,8192);renderer.PrepareShaderBuffers(std::array<Rasterizer::ShaderBufferRequest,1>{{{base+32768,4096,false,true}}});
            const Rasterizer::BufferCopy remap{base+16384+256,base+32768,32};renderer.CopyBufferBatch(std::span(&remap,1));renderer.ResolveGpuBuffers(base,bytes);
            Require(physical[16384+256]==std::byte{0x3c},"remap used stale resident bytes");
            auto* hole=VirtualAlloc(nullptr,12288,MEM_RESERVE,PAGE_NOACCESS);Require(hole,"PRT reserve");
            const auto hole_base=reinterpret_cast<VAddr>(hole);Require(VirtualAlloc(hole,4096,MEM_COMMIT,PAGE_READWRITE),"PRT first page");
            renderer.MapMemory(hole_base,12288);
            const Rasterizer::BufferCopy hole_copy[]{{base+16384,base,16},{hole_base+4096,base,16}};
            rejected=false;try{renderer.CopyBufferBatch(hole_copy);}catch(const std::exception&){rejected=true;}
            Require(rejected,"PRT reserved hole accepted");renderer.UnmapMemory(hole_base,12288);VirtualFree(hole,0,MEM_RELEASE);
        }
        renderer.ValidateDebugMessages();
    }
    UnmapViewOfFile(physical);UnmapViewOfFile(guest);CloseHandle(section);
}
static void ReentrantSubmission(ID3D12Device* device){
    Rasterizer renderer(device);
    auto upload=Buffer(device,4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    auto readback=Buffer(device,4,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    void* data{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&data));
    const u32 expected=0x7193a5c7;memcpy(data,&expected,4);upload.handle->Unmap(0,nullptr);
    renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,upload.handle.Get(),0,4);
    unsigned publications{},retirements{};
    renderer.RetainUntilFinish([&](u64){++retirements;});
    renderer.NotifyOnSubmission([&](u64){
        // A nested publication/retirement can submit internal transfer work.
        // It must not publish or retain the outer batch a second time.
        if(++publications==1)renderer.Flush();
    });
    renderer.Flush();renderer.Finish();
    Require(publications==1&&retirements==1,"Reentrant Flush duplicated submission or retirement callbacks");
    D3D12_RANGE range{0,4};Check(readback.handle->Map(0,&range,&data));u32 actual{};
    memcpy(&actual,data,4);readback.handle->Unmap(0,&empty);
    Require(actual==expected,"Reentrant submission lost GPU copy contents");
    renderer.ValidateDebugMessages();
}
int main(int argc,char** argv){try{
    _putenv_s("SHADPS4_NATIVE_DIAGNOSTICS","1");
    auto& hub=Diagnostics::Hub::Get();hub.worker.request_stop();hub.wake.notify_all();hub.worker.join();
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    ReentrantSubmission(device.Get());
    if(argc>1&&(std::string_view(argv[1])=="physical-proof"||std::string_view(argv[1])=="unaligned-proof"||
        std::string_view(argv[1])=="cpu-window"||std::string_view(argv[1])=="frame-readback"||std::string_view(argv[1])=="indirect-arguments")){
        const bool indirect=std::string_view(argv[1])=="indirect-arguments";
        const bool cpu_window=std::string_view(argv[1])=="cpu-window"||indirect;const u64 allocation_bytes=cpu_window?1024*1024:65536;
        HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(allocation_bytes),nullptr);
        Require(section,"Physical proof section");SCOPE_EXIT {CloseHandle(section);};
        auto* guest=static_cast<std::byte*>(MapViewOfFileEx(section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(allocation_bytes),
            cpu_window?reinterpret_cast<void*>(0x1000000000ULL):nullptr));
        auto* physical=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,SIZE_T(allocation_bytes)));
        Require(guest&&physical,"Physical proof aliases");SCOPE_EXIT {UnmapViewOfFile(guest);UnmapViewOfFile(physical);};
        memset(physical,0x4d,SIZE_T(allocation_bytes));
        if(indirect)IndirectArguments(device.Get(),guest,physical,reinterpret_cast<VAddr>(guest));
        else if(std::string_view(argv[1])=="frame-readback")FrameBufferReadback(device.Get(),guest,physical,reinterpret_cast<VAddr>(guest));
        else if(cpu_window)CpuDownloadWindow(device.Get(),guest,physical,reinterpret_cast<VAddr>(guest));
        else if(std::string_view(argv[1])=="unaligned-proof")UnalignedPageWitness(device.Get(),guest,physical,reinterpret_cast<VAddr>(guest));
        else ProtectedDmaBacking(device.Get(),guest,physical,reinterpret_cast<VAddr>(guest));return 0;
    }
    Run(device.Get(),argc>1&&std::string_view(argv[1])=="f2");puts("PASS: production guest buffer access");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
