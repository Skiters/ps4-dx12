// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_stage_bindings.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>

namespace NativeDX12 {
inline void RunIndependentDepthFixture(ID3D12Device* device) {
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    for(const UINT format:{2u,4u})for(const UINT tile:{0u,8u,13u}){
        struct Guest {void* memory{};~Guest(){if(memory)VirtualFree(memory,0,MEM_RELEASE);}} guest;
        GuestTextureCache cache(device);Rasterizer renderer(device);
        GuestTextureDesc desc;desc.address=0x1600000000ULL;desc.width=13;desc.height=11;desc.pitch=16;
        desc.layers=desc.view_layers=2;desc.tile_mode=tile;desc.data_format=format;
        desc.number_format=format==2?0:7;desc.depth_target=true;
        const auto layout=LayoutGuestTexture(desc);const auto component=layout.bytes_per_pixel;
        guest.memory=VirtualAlloc(reinterpret_cast<void*>(desc.address),SIZE_T(layout.guest_bytes),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        require(guest.memory,"Depth fixture guest allocation");renderer.MapMemory(desc.address,layout.guest_bytes);
        std::memset(guest.memory,0xa5,size_t(layout.guest_bytes));
        require(IsIndependentGuestDepthStorage(desc,layout),"Valid D16/D32 independent plane rejected");
        auto invalid=desc;invalid.stencil_address=0x80000;
        require(!IsIndependentGuestDepthStorage(invalid,layout),"Split stencil acquired single-plane ownership");
        invalid=desc;invalid.samples=2;
        require(!IsIndependentGuestDepthStorage(invalid,layout),"MSAA depth acquired single-sample ownership");
        invalid=desc;invalid.writable=true;
        require(!IsIndependentGuestDepthStorage(invalid,layout),"Depth storage authorized a shader UAV");
        cache.SetBufferAliasRetention([](const auto& d,const auto&){return d.writable&&!d.depth_target&&!d.stencil_address;});
        cache.SetIndependentImageRetention([](const auto& d,const auto& l){
            return IsIndependentGuestDepthStorage(d,l)||(d.writable&&!d.depth_target&&!d.stencil_address);
        });
        const auto acquire=[&](auto& image){
            const auto owner=DescribeGuestImageOwnership(image);
            if(!owner||!cache.CanRetainIndependentImage(owner->desc,owner->layout))return false;
            image.buffer_alias_version=true;image.SetGpuOwned(true);return true;
        };
        cache.SetOwnershipAcquirer(acquire);
        cache.SetOwnershipResolver([](auto&){throw std::runtime_error("Depth I was prematurely resolved into physical B");});
        cache.SetIndependentVersionTransfer([](auto&,u64,u64)->Resource{
            throw std::runtime_error("Compatible sampled depth view requested an independent conversion");
        },acquire,[](auto& image){image.buffer_alias_version=false;image.SetGpuOwned(false);});
        cache.SetTransferSubmissionPublisher([&]{renderer.NotifyOnSubmission([&](u64 fence){cache.Submitted(fence);});});
        cache.SetPendingRetirement([&]{renderer.Finish();cache.Collect(renderer.CompletedFence());renderer.RecordingList();});
        std::vector<std::byte> physical(size_t(layout.guest_bytes),std::byte{0xa5});
        const auto original=physical;
        auto& image=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        renderer.NotifyOnSubmission([&](u64 fence){cache.Submitted(fence);});
        require(cache.Own(image),"Independent depth ownership was not acquired");
        const auto owner=DescribeGuestImageOwnership(image);
        require(owner&&owner->layout.guest_bytes==layout.guest_bytes&&!owner->desc.depth_target&&owner->desc.writable,
            "Depth physical ownership changed geometry or native attachment access");
        auto dsv=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,2,false);
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
        for(UINT layer=0;layer<2;++layer){
            auto view=desc;view.first_layer=layer;view.view_layers=1;
            const auto depth=image.Dsv(view);
            device->CreateDepthStencilView(image.texture.handle.Get(),&depth,dsv->Cpu(layer));
            renderer.RecordingList()->ClearDepthStencilView(dsv->Cpu(layer),D3D12_CLEAR_FLAG_DEPTH,
                layer?.5f:.25f,0,0,nullptr);
        }
        // Independent native plane copies are the bit-exact I oracle. D16's
        // midpoint clear quantizes on the device; do not infer those bits from
        // a CPU float conversion or use the production tiler/readback to get them.
        struct NativePlane {Resource readback;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;};
        const auto capture_native=[&](Resource& texture){
            std::vector<NativePlane> planes;const auto native_desc=texture.handle->GetDesc();
            texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            for(UINT layer=0;layer<2;++layer){
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes{};
                device->GetCopyableFootprints(&native_desc,layer,1,0,&fp,nullptr,nullptr,&bytes);
                planes.push_back({Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST),fp});
                D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=texture.handle.Get();
                from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=layer;
                D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=planes.back().readback.handle.Get();
                to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=fp;
                renderer.RecordingList()->CopyTextureRegion(&to,0,0,0,&from,nullptr);
            }
            return planes;
        };
        auto native_planes=capture_native(image.texture);
        // B1 and physical B0 are distinct from depth I. Retile must only replace
        // the actual depth texels of the private output, keeping B1 padding.
        auto initial=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto upload=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
        std::memset(mapped,0x5a,size_t(layout.guest_bytes));upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(initial.handle.Get(),0,upload.handle.Get(),0,layout.guest_bytes);
        auto version=cache.RetileBufferVersion(image,initial,renderer.RecordingList(),renderer.CompletedFence());
        auto sampled=desc;sampled.depth_target=false;
        require(CompatibleGuestShaderStorage(desc,sampled)&&CompatibleGuestImageMipGeometry(desc,layout,sampled,layout),
            "Ordered depth/sample domain rejected exact readonly storage");
        auto writer=sampled;writer.writable=true;
        require(!CompatibleGuestShaderStorage(desc,writer)&&!CompatibleGuestImageMipGeometry(desc,layout,writer,layout),
            "Depth allocation was admitted as shared shader-writable color storage");
        auto& sample=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
        require(sample.texture.handle.Get()==image.texture.handle.Get(),"Sampled depth view replaced retained native I");
        require(image.buffer_alias_version&&image.gpu_owned,"Retile/sample discarded depth ownership");
        const Rasterizer::ShaderBufferRequest backing{desc.address,layout.guest_bytes,false,true,true};
        renderer.PrepareShaderBuffers(std::span(&backing,1));
        renderer.FillPreparedShaderBuffer(backing.address,backing.size,0x5a5a5a5a,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        std::vector<Rasterizer::ShaderBufferRequest> requests{backing};
        const OrderedGuestBuffer buffers[]{{true,true,false,0,4,layout.guest_bytes}};
        const OrderedGuestImage images[]{{sampled,layout,false,false}};
        const GuestStageBindingSpan stages[]{{0,1,0,1}};
        auto prepared=PrepareOrderedGuestStages(device,renderer,cache,stages,requests,buffers,images,true,
            [](VAddr,u64)->VAddr{throw std::runtime_error("Depth formatted read became a private CPU stream");});
        require(prepared.images[0]==&image,"Ordered depth stage replaced the preceding native I");
        auto ordered=renderer.CapturePreparedShaderBuffer(desc.address,layout.guest_bytes);
        // The reverse production GPU upload must preserve these exact depth
        // bits, rather than interpreting the guest words as a color UAV.
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        auto target_desc=desc;target_desc.address+=0x100000;
        auto& target=cache.Upload(target_desc,{},renderer.RecordingList(),renderer.CompletedFence(),{},true,&version);
        auto target_planes=capture_native(target.texture);
        auto readback=Buffer(device,3*layout.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        initial.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,version.handle.Get(),0,layout.guest_bytes);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),layout.guest_bytes,initial.handle.Get(),0,layout.guest_bytes);
        ordered.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),2*layout.guest_bytes,ordered.handle.Get(),0,layout.guest_bytes);
        GuestLinearImageReadback compact(device,image,renderer.RecordingList());
        renderer.NotifyOnSubmission([&](u64 fence){cache.Submitted(fence);});
        const auto fence=renderer.Flush();compact.Submitted(fence);
        std::vector<std::byte> linear(size_t(compact.Bytes()+16),std::byte{0xc3});
        bool rejected=false;try{compact.Resolve(0,linear);}catch(const std::logic_error&){rejected=true;}
        require(rejected,"Depth download accepted an unfinished GPU fence");
        rejected=false;try{compact.ReleaseSource(0);}catch(const std::logic_error&){rejected=true;}
        require(rejected,"Depth download released its unfinished placed source");
        renderer.Finish();compact.ReleaseSource(renderer.CompletedFence());compact.Resolve(renderer.CompletedFence(),linear);
        std::vector<std::byte> expected(size_t(layout.guest_bytes),std::byte{0x5a});
        std::vector<std::byte> expected_linear(linear.size(),std::byte{0xc3});
        for(UINT layer=0;layer<2;++layer){
            const auto& plane=native_planes[layer];const auto& target_plane=target_planes[layer];void* raw{};void* roundtrip{};
            D3D12_RANGE range{0,SIZE_T(plane.readback.handle->GetDesc().Width)};
            Check(plane.readback.handle->Map(0,&range,&raw));
            D3D12_RANGE target_range{0,SIZE_T(target_plane.readback.handle->GetDesc().Width)};
            Check(target_plane.readback.handle->Map(0,&target_range,&roundtrip));
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x){
                const auto* value=static_cast<const std::byte*>(raw)+plane.footprint.Offset+
                    u64(y)*plane.footprint.Footprint.RowPitch+u64(x)*component;
                const auto* target_value=static_cast<const std::byte*>(roundtrip)+target_plane.footprint.Offset+
                    u64(y)*target_plane.footprint.Footprint.RowPitch+u64(x)*component;
                require(!std::memcmp(value,target_value,component),"GPU B-to-depth upload changed native depth bits");
                if(format==4){u32 word{};std::memcpy(&word,value,4);
                    require(word==std::bit_cast<u32>(layer?.5f:.25f),"Independent native depth oracle lost D32 clear");}
                const auto at=GuestPixelOffset(desc,layout,0,layer,x,y);
                std::memcpy(expected.data()+at,value,component);
                std::memcpy(expected_linear.data()+((u64(layer)*desc.height+y)*desc.pitch+x)*component,value,component);
            }
            plane.readback.handle->Unmap(0,&empty);
            target_plane.readback.handle->Unmap(0,&empty);
        }
        const D3D12_RANGE all{0,SIZE_T(3*layout.guest_bytes)};Check(readback.handle->Map(0,&all,&mapped));
        const auto* data=static_cast<const std::byte*>(mapped);
        const bool pixels=!std::memcmp(data,expected.data(),expected.size());
        if(!pixels)for(size_t i=0;i<expected.size();++i)if(data[i]!=expected[i]){
            fprintf(stderr,"depth mismatch format=%u tile=%u byte=%zu got=%02x expected=%02x\n",
                format,tile,i,unsigned(data[i]),unsigned(expected[i]));break;
        }
        bool untouched=true;for(u64 i=0;i<layout.guest_bytes;++i)untouched&=data[layout.guest_bytes+i]==std::byte{0x5a};
        const bool ordered_pixels=!std::memcmp(data+2*layout.guest_bytes,expected.data(),expected.size());
        readback.handle->Unmap(0,&empty);
        require(pixels,"D16/D32 GPU retile changed texels, layer identity or B1 padding");
        require(ordered_pixels,"Ordered formatted depth transfer lost native I or B1 padding");
        require(untouched&&physical==original&&!std::memcmp(guest.memory,original.data(),original.size()),
            "Depth transfer changed B1 or physical B0");
        require(linear==expected_linear,"Depth compact download changed values, pitch padding or outer guard");
        cache.Collect(renderer.CompletedFence());renderer.ValidateDebugMessages();
    }
    puts("PASS: independent single-plane D16/D32 I/B retile, sampled native identity, compact depth readback, padding and fence negatives");
}
} // namespace NativeDX12
