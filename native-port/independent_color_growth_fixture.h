// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <cstring>

namespace NativeDX12 {
inline void RunIndependentColorLayerGrowth(ID3D12Device* device){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    for(const UINT tile:{0u,8u})for(const UINT layers:{1u,2u}){
        Rasterizer renderer(device);GuestTextureCache cache(device);
        GuestTextureDesc source;source.address=0x40000;source.width=13;source.height=11;source.pitch=16;
        source.levels=2;source.layers=source.view_layers=layers;source.tile_mode=tile;
        source.data_format=4;source.number_format=4;source.render_target=true;
        auto target=source;target.levels=3;target.layers=target.view_layers=layers+1;
        const auto old_layout=LayoutGuestTexture(source),new_layout=LayoutGuestTexture(target);
        require(old_layout.mips[1].offset!=new_layout.mips[1].offset,
            "Color layer growth must move the second mip's byte origin");
        std::vector<std::byte> original(size_t(old_layout.guest_bytes),std::byte{0xa5});
        std::vector<std::byte> physical(size_t(new_layout.guest_bytes));
        for(size_t i=0;i<physical.size();++i)physical[i]=std::byte((i*37u+0x6du)&255u);
        auto expected=physical;
        for(UINT mip=0;mip<source.levels;++mip)for(UINT layer=0;layer<source.layers;++layer)
        for(UINT y=0;y<std::max(source.height>>mip,1u);++y)for(UINT x=0;x<std::max(source.width>>mip,1u);++x){
            const u32 value=0x13570000u^(mip<<12)^(layer<<8)^(y<<4)^x;
            memcpy(original.data()+GuestPixelOffset(source,old_layout,mip,layer,x,y),&value,4);
            memcpy(expected.data()+GuestPixelOffset(target,new_layout,mip,layer,x,y),&value,4);
        }
        auto upload=Buffer(device,physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto seed=Buffer(device,physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
        memcpy(mapped,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(seed.handle.Get(),0,upload.handle.Get(),0,physical.size());
        seed.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        renderer.RetainUntilFinish([upload,seed](u64){});
        cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
        cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
        const auto acquire=[](auto& image){image.buffer_alias_version=image.buffer_alias_watch=true;
            image.SetGpuOwned(true);return true;};
        cache.SetOwnershipAcquirer(acquire);
        cache.SetIndependentVersionTransfer({},acquire,[](auto& image){
            image.SetGpuOwned(false);image.buffer_alias_version=image.buffer_alias_watch=false;});
        cache.SetOwnershipResolver([](auto&){throw std::runtime_error("Color growth published I into B");});
        auto& old=cache.Upload(source,original,renderer.RecordingList(),renderer.CompletedFence());
        old.NoteLogicalImageWrite();require(acquire(old),"Color growth source ownership");
        auto pin=cache.PinBinding(old);unsigned captures{};
        const auto capture=[&](u64 address,u64 bytes){
            require(address==target.address&&bytes==physical.size(),"Color growth seed envelope");
            ++captures;return seed;
        };
        auto invalid=target;invalid.pitch+=8;
        bool rejected=false;
        try{cache.CaptureIndependentImageVersion(old,invalid,LayoutGuestTexture(invalid),
            renderer.RecordingList(),renderer.CompletedFence(),capture);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&!captures&&old.gpu_owned,"Invalid color growth mutated or captured its source");
        cache.SetIndependentImageTransfer([&](auto& image,const auto& desc,const auto& layout){
            return cache.CaptureIndependentImageVersion(image,desc,layout,
                renderer.RecordingList(),renderer.CompletedFence(),capture);});
        require(cache.FindIndependentUploadSource(target,new_layout)==&old,"Color growth lost preceding I");
        auto& grown=cache.Upload(target,physical,renderer.RecordingList(),renderer.CompletedFence());
        require(&grown!=&old&&grown.gpu_owned&&!old.gpu_owned&&captures==1,
            "Color growth replacement ownership or seed count");
        GuestImageReadback readback(device,grown,renderer.RecordingList());
        GuestImageReadback old_readback(device,old,renderer.RecordingList());
        auto raw=Buffer(device,physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        seed.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(raw.handle.Get(),0,seed.handle.Get(),0,physical.size());
        renderer.RetainUntilFinish([raw,seed](u64){});
        const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);old_readback.Submitted(fence);
        renderer.Finish();auto actual=physical,old_actual=original;
        readback.Resolve(renderer.CompletedFence(),actual);old_readback.Resolve(renderer.CompletedFence(),old_actual);
        require(actual==expected&&old_actual==original,"Color growth changed logical texels, new layers or padding");
        D3D12_RANGE range{0,physical.size()};Check(raw.handle->Map(0,&range,&mapped));
        const bool independent=memcmp(mapped,physical.data(),physical.size())==0;raw.handle->Unmap(0,&empty);
        require(independent,"Color growth overwrote independent raw B");renderer.ValidateDebugMessages();
    }
}
}
