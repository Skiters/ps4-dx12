// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <cstring>
#include <cstdio>
using namespace NativeDX12;
template<class Component=u32>
void RunMultisample(ID3D12Device* device,UINT samples,UINT number_format,bool writable,bool pressure=false,UINT data_format=4,
    bool raw_source=false,bool raw_destination=false){
    Rasterizer renderer(device);GuestTextureCache cache(device);GuestMultisample transfer(device);
    GuestTextureDesc desc;desc.address=0x40000;desc.width=7;desc.height=5;desc.pitch=8;desc.layers=2;
    desc.tile_mode=13;desc.data_format=data_format;desc.number_format=number_format;desc.samples=samples;desc.writable=true;
    desc.native_raw32=raw_source;
    auto sampled=desc;sampled.writable=writable;sampled.tile_mode=10;
    sampled.native_raw32=raw_destination;
    if((raw_source||raw_destination)&&(data_format!=10||(raw_destination&&!writable)))
        throw std::invalid_argument("Raw RGBA8 fixture needs data10 and a writable raw destination");
    sampled.number_format=number_format==4?5:number_format==5?7:4;
    if constexpr(sizeof(Component)==2)
        sampled.number_format=number_format==0?1:number_format==1?4:number_format==4?5:number_format==5?7:0;
    if constexpr(sizeof(Component)==1)
        sampled.number_format=number_format==0?1:number_format==1?4:number_format==4?5:
            number_format==5&&data_format==10?9:0;
    const auto layout=LayoutGuestTexture(desc),sampled_layout=LayoutGuestTexture(sampled);
    if(!GuestTextureCache::CanTransferIndependentMultisample(desc,layout)||
       !GuestTextureCache::CanTransferIndependentMultisample(sampled,sampled_layout))
        throw std::runtime_error("Expected raw-component MSAA domain was rejected");
    const auto components=layout.bytes_per_pixel/sizeof(Component);
    std::vector<std::byte> physical(std::max(layout.guest_bytes,sampled_layout.guest_bytes),std::byte{0xa7});
    std::vector<Component> old_pixels(size_t(desc.width)*desc.height*samples*components,
        static_cast<Component>(sizeof(Component)==1?0x55u:sizeof(Component)==2?0x3c00u:0x3f800000u));
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
        const auto require_raw_storage=[](const auto& image){
            if(image.layout.format!=DXGI_FORMAT_R32_UINT||
               image.texture.handle->GetDesc().Format!=DXGI_FORMAT_R32_TYPELESS||
               image.storage_samples.size()!=image.desc.samples||image.Uav(0).Format!=DXGI_FORMAT_R32_UINT)
                throw std::runtime_error("Raw RGBA8 did not retain R32 native/storage views");
            for(const auto& plane:image.storage_samples)
                if(plane.handle->GetDesc().Format!=DXGI_FORMAT_R32_TYPELESS)
                    throw std::runtime_error("Raw RGBA8 storage plane changed its allocation family");
        };
        if(raw_source)require_raw_storage(source);
        if(raw_destination&&!raw_source){
            auto invalid=desc;invalid.native_raw32=true;
            bool rejected{};
            try{transfer.Prepare(source.texture,gpu,invalid,LayoutGuestTexture(invalid),0,0,false);}
            catch(const std::invalid_argument& error){
                rejected=strcmp(error.what(),"Native raw RGBA8 MSAA needs R32 backing and layout")==0;
            }
            if(!rejected)throw std::runtime_error("Raw RGBA8 accepted an ordinary RGBA allocation");
        }
        const auto fp=GuestMultisample::Footprint(desc,layout);
        const auto linear_bytes=GuestMultisample::LinearBytes(desc,fp);
        writes.reserve(desc.layers);writer_leases.reserve(desc.layers);
        for(UINT layer=0;layer<desc.layers;++layer){
            std::vector<Component> values(old_pixels.size());
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
                for(UINT component=0;component<components;++component){
                    auto value=0x3e000000u|(component<<21)|(layer<<19)|(sample<<15)|(y<<8)|x|
                        (number_format==5?0x80000000u:0u);
                    // Integer transfer must preserve these float encodings as
                    // bits too: negative zero, NaN payloads and subnormals.
                    static constexpr u32 edge_bits[]{0x80000000u,0x7fc12345u,0xff812345u,0x00000001u};
                    if(components>1&&sample==0&&y==0&&x<4)value=edge_bits[(x+component)%4]^(layer?0x80000000u:0u);
                    if constexpr(sizeof(Component)==2){
                        value=0x1000u|(component<<13)|(layer<<11)|(sample<<8)|(y<<4)|x|
                            (number_format==5?0x8000u:0u);
                        // Include both SNORM encodings of -1, UNORM limits,
                        // half-float NaN payloads and a subnormal. Passing
                        // through a normalized/float view would lose raw bits.
                        static constexpr u16 edge16[]{0x8000u,0x8001u,0x7fffu,0xffffu,0x7e55u,0x7d23u,0x0001u};
                        if(sample==0&&y==0)value=edge16[(x+component)%7]^(layer?0x8000u:0u);
                    }
                    if constexpr(sizeof(Component)==1){
                        value=(component*37u+layer*53u+sample*17u+y*7u+x*3u)&255u;
                        // SNORM -1 has two raw encodings. Midrange sRGB values
                        // distinguish an exact UINT transfer from a gamma pass.
                        static constexpr u8 edge8[]{0x80u,0x81u,0x7fu,0xffu,0x00u,0x0au,0xbcu};
                        if(sample==0&&y==0)value=edge8[(x+component)%7]^(layer?0x80u:0u);
                    }
                    values[((sample*desc.height+y)*desc.width+x)*components+component]=static_cast<Component>(value);
                }
            EncodeGuestSubresource(desc,layout,expected_private,std::as_bytes(std::span(values)),0,layer);
            writes.push_back(Buffer(device,linear_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            auto& buffer=writes.back();Check(buffer.handle->Map(0,nullptr,&mapped));memset(mapped,0xdc,size_t(linear_bytes));
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y)
                memcpy(static_cast<std::byte*>(mapped)+(sample*desc.height+y)*fp.Footprint.RowPitch,
                    values.data()+(sample*desc.height+y)*desc.width*components,desc.width*layout.bytes_per_pixel);
            buffer.handle->Unmap(0,nullptr);
            writer_leases.push_back(transfer.Prepare(source.texture,buffer,desc,layout,layer,0,true));
            writer_leases.back()->Record(renderer.RecordingList(),source.texture,buffer);
        }
        if(!cache.Own(source))throw std::runtime_error("MSAA source ownership acquisition failed");
        if(pressure){
            const auto native=source.texture.handle.Get();const auto uid=source.image_uid;
            const auto allocation=source.allocation_bytes;
            if(cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Pending MSAA storage was retired");
            cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            if(!source.fence||cache.RetireMultisampleStorage(source,source.fence-1))
                throw std::runtime_error("MSAA storage ignored its completion fence");
            auto pin=cache.PinBinding(source);
            if(cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Pinned MSAA storage was retired");
            pin.reset();
            auto retained=source.storage_samples.at(0).allocation;
            if(cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Leased MSAA sample allocation was retired");
            retained.reset();
            if(!cache.RetireMultisampleStorage(source,renderer.CompletedFence())||
               !source.storage_retired||!source.storage_samples.empty()||source.storage_linear.handle||
               source.allocation_bytes>=allocation||source.texture.handle.Get()!=native||source.image_uid!=uid||
               !source.gpu_owned||!source.buffer_alias_version)
                throw std::runtime_error("MSAA pressure retirement changed its authoritative image domain");
            // Preparation lazily recreates every sample plane. Its normal read
            // pass must seed those new allocations from native MSAA, preserving
            // the distinct per-sample/per-layer bits written above.
            auto storage=cache.PrepareMultisampleStorage(source);
            if(source.storage_retired||source.storage_samples.size()!=samples||!source.storage_linear.handle||
               source.allocation_bytes!=allocation||source.texture.handle.Get()!=native||source.image_uid!=uid)
                throw std::runtime_error("MSAA storage restore changed allocation accounting or native image");
            storage->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,false);
            storage->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,true);
            cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            if(cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Externally retained MSAA storage transfer lost its allocation");
            storage.reset();
            if(!cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Completed MSAA storage could not be retired a second time");
            // The following existing private retile/alias conversion verifies
            // reconstructed bits, untouched tiled padding and physical memory.
        }
        if(samples==2&&number_format==4){
            auto mismatch=sampled;mismatch.samples=4;
            std::vector<std::byte> invalid_bytes(LayoutGuestTexture(mismatch).guest_bytes);
            bool rejected{};
            try{cache.Upload(mismatch,invalid_bytes,renderer.RecordingList(),renderer.CompletedFence());}
            catch(const std::runtime_error& error){
                rejected=strcmp(error.what(),"Independent MSAA image transfer needs matching raw-component sample domains")==0;
            }
            if(!rejected||transfers||!source.gpu_owned)
                throw std::runtime_error("MSAA sample-count mismatch changed its independent owner");
        }
        const auto source_uid=source.image_uid;
        auto& converted=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
        if(raw_destination)require_raw_storage(converted);
        const bool ownership=transfers==1&&converted.image_uid!=source_uid&&converted.gpu_owned&&
            converted.buffer_alias_version&&!source.gpu_owned;
        const auto result_fp=GuestMultisample::Footprint(sampled,sampled_layout);
        if(sampled_layout.format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB&&
           result_fp.Footprint.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)
            throw std::runtime_error("MSAA sRGB transfer did not use its linear footprint");
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
                    expected.data()+size_t(sample*desc.height+y)*desc.width*sampled_layout.bytes_per_pixel,
                    desc.width*sampled_layout.bytes_per_pixel)==0;
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
        renderer.ValidateDebugMessages();printf("MSAA independent transfer data_format=%u samples=%u number_format=%u writable=%u pressure=%u raw=%u/%u layers=2 exact\n",data_format,samples,number_format,unsigned(writable),unsigned(pressure),unsigned(raw_source),unsigned(raw_destination));
    }catch(...){renderer.Finish();throw;}
}
int main(int argc,char** argv){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");auto device=CreateHardwareDevice();
    if(argc==2&&strcmp(argv[1],"--msaa")==0){
        for(const UINT samples:{2u,4u,8u})for(const UINT number_format:{4u,5u,7u})for(const bool writable:{false,true})
            RunMultisample(device.Get(),samples,number_format,writable);
        puts("PASS: independent R32 MSAA samples/layers, exact private padding, unchanged physical domain and ownership");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-pressure")==0){
        RunMultisample(device.Get(),2,4,false,true);
        RunMultisample(device.Get(),4,5,true,true);
        RunMultisample(device.Get(),8,7,true,true);
        puts("PASS: independent MSAA scratch retirement, lazy restore, fences, pins, leases and exact domains");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-vector32")==0){
        GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=13;rejected.number_format=7;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("Expanded RGB32 acquired raw-component MSAA ownership");
        rejected.data_format=14;rejected.samples=16;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("16-sample image acquired unsupported MSAA ownership");
        for(const UINT data:{11u,14u})for(const bool writable:{false,true}){
            RunMultisample(device.Get(),2,4,writable,writable,data);
            RunMultisample(device.Get(),4,5,writable,writable,data);
            RunMultisample(device.Get(),8,7,writable,writable,data);
        }
        puts("PASS: independent RG32/RGBA32 raw sample components, pressure restore and unchanged physical domain");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-vector16")==0){
        GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=9;rejected.number_format=0;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("Packed image acquired unsupported raw-component MSAA ownership");
        rejected.data_format=2;rejected.number_format=0;rejected.depth_target=true;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("Depth image acquired color MSAA ownership");
        for(const UINT data:{2u,5u,12u}){
            RunMultisample<u16>(device.Get(),2,0,false,false,data);
            RunMultisample<u16>(device.Get(),4,1,true,true,data);
            RunMultisample<u16>(device.Get(),2,4,false,false,data);
            RunMultisample<u16>(device.Get(),4,5,true,true,data);
            RunMultisample<u16>(device.Get(),8,7,true,true,data);
        }
        puts("PASS: independent R16/RG16/RGBA16 raw bits, odd row padding, pressure restore and unchanged physical domain");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-vector8")==0){
        GuestTextureDesc packed;packed.samples=2;packed.data_format=10;packed.number_format=4;
        packed.writable=true;packed.native_raw32=true;
        const auto packed_layout=LayoutGuestTexture(packed);
        if(!packed.UsesRaw32()||packed_layout.format!=DXGI_FORMAT_R32_UINT||
           !GuestTextureCache::CanTransferIndependentMultisample(packed,packed_layout)||
           !GuestTextureCache::CanUploadGpuSnapshot(packed,packed_layout,true))
            throw std::runtime_error("R32-backed RGBA8 lost its exact word transfer");
        auto mismatched=packed_layout;mismatched.format=DXGI_FORMAT_R8G8B8A8_UINT;
        if(GuestTextureCache::CanTransferIndependentMultisample(packed,mismatched)||
           GuestTextureCache::CanUploadGpuSnapshot(packed,mismatched,true))
            throw std::runtime_error("Raw RGBA8 accepted a non-R32 transfer layout");
        packed.data_format=4;
        if(!GuestTextureCache::CanTransferIndependentMultisample(packed,LayoutGuestTexture(packed)))
            throw std::runtime_error("Raw R32 lost its exact MSAA transfer");
        for(const UINT data:{1u,3u,10u}){
            RunMultisample<u8>(device.Get(),2,0,false,false,data);
            RunMultisample<u8>(device.Get(),4,1,true,true,data);
            RunMultisample<u8>(device.Get(),8,4,false,false,data);
            RunMultisample<u8>(device.Get(),4,5,true,true,data);
        }
        RunMultisample<u8>(device.Get(),2,5,false,false,10);
        RunMultisample<u8>(device.Get(),2,9,false,false,10);
        RunMultisample<u8>(device.Get(),8,9,true,true,10);
        puts("PASS: independent R8/RG8/RGBA8 raw bytes, sRGB without gamma, byte tails and pressure restore");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-rawrgba8")==0){
        RunMultisample<u8>(device.Get(),2,4,false,true,10,true,false);
        RunMultisample<u8>(device.Get(),4,5,true,true,10,true,false);
        RunMultisample<u8>(device.Get(),8,1,true,true,10,true,true);
        RunMultisample<u8>(device.Get(),2,5,true,false,10,false,true);
        RunMultisample<u8>(device.Get(),4,1,true,false,10,false,true);
        RunMultisample<u8>(device.Get(),8,4,true,false,10,false,true);
        puts("PASS: raw RGBA8 MSAA DWORD upload/readback, typed reinterpretation and pressure restore");return 0;
    }
    if(argc!=1)throw std::invalid_argument("Expected no arguments, --msaa, --msaa-pressure, --msaa-vector32, --msaa-vector16, --msaa-vector8 or --msaa-rawrgba8");
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
