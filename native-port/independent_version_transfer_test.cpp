// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <cstring>
#include <cstdio>
using namespace NativeDX12;
void RunMultisample(ID3D12Device* device,UINT samples,UINT number_format,bool writable){
    Rasterizer renderer(device);GuestTextureCache cache(device);GuestMultisample transfer(device);
    GuestTextureDesc desc;desc.address=0x40000;desc.width=7;desc.height=5;desc.pitch=8;desc.layers=2;
    desc.tile_mode=13;desc.data_format=4;desc.number_format=number_format;desc.samples=samples;desc.writable=true;
    auto sampled=desc;sampled.writable=writable;sampled.tile_mode=10;
    sampled.number_format=number_format==4?5:number_format==5?7:4;
    const auto layout=LayoutGuestTexture(desc),sampled_layout=LayoutGuestTexture(sampled);
    std::vector<std::byte> physical(std::max(layout.guest_bytes,sampled_layout.guest_bytes),std::byte{0xa7});
    std::vector<u32> old_pixels(size_t(desc.width)*desc.height*samples,0x3f800000u);
    for(UINT layer=0;layer<desc.layers;++layer)
        EncodeGuestSubresource(desc,layout,physical,std::as_bytes(std::span(old_pixels)),0,layer);
    auto expected_private=physical;
    Resource gpu,upload,physical_readback,private_readback;
    std::vector<Resource> writes,linear,readbacks,requested_versions;
    std::vector<std::shared_ptr<GuestMultisample::Lease>> writer_leases,reader_leases;
    // Keep cache/resources alive while retiring any commands on an exceptional
    // preparation exit as well as the successful comparison path.
    try{
        gpu=Buffer(device,physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        upload=Buffer(device,physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        physical_readback=Buffer(device,physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        private_readback=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};Check(upload.handle->Map(0,nullptr,&mapped));
        memcpy(mapped,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(gpu.handle.Get(),0,upload.handle.Get(),0,physical.size());
        cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        const auto acquire=[](auto& image){image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
        cache.SetOwnershipAcquirer(acquire);
        unsigned transfers{};
        cache.SetIndependentVersionTransfer([&](auto& image,u64 address,u64 size){
            ++transfers;
            if(address!=desc.address||size!=sampled_layout.guest_bytes)
                throw std::runtime_error("Unexpected MSAA transfer extent");
            auto version=cache.RetileBufferVersion(image,gpu,renderer.RecordingList(),renderer.CompletedFence());
            version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(private_readback.handle.Get(),0,version.handle.Get(),0,layout.guest_bytes);
            // The production callback seeds the destination envelope from its
            // physical domain before overlaying the source image intersection.
            requested_versions.push_back(Buffer(device,physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST));
            auto& requested=requested_versions.back();
            gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,gpu.handle.Get(),0,physical.size());
            renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,version.handle.Get(),0,std::min(size,layout.guest_bytes));
            requested.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            return requested;
        },acquire,[](auto& image){image.buffer_alias_version=false;image.SetGpuOwned(false);});
        auto& source=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        const auto fp=GuestMultisample::Footprint(desc,layout);
        const auto linear_bytes=GuestMultisample::LinearBytes(desc,fp);
        writes.reserve(desc.layers);writer_leases.reserve(desc.layers);
        for(UINT layer=0;layer<desc.layers;++layer){
            std::vector<u32> values(old_pixels.size());
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
                values[(sample*desc.height+y)*desc.width+x]=
                    0x3e000000u|(layer<<19)|(sample<<15)|(y<<8)|x|(number_format==5?0x80000000u:0u);
            EncodeGuestSubresource(desc,layout,expected_private,std::as_bytes(std::span(values)),0,layer);
            writes.push_back(Buffer(device,linear_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            auto& buffer=writes.back();Check(buffer.handle->Map(0,nullptr,&mapped));memset(mapped,0xdc,size_t(linear_bytes));
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y)
                memcpy(static_cast<std::byte*>(mapped)+(sample*desc.height+y)*fp.Footprint.RowPitch,
                    values.data()+(sample*desc.height+y)*desc.width,desc.width*sizeof(u32));
            buffer.handle->Unmap(0,nullptr);
            writer_leases.push_back(transfer.Prepare(source.texture,buffer,desc,layout,layer,0,true));
            writer_leases.back()->Record(renderer.RecordingList(),source.texture,buffer);
        }
        if(!cache.Own(source))throw std::runtime_error("MSAA source ownership acquisition failed");
        if(samples==2&&number_format==4){
            auto mismatch=sampled;mismatch.samples=4;
            std::vector<std::byte> invalid_bytes(LayoutGuestTexture(mismatch).guest_bytes);
            bool rejected{};
            try{cache.Upload(mismatch,invalid_bytes,renderer.RecordingList(),renderer.CompletedFence());}
            catch(const std::runtime_error& error){
                rejected=strcmp(error.what(),"Independent MSAA image transfer needs matching R32 sample domains")==0;
            }
            if(!rejected||transfers||!source.gpu_owned)
                throw std::runtime_error("MSAA sample-count mismatch changed its independent owner");
        }
        const auto source_uid=source.image_uid;
        auto& converted=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
        const bool ownership=transfers==1&&converted.image_uid!=source_uid&&converted.gpu_owned&&
            converted.buffer_alias_version&&!source.gpu_owned;
        const auto result_fp=GuestMultisample::Footprint(sampled,sampled_layout);
        const auto result_bytes=GuestMultisample::LinearBytes(sampled,result_fp);
        linear.reserve(desc.layers);readbacks.reserve(desc.layers);reader_leases.reserve(desc.layers);
        for(UINT layer=0;layer<desc.layers;++layer){
            linear.push_back(GuestTiler::Scratch(device,result_bytes));
            readbacks.push_back(Buffer(device,result_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST));
            reader_leases.push_back(transfer.Prepare(converted.texture,linear.back(),sampled,sampled_layout,layer,0,false));
            reader_leases.back()->Record(renderer.RecordingList(),converted.texture,linear.back());
            linear.back().Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(readbacks.back().handle.Get(),0,linear.back().handle.Get(),0,result_bytes);
        }
        gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(physical_readback.handle.Get(),0,gpu.handle.Get(),0,physical.size());
        cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
        for(UINT layer=0;layer<desc.layers;++layer){
            const auto expected=DecodeGuestSubresource(sampled,sampled_layout,expected_private,0,layer);
            Check(readbacks[layer].handle->Map(0,nullptr,&mapped));bool equal=true;
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y)
                equal&=memcmp(static_cast<const std::byte*>(mapped)+(sample*desc.height+y)*result_fp.Footprint.RowPitch,
                    expected.data()+size_t(sample*desc.height+y)*desc.width*sizeof(u32),desc.width*sizeof(u32))==0;
            readbacks[layer].handle->Unmap(0,nullptr);
            if(!equal)throw std::runtime_error("MSAA reinterpret lost sample/layer/format bits");
        }
        Check(private_readback.handle->Map(0,nullptr,&mapped));
        const bool private_exact=memcmp(mapped,expected_private.data(),size_t(layout.guest_bytes))==0;
        private_readback.handle->Unmap(0,nullptr);
        Check(physical_readback.handle->Map(0,nullptr,&mapped));
        const bool unchanged=memcmp(mapped,physical.data(),physical.size())==0;physical_readback.handle->Unmap(0,nullptr);
        if(!private_exact)throw std::runtime_error("MSAA private retile changed padding or sample layout");
        if(!unchanged)throw std::runtime_error("MSAA transfer changed physical buffer domain/padding");
        if(!ownership)throw std::runtime_error("MSAA logical ownership transfer failed");
        renderer.ValidateDebugMessages();printf("MSAA independent transfer samples=%u number_format=%u writable=%u layers=2 exact\n",samples,number_format,unsigned(writable));
    }catch(...){renderer.Finish();throw;}
}
int main(int argc,char** argv){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");auto device=CreateHardwareDevice();
    if(argc==2&&strcmp(argv[1],"--msaa")==0){
        for(const UINT samples:{2u,4u,8u})for(const UINT number_format:{4u,5u,7u})for(const bool writable:{false,true})
            RunMultisample(device.Get(),samples,number_format,writable);
        puts("PASS: independent R32 MSAA samples/layers, exact private padding, unchanged physical domain and ownership");return 0;
    }
    if(argc!=1)throw std::invalid_argument("Expected no arguments or --msaa");
    Rasterizer renderer(device.Get());
    GuestTextureCache cache(device.Get());GuestTextureDesc desc;desc.address=0x40000;desc.width=desc.height=desc.pitch=8;
    desc.tile_mode=13;desc.data_format=4;desc.number_format=7;desc.writable=true;
    auto sampled=desc;sampled.writable=false;sampled.data_format=10;sampled.number_format=0;sampled.tile_mode=10;
    const auto sampled_layout=LayoutGuestTexture(sampled);
    const auto layout=LayoutGuestTexture(desc);std::vector<std::byte> physical(std::max(layout.guest_bytes,sampled_layout.guest_bytes),std::byte{0xa7});
    std::array<float,64> old_pixels;old_pixels.fill(1.f);EncodeGuestSubresource(desc,layout,physical,std::as_bytes(std::span(old_pixels)),0,0);
    auto gpu=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto upload=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};Check(upload.handle->Map(0,nullptr,&mapped));memcpy(mapped,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
    renderer.RecordingList()->CopyBufferRegion(gpu.handle.Get(),0,upload.handle.Get(),0,physical.size());
    cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
    const auto acquire=[](auto& image){image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
    cache.SetOwnershipAcquirer(acquire);
    cache.SetIndependentVersionTransfer([&](auto& image,u64 address,u64 size){
        if(address!=desc.address||size!=sampled_layout.guest_bytes)throw std::runtime_error("Unexpected test transfer extent");
        auto version=cache.RetileBufferVersion(image,gpu,renderer.RecordingList(),renderer.CompletedFence());
        auto requested=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,gpu.handle.Get(),0,physical.size());
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,version.handle.Get(),0,std::min(size,layout.guest_bytes));
        version=std::move(requested);
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return version;
    },acquire,[](auto& image){image.buffer_alias_version=false;image.SetGpuOwned(false);});
    auto& source=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
    Descriptors views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);const auto slot=views.Allocate(0);
    Descriptors cpu_views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,false);const auto cpu_slot=cpu_views.Allocate(0);
    const auto uav=source.Uav(desc,0);device->CreateUnorderedAccessView(source.texture.handle.Get(),nullptr,&uav,views.Cpu(slot));
    device->CreateUnorderedAccessView(source.texture.handle.Get(),nullptr,&uav,cpu_views.Cpu(cpu_slot));
    ID3D12DescriptorHeap* heaps[]{views.Heap()};renderer.RecordingList()->SetDescriptorHeaps(1,heaps);
    source.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const float value[]{0.25f,0,0,0};renderer.RecordingList()->ClearUnorderedAccessViewFloat(views.Gpu(slot),cpu_views.Cpu(cpu_slot),source.texture.handle.Get(),value,0,nullptr);
    if(!cache.Own(source))throw std::runtime_error("Source ownership acquisition failed");
    const auto source_uid=source.image_uid;
    auto& converted=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
    const bool transferred=converted.image_uid!=source_uid&&converted.gpu_owned&&converted.buffer_alias_version&&!source.gpu_owned;
    auto pixels=TextureTransfer::Create(device.Get(),converted.texture,true);pixels.Readback(renderer.RecordingList(),converted.texture);
    auto backing=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);renderer.RecordingList()->CopyBufferRegion(backing.handle.Get(),0,gpu.handle.Get(),0,physical.size());
    cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
    std::array<u32,64> restored{};pixels.ReadRows(restored.data(),sizeof(restored),32);
    auto private_bytes=physical;std::array<float,64> new_pixels;new_pixels.fill(0.25f);
    EncodeGuestSubresource(desc,layout,private_bytes,std::as_bytes(std::span(new_pixels)),0,0);
    const auto expected=DecodeGuestSubresource(sampled,sampled_layout,private_bytes,0,0);
    if(memcmp(restored.data(),expected.data(),expected.size()))throw std::runtime_error("New image interpretation lost GPU-written float bits/layout");
    Check(backing.handle->Map(0,nullptr,&mapped));const bool unchanged=memcmp(mapped,physical.data(),physical.size())==0;backing.handle->Unmap(0,nullptr);
    if(!unchanged)throw std::runtime_error("Image transfer overwrote the physical buffer domain or padding");
    if(!transferred)throw std::runtime_error("Independent interpretation did not transfer its logical version");
    renderer.ValidateDebugMessages();puts("PASS: GPU writer -> retile private version -> different sampled format; physical GPU buffer/padding unchanged; logical ownership transfer");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
