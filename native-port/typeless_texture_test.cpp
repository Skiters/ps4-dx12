// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    Rasterizer renderer(device.Get());GuestTextureCache cache(device.Get());
    struct FormatCase{u32 data,component_bits,components;};
    for(auto test:{FormatCase{1,8,1},{2,16,1},{3,8,2},{4,32,1},{5,16,2},
        {9,10,4},{10,8,4},{11,32,2},{12,16,4},{14,32,4}}){
        GuestTextureDesc desc;desc.address=u64(test.data)<<20;desc.width=9;desc.height=5;desc.pitch=16;
        desc.levels=2;desc.layers=desc.view_layers=2;desc.data_format=test.data;desc.number_format=4;desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xee}),expected=guest;
        auto& entry=cache.Upload(desc,guest,renderer.RecordingList(),renderer.CompletedFence());
        if(entry.texture.handle->GetDesc().Format==layout.format)throw std::runtime_error("Writable resource remains fully typed");
        Descriptors visible(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true),cpu(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,false);
        ID3D12DescriptorHeap* heaps[]{visible.Heap()};auto* list=renderer.RecordingList();list->SetDescriptorHeaps(1,heaps);
        entry.texture.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        for(u32 mip=0;mip<desc.levels;++mip){auto v=visible.Allocate(0),c=cpu.Allocate(0);auto view=entry.Uav(mip);
            device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,visible.Cpu(v));
            device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,cpu.Cpu(c));
            const UINT value[]{15,15,15,3};list->ClearUnorderedAccessViewUint(visible.Gpu(v),cpu.Cpu(c),entry.texture.handle.Get(),value,0,nullptr);
            std::array<std::byte,16> pixel{};
            if(test.data==9){const u32 packed=15|(15<<10)|(15<<20)|(3u<<30);memcpy(pixel.data(),&packed,4);}
            else for(u32 component=0;component<test.components;++component)
                memcpy(pixel.data()+component*test.component_bits/8,&value[component],test.component_bits/8);
            for(u32 layer=0;layer<desc.layers;++layer)for(u32 y=0;y<std::max(desc.height>>mip,1u);++y)
                for(u32 x=0;x<std::max(desc.width>>mip,1u);++x)
                    memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,layer,x,y),pixel.data(),layout.bytes_per_pixel);
        }
        GuestImageReadback readback(device.Get(),entry,list);const auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);
        renderer.Finish();readback.Resolve(renderer.CompletedFence(),guest);cache.Collect(renderer.CompletedFence());
        if(guest!=expected)throw std::runtime_error("Typeless upload/UAV/readback corrupted format "+std::to_string(test.data));
        auto alternate=desc;alternate.number_format=test.data==9?0:test.data==10?9:5;
        const auto alternate_layout=LayoutGuestTexture(alternate);
        auto& reused=cache.Upload(alternate,guest,renderer.RecordingList(),renderer.CompletedFence());
        if(&reused!=&entry)throw std::runtime_error("Alternate number format duplicated texture storage");
        auto view=reused.Uav(alternate,0);
        auto swizzled=alternate;swizzled.swizzle={5,4,7,6};
        const auto swizzled_view=reused.Uav(swizzled,0);
        if(swizzled_view.Format!=view.Format)throw std::runtime_error("Shader swizzle changed the physical UAV format");
        if(view.Format!=alternate_layout.format)throw std::runtime_error("Alternate UAV retained prior number format");
        device->CreateUnorderedAccessView(reused.texture.handle.Get(),nullptr,&view,cpu.Cpu(0));
        const auto alternate_fence=renderer.Flush();cache.Submitted(alternate_fence);renderer.Finish();cache.Collect(renderer.CompletedFence());
        renderer.ValidateDebugMessages();
    }
    puts("PASS: ten typeless UAV families preserve guest bits, mip/layer padding and typed view reuse");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
