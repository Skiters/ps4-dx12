// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include "texture_heap_pressure_fixture.h"
#include "texture_initial_layout_fixture.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(int argc,char** argv){try{
    const bool large=argc>1&&std::string_view(argv[1])=="large";
    const bool texture_heaps=argc>1&&std::string_view(argv[1])=="texture-heaps";
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    if(argc>1&&std::string_view(argv[1])=="texture-initial-layout"){
        if(!RunTextureInitialLayoutFixture(device.Get())){puts("SKIP: native format casting unavailable");return 77;}
        puts("PASS: cast-list COPY_DEST and COMMON creation, enhanced transitions and actual texture word readback");return 0;
    }
    if(texture_heaps){RunTextureHeapPressureFixture(device.Get());return 0;}
    {
        constexpr u64 page=65536;
        PlacedBufferAllocator buffers(device.Get(),page,4*page);
        auto a=buffers.Allocate(page),b=buffers.Allocate(page);
        buffers.SetBudget(page);a={};b={};
        bool rejected=false;try{auto oversized=buffers.Allocate(2*page);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Buffer allocator forgot the reduced budget after releasing old leases");
        auto compact_resource=buffers.Allocate(page);
        if(buffers.CommittedBytes()!=page)throw std::runtime_error("Buffer allocator did not reclaim idle heaps under pressure");
        PlacedTextureAllocator textures(device.Get(),page,4*page);
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=desc.Height=128;desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        auto x=textures.Allocate(desc,D3D12_RESOURCE_STATE_COMMON),y=textures.Allocate(desc,D3D12_RESOURCE_STATE_COMMON);textures.SetBudget(page);x={};y={};
        desc.Width=256;rejected=false;
        try{auto oversized=textures.Allocate(desc,D3D12_RESOURCE_STATE_COMMON);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Texture allocator forgot the reduced budget after releasing old leases");
        desc.Width=128;auto restored=textures.Allocate(desc,D3D12_RESOURCE_STATE_COMMON);
        if(textures.CommittedBytes()!=page)throw std::runtime_error("Texture allocator did not reclaim idle heaps under pressure");
    }
    const u64 Bytes=(large?832ull:320ull)*1024*1024;
    struct Allocation {void* memory{};~Allocation(){if(memory)VirtualFree(memory,0,MEM_RELEASE);}} allocations[3];
    Rasterizer rasterizer(device.Get());
    printf("Adapter buffer cache budget: %llu bytes\n",static_cast<unsigned long long>(rasterizer.BufferCacheBudget()));
    if(!large&&rasterizer.BufferCacheBudget()<3*Bytes){puts("SKIP: adapter budget cannot hold the 960-MiB test batch");return 77;}
    std::vector<Rasterizer::ShaderBufferRequest> requests;
    for(u32 i=0;i<3;++i){
        allocations[i].memory=VirtualAlloc(nullptr,Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        if(!allocations[i].memory)throw std::runtime_error("Cannot allocate large guest fixture");
        auto* words=static_cast<u32*>(allocations[i].memory);
        words[0]=0xabcd0000+i;words[Bytes/4-1]=0xface0000+i;
        const auto address=reinterpret_cast<VAddr>(words);rasterizer.MapMemory(address,Bytes);
        requests.push_back({address,Bytes,false,false});
    }
    const auto verify=[&](const auto& views,bool changed){
        auto rb=Buffer(device.Get(),36,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        for(u32 i=0;i<3;++i){
            Resource source;source.handle=views[i].resource;
            source.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            source.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            const u64 offsets[]{0,12345*4,Bytes-4};
            for(u32 j=0;j<3;++j)rasterizer.RecordingList()->CopyBufferRegion(rb.handle.Get(),i*12+j*4,source.handle.Get(),views[i].offset+offsets[j],4);
            source.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        rasterizer.Finish();void* data{};D3D12_RANGE range{0,36};Check(rb.handle->Map(0,&range,&data));
        u32 values[9];memcpy(values,data,sizeof(values));D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
        for(u32 i=0;i<3;++i)if(values[i*3]!=0xabcd0000+i||values[i*3+1]!=(changed?0x12340000+i:0)||values[i*3+2]!=0xface0000+i)
            throw std::runtime_error("Large cache batch lost contents or CPU refresh");
    };
    constexpr u64 AliasOffset=16ull*1024*1024;
    requests.push_back({requests[0].address+AliasOffset,Bytes-AliasOffset,false,false});
    const auto first=rasterizer.PrepareShaderBuffers(requests);verify(first,false);
    if(first[3].resource.Get()!=first[0].resource.Get()||first[3].offset!=first[0].offset+AliasOffset||
       first[3].size!=Bytes-AliasOffset)
        throw std::runtime_error("Large overlapping view lost its common backing or offset");
    if(rasterizer.CachedBufferBytes()!=3*Bytes)throw std::runtime_error("Large cache batch changed its resident extent");
    for(u32 i=0;i<3;++i)static_cast<u32*>(allocations[i].memory)[12345]=0x12340000+i;
    const auto second=rasterizer.PrepareShaderBuffers(requests);verify(second,true);
    for(u32 i=0;i<3;++i)if(first[i].resource.Get()!=second[i].resource.Get())
        throw std::runtime_error("Unchanged large cache backing was unnecessarily replaced");
    rasterizer.ValidateDebugMessages();
    printf("PASS: buffer/texture budget shrink and heap reclamation; real %llu-MiB cache batch, overlapping views, endpoint contents, incremental CPU refresh and allocation reuse\n",3*Bytes/MemoryMiB);return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
