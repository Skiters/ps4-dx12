// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "common/scope_exit.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_operations.h"
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "msaa_depth_initialization_fixture.h"
#include "independent_depth_fixture.h"
#include "requested_sample_backing_fixture.h"
#include "sample_backing_pressure_fixture.h"
#include "robust_sample_copy_fixture.h"
#include "paired_depth_snapshot_fixture.h"
#include "independent_depth_growth_fixture.h"
#include "depth_subview_fixture.h"
#include "dirty_cached_depth_fixture.h"
#include "dirty_owned_depth_fixture.h"
#include "required_format_device.h"
#include "packed19_msaa_attachment_fixture.h"
#include "readonly_numeric_views_fixture.h"
#include "color_to_ms_depth_fixture.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <cstring>
#include <cstdio>
#include <cmath>
using namespace NativeDX12;
struct RawMultisampleClear {
    float color[4];
    u8 packed[4];
};
template<class Component=u32>
void RunMultisample(ID3D12Device* device,UINT samples,UINT number_format,bool writable,bool pressure=false,UINT data_format=4,
    bool raw_source=false,bool raw_destination=false,const RawMultisampleClear* clear=nullptr,UINT view_data=0){
    Rasterizer renderer(device);GuestTextureCache cache(device);GuestMultisample transfer(device);
    GuestTextureDesc desc;desc.address=0x40000;desc.width=7;desc.height=5;desc.pitch=8;desc.layers=2;
    desc.tile_mode=13;desc.data_format=data_format;desc.number_format=number_format;desc.samples=samples;desc.writable=data_format!=13;
    desc.native_raw32=raw_source&&data_format==10;
    desc.native_packed10_raw=raw_source&&data_format==9;
    const bool packed16=data_format>=16&&data_format<=19;
    if(clear){
        const bool raw_word=raw_source&&((data_format==10&&sizeof(Component)==1)||
            (data_format==9&&sizeof(Component)==4));
        if(!pressure||(!raw_word&&!(packed16&&sizeof(Component)==2)))
            throw std::invalid_argument("Converted clear fixture needs raw guest words and pressure checks");
        // HLE clears cover the complete image even when a narrow view first
        // populated the cache. Both native array layers must change.
        desc.first_layer=1;desc.view_layers=1;
    }
    auto sampled=desc;sampled.writable=writable;sampled.tile_mode=10;
    if(view_data)sampled.data_format=view_data;
    sampled.native_raw32=raw_destination&&data_format==10;
    sampled.native_packed10_raw=raw_destination&&data_format==9;
    if((raw_source||raw_destination)&&((data_format!=9&&data_format!=10)||(raw_destination&&!writable)))
        throw std::invalid_argument("Raw packed fixture needs data9/10 and a writable raw destination");
    sampled.number_format=number_format==4?5:number_format==5?7:4;
    if constexpr(sizeof(Component)==2)
        sampled.number_format=number_format==0?1:number_format==1?4:number_format==4?5:number_format==5?7:0;
    if constexpr(sizeof(Component)==1)
        sampled.number_format=number_format==0?1:number_format==1?4:number_format==4?5:
            number_format==5&&data_format==10?9:0;
    if(data_format==9)sampled.number_format=number_format==0?1:number_format==1?4:number_format==4?5:0;
    if(sampled.data_format==6||sampled.data_format==34)sampled.number_format=7;
    if(packed16){
        if(sizeof(Component)!=2||number_format!=0)
            throw std::invalid_argument("Packed16 fixture requires UNORM guest halfword storage");
        sampled.number_format=0;
    }
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
    Resource gpu,upload,physical_readback,private_readback,expanded_version,expanded_readback;
    std::vector<std::byte> expanded_expected;
    std::vector<Resource> writes,linear,readbacks,requested_versions;
    std::vector<std::shared_ptr<GuestMultisample::Lease>> writer_leases,reader_leases;
    std::weak_ptr<GuestPackedTransfer> clear_resources;
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
        // RGB9E5 is a sampled alias, not a guest storage-write format. Require
        // the same separate ownership witness used by the production rasterizer.
        cache.SetBufferAliasRetention([](const auto& owner,const auto&){
            return owner.writable&&owner.data_format!=13&&owner.data_format!=34;});
        cache.SetIndependentImageRetention([](const auto& owner,const auto&){
            return owner.data_format!=34&&(owner.data_format!=13||!owner.writable);});
        const auto acquire=[&](auto& image){
            const auto owner=DescribeGuestImageOwnership(image);
            if(!owner||!cache.CanRetainIndependentImage(owner->desc,owner->layout))return false;
            image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
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
                throw std::runtime_error("Raw packed DWORD did not retain R32 native/storage views");
            for(const auto& plane:image.storage_samples)
                if(plane.handle->GetDesc().Format!=DXGI_FORMAT_R32_TYPELESS)
                    throw std::runtime_error("Raw packed DWORD storage plane changed its allocation family");
        };
        if(raw_source)require_raw_storage(source);
        if(raw_destination&&!raw_source){
            auto invalid=desc;
            if(data_format==9)invalid.native_packed10_raw=true;else invalid.native_raw32=true;
            bool rejected{};
            try{transfer.Prepare(source.texture,gpu,invalid,LayoutGuestTexture(invalid),0,0,false);}
            catch(const std::invalid_argument& error){
                rejected=strcmp(error.what(),data_format==9?
                    "Native raw packed MSAA needs its exact R32 word layout":
                    "Native raw RGBA8 MSAA needs R32 backing and layout")==0;
            }
            if(!rejected)throw std::runtime_error("Raw packed DWORD accepted an ordinary packed allocation");
        }
        if(packed16&&(source.layout.bytes_per_pixel!=2||source.layout.HostBytesPerPixel()!=4||
           source.texture.handle->GetDesc().Format!=DXGI_FORMAT_R32_UINT||source.Uav(0).Format!=DXGI_FORMAT_R32_UINT))
            throw std::runtime_error("Packed16 did not use one low guest halfword per R32 plane texel");
        if(data_format==13){
            auto invalid_layout=layout;invalid_layout.bytes_per_pixel=16;
            bool rejected{};
            try{transfer.Prepare(source.texture,gpu,desc,invalid_layout,0,0,false);}
            catch(const std::invalid_argument& error){
                rejected=strcmp(error.what(),"Native RGB32 transfer needs 12-byte guest words and typeless RGBA32 backing")==0;}
            if(!rejected)throw std::runtime_error("RGB32 transfer accepted RGBA32 guest geometry");
        }
        if(data_format==6){
            if(!source.texture.AllowsCast(DXGI_FORMAT_R11G11B10_FLOAT)||
               !source.texture.AllowsCast(DXGI_FORMAT_R32_UINT))
                throw std::runtime_error("Packed-float source lost its actual typed/raw view list");
            for(const auto only_view:{DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R11G11B10_FLOAT}){
                auto missing_view=source.texture;
                missing_view.castable_formats=std::make_shared<const std::vector<DXGI_FORMAT>>(
                    std::initializer_list<DXGI_FORMAT>{only_view});
                bool rejected{};
                try{transfer.Prepare(missing_view,gpu,desc,layout,0,0,false);}
                catch(const std::runtime_error& error){
                    rejected=strcmp(error.what(),"Native packed-float transfer needs declared typed and R32 UINT views")==0;}
                if(!rejected)throw std::runtime_error("Packed-float transfer trusted an incomplete cast list");
            }
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
                    if(data_format==6||sampled.data_format==34){
                        // R11 special payloads and RGB9E5 zero mantissas with
                        // distinct shared exponents must remain different words.
                        // A numeric load/store round trip would canonicalize them.
                        static constexpr u32 packed_float_edges[]{0x00000000u,0x08000000u,
                            0xf8000000u,0x000007c1u,0x003e0801u,0x7fffff80u,0xffffffffu};
                        value=packed_float_edges[x%7]^((layer*19u+sample*43u+y*11u)<<11);
                    }
                    if(data_format==9){
                        // Both SNORM -1 encodings (10-bit and two-bit alpha),
                        // signed endpoints and arbitrary packed words must keep
                        // their exact bits through raw and native UINT views.
                        static constexpr u32 packed_edges[]{0x20080200u,0x20180601u,0x7fffffffu,
                            0x80000000u,0xc0000000u,0xffffffffu,0x00000001u};
                        value=packed_edges[x%7]^((layer*17u+sample*31u+y*7u)<<10);
                    }
                    values[((sample*desc.height+y)*desc.width+x)*components+component]=static_cast<Component>(value);
                }
            EncodeGuestSubresource(desc,layout,expected_private,std::as_bytes(std::span(values)),0,layer);
            writes.push_back(Buffer(device,linear_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            auto& buffer=writes.back();Check(buffer.handle->Map(0,nullptr,&mapped));memset(mapped,0xdc,size_t(linear_bytes));
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y){
                auto* row=static_cast<std::byte*>(mapped)+(sample*desc.height+y)*fp.Footprint.RowPitch;
                const auto* pixels=values.data()+(sample*desc.height+y)*desc.width*components;
                if(packed16){
                    for(UINT x=0;x<desc.width;++x){
                        // The high half is host-only storage. Retile must discard
                        // it rather than corrupt the adjacent guest halfword.
                        const u32 word=u32(pixels[x])|0x5a5a0000u;
                        memcpy(row+x*4,&word,4);
                    }
                }else if(data_format==13){
                    // Internal UINT4 transfer seeds a readonly RGB32 version;
                    // this is not a guest IMAGE_WRITE13. Alpha is host-only.
                    for(UINT x=0;x<desc.width;++x){
                        memcpy(row+x*16,pixels+x*3,12);
                        const u32 poison=0xdeadc0deu;memcpy(row+x*16+12,&poison,4);
                    }
                }else memcpy(row,pixels,desc.width*layout.bytes_per_pixel);
            }
            buffer.handle->Unmap(0,nullptr);
            writer_leases.push_back(transfer.Prepare(source.texture,buffer,desc,layout,layer,0,true));
            writer_leases.back()->Record(renderer.RecordingList(),source.texture,buffer);
        }
        if(!cache.Own(source))throw std::runtime_error("MSAA source ownership acquisition failed");
        if(clear){
            if(!GuestConvertedMultisampleClearCompatible(source,clear->color)||
               GuestMultisampleStorageClearCompatible(source,clear->color))
                throw std::runtime_error("Nonzero packed word did not select the converted MSAA clear");
            const auto native=source.texture.handle.Get();const auto uid=source.image_uid;
            const auto allocation=source.allocation_bytes;
            const auto reject=[&](float (&invalid)[4]){
                if(GuestConvertedMultisampleClearCompatible(source,invalid))
                    throw std::runtime_error("Converted MSAA clear accepted an inexact numeric value");
                bool rejected{};
                try{cache.ClearRawStorageImage(source,invalid,renderer.RecordingList(),renderer.CompletedFence());}
                catch(const std::invalid_argument& error){
                    rejected=strcmp(error.what(),"Incompatible converted guest storage image clear")==0;
                }
                if(!rejected||!source.format_transfers.empty()||source.format_retirement_bytes||
                   source.allocation_bytes!=allocation||source.texture.handle.Get()!=native||source.image_uid!=uid)
                    throw std::runtime_error("Rejected MSAA clear mutated the independent image");
            };
            float invalid[4];memcpy(invalid,clear->color,sizeof(invalid));
            if(number_format==4){const auto maximum=data_format==9?1023u:255u;
                invalid[0]=std::bit_cast<float>(maximum+1);reject(invalid);
                invalid[0]=std::bit_cast<float>(UINT32_MAX);reject(invalid);
                if(data_format==9){memcpy(invalid,clear->color,sizeof(invalid));
                    invalid[3]=std::bit_cast<float>(4u);reject(invalid);}}
            else if(number_format==5){const int32_t limit=data_format==9?512:128;
                invalid[0]=std::bit_cast<float>(limit);reject(invalid);
                invalid[0]=std::bit_cast<float>(-limit-1);reject(invalid);
                if(data_format==9){memcpy(invalid,clear->color,sizeof(invalid));
                    invalid[3]=std::bit_cast<float>(int32_t(2));reject(invalid);
                    invalid[3]=std::bit_cast<float>(int32_t(-3));reject(invalid);}}
            else{invalid[0]=std::bit_cast<float>(0x7fc12345u);reject(invalid);
                invalid[0]=std::bit_cast<float>(0x7f800000u);reject(invalid);}
            cache.ClearRawStorageImage(source,clear->color,renderer.RecordingList(),renderer.CompletedFence());
            if(source.format_transfers.size()!=1||!source.format_retirement_bytes||
               source.allocation_bytes!=allocation+source.format_retirement_bytes||
               source.texture.handle.Get()!=native||source.image_uid!=uid||!source.pending||
               !source.gpu_owned||!source.buffer_alias_version)
                throw std::runtime_error("Converted MSAA clear lost resources, accounting or image ownership");
            clear_resources=source.format_transfers.front();
            cache.Collect(renderer.CompletedFence());
            if(clear_resources.expired()||!source.format_retirement_bytes)
                throw std::runtime_error("Pending MSAA clear released conversion resources");
            std::vector<Component> values(old_pixels.size());
            for(size_t i=0;i<values.size();++i)
                memcpy(&values[i],clear->packed+(i%components)*sizeof(Component),sizeof(Component));
            for(UINT layer=0;layer<desc.layers;++layer)
                EncodeGuestSubresource(desc,layout,expected_private,std::as_bytes(std::span(values)),0,layer);
            // A shader binding must refresh every previously stale sample plane
            // from the cleared native image before a following storage writeback.
            auto storage=cache.PrepareMultisampleStorage(source);
            storage->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,false);
            storage->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,true);
        }
        if(pressure){
            const auto native=source.texture.handle.Get();const auto uid=source.image_uid;
            const auto allocation=source.allocation_bytes-source.format_retirement_bytes;
            if(cache.RetireMultisampleStorage(source,renderer.CompletedFence()))
                throw std::runtime_error("Pending MSAA storage was retired");
            cache.Submitted(renderer.Flush());
            if(clear){
                if(!source.fence)throw std::runtime_error("Converted MSAA clear was not submitted");
                cache.Collect(source.fence-1);
                if(clear_resources.expired()||!source.format_retirement_bytes)
                    throw std::runtime_error("Uncompleted MSAA clear released descriptors or allocations");
            }
            renderer.Finish();cache.Collect(renderer.CompletedFence());
            if(clear&&(!clear_resources.expired()||source.format_retirement_bytes||
               source.allocation_bytes!=allocation))
                throw std::runtime_error("Completed MSAA clear did not release its temporary allocations");
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
        if(clear&&data_format==10){
            // Converting a retired raw owner into ordinary sampled storage must
            // clear the old lazy-restore marker even though no planes are needed.
            auto ordinary=desc;ordinary.writable=false;ordinary.native_raw32=false;
            const auto uid=source.image_uid;
            auto& sampled_backing=cache.Upload(ordinary,physical,renderer.RecordingList(),renderer.CompletedFence());
            if(&sampled_backing!=&source||sampled_backing.image_uid!=uid||sampled_backing.storage_retired||
               !sampled_backing.storage_samples.empty()||sampled_backing.storage_linear.handle||
               sampled_backing.desc.writable||transfers)
                throw std::runtime_error("Packed conversion retained the retired writable sample domain");
            auto& raw_backing=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
            if(&raw_backing!=&source||raw_backing.image_uid!=uid||raw_backing.storage_retired||
               raw_backing.storage_samples.size()!=samples||!raw_backing.storage_linear.handle||transfers)
                throw std::runtime_error("Packed conversion did not publish replacement sample storage");
            require_raw_storage(raw_backing);
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
        if(sampled.data_format==6||sampled.data_format==34){
            if(!converted.texture.AllowsCast(sampled_layout.format)||
               !converted.texture.AllowsCast(DXGI_FORMAT_R32_UINT))
                throw std::runtime_error("Packed-float snapshot lost typed/raw resource cast lists");
            if(sampled.data_format==34){
                auto invalid_usage=sampled;invalid_usage.writable=true;
                if(GuestTextureCache::CanTransferIndependentMultisample(invalid_usage,sampled_layout))
                    throw std::runtime_error("RGB9E5 transfer admitted guest storage writes");
                invalid_usage=sampled;invalid_usage.render_target=true;
                if(GuestTextureCache::CanTransferIndependentMultisample(invalid_usage,sampled_layout))
                    throw std::runtime_error("RGB9E5 transfer admitted guest attachment writes");
                const auto owner=DescribeGuestImageOwnership(converted);
                if(!owner||owner->desc.data_format!=4||owner->desc.number_format!=4||
                   !owner->desc.writable||converted.desc.writable||converted.desc.data_format!=34)
                    throw std::runtime_error("RGB9E5 ownership changed actual sampled image usage");
                auto mismatched=converted.layout;mismatched.mips[0].pitch+=1;
                const auto original=converted.layout;converted.layout=std::move(mismatched);
                const bool rejected=!DescribeGuestImageOwnership(converted);
                converted.layout=original;
                if(!rejected)throw std::runtime_error("Ownership accepted a mismatched guest mip layout");
            }
        }
        if(sampled.data_format==13){
            auto invalid_usage=sampled;invalid_usage.writable=true;
            if(GuestTextureCache::CanTransferIndependentMultisample(invalid_usage,sampled_layout))
                throw std::runtime_error("RGB32 independent transfer admitted guest storage writes");
            invalid_usage=sampled;invalid_usage.render_target=true;
            if(GuestTextureCache::CanTransferIndependentMultisample(invalid_usage,sampled_layout))
                throw std::runtime_error("RGB32 independent transfer admitted guest attachment writes");
            const auto owner=DescribeGuestImageOwnership(converted);
            if(!owner||owner->desc.data_format!=13||owner->desc.writable||
               !cache.CanRetainIndependentImage(owner->desc,owner->layout)||
               cache.CanRetainBufferAlias(owner->desc,owner->layout)||
               converted.layout.bytes_per_pixel!=12||converted.layout.HostBytesPerPixel()!=16||
               converted.expanded_guest_backing.handle||converted.expanded_written_mask.handle||
               !converted.storage_samples.empty())
                throw std::runtime_error("RGB32 readonly ownership changed its physical/component domain");
            const auto srv=converted.Srv();
            if(D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(3,srv.Shader4ComponentMapping)!=
               D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1)
                throw std::runtime_error("RGB32 sampled alpha did not use guest constant one");
        }
        const bool ownership=transfers==1&&converted.image_uid!=source_uid&&converted.gpu_owned&&
            converted.buffer_alias_version&&!source.gpu_owned;
        const bool expanded=sampled_layout.expanded_bytes_per_pixel==16;
        const bool exact_roundtrip=expanded||(packed16&&!sampled.writable)||
            sampled.data_format==6||sampled.data_format==13||sampled.data_format==34;
        if(exact_roundtrip){
            if(expanded&&(!converted.expanded_guest_backing.handle||!converted.expanded_written_mask.handle||
               !converted.expanded_backing_bytes||!converted.tiler))
                throw std::runtime_error("Expanded snapshot discarded exact packed backing or mask");
            expanded_expected=physical;
            for(UINT layer=0;layer<sampled.layers;++layer){
                const auto words=DecodeGuestSubresource(sampled,sampled_layout,expected_private,0,layer);
                EncodeGuestSubresource(sampled,sampled_layout,expanded_expected,words,0,layer);
            }
            expanded_version=cache.RetileBufferVersion(converted,gpu,renderer.RecordingList(),renderer.CompletedFence());
            expanded_readback=Buffer(device,sampled_layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            expanded_version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(expanded_readback.handle.Get(),0,expanded_version.handle.Get(),0,sampled_layout.guest_bytes);
        }
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
            for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<desc.height;++y){
                const auto* row=static_cast<const std::byte*>(mapped)+(sample*desc.height+y)*result_fp.Footprint.RowPitch;
                const auto* pixels=expected.data()+size_t(sample*desc.height+y)*desc.width*sampled_layout.bytes_per_pixel;
                if(expanded){
                    for(UINT x=0;x<desc.width;++x){u32 guest{};
                        memcpy(&guest,pixels+x*sampled_layout.bytes_per_pixel,sampled_layout.bytes_per_pixel);
                        for(UINT component=0;component<4;++component){
                            if(data_format==19){
                                // Guest format19 is R4/G8/B12/A0; the expanded
                                // typed image keeps logical RGBA, not nibble order.
                                const UINT shift=component==3?0:(component+1)*4;
                                const float expected_value=float((guest>>shift)&15)/15.f;
                                float actual{};memcpy(&actual,row+x*16+component*4,4);
                                equal&=std::isfinite(actual)&&std::abs(actual-expected_value)<=0.000001f;
                                continue;
                            }
                            const UINT bits=component==3?2:10,raw=(guest>>(component*10))&((1u<<bits)-1);
                            const int32_t expected_value=int32_t(raw)-((raw&(1u<<(bits-1)))?int32_t(1u<<bits):0);
                            u32 actual{};memcpy(&actual,row+x*16+component*4,4);
                            if(sampled.number_format==5)equal&=actual==std::bit_cast<u32>(expected_value);
                            else{const float expected_value_f=std::max(float(expected_value)/float((1u<<(bits-1))-1),-1.f);
                                const float actual_f=std::bit_cast<float>(actual);
                                equal&=std::isfinite(actual_f)&&std::abs(actual_f-expected_value_f)<=0.000001f;}
                        }
                    }
                }else if(packed16){
                    for(UINT x=0;x<desc.width;++x){u16 guest{};u32 actual{};
                        memcpy(&guest,pixels+x*2,2);
                        if(sampled.writable){memcpy(&actual,row+x*4,4);equal&=actual==guest;}
                        else{memcpy(&actual,row+x*2,2);
                            const u16 native=data_format==18?u16((guest>>1)|(guest<<15)):guest;
                            equal&=actual==native;}
                    }
                }else if(sampled.data_format==13){
                    for(UINT x=0;x<desc.width;++x){
                        equal&=memcmp(row+x*16,pixels+x*12,12)==0;
                        u32 alpha{};memcpy(&alpha,row+x*16+12,4);
                        equal&=alpha==(sampled.number_format==7?0x3f800000u:1u);
                    }
                }else equal&=memcmp(row,pixels,desc.width*sampled_layout.bytes_per_pixel)==0;
            }
            readbacks[layer].handle->Unmap(0,nullptr);
            if(!equal)throw std::runtime_error("MSAA reinterpret lost sample/layer/format bits");
        }
        if(exact_roundtrip){
            Check(expanded_readback.handle->Map(0,nullptr,&mapped));
            const bool exact=memcmp(mapped,expanded_expected.data(),size_t(sampled_layout.guest_bytes))==0;
            expanded_readback.handle->Unmap(0,nullptr);
            if(!exact)throw std::runtime_error("Expanded MSAA snapshot lost original packed words or padding");
            // After transient upload leases retire, merge into a fresh physical
            // version with a different padding sentinel. Retile must copy only
            // logical words, using retained coverage for expanded images or new
            // coverage for native packed16; include duplicate SNORM -1 encodings.
            std::vector<std::byte> seed(size_t(sampled_layout.guest_bytes),std::byte{0x3c});
            auto expected=seed;
            for(UINT layer=0;layer<sampled.layers;++layer){
                const auto words=DecodeGuestSubresource(sampled,sampled_layout,expected_private,0,layer);
                EncodeGuestSubresource(sampled,sampled_layout,expected,words,0,layer);
            }
            writes.push_back(Buffer(device,seed.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ));
            Check(writes.back().handle->Map(0,nullptr,&mapped));memcpy(mapped,seed.data(),seed.size());
            writes.back().handle->Unmap(0,nullptr);
            requested_versions.push_back(Buffer(device,seed.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST));
            auto& fresh=requested_versions.back();
            renderer.RecordingList()->CopyBufferRegion(fresh.handle.Get(),0,writes.back().handle.Get(),0,seed.size());
            auto second=cache.RetileBufferVersion(converted,fresh,renderer.RecordingList(),renderer.CompletedFence());
            requested_versions.push_back(second);
            second.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            renderer.RecordingList()->CopyBufferRegion(expanded_readback.handle.Get(),0,second.handle.Get(),0,seed.size());
            cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            Check(expanded_readback.handle->Map(0,nullptr,&mapped));
            const bool repeated=memcmp(mapped,expected.data(),expected.size())==0;
            expanded_readback.handle->Unmap(0,nullptr);
            if(!repeated)throw std::runtime_error("Completed expanded snapshot lost exact words or copied stale padding");
        }
        Check(private_readback.handle->Map(0,nullptr,&mapped));
        const bool private_exact=memcmp(mapped,expected_private.data(),size_t(layout.guest_bytes))==0;
        private_readback.handle->Unmap(0,nullptr);
        Check(physical_readback.handle->Map(0,nullptr,&mapped));
        const bool unchanged=memcmp(mapped,physical.data(),physical.size())==0;physical_readback.handle->Unmap(0,nullptr);
        if(!private_exact)throw std::runtime_error("MSAA private retile changed padding or sample layout");
        if(!unchanged)throw std::runtime_error("MSAA transfer changed physical buffer domain/padding");
        if(!ownership)throw std::runtime_error("MSAA logical ownership transfer failed");
        renderer.ValidateDebugMessages();printf("MSAA independent transfer data_format=%u view_data=%u samples=%u number_format=%u writable=%u pressure=%u raw=%u/%u clear=%u layers=2 exact\n",data_format,sampled.data_format,samples,number_format,unsigned(writable),unsigned(pressure),unsigned(raw_source),unsigned(raw_destination),unsigned(clear!=nullptr));
    }catch(...){renderer.Finish();throw;}
}
void RunReadonlyMultisampleWordStorage(ID3D12Device* device,UINT samples){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    Rasterizer renderer(device);GuestTextureCache cache(device);PlacedTextureAllocator allocator(device);
    GuestTextureDesc desc;desc.address=0x40000;desc.width=7;desc.height=5;desc.pitch=8;
    desc.layers=desc.view_layers=2;desc.tile_mode=13;desc.samples=samples;
    desc.data_format=34;desc.number_format=7;desc.native_raw32=true;
    const auto layout=LayoutGuestTexture(desc);
    std::vector<std::byte> physical(layout.guest_bytes,std::byte{0xa7}),expected;
    for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<samples;++sample)
    for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
        const u32 word=0x13570000u|(layer<<12)|(sample<<8)|(y<<4)|x;
        memcpy(physical.data()+GuestPixelOffset(desc,layout,0,layer,x,y,sample),&word,4);
    }
    expected=physical;
    Resource gpu,upload,readback;
    try{
        gpu=Buffer(device,physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        upload=Buffer(device,physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        readback=Buffer(device,physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* cpu{};Check(upload.handle->Map(0,nullptr,&cpu));memcpy(cpu,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(gpu.handle.Get(),0,upload.handle.Get(),0,physical.size());
        cache.SetIndependentImageRetention([](const auto& d,const auto&){return d.data_format==4&&d.writable;});
        cache.SetOwnershipAcquirer([&](auto& image){const auto owner=DescribeGuestImageOwnership(image);
            if(!owner||!CanRetainGuestImageOwnership(cache,*owner))return false;
            image.buffer_alias_version=true;image.SetGpuOwned(true);return true;});
        auto& source=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        const auto uid=source.image_uid;auto* native=source.texture.handle.Get();
        const auto original_bytes=source.allocation_bytes;
        auto writer=desc;writer.data_format=4;writer.number_format=4;writer.native_raw32=false;writer.writable=true;
        auto readonly=writer;readonly.writable=false;
        bool denied=false;try{cache.EnsureMultisampleStorage(source,readonly);}catch(const std::invalid_argument&){denied=true;}
        require(denied&&source.storage_samples.empty()&&!source.storage_linear.handle,
            "Readonly MSAA view invented storage writer authority");
        auto transfer=cache.PrepareMultisampleStorage(source,writer);
        require(source.desc==desc&&source.image_uid==uid&&source.texture.handle.Get()==native&&
            !source.logical_gpu_modified,"Storage preparation changed original readonly class/history");
        denied=false;try{(void)source.Uav(writer,0);}catch(const std::invalid_argument&){denied=true;}
        require(denied,"MSAA base resource acquired an impossible UAV permission");
        { // Genuine native permission/cast/range negatives, not descriptor-only rejection.
            auto bad_view=writer;bad_view.samples=samples==2?4:2;
            denied=false;try{(void)source.StorageUav(bad_view,0);}catch(const std::invalid_argument&){denied=true;}
            require(denied,"MSAA storage admitted a different logical sample count");
            denied=false;try{(void)source.StorageUav(writer,samples);}catch(const std::invalid_argument&){denied=true;}
            require(denied,"MSAA storage admitted an out-of-range plane");
            GuestTextureCache::Entry invalid;invalid.desc=source.desc;invalid.layout=source.layout;
            invalid.texture=source.texture;invalid.storage_samples=source.storage_samples;
            auto shape=invalid.storage_samples[0].handle->GetDesc();shape.Flags=D3D12_RESOURCE_FLAG_NONE;
            invalid.storage_samples[0]=allocator.Allocate(shape,D3D12_RESOURCE_STATE_COMMON);
            denied=false;try{(void)invalid.StorageUav(writer,0);}catch(const std::invalid_argument&){denied=true;}
            require(denied,"MSAA storage plane invented actual UAV permission");
            shape.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;shape.Format=DXGI_FORMAT_R16G16_UINT;
            invalid.storage_samples[0]=allocator.Allocate(shape,D3D12_RESOURCE_STATE_COMMON);
            denied=false;try{(void)invalid.StorageUav(writer,0);}catch(const std::invalid_argument&){denied=true;}
            require(denied,"MSAA storage plane invented a missing native format cast");
        }
        transfer->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,false);
        auto visible=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,samples*2,true);
        auto cpu_views=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,samples*2,false);
        renderer.RetainUntilFinish([visible,cpu_views](u64){(void)visible;(void)cpu_views;});
        for(UINT sample=0;sample<samples;++sample)for(UINT layer=0;layer<2;++layer){
            auto view=writer;view.first_layer=layer;view.view_layers=1;
            const auto uav=source.StorageUav(view,sample);const UINT index=sample*2+layer;
            auto& plane=source.storage_samples[sample];
            device->CreateUnorderedAccessView(plane.handle.Get(),nullptr,&uav,visible->Cpu(index));
            device->CreateUnorderedAccessView(plane.handle.Get(),nullptr,&uav,cpu_views->Cpu(index));
            auto* list=renderer.RecordingList();plane.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            ID3D12DescriptorHeap* heaps[]{visible->Heap()};list->SetDescriptorHeaps(1,heaps);
            const UINT values[]{0x24680000u|(layer<<12)|(sample<<8),0,0,0};
            list->ClearUnorderedAccessViewUint(visible->Gpu(index),cpu_views->Cpu(index),plane.handle.Get(),values,0,nullptr);
            plane.UavBarrier(list);
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
                memcpy(expected.data()+GuestPixelOffset(desc,layout,0,layer,x,y,sample),values,4);
        }
        transfer->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,true);
        require(cache.Own(source),"Written readonly MSAA I lost independent ownership");
        const auto with_storage=source.allocation_bytes;
        require(!cache.RetireMultisampleStorage(source,renderer.CompletedFence()),"Pending readonly MSAA storage retired");
        cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
        require(!cache.RetireMultisampleStorage(source,source.fence-1),"Readonly MSAA storage ignored its fence");
        require(!cache.RetireMultisampleStorage(source,renderer.CompletedFence()),"Live storage transfer lost its plane leases");
        transfer.reset();
        auto pin=cache.PinBinding(source);require(!cache.RetireMultisampleStorage(source,renderer.CompletedFence()),"Pinned readonly MSAA storage retired");pin.reset();
        auto retained=source.storage_samples[0].allocation;
        require(!cache.RetireMultisampleStorage(source,renderer.CompletedFence()),"Bound readonly MSAA plane allocation retired");retained.reset();
        require(cache.RetireMultisampleStorage(source,renderer.CompletedFence())&&source.storage_retired&&
            source.allocation_bytes==original_bytes,"Readonly MSAA storage failed exact retirement accounting");
        transfer=cache.PrepareMultisampleStorage(source,writer);
        require(source.desc==desc&&source.image_uid==uid&&source.texture.handle.Get()==native&&
            source.allocation_bytes==with_storage&&!source.storage_retired,"Readonly MSAA restore changed class/UID/accounting");
        transfer->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,false);
        transfer->Record(renderer.RecordingList(),source.texture,source.storage_samples,source.storage_linear,true);
        GuestImageReadback image_readback(device,source,renderer.RecordingList());
        gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,gpu.handle.Get(),0,physical.size());
        const auto fence=renderer.Flush();cache.Submitted(fence);image_readback.Submitted(fence);renderer.Finish();cache.Collect(renderer.CompletedFence());
        auto observed=physical;image_readback.Resolve(renderer.CompletedFence(),observed);
        require(observed==expected,"Readonly MSAA restored GPU words/layers or full guest padding differ");
        Check(readback.handle->Map(0,nullptr,&cpu));const bool unchanged=!memcmp(cpu,physical.data(),physical.size());D3D12_RANGE none{};readback.handle->Unmap(0,&none);
        require(unchanged,"Readonly MSAA storage changed independent GPU B");
        require(source.desc==desc&&source.image_uid==uid&&source.texture.handle.Get()==native&&source.logical_gpu_modified&&
            source.gpu_owned&&source.buffer_alias_version,"Readonly MSAA write lost original I identity/history/ownership");
        renderer.ValidateDebugMessages();
        printf("PASS: readonly R9 MSAA%u,R32 writer planes,2 layers,real permission/cast negatives,pressure retirement/restore,distinct GPU I/B and padding\n",samples);
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
static void RunCurrentIndependentBinding(ID3D12Device* device){
    Rasterizer renderer(device);GuestTextureCache cache(device);
    SCOPE_EXIT {renderer.Finish();};
    const auto require=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};
    GuestTextureDesc sampled;sampled.address=0x40000;sampled.width=sampled.height=64;
    sampled.pitch=128;sampled.layers=6;sampled.view_layers=6;
    sampled.data_format=5;sampled.number_format=7;sampled.tile_mode=14;
    auto attachment=sampled;attachment.render_target=true;
    const auto layout=LayoutGuestTexture(sampled);
    std::vector<std::byte> old_bytes(layout.guest_bytes,std::byte{0x11});
    std::vector<std::byte> new_bytes(layout.guest_bytes,std::byte{0x22});
    cache.SetOwnershipAcquirer([](auto& image){image.buffer_alias_version=true;
        image.buffer_alias_watch=true;image.SetGpuOwned(true);return true;});
    auto& old=cache.Upload(sampled,old_bytes,renderer.RecordingList(),renderer.CompletedFence());
    auto old_pin=cache.PinBinding(old);
    auto& current=cache.Upload(attachment,new_bytes,renderer.RecordingList(),renderer.CompletedFence());
    require(&current!=&old&&current.texture.handle.Get()!=old.texture.handle.Get(),"Fixture needs distinct sampled/RT resources");
    require(cache.Own(old)&&cache.Own(current),"Fixture cannot publish image owners");
    require(!old.cached_native_current&&current.cached_native_current,"Original image class did not advance");
    // Even an exact old descriptor must bind the current original image class.
    require(cache.FindValidIndependentImage(sampled,layout)==&current,"Exact old sampled image shadowed its current RT version");
    auto view=sampled;view.first_layer=2;view.view_layers=1;
    require(cache.FindIndependentUploadSource(view,layout)==&current,"Subview did not select current original image class");
    current.cached_native_current=false;bool rejected=false;
    try{(void)cache.FindValidIndependentImage(view,layout);}catch(const std::runtime_error&){rejected=true;}
    require(rejected,"Unresolved independent image ambiguity was silently accepted");
    current.cached_native_current=true;current.logical_dirty=true;
    require(cache.FindValidIndependentImage(sampled,layout)==&old,"Dirty current allocation shadowed a valid source");
    current.logical_dirty=false;
    GuestImageReadback old_read(device,old,renderer.RecordingList());
    GuestImageReadback new_read(device,current,renderer.RecordingList());
    const auto fence=renderer.Flush();cache.Submitted(fence);old_read.Submitted(fence);new_read.Submitted(fence);renderer.Finish();
    auto observed_old=old_bytes,observed_new=new_bytes;
    // Poison logical pixels so a skipped readback cannot satisfy the oracle.
    auto expected_old=old_bytes,expected_new=new_bytes;
    std::vector<std::byte> poison(size_t(sampled.width)*sampled.height*layout.bytes_per_pixel,std::byte{0x6d});
    for(UINT layer=0;layer<sampled.layers;++layer){
        EncodeGuestSubresource(sampled,layout,observed_old,poison,0,layer);
        EncodeGuestSubresource(sampled,layout,observed_new,poison,0,layer);
    }
    old_read.Resolve(renderer.CompletedFence(),observed_old);new_read.Resolve(renderer.CompletedFence(),observed_new);
    require(observed_old==expected_old&&observed_new==expected_new,"Current binding lost new contents or mutated pinned old GPU version");
    renderer.ValidateDebugMessages();puts("PASS: current original image class and pinned preceding GPU contents");
}
static void RunColorArraySubviews(ID3D12Device* device){
    Rasterizer renderer(device);GuestTextureCache cache(device);SCOPE_EXIT {renderer.Finish();};
    const auto require=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};
    GuestTextureDesc sampled;sampled.address=0x40000;sampled.width=sampled.height=64;sampled.pitch=128;
    sampled.layers=sampled.view_layers=6;sampled.data_format=5;sampled.number_format=7;sampled.tile_mode=14;
    const auto layout=LayoutGuestTexture(sampled);
    std::vector<std::byte> physical(layout.guest_bytes,std::byte{0x11}),expected=physical;
    cache.SetOwnershipAcquirer([](auto& image){image.buffer_alias_version=true;
        image.buffer_alias_watch=true;image.SetGpuOwned(true);return true;});
    auto& parent=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
    auto pin=cache.PinBinding(parent);require(cache.Own(parent),"Color array ownership");
    Descriptors rtvs(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,6,false);
    constexpr u16 halves[]{0x3000,0x3400,0x3600,0x3800,0x3900,0x3a00};
    for(UINT layer=0;layer<6;++layer){
        auto attachment=sampled;attachment.render_target=true;attachment.layers=layer+1;
        attachment.first_layer=layer;attachment.view_layers=1;attachment.storage_height=64;
        attachment.storage_slice_bytes=layout.mips[0].slice_bytes;
        const auto requested=LayoutGuestTexture(attachment);
        require(CompatibleGuestColorSubview(sampled,layout,attachment,requested),"Compatible color face rejected");
        auto& selected=cache.Upload(attachment,std::span(physical).first(requested.guest_bytes),
            renderer.RecordingList(),renderer.CompletedFence());
        require(&selected==&parent,"CB face binding split the original color array");
        const auto view=parent.Rtv(attachment);
        require(view.ViewDimension==D3D12_RTV_DIMENSION_TEXTURE2DARRAY&&
            view.Texture2DArray.FirstArraySlice==layer&&view.Texture2DArray.ArraySize==1,"RTV lost native array/view identity");
        device->CreateRenderTargetView(parent.texture.handle.Get(),&view,rtvs.Cpu(layer));
        parent.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const float color[]{float(layer+1)/8,.5f,0,1};
        renderer.RecordingList()->ClearRenderTargetView(rtvs.Cpu(layer),color,0,nullptr);
        require(cache.Own(parent),"Color face ownership lost");
        std::vector<u16> pixels(size_t(sampled.width)*sampled.height*2);
        for(size_t pixel=0;pixel<pixels.size();pixel+=2){pixels[pixel]=halves[layer];pixels[pixel+1]=0x3800;}
        EncodeGuestSubresource(sampled,layout,expected,std::as_bytes(std::span(pixels)),0,layer);
    }
    auto one=sampled;one.layers=one.view_layers=1;
    const auto srv=parent.Srv(one);require(srv.ViewDimension==D3D12_SRV_DIMENSION_TEXTURE2DARRAY&&
        srv.Texture2DArray.ArraySize==1,"One-layer SRV lost native array dimension");
    auto invalid=one;invalid.pitch*=2;
    require(!CompatibleGuestColorSubview(sampled,layout,invalid,LayoutGuestTexture(invalid)),"Different physical pitch accepted");
    invalid=sampled;invalid.layers=invalid.view_layers=7;
    require(!CompatibleGuestColorSubview(sampled,layout,invalid,LayoutGuestTexture(invalid)),"Out-of-bounds layer view accepted");
    GuestImageReadback readback(device,parent,renderer.RecordingList());
    const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
    auto observed=physical;readback.Resolve(renderer.CompletedFence(),observed);
    require(observed==expected,"Color face GPU writes, untouched layers or padding differ");
    require(cache.FindValidIndependentImage(sampled,layout)==&parent,"Color array no longer has one authoritative image");
    renderer.ValidateDebugMessages();puts("PASS: sampled array/CB subviews share one image; six exact GPU-written faces and padding");
}
static void RunConvertedClearPressure(ID3D12Device* device){
    Rasterizer renderer(device);GuestTextureCache cache(device);SCOPE_EXIT {renderer.Finish();};
    const auto require=[](bool v,const char* m){if(!v)throw std::runtime_error(m);};
    GuestTextureDesc desc;desc.address=0x40000;desc.width=7;desc.height=5;desc.pitch=16;
    desc.layers=desc.view_layers=2;desc.data_format=10;desc.number_format=0;desc.tile_mode=13;
    desc.writable=true;desc.native_raw32=true;const auto layout=LayoutGuestTexture(desc);
    std::vector<std::byte> physical(layout.guest_bytes,std::byte{0x11}),expected=physical;
    auto& image=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
    cache.SetRecordingListProvider([&]{return renderer.RecordingList();});
    const float initial[]{0,0,0,0};cache.ClearRawStorageImage(image,initial,renderer.RecordingList(),renderer.CompletedFence());
    std::weak_ptr<GuestPackedTransfer> pending=image.format_transfers.back();
    // Inject only policy accounting, not a multi-GiB allocation. The actual
    // pending clear/resources must still execute and retire at a real fence.
    constexpr u64 pressure=16ULL*1024*1024*1024;
    image.allocation_bytes+=pressure;image.format_retirement_bytes+=pressure;
    unsigned drains{};
    cache.SetPendingRetirement([&]{
        ++drains;require(!pending.expired(),"Pending clear released before its GPU fence");
        cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
        require(pending.expired(),"Completed clear conversion was not reclaimed");
    });
    const float final[]{1,0,1,1};cache.ClearRawStorageImage(image,final,renderer.RecordingList(),renderer.CompletedFence());
    require(drains==1&&image.allocation_bytes<pressure,"Clear pressure did not retire exactly one old batch");
    std::vector<u32> pixels(size_t(desc.width)*desc.height,0xffff00ffU);
    for(UINT layer=0;layer<desc.layers;++layer)
        EncodeGuestSubresource(desc,layout,expected,std::as_bytes(std::span(pixels)),0,layer);
    GuestImageReadback readback(device,image,renderer.RecordingList());
    const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
    auto observed=physical;readback.Resolve(renderer.CompletedFence(),observed);
    require(observed==expected,"Pressure clear lost GPU contents, layers or padding");
    cache.Collect(renderer.CompletedFence());
    const auto saved=image.texture.handle;const auto before=image.allocation_bytes;
    image.allocation_bytes+=pressure;image.format_retirement_bytes+=pressure;image.pending=true;
    unsigned blocked_drains{};cache.SetPendingRetirement([&]{++blocked_drains;});
    bool rejected=false;try{cache.ClearRawStorageImage(image,initial,renderer.RecordingList(),renderer.CompletedFence());}
    catch(const std::runtime_error&){rejected=true;}
    require(rejected&&blocked_drains==1&&image.texture.handle.Get()==saved.Get()&&
        image.allocation_bytes==before+pressure,"Unreclaimable clear pressure replayed, mutated storage or falsely succeeded");
    image.allocation_bytes-=pressure;image.format_retirement_bytes-=pressure;image.pending=false;
    renderer.ValidateDebugMessages();
    puts("PASS: converted clear drains pending GPU transfers under injected accounting pressure; exact contents/padding and no-progress rejection");
}
int main(int argc,char** argv){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");
    const bool packed19_rt=(argc==2&&std::string_view(argv[1])=="--packed19-msaa-attachment")||
        (argc==5&&std::string_view(argv[1])=="--cached-depth-color"&&std::string_view(argv[2])=="19"&&std::string_view(argv[4])=="rt")||
        (argc==3&&std::string_view(argv[1])=="--cached-packed-color-depth"&&std::string_view(argv[2])=="19");
    auto device=CreateFormatTestDevice(packed19_rt?DXGI_FORMAT_B4G4R4A4_UNORM:DXGI_FORMAT_UNKNOWN);
    if(argc==2&&strcmp(argv[1],"--current-independent-binding")==0){RunCurrentIndependentBinding(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--color-array-subviews")==0){RunColorArraySubviews(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--converted-clear-pressure")==0){RunConvertedClearPressure(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--requested-sample-backings")==0){for(UINT samples:{1u,8u})RunRequestedSampleBackings(device.Get(),samples);return 0;}
    if(argc==2&&strcmp(argv[1],"--sample-backing-pressure")==0){for(UINT samples:{1u,8u})RunSampleBackingPressure(device.Get(),samples);return 0;}
    if(argc==2&&strcmp(argv[1],"--robust-sample-copy")==0){RunMultisampleReadbackPermission(device.Get());RunRobustSampleCopy(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--readonly-mutable32-views")==0){RunReadonlyNumericViewsFixture(device.Get(),true,false,0);return 0;}
    if(argc==2&&strcmp(argv[1],"--readonly-word-numeric-views")==0){RunReadonlyNumericViewsFixture(device.Get(),false,true);return 0;}
    if(argc==2&&strcmp(argv[1],"--readonly-word-views")==0){RunReadonlyNumericViewsFixture(device.Get(),true);return 0;}
    if(argc==2&&strcmp(argv[1],"--readonly-numeric-views")==0){RunReadonlyNumericViewsFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--packed19-msaa-attachment")==0){RunPacked19MsaaAttachmentFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--msaa-depth-init")==0){RunMsaaDepthInitializationFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--depth")==0){RunIndependentDepthFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--paired-depth")==0){RunPairedDepthSnapshotFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--depth-recreation-extents")==0){RunColorToMsDepthFixture(device.Get(),false,false,0,true);return 0;}
    if(argc==2&&strcmp(argv[1],"--dirty-owned-depth")==0){
        RunDirtyOwnedDepthFixture(device.Get());RunDirtyOwnedDepthFixture(device.Get(),true);return 0;}
    if(argc==2&&strcmp(argv[1],"--dirty-cached-depth")==0){RunDirtyCachedDepthFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--dirty-cached-color-depth")==0){RunDirtyCachedDepthFixture(device.Get(),true);return 0;}
    if(argc==2&&strcmp(argv[1],"--cached-depth-uav")==0){RunDirtyCachedDepthFixture(device.Get(),false,false,0,true);return 0;}
    if(argc==2&&strcmp(argv[1],"--cached-depth-rt")==0){RunDirtyCachedDepthFixture(device.Get(),false,true);return 0;}
    if(argc==5&&strcmp(argv[1],"--cached-depth-color")==0){
        const UINT format=std::stoul(argv[2]),number=std::stoul(argv[3]);const bool writable=std::string_view(argv[4])=="uav";
        if((format!=3&&format!=5&&format!=6&&format!=9&&format!=10&&(format<16||format>19))||
           (format==6?number!=7:format>=16?number!=0:number!=0&&number!=4)||
           (!writable&&std::string_view(argv[4])!="rt"))throw std::invalid_argument("Invalid depth/color fixture domain");
        RunDirtyCachedDepthFixture(device.Get(),false,!writable,0,writable,format,number);return 0;
    }
    if(argc==3&&strcmp(argv[1],"--cached-packed-color-depth")==0){
        const UINT format=std::stoul(argv[2]);if(format<16||format>19)throw std::invalid_argument("Invalid packed depth source");
        RunDirtyCachedDepthFixture(device.Get(),true,false,0,false,format,0);return 0;
    }
    if(argc==2&&strcmp(argv[1],"--cached-packed19-sampled-depth")==0){
        RunDirtyCachedDepthFixture(device.Get(),true,false,0,false,19,0,true);return 0;
    }
    if(argc==2&&strcmp(argv[1],"--planned-depth-native-change")==0){RunDirtyCachedDepthFixture(device.Get(),false,false,1);return 0;}
    if(argc==2&&strcmp(argv[1],"--planned-depth-current-buffer")==0){RunDirtyCachedDepthFixture(device.Get(),false,false,2);return 0;}
    if(argc==2&&strcmp(argv[1],"--depth-subviews")==0){RunDepthSubviewFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--depth-crossed-resources")==0){RunIndependentDepthGrowthFixture(device.Get(),true);return 0;}
    if(argc==2&&strcmp(argv[1],"--depth-growth")==0){RunIndependentDepthGrowthFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--color-ms-depth")==0){RunColorToMsDepthFixture(device.Get());return 0;}
    if(argc==2&&strcmp(argv[1],"--color-ms-depth-srgb")==0){RunColorToMsDepthFixture(device.Get(),true);return 0;}
    if(argc==2&&strcmp(argv[1],"--color-ms-depth-wide")==0){RunColorToMsDepthFixture(device.Get(),false,true);return 0;}
    if(argc==2&&strcmp(argv[1],"--color-ms-depth-overlap-class")==0){
        RunColorToMsDepthFixture(device.Get(),false,false,1);RunColorToMsDepthFixture(device.Get(),false,false,2);return 0;}
    if(argc==2&&strcmp(argv[1],"--msaa")==0){
        for(const UINT samples:{2u,4u,8u})for(const UINT number_format:{4u,5u,7u})for(const bool writable:{false,true})
            RunMultisample(device.Get(),samples,number_format,writable);
        puts("PASS: independent R32 MSAA samples/layers, exact private padding, unchanged physical domain and ownership");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-pressure")==0){
        RunMultisample(device.Get(),2,4,false,true);
        RunMultisample(device.Get(),4,5,true,true);
        RunMultisample(device.Get(),8,7,true,true);
        for(UINT samples:{2u,4u,8u})RunReadonlyMultisampleWordStorage(device.Get(),samples);
        puts("PASS: independent MSAA scratch retirement, lazy restore, fences, pins, leases and exact domains");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-vector32")==0){
        GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=13;rejected.number_format=7;
        rejected.writable=true;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("Writable RGB32 acquired sampled-only MSAA ownership");
        rejected.data_format=14;rejected.samples=16;rejected.writable=false;
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
        GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=9;rejected.number_format=1;
        auto invalid=LayoutGuestTexture(rejected);invalid.expanded_bytes_per_pixel=8;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,invalid))
            throw std::runtime_error("Expanded signed packed image accepted a mismatched host footprint");
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
    if(argc==2&&strcmp(argv[1],"--msaa-rawrgba8-clear")==0){
        const RawMultisampleClear uint_clear{{std::bit_cast<float>(1u),std::bit_cast<float>(127u),
            std::bit_cast<float>(128u),std::bit_cast<float>(255u)},{1,127,128,255}};
        const RawMultisampleClear sint_clear{{std::bit_cast<float>(int32_t(-128)),std::bit_cast<float>(int32_t(-1)),
            std::bit_cast<float>(int32_t(1)),std::bit_cast<float>(int32_t(127))},{128,255,1,127}};
        const RawMultisampleClear snorm_clear{{-2.f,-0.5f,0.5f,2.f},{129,192,64,127}};
        const RawMultisampleClear unorm_clear{{0.f,0.25f,0.5f,1.f},{0,64,128,255}};
        // Authored linear-to-sRGB golden; execution remains deferred with the
        // rest of this fixture. Alpha remains linear and all samples must match.
        const RawMultisampleClear srgb_clear{{0.25f,0.5f,0.f,1.f},{137,188,0,255}};
        RunMultisample<u8>(device.Get(),2,4,false,true,10,true,false,&uint_clear);
        RunMultisample<u8>(device.Get(),4,5,true,true,10,true,false,&sint_clear);
        RunMultisample<u8>(device.Get(),8,1,true,true,10,true,true,&snorm_clear);
        RunMultisample<u8>(device.Get(),2,0,false,true,10,true,false,&unorm_clear);
        RunMultisample<u8>(device.Get(),2,9,false,true,10,true,false,&srgb_clear);
        RunMultisample<u8>(device.Get(),4,9,true,true,10,true,false,&srgb_clear);
        puts("PASS: nonzero raw RGBA8 MSAA converted clears, all samples/layers, numeric rejection, fence retirement and independent domains");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed-clear")==0){
        // Fixed guest packed words, authored independently of the transfer path.
        // They cover packed10 signed fields and each packed16 component layout.
        const RawMultisampleClear uint_clear{{std::bit_cast<float>(1u),std::bit_cast<float>(127u),
            std::bit_cast<float>(1023u),std::bit_cast<float>(3u)},{0x01,0xfc,0xf1,0xff}};
        const RawMultisampleClear sint_clear{{std::bit_cast<float>(int32_t(-512)),std::bit_cast<float>(int32_t(-1)),
            std::bit_cast<float>(int32_t(1)),std::bit_cast<float>(int32_t(-2))},{0x00,0xfe,0x1f,0x80}};
        const RawMultisampleClear snorm_clear{{-1.f,0.f,1.f,-1.f},{0x01,0x02,0xf0,0xdf}};
        const RawMultisampleClear unorm_clear{{1.f,0.f,0.f,1.f},{0xff,0x03,0x00,0xc0}};
        RunMultisample<u32>(device.Get(),2,4,true,true,9,true,true,&uint_clear);
        RunMultisample<u32>(device.Get(),4,5,true,true,9,true,true,&sint_clear);
        RunMultisample<u32>(device.Get(),8,1,true,true,9,true,true,&snorm_clear);
        RunMultisample<u32>(device.Get(),2,0,true,true,9,true,true,&unorm_clear);
        const RawMultisampleClear packed16[]{
            {{1.f,0.f,0.f,1.f},{0x00,0xf8,0,0}},
            {{1.f,0.f,0.f,1.f},{0x00,0xfc,0,0}},
            {{1.f,0.f,0.f,1.f},{0x01,0xf8,0,0}},
            {{1.f,0.f,0.f,1.f},{0xff,0x00,0,0}}};
        for(UINT format=16;format<=19;++format)for(const UINT samples:{2u,4u,8u})
            RunMultisample<u16>(device.Get(),samples,0,true,true,format,false,false,&packed16[format-16]);
        puts("PASS: nonzero packed10/16 MSAA clears, narrow integer guards, all samples/layers and private padding");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed10-expanded")==0){
        for(const UINT samples:{2u,4u,8u}){
            // UNORM->SNORM and UINT->SINT interpretations exercise the same raw
            // DWORDs, including both normalized encodings of minus one.
            RunMultisample<u32>(device.Get(),samples,0,false,true,9,true,false);
            RunMultisample<u32>(device.Get(),samples,4,false,true,9,true,false);
        }
        puts("PASS: signed packed10 expanded MSAA snapshots, exact retained words/masks, repeated private retile and new padding");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed10")==0){
        GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=9;rejected.number_format=1;
        if(!GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
            throw std::runtime_error("Signed packed sampled expansion lost its retained exact-word contract");
        rejected.writable=true;
        auto invalid=LayoutGuestTexture(rejected);invalid.format=DXGI_FORMAT_R10G10B10A2_UINT;
        if(GuestTextureCache::CanTransferIndependentMultisample(rejected,invalid))
            throw std::runtime_error("Raw packed10 accepted native R10 transfer metadata");
        RunMultisample<u32>(device.Get(),2,0,true,true,9,true,true);
        RunMultisample<u32>(device.Get(),4,1,true,true,9,true,false);
        RunMultisample<u32>(device.Get(),8,4,true,true,9,false,true);
        RunMultisample<u32>(device.Get(),2,5,true,true,9,true,true);
        RunMultisample<u32>(device.Get(),4,0,true,true,9,false,true);
        RunMultisample<u32>(device.Get(),8,4,true,true,9,true,true);
        puts("PASS: packed10 raw/native UINT MSAA words, samples/layers, pressure and independent padding/domain");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-rgb32")==0){
        for(const UINT samples:{2u,4u,8u})for(const UINT number:{4u,5u,7u})
            RunMultisample<u32>(device.Get(),samples,number,false,false,13);
        puts("PASS: readonly RGB32 MSAA guest12/host16 exact components, alpha discard, independent ownership and padding");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed-float")==0){
        for(const UINT samples:{2u,4u,8u}){
            RunMultisample<u32>(device.Get(),samples,7,false,true,6);
            RunMultisample<u32>(device.Get(),samples,7,true,true,6);
            RunMultisample<u32>(device.Get(),samples,7,false,true,6,false,false,nullptr,34);
            RunMultisample<u32>(device.Get(),samples,4,false,true,4,false,false,nullptr,34);
        }
        puts("PASS: independent packed-float MSAA exact words, cast-list guards, readonly RGB9E5 ownership and pressure restore");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed16-sampled")==0){
        for(const UINT format:{16u,17u,18u,19u})for(const UINT samples:{2u,4u,8u})
            RunMultisample<u16>(device.Get(),samples,0,false,true,format);
        puts("PASS: native packed16 and expanded19 sampled MSAA versions, guest word ordering and private retile padding");return 0;
    }
    if(argc==2&&strcmp(argv[1],"--msaa-packed16-raw")==0){
        for(const UINT format:{16u,17u,18u,19u}){
            GuestTextureDesc rejected;rejected.samples=2;rejected.data_format=format;rejected.number_format=0;
            if(!GuestTextureCache::CanTransferIndependentMultisample(rejected,LayoutGuestTexture(rejected)))
                throw std::runtime_error("Packed16 sampled representation lost its explicit transfer contract");
            rejected.writable=true;
            auto invalid=LayoutGuestTexture(rejected);invalid.format=DXGI_FORMAT_R16_UINT;
            if(GuestTextureCache::CanTransferIndependentMultisample(rejected,invalid))
                throw std::runtime_error("Packed16 raw storage accepted an R16 host footprint");
            for(const UINT samples:{2u,4u,8u})
                RunMultisample<u16>(device.Get(),samples,0,true,true,format);
        }
        puts("PASS: writable packed16 low-word MSAA storage, host high-half discard, samples/layers and private padding");return 0;
    }
    if(argc!=1)throw std::invalid_argument("Expected no arguments or a registered --msaa mode");
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
    cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
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
    const GuestTextureCache::BufferResidency inherited{{desc.address,layout.guest_bytes}};
    source.pending_buffer_residency=inherited;
    GuestTextureCache::BufferResidency published;unsigned publications{};
    cache.SetBufferResidencyPublisher([&](auto ranges){++publications;published.assign(ranges.begin(),ranges.end());});
    auto& converted=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence(),
        {},false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
    if(publications||converted.pending_buffer_residency!=inherited)
        throw std::runtime_error("Private I conversion lost or prematurely published its parent's B residency");
    cache.PublishBufferResidency(converted);cache.PublishBufferResidency(converted);
    if(publications!=1||published!=inherited||!converted.pending_buffer_residency.empty())
        throw std::runtime_error("Converted I did not consume its original B-source effect exactly once");
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
