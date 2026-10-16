// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_multisample.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/multisample_readback.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <d3d12sdklayers.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace NativeDX12 {

// Authored only. Production upload, independent native sample-load oracle.
inline void RunMsaaDepthInitializationFixture(ID3D12Device* device) {
    constexpr UINT Width=13,Height=11,Layers=3,SmallWidth=7,SmallHeight=5;
    if(!device)throw std::invalid_argument("MSAA depth fixture needs a hardware device");
    ComPtr<ID3D12InfoQueue> info_queue;
    Check(device->QueryInterface(IID_PPV_ARGS(&info_queue))); // Severe validation is mandatory here.
    PlacedTextureAllocator allocator(device,4*1024*1024,32*1024*1024);
    GuestMultisample transfer(device);
    Rasterizer renderer(device); // Drains before transfer programs/allocator die.
    const auto depth_code=[](UINT phase,UINT layer,UINT sample,UINT x,UINT y){
        return 4096u+phase*4096u+layer*1024u+sample*256u+y*Width+x;
    };
    const auto stencil_code=[](UINT phase,UINT layer,UINT sample,UINT x,UINT y){
        return 1u+(phase*31u+layer*47u+sample*53u+y*Width+x)%251u;
    };
    const auto clear_depth=[](UINT layer){return .75f+float(layer)/32;};
    const auto clear_stencil=[](UINT layer){return std::array<UINT,Layers>{0x80,0xa5,0xff}[layer];};
    struct VerifyLease {
        Resource texture,output,initial,readback;
        std::shared_ptr<Descriptors> descriptors;
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> pipeline;
    };
    const auto verify=[&](Resource& texture,const GuestTextureDesc& d,bool stencil,auto&& expected){
        auto lease=std::make_shared<VerifyLease>();lease->texture=texture;
        const UINT words=d.width*d.height*d.samples*d.layers;
        const u64 bytes=u64(words)*4+64;
        D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
        buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        lease->output.state=D3D12_RESOURCE_STATE_COPY_DEST;
        Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,lease->output.state,
            nullptr,IID_PPV_ARGS(&lease->output.handle)));
        lease->initial=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        lease->readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE empty{};Check(lease->initial.handle->Map(0,&empty,&mapped));
        std::memset(mapped,0xa7,size_t(bytes));lease->initial.handle->Unmap(0,nullptr);
        const std::string source=std::string(stencil?"#define STENCIL 1\n":"#define STENCIL 0\n")+R"(
