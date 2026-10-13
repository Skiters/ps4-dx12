// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <d3d12sdklayers.h>
#include <source_location>
#include <cstdio>
#include <cmath>
using namespace NativeDX12;
namespace {
void Require(bool value,std::source_location where=std::source_location::current()){
    if(!value)throw std::runtime_error("Packed backing assertion at line "+std::to_string(where.line()));
}
void CALLBACK ValidationError(D3D12_MESSAGE_CATEGORY,D3D12_MESSAGE_SEVERITY severity,D3D12_MESSAGE_ID id,LPCSTR text,void*){
    if(severity<=D3D12_MESSAGE_SEVERITY_ERROR){fprintf(stderr,"D3D12 validation %u: %s\n",UINT(id),text);fflush(stderr);}
}
struct Mapping {
    HANDLE section{};std::byte* guest{};std::byte* alias{};
    explicit Mapping(size_t bytes){
        const auto size=(bytes+65535)&~size_t(65535);
        section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,DWORD(size>>32),DWORD(size),nullptr);
        Require(section!=nullptr);
        guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,size));
        alias=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,size));Require(guest&&alias);
    }
    ~Mapping(){if(alias)UnmapViewOfFile(alias);if(guest)UnmapViewOfFile(guest);if(section)CloseHandle(section);}
};
struct Owner:Rasterizer {
    using Rasterizer::Rasterizer;
    std::function<void()> resolve;
    bool ReadMemory(VAddr address,u64 size,bool assume=false)override{
        std::lock_guard lock(ExecutionMutex());if(resolve)resolve();return Rasterizer::ReadMemory(address,size,assume);
    }
};
u16 NativeWord(u16 value,u32 format){
    if(format==18)return u16((value>>1)|(value<<15));
    if(format==19)return u16(((value&15)<<12)|((value&240)<<4)|((value&3840)>>4)|((value&61440)>>12));
    return value;
}
void CheckPhysicalWords(ID3D12Device* device,Owner& renderer,GuestTextureCache& cache,
    GuestTextureCache::Entry& image,const std::vector<std::byte>& expected){
    const auto desc=image.texture.handle->GetDesc();Require(desc.SampleDesc.Count==1);
    struct Slice{Resource readback;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;UINT mip,layer;};
    std::vector<Slice> slices;
    for(UINT layer=0;layer<image.desc.layers;++layer)for(UINT mip=0;mip<image.desc.levels;++mip){
        UINT64 bytes{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        device->GetCopyableFootprints(&desc,mip+layer*image.desc.levels,1,0,&fp,nullptr,nullptr,&bytes);
        slices.push_back({Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST),fp,mip,layer});
    }
    auto* list=renderer.RecordingList();image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    for(auto& slice:slices){
        D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=image.texture.handle.Get();
        from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=slice.mip+slice.layer*image.desc.levels;
        to.pResource=slice.readback.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=slice.fp;
        list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    }
    const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
    for(auto& slice:slices){
        void* mapped{};D3D12_RANGE range{0,SIZE_T(slice.readback.handle->GetDesc().Width)};
        Check(slice.readback.handle->Map(0,&range,&mapped));
        const UINT pixel_bytes=image.desc.data_format==9?4:2;
        for(UINT z=0;z<slice.fp.Footprint.Depth;++z)
        for(UINT y=0;y<slice.fp.Footprint.Height;++y)for(UINT x=0;x<slice.fp.Footprint.Width;++x){
            UINT word{},observed{};memcpy(&word,expected.data()+GuestPixelOffset(image.desc,image.layout,slice.mip,
                image.desc.volume_depth?z:slice.layer,x,y),pixel_bytes);
            memcpy(&observed,static_cast<std::byte*>(mapped)+slice.fp.Offset+
                (size_t(z)*slice.fp.Footprint.Height+y)*slice.fp.Footprint.RowPitch+x*pixel_bytes,pixel_bytes);
            Require(observed==(pixel_bytes==4?word:NativeWord(u16(word),image.desc.data_format)));
        }
        D3D12_RANGE empty{};slice.readback.handle->Unmap(0,&empty);
    }
}
void CheckExpandedWords(ID3D12Device* device,Owner& renderer,GuestTextureCache& cache,
    GuestTextureCache::Entry& image,const std::vector<std::byte>& expected){
    const auto desc=image.texture.handle->GetDesc();Require(desc.SampleDesc.Count==1);
    struct Slice{Resource readback;D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;UINT mip,layer;};
    std::vector<Slice> slices;
    for(UINT layer=0;layer<image.desc.layers;++layer)for(UINT mip=0;mip<image.desc.levels;++mip){
        UINT64 bytes{};D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
        device->GetCopyableFootprints(&desc,mip+layer*image.desc.levels,1,0,&fp,nullptr,nullptr,&bytes);
        slices.push_back({Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST),fp,mip,layer});
    }
    auto* list=renderer.RecordingList();image.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    for(auto& slice:slices){
        D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=image.texture.handle.Get();
        from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;from.SubresourceIndex=slice.mip+slice.layer*image.desc.levels;
        to.pResource=slice.readback.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=slice.fp;
        list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    }
    const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
    for(auto& slice:slices){
        void* mapped{};D3D12_RANGE range{0,SIZE_T(slice.readback.handle->GetDesc().Width)};
        Check(slice.readback.handle->Map(0,&range,&mapped));
        for(UINT z=0;z<slice.fp.Footprint.Depth;++z)
        for(UINT y=0;y<slice.fp.Footprint.Height;++y)for(UINT x=0;x<slice.fp.Footprint.Width;++x){
            UINT word{};memcpy(&word,expected.data()+GuestPixelOffset(image.desc,image.layout,slice.mip,
                image.desc.volume_depth?z:slice.layer,x,y),4);
            const auto* pixel=static_cast<const std::byte*>(mapped)+slice.fp.Offset+
                (size_t(z)*slice.fp.Footprint.Height+y)*slice.fp.Footprint.RowPitch+x*16;
            for(UINT channel=0;channel<4;++channel){
                const UINT bits=channel==3?2:10,shift=channel==3?30:channel*10,sign=1u<<(bits-1);
                const UINT value=(word>>shift)&((1u<<bits)-1);const int signed_value=int(value^sign)-int(sign);
                UINT observed{};memcpy(&observed,pixel+channel*4,4);
                if(image.desc.number_format==5)Require(observed==UINT(signed_value));
                else{const float decoded=std::max(-1.0f,float(signed_value)/float(sign-1));float actual{};
                    memcpy(&actual,&observed,4);Require(std::abs(actual-decoded)<0.000001f);}
            }
        }
        D3D12_RANGE empty{};slice.readback.handle->Unmap(0,&empty);
    }
}
}
int main(int argc,char** argv){try{
    Require(argc>=4&&argc<=7);const UINT format=std::stoul(argv[1]),tile=std::stoul(argv[2]),samples=std::stoul(argv[3]);
    const UINT number=argc>=5?std::stoul(argv[4]):0,shape=argc>=6?std::stoul(argv[5]):0;
    const bool mixed=argc==7&&std::string_view(argv[6])=="mixed";
    const bool packed10=format==9&&(number==1||number==5);
    Require((packed10&&shape<=2&&(!shape||samples==1)&&(argc<7||mixed))||(argc==4&&format>=16&&format<=19));
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    auto device=CreateHardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    ComPtr<ID3D12InfoQueue1> live_messages;DWORD validation_cookie{};
    if(SUCCEEDED(device.As(&live_messages)))Check(live_messages->RegisterMessageCallback(
        ValidationError,D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&validation_cookie));
    GuestTextureDesc desc;desc.width=17;desc.height=9;desc.pitch=24;desc.layers=desc.view_layers=3;
    desc.levels=samples>1?1:2;desc.samples=samples;desc.data_format=format;desc.number_format=number;desc.tile_mode=tile;desc.writable=true;
    if(shape==1){desc.one_dimensional=true;desc.height=1;}
    if(shape==2){desc.volume_depth=4;desc.layers=desc.view_layers=1;}
    const auto layout=LayoutGuestTexture(desc);Mapping mapping(layout.guest_bytes);
    desc.address=reinterpret_cast<VAddr>(mapping.guest);const auto address=desc.address;
    std::vector<std::byte> initial(layout.guest_bytes,std::byte{0xee});
    for(UINT mip=0;mip<desc.levels;++mip)
    for(UINT layer=0;layer<(desc.volume_depth?std::max(desc.volume_depth>>mip,1u):desc.layers);++layer)
    for(UINT sample=0;sample<samples;++sample)for(UINT y=0;y<std::max(desc.height>>mip,1u);++y)
    for(UINT x=0;x<std::max(desc.width>>mip,1u);++x){
        const UINT word=packed10?0xa0080200u+x*37+y*139+layer*431+sample*1023+mip*113:
            u16(0x93b5+x*37+y*139+layer*431+sample*1023+mip*113);
        memcpy(initial.data()+GuestPixelOffset(desc,layout,mip,layer,x,y,sample),&word,packed10?4:2);
    }
    memcpy(mapping.alias,initial.data(),initial.size());auto expected=initial;
    Owner renderer(device.Get());renderer.MapMemory(address,layout.guest_bytes);
    renderer.NotifyHostMapping(address,layout.guest_bytes,true,true);GuestTextureCache cache(device.Get());
    auto& image=cache.Upload(desc,{mapping.guest,size_t(layout.guest_bytes)},renderer.RecordingList(),renderer.CompletedFence());
    if(samples==1){
        Descriptors visible(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,desc.levels,true);
        Descriptors cpu(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,desc.levels,false);
        image.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for(UINT mip=0;mip<desc.levels;++mip){
            const auto view=image.Uav(mip);device->CreateUnorderedAccessView(image.texture.handle.Get(),nullptr,&view,visible.Cpu(mip));
            device->CreateUnorderedAccessView(image.texture.handle.Get(),nullptr,&view,cpu.Cpu(mip));
            ID3D12DescriptorHeap* heaps[]{visible.Heap()};renderer.RecordingList()->SetDescriptorHeaps(1,heaps);
            const UINT values[]{packed10?0xa0080200u+mip*113:0x4321+mip*113,0,0,0};
            renderer.RecordingList()->ClearUnorderedAccessViewUint(visible.Gpu(mip),cpu.Cpu(mip),image.texture.handle.Get(),values,0,nullptr);
            for(UINT layer=0;layer<(desc.volume_depth?std::max(desc.volume_depth>>mip,1u):desc.layers);++layer)
            for(UINT y=0;y<std::max(desc.height>>mip,1u);++y)
            for(UINT x=0;x<std::max(desc.width>>mip,1u);++x){const UINT word=packed10?values[0]:u16(values[0]);
                memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,layer,x,y),&word,packed10?4:2);}
        }
        const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
    }else{const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();}
    unsigned resolves{},faults{};PageWatch* watcher{};
    const auto resolve=[&]{std::lock_guard lock(renderer.ExecutionMutex());if(!image.gpu_owned)return;
        renderer.Finish();std::vector<std::byte> result(mapping.alias,mapping.alias+layout.guest_bytes);
        GuestImageReadback readback(device.Get(),image,renderer.RecordingList());const auto fence=renderer.Flush();
        cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();readback.Resolve(renderer.CompletedFence(),result);
        Require(result==expected);memcpy(mapping.alias,result.data(),result.size());image.SetGpuOwned(false);
        ++resolves;watcher->Untrack(address,layout.guest_bytes);
    };
    PageWatch watch([&](uint64_t){++faults;resolve();});watcher=&watch;renderer.resolve=resolve;
    cache.SetOwnershipResolver([&](auto&){resolve();renderer.RecordingList();});
    // PageWatch owns whole host pages; small packed images still have a full
    // mapped page. Readback below must preserve every byte outside the image.
    Require(watch.Track(address,(layout.guest_bytes+4095)&~uint64_t(4095)));image.SetGpuOwned(true);const auto uid=image.image_uid;
    for(unsigned iteration=0;iteration<(mixed?9u:packed10?6u:4u);++iteration){
        auto view=desc;view.writable=iteration%2!=0;
        if(mixed){const UINT numbers[]{1,0,4,5,1,4,0,1,5};view.number_format=numbers[iteration];
            view.writable=iteration==2||iteration==4||iteration==6||iteration==8;}
        else if(packed10){view.writable=iteration==2||iteration==5;
            view.number_format=iteration==1||iteration==2||iteration==3?5:1;}
        const auto previous=image.texture.handle.Get();
        Require(cache.HasOwnedShaderStorage(view));
        auto& converted=cache.Upload(view,{mapping.guest,size_t(layout.guest_bytes)},renderer.RecordingList(),renderer.CompletedFence());
        Require(&converted==&image&&image.image_uid==uid&&image.texture.handle.Get()!=previous&&image.gpu_owned&&resolves==0&&faults==0);
        const auto retirement=image.format_retirement_bytes;Require(retirement>0&&!image.format_transfers.empty());
        cache.Collect(renderer.CompletedFence());Require(image.format_retirement_bytes==retirement);
        if(!view.writable&&samples==1){
            if(image.layout.expanded_bytes_per_pixel)CheckExpandedWords(device.Get(),renderer,cache,image,expected);
            else CheckPhysicalWords(device.Get(),renderer,cache,image,expected);
        }
        else{const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();}
        {
            GuestImageReadback readback(device.Get(),image,renderer.RecordingList());const auto fence=renderer.Flush();
            cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();auto result=initial;
            readback.Resolve(renderer.CompletedFence(),result);Require(result==expected);
        }
        cache.Collect(renderer.CompletedFence());Require(image.format_retirement_bytes==0&&image.format_transfers.empty());
        if(packed10){const bool expanded=image.layout.expanded_bytes_per_pixel!=0;
            Require(bool(image.expanded_guest_backing.handle)==expanded);
            Require((image.expanded_backing_bytes>0)==expanded);}
        Require(std::equal(initial.begin(),initial.end(),mapping.alias)&&resolves==0&&faults==0);
    }
    volatile auto* cpu=reinterpret_cast<volatile unsigned char*>(mapping.guest);Require(cpu[0]==std::to_integer<unsigned char>(expected[0]));
    Require(resolves==1&&faults==1&&!image.gpu_owned&&std::equal(expected.begin(),expected.end(),mapping.alias));
    renderer.resolve={};cache.SetOwnershipResolver({});cache.Invalidate(address,layout.guest_bytes,renderer.CompletedFence());
    renderer.UnmapMemory(address,layout.guest_bytes);
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);
        std::vector<std::byte> buffer(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(buffer.data());Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    Require(GetModuleHandleW(L"vulkan-1.dll")==nullptr);
    puts("PASS: packed backing GPU conversions preserve physical words, ownership, mip/layer/sample contents, padding and fence retirement");return 0;
}catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 1;}}
