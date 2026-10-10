#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include "video_core/amdgpu/regs_color.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
void Require(bool b){if(!b)throw std::runtime_error("CMASK test assertion failed");}
int main(){try {
    AmdGpu::ColorBuffer b{};b.base_address=0x10000;b.cmask_base_address=0x20000;
    b.info.format=10;b.info.fast_clear=1;b.attrib.tile_mode_index=AmdGpu::TileMode(13);
    b.pitch.tile_max=2;b.slice.tile_max=5;b.view.slice_start=1;b.view.slice_max=2;
    b.clear_word0=0x80402010;b.cmask_slice.tile_max=0;
    AmdGpu::CbDbExtent extent{};extent.width=17;extent.height=9;
    auto desc=DescribeGuestColorBuffer(b,extent);auto layout=LayoutGuestTexture(desc);
    Require(GuestCmaskBytes(b)==384);
    std::vector<std::byte> color(layout.guest_bytes,std::byte{0xee}),metadata(384,std::byte{0xff});
    std::fill_n(metadata.begin()+128,128,std::byte{0});
    // Invalid mixed metadata must leave both planes unchanged.
    metadata[256]=std::byte{0x0f};auto before=color;auto meta_before=metadata;bool rejected=false;
    try{MaterializeGuestColorClear(b,desc,layout,color,metadata);}catch(const std::runtime_error&){rejected=true;}
    Require(rejected&&color==before&&metadata==meta_before);metadata[256]=std::byte{0xff};
    Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
    auto expected=before;
    for(UINT y=0;y<9;++y)for(UINT x=0;x<17;++x) {
        auto offset=GuestPixelOffset(desc,layout,0,1,x,y);memcpy(expected.data()+offset,&b.clear_word0,4);
    }
    Require(color==expected);
    Require(std::all_of(metadata.begin(),metadata.end(),[](auto v){return v==std::byte{0xff};}));
    Require(!MaterializeGuestColorClear(b,desc,layout,color,metadata));
    // An enabled fast-clear flag with expanded metadata requires no color
    // access at all. In production the color plane may be GPU-owned/guarded.
    Require(!GuestColorClearPending(b,desc,layout,metadata));
    Require(!MaterializeGuestColorClear(b,desc,layout,{},metadata));
    metadata[0]=std::byte{0}; // unselected layer must not trigger readback
    Require(!GuestColorClearPending(b,desc,layout,metadata));
    metadata[0]=std::byte{0xff};
    std::fill_n(metadata.begin()+128,128,std::byte{0});
    Require(GuestColorClearPending(b,desc,layout,metadata));
    std::fill_n(metadata.begin()+128,128,std::byte{0xff});
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    Require(device!=nullptr);ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    GuestTextureCache cache(device.Get());auto& entry=cache.Upload(desc,color,list.Get(),0);
    GuestImageReadback readback(device.Get(),entry,list.Get());Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());auto fence=timeline.Submit(queue.Get(),list.Get());
    cache.Submitted(fence);readback.Submitted(fence);timeline.Wait(fence);readback.Resolve(timeline.Completed(),color);
    Require(color==expected);
    // 64-bit and 128-bit register packing, selected layer range and no repeat clear.
    for(UINT format:{12u,14u}) {
        b.info.format=format;b.info.number_type=7;b.view.slice_start=0;b.view.slice_max=0;
        b.clear_word0=0x12345678;b.clear_word1=0x9abcdef0;
        desc=DescribeGuestColorBuffer(b,extent);layout=LayoutGuestTexture(desc);
        color.assign(layout.guest_bytes,std::byte{0xee});metadata.assign(GuestCmaskBytes(b),std::byte{0});
        Require(MaterializeGuestColorClear(b,desc,layout,color,metadata));
        auto linear=DecodeGuestSubresource(desc,layout,color,0,0);
        const UINT values[]{b.clear_word0,format==14?b.clear_word0:b.clear_word1,b.clear_word0,b.clear_word1};
        for(size_t offset=0;offset<linear.size();offset+=layout.bytes_per_pixel)
            Require(memcmp(linear.data()+offset,values,layout.bytes_per_pixel)==0);
    }
    puts("PASS: native CMASK fast-clear expansion, layer/padding preservation, idempotence, invalid-state atomicity, 32/64/128-bit packing and actual D3D12 upload/readback");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
