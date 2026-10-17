// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_buffer_access.h"
#include "video_core/renderer_d3d12/guest_dma.h"
#include "common/scope_exit.h"
#include <cstring>

namespace NativeDX12 {
inline void RunPreservedBufferBudgetFixture(ID3D12Device* device){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    constexpr u64 bytes=16*MemoryMiB,source_bytes=4*MemoryMiB,hot_bytes=2*MemoryMiB;
    constexpr VAddr base=0x1500000000ULL;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);
    require(section!=nullptr,"Cannot allocate preserved-budget backing");
    auto* guest=static_cast<std::byte*>(MapViewOfFileEx(section,FILE_MAP_ALL_ACCESS,0,0,bytes,reinterpret_cast<void*>(base)));
    auto* physical=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    require(guest&&physical,"Cannot map preserved-budget aliases");
    SCOPE_EXIT {UnmapViewOfFile(guest);UnmapViewOfFile(physical);CloseHandle(section);};
    struct Access:GuestBufferAccess {
        std::byte* physical;u64 commits{},committed{};
        Access(ID3D12Device* device,std::byte* p):GuestBufferAccess(device),physical(p){
            MapMemory(base,bytes);NotifyHostMapping(base,bytes,true,true);
            SetBufferBackingAccess([](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
                [this](VAddr a,std::span<const std::byte> data){++commits;committed+=data.size();
                    memcpy(physical+a-base,data.data(),data.size());return true;});
        }
        bool ReadDmaBacking(VAddr a,std::span<std::byte> out)override{
            if(a<base||a-base>bytes||out.size()>bytes-(a-base))return false;
            memcpy(out.data(),physical+a-base,out.size());return true;
        }
    };
    const auto read=[&](Access& renderer,VAddr address,u64 size){
        auto snapshot=renderer.CapturePreparedShaderBuffer(address,size);
        auto rb=Buffer(device,size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,snapshot.handle.Get(),0,size);renderer.Finish();
        std::vector<std::byte> result(static_cast<size_t>(size));void* mapped{};D3D12_RANGE all{0,SIZE_T(size)};
        Check(rb.handle->Map(0,&all,&mapped));memcpy(result.data(),mapped,result.size());
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);return result;
    };
    for(const bool derived:{false,true}){
        memset(physical,0x31,size_t(bytes));Access renderer(device,physical);
        const Rasterizer::ShaderBufferRequest source{base,source_bytes,false,true,true};
        auto selected=renderer.PrepareShaderBuffers(std::span(&source,1));
        const auto* original=selected.front().resource.Get();
        if(derived){renderer.FillPreparedShaderBuffer(base,source_bytes,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);selected.clear();}
        const auto residency=renderer.DmaMemory().ResidentIntersections(base,source_bytes);
        require(!residency.empty(),"Preserved-budget source never acquired original semantic residency");
        const auto pressure=[&](unsigned index){
            const auto address=base+source_bytes+(index%6)*hot_bytes;
            const Rasterizer::ShaderBufferRequest request{address,hot_bytes,false,true,true};
            auto current=renderer.PrepareShaderBuffers(std::span(&request,1));
            require(current.size()==1&&renderer.CachedBufferBytes()==source_bytes+hot_bytes,
                "Cache admission discarded required backing or retained reconstructible inactive buffers");
            renderer.Finish();
        };
        for(unsigned i=0;i<6;++i)pressure(i);
        auto current=renderer.PreparedDmaBuffer(base,source_bytes);
        require(current&&current->resource.Get()==original&&renderer.DmaMemory().ResidentIntersections(base,source_bytes)==residency,
            "Pressure changed selected B identity or semantic residency");
        current.reset();
        require(renderer.commits==0,"Pressure exposed a private GPU version as guest writes");
        if(!derived){
            selected.clear();
            const Rasterizer::ShaderBufferRequest next{base+source_bytes,hot_bytes,false,true,true};
            renderer.PrepareShaderBuffers(std::span(&next,1));renderer.Finish();
            require(renderer.CachedBufferBytes()<=4*MemoryMiB&&!renderer.PreparedDmaBuffer(base,source_bytes),
                "Released CPU-equal generation remained pinned instead of obeying the cache target");
            renderer.ValidateDebugMessages();continue;
        }
        auto gpu=read(renderer,base,source_bytes);
        for(size_t offset=0;offset<gpu.size();offset+=4){u32 word{};memcpy(&word,gpu.data()+offset,4);
            require(word==0x41424344,"Pressure replaced B1 by physical CPU B0");}
        require(renderer.ReadDataFromGds(0)==0,"Direct Find/GDS admission failed with retained B1");
        const auto after_direct=renderer.PreparedDmaBuffer(base,source_bytes);
        require(after_direct&&after_direct->resource.Get()==original,"Direct Find pressure discarded canonical B1");
        for(u64 i=0;i<source_bytes;++i)require(physical[i]==std::byte{0x31},"Pressure changed physical B0");
        MEMORY_BASIC_INFORMATION state{};VirtualQuery(guest,&state,sizeof(state));
        require(state.Protect==PAGE_READONLY,"Pressure released B's original CPU-store witness");
        // A real write fault must be remembered even for an identical value.
        reinterpret_cast<volatile std::byte*>(guest)[4096+17]=std::byte{0x31};
        for(unsigned i=0;i<6;++i)pressure(i);
        selected=renderer.PrepareShaderBuffers(std::span(&source,1));
        require(selected.front().resource.Get()==original,"Pressure/CPU store recreated the canonical B identity");selected.clear();
        gpu=read(renderer,base,source_bytes);
        for(size_t offset=0;offset<gpu.size();offset+=4){u32 word{};memcpy(&word,gpu.data()+offset,4);
            require(word==(offset>=4096&&offset<8192?0x31313131U:0x41424344U),
                "Pressure lost a same-value CPU winner or overwrote untouched GPU-only pages");}
        const VAddr written=base+2*MemoryMiB+4;
        const Rasterizer::ShaderBufferRequest writer{written,4,true,true,true};
        renderer.PrepareShaderBuffers(std::span(&writer,1));
        renderer.FillPreparedShaderBuffer(written,4,0x61626364,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for(unsigned i=0;i<3;++i)pressure(i);
        require(renderer.commits==0,"Cache pressure prematurely committed a protected actual GPU write");
        require(renderer.ReadMemory(written,4)&&renderer.commits==1&&renderer.committed==4,
            "Preserved GPU write failed exact CPU visibility after pressure");
        for(size_t offset=0;offset<source_bytes;offset+=4){u32 word{};memcpy(&word,physical+offset,4);
            require(word==(offset==written-base?0x61626364U:0x31313131U),
                "Pressure committed formatted B1 or padding instead of the original GPU write");}
        renderer.ValidateDebugMessages();
    }
}
}
