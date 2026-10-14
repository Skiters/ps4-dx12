// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <cstring>
#include <cstdio>
using namespace NativeDX12;
int main(){try{
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","1");auto device=CreateHardwareDevice();Rasterizer renderer(device.Get());
    GuestTextureCache cache(device.Get());GuestTextureDesc desc;desc.address=0x40000;desc.width=desc.height=desc.pitch=8;
    desc.tile_mode=13;desc.data_format=4;desc.number_format=7;desc.writable=true;
    auto sampled=desc;sampled.writable=false;sampled.data_format=10;sampled.number_format=0;sampled.tile_mode=10;
    const auto sampled_layout=LayoutGuestTexture(sampled);
    const auto layout=LayoutGuestTexture(desc);std::vector<std::byte> physical(std::max(layout.guest_bytes,sampled_layout.guest_bytes),std::byte{0xa7});
    std::array<float,64> old_pixels;old_pixels.fill(1.f);EncodeGuestSubresource(desc,layout,physical,std::as_bytes(std::span(old_pixels)),0,0);
    auto gpu=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto upload=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};Check(upload.handle->Map(0,nullptr,&mapped));memcpy(mapped,physical.data(),physical.size());upload.handle->Unmap(0,nullptr);
    renderer.RecordingList()->CopyBufferRegion(gpu.handle.Get(),0,upload.handle.Get(),0,physical.size());
    cache.SetBufferAliasRetention([](const auto&,const auto&){return true;});
    const auto acquire=[](auto& image){image.buffer_alias_version=true;image.SetGpuOwned(true);return true;};
    cache.SetOwnershipAcquirer(acquire);
    cache.SetIndependentVersionTransfer([&](auto& image,u64 address,u64 size){
        if(address!=desc.address||size!=sampled_layout.guest_bytes)throw std::runtime_error("Unexpected test transfer extent");
        auto version=cache.RetileBufferVersion(image,gpu,renderer.RecordingList(),renderer.CompletedFence());
        auto requested=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,gpu.handle.Get(),0,physical.size());
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(requested.handle.Get(),0,version.handle.Get(),0,std::min(size,layout.guest_bytes));
        version=std::move(requested);
        version.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return version;
    },acquire,[](auto& image){image.buffer_alias_version=false;image.SetGpuOwned(false);});
    auto& source=cache.Upload(desc,physical,renderer.RecordingList(),renderer.CompletedFence());
    Descriptors views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);const auto slot=views.Allocate(0);
    Descriptors cpu_views(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,false);const auto cpu_slot=cpu_views.Allocate(0);
    const auto uav=source.Uav(desc,0);device->CreateUnorderedAccessView(source.texture.handle.Get(),nullptr,&uav,views.Cpu(slot));
    device->CreateUnorderedAccessView(source.texture.handle.Get(),nullptr,&uav,cpu_views.Cpu(cpu_slot));
    ID3D12DescriptorHeap* heaps[]{views.Heap()};renderer.RecordingList()->SetDescriptorHeaps(1,heaps);
    source.texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const float value[]{0.25f,0,0,0};renderer.RecordingList()->ClearUnorderedAccessViewFloat(views.Gpu(slot),cpu_views.Cpu(cpu_slot),source.texture.handle.Get(),value,0,nullptr);
    if(!cache.Own(source))throw std::runtime_error("Source ownership acquisition failed");
    const auto source_uid=source.image_uid;
    auto& converted=cache.Upload(sampled,physical,renderer.RecordingList(),renderer.CompletedFence());
    const bool transferred=converted.image_uid!=source_uid&&converted.gpu_owned&&converted.buffer_alias_version&&!source.gpu_owned;
    auto pixels=TextureTransfer::Create(device.Get(),converted.texture,true);pixels.Readback(renderer.RecordingList(),converted.texture);
    auto backing=Buffer(device.Get(),physical.size(),D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    gpu.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);renderer.RecordingList()->CopyBufferRegion(backing.handle.Get(),0,gpu.handle.Get(),0,physical.size());
    cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
    std::array<u32,64> restored{};pixels.ReadRows(restored.data(),sizeof(restored),32);
    auto private_bytes=physical;std::array<float,64> new_pixels;new_pixels.fill(0.25f);
    EncodeGuestSubresource(desc,layout,private_bytes,std::as_bytes(std::span(new_pixels)),0,0);
    const auto expected=DecodeGuestSubresource(sampled,sampled_layout,private_bytes,0,0);
    if(memcmp(restored.data(),expected.data(),expected.size()))throw std::runtime_error("New image interpretation lost GPU-written float bits/layout");
    Check(backing.handle->Map(0,nullptr,&mapped));const bool unchanged=memcmp(mapped,physical.data(),physical.size())==0;backing.handle->Unmap(0,nullptr);
    if(!unchanged)throw std::runtime_error("Image transfer overwrote the physical buffer domain or padding");
    if(!transferred)throw std::runtime_error("Independent interpretation did not transfer its logical version");
    renderer.ValidateDebugMessages();puts("PASS: GPU writer -> retile private version -> different sampled format; physical GPU buffer/padding unchanged; logical ownership transfer");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
