// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/rasterizer.h"
#include "common/scope_exit.h"
#include <cstring>
#include <stdexcept>

namespace NativeDX12 {
template<class Adapter>
void RunBufferEnvelopeFixture(ID3D12Device* device,std::byte* physical,VAddr base,u64 bytes){
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const std::vector<std::byte> saved(physical,physical+bytes);
    SCOPE_EXIT {memcpy(physical,saved.data(),saved.size());};
    const auto attach=[&](Adapter& renderer,u64& commits,u64& committed){
        renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
        renderer.SetBufferBackingAccess([=](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
            [&,base](VAddr a,std::span<const std::byte> data){
                ++commits;committed+=data.size();memcpy(physical+a-base,data.data(),data.size());return true;
            });
        renderer.PhysicalReader([=](VAddr a,std::span<std::byte> out){
            if(a<base||a-base>bytes||out.size()>bytes-(a-base))return false;
            memcpy(out.data(),physical+a-base,out.size());return true;
        });
    };
    const auto read=[&](Adapter& renderer,VAddr address,u64 size){
        auto snapshot=renderer.CapturePreparedShaderBuffer(address,size);
        auto rb=Buffer(device,size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,snapshot.handle.Get(),0,size);renderer.Finish();
        std::vector<std::byte> result(static_cast<size_t>(size));void* mapped{};
        D3D12_RANGE all{0,SIZE_T(size)};Check(rb.handle->Map(0,&all,&mapped));memcpy(result.data(),mapped,result.size());
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);return result;
    };
    // mode0: RO B1; mode1: one real GPU-written page and one formatted page;
    // mode2: owned reservation with no actual writes. Store0/1/2: no store,
    // actual same-value store before growth, or during the new guard callback.
    for(unsigned mode=0;mode<3;++mode)for(unsigned store=0;store<3;++store){
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        const VAddr old=base+4096;constexpr u64 old_size=8192,new_size=12288;
        unsigned growth_guards{};bool inject=store==2;
        renderer.BeforeWatch([&](VAddr a,u64 n){if(a!=old||n!=new_size)return;
            MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(old),&state,sizeof(state));
            require(state.Protect==PAGE_READONLY,"Buffer growth released the original RO guard");
            ++growth_guards;
            if(inject){inject=false;
                reinterpret_cast<volatile std::byte*>(physical+old-base+4096)[17]=std::byte{0x31};
                renderer.NotifyCpuWrite(old+4096+17,1);
            }
        });
        const Rasterizer::ShaderBufferRequest source{old,old_size,mode!=0,true,true};
        const Rasterizer::BufferWrite written{old,4096};
        auto old_views=mode==1?renderer.PrepareShaderBuffers(std::span(&source,1),std::span(&written,1)):
            mode==2?renderer.PrepareShaderBuffers(std::span(&source,1),std::span<const Rasterizer::BufferWrite>{}):
                renderer.PrepareShaderBuffers(std::span(&source,1));
        const auto* old_native=old_views.front().resource.Get();
        const bool placed=bool(old_views.front().allocation);std::weak_ptr<void> old_allocation=old_views.front().allocation;
        renderer.FillPreparedShaderBuffer(old,4096,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillPreparedShaderBuffer(old+4096,4096,0x51525354,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        if(store==1){
            reinterpret_cast<volatile std::byte*>(physical+old-base+4096)[17]=std::byte{0x31};
            renderer.NotifyCpuWrite(old+4096+17,1);
        }
        const Rasterizer::ShaderBufferRequest extension{old+4096,8192,false,true,true};
        const auto grown=renderer.PrepareShaderBuffers(std::span(&extension,1));
        require(grown.size()==1&&grown.front().size==8192&&grown.front().offset==4096&&
            grown.front().resource.Get()!=old_native,"Growth changed sharp bounds or failed to preserve the old tail");
        require(renderer.CachedBufferBytes()==new_size&&growth_guards==1&&renderer.guard_releases==0,
            "Growth double-counted B or opened a guard-release window");
        old_views.clear();
        if(placed)require(!old_allocation.expired(),"Old placed B storage died before its copy fence");
        const auto result=read(renderer,old,new_size);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            const auto expected=offset<4096?0x41424344U:offset<8192&&store==0?0x51525354U:0x31313131U;
            require(word==expected,"Expanded B lost GPU-only bytes, padding, or an actual CPU-store page");
        }
        std::vector<std::byte> expected(size_t(bytes),std::byte{0x31});
        if(mode==1)for(size_t offset=4096;offset<8192;offset+=4){const u32 word=0x41424344;memcpy(expected.data()+offset,&word,4);}
        require(!memcmp(physical,expected.data(),expected.size())&&commits==(mode==1?1u:0u)&&
            committed==(mode==1?4096u:0u),"Envelope growth committed formatted B as a new guest write");
        renderer.CollectRetirements();
        if(placed)require(old_allocation.expired(),"Old B growth lease survived completed retirement");
        renderer.ValidateDebugMessages();
    }
    {
        // One request bridges two preceding B versions plus fresh physical
        // backing. Neither old prefix/suffix belongs to the narrow new sharp.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        const Rasterizer::ShaderBufferRequest sources[]{{base,4096,false,true,true},{base+8192,4096,false,true,true}};
        auto old_views=renderer.PrepareShaderBuffers(sources);
        renderer.FillPreparedShaderBuffer(base,4096,0x61626364,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillPreparedShaderBuffer(base+8192,4096,0x71727374,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const Rasterizer::ShaderBufferRequest bridge{base+2048,8192,false,true,true};
        const auto views=renderer.PrepareShaderBuffers(std::span(&bridge,1));old_views.clear();
        require(views.front().offset==2048&&views.front().size==8192&&renderer.CachedBufferBytes()==12288,
            "Bridged B views lost their original bounds or complete old allocation geometry");
        const auto result=read(renderer,base,12288);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            require(word==(offset<4096?0x61626364U:offset<8192?0x31313131U:0x71727374U),
                "Bridged B allocation lost a prior GPU version or initialized its gap from I");
        }
        require(commits==0&&committed==0&&renderer.guard_releases==0,
            "Joining B versions changed CPU ownership or released guards");
        for(size_t i=0;i<bytes;++i)require(physical[i]==std::byte{0x31},"Joining B versions changed physical CPU backing");
        renderer.ValidateDebugMessages();
    }
    {
        // The store is outside the preceding tiny mapping but on the same
        // physical page. The provisional enlarged B must record its CPU winner
        // even before that B is visible in the ordinary mapping index.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        require(renderer.TrackReadOnly(base,4096),"Tiny B image-witness guard setup failed");
        const Rasterizer::ShaderBufferRequest tiny{base+64,4,false,true,true};
        auto old_views=renderer.PrepareShaderBuffers(std::span(&tiny,1));
        renderer.FillPreparedShaderBuffer(base+64,4,0x81828384,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        bool injected=false;
        renderer.BeforeWatch([&](VAddr address,u64 size){if(address!=base||size!=4096||injected)return;
            injected=true;reinterpret_cast<volatile std::byte*>(physical)[512]=std::byte{0x31};
            renderer.NotifyCpuWrite(base+512,1);
        });
        const Rasterizer::ShaderBufferRequest full{base,4096,false,true,true};
        const auto current=renderer.PrepareShaderBuffers(std::span(&full,1));old_views.clear();
        const auto result=read(renderer,base,4096);
        require(injected&&commits==0&&committed==0&&current.front().size==4096,
            "Provisional B CPU-store fixture did not use the real growth path");
        for(const auto value:result)require(value==std::byte{0x31},
            "A CPU store outside the old tiny sharp failed to restore its complete resident page");
        renderer.ValidateDebugMessages();
    }
}
}
