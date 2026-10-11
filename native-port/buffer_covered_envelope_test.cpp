// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");
    auto device=CreateHardwareDevice();
    constexpr u64 Bytes=4*65536;
    struct Guest{void* p=VirtualAlloc(nullptr,Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        ~Guest(){if(p)VirtualFree(p,0,MEM_RELEASE);}} guest;
    if(!guest.p)throw std::runtime_error("guest allocation");
    auto* words=static_cast<u32*>(guest.p);for(u32 i=0;i<Bytes/4;++i)words[i]=i+19;
    Rasterizer raster(device.Get());raster.EnableBufferArenas(true);
    const auto base=reinterpret_cast<VAddr>(guest.p);
    raster.MapMemory(base,Bytes);raster.NotifyHostMapping(base,Bytes,true,true);
    // A writable sharp creates an exact, page-aligned but non-64KiB envelope.
    // Read-only UAV sharps inside it need only a logical view, even when their
    // optional arena rounding would extend beyond that existing allocation.
    const Rasterizer::ShaderBufferRequest whole[]{{base+4096,2*65536,true,true}};
    auto original=raster.PrepareShaderBuffers(whole);raster.Finish();
    const auto verify=[&](const Rasterizer::ShaderBufferView& view){
        auto rb=Buffer(device.Get(),view.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource source;source.handle=view.resource;source.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        source.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        raster.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,source.handle.Get(),view.offset,view.size);
        source.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        raster.Finish();void* p{};D3D12_RANGE range{0,size_t(view.size)};Check(rb.handle->Map(0,&range,&p));
        const bool equal=memcmp(p,reinterpret_cast<void*>(base+4096+view.offset),size_t(view.size))==0;
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
        if(!equal)throw std::runtime_error("covered logical view lost actual contents");
    };
    for(unsigned iteration=0;iteration<6;++iteration){
        const Rasterizer::ShaderBufferRequest reads[]{{base+4096,65536,false,true},
            {base+4096+65536+16,65536-16,false,true}};
        auto views=raster.PrepareShaderBuffers(reads);
        for(const auto& view:views)if(view.resource.Get()!=original[0].resource.Get())
            throw std::runtime_error("arena rounding recreated an already covering allocation");
        if(views[0].offset!=0||views[1].offset!=65536+16||raster.CachedBufferBytes()!=2*65536)
            throw std::runtime_error("logical view offsets or resident envelope changed");
        verify(views[0]);verify(views[1]);
        words[1024+iteration]=0x12340000+iteration;
        raster.NotifyCpuWrite(base+4096+iteration*4,4);
        auto rewritten=raster.PrepareShaderBuffers(whole);
        if(rewritten[0].resource.Get()!=original[0].resource.Get())throw std::runtime_error("alternating role lost backing");
        verify(rewritten[0]);
    }
    raster.ValidateDebugMessages();
    puts("PASS: exact writable envelope survives six disjoint read/write cycles; full GPU contents, CPU updates and offsets verified; zero recreations after initial upload");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