#if STENCIL
Texture2DMSArray<uint4> image : register(t0);
#else
Texture2DMSArray<float4> image : register(t0);
#endif
RWByteAddressBuffer result : register(u0);
cbuffer Shape : register(b0) { uint width; uint height; uint samples; uint layers; };
[numthreads(8,8,1)] void main(uint3 p:SV_DispatchThreadID) {
    if(p.x>=width || p.y>=height || p.z>=samples*layers) return;
    uint layer=p.z/samples, sample=p.z%samples;
#if STENCIL
    uint bits=image.Load(int3(p.xy,layer),sample).x;
#else
    uint bits=asuint(image.Load(int3(p.xy,layer),sample).x);
#endif
    result.Store(4*((p.z*height+p.y)*width+p.x),bits);
})";
        const auto code=CompileMultisampleShader(source.c_str(),"cs_5_1");
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
            {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable={2,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants={0,0,4};
        D3D12_ROOT_SIGNATURE_DESC root_desc{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> signature,errors;
        Check(D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
        Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&lease->root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=lease->root.Get();
        pipeline.CS={code->GetBufferPointer(),code->GetBufferSize()};
        Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&lease->pipeline)));
        lease->descriptors=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format=stencil?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:d.stencil_address?
            DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:d.data_format==2?DXGI_FORMAT_R16_UNORM:DXGI_FORMAT_R32_FLOAT;
        srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={0,d.layers};
        srv.Shader4ComponentMapping=stencil?D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1):D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        device->CreateShaderResourceView(texture.handle.Get(),&srv,lease->descriptors->Cpu(0));
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;
        uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(bytes/4);
        uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
        device->CreateUnorderedAccessView(lease->output.handle.Get(),nullptr,&uav,lease->descriptors->Cpu(1));
        renderer.RetainUntilFinish([lease](u64){(void)lease;});
        auto* list=renderer.RecordingList();
        list->CopyBufferRegion(lease->output.handle.Get(),0,lease->initial.handle.Get(),0,bytes);
        texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        lease->output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        ID3D12DescriptorHeap* heaps[]{lease->descriptors->Heap()};list->SetDescriptorHeaps(1,heaps);
        list->SetComputeRootSignature(lease->root.Get());list->SetPipelineState(lease->pipeline.Get());
        list->SetComputeRootDescriptorTable(0,lease->descriptors->Gpu(0));
        const UINT shape[]{d.width,d.height,d.samples,d.layers};list->SetComputeRoot32BitConstants(1,4,shape,0);
        list->Dispatch((d.width+7)/8,(d.height+7)/8,d.samples*d.layers);
        lease->output.UavBarrier(list);lease->output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(lease->readback.handle.Get(),0,lease->output.handle.Get(),0,bytes);
        renderer.Finish();
        D3D12_RANGE read{0,SIZE_T(bytes)};Check(lease->readback.handle->Map(0,&read,&mapped));
        bool valid=true;const auto* values=static_cast<const u32*>(mapped);
        for(UINT layer=0;layer<d.layers;++layer)for(UINT sample=0;sample<d.samples;++sample)
        for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){
            const u32 bits=values[((layer*d.samples+sample)*d.height+y)*d.width+x];
            const u32 wanted=expected(layer,sample,x,y);
            if(!stencil&&d.data_format==2){
                const float value=std::bit_cast<float>(bits);
                valid&=std::isfinite(value)&&std::abs(value-float(wanted)/65535.f)<.000001f;
            }else valid&=bits==wanted;
        }
        const auto* tail=static_cast<const std::byte*>(mapped)+u64(words)*4;
        for(UINT i=0;i<64;++i)valid&=tail[i]==std::byte{0xa7};
        lease->readback.handle->Unmap(0,&empty);
        if(!valid)throw std::runtime_error("MSAA depth initialization lost sample/layer/plane/partial preservation or oracle bounds");
        renderer.ValidateDebugMessages();
    };
    const auto upload=[&](Resource& texture,const GuestTextureDesc& d,UINT layer,UINT plane,UINT phase){
        const auto interpreted=plane?DescribeGuestStencil(d):d;
        const auto layout=LayoutGuestTexture(d),physical=LayoutGuestTexture(interpreted);
        const auto fp=GuestMultisample::Footprint(interpreted,physical);
        const u64 bytes=GuestMultisample::LinearBytes(interpreted,fp);
        auto input=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE empty{};Check(input.handle->Map(0,&empty,&mapped));
        std::memset(mapped,0x9d,size_t(bytes));
        for(UINT sample=0;sample<d.samples;++sample)for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){
            const u32 code=plane?stencil_code(phase,layer,sample,x,y):depth_code(phase,layer,sample,x,y);
            const u32 bits=plane||d.data_format==2?code:std::bit_cast<u32>(float(code)/65536.f);
            std::memcpy(static_cast<std::byte*>(mapped)+(u64(sample)*d.height+y)*fp.Footprint.RowPitch+
                u64(x)*physical.HostBytesPerPixel(),&bits,physical.HostBytesPerPixel());
        }
        input.handle->Unmap(0,nullptr);
        auto lease=transfer.Prepare(texture,input,d,layout,layer,plane,true);
        renderer.RetainUntilFinish([lease](u64){(void)lease;});
        lease->Record(renderer.RecordingList(),texture,input);
    };
    const auto verify_stencil_transfer=[&](Resource& texture,const GuestTextureDesc& d,auto&& expected){
        const auto stencil=DescribeGuestStencil(d);
        const auto physical=LayoutGuestTexture(stencil);
        const auto fp=GuestMultisample::Footprint(stencil,physical);
        const auto bytes=GuestMultisample::LinearBytes(stencil,fp);
        for(UINT layer=0;layer<d.layers;++layer){
            auto storage=std::make_shared<VerifyLease>();storage->texture=texture;
            D3D12_RESOURCE_DESC buffer{};buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width=bytes;buffer.Height=buffer.DepthOrArraySize=buffer.MipLevels=1;
            buffer.SampleDesc.Count=1;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            buffer.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
            storage->output.state=D3D12_RESOURCE_STATE_COPY_DEST;
            Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,storage->output.state,
                nullptr,IID_PPV_ARGS(&storage->output.handle)));
            storage->initial=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            storage->readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            void* mapped{};D3D12_RANGE empty{};Check(storage->initial.handle->Map(0,&empty,&mapped));
            std::memset(mapped,0xb9,size_t(bytes));storage->initial.handle->Unmap(0,nullptr);
            auto lease=transfer.Prepare(texture,storage->output,d,LayoutGuestTexture(d),layer,1,false);
            renderer.RetainUntilFinish([storage,lease](u64){(void)storage;(void)lease;});
            auto* list=renderer.RecordingList();
            list->CopyBufferRegion(storage->output.handle.Get(),0,storage->initial.handle.Get(),0,bytes);
            lease->Record(list,texture,storage->output);
            storage->output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(storage->readback.handle.Get(),0,storage->output.handle.Get(),0,bytes);
            renderer.Finish();
            std::vector<std::byte> oracle(size_t(bytes),std::byte{0xb9});
            for(UINT sample=0;sample<d.samples;++sample)for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x)
                oracle[(u64(sample)*d.height+y)*fp.Footprint.RowPitch+x]=std::byte(expected(layer,sample,x,y));
            D3D12_RANGE read{0,SIZE_T(bytes)};Check(storage->readback.handle->Map(0,&read,&mapped));
            const bool valid=std::memcmp(mapped,oracle.data(),size_t(bytes))==0;
            storage->readback.handle->Unmap(0,&empty);
            if(!valid)throw std::runtime_error("Production MSAA stencil extraction lost unsigned values, samples or pitched padding");
            renderer.ValidateDebugMessages();
        }
    };
    const auto allocate=[&](const GuestTextureDesc& d){
        D3D12_RESOURCE_DESC native{};native.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        native.Width=d.width;native.Height=d.height;native.DepthOrArraySize=UINT16(d.layers);native.MipLevels=1;
        native.Format=d.stencil_address?DXGI_FORMAT_R32G8X24_TYPELESS:d.data_format==2?DXGI_FORMAT_R16_TYPELESS:DXGI_FORMAT_R32_TYPELESS;
        native.SampleDesc.Count=d.samples;native.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        auto texture=allocator.Allocate(native,D3D12_RESOURCE_STATE_COMMON);
        renderer.RetainUntilFinish([texture](u64){(void)texture;});
        D3D12_RESOURCE_BARRIER alias{};alias.Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        alias.Aliasing.pResourceAfter=texture.handle.Get();renderer.RecordingList()->ResourceBarrier(1,&alias);
        return texture;
    };
    const auto shape=[](bool d16,bool stencil,UINT samples){
        GuestTextureDesc d;d.address=0x40000;d.width=d.pitch=Width;d.height=Height;d.layers=Layers;
        d.data_format=d16?2:4;d.number_format=d16?0:7;d.depth_target=true;d.tile_mode=8;
        d.samples=samples;if(stencil)d.stencil_address=0x80000;return d;
    };
    const auto supports=[&](DXGI_FORMAT format,UINT samples){
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
        Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
        return quality.NumQualityLevels!=0;
    };
    UINT first_use_cases=0;
    for(const bool d16:{true,false}){
        const auto format=d16?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT;
        const UINT samples=supports(format,4)?4:supports(format,2)?2:0;
        if(!samples){std::printf("SKIP: depth format%u lacks2x/4x MSAA\n",UINT(format));continue;}
        const auto d=shape(d16,false,samples);auto texture=allocate(d);
        // TRUE first use: no fixture clear/discard/copy initializes this target.
        for(UINT layer=0;layer<Layers;++layer)upload(texture,d,layer,0,1);
        verify(texture,d,false,[&](UINT l,UINT s,UINT x,UINT y){
            const auto code=depth_code(1,l,s,x,y);return d16?code:std::bit_cast<u32>(float(code)/65536.f);
        });
        ++first_use_cases;
    }
    if(!first_use_cases)throw std::runtime_error("No supported first-use MSAA depth format");
    const UINT samples=supports(DXGI_FORMAT_D32_FLOAT_S8X24_UINT,4)?4:
        supports(DXGI_FORMAT_D32_FLOAT_S8X24_UINT,2)?2:0;
    if(samples){
        const auto d=shape(false,true,samples);auto texture=allocate(d);
        auto views=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,Layers,false);
        renderer.RetainUntilFinish([texture,views](u64){(void)texture;(void)views;});
        texture.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_DEPTH_WRITE);
        for(UINT layer=0;layer<Layers;++layer){
            D3D12_DEPTH_STENCIL_VIEW_DESC view{};view.Format=DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;view.Texture2DMSArray={layer,1};
            device->CreateDepthStencilView(texture.handle.Get(),&view,views->Cpu(layer));
            renderer.RecordingList()->ClearDepthStencilView(views->Cpu(layer),D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,
                clear_depth(layer),UINT8(clear_stencil(layer)),0,nullptr);
        }
        std::array<UINT,Layers> depth_phase{},stencil_phase{};
        std::array<bool,Layers> partial_depth{},partial_stencil{};
        const auto check=[&]{
            verify(texture,d,false,[&](UINT l,UINT s,UINT x,UINT y){
                if(depth_phase[l]&&(!partial_depth[l]||(x<SmallWidth&&y<SmallHeight)))
                    return std::bit_cast<u32>(float(depth_code(depth_phase[l],l,s,x,y))/65536.f);
                return std::bit_cast<u32>(clear_depth(l));
            });
            const auto expected_stencil=[&](UINT l,UINT s,UINT x,UINT y){
                return stencil_phase[l]&&(!partial_stencil[l]||(x<SmallWidth&&y<SmallHeight))?
                    stencil_code(stencil_phase[l],l,s,x,y):clear_stencil(l);
            };
            verify(texture,d,true,expected_stencil);
            // A separate production readback check catches its typed stencil
            // declaration even when shader stencil writes are unsupported.
            verify_stencil_transfer(texture,d,expected_stencil);
        };
        check(); // Independent DSV baselines also validate verifier layer/plane selection.
        upload(texture,d,1,0,2);depth_phase[1]=2;check();
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        Check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options)));
        if(options.PSSpecifiedStencilRefSupported){
            upload(texture,d,1,1,3);stencil_phase[1]=3;check(); // Depth then stencil, same layer.
            upload(texture,d,2,1,4);stencil_phase[2]=4;check();
            upload(texture,d,2,0,5);depth_phase[2]=5;check(); // Stencil then depth, same layer.
        }else std::puts("SKIP: stencil uploads require PSSpecifiedStencilRefSupported; depth preserves cleared stencil checked");
        auto smaller=d;smaller.width=smaller.pitch=SmallWidth;smaller.height=SmallHeight;
        upload(texture,smaller,0,0,6);depth_phase[0]=6;partial_depth[0]=true;check();
        if(options.PSSpecifiedStencilRefSupported){
            // Layer0 outside the upload rectangle still has its preinitialized baseline.
            upload(texture,smaller,0,1,7);stencil_phase[0]=7;partial_stencil[0]=true;check();
        }
    }else std::puts("SKIP: combined D32/S8 lacks2x/4x MSAA; plane/partial controls unavailable");
    renderer.Finish();renderer.ValidateDebugMessages();
    std::printf("PASS: MSAA depth initialization; %u fresh placed D16/D32 cases, conditional DS plane/layer/partial controls\n",first_use_cases);
}
} // namespace NativeDX12
