// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_stage_bindings.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include <cstring>
#include <source_location>
#include <string>

namespace NativeDX12 {
inline void RunOrderedConvertedPackedStages(ID3D12Device* device){
    struct Allocation {
        std::byte* bytes=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Allocation(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } memory;
    if(!memory.bytes)throw std::runtime_error("Converted ordered-stage fixture allocation failed");
    const auto base=reinterpret_cast<VAddr>(memory.bytes);
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    struct FixtureRasterizer final:Rasterizer {
        GuestTextureCache* cache{};
        using Rasterizer::Rasterizer;
        void NotifyGpuBufferWrite(VAddr address,u64 size)override{
            for(const auto& image:cache->Entries())if(image->desc.address==address&&size){
                image->MarkLogicalDirty();image->SetGpuOwned(false);
                image->buffer_alias_version=image->buffer_alias_watch=false;
            }
        }
    } rasterizer(device);
    GuestTextureCache textures(device);rasterizer.cache=&textures;rasterizer.MapMemory(base,65536);
    rasterizer.SetBufferOwnership([](VAddr,u64){return true;},
        [](VAddr address,std::span<const std::byte> data){
            memcpy(reinterpret_cast<void*>(address),data.data(),data.size());return true;},[](VAddr,u64){});
    textures.SetBufferAliasRetention([](const auto&,const auto&){return true;});
    textures.SetIndependentImageRetention([](const auto&,const auto&){return true;});
    const auto acquire=[](auto& image){
        image.buffer_alias_version=image.buffer_alias_watch=true;image.SetGpuOwned(true);return true;};
    textures.SetOwnershipAcquirer(acquire);
    textures.SetIndependentVersionTransfer([&](auto& image,VAddr address,u64 bytes){
        auto initial=rasterizer.CaptureShaderBuffer(image.desc.address,image.layout.guest_bytes);
        auto tiled=textures.RetileBufferVersion(image,initial,rasterizer.RecordingList(),rasterizer.CompletedFence());
        if(address!=image.desc.address||bytes!=image.layout.guest_bytes)
            throw std::runtime_error("Converted ordered-stage source changed physical guest envelope");
        tiled.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return tiled;
    },acquire,[](auto& image){image.SetGpuOwned(false);image.buffer_alias_version=false;});
    textures.SetOwnershipResolver([](auto&){
        throw std::runtime_error("Converted ordered-stage fixture unexpectedly downloaded I into B");});

    AmdGpu::Image sharp{};sharp.base_address=base>>8;sharp.width=sharp.height=sharp.pitch=31;
    sharp.depth=0;sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=9;sharp.num_format=1;
    sharp.tiling_index=13;sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    auto sampled=DescribeGuestTexture(sharp);const auto sampled_layout=LayoutGuestTexture(sampled);
    auto writable=sampled;writable.writable=true;const auto writable_layout=LayoutGuestTexture(writable);
    require(sampled_layout.guest_bytes==writable_layout.guest_bytes&&
        sampled_layout.expanded_bytes_per_pixel==16&&writable_layout.format==DXGI_FORMAT_R32_UINT&&
        CompatibleGuestPackedStorage(sampled,writable),"Converted ordered-stage packed domain changed");
    require(sampled_layout.guest_bytes<=65536,"Converted ordered-stage image exceeds fixture allocation");
    std::vector<std::byte> initial(size_t(sampled_layout.guest_bytes),std::byte{0xa6});
    std::vector<std::byte> current(size_t(sampled_layout.guest_bytes),std::byte{0x5c});
    for(UINT y=0;y<sampled.height;++y)for(UINT x=0;x<sampled.width;++x){
        const u32 old_word=0x40100200u^((y*sampled.width+x)*0x00104041u);
        const u32 new_word=0x80200400u^((y*sampled.width+x)*0x00081021u);
        const auto offset=GuestPixelOffset(sampled,sampled_layout,0,0,x,y);
        memcpy(initial.data()+offset,&old_word,4);memcpy(current.data()+offset,&new_word,4);
    }
    memcpy(memory.bytes,current.data(),current.size());
    auto& seed=textures.Upload(sampled,initial,rasterizer.RecordingList(),rasterizer.CompletedFence(),
        {},true,nullptr,true,true);
    require(textures.Own(seed),"Converted ordered-stage seed could not retain I");
    std::vector<Rasterizer::ShaderBufferRequest> requests{{base,sampled_layout.guest_bytes,true,true,true}};
    const std::vector<OrderedGuestBuffer> buffers{{false,false,false,0,4,sampled_layout.guest_bytes}};
    const std::vector<OrderedGuestImage> images{{sampled,sampled_layout,false,false},
        {writable,writable_layout,true,false}};
    const std::vector<GuestStageBindingSpan> stages{{0,0,0,1},{0,1,1,1},{1,1,1,2}};
    auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
        [](VAddr,u64)->VAddr{throw std::runtime_error("Converted ordered-stage fixture requested a CPU stream");});
    require(prepared.images[0]&&prepared.images[1]&&prepared.images[0]!=prepared.images[1],
        "Converted ordered-stage views did not retain separate native allocations");
    require(prepared.images[0]->texture.handle->GetDesc().Format!=prepared.images[1]->texture.handle->GetDesc().Format,
        "Converted ordered-stage views unexpectedly use one native format");
    GuestImageReadback early(device,*prepared.images[0],rasterizer.RecordingList());
    GuestImageReadback late(device,*prepared.images[1],rasterizer.RecordingList());
    const auto fence=rasterizer.Flush();textures.Submitted(fence);early.Submitted(fence);late.Submitted(fence);rasterizer.Finish();
    std::vector<std::byte> observed_early(initial.size()),observed_late(current.size());
    early.Resolve(rasterizer.CompletedFence(),observed_early);late.Resolve(rasterizer.CompletedFence(),observed_late);
    require(observed_early==initial,"Early converted sampled view did not preserve original I and padding");
    require(observed_late==current,"Late converted writable view did not use current B and padding");
    rasterizer.ValidateDebugMessages();
}

// GPU transfer/ordering controls. The cache notification adapter
// supplies the original exact-base dirty event; this is not a PageWatch or
// whole-graphics shader execution/acceptance test.
inline int RunOrderedStageBindingsFixture(ID3D12Device* device){
    struct Allocation {
        std::byte* bytes=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Allocation(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } memory,scratch;
    if(!memory.bytes||!scratch.bytes)throw std::runtime_error("Ordered stage fixture allocation failed");
    const auto base=reinterpret_cast<VAddr>(memory.bytes),private_base=reinterpret_cast<VAddr>(scratch.bytes);
    for(const bool interior:{false,true})for(const bool refresh:{false,true})for(const bool early_retile:{false,true})
    for(const u8 read_adjust:{u8(0),u8(1),u8(2)})for(const bool prefix_seed:{false,true}){
        const auto require=[&](bool value,const std::source_location where=std::source_location::current()){
            if(!value)throw std::runtime_error("Ordered stage cache/physical oracle mismatch at line "+
                std::to_string(where.line())+" interior="+std::to_string(interior)+" refresh="+std::to_string(refresh)+
                " early="+std::to_string(early_retile)+" adjust="+std::to_string(read_adjust)+" prefix="+std::to_string(prefix_seed));
        };
        struct FixtureRasterizer final:Rasterizer {
            GuestTextureCache* cache{};
            using Rasterizer::Rasterizer;
            void NotifyGpuBufferWrite(VAddr address,u64 size)override{
                for(const auto& image:cache->Entries())if(image->desc.address==address&&size){
                    image->MarkLogicalDirty();image->SetGpuOwned(false);
                    image->buffer_alias_version=image->buffer_alias_watch=false;
                }
            }
        } rasterizer(device);
        GuestTextureCache textures(device);rasterizer.cache=&textures;
        rasterizer.MapMemory(base,65536);rasterizer.MapMemory(private_base,65536);
        rasterizer.SetBufferOwnership([](VAddr,u64){return true;},
            [](VAddr address,std::span<const std::byte> data){memcpy(reinterpret_cast<void*>(address),data.data(),data.size());return true;},
            [](VAddr,u64){});
        textures.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        textures.SetIndependentImageRetention([](const auto&,const auto&){return true;});
        const auto acquire=[](auto& image){image.buffer_alias_version=image.buffer_alias_watch=true;image.SetGpuOwned(true);return true;};
        textures.SetOwnershipAcquirer(acquire);
        textures.SetIndependentVersionTransfer([&](auto& image,VAddr address,u64 bytes){
            auto initial=rasterizer.CaptureShaderBuffer(image.desc.address,image.layout.guest_bytes);
            auto tiled=textures.RetileBufferVersion(image,initial,rasterizer.RecordingList(),rasterizer.CompletedFence());
            if(bytes!=image.layout.guest_bytes){
                require(address==image.desc.address&&bytes>image.layout.guest_bytes);
                auto grown=rasterizer.CaptureShaderBuffer(address,bytes);
                tiled.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                grown.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
                DX12_API_CALL(rasterizer.RecordingList()->CopyBufferRegion(grown.handle.Get(),0,tiled.handle.Get(),0,image.layout.guest_bytes));
                tiled=std::move(grown);
            }
            tiled.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return tiled;
        },acquire,[](auto& image){image.SetGpuOwned(false);image.buffer_alias_version=false;});
        textures.SetOwnershipResolver([](auto&){throw std::runtime_error("Ordered fixture unexpectedly downloaded I into B");});
        AmdGpu::Image sharp{};sharp.base_address=base>>8;sharp.width=sharp.height=sharp.pitch=31;
        sharp.depth=1;sharp.type=u64(AmdGpu::ImageType::Color2DArray);sharp.last_array=1;
        sharp.last_level=1;sharp.data_format=4;sharp.num_format=4;sharp.tiling_index=13;
        auto desc=DescribeGuestTexture(sharp);desc.writable=true;const auto layout=LayoutGuestTexture(desc);
        auto seed_desc=desc;if(prefix_seed)seed_desc.levels=1;
        const auto seed_layout=LayoutGuestTexture(seed_desc);
        require(layout.guest_bytes<=65536);
        memset(memory.bytes,0xb4,65536);memset(scratch.bytes,0,65536);
        std::vector<std::byte> image_bytes(size_t(layout.guest_bytes),std::byte{0xe7});
        std::vector<std::byte> retiled(65536,std::byte{0xb4});
        for(UINT layer=0;layer<desc.layers;++layer)for(UINT mip=0;mip<seed_desc.levels;++mip)
            for(UINT y=0;y<std::max(desc.height>>mip,1u);++y)for(UINT x=0;x<std::max(desc.width>>mip,1u);++x){
                const u32 value=0x13570000u^(layer<<12)^(mip<<10)^(y<<5)^x;
                const auto offset=GuestPixelOffset(desc,layout,mip,layer,x,y);
                memcpy(image_bytes.data()+offset,&value,4);memcpy(retiled.data()+offset,&value,4);
            }
        auto& seed=textures.Upload(seed_desc,std::span(image_bytes).first(size_t(seed_layout.guest_bytes)),
            rasterizer.RecordingList(),rasterizer.CompletedFence(),{},true,nullptr,true,true);
        require(textures.Own(seed));
        const auto seed_pin=textures.PinBinding(seed);
        // Six original stages: private early stream, optional retile + image,
        // exact/interior buffer writer + optional image refresh, later retile,
        // another exact invalidation, and a final resident read.
        std::vector<Rasterizer::ShaderBufferRequest> requests{
            {base,64,false,true,true,read_adjust,u64(64-read_adjust)},{base,65536,false,true,true},
            {base+(interior?256:0),4,true,true,true},{base,65536,false,true,true},
            {base,4,true,true,true},{base,65536,false,true,true}};
        std::vector<OrderedGuestBuffer> buffers{
            {true,true,true,read_adjust,1,u64(64-read_adjust)},{true,early_retile,false,0,4,65536},
            {false,false,false,0,4,4},{true,true,false,0,4,65536},
            {false,false,false,0,4,4},{true,true,false,0,4,65536}};
        std::vector<OrderedGuestImage> images{{desc,layout,true,false}};
        if(refresh)images.push_back({desc,layout,false,false});
        std::vector<GuestStageBindingSpan> stages{
            {0,1,0,0},{1,2,0,1},{2,3,1,refresh?2u:1u},
            {3,4,images.size(),images.size()},{4,5,images.size(),images.size()},
            {5,6,images.size(),images.size()}};
        auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
            [&](VAddr address,u64 bytes){memcpy(scratch.bytes,reinterpret_cast<const void*>(address),size_t(bytes));return private_base;});
        require(prepared.stream_views[0].has_value()&&prepared.images[0]);
        require(prepared.images[0]->desc.levels==desc.levels);
        if(refresh)require(prepared.images[0]==prepared.images[1]);
        rasterizer.SynchronizePreparedDmaBuffers(requests,prepared.buffers);
        // The last exact-base buffer bind leaves I dirty. Final draw preparation
        // must not revive it or download its independent pixels over B.
        require(prepared.images[0]->logical_dirty&&!prepared.images[0]->gpu_owned);
        auto snapshot=rasterizer.CapturePreparedShaderBuffer(base,65536);
        auto readback=Buffer(device,65536,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        DX12_API_CALL(rasterizer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,65536));
        GuestImageReadback image_readback(device,*prepared.images[0],rasterizer.RecordingList());
        const auto fence=rasterizer.Flush();textures.Submitted(fence);image_readback.Submitted(fence);rasterizer.Finish();
        const bool copied_i=early_retile||interior;
        const std::vector<std::byte> original(65536,std::byte{0xb4});
        const auto& expected_b=copied_i?retiled:original;
        void* mapped{};D3D12_RANGE range{0,65536};Check(readback.handle->Map(0,&range,&mapped));
        require(!memcmp(mapped,expected_b.data(),65536));D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
        std::vector<std::byte> observed_i(size_t(layout.guest_bytes),std::byte{0xb4});
        image_readback.Resolve(rasterizer.CompletedFence(),observed_i);
        const auto& expected_i=refresh&&!interior&&!early_retile?original:retiled;
        if(memcmp(observed_i.data(),expected_i.data(),observed_i.size()))for(size_t at=0;at<observed_i.size();++at)
            if(observed_i[at]!=expected_i[at]){u32 got{},expected{};const auto word=at&~size_t(3);
                memcpy(&got,observed_i.data()+word,4);memcpy(&expected,expected_i.data()+word,4);
                fprintf(stderr,"Ordered I mismatch byte=%zu got=%08x expected=%08x seed_bytes=%llu target_bytes=%llu\n",
                    at,got,expected,(unsigned long long)seed_layout.guest_bytes,(unsigned long long)layout.guest_bytes);break;}
        require(!memcmp(observed_i.data(),expected_i.data(),observed_i.size()));
        require(!memcmp(scratch.bytes,original.data(),64));
        rasterizer.ValidateDebugMessages();
    }
    RunOrderedConvertedPackedStages(device);
    puts("PASS: ordered stage I/B refreshes preserve shared views, dirty state, streams and complete tiled padding");
    return 0;
}
}
