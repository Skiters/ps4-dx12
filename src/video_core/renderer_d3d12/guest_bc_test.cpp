#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "guest_image_readback.h"
#include "rasterizer.h"
#include <dxgi1_6.h>
#include <cstdio>
using namespace NativeDX12;
int main(){try{
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;if(DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 info{};Check(DX12_API_CALL(a->GetDesc1(&info)));if(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    Rasterizer raster(device.Get());GuestTextureCache cache(device.Get());
    const UINT formats[]{6,9,34,35,36,37,38,39,40,41};
    for(UINT format:formats){
        GuestTextureDesc d;d.address=0x1000000+format*0x10000;d.width=d.pitch=32;d.height=16;d.levels=6;d.layers=d.view_layers=2;d.data_format=format;
        if(format==6||format==34)d.number_format=7;
        const auto layout=LayoutGuestTexture(d);
        if(format>=35&&(layout.block_extent!=4||layout.guest_bytes!=(format==35||format==38?6144:12288)))throw std::runtime_error("BC mip/slice layout mismatch");
        if(format<35&&(layout.block_extent!=1||layout.bytes_per_pixel!=4))throw std::runtime_error("Packed texture layout mismatch");
        std::vector<std::byte> backing(size_t(layout.guest_bytes),std::byte{0xcd});
        for(UINT m=0;m<d.levels;++m)for(UINT layer=0;layer<d.layers;++layer){
            const UINT w=(std::max(d.width>>m,1u)+layout.block_extent-1)/layout.block_extent,h=(std::max(d.height>>m,1u)+layout.block_extent-1)/layout.block_extent;
            for(UINT y=0;y<h;++y)for(UINT x=0;x<w;++x){
                const UINT morton=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
                const auto at=layout.mips[m].offset+layer*layout.mips[m].slice_bytes+((y/8)*(layout.mips[m].pitch/8)+x/8)*64*layout.bytes_per_pixel+morton*layout.bytes_per_pixel;
                for(UINT c=0;c<layout.bytes_per_pixel;++c)backing[at+c]=std::byte((format*3+m*7+layer*11+x*17+y*31+c)&255);
            }
        }
        for(unsigned pass=0;pass<3;++pass){
            if(pass==2)backing[0]^=std::byte{0x55};
            auto& entry=cache.Upload(d,backing,raster.RecordingList(),raster.CompletedFence());
            const bool upload_pending=!entry.uploads.empty()||!entry.tiler_leases.empty()||!entry.multisample_leases.empty();
            if((pass!=1)!=upload_pending)throw std::runtime_error("Native sampled texture reuse/CPU change detection mismatch");
            auto token=raster.Flush();cache.Submitted(token);raster.Finish();
            GuestImageReadback rb(device.Get(),entry,raster.RecordingList());token=raster.Flush();rb.Submitted(token);cache.Used(entry,token);raster.Finish();
            std::vector<std::byte> result(backing.size(),std::byte{0xcd});rb.Resolve(raster.CompletedFence(),result);
            if(result!=backing)throw std::runtime_error("BC hardware mip/array roundtrip changed compressed blocks/padding");
        }
        cache.Invalidate(d.address,layout.guest_bytes,raster.CompletedFence());
    }
    puts("PASS: packed HDR/UNORM and BC1..BC7 guest golden Morton blocks -> native 6-mip 2-layer textures -> exact hardware readback, padding preserved");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
