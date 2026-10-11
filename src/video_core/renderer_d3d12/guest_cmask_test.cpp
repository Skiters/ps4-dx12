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
