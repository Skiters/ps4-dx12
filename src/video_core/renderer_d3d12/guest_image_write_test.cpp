#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_image_readback.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <cstring>
#include <d3d12sdklayers.h>
using namespace NativeDX12;
static ComPtr<ID3D12InfoQueue> debug_queue;
int main(int argc,char** argv){try {
    const std::string_view mode=argc>1?argv[1]:"";
    const bool volume=mode=="volume"||mode=="deep-volume";
    const bool deep_volume=mode=="deep-volume";
    const bool rgb96=mode.starts_with("rgb96");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware D3D12 device");
    DX12_API_CALL(device.As(&debug_queue));
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    Timeline timeline(device.Get());
    GuestTextureDesc desc;desc.width=17;desc.height=9;desc.pitch=24;desc.levels=3;desc.layers=2;desc.view_layers=2;
    desc.data_format=4;desc.number_format=4;desc.writable=true;
    if(rgb96){desc.data_format=13;desc.number_format=mode=="rgb96-float"?7:4;
        if(mode=="rgb96-linear")desc.tile_mode=8;}
    if(volume){desc.volume_depth=7;desc.layers=desc.view_layers=1;desc.tile_mode=19;}
    if(deep_volume){
        desc.width=desc.height=desc.pitch=1;desc.volume_depth=64;desc.levels=7;
        auto invalid=desc;invalid.volume_depth=0;invalid.layers=invalid.view_layers=64;
        bool rejected=false;
        try {(void)LayoutGuestTexture(invalid);}catch(const std::runtime_error&){rejected=true;}
        if(!rejected)throw std::runtime_error("Array slices incorrectly extended mip chain");
        invalid=desc;invalid.width=invalid.pitch=4096;
        rejected=false;
        try {(void)LayoutGuestTexture(invalid);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("Oversized 3D width accepted");
    }
    auto layout=LayoutGuestTexture(desc);std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xee}),expected=guest;
    if(rgb96){
        if(layout.bytes_per_pixel!=12||layout.HostBytesPerPixel()!=16)
            throw std::runtime_error("RGB96 guest/host allocation mismatch");
        for(UINT mip=0;mip<desc.levels;++mip)for(UINT layer=0;layer<desc.layers;++layer)
        for(UINT y=0;y<std::max(desc.height>>mip,1u);++y)for(UINT x=0;x<std::max(desc.width>>mip,1u);++x){
            const UINT value[]{0x7fc00001u+x,0x80000000u+y,0xff800000u+layer+mip};
            memcpy(guest.data()+GuestPixelOffset(desc,layout,mip,layer,x,y),value,12);
        }
        expected=guest;
    }
    if(deep_volume){
        for(UINT mip=0;mip<desc.levels;++mip)for(UINT z=0;z<std::max(desc.volume_depth>>mip,1u);++z){
            const UINT value=0x55000000|(mip<<16)|z;
            memcpy(guest.data()+GuestPixelOffset(desc,layout,mip,z,0,0),&value,4);
        }
        expected=guest;
    }
    if(volume&&!deep_volume&&(GuestPixelOffset(desc,layout,0,1,0,0)!=32||
       GuestPixelOffset(desc,layout,0,2,0,0)!=128||GuestPixelOffset(desc,layout,0,0,4,0)!=256||
       GuestPixelOffset(desc,layout,0,0,0,4)!=512))throw std::runtime_error("PS4 thick32 golden microtile mismatch");
    GuestTextureCache cache(device.Get());auto& entry=cache.Upload(desc,guest,list.Get(),0);
    if(deep_volume||rgb96){
        // Verify the guest upload independently before overwriting with UAVs.
        GuestImageReadback uploaded(device.Get(),entry,list.Get());
        Check(DX12_API_CALL(list->Close()));
        const auto upload_fence=timeline.Submit(queue.Get(),list.Get());
        cache.Submitted(upload_fence);uploaded.Submitted(upload_fence);timeline.Wait(upload_fence);
        auto observed=guest;uploaded.Resolve(timeline.Completed(),observed);
        if(observed!=guest)throw std::runtime_error("Native initial upload/readback changed guest bytes");
        cache.Collect(timeline.Completed());
        Check(DX12_API_CALL(allocator->Reset()));Check(DX12_API_CALL(list->Reset(allocator.Get(),nullptr)));
    }
    Descriptors visible(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,desc.levels,true),cpu(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,desc.levels,false);
    ID3D12DescriptorHeap* heaps[]{visible.Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    entry.texture.Transition(list.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    for(UINT mip=0;mip<desc.levels;++mip) {
        auto writable_view=desc;writable_view.number_format=4;
        auto v=visible.Allocate(0),c=cpu.Allocate(0);auto view=entry.Uav(writable_view,mip);
        DX12_API_CALL(device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,visible.Cpu(v)));
        DX12_API_CALL(device->CreateUnorderedAccessView(entry.texture.handle.Get(),nullptr,&view,cpu.Cpu(c)));
        const UINT value[]{0x12340000+mip,rgb96?0x7fc00011u:0u,rgb96?0x80000000u:0u,0xdeadbeefu};
        DX12_API_CALL(list->ClearUnorderedAccessViewUint(visible.Gpu(v),cpu.Cpu(c),entry.texture.handle.Get(),value,0,nullptr));
        // Expected storage uses the independently tested PS4 addressing, leaves
        // all padding at its original sentinel and changes every layer/mip.
        for(UINT layer=0;layer<(volume?std::max(desc.volume_depth>>mip,1u):desc.layers);++layer)for(UINT y=0;y<std::max(desc.height>>mip,1u);++y)
            for(UINT x=0;x<std::max(desc.width>>mip,1u);++x) {
                auto offset=GuestPixelOffset(desc,layout,mip,layer,x,y);memcpy(expected.data()+offset,value,layout.bytes_per_pixel);
            }
    }
    if(rgb96){
        auto sampled=desc;sampled.writable=false;sampled.swizzle={7,4,7,6};
        const auto view=entry.Srv(sampled);
        if(D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(0,view.Shader4ComponentMapping)!=D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1||
           D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(2,view.Shader4ComponentMapping)!=D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1)
            throw std::runtime_error("RGB96 absent alpha was exposed through the sampled swizzle");
    }
    GuestImageReadback readback(device.Get(),entry,list.Get());
    bool rejected=false;try{readback.Resolve(0,guest);}catch(const std::logic_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Unsubmitted image readback accepted");
    Check(DX12_API_CALL(list->Close()));auto fence=timeline.Submit(queue.Get(),list.Get());
    cache.Submitted(fence);readback.Submitted(fence);timeline.Wait(fence);readback.Resolve(timeline.Completed(),guest);
    if(guest!=expected)throw std::runtime_error("GPU image writes did not preserve guest tiling/mips/layers/padding");
    cache.Collect(timeline.Completed());
    puts("PASS: native typed UAV GPU writes -> all mip/layer footprints -> PS4 tiled guest memory; padding preserved, retirement enforced");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());if(debug_queue)for(UINT64 i=0;i<DX12_API_CALL(debug_queue->GetNumStoredMessages());++i){SIZE_T size{};DX12_API_CALL(debug_queue->GetMessage(i,nullptr,&size));std::vector<std::byte> data(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());if(SUCCEEDED(DX12_API_CALL(debug_queue->GetMessage(i,message,&size))))fprintf(stderr,"D3D12: %s\n",message->pDescription);}return 1;}}
