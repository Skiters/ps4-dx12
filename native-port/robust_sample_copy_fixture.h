// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace NativeDX12 {
inline void RunMultisampleReadbackPermission(ID3D12Device* device){
    Rasterizer renderer(device);GuestTextureCache cache(device);GuestMultisample transfer(device);
    Resource missing;
    try{
        GuestTextureDesc desc;desc.address=0x80000;desc.width=desc.height=1;desc.pitch=8;
        desc.samples=2;desc.data_format=4;desc.number_format=4;desc.tile_mode=8;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> bytes(layout.guest_bytes,std::byte{0});
        auto& image=cache.Upload(desc,bytes,renderer.RecordingList(),renderer.CompletedFence());
        const auto size=GuestMultisample::LinearBytes(desc,GuestMultisample::Footprint(desc,layout));
        missing=Buffer(device,size,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        bool denied=false;try{(void)transfer.Prepare(image.texture,missing,desc,layout,0,0,false);}
        catch(const std::invalid_argument&){denied=true;}
        if(!denied)throw std::runtime_error("MSAA readback invented an actual UAV permission");
        renderer.Finish();renderer.ValidateDebugMessages();
        puts("PASS: real no-UAV readback buffer rejected before descriptor/GPU mutation");
    }catch(...){try{renderer.Finish();}catch(...){}throw;}
}
inline void RunRobustSampleCopy(ID3D12Device* device){
    const auto require=[](bool v,const char* message){if(!v)throw std::runtime_error(message);};
    Rasterizer renderer(device);GuestTextureCache cache(device);
    unsigned cases{};
    for(UINT reverse:{0u,1u}){
    GuestMultisample transfer(device);
    for(UINT number:{4u,5u,7u})for(UINT index=0;index<2;++index){
        const UINT data=reverse?(index?13:14):(index?14:13),components=data==13?3:4;
        GuestTextureDesc src;src.address=0x100000+cases*0x20000;src.width=5;src.height=3;src.pitch=8;
        src.layers=src.view_layers=2;src.samples=2;src.tile_mode=8;src.data_format=data;src.number_format=number;
        auto dst=src;dst.address+=0x10000;dst.samples=4;
        const auto sl=LayoutGuestTexture(src),dl=LayoutGuestTexture(dst);
        std::vector<std::byte> source(sl.guest_bytes,std::byte{0xa7}),destination(dl.guest_bytes,std::byte{0x5a});
        const UINT one=number==7?0x3f800000u:1u;
        const auto values=[&](UINT layer,UINT sample,UINT x,UINT y){
            std::array<UINT,4> result{};
            for(UINT c=0;c<4;++c)result[c]=(number==7?0x3f000000u:number==5?0x80000000u:0x10000000u)|
                ((layer+1)<<16)|(sample<<12)|(y<<8)|(x<<4)|c;
            if(data==13)result[3]=one;
            return result;
        };
        for(UINT layer=0;layer<2;++layer)for(UINT sample=0;sample<2;++sample)
        for(UINT y=0;y<src.height;++y)for(UINT x=0;x<src.width;++x){
            const auto value=values(layer,sample,x,y);
            memcpy(source.data()+GuestPixelOffset(src,sl,0,layer,x,y,sample),value.data(),components*4);
        }
        std::vector<Resource> linear,readback;
        std::vector<std::shared_ptr<GuestMultisample::CopyLease>> copies;
        std::vector<std::shared_ptr<GuestMultisample::Lease>> reads;
        try{
            auto& a=cache.Upload(src,source,renderer.RecordingList(),renderer.CompletedFence());
            auto& b=cache.Upload(dst,destination,renderer.RecordingList(),renderer.CompletedFence());
            require(a.texture.handle->GetDesc().SampleDesc.Count==2&&b.texture.handle->GetDesc().SampleDesc.Count==4,
                "Robust sample fixture did not create real2/4 sample resources");
            for(UINT layer=0;layer<2;++layer){
                auto copy=transfer.PrepareCopy(a.texture,b.texture,dst,dl,layer);
                copy->Record(renderer.RecordingList(),a.texture,b.texture);copies.push_back(std::move(copy));
            }
            // Inspect all four PHYSICAL channels, before guest RGB retile can
            // discard expanded A. Raw UINT avoids a float conversion oracle.
            auto physical=dst;physical.data_format=14;physical.number_format=4;
            const auto pl=LayoutGuestTexture(physical);
            const auto fp=GuestMultisample::Footprint(physical,pl);
            const auto bytes=GuestMultisample::LinearBytes(physical,fp);
            for(UINT layer=0;layer<2;++layer){
                linear.push_back(GuestTiler::Scratch(device,bytes));
                readback.push_back(Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST));
                auto read=transfer.Prepare(b.texture,linear.back(),physical,pl,layer,0,false);
                read->Record(renderer.RecordingList(),b.texture,linear.back());reads.push_back(std::move(read));
                linear.back().Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                renderer.RecordingList()->CopyBufferRegion(readback.back().handle.Get(),0,linear.back().handle.Get(),0,bytes);
            }
            cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
            for(UINT layer=0;layer<2;++layer){
                void* mapped{};Check(readback[layer].handle->Map(0,nullptr,&mapped));bool valid=true;
                for(UINT sample=0;sample<4;++sample)for(UINT y=0;y<src.height;++y)for(UINT x=0;x<src.width;++x){
                    const auto expected=sample<2?values(layer,sample,x,y):std::array<UINT,4>{0,0,0,data==13?one:0};
                    std::array<UINT,4> observed{};
                    const auto at=(u64(sample)*src.height+y)*fp.Footprint.RowPitch+u64(x)*16;
                    memcpy(observed.data(),static_cast<const std::byte*>(mapped)+at,16);
                    if(observed!=expected){if(valid)fprintf(stderr,
                        "ROBUST data=%u number=%u reverse=%u layer=%u sample=%u xy=%u,%u got=%08x,%08x,%08x,%08x expected=%08x,%08x,%08x,%08x\n",
                        data,number,reverse,layer,sample,x,y,observed[0],observed[1],observed[2],observed[3],expected[0],expected[1],expected[2],expected[3]);valid=false;}
                }
                D3D12_RANGE none{};readback[layer].handle->Unmap(0,&none);
                require(valid,"Robust MSAA sample copy lost in-range bits, zero fallback or missing-alpha default");
            }
            renderer.ValidateDebugMessages();++cases;
        }catch(...){try{renderer.Finish();}catch(...){}throw;}
    }
    }
    printf("PASS:%u real2->4 MSAA copies,RGB32/RGBA32 UINT/SINT/FLOAT,both fresh cache orders,2 layers,exact in-range words,robust zero and physical missing-alpha defaults\n",cases);
}
} // namespace NativeDX12
