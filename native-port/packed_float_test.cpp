// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <d3d12sdklayers.h>
#include <source_location>
#include <cstdio>
#include <string_view>
using namespace NativeDX12;
namespace {
void Require(bool value,std::source_location where=std::source_location::current()){
    if(!value)throw std::runtime_error("Packed float assertion at line "+std::to_string(where.line()));
}
struct Mapping{
    std::byte* guest{};
    explicit Mapping(size_t bytes){guest=static_cast<std::byte*>(VirtualAlloc(nullptr,
        (bytes+65535)&~size_t(65535),MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Require(guest!=nullptr);}
    ~Mapping(){if(guest)VirtualFree(guest,0,MEM_RELEASE);}
};
constexpr std::array<u32,16> words{0,1,0xffffffff,0x7ff,0x7c1,0x781,0x7fe,
    0x3e0,0xffe003ff,0xf8000000,0x80000000,0x00010001,0xaaaaaaaa,0x55555555,
    0x12345678,0x87654321};
void ReadExact(ID3D12Device* device,Rasterizer& renderer,GuestTextureCache& cache,
    GuestTextureCache::Entry& image,const std::vector<std::byte>& expected){
    GuestImageReadback readback(device,image,renderer.RecordingList());
    const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
    auto observed=expected;
    for(UINT mip=0;mip<image.desc.levels;++mip)for(UINT layer=0;layer<image.desc.layers;++layer)
    for(UINT sample=0;sample<image.desc.samples;++sample)
    for(UINT y=0;y<std::max(1u,image.desc.height>>mip);++y)for(UINT x=0;x<std::max(1u,image.desc.width>>mip);++x){
        const auto offset=GuestPixelOffset(image.desc,image.layout,mip,layer,x,y,sample);
        for(UINT byte=0;byte<4;++byte)observed[offset+byte]^=std::byte{0xff};
    }
    readback.Resolve(renderer.CompletedFence(),observed);
    Require(observed==expected);
}
void Case(ID3D12Device* device,u32 format,u32 tile,UINT samples){
    GuestTextureDesc desc;desc.width=17;desc.height=9;desc.pitch=24;
    desc.layers=desc.view_layers=3;desc.levels=samples==1?3:1;desc.samples=samples;
    desc.data_format=format;desc.number_format=7;desc.tile_mode=tile;
    const auto layout=LayoutGuestTexture(desc);Mapping memory(layout.guest_bytes);
    desc.address=reinterpret_cast<VAddr>(memory.guest);
    Rasterizer renderer(device);GuestTextureCache cache(device);
    std::vector<std::byte> expected(layout.guest_bytes,std::byte{0xd7});
    const auto fill=[&](UINT generation){
        for(UINT mip=0;mip<desc.levels;++mip)for(UINT layer=0;layer<desc.layers;++layer)
        for(UINT sample=0;sample<samples;++sample)
        for(UINT y=0;y<std::max(1u,desc.height>>mip);++y)for(UINT x=0;x<std::max(1u,desc.width>>mip);++x){
            const auto word=words[(x+y*3+layer*5+sample*7+mip*11+generation)%words.size()];
            memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,layer,x,y,sample),&word,4);
        }
        memcpy(memory.guest,expected.data(),expected.size());
    };
    fill(0);
    for(UINT generation=0;generation<3;++generation){
        if(generation==2)fill(1);
        auto& image=cache.Upload(desc,{memory.guest,size_t(layout.guest_bytes)},
            renderer.RecordingList(),renderer.CompletedFence());
        Require(image.texture.AllowsCast(layout.format)&&image.texture.AllowsCast(DXGI_FORMAT_R32_UINT));
        ReadExact(device,renderer,cache,image,expected);
        Require(std::equal(expected.begin(),expected.end(),memory.guest));
        auto& warm=cache.Upload(desc,{memory.guest,size_t(layout.guest_bytes)},
            renderer.RecordingList(),renderer.CompletedFence());
        Require(&warm==&image);ReadExact(device,renderer,cache,warm,expected);
        if(samples>1){
            cache.SetBackingSamples(image,1,renderer.RecordingList());
            const auto single=image.texture;
            Require(single.AllowsCast(DXGI_FORMAT_R32_UINT)&&single.handle->GetDesc().SampleDesc.Count==1);
            cache.SetBackingSamples(image,samples,renderer.RecordingList());
            auto broadcast=expected;
            for(UINT layer=0;layer<desc.layers;++layer)for(UINT sample=1;sample<samples;++sample)
            for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
                memcpy(broadcast.data()+GuestPixelOffset(desc,layout,0,layer,x,y,sample),
                    expected.data()+GuestPixelOffset(desc,layout,0,layer,x,y,0),4);
            ReadExact(device,renderer,cache,image,broadcast);
            Require(std::equal(expected.begin(),expected.end(),memory.guest));
            auto retained=single;
            cache.Collect(renderer.CompletedFence());Require(retained.handle!=nullptr);
            cache.Invalidate(desc.address,layout.guest_bytes,renderer.CompletedFence());
        }
        else cache.Collect(renderer.CompletedFence());
    }
}
}
int main(int argc,char** argv){try{
    const bool r11_only=argc==2&&std::string_view(argv[1])=="r11";
    const bool r9_ms=argc==2&&std::string_view(argv[1])=="r9-ms";
    if(argc!=1&&!r11_only&&!r9_ms)throw std::invalid_argument("Expected no arguments, r11 or r9-ms");
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    auto device=CreateHardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    PlacedTextureAllocator allocator(device.Get());Require(allocator.SupportsFormatCasting());
    for(const auto format:{6u,34u}){
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format==6?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R9G9B9E5_SHAREDEXP};
        Check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)));
        printf("FORMAT data=%u support1=%08x support2=%08x\n",format,unsigned(support.Support1),unsigned(support.Support2));
    }
    for(const auto format:{6u,34u})for(const auto tile:{8u,13u})for(const auto samples:{1u,4u}){
        if(r11_only&&format!=6)continue;
        if(r9_ms&&(format!=34||tile!=8||samples!=4))continue;
        printf("CASE data=%u tile=%u samples=%u\n",format,tile,samples);fflush(stdout);
        Case(device.Get(),format,tile,samples);
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){
        SIZE_T bytes{};messages->GetMessage(i,nullptr,&bytes);std::vector<std::byte> storage(bytes);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());Check(messages->GetMessage(i,message,&bytes));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    Require(GetModuleHandleW(L"vulkan-1.dll")==nullptr);
    puts("PASS: packed float raw bits survive native upload/readback, mips, layers, samples, backing changes and padding");return 0;
}catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 1;}}
