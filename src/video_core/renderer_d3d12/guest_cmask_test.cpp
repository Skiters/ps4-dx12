#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include "guest_cmask_address.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
void Require(bool b){if(!b)throw std::runtime_error("CMASK test assertion failed");}
void CheckCmaskAddressing() {
    using P=AmdGpu::PipeConfig;
    struct Golden {P pipe;uint32_t x,y;uint64_t byte;uint32_t bit;};
    // Fixed per-pipe vectors from the pinned AddrLib equations; they are not
    // generated with the native layout helper. Every surface here is 1024x512.
    const Golden vectors[]={
        {P::P2,8,0,256,0},{P::P2,16,0,3,4},{P::P2,0,16,1,0},
        {P::P8_32x32_16x16,8,0,256,0},{P::P8_32x32_16x16,16,0,769,4},
        {P::P8_32x32_16x16,0,8,256,4},
        {P::P8_32x32_8x16,8,0,512,0},{P::P8_32x32_8x16,16,0,257,4},
        {P::P8_32x32_8x16,0,16,512,4},
        {P::P16_32x32_8x16,8,0,512,0},{P::P16_32x32_8x16,16,0,257,4},
        {P::P16_32x32_8x16,0,8,256,0},{P::P16_32x32_8x16,32,0,1024,0},
    };
    for(bool linear:{false,true})for(const auto& v:vectors){
        const GuestCmaskAddress address(1024,512,v.pipe,linear);
        Require(address.slice_bytes==4096);
        for(uint32_t layer:{0u,1u,2u}){
            const auto [byte,bit]=address.At(layer,v.x,v.y);
            Require(byte==uint64_t(layer)*4096+v.byte&&bit==v.bit);
        }
    }
    struct Boundary {P pipe;bool linear;uint32_t x,y;uint64_t byte;uint32_t bit;};
    const Boundary boundaries[]={
        {P::P2,false,256,0,128,0},{P::P2,true,256,0,32,0},
        {P::P8_32x32_16x16,false,512,0,128,0},
        {P::P8_32x32_16x16,true,512,0,16,0},
        {P::P16_32x32_8x16,false,512,0,128,0},
        {P::P16_32x32_8x16,true,512,0,8,0},
        {P::P16_32x32_8x16,false,0,32,2056,0},
        {P::P16_32x32_8x16,true,0,32,2064,0},
    };
    for(const auto& v:boundaries){
        const GuestCmaskAddress address(1024,512,v.pipe,v.linear);
        Require(address.At(1,v.x,v.y)==std::pair<uint64_t,uint32_t>{4096+v.byte,v.bit});
    }
    for(P pipe:{P::P2,P::P8_32x32_8x16,P::P8_32x32_16x16,P::P16_32x32_8x16})
    for(bool linear:{false,true})for(uint32_t width:{17u,129u,1024u})
    for(uint32_t rows:{9u,65u,512u}){
        const GuestCmaskAddress address(width,rows,pipe,linear);
        std::vector<bool> used(size_t(address.slice_bytes*2));
        for(uint32_t y=0;y<address.height;y+=8)for(uint32_t x=0;x<address.pitch;x+=8){
            const auto [byte,bit]=address.At(0,x,y);
            Require(bit==0||bit==4);
            const auto nibble=size_t(byte*2+bit/4);
            Require(nibble<used.size()&&!used[nibble]);used[nibble]=true;
            Require(address.At(2,x,y).first==address.slice_bytes*2+byte);
        }
    }
}
void CheckCmaskPartialSurfaces() {
    // The two zero nibbles are deliberately placed using fixed addresses:
    // (16,0) and (0,16). All other pixels, samples, layers and padding survive.
    struct Case {AmdGpu::TileMode tile;bool alt;uint64_t first,second;uint32_t second_bit;};
    const Case cases[]={
        {AmdGpu::TileMode::Thin1DThin,false,769,512,0},
        {AmdGpu::TileMode::DisplayThinPrt,false,257,512,4},
        {AmdGpu::TileMode::Thin1DThin,true,257,512,4},
    };
    for(const auto& c:cases)for(bool linear:{false,true})for(uint32_t log2_samples:{0u,2u}){
        AmdGpu::ColorBuffer b{};b.base_address=0x10000;b.cmask_base_address=0x20000;
        b.info.format=10;b.info.fast_clear=1;b.info.cmask_is_linear=linear;
        b.info.alt_tile_mode=c.alt;b.attrib.tile_mode_index=c.tile;
        b.attrib.num_fragments_log2=log2_samples;b.pitch.tile_max=127;b.slice.tile_max=8191;
        b.view.slice_start=1;b.view.slice_max=2;b.cmask_slice.tile_max=31;
        b.clear_word0=0x80402010;
        AmdGpu::CbDbExtent extent{};extent.width=41;extent.height=25;
        auto desc=DescribeGuestColorBuffer(b,extent);desc.alt_tile=c.alt;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}),expected=color;
        std::vector<std::byte> metadata(GuestCmaskBytes(b),std::byte{0xff});
        metadata[size_t(4096+c.first)]&=std::byte{0x0f};
        metadata[size_t(4096+c.second)]&=std::byte(c.second_bit?0x0f:0xf0);
        for(uint32_t sample=0;sample<desc.samples;++sample)
        for(uint32_t y=0;y<desc.height;++y)for(uint32_t x=0;x<desc.width;++x)
            if((x>=16&&x<24&&y<8)||(x<8&&y>=16&&y<24)){
                const auto offset=GuestPixelOffset(desc,layout,0,1,x,y,sample);
                memcpy(expected.data()+offset,&b.clear_word0,4);
            }
        Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
        Require(color==expected);
        Require(std::all_of(metadata.begin(),metadata.end(),[](auto v){return v==std::byte{0xff};}));
        Require(!GuestColorClearPending(b,desc,layout,metadata));
    }
}
int main(){try {
    CheckCmaskAddressing();CheckCmaskPartialSurfaces();
    AmdGpu::ColorBuffer b{}; b.base_address=0x10000; b.cmask_base_address=0x20000;
    b.info.format=10; b.info.fast_clear=1; b.attrib.tile_mode_index=AmdGpu::TileMode(13);
    b.pitch.tile_max=63; b.slice.tile_max=2047; b.view.slice_start=1; b.view.slice_max=2;
    b.clear_word0=0x80402010; b.cmask_slice.tile_max=15;
    AmdGpu::CbDbExtent extent{}; extent.width=17; extent.height=9;
    auto desc=DescribeGuestColorBuffer(b,extent); auto layout=LayoutGuestTexture(desc);
    Require(GuestCmaskBytes(b)==6144);
    std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}), metadata(6144,std::byte{0xff});
    // Layer 1: first 8x8 tile is a clear (zero nibble), adjacent tile is expanded.
    metadata[2048]=std::byte{0xf0};
    Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
    const auto clear=GuestPixelOffset(desc,layout,0,1,0,0), expanded=GuestPixelOffset(desc,layout,0,1,8,0);
    Require(memcmp(color.data()+clear,&b.clear_word0,4)==0);
    Require(color[expanded]==std::byte{0xee});
    Require(metadata[2048]==std::byte{0xff});
    Require(!MaterializeGuestColorClear(b,desc,layout,color,metadata));
    // An untouched view layer remains untouched.
    std::fill(metadata.begin(),metadata.begin()+2048,std::byte{0});
    Require(!GuestColorClearPending(b,desc,layout,metadata));
    std::fill(metadata.begin()+2048,metadata.begin()+4096,std::byte{0xff});
    for(uint32_t log2_samples=1;log2_samples<=4;++log2_samples){
        auto msaa=b;msaa.attrib.num_fragments_log2=log2_samples;
        const auto msaa_desc=DescribeGuestColorBuffer(msaa,extent);
        const auto msaa_layout=LayoutGuestTexture(msaa_desc);
        std::vector<std::byte> samples(msaa_layout.guest_bytes,std::byte{0xee});
        std::vector<std::byte> masks(6144,std::byte{0xff});masks[2048]=std::byte{0xf0};
        Require(MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        Require(std::count_if(samples.begin(),samples.end(),[](auto value){return value!=std::byte{0xee};})==
            8*8*4*msaa_desc.samples);
        for(uint32_t layer=0;layer<3;++layer){
            const auto decoded=DecodeGuestSubresource(msaa_desc,msaa_layout,samples,0,layer);
            for(uint32_t sample=0;sample<msaa_desc.samples;++sample)
            for(uint32_t y=0;y<9;++y)for(uint32_t x=0;x<17;++x){
                uint32_t actual{};memcpy(&actual,decoded.data()+((sample*9+y)*17+x)*4,4);
                Require(actual==(layer==1&&x<8&&y<8?msaa.clear_word0:0xeeeeeeee));
            }
        }
        Require(!MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        std::fill(samples.begin(),samples.end(),std::byte{0xee});
        std::fill(masks.begin()+2048,masks.begin()+4096,std::byte{0});
        Require(MaterializeGuestColorClear(msaa,msaa_desc,msaa_layout,samples,masks));
        Require(std::count_if(samples.begin(),samples.end(),[](auto value){return value!=std::byte{0xee};})==
            17*9*4*msaa_desc.samples);
        const auto decoded=DecodeGuestSubresource(msaa_desc,msaa_layout,samples,0,1);
        for(size_t offset=0;offset<decoded.size();offset+=4){
            uint32_t actual{};memcpy(&actual,decoded.data()+offset,4);Require(actual==msaa.clear_word0);
        }
    }
    ComPtr<IDXGIFactory6> factory; Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 ad{};Check(DX12_API_CALL(a->GetDesc1(&ad)));if(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    Require(device!=nullptr); ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC q{}; Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator; Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list; Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    GuestTextureCache cache(device.Get()); auto& entry=cache.Upload(desc,color,list.Get(),0); GuestImageReadback readback(device.Get(),entry,list.Get());
    Check(DX12_API_CALL(list->Close())); Timeline timeline(device.Get()); auto fence=timeline.Submit(queue.Get(),list.Get()); cache.Submitted(fence); readback.Submitted(fence); timeline.Wait(fence); std::vector<std::byte> restored=color; readback.Resolve(timeline.Completed(),restored);
    Require(restored==color);
    puts("PASS: mixed CMASK tile equation, exact changed/unchanged pixels, idempotence, untouched layer and D3D12 readback"); return 0;
} catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
