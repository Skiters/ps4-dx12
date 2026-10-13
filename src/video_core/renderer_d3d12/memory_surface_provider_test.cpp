#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "memory_surface_provider.h"
#include "guest_texture_cache.h"
#include "texture_resources.h"
#include "backend_factory.h"
#include <cstdio>
#include <cstring>
#include <d3d12sdklayers.h>
using namespace NativeDX12;
struct TestRenderer:Rasterizer {
    GuestTextureCache cache;
    bool hit{};
    TestRenderer(ID3D12Device* d):Rasterizer(d),cache(d){}
    Resource* CoherentDisplayTexture(VAddr a,UINT w,UINT h,UINT p)override{
        auto* result=cache.CoherentDisplayTexture(a,w,h,p,CompletedFence());hit=bool(result);return result;
    }
};
int main(){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
    auto device=CreateHardwareDevice();D3D12_COMMAND_QUEUE_DESC q{};
    ComPtr<ID3D12CommandQueue> queue;Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    TestRenderer renderer(device.Get());
    GuestTextureDesc desc;desc.width=17;desc.height=9;desc.pitch=32;desc.tile_mode=10;
    auto layout=LayoutGuestTexture(desc);
    std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xee});desc.address=reinterpret_cast<VAddr>(guest.data());
    renderer.MapMemory(desc.address,guest.size());
    std::vector<std::byte> linear(desc.width*desc.height*4);
    for(size_t i=0;i<linear.size();++i)linear[i]=std::byte((i*13+37)&255);
    EncodeGuestSubresource(desc,layout,guest,linear,0,0);
    auto& entry=renderer.cache.Upload(desc,guest,renderer.RecordingList(),0);
    // Pending uploads and changed CPU backing must never enter the direct path.
    if(renderer.cache.CoherentDisplayTexture(desc.address,17,9,32,0))throw std::runtime_error("In-flight display accepted");
    auto fence=renderer.Flush();renderer.cache.Submitted(fence);renderer.Finish();renderer.cache.Collect(renderer.CompletedFence());
    Libraries::VideoOut::BufferAttributeGroup group{};group.attrib.width=17;group.attrib.height=9;group.attrib.pitch_in_pixel=32;
    group.attrib.tiling_mode=Libraries::VideoOut::TilingMode::Tile;group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A8B8G8R8Srgb;
    MemorySurfaceProvider provider(device.Get(),queue.Get(),renderer,1920,1200);
    auto first=provider.Snapshot(group,desc.address);
    if(!renderer.hit||DX12_API_CALL(first.texture->handle->GetDesc()).Width!=17)throw std::runtime_error("Direct GPU display path not used or CPU scaling remains");
    auto* first_resource=first.texture->handle.Get();
    auto second=provider.Snapshot(group,desc.address);
    if(first.texture->handle.Get()==second.texture->handle.Get())throw std::runtime_error("Live immutable display frame overwritten");
    // CPU changes invalidate the cached texture without requiring a game hook.
    linear[0]^=std::byte{1};EncodeGuestSubresource(desc,layout,guest,linear,0,0);
    auto changed=provider.Snapshot(group,desc.address);
    if(renderer.hit)throw std::runtime_error("Stale GPU display accepted after CPU modification");
    auto read=[&](Resource& texture){
        // Production presentation is submitted to the provider's same queue.
        // This test deliberately reads back through Rasterizer's other queue.
        provider.Drain();
        auto transfer=TextureTransfer::Create(device.Get(),texture,true);
        transfer.Readback(renderer.RecordingList(),texture);renderer.Finish();
        const auto td=texture.handle->GetDesc();std::vector<std::byte> result(size_t(td.Width)*td.Height*4);
        transfer.ReadRows(result.data(),result.size(),u64(td.Width)*4);return result;
    };
    if(read(*changed.texture)!=linear)throw std::runtime_error("CPU fallback display pixels differ");
    auto original=linear;original[0]^=std::byte{1};
    if(read(*first.texture)!=original||read(*second.texture)!=original)throw std::runtime_error("Retained GPU display frame changed");
    first.texture.reset();
    auto reused=provider.Snapshot(group,desc.address);
    if(reused.texture->handle.Get()!=first_resource)
        throw std::runtime_error("Retired immutable display allocation was not reused");
    if(read(*reused.texture)!=linear)
        throw std::runtime_error("Reused immutable display frame differs");
    // VideoOut preserves raw guest storage and performs BGRA/opaque-alpha
    // conversion through the immutable snapshot's native SRV mapping.
    group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A8R8G8B8Srgb;
    auto bgra=provider.Snapshot(group,desc.address);auto expected=linear;
    provider.Drain();
    const auto mapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(2,1,0,D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
    if(read(*bgra.texture)!=expected||bgra.component_mapping!=mapping||
       bgra.view_format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
        throw std::runtime_error("BGRA display storage/view mapping differs");
    // Non-RGBA8 VideoOut formats must reach the existing RGBA8 presenter
    // through the checked conversion fallback rather than aborting a frame.
    std::array<uint32_t,2> ten_bit{0u|(512u<<10)|(1023u<<20)|(3u<<30),
        1023u|(256u<<10)|(0u<<20)|(1u<<30)};
    const auto ten_address=reinterpret_cast<VAddr>(ten_bit.data());renderer.MapMemory(ten_address,sizeof(ten_bit));
    group.attrib.width=2;group.attrib.height=1;group.attrib.pitch_in_pixel=2;
    group.attrib.tiling_mode=Libraries::VideoOut::TilingMode::Linear;
    group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A2R10G10B10;
    auto ten=provider.Snapshot(group,ten_address);const std::vector<std::byte> ten_expected{
        std::byte{0},std::byte{127},std::byte{255},std::byte{255},
        std::byte{255},std::byte{63},std::byte{0},std::byte{85}};
    if(read(*ten.texture)!=ten_expected)throw std::runtime_error("10-bit VideoOut conversion differs");
    const std::array<uint16_t,4> fp16{0x0000,0x3800,0x3c00,0x3400};
    const auto fp_address=reinterpret_cast<VAddr>(fp16.data());renderer.MapMemory(fp_address,sizeof(fp16));
    group.attrib.width=group.attrib.pitch_in_pixel=1;
    group.attrib.pixel_format=Libraries::VideoOut::PixelFormat::A16R16G16B16Float;
    auto fp=provider.Snapshot(group,fp_address);const std::vector<std::byte> fp_expected{
        std::byte{0},std::byte{128},std::byte{255},std::byte{64}};
    if(read(*fp.texture)!=fp_expected)throw std::runtime_error("FP16 VideoOut conversion differs");
    ComPtr<ID3D12InfoQueue> messages;
    if(SUCCEEDED(DX12_API_CALL(device.As(&messages))))for(UINT64 i=0;i<DX12_API_CALL(messages->GetNumStoredMessages());++i){
        SIZE_T size{};DX12_API_CALL(messages->GetMessage(i,nullptr,&size));std::vector<std::byte> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());Check(DX12_API_CALL(messages->GetMessage(i,message,&size)));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Vulkan loaded");
    puts("PASS: coherent native GPU display copy, CPU invalidation fallback, immutable frame retention/reuse and BGRA conversion; no CPU upscale or Vulkan");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
