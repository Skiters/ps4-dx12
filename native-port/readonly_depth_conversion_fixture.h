// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
inline void RunReadonlyDepthConversion(ID3D12Device* device) {
    using namespace NativeDX12;
    Rasterizer renderer(device);GuestTextureCache cache(device);
    SCOPE_EXIT {renderer.Finish();};
    const auto require=[](bool value,const char* why){if(!value)throw std::runtime_error(why);};
    GuestTextureDesc source;source.address=0x40000;source.width=13;source.height=11;
    source.pitch=16;source.data_format=4;source.number_format=7;source.tile_mode=0;
    auto target=source;target.depth_target=true;
    const auto layout=LayoutGuestTexture(source),physical=LayoutGuestTexture(target);
    std::vector<std::byte> initial(layout.guest_bytes,std::byte{0xa3});
    std::vector<std::byte> seed(physical.guest_bytes,std::byte{0x75}),expected=seed;
    for(UINT y=0;y<source.height;++y)for(UINT x=0;x<source.width;++x){
        const auto bits=std::bit_cast<u32>(float(1+x+y*13)/256.f);
        memcpy(initial.data()+GuestPixelOffset(source,layout,0,0,x,y),&bits,4);
        memcpy(expected.data()+GuestPixelOffset(target,physical,0,0,x,y),&bits,4);
    }
    cache.SetIndependentImageRetention([](const auto&,const auto&){return true;});
    cache.SetIndependentVersionTransfer({},[](auto& image){
        image.buffer_alias_version=true;image.SetGpuOwned(true);return true;
    },{});
    auto& image=cache.Upload(source,initial,renderer.RecordingList(),renderer.CompletedFence());
    require(cache.RevalidateReadonly(image),"Readonly initialized native I could not acquire ownership");
    require(!image.logical_gpu_modified&&!image.logical_dirty&&image.cached_native_current,
        "Fixture must retain initialized contents without inventing a GPU write");
    unsigned captures{};
    const auto capture=[&](u64 address,u64 bytes){
        require(address==target.address&&bytes==physical.guest_bytes,"Unexpected conversion seed envelope");
        ++captures;auto upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto buffer=Buffer(device,bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};Check(upload.handle->Map(0,nullptr,&mapped));memcpy(mapped,seed.data(),seed.size());
        upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(buffer.handle.Get(),0,upload.handle.Get(),0,bytes);
        buffer.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        renderer.RetainUntilFinish([upload,buffer](u64){(void)upload;(void)buffer;});return buffer;
    };
    auto version=cache.CaptureIndependentImageVersion(image,target,physical,
        renderer.RecordingList(),renderer.CompletedFence(),capture);
    auto readback=Buffer(device,physical.guest_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    version.depth.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,version.depth.handle.Get(),0,physical.guest_bytes);
    renderer.Finish();void* mapped{};Check(readback.handle->Map(0,nullptr,&mapped));
    const bool equal=!memcmp(mapped,expected.data(),expected.size());readback.handle->Unmap(0,nullptr);
    require(equal&&captures==1,"Readonly native I conversion lost texels or target B padding");
    require(!image.logical_gpu_modified&&image.gpu_owned&&!image.logical_dirty,
        "Private conversion changed source write history/ownership");
    for(unsigned invalid=0;invalid<2;++invalid){
        image.cached_native_current=invalid!=0;image.logical_dirty=invalid!=0;
        bool rejected=false;try{cache.CaptureIndependentImageVersion(image,target,physical,
            renderer.RecordingList(),renderer.CompletedFence(),capture);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&captures==1,"Stale/uninitialized readonly I was admitted or read B");
    }
    renderer.ValidateDebugMessages();
    puts("PASS: initialized readonly native I depth conversion, GPU texels/padding, no invented GPU write, stale/dirty negatives");
}
