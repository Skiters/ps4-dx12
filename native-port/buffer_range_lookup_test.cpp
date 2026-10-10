// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include <chrono>
#include <cstdio>
using namespace NativeDX12;
int main() { try {
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
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
