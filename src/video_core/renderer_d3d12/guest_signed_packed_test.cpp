// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_texture_cache.h"
#include "guest_image_readback.h"
#include "guest_multisample.h"
#include "rasterizer.h"
#include <d3d12sdklayers.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::array<int32_t,4> Values(UINT mip,UINT layer,UINT x,UINT y,UINT phase){
    std::array<int32_t,4> result;
    for(UINT c=0;c<3;++c)result[c]=int32_t((x*13+y*89+mip*211+layer*307+phase*31+c*173)%1024)-512;
    result[3]=int32_t((x+y+mip+layer+phase)%4)-2;
    return result;
}
struct Readback {Resource buffer;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;UINT mip,layer;UINT64 bytes;
    Resource linear{};std::shared_ptr<GuestMultisample::Lease> transfer{};};
static void Run(ID3D12Device* device,UINT number,UINT tile,UINT shape){
    Rasterizer raster(device);GuestTextureCache cache(device);
    GuestTextureDesc d;d.address=0x1000000;d.data_format=9;d.number_format=number;d.tile_mode=tile;
    d.width=shape==1?1024:17;d.pitch=d.width+8;d.height=shape==1?1:9;d.levels=3;
    d.layers=d.view_layers=shape==2?1:3;d.one_dimensional=shape==1;d.volume_depth=shape==2?4:0;
    if(shape==3){d.samples=4;d.levels=1;}
    const auto layout=LayoutGuestTexture(d);
    Require(layout.bytes_per_pixel==4&&layout.HostBytesPerPixel()==16&&layout.block_extent==1,"signed packed guest/host size");
    auto writable=d;writable.writable=true;const auto raw=LayoutGuestTexture(writable);
    Require(raw.format==DXGI_FORMAT_R32_UINT&&raw.bytes_per_pixel==4&&raw.HostBytesPerPixel()==4&&
        raw.guest_bytes==layout.guest_bytes,"signed packed writable raw layout");
    auto color=d;color.one_dimensional=false;color.volume_depth=0;color.number_format=5;color.render_target=true;
    const auto color_layout=LayoutGuestTexture(color);
    Require(color_layout.format==DXGI_FORMAT_R10G10B10A2_UINT&&color_layout.HostBytesPerPixel()==4&&
        !CompatibleGuestSampledStorage(color,d),"signed integer attachment retains packed storage and separate sampled conversion");
    for(UINT usage=0;usage<2;++usage){auto invalid=d;
        if(usage==0){invalid.render_target=true;invalid.number_format=1;}else{invalid.samples=3;invalid.levels=1;}
        bool rejected{};try{LayoutGuestTexture(invalid);}catch(const std::exception&){rejected=true;}
        Require(rejected,"signed normalized attachment/invalid sample count accepted");
    }
    std::vector<std::byte> guest(size_t(layout.guest_bytes),std::byte{0xee});
    GuestTextureCache::Entry* first{};
    for(UINT pass=0;pass<3;++pass){const UINT phase=pass==2?1:0;
        if(pass!=1)for(UINT mip=0;mip<d.levels;++mip){
            const UINT slices=d.volume_depth?std::max(d.volume_depth>>mip,1u):d.layers;
            for(UINT layer=0;layer<slices;++layer)for(UINT sample=0;sample<d.samples;++sample)for(UINT y=0;y<std::max(d.height>>mip,1u);++y)
            for(UINT x=0;x<std::max(d.width>>mip,1u);++x){
                const auto values=Values(mip,layer,x,y,phase+sample*3);
                const UINT packed=(UINT(values[0])&1023)|((UINT(values[1])&1023)<<10)|
                    ((UINT(values[2])&1023)<<20)|((UINT(values[3])&3)<<30);
                const auto at=GuestPixelOffset(d,layout,mip,layer,x,y,sample);memcpy(guest.data()+at,&packed,4);
            }
        }
        const auto original=guest;
        auto& entry=cache.Upload(d,guest,raster.RecordingList(),raster.CompletedFence());
        Require(cache.HasGuestRangeOverlap(d.address,1)&&cache.HasGuestRangeOverlap(d.address+layout.guest_bytes-1,1)&&
            !cache.HasGuestRangeOverlap(d.address-1,1)&&!cache.HasGuestRangeOverlap(d.address+layout.guest_bytes,1),
            "cached guest image overlap boundaries");
        Require(!cache.HasGuestRangeOverlap(d.address,layout.guest_bytes,true),"immutable sampled image retained false GPU ownership");
        if(!first)first=&entry;Require(first==&entry,"signed sampled cache entry changed");
        const bool upload_pending=!entry.uploads.empty()||!entry.tiler_leases.empty()||!entry.multisample_leases.empty();
        Require((pass!=1)==upload_pending,"signed sampled warm/change upload state");
        Descriptors descriptors(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
        const auto slot=descriptors.Allocate(0);const auto srv=entry.Srv(d);
        Require(srv.Format==layout.format,"signed sampled typed view format");
        device->CreateShaderResourceView(entry.texture.handle.Get(),&srv,descriptors.Cpu(slot));
        auto* list=raster.RecordingList();entry.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        const auto resource=entry.texture.handle->GetDesc();std::vector<Readback> readbacks;
        GuestMultisample multisample(device);
        for(UINT layer=0;layer<d.layers;++layer)for(UINT mip=0;mip<d.levels;++mip){
            if(d.samples>1){
                auto fp=GuestMultisample::Footprint(d,layout);const auto total=GuestMultisample::LinearBytes(d,fp);
                auto linear=GuestTiler::Scratch(device,total);
                auto transfer=multisample.Prepare(entry.texture,linear,d,layout,layer,0,false);
                transfer->Record(list,entry.texture,linear);linear.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
                auto buffer=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                list->CopyBufferRegion(buffer.handle.Get(),0,linear.handle.Get(),0,total);fp.Footprint.Depth=d.samples;
                readbacks.push_back({std::move(buffer),fp,mip,layer,total,std::move(linear),std::move(transfer)});continue;
            }
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT rows;UINT64 row_bytes,total;
            device->GetCopyableFootprints(&resource,mip+layer*d.levels,1,0,&fp,&rows,&row_bytes,&total);
            auto buffer=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=entry.texture.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=mip+layer*d.levels;
            D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=buffer.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;
            list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);readbacks.push_back({std::move(buffer),fp,mip,layer,total});
        }
        const auto fence=raster.Flush();cache.Submitted(fence);cache.Used(entry,fence);descriptors.Retire(slot,fence);raster.Finish();
        for(auto& rb:readbacks){void* mapped{};D3D12_RANGE range{0,SIZE_T(rb.bytes)};Check(rb.buffer.handle->Map(0,&range,&mapped));
            bool valid=true;
            for(UINT z=0;z<rb.footprint.Footprint.Depth;++z)
            for(UINT y=0;y<rb.footprint.Footprint.Height;++y)for(UINT x=0;x<rb.footprint.Footprint.Width;++x){
                const auto values=Values(rb.mip,d.volume_depth?z:rb.layer,x,y,phase+(d.samples>1?z*3:0));
                const auto* at=static_cast<const std::byte*>(mapped)+rb.footprint.Offset+
                    (UINT64(z)*rb.footprint.Footprint.Height+y)*rb.footprint.Footprint.RowPitch+x*16;
                for(UINT c=0;c<4;++c){if(number==5){int32_t actual;memcpy(&actual,at+c*4,4);valid&=actual==values[c];}
                    else{float actual;memcpy(&actual,at+c*4,4);
                        const float expected=values[c]<-(c==3?1:511)?-1.f:float(values[c])/float(c==3?1:511);
                        valid&=std::isfinite(actual)&&std::abs(actual-expected)<1e-7f;}}
            }
            D3D12_RANGE empty{0,0};rb.buffer.handle->Unmap(0,&empty);Require(valid,"signed packed real GPU texture data");
        }
        GuestImageReadback retained(device,entry,raster.RecordingList());std::vector<std::byte> restored(guest.size());
        const auto retained_fence=raster.Flush();retained.Submitted(retained_fence);raster.Finish();
        retained.Resolve(raster.CompletedFence(),restored);Require(restored==original&&guest==original,"signed source/padding retention");
        raster.Finish();raster.ValidateDebugMessages();
    }
    cache.Invalidate(d.address,layout.guest_bytes,raster.CompletedFence());
    Require(!cache.HasGuestRangeOverlap(d.address,layout.guest_bytes),"retired cached image overlap index");
}
static void RunPacked16(ID3D12Device* device,UINT format,UINT tile,bool one_dimensional){
    Rasterizer raster(device);GuestTextureCache cache(device);
    GuestTextureDesc d;d.address=0x2000000;d.data_format=format;d.number_format=0;d.tile_mode=tile;
    d.width=17;d.pitch=25;d.height=one_dimensional?1:9;d.volume_depth=one_dimensional?0:4;d.levels=3;
    d.one_dimensional=one_dimensional;d.layers=one_dimensional?3:1;
    if(one_dimensional){d.first_mip=d.first_layer=1;d.view_layers=1;}
    const auto layout=LayoutGuestTexture(d);auto writable=d;writable.writable=true;
    const auto raw=LayoutGuestTexture(writable);
    Require(layout.format==DXGI_FORMAT_R32G32B32A32_FLOAT&&layout.bytes_per_pixel==2&&
        layout.HostBytesPerPixel()==16&&HasExactPackedExpansion(d,layout)&&
        raw.format==DXGI_FORMAT_R32_UINT&&raw.HostBytesPerPixel()==4&&
        layout.guest_bytes==raw.guest_bytes&&CompatibleGuestPackedStorage(writable,d),
        "packed volume physical/expanded storage");
    std::vector<std::byte> guest(size_t(layout.guest_bytes),std::byte{0xee});
    const auto word=[](UINT mip,UINT z,UINT x,UINT y){return uint16_t(x*173+y*977+z*4099+mip*8191+0x351b);};
    for(UINT mip=0;mip<d.levels;++mip)for(UINT z=0;z<(one_dimensional?d.layers:std::max(d.volume_depth>>mip,1u));++z)
    for(UINT y=0;y<std::max(d.height>>mip,1u);++y)for(UINT x=0;x<std::max(d.width>>mip,1u);++x){
        const auto packed=word(mip,z,x,y);memcpy(guest.data()+GuestPixelOffset(d,layout,mip,z,x,y),&packed,2);
    }
    const auto original=guest;std::vector<std::byte> poison(guest.size(),std::byte{0xa6});
    auto& first=cache.Upload(writable,guest,raster.RecordingList(),raster.CompletedFence());first.SetGpuOwned(true);
    const auto uid=first.image_uid;
    for(UINT pass=0;pass<2;++pass){
        if(pass){auto& writer=cache.Upload(writable,poison,raster.RecordingList(),raster.CompletedFence());
            Require(writer.image_uid==uid&&writer.gpu_owned&&writer.layout.format==DXGI_FORMAT_R32_UINT,
                "packed volume inverse retained backing");
            GuestImageReadback inverse(device,writer,raster.RecordingList());auto restored=original;
            const auto fence=raster.Flush();cache.Submitted(fence);inverse.Submitted(fence);raster.Finish();
            inverse.Resolve(raster.CompletedFence(),restored);Require(restored==original,"packed volume inverse exact bits/padding");
        }
        auto& sampled=cache.Upload(d,poison,raster.RecordingList(),raster.CompletedFence());
        Require(sampled.image_uid==uid&&sampled.gpu_owned&&sampled.expanded_guest_backing.handle&&
            sampled.expanded_written_mask.handle&&sampled.expanded_backing_bytes,
            "packed volume conversion retained ownership and guest backing");
        const auto description=sampled.texture.handle->GetDesc();std::vector<Readback> readbacks;
        auto* list=raster.RecordingList();sampled.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        for(UINT layer=0;layer<d.layers;++layer)for(UINT mip=0;mip<d.levels;++mip){
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 total;
            const auto sub=mip+layer*d.levels;
            device->GetCopyableFootprints(&description,sub,1,0,&fp,nullptr,nullptr,&total);
            auto buffer=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=sampled.texture.handle.Get();
            src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=sub;
            D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=buffer.handle.Get();
            dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;
            list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);readbacks.push_back({std::move(buffer),fp,mip,layer,total});
        }
        GuestImageReadback retained(device,sampled,list);auto restored=original;
        const auto fence=raster.Flush();cache.Submitted(fence);retained.Submitted(fence);raster.Finish();
        retained.Resolve(raster.CompletedFence(),restored);Require(restored==original&&guest==original,
            "packed volume retained exact source and padding");
        for(auto& rb:readbacks){void* mapped{};D3D12_RANGE range{0,SIZE_T(rb.bytes)};
            Check(rb.buffer.handle->Map(0,&range,&mapped));bool valid=true;
            for(UINT z=0;z<rb.footprint.Footprint.Depth;++z)
            for(UINT y=0;y<rb.footprint.Footprint.Height;++y)for(UINT x=0;x<rb.footprint.Footprint.Width;++x){
                UINT packed=word(rb.mip,one_dimensional?rb.layer:z,x,y);
                if(format==18)packed=((packed>>1)|(packed<<15))&65535;
                if(format==19)packed=((packed&15)<<12)|((packed&240)<<4)|((packed&3840)>>4)|((packed&61440)>>12);
                const std::array<UINT,8> fields=format==16?std::array<UINT,8>{11,5,5,6,0,5,0,0}:
                    format==19?std::array<UINT,8>{8,4,4,4,0,4,12,4}:std::array<UINT,8>{10,5,5,5,0,5,15,1};
                const auto* at=static_cast<const std::byte*>(mapped)+rb.footprint.Offset+
                    (UINT64(z)*rb.footprint.Footprint.Height+y)*rb.footprint.Footprint.RowPitch+x*16;
                for(UINT c=0;c<4;++c){const UINT mask=(1u<<fields[c*2+1])-1;
                    const float expected=mask?float((packed>>fields[c*2])&mask)/float(mask):1.f;
                    float actual;memcpy(&actual,at+c*4,4);valid&=std::isfinite(actual)&&std::abs(actual-expected)<1e-7f;}
            }
            D3D12_RANGE empty{0,0};rb.buffer.handle->Unmap(0,&empty);Require(valid,"packed volume all GPU float channels");
        }
        raster.ValidateDebugMessages();
    }
    first.SetGpuOwned(false);cache.Invalidate(d.address,layout.guest_bytes,raster.CompletedFence());
}
int main(){try{
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;Check(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
    for(UINT number:{1u,5u})for(UINT tile:{8u,13u})for(UINT shape=0;shape<4;++shape)Run(device.Get(),number,tile,shape);
    for(UINT format:{16u,17u,18u,19u})for(UINT tile:{8u,13u})for(bool one_dimensional:{false,true})
        RunPacked16(device.Get(),format,tile,one_dimensional);
    puts("PASS: signed packed sampled texture GPU contents, 1D/2D/3D/MSAA, mips/layers, warm cache and guest padding");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
