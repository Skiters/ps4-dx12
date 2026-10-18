// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace NativeDX12 {
inline void RunRequestedSampleBackings(ID3D12Device* device,UINT original_samples){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    Rasterizer renderer(device);GuestTextureCache cache(device),foreign(device);
    GuestTextureDesc desc;desc.address=0x80000;desc.width=7;desc.height=5;desc.pitch=8;
    desc.layers=desc.view_layers=2;desc.samples=original_samples;desc.tile_mode=13;desc.data_format=4;desc.number_format=4;
    const auto layout=LayoutGuestTexture(desc);
    const auto make_bytes=[](const GuestTextureDesc& d,const GuestTextureLayout& l,u32 generation){
        std::vector<std::byte> data(l.guest_bytes,std::byte{0xa7});
        for(UINT layer=0;layer<d.layers;++layer)for(UINT sample=0;sample<d.samples;++sample)
        for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){
            const u32 value=0x13570000u|(generation<<16)|(layer<<12)|(sample<<8)|(y<<4)|x;
            memcpy(data.data()+GuestPixelOffset(d,l,0,layer,x,y,sample),&value,4);
        }
        return data;
    };
    auto physical=make_bytes(desc,layout,0);
    try{
        auto& image=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
        const auto original=image.texture;const auto uid=image.image_uid,initial_bytes=image.allocation_bytes;
        const auto read=[&](const std::vector<std::byte>& expected){
            require(image.RequestedBackingSamples()==desc.samples&&image.texture.handle->GetDesc().SampleDesc.Count==desc.samples,
                "Guest readback admitted an alternate requested backing");
            GuestImageReadback pixels(device,image,renderer.RecordingList());
            const auto fence=renderer.Flush();cache.Submitted(fence);pixels.Submitted(fence);renderer.Finish();cache.Collect(renderer.CompletedFence());
            auto observed=physical;
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=0;sample<desc.samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
                for(UINT byte=0;byte<4;++byte)observed[GuestPixelOffset(desc,layout,0,layer,x,y,sample)+byte]^=std::byte{0xff};
            pixels.Resolve(renderer.CompletedFence(),observed);require(observed==expected,"Requested backing lost sample/layer words or guest padding");
        };
        for(UINT invalid:{0u,3u,32u}){
            bool denied=false;try{cache.SetBackingSamples(image,invalid,renderer.RecordingList());}catch(const std::invalid_argument&){denied=true;}
            require(denied&&image.RequestedBackingSamples()==desc.samples&&image.sample_backings.empty()&&
                image.texture.handle==original.handle&&image.allocation_bytes==initial_bytes,"Invalid count changed backing identity/accounting");
        }
        bool denied=false;try{foreign.SetBackingSamples(image,desc.samples,renderer.RecordingList());}catch(const std::invalid_argument&){denied=true;}
        require(denied,"Foreign cache accepted a sample backing no-op");
        image.desc.samples=16;
        denied=false;try{cache.SetBackingSamples(image,16,renderer.RecordingList());}catch(const std::invalid_argument&){denied=true;}
        image.desc=desc;
        require(denied&&image.RequestedBackingSamples()==desc.samples&&image.sample_backings.empty(),
            "Original guest16 payload was silently relabeled as native8");
        cache.SetRecordingListProvider([]()->ID3D12GraphicsCommandList*{return nullptr;});
        denied=false;try{cache.SetBackingSamples(image,16,renderer.RecordingList());}catch(const std::runtime_error&){denied=true;}
        require(denied&&image.RequestedBackingSamples()==desc.samples&&image.sample_backings.empty()&&
            image.texture.handle==original.handle&&image.allocation_bytes==initial_bytes&&image.sample_copy_leases.empty(),
            "Failed current-list acquisition published an alternate backing");
        unsigned resumed{};
        cache.SetRecordingListProvider([&]{
            ++resumed;cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            return renderer.RecordingList();
        });
        cache.SetBackingSamples(image,16,renderer.RecordingList());
        const auto alternate=image.texture;const auto native=alternate.handle->GetDesc().SampleDesc.Count;
        require(resumed==1&&image.pending&&image.RequestedBackingSamples()==16&&
            native==GuestMultisample::SelectBackingSamples(device,layout.format,16)&&native>=desc.samples&&
            alternate.handle!=original.handle&&image.sample_backings.at(desc.samples).handle==original.handle,
            "Requested16 identity collapsed into physical sample count or lost resumed copy receipt");
        const auto with_alternate=image.allocation_bytes;
        cache.SetBackingSamples(image,16,renderer.RecordingList());
        require(resumed==1&&image.texture.handle==alternate.handle&&image.allocation_bytes==with_alternate,
            "Repeated requested backing rebuilt/copied a different physical identity");
        cache.SetRecordingListProvider([&]{return renderer.RecordingList();});
        cache.SetBackingSamples(image,desc.samples,renderer.RecordingList());
        require(image.texture.handle==original.handle&&image.sample_backings.at(16).handle==alternate.handle&&
            image.RequestedBackingSamples()==desc.samples&&image.allocation_bytes==with_alternate,"Requested backing identities merged");
        read(physical); // Every original sample is distinguishable, before any write.

        auto donor_desc=desc;donor_desc.address=0x180000;donor_desc.samples=native;
        const auto donor_layout=LayoutGuestTexture(donor_desc);auto changed=make_bytes(donor_desc,donor_layout,8);
        auto& donor=cache.Upload(donor_desc,changed,renderer.RecordingList(),renderer.CompletedFence());
        cache.SetBackingSamples(image,16,renderer.RecordingList(),false);
        donor.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RecordingList()->CopyResource(image.texture.handle.Get(),donor.texture.handle.Get());
        image.NoteLogicalImageWrite();image.pending=true;
        cache.SetBackingSamples(image,desc.samples,renderer.RecordingList(),false);
        read(physical); // copy=false restores the old original backing, not the changed alternate.
        cache.SetBackingSamples(image,16,renderer.RecordingList(),false);
        cache.SetBackingSamples(image,desc.samples,renderer.RecordingList(),true);
        auto expected=make_bytes(desc,layout,8);read(expected);
        require(image.desc==desc&&image.image_uid==uid&&image.texture.handle==original.handle&&
            image.logical_gpu_modified&&!image.logical_dirty&&image.allocation_bytes==with_alternate,
            "Backing transitions changed original image class/UID/history/accounting");
        require(physical==make_bytes(desc,layout,0),"Backing copy mutated physical guest bytes");
        renderer.ValidateDebugMessages();
        printf("PASS: requested16/native%u and requested%u/native%u remain distinct;2 layers/all original sample words,copy/no-copy,current-list resume,failed-publication/foreign/count negatives,original UID/class/history and full padding\n",native,desc.samples,desc.samples);
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
} // namespace NativeDX12
