// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_buffer_access.h"
#include <cstdio>
#include <future>
using namespace NativeDX12;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Fixture final : public GuestBufferAccess {
public:
    using GuestBufferAccess::GuestBufferAccess;
    using GuestBufferAccess::SetBufferBackingAccess;
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
