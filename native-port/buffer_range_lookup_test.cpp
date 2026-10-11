// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <chrono>
#include <cstdio>
using namespace NativeDX12;
// Mirror GuestRasterizer's backing validation: CPU access first resolves any
// GPU-owned overlap. Bound recursion so the old bug fails instead of crashing.
class CoherentRasterizer final : public Rasterizer {
    unsigned finish_depth{};
public:
    using Rasterizer::Rasterizer;
    unsigned max_finish_depth{};
    void Finish() override {
        ++finish_depth;
        max_finish_depth=std::max(max_finish_depth,finish_depth);
        try {
            if(finish_depth>4)throw std::runtime_error("recursive buffer reconciliation");
            Rasterizer::Finish();
        } catch(...) {--finish_depth;throw;}
        --finish_depth;
    }
protected:
    void CheckBacking(VAddr address,u64 size,bool writing) override {
        ResolveGpuBuffers(address,size);
        Rasterizer::CheckBacking(address,size,writing);
    }
};
void CheckMixedOwnership(ID3D12Device* device,bool fail_commit=false) {
    std::array<u32,192> words;words.fill(0xdeadbeef);
    for(unsigned i=0;i<64;++i)words[i]=0x12340000+i;
    const auto address=reinterpret_cast<VAddr>(words.data());
    CoherentRasterizer rasterizer(device);rasterizer.MapMemory(address,sizeof(words));
    unsigned commits{},releases{};
    rasterizer.SetBufferOwnership([&](VAddr base,u64){return base==address+256;},
        [&](VAddr dst,std::span<const std::byte> bytes){
            if(fail_commit){fail_commit=false;throw std::runtime_error("injected commit failure");}
            ++commits;memcpy(reinterpret_cast<void*>(dst),bytes.data(),bytes.size());return true;},
        [&](VAddr,u64){++releases;});
    const Rasterizer::ShaderBufferRequest requests[]{
        {address,256,false,true},{address+256,256,true,true},{address+512,256,true,true}};
    auto views=rasterizer.PrepareShaderBuffers(requests);
    auto* list=rasterizer.RecordingList();
    // Real GPU copies: both ownership paths must retire exact changed bytes.
    for(unsigned i=1;i<3;++i){
        D3D12_RESOURCE_BARRIER barriers[2]{};
        for(unsigned j=0;j<2;++j){auto& b=barriers[j];b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource=views[j?i:0].resource.Get();
            b.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            b.Transition.StateAfter=j?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE;
            b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;}
        list->ResourceBarrier(2,barriers);
        list->CopyBufferRegion(views[i].resource.Get(),views[i].offset+16,views[0].resource.Get(),views[0].offset+16,224);
        for(auto& b:barriers)std::swap(b.Transition.StateBefore,b.Transition.StateAfter);
        list->ResourceBarrier(2,barriers);
    }
    if(fail_commit){
        bool failed{};
        try{rasterizer.ResolveGpuBuffers(address+256,256);}
        catch(const std::runtime_error& e){
            if(std::string_view(e.what())!="injected commit failure")throw;
            failed=true;
        }
        if(!failed||commits||releases)throw std::runtime_error("failed writeback released GPU ownership");
    }
    rasterizer.ResolveGpuBuffers(address+256,256);
    if(rasterizer.max_finish_depth!=1||commits!=1||releases!=1)
        throw std::runtime_error("mixed ownership did not retire once");
    for(unsigned i=64;i<192;++i){const auto pos=i%64;
        const auto expected=pos>=4&&pos<60?words[pos]:0xdeadbeef;
        if(words[i]!=expected)throw std::runtime_error("mixed ownership writeback lost data or padding");}
    rasterizer.ResolveGpuBuffers(address+256,256);
    if(commits!=1||releases!=1)throw std::runtime_error("completed ownership retired twice");
    rasterizer.ValidateDebugMessages();
}
int main() { try {
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    CheckMixedOwnership(device.Get());
    CheckMixedOwnership(device.Get(),true);
    std::vector<u32> memory(1024*1024,0x12345678);
    const auto address=reinterpret_cast<VAddr>(memory.data());
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,memory.size()*4);
    std::vector<Rasterizer::ShaderBufferRequest> requests;
    for(u64 i=0;i<1024;++i)requests.push_back({address+i*4096,16,false,false});
    auto views=rasterizer.PrepareShaderBuffers(requests);rasterizer.Finish();
    const auto begin=std::chrono::steady_clock::now();
    for(unsigned i=0;i<10000;++i)rasterizer.ResolveGpuBuffers(address+512*4096+4,4);
    const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    printf("range_lookup mappings=1024 calls=10000 cpu_wall_ms=%.3f\n",elapsed);
    std::vector<VAddr> released;
    rasterizer.SetBufferOwnership([](VAddr,u64){return true;},
        [](VAddr dst,std::span<const std::byte> bytes){memcpy(reinterpret_cast<void*>(dst),bytes.data(),bytes.size());return true;},
        [&](VAddr base,u64){released.push_back(base);});
    const Rasterizer::ShaderBufferRequest writable[]{{address+4096,16,true,true},{address+8192,16,true,true}};
    rasterizer.PrepareShaderBuffers(writable);
    rasterizer.ResolveGpuBuffers(address+4096+16,4096-16); // exclusive boundaries
    if(!released.empty())throw std::runtime_error("adjacent range reconciled non-overlapping buffers");
    rasterizer.ResolveGpuBuffers(address+4096+4,4); // starts inside preceding mapping
    if(released.size()!=1||released[0]!=address+4096)throw std::runtime_error("predecessor overlap missed or unrelated ownership released");
    rasterizer.ResolveGpuBuffers(address,12288);
    if(released.size()!=2||released[1]!=address+8192)throw std::runtime_error("multi-mapping reconciliation missed GPU ownership");
    if(memory[1024]!=0x12345678||memory[2048]!=0x12345678)throw std::runtime_error("reconciliation damaged guest data");
    rasterizer.ValidateDebugMessages();puts("PASS: bounded overlap lookup and exact ownership boundaries");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;} }
