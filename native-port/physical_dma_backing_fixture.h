// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_dma.h"
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "common/scope_exit.h"
#include <cstring>

namespace NativeDX12 {
inline void RunPhysicalDmaBackingFixture(ID3D12Device* device){
    namespace Dma=Shader::NativeDma;
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    constexpr VAddr base=0x1400000000ULL;constexpr u64 bytes=Dma::PageSize;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);
    require(section!=nullptr,"Cannot create physical DMA fixture backing");
    auto* guest=static_cast<std::byte*>(MapViewOfFileEx(section,FILE_MAP_ALL_ACCESS,0,0,bytes,reinterpret_cast<void*>(base)));
    auto* alias=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    require(guest&&alias,"Cannot map physical DMA fixture aliases");
    SCOPE_EXIT {if(guest)UnmapViewOfFile(guest);if(alias)UnmapViewOfFile(alias);CloseHandle(section);};
    struct PhysicalRasterizer:Rasterizer {
        std::byte* alias;bool image_owned{},reject_reads{};unsigned physical_reads{};
        PhysicalRasterizer(ID3D12Device* device,std::byte* p):Rasterizer(device),alias(p){}
        void CheckGpuBacking(VAddr address,u64 size,bool)override{
            if(address<base||address-base>bytes||size>bytes-(address-base))
                throw std::out_of_range("Physical DMA fixture extent");
        }
        void CheckBacking(VAddr address,u64 size,bool writing)override{
            if(image_owned)throw std::runtime_error("Physical DMA used a CPU path under independent I");
            Rasterizer::CheckBacking(address,size,writing);
        }
        void PrepareGpuBacking(VAddr,u64,bool)override{
            if(image_owned)throw std::runtime_error("Physical DMA tried to resolve independent I into B");
        }
        bool HasGpuImageOverlap(VAddr,u64)const override{return image_owned;}
        bool ReadDmaBacking(VAddr address,std::span<std::byte> out)override{
            ++physical_reads;if(reject_reads||address<base||address-base>bytes||out.size()>bytes-(address-base))return false;
            memcpy(out.data(),alias+address-base,out.size());return true;
        }
    };
    for(const bool partial:{false,true}){
        memset(alias,0x31,size_t(bytes));
        const auto start=base+Dma::SubpageSize;
        PhysicalRasterizer renderer(device,alias);
        renderer.MapMemory(partial?start:base,partial?Dma::SubpageSize:bytes);
        renderer.NotifyHostMapping(partial?start:base,partial?Dma::SubpageSize:bytes,true,true);
        Rasterizer::ShaderBufferRequest tiny{start+4,4,false,true,true};tiny.publish_dma_residency=false;
        const auto selected=renderer.PrepareShaderBuffers(std::span(&tiny,1));
        renderer.FillPreparedShaderBuffer(tiny.address,tiny.size,0x41424344,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        GuestTextureCache cache(device);SCOPE_EXIT {renderer.Finish();};
        GuestTextureDesc desc;desc.address=start;desc.width=desc.height=desc.pitch=4;
        desc.data_format=4;desc.number_format=4;desc.tile_mode=31;desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);std::vector<std::byte> seed(layout.guest_bytes,std::byte{0x77});
        auto& image=cache.Upload(desc,seed,renderer.RecordingList(),renderer.CompletedFence());
        const float color[]{std::bit_cast<float>(0x89abcdefu),0,0,0};
        auto clear=RecordGuestStorageImageClear(device,image,renderer.RecordingList(),color);
        image.NoteLogicalImageWrite();image.SetGpuOwned(true);const auto native=image.texture.handle;
        DWORD previous{};require(VirtualProtect(guest,bytes,PAGE_NOACCESS,&previous),"Cannot protect independent image backing");
        SCOPE_EXIT {DWORD ignored{};VirtualProtect(guest,bytes,PAGE_READWRITE,&ignored);};
        renderer.image_owned=true;
        auto& dma=renderer.DmaMemory();auto actual=tiny;actual.publish_dma_residency=true;
        dma.ObserveResidentSharps(std::span(&actual,1));
        require(!renderer.PreparedDmaBuffer(start,Dma::SubpageSize),"Tiny DMA control unexpectedly covered its page");
        dma.MaterializeResidentBacking(renderer);
        const auto current=renderer.RefreshPreparedShaderBuffers(std::span(&tiny,1),selected);
        require(current.front().resource.Get()!=selected.front().resource.Get(),"Physical DMA did not grow tiny B");
        require(bool(renderer.PreparedDmaBuffer(base,bytes))==!partial&&
            bool(renderer.PreparedDmaBuffer(start,Dma::SubpageSize)),"Physical DMA legal coverage mismatch");
        require(!partial||!renderer.PreparedDmaBuffer(start+Dma::SubpageSize,Dma::SubpageSize),
            "Physical DMA exposed a logically inaccessible subpage");
        auto snapshot=renderer.CapturePreparedShaderBuffer(start,Dma::SubpageSize);
        auto rb=Buffer(device,Dma::SubpageSize,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,snapshot.handle.Get(),0,Dma::SubpageSize);
        GuestImageReadback image_readback(device,image,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);image_readback.Submitted(fence);renderer.Finish();
        void* mapped{};D3D12_RANGE all{0,Dma::SubpageSize};Check(rb.handle->Map(0,&all,&mapped));
        bool valid=true;for(size_t offset=0;offset<Dma::SubpageSize;offset+=4){u32 word{};
            memcpy(&word,static_cast<const std::byte*>(mapped)+offset,4);valid&=word==(offset==4?0x41424344U:0x31313131U);}
        D3D12_RANGE empty{};rb.handle->Unmap(0,&empty);require(valid,"Physical DMA replaced B1 or initialized tails from I");
        image_readback.Resolve(renderer.CompletedFence(),seed);
        for(size_t offset=0;offset<layout.logical_bytes;offset+=4){u32 word{};memcpy(&word,seed.data()+offset,4);
            require(word==0x89abcdef,"Physical DMA changed independent GPU I contents");}
        MEMORY_BASIC_INFORMATION protection{};require(VirtualQuery(guest,&protection,sizeof(protection))&&
            protection.Protect==PAGE_NOACCESS&&image.gpu_owned&&image.texture.handle==native,
            "Physical DMA released the image's protection/ownership or native identity");
        for(size_t i=0;i<bytes;++i)require(alias[i]==std::byte{0x31},"Physical DMA committed I/B1 into CPU B0");
        const auto reads=renderer.physical_reads;renderer.reject_reads=true;dma.MaterializeResidentBacking(renderer);
        require(renderer.physical_reads==reads,"Already initialized DMA backing was redundantly read/materialized");
        {
            PhysicalRasterizer rejected(device,alias);rejected.MapMemory(start,Dma::SubpageSize);rejected.image_owned=true;rejected.reject_reads=true;
            Rasterizer::ShaderBufferRequest request{start,Dma::SubpageSize,false,true,true};request.publish_dma_residency=false;
            bool failed=false;try{rejected.PreparePhysicalDmaBuffers(std::span(&request,1));}catch(const std::runtime_error&){failed=true;}
            require(failed&&rejected.physical_reads>0&&!rejected.PreparedDmaBuffer(start,Dma::SubpageSize),
                "Rejected physical DMA reader fell back to image/CPU resolution or exposed failed contents");
        }
        renderer.ValidateDebugMessages();
    }
}
}
