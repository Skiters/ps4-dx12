// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include <cstring>
#include <stdexcept>

namespace NativeDX12 {
// Exercises actual cache election/reclamation and its ownership callback. The
// separate B and I GPU contents deliberately disagree with physical CPU bytes.
inline void RunInvalidImagePressureFixture(ID3D12Device* device){
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    for(const auto tile:{8u,10u})for(const bool callback_pin:{false,true}){
        GuestTextureDesc desc;desc.width=32;desc.height=16;desc.pitch=64;
        desc.layers=desc.view_layers=desc.levels=2;desc.tile_mode=tile;
        desc.data_format=4;desc.number_format=7;desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> physical(size_t(layout.guest_bytes),std::byte{0x31});
        const auto original=physical;desc.address=reinterpret_cast<VAddr>(physical.data());
        Rasterizer renderer(device);renderer.MapMemory(desc.address,layout.guest_bytes);
        GuestTextureCache cache(device);
        const Rasterizer::ShaderBufferRequest request{desc.address,layout.guest_bytes,false,true,true};
        const auto buffers=renderer.PrepareShaderBuffers(std::span(&request,1));
        renderer.FillPreparedShaderBuffer(desc.address,layout.guest_bytes,0x41424344,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto& image=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        const auto uid=image.image_uid;
        const float stale_color[]{0.75f,0.75f,0.75f,0.75f};
        auto clear=RecordGuestStorageImageClear(device,image,renderer.RecordingList(),stale_color);
        image.NoteLogicalImageWrite();image.buffer_alias_version=image.buffer_alias_watch=true;
        image.SetGpuOwned(true);
        unsigned discarded{};std::shared_ptr<void> acquired_pin;
        const auto must_not_download=[](auto&){throw std::runtime_error("Invalid I reached a pressure download resolver");};
        cache.SetOwnershipResolver(must_not_download);cache.SetPressureRetirement(must_not_download);
        cache.SetIndependentVersionTransfer({}, {}, [&](auto& retiring){
            require(&retiring==&image&&retiring.image_uid==uid&&retiring.logical_dirty,
                "Pressure discarded the wrong or still-valid I");
            require(physical==original,"Pressure wrote stale I into physical B before ownership release");
            ++discarded;retiring.buffer_alias_version=retiring.buffer_alias_watch=false;
            retiring.linear_download_pending=false;retiring.SetGpuOwned(false);
            if(callback_pin)acquired_pin=cache.PinBinding(retiring);
        });
        require(!cache.ReclaimResidentMemory(0,0)&&discarded==0,
            "Pressure reclaimed a pending image");
        const auto fence=renderer.Flush();cache.Submitted(fence);
        require(!cache.ReclaimResidentMemory(0,fence-1)&&discarded==0,
            "Pressure reclaimed an in-flight image");
        renderer.Finish();cache.Collect(renderer.CompletedFence());clear.reset();
        {
            auto pin=cache.PinBinding(image);
            require(!cache.ReclaimResidentMemory(0,renderer.CompletedFence())&&discarded==0,
                "Pressure reclaimed a pinned image");
        }
        require(!cache.ReclaimResidentMemory(0,renderer.CompletedFence(),&image)&&discarded==0,
            "Pressure reclaimed the active allocation caller");
        if(tile!=8)require(!cache.ReclaimResidentMemory(0,renderer.CompletedFence())&&discarded==0,
            "Pressure downloaded a valid tiled I excluded by pinned GC");
        // Exact-base buffer invalidation retires logical I; it does not retile
        // these stale pixels into B or make their old GPU data a CPU winner.
        const auto cpu_page=(desc.address+4095)&~VAddr(4095);
        require(cpu_page>=desc.address&&cpu_page+4096<=desc.address+layout.guest_bytes,
            "Pressure CPU-store fixture lacks a complete physical page");
        reinterpret_cast<volatile std::byte*>(cpu_page)[17]=std::byte{0x31};
        renderer.NotifyCpuWrite(cpu_page+17,1); // an actual same-value store
        image.MarkLogicalDirty();image.linear_download_pending=true;
        if(callback_pin){
            bool rejected=false;
            try{cache.ReclaimResidentMemory(0,renderer.CompletedFence());}
            catch(const std::runtime_error&){rejected=true;}
            require(rejected&&acquired_pin&&cache.Entries().size()==1&&discarded==1,
                "Pressure erased an image acquired during its retirement callback");
            acquired_pin.reset();
        }
        require(cache.ReclaimResidentMemory(0,renderer.CompletedFence())&&cache.Entries().empty()&&
            cache.ResidentBytes()==0&&cache.PressureRetirementBytes()==0&&discarded==1,
            "Invalid independent I was not reclaimed without a download");
        require(physical==original,"Invalid independent pressure eviction changed CPU backing/padding");
        const auto current=renderer.PreparedDmaBuffer(desc.address,layout.guest_bytes);
        require(current&&current->resource.Get()==buffers.front().resource.Get()&&
            current->offset==buffers.front().offset,"Image eviction recreated resident B");
        for(const bool synchronize:{false,true}){
            if(synchronize)renderer.SynchronizePreparedDmaBuffers(std::span(&request,1),buffers);
            auto snapshot=renderer.CapturePreparedShaderBuffer(desc.address,layout.guest_bytes);
            auto readback=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,layout.guest_bytes);
            renderer.Finish();void* mapped{};D3D12_RANGE all{0,SIZE_T(layout.guest_bytes)};
            Check(readback.handle->Map(0,&all,&mapped));bool preserved=true;
            for(size_t offset=0;offset<layout.guest_bytes;offset+=4){u32 word{};
                memcpy(&word,static_cast<const std::byte*>(mapped)+offset,4);
                const auto address=desc.address+offset;
                const auto expected=synchronize&&address>=cpu_page&&address<cpu_page+4096?0x31313131U:0x41424344U;
                preserved&=word==expected;}
            D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
            require(preserved,"Pressure eviction changed GPU B/padding or erased an actual CPU-store marker");
        }
        renderer.ValidateDebugMessages();
    }
}
}
