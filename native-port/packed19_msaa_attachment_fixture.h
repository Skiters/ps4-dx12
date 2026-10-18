// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <d3dcompiler.h>
#include <array>
#include <cstdio>
#include <cstring>

namespace NativeDX12 {
inline void RunPacked19MsaaAttachmentFixture(ID3D12Device* device){
    const auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    const auto compile=[](const char* source,const char* target){ComPtr<ID3DBlob> code,errors;
        const auto hr=D3DCompile(source,strlen(source),"packed19-msaa-fixture",nullptr,nullptr,"main",target,
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&code,&errors);
        if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"Packed19 fixture compile failed");
        return code;};
    const auto vs=compile("float4 main(uint id:SV_VertexID):SV_Position{return float4(id==2?3:-1,id==1?3:-1,0,1);}","vs_5_1");
    const auto ps=compile(R"(
float4 main(float4 p:SV_Position,uint s:SV_SampleIndex):SV_Target{
    return float4(1+s%3,2,14,13)/15.0;
})","ps_5_1");
    // Independent typed observation, not GuestMultisample's packer/retile.
    // Original format19 guest word is B[15:12],G[11:8],R[7:4],A[3:0].
    const auto cs=compile(R"(
Texture2DMSArray<float4> input:register(t0);RWByteAddressBuffer output:register(u0);
cbuffer Shape:register(b0){uint width,height,samples,layers;};
[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){
    if(p.x>=width||p.y>=height||p.z>=samples*layers)return;
    uint4 c=(uint4)round(input.Load(int3(p.xy,p.z/samples),p.z%samples)*15.0);
    output.Store(4*((p.z*height+p.y)*width+p.x),(c.z<<12)|(c.y<<8)|(c.x<<4)|c.w);
})","cs_5_1");
    for(const UINT samples:{2u,4u,8u})for(const UINT tile:{0u,14u}){
        printf("CASE: packed19 native RTV samples=%u tile=%u layers=2 add-blend RG-only alternating-sample-mask\n",samples,tile);
        Rasterizer renderer(device);GuestTextureCache cache(device);
        try{
            GuestTextureDesc d;d.address=0x40000;d.width=13;d.height=11;d.pitch=16;d.layers=2;d.view_layers=2;
            d.samples=samples;d.tile_mode=tile;d.data_format=19;d.number_format=0;d.render_target=true;
            const auto layout=LayoutGuestTexture(d);
            require(layout.format==DXGI_FORMAT_B4G4R4A4_UNORM&&!layout.expanded_bytes_per_pixel&&layout.HostBytesPerPixel()==2,
                "Native19 attachment silently expanded or changed native packing");
            auto wrong_layout=layout;wrong_layout.format=DXGI_FORMAT_R16_UNORM;
            require(!GuestTextureCache::CanTransferIndependentMultisample(d,wrong_layout),
                "Native19 transfer admitted a mismatched typed allocation");
            wrong_layout=layout;wrong_layout.bytes_per_pixel=4;
            require(!GuestTextureCache::CanTransferIndependentMultisample(d,wrong_layout),
                "Native19 transfer admitted non-halfword guest geometry");
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{layout.format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
            require(quality.NumQualityLevels!=0,"Original-capable test adapter lacks required B4 sample count");
            const auto seed_word=[](UINT layer,UINT s,UINT x,UINT y){
                const UINT r=(x+layer+s*2)%16,g=(2*y+s+layer*3)%16,b=(x+y+s*3)%16,a=(4+x*3+y+s+layer)%16;
                return u16((b<<12)|(g<<8)|(r<<4)|a);};
            // CPU address oracle and explicit original bit fields; no production
            // native decoder, inverse tiler or GPU readback defines expectations.
            std::vector<std::byte> initial(size_t(layout.guest_bytes),std::byte{0xa7});
            for(UINT l=0;l<d.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){
                const auto word=seed_word(l,s,x,y);memcpy(initial.data()+GuestPixelOffset(d,layout,0,l,x,y,s),&word,2);}
            auto& image=cache.Upload(d,initial,renderer.RecordingList(),renderer.CompletedFence());
            auto pin=cache.PinBinding(image);
            // This fixture checks native sample contents/transfer, not PageWatch.
            // Physical ownership has the same exact packed guest-word geometry.
            cache.SetIndependentImageRetention([&](const auto& owner,const auto& physical){
                return owner.data_format==19&&owner.number_format==0&&physical.guest_bytes==layout.guest_bytes;});
            cache.SetOwnershipAcquirer([&](auto& entry){const auto owner=DescribeGuestImageOwnership(entry);
                if(!owner||!CanRetainGuestImageOwnership(cache,*owner))return false;
                entry.buffer_alias_version=true;entry.SetGpuOwned(true);return true;});
            require(cache.Own(image),"Packed19 native ownership geometry rejected");
            auto physical=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            auto physical_upload=Buffer(device,layout.guest_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            std::vector<std::byte> physical_bytes(size_t(layout.guest_bytes),std::byte{0x5d});void* mapped{};
            Check(physical_upload.handle->Map(0,nullptr,&mapped));memcpy(mapped,physical_bytes.data(),physical_bytes.size());physical_upload.handle->Unmap(0,nullptr);
            renderer.RecordingList()->CopyBufferRegion(physical.handle.Get(),0,physical_upload.handle.Get(),0,layout.guest_bytes);
            renderer.RetainUntilFinish([physical,physical_upload](u64){(void)physical;(void)physical_upload;});
            ComPtr<ID3D12RootSignature> draw_root,probe_root;ComPtr<ID3DBlob> blob,errors;
            D3D12_ROOT_SIGNATURE_DESC empty{};Check(D3D12SerializeRootSignature(&empty,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
            Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&draw_root)));
            D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
            D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].DescriptorTable={2,ranges};
            parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,4};
            D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
            Check(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
            Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&probe_root)));
            const UINT sample_mask=0x55u&((1u<<samples)-1u);
            D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=draw_root.Get();state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
            state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=TRUE;
            state.SampleMask=sample_mask;state.SampleDesc.Count=samples;state.NumRenderTargets=1;state.RTVFormats[0]=layout.format;state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            auto& blend=state.BlendState.RenderTarget[0];blend.BlendEnable=TRUE;blend.SrcBlend=blend.DestBlend=D3D12_BLEND_ONE;blend.BlendOp=D3D12_BLEND_OP_ADD;
            blend.SrcBlendAlpha=blend.DestBlendAlpha=D3D12_BLEND_ONE;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_RED|D3D12_COLOR_WRITE_ENABLE_GREEN;
            ComPtr<ID3D12PipelineState> draw_pipeline,probe_pipeline;Check(device->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&draw_pipeline)));
            D3D12_COMPUTE_PIPELINE_STATE_DESC compute{};compute.pRootSignature=probe_root.Get();compute.CS={cs->GetBufferPointer(),cs->GetBufferSize()};Check(device->CreateComputePipelineState(&compute,IID_PPV_ARGS(&probe_pipeline)));
            auto rtvs=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,d.layers,false);
            auto views=std::make_shared<Descriptors>(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);
            const u64 probe_bytes=u64(d.layers)*samples*d.width*d.height*4;auto probe=GuestTiler::Scratch(device,probe_bytes);
            D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=layout.format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2DMSArray={0,d.layers};
            device->CreateShaderResourceView(image.texture.handle.Get(),&srv,views->Cpu(0));
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(probe_bytes/4);uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
            device->CreateUnorderedAccessView(probe.handle.Get(),nullptr,&uav,views->Cpu(1));
            renderer.RetainUntilFinish([draw_root,probe_root,draw_pipeline,probe_pipeline,rtvs,views,probe,pin](u64){(void)draw_root;(void)probe_root;(void)draw_pipeline;(void)probe_pipeline;(void)rtvs;(void)views;(void)probe;(void)pin;});
            for(UINT phase=0;phase<2;++phase){
                auto* list=renderer.RecordingList();
                if(phase){image.texture.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);list->SetGraphicsRootSignature(draw_root.Get());list->SetPipelineState(draw_pipeline.Get());
                    D3D12_VIEWPORT viewport{0,0,float(d.width),float(d.height),0,1};D3D12_RECT scissor{1,2,LONG(d.width-1),LONG(d.height-1)};
                    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                    for(UINT l=0;l<d.layers;++l){D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=layout.format;view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;view.Texture2DMSArray={l,1};
                        device->CreateRenderTargetView(image.texture.handle.Get(),&view,rtvs->Cpu(l));const auto rtv=rtvs->Cpu(l);list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);list->DrawInstanced(3,1,0,0);}}
                image.texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);probe.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                ID3D12DescriptorHeap* heaps[]{views->Heap()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(probe_root.Get());list->SetPipelineState(probe_pipeline.Get());list->SetComputeRootDescriptorTable(0,views->Gpu(0));
                const UINT constants[]{d.width,d.height,samples,d.layers};list->SetComputeRoot32BitConstants(1,4,constants,0);list->Dispatch((d.width+7)/8,(d.height+7)/8,d.layers*samples);
                auto observed=Buffer(device,probe_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);probe.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(observed.handle.Get(),0,probe.handle.Get(),0,probe_bytes);
                renderer.Finish();Check(observed.handle->Map(0,nullptr,&mapped));const auto* words=static_cast<const u32*>(mapped);bool equal=true;
                auto expected=physical_bytes;
                for(UINT l=0;l<d.layers;++l)for(UINT s=0;s<samples;++s)for(UINT y=0;y<d.height;++y)for(UINT x=0;x<d.width;++x){
                    u16 word=seed_word(l,s,x,y);
                    if(phase&&(sample_mask&(1u<<s))&&x>=1&&x<d.width-1&&y>=2&&y<d.height-1){
                        const UINT red=std::min(15u,UINT((word>>4)&15)+1+s%3),green=std::min(15u,UINT((word>>8)&15)+2);
                        word=u16((word&0xf00fu)|(green<<8)|(red<<4));}
                    const auto index=((l*samples+s)*d.height+y)*d.width+x;
                    if(equal&&words[index]!=word)fprintf(stderr,"Typed native19 phase=%u layer=%u sample=%u x=%u y=%u got=%04x expected=%04x\n",phase,l,s,x,y,words[index],word);
                    equal&=words[index]==word;memcpy(expected.data()+GuestPixelOffset(d,layout,0,l,x,y,s),&word,2);
                }
                D3D12_RANGE none{};observed.handle->Unmap(0,&none);require(equal,"Independent native19 typed sample contents mismatch");
                // Upload/sample draw executed before this transfer admission.
                // Old implementation rejects native19 here, despite legal RTV.
                auto retiled=cache.RetileBufferVersion(image,physical,renderer.RecordingList(),renderer.CompletedFence());
                auto result=Buffer(device,layout.guest_bytes*2,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                retiled.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);physical.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),0,retiled.handle.Get(),0,layout.guest_bytes);
                renderer.RecordingList()->CopyBufferRegion(result.handle.Get(),layout.guest_bytes,physical.handle.Get(),0,layout.guest_bytes);
                cache.Submitted(renderer.Flush());renderer.Finish();cache.Collect(renderer.CompletedFence());
                Check(result.handle->Map(0,nullptr,&mapped));const auto* bytes=static_cast<const std::byte*>(mapped);
                equal=memcmp(bytes,expected.data(),expected.size())==0;const bool unchanged=memcmp(bytes+layout.guest_bytes,physical_bytes.data(),physical_bytes.size())==0;
                if(!equal)for(size_t i=0;i<expected.size();++i)if(bytes[i]!=expected[i]){fprintf(stderr,"Retile native19 phase=%u offset=%zu got=%02x expected=%02x\n",phase,i,std::to_integer<unsigned>(bytes[i]),std::to_integer<unsigned>(expected[i]));break;}
                result.handle->Unmap(0,&none);require(equal&&unchanged,"Packed19 retile corrupted sample word, physical B or padding");renderer.ValidateDebugMessages();
            }
            puts("PASS: packed19 native MSAA exact seed and blended sample contents, masks, independent typed probes, full private guest bytes and unchanged B/padding");
        }catch(...){renderer.Finish();throw;}
    }
}
} // namespace NativeDX12
