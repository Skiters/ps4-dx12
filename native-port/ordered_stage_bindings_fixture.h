// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/scope_exit.h"
#include "video_core/renderer_d3d12/guest_stage_bindings.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include <cstring>
#include <source_location>
#include <string>

namespace NativeDX12 {
inline int RunOrderedInteriorFormattedReads(ID3D12Device* device,const ShaderPipeline* single_stage=nullptr){
    const auto require=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};
    for(const bool gpu_modified:{true,false})for(const UINT offset:{0u,1u,256u,257u}){
        if(!single_stage&&!gpu_modified)continue;
        printf("CASE: %s formatted sharp offset=%u GpuModified=%u\n",single_stage?"single":"ordered",offset,unsigned(gpu_modified));fflush(stdout);
        struct Allocation {void* p=VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            ~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} memory;
        require(memory.p,"Interior formatted fixture allocation");
        const auto base=reinterpret_cast<VAddr>(memory.p);memset(memory.p,0x4d,65536);
        Rasterizer renderer(device);GuestTextureCache cache(device);
        SCOPE_EXIT {renderer.Finish();};
        renderer.MapMemory(base,65536);
        renderer.SetBufferOwnership([](VAddr,u64){return true;},
            [](VAddr address,std::span<const std::byte> bytes){memcpy(reinterpret_cast<void*>(address),bytes.data(),bytes.size());return true;},
            [](VAddr,u64){});
        cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
        const auto acquire=[](auto& image){image.buffer_alias_version=image.buffer_alias_watch=true;image.SetGpuOwned(true);return true;};
        cache.SetOwnershipAcquirer(acquire);cache.SetIndependentVersionTransfer({},acquire,{});
        GuestTextureDesc desc;desc.address=base;desc.width=desc.height=desc.pitch=16;
        desc.data_format=4;desc.number_format=4;desc.tile_mode=13;desc.writable=gpu_modified;
        const auto layout=LayoutGuestTexture(desc);require(layout.guest_bytes>offset+16,"Fixture sharp outside image");
        std::vector<std::byte> image_bytes(layout.guest_bytes,std::byte{0x6e});
        auto& image=cache.Upload(desc,image_bytes,renderer.RecordingList(),renderer.CompletedFence());
        require(gpu_modified?cache.Own(image):cache.RevalidateReadonly(image),"Interior formatted fixture ownership");auto pin=cache.PinBinding(image);
        require(image.logical_gpu_modified==gpu_modified,"Readonly fixture invented GPU write history");
        const u64 sharp_bytes=single_stage?32768:16;
        const u8 adjust=offset&3;const u64 native_bytes=(sharp_bytes+adjust+3)&~u64(3);
        std::vector<Rasterizer::ShaderBufferRequest> requests{{base+(offset&~3u),native_bytes,false,true,true,adjust,sharp_bytes}};
        const OrderedGuestBuffer buffers[]{{true,true,false,adjust,1,sharp_bytes}};
        const GuestStageBindingSpan stages[]{{0,1,0,0}};
        bool rejected=false;
        try{
            if(single_stage){
                Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;
                info.flattened_ud_buf.resize(4);AmdGpu::Buffer sharp{};
                sharp.base_address=base+offset;sharp.stride=1;sharp.num_records=u32(sharp_bytes);
                memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
                Shader::BufferResource buffer;buffer.buffer_type=Shader::BufferType::Guest;
                buffer.is_formatted=true;buffer.used_types=Shader::IR::Type::U32;
                buffer.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
                info.buffers.push_back(buffer);
                // Optimized-away descriptors still run original BindBuffers
                // effects; the empty GPU dispatch must retain those effects.
                ExecuteGuestBufferCompute(device,renderer,*single_stage,info,{},ComputeInvocation{},&cache);
            }else (void)PrepareOrderedGuestStages(device,renderer,cache,stages,requests,buffers,{},true,
                [](VAddr,u64)->VAddr{throw std::runtime_error("Unexpected private CPU stream");});
        }
        catch(const std::runtime_error& e){
            if(std::string_view(e.what())!=(single_stage?
                "Independent formatted image/buffer partial or simultaneous-write ordering is not adapted":
                "Ordered formatted readers retain the original exact-base image contract"))throw;
            rejected=true;
        }
        require(rejected==(offset==1&&gpu_modified),"Formatted image address-bucket admission differs from original");
        if(rejected){renderer.Finish();continue;}
        // An interior single-stage sharp need not prepare the image's leading
        // bytes. Materialize that readback envelope through ordinary B only.
        auto snapshot=single_stage?renderer.CaptureShaderBuffer(base,layout.guest_bytes):
            renderer.CapturePreparedShaderBuffer(base,layout.guest_bytes);
        auto readback=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,layout.guest_bytes);
        GuestImageReadback image_readback(device,image,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);image_readback.Submitted(fence);renderer.Finish();
        std::vector<std::byte> expected(layout.guest_bytes,std::byte{0x4d});
        if(!offset&&gpu_modified){std::vector<std::byte> pixels(size_t(desc.width)*desc.height*4,std::byte{0x6e});
            EncodeGuestSubresource(desc,layout,expected,pixels,0,0);}
        void* mapped{};D3D12_RANGE all{0,SIZE_T(layout.guest_bytes)};Check(readback.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),expected.size());D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
        require(equal,"Interior formatted reader changed independent B or exact reader missed I");
        auto observed=image_bytes;image_readback.Resolve(renderer.CompletedFence(),observed);
        require(observed==image_bytes&&image.logical_gpu_modified==gpu_modified&&!image.logical_dirty&&image.gpu_owned,"Formatted reader changed independent I");
        require(std::ranges::all_of(std::span(static_cast<const std::byte*>(memory.p),65536),
            [](std::byte value){return value==std::byte{0x4d};}),"Formatted read reconciled physical CPU bytes");
        renderer.ValidateDebugMessages();
    }
    puts("PASS: ordered formatted exact/interior/unaligned address buckets preserve original independent B/I contents");return 0;
}
inline void RunOrderedWritableOwnershipAcquisition(ID3D12Device* device){
    struct Allocation {
        std::byte* bytes=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Allocation(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } memory;
    if(!memory.bytes)throw std::runtime_error("Ordered writable ownership allocation failed");
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    const auto base=reinterpret_cast<VAddr>(memory.bytes);memset(memory.bytes,0x4d,65536);
    Rasterizer rasterizer(device);GuestTextureCache textures(device);rasterizer.MapMemory(base,65536);
    rasterizer.SetBufferOwnership([](VAddr,u64){return true;},
        [](VAddr address,std::span<const std::byte> data){
            memcpy(reinterpret_cast<void*>(address),data.data(),data.size());return true;},[](VAddr,u64){});
    textures.SetBufferAliasRetention([](const auto&,const auto&){return true;});
    textures.SetIndependentImageRetention([](const auto&,const auto&){return true;});
    unsigned exclusive_acquires{},independent_acquires{};
    textures.SetOwnershipAcquirer([&](auto& image){
        ++exclusive_acquires;image.SetGpuOwned(true);return true;
    });
    textures.SetIndependentVersionTransfer({},[&](auto& image){
        require(!image.buffer_alias_version,
            "Ordered writable image published its independent class before acquiring page ownership");
        ++independent_acquires;image.buffer_alias_version=image.buffer_alias_watch=true;
        image.SetGpuOwned(true);return true;
    },[](auto& image){image.buffer_alias_version=image.buffer_alias_watch=false;image.SetGpuOwned(false);});

    AmdGpu::Image sharp{};sharp.base_address=base>>8;sharp.width=sharp.height=sharp.pitch=15;
    sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=4;sharp.num_format=4;
    sharp.tiling_index=8;sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    auto desc=DescribeGuestTexture(sharp);desc.writable=true;const auto layout=LayoutGuestTexture(desc);
    require(layout.guest_bytes<=65536,"Ordered writable ownership image exceeds fixture backing");
    std::vector<Rasterizer::ShaderBufferRequest> requests{{base,layout.guest_bytes,true,true,true}};
    const std::vector<OrderedGuestBuffer> buffers{{false,false,false,0,4,layout.guest_bytes}};
    const std::vector<OrderedGuestImage> images{{desc,layout,true,false}};
    const std::vector<GuestStageBindingSpan> stages{{0,0,0,1},{0,1,1,1}};
    auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
        [](VAddr,u64)->VAddr{throw std::runtime_error("Ordered writable ownership requested a CPU stream");});
    require(prepared.images[0]&&prepared.images[0]->gpu_owned&&prepared.images[0]->buffer_alias_version,
        "Ordered writable image did not publish independent ownership");
    require(independent_acquires==1&&!exclusive_acquires,
        "Ordered writable image bypassed independent ownership acquisition");
    GuestImageReadback readback(device,*prepared.images[0],rasterizer.RecordingList());
    const auto fence=rasterizer.Flush();textures.Submitted(fence);readback.Submitted(fence);rasterizer.Finish();
    std::vector<std::byte> observed(size_t(layout.guest_bytes));readback.Resolve(rasterizer.CompletedFence(),observed);
    require(std::ranges::all_of(observed,[](std::byte value){return value==std::byte{0x4d};}),
        "Ordered writable independent acquisition changed GPU image contents");
    rasterizer.ValidateDebugMessages();
}
inline void RunOrderedBcWordStages(ID3D12Device* device,bool prefix=false,bool formatted_first=false){
    struct Allocation {
        std::byte* bytes=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Allocation(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } memory;
    if(!memory.bytes)throw std::runtime_error("BC/word ordered-stage fixture allocation failed");
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
            throw std::runtime_error("BC/word ordered-stage source changed physical guest envelope");
        tiled.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return tiled;
    },acquire,[](auto& image){image.SetGpuOwned(false);image.buffer_alias_version=false;});
    textures.SetOwnershipResolver([](auto&){
        throw std::runtime_error("BC/word ordered-stage fixture unexpectedly downloaded I into B");});

    AmdGpu::Image word{};word.base_address=base>>8;word.type=u64(AmdGpu::ImageType::Color2DArray);
    word.data_format=14;word.num_format=4;
    word.width=word.height=prefix?23:11;word.pitch=prefix?31:15;
    word.last_level=prefix?2:0;word.base_level=prefix?2:0;
    word.depth=word.last_array=1;word.tiling_index=5;
    word.dst_sel_x=4;word.dst_sel_y=5;word.dst_sel_z=6;word.dst_sel_w=7;
    auto bc=word;bc.width=bc.height=prefix?95:47;bc.pitch=prefix?127:63;
    bc.last_level=prefix?3:0;bc.base_level=0;bc.data_format=37;bc.num_format=0;
    auto writable=DescribeGuestTexture(word);writable.writable=true;
    auto sampled=DescribeGuestTexture(bc);
    const auto writable_layout=LayoutGuestTexture(writable),sampled_layout=LayoutGuestTexture(sampled);
    require(sampled_layout.block_extent==4&&writable_layout.block_extent==1&&
        (prefix?(sampled.levels==4&&writable.levels==3&&writable.first_mip==2&&
                 sampled_layout.guest_bytes==36864&&writable_layout.guest_bytes<sampled_layout.guest_bytes):
                (sampled.levels==1&&writable.levels==1&&sampled_layout.guest_bytes==8192&&
                 writable_layout.guest_bytes==sampled_layout.guest_bytes)),
        "BC/word ordered-stage physical domain changed");
    require(CompatibleGuestBlockWordStorage(sampled,sampled_layout,writable,writable_layout),
        "Exact BC/word terminal prefix was not recognized");
    auto shifted=writable;shifted.address+=256;
    auto nonterminal=writable;nonterminal.first_mip=nonterminal.levels>1?nonterminal.levels-2:0;
    if(nonterminal.levels==1)++nonterminal.levels;
    require(!CompatibleGuestBlockWordStorage(sampled,sampled_layout,shifted,writable_layout)&&
        !CompatibleGuestBlockWordStorage(sampled,sampled_layout,nonterminal,LayoutGuestTexture(nonterminal)),
        "BC/word ordered-stage guard admitted shifted or nonterminal storage");
    std::vector<std::byte> initial(size_t(sampled_layout.guest_bytes),std::byte{0xa6});
    std::vector<std::byte> current(size_t(sampled_layout.guest_bytes),std::byte{0x5c});
    for(size_t offset=0;offset<initial.size();offset+=4){
        const u32 old_word=0x07e0f800u^u32(offset*0x1041u);
        const u32 new_word=0x13579bdfu^u32(offset*0x2081u);
        memcpy(initial.data()+offset,&old_word,4);memcpy(current.data()+offset,&new_word,4);
    }
    memcpy(memory.bytes,current.data(),current.size());
    auto& seed=textures.Upload(sampled,initial,rasterizer.RecordingList(),rasterizer.CompletedFence(),
        {},true,nullptr,true,true);
    require(textures.Own(seed),"BC/word ordered-stage seed could not retain I");
    std::vector<Rasterizer::ShaderBufferRequest> requests{{base,writable_layout.guest_bytes,true,true,true}};
    std::vector<OrderedGuestBuffer> buffers{{false,false,false,0,4,writable_layout.guest_bytes}};
    const std::vector<OrderedGuestImage> images{{sampled,sampled_layout,false,false},
        {writable,writable_layout,true,false}};
    std::vector<GuestStageBindingSpan> stages{{0,0,0,1},{0,1,1,1},{1,1,1,2}};
    if(formatted_first){
        requests.insert(requests.begin(),{base,sampled_layout.guest_bytes,false,true,true});
        buffers.insert(buffers.begin(),{true,true,false,0,4,sampled_layout.guest_bytes});
        stages={{0,1,0,1},{1,2,1,1},{2,2,1,2}};
    }
    auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
        [](VAddr,u64)->VAddr{throw std::runtime_error("BC/word ordered-stage fixture requested a CPU stream");});
    require(prepared.images[0]&&prepared.images[1]&&prepared.images[0]!=prepared.images[1],
        "BC/word ordered-stage views did not retain separate native allocations");
    require(prepared.images[0]->texture.handle->GetDesc().Format==DXGI_FORMAT_BC3_TYPELESS&&
        prepared.images[1]->texture.handle->GetDesc().Format==DXGI_FORMAT_R32G32B32A32_TYPELESS,
        "BC/word ordered-stage views changed native formats");
    GuestImageReadback early(device,*prepared.images[0],rasterizer.RecordingList());
    GuestImageReadback late(device,*prepared.images[1],rasterizer.RecordingList());
    const auto fence=rasterizer.Flush();textures.Submitted(fence);early.Submitted(fence);late.Submitted(fence);rasterizer.Finish();
    // Vulkan's ExpandImage copies the logical block image. Physical padding
    // remains in current guest B, so resolve both native representations over
    // that independent padding oracle.
    auto expected_early=current;
    for(UINT layer=0;layer<sampled.layers;++layer)for(UINT mip=0;mip<sampled.levels;++mip)
    for(UINT y=0;y<(std::max(sampled.height>>mip,1u)+3)/4;++y)
    for(UINT x=0;x<(std::max(sampled.width>>mip,1u)+3)/4;++x){
        const auto offset=GuestPixelOffset(sampled,sampled_layout,mip,layer,x,y);
        memcpy(expected_early.data()+offset,initial.data()+offset,sampled_layout.bytes_per_pixel);
    }
    auto observed_early=current;
    std::vector<std::byte> observed_late(current.begin(),current.begin()+size_t(writable_layout.guest_bytes));
    early.Resolve(rasterizer.CompletedFence(),observed_early);late.Resolve(rasterizer.CompletedFence(),observed_late);
    require(observed_early==expected_early,"Early BC sampled view did not preserve original logical I or current B padding");
    require(std::equal(observed_late.begin(),observed_late.end(),
        formatted_first?expected_early.begin():current.begin()),
        "Late word writable view did not use current B prefix and padding");
    rasterizer.ValidateDebugMessages();
}

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
inline int RunOrderedStageBindingsFixture(ID3D12Device* device,bool color_views=false){
    struct Allocation {
        std::byte* bytes=static_cast<std::byte*>(VirtualAlloc(nullptr,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        ~Allocation(){if(bytes)VirtualFree(bytes,0,MEM_RELEASE);}
    } memory,scratch;
    if(!memory.bytes||!scratch.bytes)throw std::runtime_error("Ordered stage fixture allocation failed");
    const auto base=reinterpret_cast<VAddr>(memory.bytes),private_base=reinterpret_cast<VAddr>(scratch.bytes);
    for(const bool interior:{false,true})for(const bool refresh:{false,true})for(const bool early_retile:{false,true})
    for(const u8 read_adjust:{u8(0),u8(1),u8(2)})for(const bool prefix_seed:{false,true}){
        if(color_views&&prefix_seed)continue; // Separate full-storage numeric-view case.
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
        auto desc=DescribeGuestTexture(sharp);desc.writable=!color_views;
        if(color_views){desc.render_target=true;desc.number_format=7;}
        const auto layout=LayoutGuestTexture(desc);
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
        auto sampled=desc;
        if(color_views){sampled.render_target=false;sampled.number_format=4;}
        const auto sampled_layout=LayoutGuestTexture(sampled);
        std::vector<OrderedGuestImage> images{{sampled,sampled_layout,!color_views,false}};
        if(refresh)images.push_back({sampled,sampled_layout,false,false});
        std::vector<GuestStageBindingSpan> stages{
            {0,1,0,0},{1,2,0,1},{2,3,1,refresh?2u:1u},
            {3,4,images.size(),images.size()},{4,5,images.size(),images.size()},
            {5,6,images.size(),images.size()}};
        auto prepared=PrepareOrderedGuestStages(device,rasterizer,textures,stages,requests,buffers,images,true,
            [&](VAddr address,u64 bytes){memcpy(scratch.bytes,reinterpret_cast<const void*>(address),size_t(bytes));return private_base;});
        require(prepared.stream_views[0].has_value()&&prepared.images[0]);
        require(prepared.images[0]->desc.levels==desc.levels);
        if(color_views)require(prepared.images[0]->Srv(sampled).Format==DXGI_FORMAT_R32_UINT);
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
    if(color_views){puts("PASS: ordered typed color views retain original I/B stage ordering and complete contents");return 0;}
    RunOrderedWritableOwnershipAcquisition(device);
    RunOrderedConvertedPackedStages(device);
    RunOrderedBcWordStages(device);
    RunOrderedBcWordStages(device,true);
    RunOrderedBcWordStages(device,false,true);
    RunOrderedBcWordStages(device,true,true);
    puts("PASS: ordered stage I/B refreshes preserve shared views, dirty state, streams and complete tiled padding");
    return 0;
}
}
