// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/memory_surface_provider.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include "common/scope_exit.h"
#include "invalid_image_pressure_fixture.h"
#include <cstdio>
#include <source_location>
#include <thread>
using namespace NativeDX12;
static void Require(bool value,std::source_location location=std::source_location::current()){if(!value)throw std::runtime_error("GPU page ownership assertion failed at line "+std::to_string(location.line()));}
static std::atomic<uint64_t> delayed_address{};
static HANDLE fault_entered{},deliver_fault{};
static LONG CALLBACK DelayWriteFault(EXCEPTION_POINTERS* context){
    const auto& record=*context->ExceptionRecord;
    if(record.ExceptionCode==EXCEPTION_ACCESS_VIOLATION&&record.NumberParameters>=2&&
       record.ExceptionInformation[0]==1&&record.ExceptionInformation[1]==delayed_address.load()){
        SetEvent(fault_entered);WaitForSingleObject(deliver_fault,5000);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
struct OwnershipRenderer:Rasterizer {
    using Rasterizer::Rasterizer;
    std::function<void(VAddr,u64)> resolve;
    std::function<Resource*(VAddr,UINT,UINT,UINT)> coherent;
    Resource* CoherentDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch) override {
        Finish();return coherent?coherent(address,width,height,pitch):nullptr;
    }
    bool ReadMemory(VAddr address,u64 size,bool assume=false) override {
        std::lock_guard lock(ExecutionMutex());if(resolve)resolve(address,size);
        return Rasterizer::ReadMemory(address,size,assume);
    }
    void CheckBacking(VAddr address,u64 size,bool writing) override {
        ResolveGpuBuffers(address,size);if(writing)ReleaseCpuBufferWatches(address,size);
        Rasterizer::CheckBacking(address,size,writing);
    }
};
int main(int argc,char**){try{
    const UINT width=argc>1?640:512,height=argc>1?360:512;
    GuestTextureDesc desc;desc.width=desc.pitch=width;desc.height=height;desc.tile_mode=10;desc.render_target=true;
    auto layout=LayoutGuestTexture(desc);const size_t bytes=layout.guest_bytes;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);Require(section!=nullptr);
    auto* guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    auto* alias=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));Require(guest&&alias);
    auto device=CreateHardwareDevice();RunInvalidImagePressureFixture(device.Get());
    OwnershipRenderer renderer(device.Get());GuestTextureCache cache(device.Get());
    cache.SetCopySubmission([&](const auto& record){renderer.SubmitCopyCommands(record);});
    const auto address=reinterpret_cast<VAddr>(guest);renderer.MapMemory(address,bytes);renderer.NotifyHostMapping(address,bytes,true,true);
    desc.address=address;
    auto& image=cache.Upload(desc,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
    unsigned resolutions{},page_faults{};PageWatch* watcher{};
    {
        const auto resolve=[&](uint64_t){std::lock_guard lock(renderer.ExecutionMutex());if(!image.gpu_owned)return;
            renderer.Finish();std::vector<std::byte> result(alias,alias+bytes);
            GuestImageReadback readback(device.Get(),image,renderer.RecordingList());const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
            readback.Resolve(renderer.CompletedFence(),result);memcpy(alias,result.data(),bytes);
            image.RememberSnapshot(result);image.SetGpuOwned(false);++resolutions;watcher->Untrack(address,bytes);
            Require(!cache.HasGuestRangeOverlap(address,bytes,true));
        };PageWatch watch([&](uint64_t address){++page_faults;resolve(address);});watcher=&watch;
        cache.SetOwnershipResolver([&](auto&){resolve(address);renderer.RecordingList();});
        renderer.resolve=[&](VAddr address,u64){resolve(address);};
        Require(watch.Track(address,bytes));image.SetGpuOwned(true);
        Descriptors rtvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);
        auto view=image.Rtv();DX12_API_CALL(device->CreateRenderTargetView(image.texture.handle.Get(),&view,rtvs.Cpu(0)));
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float color[]{1.f,1.f,0.f,1.f};DX12_API_CALL(renderer.RecordingList()->ClearRenderTargetView(rtvs.Cpu(0),color,0,nullptr));
        auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        Require(cache.HasGuestRangeOverlap(address,bytes,true));
        Require(alias[0]==std::byte{0}&&image.gpu_owned&&resolutions==0);
        auto sampled=desc;sampled.render_target=false;
        auto& reused=cache.Upload(sampled,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());Require(&reused==&image);
        fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        Require(cache.CoherentDisplayTexture(address,width,height,width,renderer.CompletedFence())==&image.texture);
        // The first genuine CPU read must see the GPU result, with no eager
        // CPU copy during either rendering or sampling/presentation reuse.
        auto* cpu=reinterpret_cast<volatile unsigned char*>(guest);Require(cpu[0]==255);
        Require(resolutions==1&&!image.gpu_owned&&cpu[1]==255&&cpu[2]==0&&cpu[3]==255);
        const auto corner=GuestPixelOffset(desc,layout,0,0,width-1,height-1);Require(cpu[corner]==255&&cpu[corner+1]==255);
        // BGRA display swaps channels on the GPU, retaining page ownership.
        // No CPU resolve/detile/upload is needed just to present a frame.
        memset(alias,0,bytes);Require(watch.Track(address,bytes));image.SetGpuOwned(true);
        Libraries::VideoOut::BufferAttributeGroup group{};
        group.attrib.width=group.attrib.pitch_in_pixel=width;group.attrib.height=height;
        group.attrib.tiling_mode=Libraries::VideoOut::TilingMode::Tile;
        group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A8R8G8B8Srgb;
        D3D12_COMMAND_QUEUE_DESC queue_desc{};ComPtr<ID3D12CommandQueue> display_queue;
        Check(DX12_API_CALL(device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&display_queue))));
        renderer.coherent=[&](VAddr a,UINT w,UINT h,UINT p){return cache.CoherentDisplayTexture(a,w,h,p,renderer.CompletedFence());};
        MemorySurfaceProvider provider(device.Get(),display_queue.Get(),renderer,width,height);
        auto snapshot=provider.Snapshot(group,address);
        Require(resolutions==1&&image.gpu_owned);
        auto display=TextureTransfer::Create(device.Get(),*snapshot.texture,true);
        display.Readback(renderer.RecordingList(),*snapshot.texture);renderer.Finish();
        std::vector<std::byte> pixels(size_t(width)*height*4);display.ReadRows(pixels.data(),pixels.size(),width*4);
        Require(snapshot.component_mapping==D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
            2,1,0,D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1));
        for(size_t pixel=0;pixel<pixels.size();pixel+=4)
            Require(pixels[pixel]==std::byte{255}&&pixels[pixel+1]==std::byte{255}&&pixels[pixel+2]==std::byte{0}&&pixels[pixel+3]==std::byte{255});
        renderer.resolve={};
        // A compute UAV aliases the protected color target but cannot reuse
        // its render-only resource. Exercise the common Upload fallback with
        // the renderer lock held, exactly as ExecuteGuestResources does.
        memset(alias,0,bytes);Require(watch.Track(address,bytes));image.SetGpuOwned(true);
        const auto previous_faults=page_faults;
        auto writable=sampled;writable.writable=true;
        {
            std::lock_guard lock(renderer.ExecutionMutex());
            auto& compute=cache.Upload(writable,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
            Require(resolutions==2&&!image.gpu_owned&&page_faults==previous_faults);
            GuestImageReadback copied(device.Get(),compute,renderer.RecordingList());
            fence=renderer.Flush();cache.Submitted(fence);copied.Submitted(fence);renderer.Finish();
            std::vector<std::byte> copied_bytes(bytes);copied.Resolve(renderer.CompletedFence(),copied_bytes);
            Require(copied_bytes==std::vector<std::byte>(alias,alias+bytes));
        }
        guest[0]=std::byte{0x33};cache.Upload(desc,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
        GuestImageReadback changed(device.Get(),image,renderer.RecordingList());fence=renderer.Flush();cache.Submitted(fence);changed.Submitted(fence);renderer.Finish();
        std::vector<std::byte> result(alias,alias+bytes);changed.Resolve(renderer.CompletedFence(),result);Require(result[0]==std::byte{0x33}&&result[1]==std::byte{255});
    }
    {
        unsigned writes{};PageWatch* watcher{};
        const auto release=[&](auto& entry){if(entry.cpu_watched){entry.cpu_watched=false;watcher->Untrack(entry.desc.address,entry.layout.guest_bytes);}};
        PageWatch immutable_watch([&](uint64_t fault){++writes;
            for(auto& entry:cache.Entries())if(entry->cpu_watched&&fault>=entry->desc.address&&fault-entry->desc.address<entry->layout.guest_bytes)release(*entry);
        });watcher=&immutable_watch;
        cache.SetImmutableWatch([&](auto& entry){entry.cpu_watched=watcher->Track(entry.desc.address,entry.layout.guest_bytes,true);},release);
        auto sampled=desc;sampled.render_target=false;sampled.number_format=9;
        auto& immutable=cache.Upload(sampled,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
        Require(immutable.cpu_watched);
        auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        volatile auto* cpu=reinterpret_cast<volatile unsigned char*>(guest);
        Require(cpu[0]==0x33&&writes==0);
        auto& reused=cache.Upload(sampled,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
        Require(&reused==&immutable&&immutable.uploads.empty()&&writes==0);
        fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        cpu[0]=0x77;Require(writes==1&&!immutable.cpu_watched);
        cache.Upload(sampled,{guest,bytes},renderer.RecordingList(),renderer.CompletedFence());
        Require(immutable.cpu_watched);
        GuestImageReadback rb(device.Get(),immutable,renderer.RecordingList());
        fence=renderer.Flush();cache.Submitted(fence);rb.Submitted(fence);renderer.Finish();
        std::vector<std::byte> result(alias,alias+bytes);rb.Resolve(renderer.CompletedFence(),result);
        Require(result[0]==std::byte{0x77});
        cache.Invalidate(address,bytes,renderer.CompletedFence());Require(!watcher->Contains(address));
        cpu[0]=0x88;Require(writes==1);
        cache.SetImmutableWatch({},{});
    }
    {
        GuestTextureDesc copy_desc;copy_desc.width=copy_desc.pitch=256;copy_desc.height=256;
        copy_desc.tile_mode=10;copy_desc.render_target=true;
        const auto copy_layout=LayoutGuestTexture(copy_desc);
        std::vector<std::byte> first(copy_layout.guest_bytes),second(copy_layout.guest_bytes);
        copy_desc.address=reinterpret_cast<uint64_t>(first.data());
        auto& src=cache.Upload(copy_desc,first,renderer.RecordingList(),renderer.CompletedFence());
        copy_desc.address=reinterpret_cast<uint64_t>(second.data());
        auto& dst=cache.Upload(copy_desc,second,renderer.RecordingList(),renderer.CompletedFence());
        Require(GuestImageCopyCompatible(src,dst)&&GuestImageClearCompatible(src));
        const auto saved=dst.desc.tile_mode;dst.desc.tile_mode=13;
        Require(!GuestImageCopyCompatible(src,dst));dst.desc.tile_mode=saved;
        const auto saved_bytes=dst.layout.logical_bytes;--dst.layout.logical_bytes;
        // A whole-allocation copy needs matching padding coverage. Image
        // clears affect texels independently of physical guest padding.
        Require(!GuestImageCopyCompatible(src,dst)&&GuestImageClearCompatible(dst));dst.layout.logical_bytes=saved_bytes;
        const float yellow[]{1,1,0,1},green[]{0,1,0,1};
        auto src_heap=RecordGuestImageClear(device.Get(),src,renderer.RecordingList(),yellow);
        auto dst_heap=RecordGuestImageClear(device.Get(),dst,renderer.RecordingList(),green);
        RecordGuestImageCopy(src,dst,renderer.RecordingList());
        GuestImageReadback rb(device.Get(),dst,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);rb.Submitted(fence);renderer.Finish();
        rb.Resolve(renderer.CompletedFence(),second);
        for(size_t i=0;i<second.size();i+=4)Require(second[i]==std::byte{255}&&
            second[i+1]==std::byte{255}&&second[i+2]==std::byte{0}&&second[i+3]==std::byte{255});
        cache.Invalidate(src.desc.address,src.layout.guest_bytes,renderer.CompletedFence());
        cache.Invalidate(dst.desc.address,dst.layout.guest_bytes,renderer.CompletedFence());
    }
    {
        GuestTextureDesc padded;padded.width=17;padded.pitch=32;padded.height=9;
        padded.layers=3;padded.view_layers=1;padded.first_layer=1;
        padded.levels=2;padded.first_mip=1;padded.tile_mode=13;padded.render_target=true;
        const auto layout=LayoutGuestTexture(padded);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
        padded.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(padded,guest,renderer.RecordingList(),renderer.CompletedFence());
        Require(layout.logical_bytes<layout.guest_bytes&&GuestImageClearCompatible(image));
        const float yellow[]{1,1,0,1};const u32 rgba=0xff00ffff;
        auto heap=RecordGuestImageClear(device.Get(),image,renderer.RecordingList(),yellow);
        GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
        readback.Resolve(renderer.CompletedFence(),guest);
        for(u32 mip=0;mip<padded.levels;++mip)for(u32 layer=0;layer<padded.layers;++layer)
        for(u32 y=0;y<std::max(padded.height>>mip,1u);++y)
        for(u32 x=0;x<std::max(padded.width>>mip,1u);++x)
            memcpy(expected.data()+GuestPixelOffset(padded,layout,mip,layer,x,y),&rgba,4);
        Require(guest==expected);
        cache.Invalidate(padded.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 data:{4u,10u,14u})for(const u32 tile:{8u,13u})
    for(const auto swizzle:{std::array<u32,4>{5,4,7,6},std::array<u32,4>{0,4,1,7}}){
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=9;d.layers=3;d.first_layer=1;d.view_layers=1;
        d.levels=2;d.first_mip=1;d.data_format=data;d.number_format=data==10?0:7;
        d.tile_mode=tile;d.render_target=true;d.swizzle=swizzle;
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
        d.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
        Require(GuestImageClearCompatible(image));const auto original=image.texture.handle;
        const float color[]{0.25f,0.5f,-0.5f,1.f};
        auto heap=RecordGuestImageClear(device.Get(),image,renderer.RecordingList(),color);
        GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
        const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
        readback.Resolve(renderer.CompletedFence(),guest);
        const std::array<u32,4> words{0x3e800000u,0x3f000000u,0xbf000000u,0x3f800000u};
        const u32 rgba=0xff008040u;
        for(u32 mip=0;mip<d.levels;++mip)for(u32 layer=0;layer<d.layers;++layer)
        for(u32 y=0;y<std::max(d.height>>mip,1u);++y)for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
            memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y),
                   data==10?static_cast<const void*>(&rgba):words.data(),data==14?16:4);
        Require(guest==expected&&image.texture.handle==original&&image.desc==d);
        cache.Collect(renderer.CompletedFence());cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 samples:{2u,4u,8u})for(const u32 tile:{8u,13u})for(const u32 usage:{0u,1u,2u}){
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=9;d.layers=3;d.first_layer=1;d.view_layers=1;
        d.data_format=4;d.number_format=7;d.samples=samples;d.tile_mode=tile;
        d.writable=usage==1;d.render_target=usage==2;
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
        d.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
        Require(GuestImageClearCompatible(image)&&image.texture.handle->GetDesc().SampleDesc.Count==samples);
        const auto original=image.texture.handle;
        for(const u32 word:{0x3e800000u,0x80000000u}){
            const float color[]{std::bit_cast<float>(word),0.f,0.f,1.f};
            auto heap=RecordGuestImageClear(device.Get(),image,renderer.RecordingList(),color);
            GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
            const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
            readback.Resolve(renderer.CompletedFence(),guest);
            for(u32 layer=0;layer<d.layers;++layer)for(u32 sample=0;sample<samples;++sample)
            for(u32 y=0;y<d.height;++y)for(u32 x=0;x<d.width;++x)
                memcpy(expected.data()+GuestPixelOffset(d,layout,0,layer,x,y,sample),&word,4);
            Require(guest==expected&&image.texture.handle==original&&image.desc==d);
        }
        cache.Collect(renderer.CompletedFence());cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 samples:{2u,4u,8u})for(const u32 tile:{8u,13u})
    for(const u32 number:{4u,5u,7u})for(const bool raw:{false,true}){
        if(raw&&number!=7)continue;
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=9;d.layers=3;d.first_layer=1;d.view_layers=1;
        d.data_format=4;d.number_format=number;d.samples=samples;d.tile_mode=tile;d.writable=true;d.native_raw32=raw;
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
        d.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
        const auto original=image.texture.handle;
        for(const u32 word:{0x3e800000u,0x80000000u}){
            const float color[]{std::bit_cast<float>(word),0.f,0.f,1.f};
            Require(GuestMultisampleStorageClearCompatible(image,color));
            auto clear=RecordGuestMultisampleStorageClear(device.Get(),cache,image,renderer.RecordingList(),color);
            GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
            const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
            readback.Resolve(renderer.CompletedFence(),guest);
            for(u32 layer=0;layer<d.layers;++layer)for(u32 sample=0;sample<samples;++sample)
            for(u32 y=0;y<d.height;++y)for(u32 x=0;x<d.width;++x)
                memcpy(expected.data()+GuestPixelOffset(d,layout,0,layer,x,y,sample),&word,4);
            Require(guest==expected&&image.texture.handle==original&&image.desc==d);
            cache.Collect(renderer.CompletedFence());Require(image.storage_clear_leases.empty());
        }
        cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 data:{4u,11u,13u,14u})for(const u32 number:{4u,5u,7u})
    for(const bool readonly:{false,true})for(const u32 shape:{0u,1u,2u}){
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=shape==1?1:9;
        d.data_format=data;d.number_format=number;d.writable=!readonly;d.native_raw32=data==4;
        d.one_dimensional=shape==1;d.volume_depth=shape==2?4:0;
        d.layers=shape==2?1:3;d.first_layer=shape==2?0:1;d.view_layers=1;
        d.levels=3;d.first_mip=1;d.tile_mode=13;
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
        d.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
        const float color[]{1.5f,-0.f,-3.25f,0.25f};
        const auto original=image.texture.handle;const auto allocation=image.allocation_bytes;
        std::shared_ptr<GuestStorageImageClearLease> lease;
        if(readonly){Require(GuestConvertedStorageClearCompatible(image)&&!GuestStorageImageClearCompatible(image,color));
            cache.ClearRawStorageImage(image,color,renderer.RecordingList(),renderer.CompletedFence());}
        else{Require(GuestStorageImageClearCompatible(image,color));
            lease=RecordGuestStorageImageClear(device.Get(),image,renderer.RecordingList(),color);}
        GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
        Resource host_rgb;D3D12_PLACED_SUBRESOURCE_FOOTPRINT rgb_fp{};
        if(data==13){const auto texture=image.texture.handle->GetDesc();UINT64 total{};
            const auto sub=d.first_mip+d.first_layer*d.levels;
            device->GetCopyableFootprints(&texture,sub,1,0,&rgb_fp,nullptr,nullptr,&total);
            host_rgb=Buffer(device.Get(),total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=image.texture.handle.Get();
            from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=sub;
            to.pResource=host_rgb.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=rgb_fp;
            renderer.RecordingList()->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        }
        const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
        readback.Resolve(renderer.CompletedFence(),guest);
        const u32 bits[]{std::bit_cast<u32>(color[0]),std::bit_cast<u32>(color[1]),
            std::bit_cast<u32>(color[2]),std::bit_cast<u32>(color[3])};
        const u32 components=data==4?1:data==11?2:data==13?3:4;
        for(u32 mip=0;mip<d.levels;++mip)
        for(u32 layer=0;layer<(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers);++layer)
        for(u32 y=0;y<std::max(d.height>>mip,1u);++y)
        for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
            memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y),bits,components*4);
        Require(guest==expected);
        if(data==13){void* mapped{};D3D12_RANGE all{0,SIZE_T(host_rgb.handle->GetDesc().Width)};
            Check(host_rgb.handle->Map(0,&all,&mapped));bool exact=true;
            for(UINT z=0;z<rgb_fp.Footprint.Depth;++z)for(UINT y=0;y<rgb_fp.Footprint.Height;++y)
            for(UINT x=0;x<rgb_fp.Footprint.Width;++x){u32 alpha;
                memcpy(&alpha,static_cast<const std::byte*>(mapped)+rgb_fp.Offset+
                    (uint64_t(z)*rgb_fp.Footprint.Height+y)*rgb_fp.Footprint.RowPitch+x*16+12,4);
                exact&=alpha==(number==7?0x3f800000u:1u);
            }
            D3D12_RANGE none{};host_rgb.handle->Unmap(0,&none);Require(exact);
        }
        if(data==4){
            const float negative_zero[]{-0.f,0.f,0.f,0.f};const u32 sign=0x80000000;
            std::shared_ptr<GuestStorageImageClearLease> signed_lease;
            if(readonly)cache.ClearRawStorageImage(image,negative_zero,renderer.RecordingList(),renderer.CompletedFence());
            else signed_lease=RecordGuestStorageImageClear(device.Get(),image,renderer.RecordingList(),negative_zero);
            GuestImageReadback signed_readback(device.Get(),image,renderer.RecordingList());
            const auto signed_fence=renderer.Flush();cache.Submitted(signed_fence);
            signed_readback.Submitted(signed_fence);renderer.Finish();
            signed_readback.Resolve(renderer.CompletedFence(),guest);
            for(u32 mip=0;mip<d.levels;++mip)
            for(u32 layer=0;layer<(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers);++layer)
            for(u32 y=0;y<std::max(d.height>>mip,1u);++y)
            for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
                memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y),&sign,4);
            Require(guest==expected);
        }
        if(readonly){cache.Collect(renderer.CompletedFence());
            Require(image.texture.handle==original&&image.desc==d&&image.allocation_bytes==allocation&&image.format_transfers.empty());}
        cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 data:{1u,2u,3u,5u,9u,10u,12u,16u,17u,18u,19u})for(const u32 number:{0u,1u,4u,5u,7u,9u}){
        if(number==9&&data!=10)continue;
        const bool half=data==2||data==5||data==12;
        const bool packed10=data==9;
        const bool packed16=data>=16&&data<=19;
        const bool srgb=number==9;
        if(packed16&&number!=0)continue;
        if(number==7&&!half)continue;
        for(const bool raw:{false,true}){
        if((raw&&data!=10&&!packed10)||(packed10&&!raw))continue;
        for(const bool readonly:{false,true}){
        if(readonly&&raw&&!packed10)continue;
        for(const u32 shape:{0u,1u,2u}){
            GuestTextureDesc d;d.width=17;d.pitch=32;d.height=shape==1?1:9;
            d.data_format=data;d.number_format=number;d.writable=!readonly;
            d.native_raw32=raw;
            d.native_packed10_raw=packed10;
            d.one_dimensional=shape==1;d.volume_depth=shape==2?4:0;
            d.layers=shape==2?1:3;d.first_layer=shape==2?0:1;d.view_layers=1;
            d.levels=3;d.first_mip=1;d.tile_mode=13;
            const auto layout=LayoutGuestTexture(d);
            std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6}),expected=guest;
            d.address=reinterpret_cast<uint64_t>(guest.data());
            auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
            float color[]{0.25f,0.5f,-0.5f,1.f};u32 components[4]{};
            if(number==4||number==5){
                const u32 unsigned_values[]{15,127,31,1};const s32 signed_values[]{-15,127,-31,1};
                for(unsigned component=0;component<4;++component){
                    components[component]=number==4?unsigned_values[component]:u32(signed_values[component]);
                    color[component]=std::bit_cast<float>(components[component]);
                }
            }else if(number==7){
                const float floats[]{1.5f,-0.f,-3.25f,0.25f};const u32 bits[]{0x3e00,0x8000,0xc280,0x3400};
                std::copy(std::begin(floats),std::end(floats),color);
                std::copy(std::begin(bits),std::end(bits),components);
            }else{
                const u32 unorm8[]{64,128,0,255},unorm16[]{16384,32768,0,65535};
                const u32 snorm8[]{32,64,u32(-64),127},snorm16[]{8192,16384,u32(-16384),32767};
                const auto* values=number==0?(half?unorm16:unorm8):(half?snorm16:snorm8);
                std::copy(values,values+4,components);
            }
            if(packed16)components[0]=data==16?(8u<<11)|(32u<<5):data==17?(8u<<10)|(16u<<5)|(1u<<15):
                data==18?(8u<<11)|(16u<<6)|1u:(4u<<4)|(8u<<8)|15u;
            if(srgb){const u32 encoded[]{137,188,0,255};std::copy(encoded,encoded+4,components);}
            if(packed10){const u32 unorm[]{256,512,0,3},snorm[]{128,256,768,1};
                const auto* values=number==0?unorm:number==1?snorm:components;u32 word{};
                for(unsigned c=0;c<4;++c)word|=(values[c]&(c==3?3u:1023u))<<(c*10);
                components[0]=word;}
            const auto original=image.texture.handle;
            const auto allocation=image.allocation_bytes;
            if(layout.expanded_bytes_per_pixel)image.SetGpuOwned(true);
            std::shared_ptr<GuestStorageImageClearLease> lease;
            if(raw||packed16||srgb||readonly){
                Require(GuestConvertedStorageClearCompatible(image)&&!GuestStorageImageClearCompatible(image,color));
                cache.ClearRawStorageImage(image,color,renderer.RecordingList(),renderer.CompletedFence());
                Require(image.texture.handle==original&&image.desc==d&&image.pending&&
                    image.allocation_bytes>allocation&&!image.format_transfers.empty());
            }else{
                Require(GuestStorageImageClearCompatible(image,color));
                lease=RecordGuestStorageImageClear(device.Get(),image,renderer.RecordingList(),color);
            }
            GuestImageReadback readback(device.Get(),image,renderer.RecordingList());
            const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
            readback.Resolve(renderer.CompletedFence(),guest);
            const u32 component_bytes=packed10?4:half||packed16?2:1,channels=layout.bytes_per_pixel/component_bytes;
            for(u32 mip=0;mip<d.levels;++mip)
            for(u32 layer=0;layer<(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers);++layer)
            for(u32 y=0;y<std::max(d.height>>mip,1u);++y)
            for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
            for(u32 component=0;component<channels;++component)
                memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y)+component*component_bytes,
                    &components[component],component_bytes);
            Require(guest==expected);
            if(packed10){
                float edge[4];u32 values[4];
                if(number==0){const float input[]{-1.f,1.f,2.f,0.5f};const u32 bits[]{0,1023,1023,2};
                    std::copy(input,input+4,edge);std::copy(bits,bits+4,values);
                }else if(number==1){const float input[]{-1.f,-0.5f,0.5f,-1.f};const u32 bits[]{513,768,256,3};
                    std::copy(input,input+4,edge);std::copy(bits,bits+4,values);
                }else if(number==4){const u32 input[]{UINT32_MAX,1024,511,4},bits[]{1023,1023,511,3};
                    for(unsigned c=0;c<4;++c)edge[c]=std::bit_cast<float>(input[c]);std::copy(bits,bits+4,values);
                }else{const s32 input[]{-700,700,-512,-3};const u32 bits[]{512,511,512,2};
                    for(unsigned c=0;c<4;++c)edge[c]=std::bit_cast<float>(input[c]);std::copy(bits,bits+4,values);}
                u32 word{};for(unsigned c=0;c<4;++c)word|=values[c]<<(c*10);
                cache.ClearRawStorageImage(image,edge,renderer.RecordingList(),renderer.CompletedFence());
                GuestImageReadback edge_readback(device.Get(),image,renderer.RecordingList());
                const auto edge_fence=renderer.Flush();cache.Submitted(edge_fence);edge_readback.Submitted(edge_fence);
                renderer.Finish();edge_readback.Resolve(renderer.CompletedFence(),guest);
                for(u32 mip=0;mip<d.levels;++mip)
                for(u32 layer=0;layer<(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers);++layer)
                for(u32 y=0;y<std::max(d.height>>mip,1u);++y)for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
                    memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y),&word,4);
                Require(guest==expected&&image.texture.handle==original&&image.desc==d);
            }
            if(srgb){
                const float edge[]{0.003f,0.0031308f,0.004f,0.5f};const u32 encoded=0x800d0a0a;
                cache.ClearRawStorageImage(image,edge,renderer.RecordingList(),renderer.CompletedFence());
                GuestImageReadback edge_readback(device.Get(),image,renderer.RecordingList());
                const auto edge_fence=renderer.Flush();cache.Submitted(edge_fence);edge_readback.Submitted(edge_fence);
                renderer.Finish();edge_readback.Resolve(renderer.CompletedFence(),guest);
                for(u32 mip=0;mip<d.levels;++mip)
                for(u32 layer=0;layer<(d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers);++layer)
                for(u32 y=0;y<std::max(d.height>>mip,1u);++y)for(u32 x=0;x<std::max(d.width>>mip,1u);++x)
                    memcpy(expected.data()+GuestPixelOffset(d,layout,mip,layer,x,y),&encoded,4);
                Require(guest==expected&&image.texture.handle==original&&image.desc==d);
            }
            if(raw||packed16||srgb||readonly){cache.Collect(renderer.CompletedFence());
                Require(image.format_transfers.empty()&&image.format_retirement_bytes==0&&
                    image.allocation_bytes==allocation&&image.texture.handle==original);}
            if(layout.expanded_bytes_per_pixel){
                Require(image.expanded_guest_backing.handle&&image.expanded_written_mask.handle&&image.expanded_backing_bytes);
                image.SetGpuOwned(false);
            }
            cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
        }
        }
        }
    }
    // Original linear downloads copy mip zero into compact array/depth rows;
    // they do not use the physical full-mip-chain stride between array layers.
    for(const u32 data:{4u,9u,10u,13u,16u,17u,18u,19u})for(const u32 shape:{0u,1u,2u}){
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=shape==1?1:9;
        d.data_format=data;d.number_format=data==4||data==13?7:data==9?1:0;
        d.writable=data!=9&&(data<16||shape==0);
        d.native_raw32=data==10;d.one_dimensional=shape==1;d.volume_depth=shape==2?4:0;
        d.layers=shape==2?1:3;d.first_layer=shape==2?0:1;d.view_layers=1;
        d.levels=3;d.first_mip=1;d.tile_mode=8;
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> backing(layout.guest_bytes,std::byte{0xee}),expected=backing;
        d.address=reinterpret_cast<uint64_t>(backing.data());
        auto& image=cache.Upload(d,backing,renderer.RecordingList(),renderer.CompletedFence());
        const float color[]{0.25f,0.5f,-0.5f,1.f};
        if(layout.expanded_bytes_per_pixel)image.SetGpuOwned(true);
        std::shared_ptr<GuestStorageImageClearLease> lease;
        if(data==9||data==10||data>=16)cache.ClearRawStorageImage(image,color,renderer.RecordingList(),renderer.CompletedFence());
        else lease=RecordGuestStorageImageClear(device.Get(),image,renderer.RecordingList(),color);
        const auto resident_before_source=cache.ResidentBytes();
        auto retained_source=cache.RetainPressureSource(image);
        Require(cache.ResidentBytes()==resident_before_source+retained_source.bytes);
        GuestLinearImageReadback download(device.Get(),image,renderer.RecordingList());
        const auto compact=uint64_t(d.pitch)*d.height*std::max(d.volume_depth,1u)*d.layers*layout.bytes_per_pixel;
        Require(download.Bytes()==compact&&compact<backing.size());
        const auto fence=renderer.Flush();cache.Submitted(fence);download.Submitted(fence);renderer.Finish();
        download.ReleaseSource(renderer.CompletedFence());
        retained_source.lease.reset();Require(!cache.PressureRetirementBytes());
        download.Resolve(renderer.CompletedFence(),backing);
        const u32 words[]{0x3e800000,0x3f000000,0xbf000000},rgba=0xff008040;
        const u16 packed=data==16?0x4400:data==17?0xa200:data==18?0x4401:0x084f;
        const u32 signed_packed=0x70040080;
        for(u32 layer=0;layer<d.layers;++layer)for(u32 z=0;z<std::max(d.volume_depth,1u);++z)
        for(u32 y=0;y<d.height;++y)for(u32 x=0;x<d.width;++x){
            auto* texel=expected.data()+((uint64_t(layer)*std::max(d.volume_depth,1u)+z)*d.height+y)*d.pitch*layout.bytes_per_pixel+x*layout.bytes_per_pixel;
            if(data>=16)memcpy(texel,&packed,2);
            else if(data==9)memcpy(texel,&signed_packed,4);
            else memcpy(texel,data==10?&rgba:words,layout.bytes_per_pixel);
        }
        Require(backing==expected);
        if(layout.expanded_bytes_per_pixel)image.SetGpuOwned(false);
        cache.Collect(renderer.CompletedFence());
        cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    for(const u32 data:{4u,9u,16u,17u,18u,19u}){
        // Evicted cache/allocator lifetimes can end while the compact GPU copy
        // is blocked. Its source lease must retain placed storage independently.
        Rasterizer queued(device.Get());
        GuestTextureDesc d;d.width=17;d.pitch=32;d.height=9;d.layers=3;d.levels=3;
        d.data_format=data;d.number_format=data==4?7:data==9?1:0;d.writable=data==4;d.tile_mode=8;
        if(data>=16){d.layers=1;d.volume_depth=4;}
        const auto layout=LayoutGuestTexture(d);
        std::vector<std::byte> backing(layout.guest_bytes,std::byte{0xee}),expected=backing;
        d.address=reinterpret_cast<uint64_t>(backing.data());queued.MapMemory(d.address,backing.size());
        ComPtr<ID3D12Fence> gate;Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)));
        SCOPE_EXIT {gate->Signal(1);};
        uint64_t copy_fence{},retained_bytes{};
        {
            std::lock_guard lock(queued.ExecutionMutex());
            {
                GuestTextureCache retiring(device.Get());
                auto& image=retiring.Upload(d,backing,queued.RecordingList(),queued.CompletedFence());
                const float color[]{0.25f,0.5f,-0.5f,1.f};
                std::shared_ptr<GuestStorageImageClearLease> clear;
                if(layout.expanded_bytes_per_pixel){image.SetGpuOwned(true);
                    retiring.ClearRawStorageImage(image,color,queued.RecordingList(),queued.CompletedFence());}
                else clear=RecordGuestStorageImageClear(device.Get(),image,queued.RecordingList(),color);
                const auto initial=queued.Flush();retiring.Submitted(initial);queued.Finish();retiring.Collect(queued.CompletedFence());
                auto source=retiring.RetainPressureSource(image);auto second_source=source.lease;
                u64 expected_source=image.allocation_bytes;
                if(layout.expanded_bytes_per_pixel){const auto packed=image.expanded_guest_backing.handle->GetDesc();
                    expected_source+=device->GetResourceAllocationInfo(0,1,&packed).SizeInBytes;}
                Require(source.bytes==expected_source&&retiring.PressureRetirementBytes()==source.bytes);
                second_source.reset();Require(retiring.PressureRetirementBytes()==source.bytes);
                auto download=std::make_shared<GuestLinearImageReadback>(device.Get(),image,queued.RecordingList());
                queued.WaitExternalFence(gate.Get(),1);
                copy_fence=queued.Flush();retiring.Submitted(copy_fence);download->Submitted(copy_fence);
                Require(queued.CompletedFence()<copy_fence&&retiring.PressureRetirementBytes()==source.bytes);
                retained_bytes=download->Bytes()+download->AllocationBytes()+source.bytes;
                queued.RetainUntilFence(copy_fence,[download,lease=std::move(source.lease)](u64 completed){download->ReleaseSource(completed);});
                queued.DeferImageBacking(copy_fence,d.address,download->Bytes(),download->AllocationBytes()+source.bytes,
                    [&](VAddr,std::span<std::byte> data){memcpy(data.data(),backing.data(),data.size());return true;},
                    [&](VAddr,std::span<const std::byte> data){memcpy(backing.data(),data.data(),data.size());return true;},
                    [download](u64 completed,std::span<std::byte> data){download->ReleaseSource(completed);download->Resolve(completed,data);});
            }
            // Cache entries, upload pools, allocator and the clear descriptor
            // owner are gone before this GPU fence is signaled.
            Require(queued.CompletedFence()<copy_fence&&queued.PendingImageCommitBytes()==retained_bytes);
            Check(gate->Signal(1));
        }
        queued.DrainPriorityOperations();queued.CollectRetirements();
        Require(!queued.PendingImageCommitBytes());
        const u32 word=data==4?0x3e800000u:0x70040080u;
        const u16 packed=data==16?0x4400:data==17?0xa200:data==18?0x4401:0x084f;
        for(u32 layer=0;layer<d.layers;++layer)for(u32 z=0;z<std::max(d.volume_depth,1u);++z)
        for(u32 y=0;y<d.height;++y)for(u32 x=0;x<d.width;++x)
            memcpy(expected.data()+((uint64_t(layer)*std::max(d.volume_depth,1u)+z)*d.height+y)*d.pitch*layout.bytes_per_pixel+x*layout.bytes_per_pixel,
                   data>=16?static_cast<const void*>(&packed):&word,layout.bytes_per_pixel);
        Require(backing==expected);
    }
    renderer.UnmapMemory(address,bytes);UnmapViewOfFile(alias);UnmapViewOfFile(guest);CloseHandle(section);
    {
        // PS4 buffer copies also alias depth -> sampled float images. D3D12
        // permits CopyResource across the R32 typeless format family.
        GuestTextureDesc d;d.width=d.pitch=d.height=256;d.tile_mode=0;d.data_format=4;d.number_format=7;
        d.depth_target=true;auto l=LayoutGuestTexture(d);
        std::vector<std::byte> depth_bytes(l.guest_bytes),sampled_bytes(l.guest_bytes);
        d.address=reinterpret_cast<uint64_t>(depth_bytes.data());
        auto& depth=cache.Upload(d,depth_bytes,renderer.RecordingList(),renderer.CompletedFence());
        d.depth_target=false;d.address=reinterpret_cast<uint64_t>(sampled_bytes.data());
        auto& sampled=cache.Upload(d,sampled_bytes,renderer.RecordingList(),renderer.CompletedFence());
        Require(GuestImageCopyCompatible(depth,sampled));
        Descriptors dsvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);auto view=depth.Dsv();
        device->CreateDepthStencilView(depth.texture.handle.Get(),&view,dsvs.Cpu(0));
        depth.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
        renderer.RecordingList()->ClearDepthStencilView(dsvs.Cpu(0),D3D12_CLEAR_FLAG_DEPTH,0.75f,0,0,nullptr);
        RecordGuestImageCopy(depth,sampled,renderer.RecordingList());
        GuestImageReadback rb(device.Get(),sampled,renderer.RecordingList());
        auto fence=renderer.Flush();cache.Submitted(fence);rb.Submitted(fence);renderer.Finish();
        rb.Resolve(renderer.CompletedFence(),sampled_bytes);
        for(size_t i=0;i<sampled_bytes.size();i+=4){float value;memcpy(&value,sampled_bytes.data()+i,4);Require(value==0.75f);}
        cache.Invalidate(depth.desc.address,depth.layout.guest_bytes,renderer.CompletedFence());
        cache.Invalidate(sampled.desc.address,sampled.layout.guest_bytes,renderer.CompletedFence());
    }
    {
        GuestTextureDesc d;d.width=d.pitch=d.height=256;d.tile_mode=0;d.data_format=4;d.number_format=7;d.depth_target=true;
        auto dl=LayoutGuestTexture(d);const auto depth_size=dl.guest_bytes;
        GuestTextureDesc sd=d;sd.depth_target=false;sd.data_format=1;sd.number_format=4;
        auto sl=LayoutGuestTexture(sd);const auto stencil_size=sl.guest_bytes,total=depth_size+stencil_size;
        auto section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,DWORD(total),nullptr);Require(section!=nullptr);
        auto* guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,total));
        auto* alias=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,total));Require(guest&&alias);
        const auto address=reinterpret_cast<uint64_t>(guest);d.address=address;d.stencil_address=address+depth_size;
        auto& image=cache.Upload(d,{guest,size_t(depth_size)},renderer.RecordingList(),renderer.CompletedFence(),
            {guest+depth_size,size_t(stencil_size)});
        unsigned commits{};PageWatch* watcher{};
        const auto resolve=[&](uint64_t fault){Require(fault>=address&&fault<address+total);if(!image.gpu_owned)return;
            renderer.Finish();std::vector<std::byte> depth(alias,alias+depth_size),stencil(alias+depth_size,alias+total);
            GuestImageReadback rb(device.Get(),image,renderer.RecordingList());auto fence=renderer.Flush();cache.Submitted(fence);rb.Submitted(fence);renderer.Finish();
            rb.Resolve(renderer.CompletedFence(),depth,stencil);memcpy(alias,depth.data(),depth_size);memcpy(alias+depth_size,stencil.data(),stencil_size);
            image.RememberSnapshot(depth,stencil);image.SetGpuOwned(false);++commits;
            watcher->Untrack(address,depth_size);watcher->Untrack(address+depth_size,stencil_size);
        };
        PageWatch watch(resolve);watcher=&watch;
        Require(watch.Track(address,depth_size)&&watch.Track(address+depth_size,stencil_size));image.SetGpuOwned(true);
        Descriptors dsv(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);auto view=image.Dsv();
        device->CreateDepthStencilView(image.texture.handle.Get(),&view,dsv.Cpu(0));
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
        renderer.RecordingList()->ClearDepthStencilView(dsv.Cpu(0),D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,0.75f,91,0,nullptr);
        auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        auto sampled=d;sampled.depth_target=false;sampled.stencil_address=0;
        auto& reused=cache.Upload(sampled,{guest,size_t(depth_size)},renderer.RecordingList(),renderer.CompletedFence());
        Require(&reused==&image&&image.gpu_owned&&commits==0);
        auto srv=image.Srv(sampled);Require(srv.Format==DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS);
        Descriptors srvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
        device->CreateShaderResourceView(image.texture.handle.Get(),&srv,srvs.Cpu(0));
        fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();renderer.ValidateDebugMessages();
        // Access the second plane first: the fault commits BOTH GPU planes.
        const auto stencil_value=*reinterpret_cast<volatile unsigned char*>(guest+depth_size);
        Require(stencil_value==91&&commits==1&&!image.gpu_owned&&
            !watch.Contains(address)&&!watch.Contains(address+depth_size));
        const auto depth=DecodeGuestSubresource(d,dl,{guest,size_t(depth_size)},0,0);
        for(size_t i=0;i<depth.size();i+=4){float value;memcpy(&value,depth.data()+i,4);Require(value==0.75f);}
        sd.address=address+depth_size;const auto stencil=DecodeGuestSubresource(sd,sl,{guest+depth_size,size_t(stencil_size)},0,0);
        Require(std::all_of(stencil.begin(),stencil.end(),[](auto value){return value==std::byte{91};}));
        cache.Invalidate(address+depth_size,stencil_size,renderer.CompletedFence());
        Require(std::none_of(cache.Entries().begin(),cache.Entries().end(),[&](const auto& entry){return entry->desc.address==address;}));
        UnmapViewOfFile(alias);UnmapViewOfFile(guest);CloseHandle(section);
    }
    {
        GuestTextureDesc d;d.width=d.pitch=512;d.height=504;d.tile_mode=10;d.render_target=true;
        auto l=LayoutGuestTexture(d);Require(l.logical_bytes<l.guest_bytes);
        std::vector<std::byte> src_bytes(l.guest_bytes,std::byte{0x22}),dst_bytes(l.guest_bytes,std::byte{0x77});
        d.address=reinterpret_cast<uint64_t>(src_bytes.data());
        auto& src=cache.Upload(d,src_bytes,renderer.RecordingList(),renderer.CompletedFence());
        d.address=reinterpret_cast<uint64_t>(dst_bytes.data());
        auto& dst=cache.Upload(d,dst_bytes,renderer.RecordingList(),renderer.CompletedFence());
        Require(GuestImageCopyCompatible(src,dst));
        src.desc.storage_height=l.mips[0].height;Require(GuestImageCopyCompatible(src,dst));
        --src.layout.mips[0].height;Require(!GuestImageCopyCompatible(src,dst));++src.layout.mips[0].height;
        Descriptors rtvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);auto view=src.Rtv();
        device->CreateRenderTargetView(src.texture.handle.Get(),&view,rtvs.Cpu(0));
        src.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float yellow[]{1,1,0,1};renderer.RecordingList()->ClearRenderTargetView(rtvs.Cpu(0),yellow,0,nullptr);
        bool rejected=false;try{RecordGuestImageCopy(src,dst,renderer.RecordingList());}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected);memcpy(dst_bytes.data(),src_bytes.data(),src_bytes.size());
        RecordGuestImageCopy(src,dst,renderer.RecordingList(),true);
        GuestImageReadback rb(device.Get(),dst,renderer.RecordingList());auto fence=renderer.Flush();cache.Submitted(fence);rb.Submitted(fence);renderer.Finish();
        rb.Resolve(renderer.CompletedFence(),dst_bytes);
        auto allocation=d;allocation.height=l.mips[0].height;const auto full_layout=LayoutGuestTexture(allocation);
        Require(full_layout.guest_bytes==l.guest_bytes);
        const auto pixels=DecodeGuestSubresource(allocation,full_layout,dst_bytes,0,0);
        for(unsigned y=0;y<allocation.height;++y)for(unsigned x=0;x<d.width;++x){const auto i=(size_t(y)*d.width+x)*4;
            if(y<d.height)Require(pixels[i]==std::byte{255}&&pixels[i+1]==std::byte{255}&&pixels[i+2]==std::byte{0}&&pixels[i+3]==std::byte{255});
            else Require(pixels[i]==std::byte{0x22}&&pixels[i+1]==std::byte{0x22}&&pixels[i+2]==std::byte{0x22}&&pixels[i+3]==std::byte{0x22});}
        cache.Invalidate(src.desc.address,src.layout.guest_bytes,renderer.CompletedFence());
        cache.Invalidate(dst.desc.address,dst.layout.guest_bytes,renderer.CompletedFence());
    }
    {
        constexpr size_t buffer_bytes=16384;
        HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,buffer_bytes,nullptr);Require(section!=nullptr);
        auto* memory=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,buffer_bytes));
        auto* physical=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,buffer_bytes));Require(memory&&physical);
        const auto base=reinterpret_cast<VAddr>(memory);
        OwnershipRenderer buffers(device.Get());buffers.MapMemory(base,buffer_bytes);
        // The fixture owns a committed physical alias, just as Core does.
        // Reservation alone cannot authorize GPU access through NOACCESS.
        buffers.NotifyHostMapping(base,buffer_bytes,true,true);
        std::atomic<unsigned> commits{},faults{};
        bool rearm_after_resolve=false;
        PageWatch watch([&](uint64_t at){
            ++faults;
            buffers.ResolveGpuBuffers(at,1);
            buffers.ReleaseCpuBufferWatches(at,1);
            if(rearm_after_resolve){
                rearm_after_resolve=false;
                // Reproduce the legal schedule where the GPU worker acquires
                // the execution mutex after resolution and rearms this page
                // before the fault handler examines the host permissions.
                const Rasterizer::ShaderBufferRequest request[]{{base,buffer_bytes,false,false}};
                buffers.PrepareShaderBuffers(request);
                MEMORY_BASIC_INFORMATION region{};
                Require(VirtualQuery(memory,&region,sizeof(region))&&region.Protect==PAGE_READONLY);
            }
        });
        buffers.SetBufferOwnership([&](VAddr at,u64 size){return watch.Track(at,size);},
            [&](VAddr at,std::span<const std::byte> data){++commits;memcpy(physical+(at-base),data.data(),data.size());return true;},
            [&](VAddr at,u64 size){watch.Untrack(at,size);});
        buffers.SetBufferWriteWatch([&](VAddr at,u64 size){return watch.Track(at,size,true);});
        const Rasterizer::ShaderBufferRequest writable[]{{base,buffer_bytes,true,true}};
        auto first=buffers.PrepareShaderBuffers(writable);Require(buffers.BuffersMayDefer(writable));
        auto upload=Buffer(device.Get(),buffer_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));
        memset(mapped,0x5a,buffer_bytes);upload.handle->Unmap(0,nullptr);
        Resource target;target.handle=first[0].resource;target.state=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        buffers.RecordingList()->CopyBufferRegion(target.handle.Get(),first[0].offset,upload.handle.Get(),0,buffer_bytes);
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        buffers.Finish();Require(commits==0&&physical[0]==std::byte{0}&&watch.Contains(base));
        const Rasterizer::ShaderBufferRequest readonly[]{{base,buffer_bytes,false,false}};
        auto next=buffers.PrepareShaderBuffers(readonly);Require(next[0].resource.Get()==first[0].resource.Get()&&commits==0);
        auto output=Buffer(device.Get(),buffer_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        target.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        buffers.RecordingList()->CopyBufferRegion(output.handle.Get(),0,target.handle.Get(),next[0].offset,buffer_bytes);
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        buffers.Finish();D3D12_RANGE all{0,buffer_bytes};Check(output.handle->Map(0,&all,&mapped));
        for(size_t i=0;i<buffer_bytes;++i)Require(static_cast<std::byte*>(mapped)[i]==std::byte{0x5a});output.handle->Unmap(0,&none);
        Require(commits==0&&physical[4096]==std::byte{0});
        Require(*reinterpret_cast<volatile unsigned char*>(memory+4096)==0x5a);
        Require(commits==1&&faults==1&&!watch.Contains(base));
        auto watched=buffers.PrepareShaderBuffers(readonly);buffers.Finish();Require(watch.Contains(base));
        Require(*reinterpret_cast<volatile unsigned char*>(memory)==0x5a&&faults==1);
        rearm_after_resolve=true;
        *reinterpret_cast<volatile unsigned char*>(memory+4096)=0x77;Require(faults==3);
        Require(!watch.Contains(base)&&commits==1);
        MEMORY_BASIC_INFORMATION restored{};
        Require(VirtualQuery(memory,&restored,sizeof(restored))&&restored.Protect==PAGE_READWRITE);
        // Deliver a real CPU write fault after the renderer has already retired
        // its large watch, then retired a shorter interior watch. The
        // predecessor's end must not hide the earlier range covering the fault.
        buffers.PrepareShaderBuffers(readonly);buffers.Finish();Require(watch.Contains(base));
        fault_entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        deliver_fault=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        Require(fault_entered&&deliver_fault);
        delayed_address=base+8193;
        auto* gate=AddVectoredExceptionHandler(1,DelayWriteFault);Require(gate!=nullptr);
        std::thread writer([&]{*reinterpret_cast<volatile unsigned char*>(memory+8193)=0x88;});
        const auto arrived=WaitForSingleObject(fault_entered,3000);
        if(arrived==WAIT_OBJECT_0){
            buffers.ReleaseCpuBufferWatches(base,buffer_bytes);
            Require(watch.Track(base+4096,4096,true));watch.Untrack(base+4096,4096);
        }
        SetEvent(deliver_fault);writer.join();RemoveVectoredExceptionHandler(gate);
        CloseHandle(fault_entered);CloseHandle(deliver_fault);delayed_address=0;
        Require(arrived==WAIT_OBJECT_0&&physical[8193]==std::byte{0x88});
        Require(buffers.InvalidateMemory(base+4096,1));
        auto edited=buffers.PrepareShaderBuffers(readonly);Require(edited[0].resource.Get()==watched[0].resource.Get());
        target.state=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        buffers.RecordingList()->CopyBufferRegion(output.handle.Get(),0,target.handle.Get(),edited[0].offset,buffer_bytes);
        target.Transition(buffers.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        buffers.Finish();Check(output.handle->Map(0,&all,&mapped));
        for(size_t i=0;i<buffer_bytes;++i)Require(static_cast<std::byte*>(mapped)[i]==static_cast<std::byte>(i==4096?0x77:i==8193?0x88:0x5a));
        output.handle->Unmap(0,&none);buffers.UnmapMemory(base,buffer_bytes);
        Require(!watch.Contains(base)&&buffers.CompletedFence()>=buffers.SubmittedFence());
        buffers.ValidateDebugMessages();
        UnmapViewOfFile(memory);UnmapViewOfFile(physical);CloseHandle(section);
    }
    puts("PASS: GPU buffer ownership and deferred CPU fault commit, write watchers, persistent CPU invalidation, depth/stencil ownership, padded GPU copies and exact pixels");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
