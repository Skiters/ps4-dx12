// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <cstdio>
using namespace NativeDX12;
void Run(ID3D12Device* device,bool overlap,bool retry,bool prior=false){
    auto* memory=static_cast<std::byte*>(VirtualAlloc(nullptr,32768,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("VirtualAlloc");
    const auto base=reinterpret_cast<VAddr>(memory);
    for(size_t i=0;i<32768;++i)memory[i]=std::byte(i*17+3);
    std::vector<std::byte> expected(memory,memory+32768);
    {
        Rasterizer renderer(device);renderer.MapMemory(base,32768);
        u64 committed{};bool fail=retry;
        renderer.SetBufferOwnership([](VAddr,u64){return true;},
            [&](VAddr dst,std::span<const std::byte> data){
                if(fail){fail=false;return false;}
                committed+=data.size();memcpy(reinterpret_cast<void*>(dst),data.data(),data.size());return true;
            },[](VAddr,u64){});
        if(prior)renderer.PrepareShaderBuffers(std::array<Rasterizer::ShaderBufferRequest,1>{{{base+16384,12288,true,true}}});
        const Rasterizer::BufferCopy copies[]{{base+16384+64,base+32,32},
            {base+16384+(overlap?80:8256),base+96,32}};
        for(const auto& c:copies)memmove(expected.data()+c.destination-base,expected.data()+c.source-base,c.bytes);
        renderer.CopyBufferBatch(copies);
        if(retry){bool rejected{};try{renderer.ResolveGpuBuffers(base+16384,12288);}catch(const std::runtime_error&){rejected=true;}
            if(!rejected||committed)throw std::runtime_error("commit failure lost ownership");}
        renderer.ResolveGpuBuffers(base+16384,12288);
        const u64 wanted=prior?12288:overlap?48:64;
        printf("%s retry=%u committed=%llu expected=%llu\n",overlap?"overlap":"disjoint",retry,committed,wanted);
        if(committed!=wanted)throw std::runtime_error("copy envelope inflated committed bytes");
        if(memcmp(memory,expected.data(),expected.size()))throw std::runtime_error("copy data/source/gap mismatch");
        renderer.ResolveGpuBuffers(base,32768);
        if(committed!=wanted)throw std::runtime_error("repeated commit");
        renderer.ValidateDebugMessages();
    }
    VirtualFree(memory,0,MEM_RELEASE);
}
void RunLargePacket(ID3D12Device* device) {
    constexpr u32 Bytes=68*1024*1024+1;
    constexpr u64 Destination=72*1024*1024+3,Neighbour=144*1024*1024,Total=Neighbour+65536;
    auto* memory=static_cast<std::byte*>(VirtualAlloc(nullptr,SIZE_T(Total),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("large copy VirtualAlloc");
    const auto base=reinterpret_cast<VAddr>(memory);
    try {
        memset(memory,0xee,SIZE_T(Total));
        for(u64 i=0;i<Bytes;++i)memory[1+i]=std::byte((i*17+(i>>12)*31+3)&255);
        Rasterizer renderer(device);renderer.MapMemory(base,Total);
        renderer.SetBufferOwnership([](VAddr,u64){return true;},[](VAddr dst,std::span<const std::byte> data){
            memcpy(reinterpret_cast<void*>(dst),data.data(),data.size());return true;
        },[](VAddr,u64){});
        const auto neighbour=renderer.PrepareShaderBuffers(
            std::array<Rasterizer::ShaderBufferRequest,1>{{{base+Neighbour,65536,false,true}}})[0];
        const auto before=renderer.SubmittedFence();
        const auto cold_cached=renderer.CachedBufferBytes();
        // Zero-byte DMA and ordinary guarded DMA retain the CP's pending list.
        renderer.CopyBuffer(0,0,0,false,false);
        renderer.CopyBuffer(base+Destination,base+1,Bytes,false,false);
        if(renderer.CachedBufferBytes()!=cold_cached||renderer.SubmittedFence()!=before)
            throw std::runtime_error("cold large DMA allocated or submitted GPU storage");
        if(memcmp(memory+Destination,memory+1,Bytes))throw std::runtime_error("cold large byte copy mismatch");
        // The raw shader view must cover source offset 1 with a dword-aligned
        // extent. The DMA packet itself retains its odd offset and byte count.
        constexpr u64 SourceEnvelope=(u64(Bytes)+1+3)&~u64(3);
        renderer.PrepareShaderBuffers(
            std::array<Rasterizer::ShaderBufferRequest,1>{{{base,SourceEnvelope,true,true}}});
        const auto prepared=renderer.SubmittedFence();
        memset(memory+Destination,0xee,Bytes);
        renderer.CopyBuffer(base+Destination,base+1,Bytes,false,false);
        if(renderer.SubmittedFence()!=before)throw std::runtime_error("DMA submitted the pending guest list: before="+
            std::to_string(before)+" prepared="+std::to_string(prepared)+" copied="+std::to_string(renderer.SubmittedFence()));
        const auto retained=renderer.PrepareShaderBuffers(
            std::array<Rasterizer::ShaderBufferRequest,1>{{{base+Neighbour,65536,false,true}}})[0];
        if(retained.resource.Get()!=neighbour.resource.Get())
            throw std::runtime_error("DMA discarded an unrelated resident backing");
        renderer.ResolveGpuBuffers(base+Destination,Bytes);
        if(memcmp(memory+Destination,memory+1,Bytes))throw std::runtime_error("large byte copy mismatch");
        for(u64 i=Bytes+1;i<Destination;++i)if(memory[i]!=std::byte{0xee})
            throw std::runtime_error("large DMA modified the gap");
        for(u64 i=Destination+Bytes;i<Total;++i)if(memory[i]!=std::byte{0xee})
            throw std::runtime_error("large DMA modified trailing padding or neighbour");
        // GDS remains on the same real copy path, including odd byte counts.
        const auto gds_before=renderer.SubmittedFence();
        renderer.CopyBuffer(5,base+1,13,true,false);
        renderer.CopyBuffer(base+Neighbour+1,5,13,false,true);
        if(renderer.SubmittedFence()!=gds_before)throw std::runtime_error("GDS DMA drained the pending list");
        u32 expected_gds{};memcpy(&expected_gds,memory+1,4);
        if(renderer.ReadDataFromGds(5)!=expected_gds)throw std::runtime_error("GDS read did not resolve the real GPU write");
        renderer.ResolveGpuBuffers(base+Neighbour,65536);renderer.Finish();
        if(memcmp(memory+Neighbour+1,memory+1,13))throw std::runtime_error("byte GDS round trip mismatch");
        renderer.ValidateDebugMessages();
    } catch(...) {VirtualFree(memory,0,MEM_RELEASE);throw;}
    VirtualFree(memory,0,MEM_RELEASE);
}
int main(){try{
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    Run(device.Get(),false,false);Run(device.Get(),true,false);Run(device.Get(),false,true);
    Run(device.Get(),false,false,true);
    RunLargePacket(device.Get());
    puts("PASS: exact GPU copy dirty union, preserved gaps/source and commit retry");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
