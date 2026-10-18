// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace NativeDX12 {
inline void RunSampleBackingPressure(ID3D12Device* device,UINT original_samples){
    const auto require=[](bool v,const char* message){if(!v)throw std::runtime_error(message);};
    Rasterizer renderer(device);GuestTextureCache cache(device);
    cache.SetOwnershipAcquirer([](auto& image){image.buffer_alias_version=true;return true;});
    GuestTextureDesc desc;desc.address=0x80000;desc.width=7;desc.height=5;desc.pitch=8;
    desc.layers=desc.view_layers=2;desc.samples=original_samples;desc.tile_mode=13;desc.data_format=4;desc.number_format=4;
    const auto layout=LayoutGuestTexture(desc);
    const auto word=[](UINT generation,UINT layer,UINT sample,UINT x,UINT y){
        return 0x10000000u|(generation<<20)|(layer<<16)|(sample<<12)|(y<<6)|x;};
    const auto make=[&](const GuestTextureDesc& d,UINT generation){
        const auto l=LayoutGuestTexture(d);std::vector<std::byte> bytes(l.guest_bytes,std::byte{0xa7});
        for(UINT layer=0;layer<d.layers;++layer)for(UINT sample=0;sample<d.samples;++sample)
        for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){const auto v=word(generation,layer,sample,x,y);
            memcpy(bytes.data()+GuestPixelOffset(d,l,0,layer,x,y,sample),&v,4);}
        return bytes;
    };
    const auto physical=make(desc,0);Resource backing,upload,backing_readback;
    try{
        backing=Buffer(device,physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        upload=Buffer(device,physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        backing_readback=Buffer(device,physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* cpu{};Check(upload.handle->Map(0,nullptr,&cpu));memcpy(cpu,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
        renderer.RecordingList()->CopyBufferRegion(backing.handle.Get(),0,upload.handle.Get(),0,physical.size());
        auto& image=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        const auto uid=image.image_uid;auto* original=image.texture.handle.Get();
        const UINT alternate_samples=original_samples==1?4:1;
        cache.SetBackingSamples(image,alternate_samples,renderer.RecordingList());
        auto donor_desc=desc;donor_desc.address=0x180000;donor_desc.samples=alternate_samples;
        auto donor_bytes=make(donor_desc,1);
        auto& donor=cache.Upload(donor_desc,donor_bytes,renderer.RecordingList(),renderer.CompletedFence());
        donor.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RecordingList()->CopyResource(image.texture.handle.Get(),donor.texture.handle.Get());
        image.pending=true;require(cache.Own(image),"Backing history lost its independent I owner");
        const auto resident=image.allocation_bytes;
        require(!cache.ReclaimResidentMemory(0,renderer.CompletedFence(),&donor)&&
            image.sample_backings.at(original_samples).handle.Get()==original,
            "Pending alternate backing lost original history under pressure");
        cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
        require(!image.pending&&image.sample_copy_leases.empty()&&
            image.sample_backings.at(original_samples).allocation.use_count()==1,
            "Pressure regression has an unintended inactive backing lease");
        require(!cache.ReclaimResidentMemory(0,renderer.CompletedFence(),&donor),
            "Unreclaimable I owner falsely satisfied a zero resident budget");
        const auto retained=image.sample_backings.find(original_samples);
        require(retained!=image.sample_backings.end()&&retained->second.handle.Get()==original&&
            image.allocation_bytes==resident&&image.image_uid==uid&&image.gpu_owned&&image.buffer_alias_version,
            "Inactive sample backing retired despite observable copy=false contents");
        cache.SetBackingSamples(image,original_samples,renderer.RecordingList(),false);
        require(image.texture.handle.Get()==original&&image.RequestedBackingSamples()==original_samples,
            "copy=false recreated original backing instead of restoring its history");
        auto patch_desc=desc;patch_desc.address=0x280000;auto patch_bytes=make(patch_desc,2);
        auto& patch=cache.Upload(patch_desc,patch_bytes,renderer.RecordingList(),renderer.CompletedFence());
        // A real one-pixel shader copy writes every selected sample. The rest
        // of the destination must retain A, not current alternate B or garbage.
        auto region=desc;region.width=region.height=region.pitch=1;
        auto copies=std::make_shared<GuestMultisample>(device);
        for(UINT layer=0;layer<desc.layers;++layer){
            auto lease=copies->PrepareCopy(patch.texture,image.texture,region,LayoutGuestTexture(region),layer);
            lease->Record(renderer.RecordingList(),patch.texture,image.texture);
            image.sample_copy_leases.push_back(std::move(lease));
        }
        image.pending=true;auto expected=physical;
        for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<desc.samples;++sample){
            const auto v=word(2,layer,sample,0,0);memcpy(expected.data()+GuestPixelOffset(desc,layout,0,layer,0,0,sample),&v,4);}
        GuestImageReadback pixels(device,image,renderer.RecordingList());
        backing.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(backing_readback.handle.Get(),0,backing.handle.Get(),0,physical.size());
        const auto fence=renderer.Flush();cache.Submitted(fence);pixels.Submitted(fence);renderer.Finish();cache.Collect(renderer.CompletedFence());
        auto observed=physical;
        for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<desc.samples;++sample)
        for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)for(UINT byte=0;byte<4;++byte)
            observed[GuestPixelOffset(desc,layout,0,layer,x,y,sample)+byte]^=std::byte{0xff};
        pixels.Resolve(renderer.CompletedFence(),observed);
        require(observed==expected,"Pressure/copy=false/partial GPU copy lost uncovered texels,samples,layers or padding");
        Check(backing_readback.handle->Map(0,nullptr,&cpu));const bool unchanged=!memcmp(cpu,physical.data(),physical.size());
        D3D12_RANGE none{};backing_readback.handle->Unmap(0,&none);
        require(unchanged,"Inactive backing history replaced independent GPU B");
        require(image.desc==desc&&image.image_uid==uid&&image.texture.handle.Get()==original&&image.HasValidLogicalImage(),
            "Partial GPU copy changed original image class/UID/history");
        renderer.ValidateDebugMessages();
        printf("PASS: original%u/alternate%u inactive history survived completed pressure without leases;copy=false + partial GPU pixel/all samples,2 layers,untouched texels/padding and distinct GPU B\n",original_samples,alternate_samples);
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
} // namespace NativeDX12
