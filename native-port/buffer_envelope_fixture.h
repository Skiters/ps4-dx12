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
    {
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);SCOPE_EXIT {renderer.Finish();};
        const Rasterizer::ShaderBufferRequest old{base+4096,4096,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&old,1));
        const Rasterizer::ShaderBufferRequest readonly{base+16,16384-32,false,true,true};
        const Rasterizer::ShaderBufferRequest mixed[]{readonly,{base+8192,4096,true,true,true}};
        renderer.PrepareShaderBuffers(mixed);
        renderer.FillPreparedShaderBuffer(base+8192,4096,0x71727374,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.RetainedImages({{base+4096,4096}});renderer.ReleaseCpuBufferWatches(readonly.address,readonly.size);
        renderer.NotifyCpuWrite(base+4096+17,1);renderer.RetainedImages({});
        renderer.ReleaseUnusedReadOnlyWatches(base+4096,4096);
        MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(base+4096),&state,sizeof(state));
        require(state.Protect==PAGE_READWRITE,"Unrelated GPU pages retained an image-only read-only guard");
        VirtualQuery(reinterpret_cast<void*>(base+8192),&state,sizeof(state));
        require(state.Protect==PAGE_NOACCESS,"Image-only guard retirement released actual GPU pages");
        reinterpret_cast<volatile std::byte*>(base)[4096+17]=std::byte{0x77};
        require(renderer.ReadMemory(base+8192,4096),"Retained GPU page could not resolve");renderer.Finish();
        renderer.PrepareShaderBuffers(std::span(&readonly,1));const auto observed=read(renderer,readonly.address,readonly.size);
        for(size_t offset=16;offset<16384-16;++offset){const u8 word[]{0x74,0x73,0x72,0x71};
            const auto expected=offset==4096+17?std::byte{0x77}:offset>=8192&&offset<12288?std::byte{word[offset%4]}:std::byte{0x31};
            require(observed[offset-16]==expected&&physical[offset]==expected,"Image-only retirement changed CPU winner, GPU contents or padding");}
        require(committed==4096,"Image-only retirement widened actual GPU commit");
        renderer.ReleaseCpuBufferWatches(base,16384);renderer.ValidateDebugMessages();
    }
    {
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);SCOPE_EXIT {renderer.Finish();};
        const Rasterizer::ShaderBufferRequest readonly{base,8192,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&readonly,1));
        renderer.NotifyCpuWrite(base+17,1);
        require(renderer.TrackReadOnly(base,4096),"Cannot rearm borrowed dirty-page proof");
        renderer.NotifyCpuWrite(base+17,1);
        MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(base),&state,sizeof(state));
        require(state.Protect==PAGE_READWRITE,"Known-dirty CPU page kept a rearmed read-only proof");
        VirtualQuery(reinterpret_cast<void*>(base+4096),&state,sizeof(state));
        require(state.Protect==PAGE_READONLY,"Dirty-page release lost its protected neighbour");
        reinterpret_cast<volatile std::byte*>(base)[17]=std::byte{0x77};
        renderer.PrepareShaderBuffers(std::span(&readonly,1));
        const auto result=read(renderer,base,8192);
        for(size_t offset=0;offset<result.size();++offset)
            require(result[offset]==(offset==17?std::byte{0x77}:std::byte{0x31}),
                "Rearmed dirty-page proof lost CPU store or neighbour contents");
        renderer.ReleaseCpuBufferWatches(base,8192);renderer.ValidateDebugMessages();
    }
    {
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);SCOPE_EXIT {renderer.Finish();};
        const Rasterizer::ShaderBufferRequest old{base+4096,4096,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&old,1));
        const Rasterizer::ShaderBufferRequest grown[]{
            {base+16,16384-32,false,true,true},{base+8192,4096,true,true,true}};
        renderer.PrepareShaderBuffers(grown);
        renderer.FillPreparedShaderBuffer(base+8192,4096,0x71727374,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        // This page's existing RO protection is also retained by an image
        // owner. Dropping this buffer proof must not open a CPU-store window.
        renderer.RetainedImages({{base+4096,4096}});
        renderer.ReleaseCpuBufferWatches(base+16,16384-32);
        MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(base+8192),&state,sizeof(state));
        require(state.Protect==PAGE_NOACCESS,"Partial CPU proof retirement released GPU ownership");
        VirtualQuery(reinterpret_cast<void*>(base+4096),&state,sizeof(state));
        require(state.Protect==PAGE_READONLY,"Partial CPU proof retirement released a retained image page");
        renderer.RetainedImages({});renderer.Untrack(base+4096,4096);
        require(renderer.ReadMemory(base+8192,4096),"Partial proof GPU contents could not resolve");renderer.Finish();
        require(committed==4096,"Partial proof retirement changed committed write extent");
        for(size_t offset=0;offset<bytes;offset+=4){u32 word{};memcpy(&word,physical+offset,4);
            require(word==(offset>=8192&&offset<12288?0x71727374U:0x31313131U),
                "Partial proof retirement lost GPU data or changed physical padding");}
        renderer.ValidateDebugMessages();
    }
    {
        u64 commits{},committed{};Adapter renderer(device);attach(renderer,commits,committed);
        renderer.NotifyHostMapping(base,bytes,true,false);
        const Rasterizer::ShaderBufferRequest writer{base+0x810,1024,true,true,true};
        renderer.NotifyHostMapping(writer.address,writer.size,true,true);
        renderer.PrepareShaderBuffers(std::span(&writer,1));
        require(!renderer.BuffersMayDefer(std::span(&writer,1)),
            "Writable ownership expanded beyond confirmed writable guest rights");
    }
    {
        u64 commits{},committed{};Adapter renderer(device);attach(renderer,commits,committed);
        const Rasterizer::ShaderBufferRequest mixed[]{
            {base+64,64,false,false,true},{base+0x810,1024,true,true,true}};
        const auto views=renderer.PrepareShaderBuffers(mixed);
        require(views[0].resource.Get()!=views[1].resource.Get(),
            "Ownership rounding merged incompatible simultaneous SRV/UAV allocations");
        require(!renderer.BuffersMayDefer(mixed),
            "Ownership rounding crossed an incompatible simultaneous resource domain");
    }    {
        require(bytes>=65536,"Unaligned ownership fixture needs full committed pages");
        memset(physical,0x37,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        const Rasterizer::ShaderBufferRequest writer{base+0x810,43008,true,true,true};
        renderer.PrepareShaderBuffers(std::span(&writer,1));
        renderer.MarkShaderBufferWrites(std::span(&writer,1));
        require(renderer.BuffersMayDefer(std::span(&writer,1)),
            "Committed unaligned writer lost page-envelope ownership before execution");        renderer.FillPreparedShaderBuffer(writer.address,writer.size,0x6b6b6b6b,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.ReadMemory(writer.address,writer.size);renderer.Finish();
        for(u64 i=0;i<bytes;++i)require(physical[i]==
            (i>=0x810&&i<0x810+43008?std::byte{0x6b}:std::byte{0x37}),
            "Unaligned ownership growth changed GPU contents or untouched page padding");
        require(committed==writer.size,"Page ownership rounding committed extra guest bytes");
    }    {
        // Upload-arena CPU streams are immutable snapshots, not generations
        // of the mutable DEFAULT mapping subsequently created at this address.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);renderer.EnableBufferStreaming(true);
        const Rasterizer::ShaderBufferRequest stream{base+4096,64,false,false,false};
        auto selected=renderer.PrepareShaderBuffers(std::span(&stream,1));
        require(bool(selected.front().lease),"Immutable final-view fixture did not acquire a CPU stream lease");
        const Rasterizer::ShaderBufferRequest resident{base,8192,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&resident,1));
        renderer.FillPreparedShaderBuffer(stream.address,stream.size,0x51525354,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto final=renderer.RefreshPreparedShaderBuffers(std::span(&stream,1),selected);
        require(final.front().resource.Get()==selected.front().resource.Get()&&final.front().offset==selected.front().offset&&
            final.front().lease==selected.front().lease,"Final binding substituted mutable B for an immutable CPU stream");
        auto rb=Buffer(device,stream.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,final.front().resource.Get(),final.front().offset,stream.size);
        renderer.Finish();void* mapped{};D3D12_RANGE all{0,SIZE_T(stream.size)};Check(rb.handle->Map(0,&all,&mapped));
        bool unchanged=true;for(size_t i=0;i<stream.size;++i)unchanged&=static_cast<const std::byte*>(mapped)[i]==std::byte{0x31};
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);
        require(unchanged&&commits==0,"Final CPU stream contents were replaced or committed");
        auto writer=stream;writer.written=writer.unordered_access=true;bool rejected=false;
        try{renderer.RefreshPreparedShaderBuffers(std::span(&writer,1),selected);}
        catch(const std::logic_error&){rejected=true;}
        require(rejected,"Immutable CPU stream was accepted for a guest write");
        renderer.ValidateDebugMessages();
    }
    {
        // Original GPU fill extends a preceding writable arena. Preserve its
        // untouched formatted prefix, and publish only actual old/new writes.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        const VAddr old=base+4096;constexpr u64 old_size=8192,new_size=12288;
        const Rasterizer::ShaderBufferRequest source{old,old_size,true,true,true};
        const Rasterizer::BufferWrite actual{old,4};
        auto selected=renderer.PrepareShaderBuffers(std::span(&source,1),std::span(&actual,1));
        renderer.FillPreparedShaderBuffer(old,old_size,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillBuffer(old+2048,new_size-2048,0x51525354,false);
        const auto final=renderer.RefreshPreparedShaderBuffers(std::span(&source,1),selected);
        require(final.front().resource.Get()!=selected.front().resource.Get()&&
            renderer.CachedBufferBytes()==new_size,"GPU fill did not preserve/grow its original arena");
        const auto result=read(renderer,old,new_size);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            require(word==(offset<2048?0x41424344U:0x51525354U),
                "GPU fill lost the untouched formatted prefix or exact fill bytes");
        }
        require(renderer.ReadMemory(old,new_size),"GPU fill CPU visibility failed");
        require(committed==4+new_size-2048,"GPU fill committed formatted padding or lost an actual write");
        for(size_t offset=0;offset<bytes;offset+=4){u32 word{};memcpy(&word,physical+offset,4);
            const auto expected=offset==4096?0x41424344U:
                offset>=4096+2048&&offset<4096+new_size?0x51525354U:0x31313131U;
            require(word==expected,"GPU fill changed physical B0 outside actual guest writes");
        }
        renderer.ValidateDebugMessages();
    }
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
            const Rasterizer::ShaderBufferRequest nested{old,4,false,true,true};bool rejected=false;
            try{renderer.PrepareShaderBuffers(std::span(&nested,1));}
            catch(const std::logic_error&){rejected=true;}
            require(rejected,"A growth witness callback reentered resource materialization");
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
        if(mode!=0&&store==2)renderer.PhysicalReader([&](VAddr address,std::span<std::byte> out){
            if(address<base||address-base>bytes||out.size()>bytes-(address-base))return false;
            memcpy(out.data(),physical+address-base,out.size());
            // Owned growth no longer downgrades the old pages to RO. Inject
            // the existing same-value-store control at the physical witness
            // phase instead; its new CPU marker must reject deferred growth.
            if(inject){inject=false;
                reinterpret_cast<volatile std::byte*>(physical+old-base+4096)[17]=std::byte{0x31};
                renderer.NotifyCpuWrite(old+4096+17,1);
            }
            return true;
        });
        if(store==1){
            reinterpret_cast<volatile std::byte*>(physical+old-base+4096)[17]=std::byte{0x31};
            renderer.NotifyCpuWrite(old+4096+17,1);
        }
        const Rasterizer::ShaderBufferRequest extension{old+4096,8192,false,true,true};
        const auto before_growth=renderer.SubmittedFence();
        const auto grown=renderer.PrepareShaderBuffers(std::span(&extension,1));
        require(grown.size()==1&&grown.front().size==8192&&grown.front().offset==4096&&
            grown.front().resource.Get()!=old_native,"Growth changed sharp bounds or failed to preserve the old tail");
        const bool owned_growth=mode!=0&&store==0;
        require(renderer.CachedBufferBytes()==new_size&&growth_guards==(owned_growth?0u:1u)&&renderer.guard_releases==0,
            "Growth double-counted B or opened a guard-release window");
        if(owned_growth){MEMORY_BASIC_INFORMATION state{};VirtualQuery(reinterpret_cast<void*>(old),&state,sizeof(state));
            require(state.Protect==PAGE_NOACCESS&&renderer.SubmittedFence()==before_growth&&commits==0,
                "Owned envelope growth released protection, submitted or committed pending guest writes");}
        {
            const auto final=renderer.RefreshPreparedShaderBuffers(std::span(&source,1),old_views);
            require(final.front().resource.Get()==grown.front().resource.Get()&&final.front().offset==0&&
                final.front().size==old_size,"Final binding did not follow its GPU-preserving envelope generation");
            auto wrong=old_views.front();++wrong.offset;bool rejected=false;
            try{renderer.RefreshPreparedShaderBuffers(std::span(&source,1),std::span(&wrong,1));}
            catch(const std::logic_error&){rejected=true;}
            require(rejected,"Final binding accepted a changed original sharp offset");
        }
        old_views.clear();
        if(placed)require(!old_allocation.expired(),"Old placed B storage died before its copy fence");
        const auto result=read(renderer,old,new_size);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            const auto expected=offset<4096?0x41424344U:offset<8192&&store==0?0x51525354U:0x31313131U;
            require(word==expected,"Expanded B lost GPU-only bytes, padding, or an actual CPU-store page");
        }
        std::vector<std::byte> expected(size_t(bytes),std::byte{0x31});
        if(mode==1)for(size_t offset=4096;offset<8192;offset+=4){const u32 word=0x41424344;memcpy(expected.data()+offset,&word,4);}
        if(owned_growth)require(renderer.ReadMemory(old,old_size),"Grown owned B did not become visible on explicit CPU read");
        require(!memcmp(physical,expected.data(),expected.size())&&commits==(mode==1?1u:0u)&&
            committed==(mode==1?4096u:0u),"Envelope growth committed formatted B as a new guest write");
        renderer.CollectRetirements();
        if(placed)require(old_allocation.expired(),"Old B growth lease survived completed retirement");
        renderer.ValidateDebugMessages();
    }
    for(unsigned failure=0;failure<4;++failure){
        // Join two protected, still-dirty B versions. New gaps acquire their
        // own guards; rejection/exception must roll back only those gaps.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        bool injecting=false;unsigned rollbacks{};
        renderer.SetBufferOwnership([&](VAddr address,u64 size){
            if(injecting&&(failure==1||failure==2)&&address==base+8192){
                if(failure==2)throw std::runtime_error("Injected owned growth gap failure");
                return false;
            }
            if(injecting&&failure==3&&address==base+8192){
                const u32 cpu_winner=0x61626364;
                memcpy(physical+64,&cpu_winner,4);renderer.NotifyCpuWrite(base+64,4);
            }
            return renderer.TrackOpaque(address,size);
        },[&](VAddr address,std::span<const std::byte> data){
            ++commits;committed+=data.size();memcpy(physical+address-base,data.data(),data.size());return true;
        },[&](VAddr address,u64 size){++rollbacks;renderer.Untrack(address,size);});
        const Rasterizer::ShaderBufferRequest sources[]{{base+4096,4096,true,true,true},{base+12288,4096,true,true,true}};
        const Rasterizer::BufferWrite writes[]{{base+4100,4},{base+12296,4}};
        auto old=renderer.PrepareShaderBuffers(sources,std::span(writes));
        renderer.FillPreparedShaderBuffer(base+4096,4096,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillPreparedShaderBuffer(base+12288,4096,0x51525354,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto before=renderer.SubmittedFence();injecting=true;
        const Rasterizer::ShaderBufferRequest joined{base,20480,false,true,true};
        bool rejected=false;
        try{renderer.PrepareShaderBuffers(std::span(&joined,1));}
        catch(const std::runtime_error&){rejected=true;}
        require(rejected==(failure==2),"Owned growth did not propagate the injected gap exception");
        if(failure==2){
            MEMORY_BASIC_INFORMATION prefix{},owned{};
            VirtualQuery(reinterpret_cast<void*>(base),&prefix,sizeof(prefix));
            VirtualQuery(reinterpret_cast<void*>(base+4096),&owned,sizeof(owned));
            require(rollbacks==1&&prefix.Protect==PAGE_READWRITE&&owned.Protect==PAGE_NOACCESS&&
                renderer.CachedBufferBytes()==8192&&renderer.SubmittedFence()==before&&commits==0,
                "Failed owned growth changed old ownership, submitted, or leaked a new gap guard");
            const auto retained=renderer.RefreshPreparedShaderBuffers(sources,old);
            require(retained[0].resource.Get()==old[0].resource.Get()&&retained[1].resource.Get()==old[1].resource.Get(),
                "Failed owned growth replaced a preceding source generation");
            injecting=false;renderer.PrepareShaderBuffers(std::span(&joined,1));
        }
        if(failure==0||failure==2){
            require(renderer.SubmittedFence()==before&&commits==0,"Joining protected B drained its pending writes");
            for(u64 offset=0;offset<20480;offset+=4096){MEMORY_BASIC_INFORMATION state{};
                VirtualQuery(reinterpret_cast<void*>(base+offset),&state,sizeof(state));
                require(state.Protect==PAGE_NOACCESS,"Joined B has an unprotected page");}
        }else require(rollbacks==(failure==1?1u:3u)&&committed==8,
            "Rejected gap/CPU winner failed to roll back new guards and reconcile only actual old writes");
        old.clear();const auto result=read(renderer,base,20480);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            const auto expected=failure==3&&offset==64?0x61626364U:offset>=4096&&offset<8192?0x41424344U:
                offset>=12288&&offset<16384?0x51525354U:0x31313131U;
            require(word==expected,"Joined protected B lost GPU contents or physical gap bytes");
        }
        require(renderer.ReadMemory(base,20480)&&committed==8&&commits==2,
            "Joined B failed exact-write CPU visibility after success/retry/fallback");
        for(size_t offset=0;offset<bytes;offset+=4){u32 word{};memcpy(&word,physical+offset,4);
            require(word==(failure==3&&offset==64?0x61626364U:offset==4100?0x41424344U:offset==12296?0x51525354U:0x31313131U),
                "Joined B committed formatted bytes, gaps or outside padding");}
        renderer.ValidateDebugMessages();
    }
    for(const bool prior_write:{false,true}){
        // Disjoint SRV/UAV byte ranges can share an old B allocation. Preserve
        // that complete canonical version/guard; the private SRV is a late
        // GPU snapshot, not CPU B0 and not an immutable early-stage snapshot.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        const Rasterizer::ShaderBufferRequest initial{base,16384,prior_write,true,true};
        const Rasterizer::BufferWrite actual{base+4,4};
        renderer.PrepareShaderBuffers(std::span(&initial,1),prior_write?std::span(&actual,1):std::span<const Rasterizer::BufferWrite>{});
        renderer.FillPreparedShaderBuffer(base,16384,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const Rasterizer::ShaderBufferRequest requests[]{{base+12,4096,false,false,true},{base+12352,4,true,true,true}};
        auto selected=renderer.PrepareShaderBuffers(requests);
        require(selected[0].read_snapshot&&selected[0].resource.Get()!=selected[1].resource.Get()&&
            renderer.CachedBufferBytes()==16384,"Protected mixed views replaced/split canonical B");
        renderer.FillPreparedShaderBuffer(base+12,4,0x51525354,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.FillPreparedShaderBuffer(base+12352,4,0x61626364,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        renderer.SynchronizePreparedDmaBuffers(requests,selected);
        const Rasterizer::ShaderBufferRequest extension{base+16380,4100,false,true,true};
        const auto before=renderer.SubmittedFence();renderer.PrepareShaderBuffers(std::span(&extension,1));
        auto final=renderer.RefreshPreparedShaderBuffers(requests,selected);
        MEMORY_BASIC_INFORMATION protection{};VirtualQuery(reinterpret_cast<void*>(base),&protection,sizeof(protection));
        require(protection.Protect==PAGE_NOACCESS&&renderer.SubmittedFence()==before&&commits==0&&
            final[0].resource.Get()==selected[0].resource.Get(),"Private GPU read refresh lost protection/pending writes or reallocated its bound resource");
        auto rb=Buffer(device,4096,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const auto native=final[0].resource.Get();std::weak_ptr<Rasterizer::ShaderReadSnapshot> lifetime=final[0].read_snapshot;
        final[0].read_snapshot->gpu->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,final[0].resource.Get(),0,4096);
        selected.clear();final.clear();require(!lifetime.expired(),"Private shader-read allocation died before its recorded copy fence");
        renderer.Finish();void* mapped{};D3D12_RANGE all{0,4096};Check(rb.handle->Map(0,&all,&mapped));
        bool valid=true;for(size_t offset=0;offset<4096;offset+=4){u32 word{};memcpy(&word,static_cast<const std::byte*>(mapped)+offset,4);
            valid&=word==(offset==0?0x51525354U:0x41424344U);}
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);require(valid,"Protected private shader read used stale contents or CPU B0");
        renderer.CollectRetirements();require(lifetime.expired(),"Completed private shader-read state remained pinned");
        const auto canonical=read(renderer,base,20480);
        for(size_t offset=0;offset<canonical.size();offset+=4){u32 word{};memcpy(&word,canonical.data()+offset,4);
            const auto expected=offset==12?0x51525354U:offset==12352?0x61626364U:offset<16384?0x41424344U:0x31313131U;
            require(word==expected,"Mixed-view preparation lost canonical GPU bytes, writes, gaps or padding");}
        require(renderer.ReadMemory(base,20480)&&committed==(prior_write?8u:4u),"Mixed views committed an envelope instead of original exact writes");
        for(size_t offset=0;offset<bytes;offset+=4){u32 word{};memcpy(&word,physical+offset,4);
            require(word==(offset==12352?0x61626364U:prior_write&&offset==4?0x41424344U:0x31313131U),
                "Mixed native views changed physical B0 outside actual guest stores");}
        auto readonly=std::vector<Rasterizer::ShaderBufferRequest>(std::begin(requests),std::end(requests));
        for(auto& request:readonly)request.written=false;
        auto reused=renderer.PrepareShaderBuffers(readonly);
        require(reused[0].resource.Get()==native,"Retired private shader-read GPU storage was not reused");
        renderer.ValidateDebugMessages();
    }
    {
        // Follow an original view across two successive generations, but never
        // accept a retired/recreated mapping merely because its address matches.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        Rasterizer::ShaderBufferRequest source{base+4096,4096,false,true,true};source.publish_dma_residency=false;
        auto selected=renderer.PrepareShaderBuffers(std::span(&source,1));
        renderer.FillPreparedShaderBuffer(source.address,source.size,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const Rasterizer::ShaderBufferRequest left{base,12288,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&left,1));
        const Rasterizer::ShaderBufferRequest right{base+8192,8192,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&right,1));
        auto final=renderer.RefreshPreparedShaderBuffers(std::span(&source,1),selected);
        require(final.front().resource.Get()!=selected.front().resource.Get()&&final.front().offset==4096,
            "Two-step growth lost the preceding generation or rebased offset");
        auto wrong=selected.front();wrong.resource=final.front().resource;bool rejected=false;
        try{renderer.RefreshPreparedShaderBuffers(std::span(&source,1),std::span(&wrong,1));}
        catch(const std::logic_error&){rejected=true;}
        require(rejected,"Final binding accepted a substituted native resource");
        const auto contents=read(renderer,source.address,source.size);
        for(size_t offset=0;offset<contents.size();offset+=4){u32 word{};memcpy(&word,contents.data()+offset,4);
            require(word==0x41424344,"Final generation changed old GPU-only B1");}
        renderer.UnmapMemory(base,bytes);renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
        renderer.PrepareShaderBuffers(std::span(&source,1));rejected=false;
        try{renderer.RefreshPreparedShaderBuffers(std::span(&source,1),selected);}
        catch(const std::logic_error&){rejected=true;}
        require(rejected&&commits==0&&committed==0,"Final binding accepted unrelated recreated B or committed formatted bytes");
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
    {
        // Image prewrite preparation may resolve a reserved, not-yet-written B
        // and downgrade it to RO. Its later shader bind must reacquire the same
        // allocation, preserving formatted GPU bytes and exact guest stores.
        memset(physical,0x31,size_t(bytes));u64 commits{},committed{};
        Adapter renderer(device);attach(renderer,commits,committed);
        Rasterizer::ShaderBufferRequest writer{base,8192,true,true,true};
        writer.write_byte_adjust=4;writer.original_write_bytes=4;
        const auto views=renderer.PrepareShaderBuffers(std::span(&writer,1),std::span<const Rasterizer::BufferWrite>{});
        renderer.FillPreparedShaderBuffer(base+4096,4096,0x51525354,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        require(renderer.BuffersMayDefer(std::span(&writer,1)),"Prewrite B never acquired ownership");
        require(renderer.ReadMemory(base,4),"Image prewrite CPU read failed");
        require(!renderer.BuffersMayDefer(std::span(&writer,1)),"CPU resolution did not downgrade the reserved B");
        renderer.MarkShaderBufferWrites(std::span(&writer,1));
        const auto current=renderer.PreparedDmaBuffer(base,8192);
        require(renderer.BuffersMayDefer(std::span(&writer,1))&&current&&
            current->resource.Get()==views.front().resource.Get()&&current->offset==views.front().offset,
            "Shader bind failed to restore ownership of its exact selected B");
        renderer.FillPreparedShaderBuffer(base+4,4,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto result=read(renderer,base,8192);
        for(size_t offset=0;offset<result.size();offset+=4){u32 word{};memcpy(&word,result.data()+offset,4);
            require(word==(offset==4?0x41424344U:offset>=4096?0x51525354U:0x31313131U),
                "Ownership restoration lost formatted B or changed its original shader write");
        }
        require(commits==0&&committed==0,"GPU-only verification prematurely committed owned B");
        require(renderer.ReadMemory(base+4,4),"CPU visibility request failed after restored shader ownership");
        require(commits==1&&committed==4,"Ownership restoration committed an envelope instead of the exact shader store");
        for(size_t i=0;i<bytes;++i)require(physical[i]==(i>=4&&i<8?std::byte(0x44-(i-4)):std::byte{0x31}),
            "Restoring B ownership committed private I/format bytes into physical guest memory");
        renderer.ValidateDebugMessages();
    }
}
}
