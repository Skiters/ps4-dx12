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
int main(){try{
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    Run(device.Get(),false,false);Run(device.Get(),true,false);Run(device.Get(),false,true);
    Run(device.Get(),false,false,true);
    puts("PASS: exact GPU copy dirty union, preserved gaps/source and commit retry");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
