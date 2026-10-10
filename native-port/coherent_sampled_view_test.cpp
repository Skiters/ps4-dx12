#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include <cstring>
using namespace NativeDX12;
void Require(bool ok){if(!ok)throw std::runtime_error("Coherent sampled-view assertion failed");}
int main(){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");
    auto device=CreateHardwareDevice();ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC q{};Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    Timeline timeline(device.Get());GuestTextureCache cache(device.Get());
    GuestTextureDesc depth;depth.address=0x10000;depth.width=depth.pitch=32;depth.height=16;
    depth.storage_height=16;depth.tile_mode=0;depth.data_format=4;depth.number_format=7;depth.depth_target=true;
    auto layout=LayoutGuestTexture(depth);std::vector<std::byte> memory(layout.guest_bytes,std::byte{0x73});
    std::vector<float> pixels(depth.width*depth.height,0.25f);
    EncodeGuestSubresource(depth,layout,memory,std::as_bytes(std::span(pixels)),0,0);
    auto& attachment=cache.Upload(depth,memory,list.Get(),0);
    auto retire=[&]{Check(DX12_API_CALL(list->Close()));auto fence=timeline.Submit(queue.Get(),list.Get());cache.Submitted(fence);
        timeline.Wait(fence);cache.Collect(timeline.Completed());Check(DX12_API_CALL(allocator->Reset()));Check(DX12_API_CALL(list->Reset(allocator.Get(),nullptr)));return fence;};
    retire();Require(attachment.uploads.empty());
    // Exercise the real attachment-write -> readback -> coherent-snapshot path,
    // rather than borrowing a texture that only ever contained a CPU upload.
    Descriptors depth_views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
    const auto depth_slot=depth_views.Allocate(0);const auto depth_view=attachment.Dsv();
    DX12_API_CALL(device->CreateDepthStencilView(attachment.texture.handle.Get(),&depth_view,depth_views.Cpu(depth_slot)));
    attachment.texture.Transition(list.Get(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
    DX12_API_CALL(list->ClearDepthStencilView(depth_views.Cpu(depth_slot),D3D12_CLEAR_FLAG_DEPTH,0.5f,0,0,nullptr));
    GuestImageReadback depth_readback(device.Get(),attachment,list.Get());
    const auto cleared=retire();depth_readback.Submitted(cleared);
    depth_readback.Resolve(timeline.Completed(),memory);attachment.RememberSnapshot(memory);
    std::fill(pixels.begin(),pixels.end(),0.5f);
    auto sampled=depth;sampled.depth_target=false;sampled.storage_height=0;sampled.pow2_padding=true;sampled.swizzle={4,4,4,5};
    auto& alias=cache.Upload(sampled,memory,list.Get(),timeline.Completed());
    Require(&alias==&attachment&&alias.uploads.empty());
    auto view=alias.Srv(sampled);Require(view.Format==DXGI_FORMAT_R32_FLOAT&&
        view.Shader4ComponentMapping==D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,0,0,1));
    Descriptors descriptors(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
    DX12_API_CALL(device->CreateShaderResourceView(alias.texture.handle.Get(),&view,descriptors.Cpu(descriptors.Allocate(0))));
    auto readback=TextureTransfer::Create(device.Get(),alias.texture,true);readback.Readback(list.Get(),alias.texture);
    retire();std::vector<float> restored(pixels.size());readback.ReadRows(restored.data(),restored.size()*4,depth.width*4);
    Require(restored==pixels);
    // A CPU-written texel prevents borrowing the unchanged attachment. New
    // native upload must expose that new value, preserving all other texels.
    pixels[7]=0.75f;EncodeGuestSubresource(sampled,LayoutGuestTexture(sampled),memory,std::as_bytes(std::span(pixels)),0,0);
    auto& changed=cache.Upload(sampled,memory,list.Get(),timeline.Completed());Require(&changed!=&attachment&&!changed.uploads.empty());
    auto changed_readback=TextureTransfer::Create(device.Get(),changed.texture,true);changed_readback.Readback(list.Get(),changed.texture);
    retire();changed_readback.ReadRows(restored.data(),restored.size()*4,depth.width*4);Require(restored==pixels);
    // Changed descriptor interpretation must be rejected rather than silently
    // using a compatible resource with the wrong native view format.
    auto wrong=sampled;wrong.number_format=4;bool rejected=false;
    try{attachment.Srv(wrong);}catch(const std::invalid_argument&){rejected=true;}Require(rejected);
    Check(DX12_API_CALL(list->Close()));
    puts("PASS: retired depth attachment reused as swizzled sampled view; exact native GPU readback; CPU writes fall back; format mismatch rejected");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
