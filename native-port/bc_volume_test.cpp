// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "video_core/renderer_d3d12/shader_pipeline.h"
#include "video_core/renderer_d3d12/compute_pipeline.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <cmath>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
static ComPtr<ID3D12Device> Device(){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return device;
    }throw std::runtime_error("No hardware device");
}
static std::array<std::byte,16> Block(unsigned format,unsigned seed){
    std::array<std::byte,16> block{};
    auto bits=[&](unsigned offset,unsigned count,unsigned value){for(unsigned i=0;i<count;++i)
        if((value>>i)&1)block[(offset+i)/8]|=std::byte(1u<<((offset+i)%8));};
    if(format<=37){
        const unsigned at=format==35?0:8;const u16 color=seed&1?0xf800:0x001f;
        memcpy(block.data()+at,&color,2);memcpy(block.data()+at+2,&color,2);
        if(format==36)for(unsigned i=0;i<8;++i)block[i]=std::byte{0xff};
        if(format==37)block[0]=std::byte{0xff};
    }else if(format==38||format==39){
        const u8 value=u8((seed*29)%127);block[0]=block[1]=std::byte(value);
        if(format==39){block[8]=block[9]=std::byte(u8(255-value));}
    }else if(format==40){
        bits(0,5,3);
        for(unsigned channel=0;channel<3;++channel){unsigned value=16+(seed*13+channel*17)%128;
            bits(5+channel*10,10,value);bits(35+channel*10,10,value);}
    }else{
        bits(6,1,1);
        for(unsigned component=0;component<4;++component){unsigned value=component==3?127:(seed*19+component*23)%128;
            bits(7+component*14,7,value);bits(14+component*14,7,value);}
        bits(63,1,seed&1);bits(64,1,seed&1);
    }return block;
}
static void Fill(const GuestTextureDesc& volume,const GuestTextureLayout& vl,std::vector<std::byte>& vb,
    const GuestTextureDesc& reference,const GuestTextureLayout& rl,std::vector<std::byte>& rb,unsigned pass){
    for(unsigned mip=0;mip<volume.levels;++mip){
        const unsigned width=(std::max(volume.width>>mip,1u)+3)/4,height=(std::max(volume.height>>mip,1u)+3)/4;
        for(unsigned z=0;z<(volume.volume_depth?std::max(volume.volume_depth>>mip,1u):volume.layers);++z)
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const auto data=Block(volume.data_format,1+mip*7+z*11+x*17+y*23+pass*31);
            memcpy(vb.data()+GuestPixelOffset(volume,vl,mip,z,x,y),data.data(),vl.bytes_per_pixel);
            memcpy(rb.data()+GuestPixelOffset(reference,rl,mip,z,x,y),data.data(),rl.bytes_per_pixel);
        }
    }
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=3)throw std::invalid_argument("Expected DXC and pinned Mesa DLL paths");
    auto device=Device();ComPtr<ID3D12InfoQueue> errors;Check(device.As(&errors));
    Rasterizer raster(device.Get());GuestTextureCache cache(device.Get(),argv[2]);
    for(unsigned dimension=1;dimension<=3;++dimension){
    auto compiled=Shader::Backend::DX12::CompileComputeDxil(argv[1],"#define DIMENSION "+std::to_string(dimension)+"\n"+R"(
#ifndef DIMENSION
#define DIMENSION 3
#endif
#if DIMENSION == 1
Texture1DArray<float4> volume:register(t0);
#elif DIMENSION == 2
Texture2DArray<float4> volume:register(t0);
#else
Texture3D<float4> volume:register(t0);
#endif
Texture2DArray<float4> reference:register(t1);
SamplerState linear_sampler:register(s0);
struct Pair {float4 actual;float4 expected;};
RWStructuredBuffer<Pair> results:register(u0);
void Dimensions(uint mip,out uint width,out uint height,out uint depth,out uint levels){
#if DIMENSION == 1
    volume.GetDimensions(mip,width,depth,levels);height=1;
#else
    volume.GetDimensions(mip,width,height,depth,levels);
#endif
}
float4 Fetch(uint3 xyz,uint mip){
#if DIMENSION == 1
    return volume.Load(int3(xyz.x,xyz.z,mip));
#else
    return volume.Load(int4(xyz,mip));
#endif
}
float4 Sample(float3 uv,float lod){
#if DIMENSION == 1
    return volume.SampleLevel(linear_sampler,uv.xz,lod);
#else
    return volume.SampleLevel(linear_sampler,uv,lod);
#endif
}
float4 SliceSample(float3 uv,uint mip){
    uint width,height,depth,levels;Dimensions(mip,width,height,depth,levels);
    uint rw,rh,rd,rl;reference.GetDimensions(mip,rw,rh,rd,rl);
    float2 logical=float2(width,height);float2 xy=clamp(uv.xy,0.5/logical,1-0.5/logical)*logical/float2(rw,rh);
#if DIMENSION == 3
    float layer=uv.z*depth-0.5;int first=(int)floor(layer);float weight=frac(layer);
    return lerp(reference.SampleLevel(linear_sampler,float3(xy,clamp(first,0,(int)depth-1)),mip),
                reference.SampleLevel(linear_sampler,float3(xy,clamp(first+1,0,(int)depth-1)),mip),weight);
#else
    return reference.SampleLevel(linear_sampler,float3(xy,uv.z),mip);
#endif
}
[numthreads(64,1,1)]void main(uint id:SV_DispatchThreadID){
    if(id>=64)return;uint mip=id%5;uint width,height,depth,levels;Dimensions(mip,width,height,depth,levels);
    uint3 xyz=uint3(id%width,(id/width)%height,(id/(width*height))%depth);
    Pair value;
    if(id<32){value.actual=Fetch(xyz,mip);value.expected=reference.Load(int4(xyz,mip));}
    else {float lod=(id%9)*0.5;float3 uv=float3((id%7)/6.0,(id%5)/4.0,(id%11)/10.0);
#if DIMENSION != 3
        uv.z=id%depth;
#endif
        value.actual=Sample(uv,lod);
        uint first=(uint)floor(lod),last=min(first+1,4);value.expected=lerp(SliceSample(uv,first),SliceSample(uv,last),frac(lod));}
    results[id]=value;
})");
    if(!compiled.success)throw std::runtime_error(compiled.diagnostics);
    ShaderPipeline pipeline(device.Get(),argv[1],compiled.bytecode);
    auto output=UavBuffer(device.Get(),64*32),readback=Buffer(device.Get(),64*32,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    const std::pair<unsigned,unsigned> formats[]{{35,0},{35,9},{36,0},{36,9},{37,0},{37,9},{38,0},{38,1},{39,0},{39,1},{40,0},{40,1},{40,7},{41,0},{41,9}};
    unsigned serial{};
    for(const auto [format,number]:formats)for(unsigned tile:{8u,13u,7u}){
        GuestTextureDesc volume;volume.address=0x100000000ull+(++serial)*0x1000000ull;
        volume.width=17;volume.height=dimension==1?1:9;volume.pitch=24;volume.levels=5;
        volume.volume_depth=dimension==3?5:0;volume.one_dimensional=dimension==1;
        if(dimension!=3)volume.layers=volume.view_layers=5;
        volume.data_format=format;volume.number_format=number;volume.tile_mode=tile;
        auto reference=volume;reference.address+=0x800000;reference.volume_depth=0;reference.one_dimensional=false;reference.layers=reference.view_layers=5;
        reference.width=(volume.width+3)&~3u;reference.height=(volume.height+3)&~3u;
        const auto vl=LayoutGuestTexture(volume),rl=LayoutGuestTexture(reference);
        if(!vl.expanded_bytes_per_pixel||vl.block_extent!=4||vl.bytes_per_pixel!=rl.bytes_per_pixel)
            throw std::runtime_error("BC volume guest/host layout contract mismatch");
        std::vector<std::byte> vb(size_t(vl.guest_bytes),std::byte{0xcd}),rb(size_t(rl.guest_bytes),std::byte{0xcd});
        std::array<float,512> previous{};
        for(unsigned pass=0;pass<3;++pass){
            if(pass!=1)Fill(volume,vl,vb,reference,rl,rb,pass);
            auto& v=cache.Upload(volume,vb,raster.RecordingList(),raster.CompletedFence());
            auto& r=cache.Upload(reference,rb,raster.RecordingList(),raster.CompletedFence());
            if(pass==1&&(!v.uploads.empty()||!r.uploads.empty()))throw std::runtime_error("Unchanged BC snapshots did not reuse resources");
            ResourceBindings bindings(device.Get(),pipeline.Layout());bindings.Srv(0,0,v.texture.handle.Get(),v.Srv());bindings.Srv(1,0,r.texture.handle.Get(),r.Srv());
            D3D12_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxLOD=D3D12_FLOAT32_MAX;
            sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;bindings.Sampler(0,0,sampler);
            output.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=64;uav.Buffer.StructureByteStride=32;
            bindings.Uav(0,0,output.handle.Get(),uav);raster.ExecuteCompute(pipeline,bindings,1,1,1,false);
            output.UavBarrier(raster.RecordingList());output.Transition(raster.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            raster.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,64*32);
            const auto fence=raster.Flush();cache.Submitted(fence);cache.Used(v,fence);cache.Used(r,fence);raster.Finish();
            void* mapped{};D3D12_RANGE range{0,64*32},empty{};Check(readback.handle->Map(0,&range,&mapped));
            std::array<float,512> values{};memcpy(values.data(),mapped,sizeof(values));readback.handle->Unmap(0,&empty);
            for(unsigned i=0;i<64;++i)for(unsigned c=0;c<4;++c){const float actual=values[i*8+c],expected=values[i*8+4+c];
                const float tolerance=format==40?1e-6f+std::abs(expected)*1e-5f:2.f/255.f;
                if(!std::isfinite(actual)||!std::isfinite(expected)||std::abs(actual-expected)>tolerance)
                    throw std::runtime_error("BC volume GPU values disagree with the independent hardware BC array decoder");
            }
            if(pass==1&&values!=previous)throw std::runtime_error("Warm BC volume changed GPU values");
            if(pass==2&&values==previous)throw std::runtime_error("Changed BC guest blocks did not update GPU values");
            previous=values;
            GuestImageReadback preserved(device.Get(),v,raster.RecordingList());preserved.Submitted(fence);
            std::vector<std::byte> preserved_source(vb.size(),std::byte{0xee});preserved.Resolve(raster.CompletedFence(),preserved_source);
            if(preserved_source!=vb)throw std::runtime_error("Immutable BC source blocks or padding changed");
        }
        cache.Invalidate(volume.address,vl.guest_bytes,raster.CompletedFence());cache.Invalidate(reference.address,rl.guest_bytes,raster.CompletedFence());
    }
    }
    for(UINT64 i=0;i<errors->GetNumStoredMessages();++i){SIZE_T size{};Check(errors->GetMessage(i,nullptr,&size));std::vector<std::byte> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());Check(errors->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);
    }
    puts("PASS: BC1..BC7 volume GPU fetch/filter/mips, sRGB/signed, cold/warm/CPU-change, original tiling and validation");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
