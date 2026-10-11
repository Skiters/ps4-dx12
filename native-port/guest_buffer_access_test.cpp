// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_buffer_access.h"
#include <cstdio>
#include <future>
using namespace NativeDX12;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Fixture final : public GuestBufferAccess {
    std::function<void()> before_read_;
public:
    using GuestBufferAccess::GuestBufferAccess;
    using GuestBufferAccess::SetBufferBackingAccess;
    void BeforeRead(std::function<void()> callback){before_read_=std::move(callback);}
    bool ReadMemory(VAddr a,u64 s,bool assume=false) override {
        if(before_read_){auto callback=std::move(before_read_);before_read_={};callback();}
        return Rasterizer::ReadMemory(a,s,assume);
    }
};
static u64 Counter(const char* name){auto& hub=Diagnostics::Hub::Get();std::lock_guard lock(hub.mutex);
    for(const auto& c:hub.counters)if(c.name&&std::string_view(c.name)==name)return c.value;return 0;}
void Run(ID3D12Device* device,bool f2){
    constexpr u64 bytes=65536;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);
    Require(section,"section");
    auto* guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    auto* physical=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    Require(guest&&physical,"aliases");const auto base=reinterpret_cast<VAddr>(guest);
    for(u64 i=0;i<bytes;++i)physical[i]=std::byte(i*19+7);
    {
        u64 commits{},committed{};bool fail{};
        Fixture renderer(device);renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
        renderer.SetBufferBackingAccess([&](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
            [&](VAddr a,std::span<const std::byte> data){if(fail){fail=false;return false;}
                ++commits;committed+=data.size();memcpy(physical+a-base,data.data(),data.size());return true;});
        const Rasterizer::ShaderBufferRequest writable[]{{base,4096,true,true}};
        if(f2){
            const Rasterizer::ShaderBufferRequest paged[]{{base+32768,16384,false,true,true}};
            renderer.PrepareShaderBuffers(paged);
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
int main(int argc,char** argv){try{
    _putenv_s("SHADPS4_NATIVE_DIAGNOSTICS","1");
    auto& hub=Diagnostics::Hub::Get();hub.worker.request_stop();hub.wake.notify_all();hub.worker.join();
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    Run(device.Get(),argc>1&&std::string_view(argv[1])=="f2");puts("PASS: production guest buffer access");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
