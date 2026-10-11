// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/memory_surface_provider.h"
#include "video_core/renderer_d3d12/texture_resources.h"
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
    auto device=CreateHardwareDevice();OwnershipRenderer renderer(device.Get());GuestTextureCache cache(device.Get());
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
            image.RememberSnapshot(result);image.gpu_owned=false;++resolutions;watcher->Untrack(address,bytes);
        };PageWatch watch([&](uint64_t address){++page_faults;resolve(address);});watcher=&watch;
        cache.SetOwnershipResolver([&](auto&){resolve(address);renderer.RecordingList();});
        renderer.resolve=[&](VAddr address,u64){resolve(address);};
        Require(watch.Track(address,bytes));image.gpu_owned=true;
        Descriptors rtvs(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);
        auto view=image.Rtv();DX12_API_CALL(device->CreateRenderTargetView(image.texture.handle.Get(),&view,rtvs.Cpu(0)));
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float color[]{1.f,1.f,0.f,1.f};DX12_API_CALL(renderer.RecordingList()->ClearRenderTargetView(rtvs.Cpu(0),color,0,nullptr));
        auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
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
        memset(alias,0,bytes);Require(watch.Track(address,bytes));image.gpu_owned=true;
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
        for(size_t pixel=0;pixel<pixels.size();pixel+=4)
            Require(pixels[pixel]==std::byte{0}&&pixels[pixel+1]==std::byte{255}&&pixels[pixel+2]==std::byte{255}&&pixels[pixel+3]==std::byte{255});
        renderer.resolve={};
        // A compute UAV aliases the protected color target but cannot reuse
        // its render-only resource. Exercise the common Upload fallback with
        // the renderer lock held, exactly as ExecuteGuestResources does.
        memset(alias,0,bytes);Require(watch.Track(address,bytes));image.gpu_owned=true;
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
        Require(!GuestImageCopyCompatible(src,dst)&&!GuestImageClearCompatible(dst));dst.layout.logical_bytes=saved_bytes;
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
            image.RememberSnapshot(depth,stencil);image.gpu_owned=false;++commits;
            watcher->Untrack(address,depth_size);watcher->Untrack(address+depth_size,stencil_size);
        };
        PageWatch watch(resolve);watcher=&watch;
        Require(watch.Track(address,depth_size)&&watch.Track(address+depth_size,stencil_size));image.gpu_owned=true;
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
