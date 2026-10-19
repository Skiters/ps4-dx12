// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/texture_resources.h"
#include <cstdio>
using namespace NativeDX12;
static void Require(bool value){if(!value)throw std::runtime_error("Ordered float storage contract failed");}
int main(){try{
    DeviceOptions options;options.validation_enabled=true;
    auto device=CreateHardwareDevice(nullptr,options);Rasterizer renderer(device.Get());
    GuestTextureCache cache(device.Get());
    for(UINT samples:{2u,4u,8u}){
        GuestTextureDesc d;d.width=d.pitch=8;d.height=8;d.layers=d.view_layers=2;
        d.data_format=4;d.number_format=7;d.render_target=true;d.samples=samples;
        const auto layout=LayoutGuestTexture(d);std::vector<std::byte> guest(layout.guest_bytes,std::byte{0xa6});
        for(UINT sample=0;sample<samples;++sample)for(UINT layer=0;layer<2;++layer)
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x){
            const float value=float(1+sample+layer*8)/16;
            memcpy(guest.data()+GuestPixelOffset(d,layout,0,layer,x,y,sample),&value,4);
        }
        d.address=reinterpret_cast<uint64_t>(guest.data());
        auto& image=cache.Upload(d,guest,renderer.RecordingList(),renderer.CompletedFence());
        Require(image.storage_samples.empty()&&!image.storage_linear.handle);
        const auto initial_bytes=image.allocation_bytes;
        auto transfer=cache.PrepareMultisampleStorage(image);
        Require(image.storage_samples.size()==samples&&image.storage_linear.handle&&image.allocation_bytes>initial_bytes);
        const auto storage=image.storage_samples.front().handle;
        const auto allocated_bytes=image.allocation_bytes;
        cache.EnsureMultisampleStorage(image);
        Require(image.storage_samples.front().handle==storage&&image.allocation_bytes==allocated_bytes);
        transfer->Record(renderer.RecordingList(),image.texture,image.storage_samples,image.storage_linear,false);
        std::vector<TextureTransfer> copies;
        for(UINT sample=0;sample<samples;++sample)for(UINT layer=0;layer<2;++layer){
            auto view=image.StorageUav(d,sample);Require(view.Format==DXGI_FORMAT_R32_UINT);
            auto copy=TextureTransfer::CreateSubresource(device.Get(),image.storage_samples[sample].handle->GetDesc(),layer,true);
            copy.Readback(renderer.RecordingList(),image.storage_samples[sample]);copies.push_back(std::move(copy));
        }
        const auto fence=renderer.Flush();cache.Submitted(fence);renderer.Finish();
        for(UINT sample=0;sample<samples;++sample)for(UINT layer=0;layer<2;++layer){
            std::vector<float> pixels(64);copies[sample*2+layer].ReadRows(pixels.data(),256,32);
            for(float value:pixels)Require(value==float(1+sample+layer*8)/16);
        }
        renderer.ValidateDebugMessages();cache.Collect(renderer.CompletedFence());
        cache.Invalidate(d.address,layout.guest_bytes,renderer.CompletedFence());
    }
    puts("PASS: ordinary R32 float MSAA2/4/8 has no eager storage; ordered storage allocates once and preserves every layer/sample");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
