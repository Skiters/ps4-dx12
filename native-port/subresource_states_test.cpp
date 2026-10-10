// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(int argc,char**){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    const bool enhanced=argc>1;
    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options{};
    if(enhanced&&(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,&options,sizeof(options)))||!options.EnhancedBarriersSupported))return 77;
    Rasterizer renderer(device.Get());GuestTextureCache cache(device.Get());
    for(bool batched:{false,true})for(bool volume:{false,true}){
        GuestTextureDesc desc;desc.address=(volume?0x20000:0x10000)+(batched?0x40000:0);desc.width=9;desc.height=5;desc.pitch=16;
        desc.levels=2;desc.layers=desc.view_layers=volume?1:2;desc.volume_depth=volume?4:0;
        desc.tile_mode=volume?19:13;desc.data_format=4;desc.number_format=4;desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);
        std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xee}),expected=guest;
        auto& entry=cache.Upload(desc,guest,renderer.RecordingList(),renderer.CompletedFence());
        if(entry.texture.SubresourceCount()!=(volume?2:4))throw std::runtime_error("Wrong 3D/array subresource count");
        auto* list=renderer.RecordingList();
        if(enhanced){BarrierBatch initial;initial.Transition(entry.texture,D3D12_RESOURCE_STATE_COPY_SOURCE);initial.Flush(list);
            if(!entry.texture.enhanced_barriers)throw std::runtime_error("Enhanced test did not migrate barriers");}
        const auto original=entry.texture.state;
        {BarrierBatch roundtrip;
            roundtrip.Transition(entry.texture,D3D12_RESOURCE_STATE_COPY_DEST);
            roundtrip.Transition(entry.texture,D3D12_RESOURCE_STATE_COPY_SOURCE,0);
            roundtrip.Transition(entry.texture,original);
            if(entry.texture.state!=original)throw std::runtime_error("Queued barriers changed live state before flush");
            roundtrip.Flush(list);
            if(entry.texture.state!=original)throw std::runtime_error("Barrier batch lost A -> B -> A transition");}
        {BarrierBatch reusable;reusable.Transition(entry.texture,original);reusable.Flush(list);
            entry.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            reusable.Transition(entry.texture,original);reusable.Flush(list);
            if(entry.texture.state!=original)throw std::runtime_error("Empty flush retained stale planned state");}
        const UINT subresource=volume?0:2;
        entry.texture.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,subresource);
        Descriptors visible(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true),cpu(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,false);
        auto v=visible.Allocate(0),c=cpu.Allocate(0);auto view_desc=desc;
        if(!volume){view_desc.first_layer=1;view_desc.view_layers=1;}
        auto view=entry.Uav(view_desc,0);
        device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,visible.Cpu(v));
        device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,cpu.Cpu(c));
        ID3D12DescriptorHeap* heaps[]{visible.Heap()};list->SetDescriptorHeaps(1,heaps);
        const UINT value[]{0x12345678,0,0,0};list->ClearUnorderedAccessViewUint(visible.Gpu(v),cpu.Cpu(c),entry.texture.handle.Get(),value,0,nullptr);
        for(UINT layer=volume?0:1;layer<(volume?4u:2u);++layer)for(UINT y=0;y<desc.height;++y)for(UINT x=0;x<desc.width;++x)
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,layer,x,y),value,4);
        if(entry.texture.subresource_states.empty())throw std::runtime_error("Fixture did not retain mixed subresource states");
        if(batched){BarrierBatch reconcile;reconcile.Transition(entry.texture,D3D12_RESOURCE_STATE_COPY_SOURCE);reconcile.Flush(list);}
        GuestImageReadback readback(device.Get(),entry,list);
        if(!entry.texture.subresource_states.empty()||entry.texture.state!=D3D12_RESOURCE_STATE_COPY_SOURCE)
            throw std::runtime_error("Whole-resource transition did not converge mixed states");
        auto fence=renderer.Flush();cache.Submitted(fence);readback.Submitted(fence);renderer.Finish();
        readback.Resolve(renderer.CompletedFence(),guest);cache.Collect(renderer.CompletedFence());
        if(guest!=expected)throw std::runtime_error("Mixed barriers corrupted mip/layer/volume data");
        renderer.ValidateDebugMessages();
    }
    puts("PASS: independent 2D-array/3D mip states converge for readback without touching unrelated storage");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
