// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/scope_exit.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <bit>
#include <cstring>

namespace NativeDX12 {
inline void RunDirtyOwnedDepthFixture(ID3D12Device* device,bool color_source=false,int chance_overlap=0) {
    const auto require=[](bool ok,const char* why){if(!ok)throw std::runtime_error(why);};
    GuestTextureCache cache(device);Rasterizer renderer(device);
    SCOPE_EXIT { renderer.Finish(); };
    GuestTextureDesc depth;depth.address=0x40000;depth.width=13;depth.height=11;depth.pitch=16;
    depth.depth_target=!color_source;depth.render_target=color_source;
    depth.data_format=4;depth.number_format=7;depth.tile_mode=0;
    if(chance_overlap==2)depth.layers=depth.view_layers=2;
    const auto layout=LayoutGuestTexture(depth);
    std::vector<std::byte> old(size_t(layout.guest_bytes),std::byte{0xa3}),current=old;
    const u32 old_word=std::bit_cast<u32>(0.25f),new_word=std::bit_cast<u32>(0.75f);
    for(UINT y=0;y<depth.height;++y)for(UINT x=0;x<depth.width;++x){
        const auto at=GuestPixelOffset(depth,layout,0,0,x,y);
        memcpy(old.data()+at,&old_word,4);memcpy(current.data()+at,&new_word,4);
    }
    cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
    const auto acquire=[](auto& image){image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
    unsigned retired{},captured{};
    cache.SetOwnershipAcquirer(acquire);
    cache.SetOwnershipResolver([](auto&){throw std::runtime_error("Dirty depth downloaded stale I into current B");});
    cache.SetIndependentVersionTransfer({},acquire,[&](auto& image){
        ++retired;image.buffer_alias_version=false;image.SetGpuOwned(false);
    });
    cache.SetIndependentImageTransfer([&](auto& image,const auto& target,const auto& physical){
        ++captured;
        return cache.CaptureIndependentImageVersion(image,target,physical,renderer.RecordingList(),
            renderer.CompletedFence(),[](u64,u64)->Resource{throw std::runtime_error("Dirty I captured a B seed");});
    });
    auto& source=cache.Upload(depth,old,renderer.RecordingList(),renderer.CompletedFence());
    require(cache.Own(source),"Dirty source ownership failed");
    auto pin=cache.PinBinding(source);
    if(!chance_overlap)source.MarkLogicalDirty(); // Same retained-allocation state as a real B write invalidation.
    auto sampled=depth;sampled.depth_target=false;sampled.render_target=false;sampled.number_format=4;
    if(chance_overlap)sampled.address=chance_overlap>0?depth.address+256:depth.address-256;
    if(chance_overlap==2){
        // A real slice boundary must NOT enter the chance-overlap fast path.
        // Shifted native-depth slices still require their explicit mapping;
        // until connected, retain the existing rejection rather than silently
        // accepting independent B as the required subresource contents.
        sampled.address=depth.address+layout.mips[0].slice_bytes;
        sampled.layers=sampled.view_layers=1;bool rejected=false;
        try{(void)cache.Upload(sampled,current,renderer.RecordingList(),renderer.CompletedFence());}
        catch(const std::runtime_error& e){rejected=std::string(e.what())==
            "Independent depth/color overlap needs an exact physical transfer plan";}
        require(rejected&&!retired&&!captured&&source.gpu_owned&&!source.logical_dirty,
            "Real shifted depth slice bypassed required mapping or changed its source");
        renderer.Finish();renderer.ValidateDebugMessages();
        puts("PASS: known shifted-depth-slice mapping gap remains explicitly rejected, never misclassified as chance overlap");
        return;
    }
    auto& target=cache.Upload(sampled,current,renderer.RecordingList(),renderer.CompletedFence());
    // Publishing a current same-class refresh supersedes cached_native_current;
    // the old pinned allocation must still retain its original GPU contents.
    if(chance_overlap){
        require(!retired&&!captured&&source.gpu_owned&&source.buffer_alias_version&&
            !source.logical_dirty&&source.cached_native_current&&target.cached_native_current,
            "Chance overlap copied/retired independent I or lost the separate current-B image");
    }else require(retired==1&&!captured&&!source.gpu_owned&&source.logical_dirty&&
        !source.cached_native_current&&target.cached_native_current,
        "Dirty source was copied, remained owned, or superseded current refresh");
    const auto read=[&](Resource& image){
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes{};const auto desc=image.handle->GetDesc();
        device->GetCopyableFootprints(&desc,0,1,0,&fp,nullptr,nullptr,&bytes);
        auto out=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        image.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=image.handle.Get();
        from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;to.pResource=out.handle.Get();
        to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=fp;
        renderer.RecordingList()->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        return std::pair(std::move(out),fp);
    };
    auto before=read(source.texture),after=read(target.texture);renderer.Finish();
    for(auto* plane:{&before,&after}){
        void* mapped{};D3D12_RANGE range{0,SIZE_T(plane->first.handle->GetDesc().Width)},empty{};
        Check(plane->first.handle->Map(0,&range,&mapped));bool exact=true;
        for(UINT y=0;y<depth.height;++y)for(UINT x=0;x<depth.width;++x){u32 word{};
            memcpy(&word,static_cast<std::byte*>(mapped)+plane->second.Offset+y*plane->second.Footprint.RowPitch+4*x,4);
            exact&=word==(plane==&before?old_word:new_word);
        }
        plane->first.handle->Unmap(0,&empty);require(exact,"Dirty native history/current B GPU contents mismatch");
    }
    renderer.ValidateDebugMessages();
    if(chance_overlap)printf("PASS: %s %s chance overlap preserves current I and samples independent B, without copy/retirement\n",
        color_source?"color":"depth",chance_overlap>0?"right":"left");
    else puts("PASS: dirty owned image retires stale I without capture/download, preserves old native contents and samples current B");
}
} // namespace NativeDX12
