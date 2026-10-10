// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/multisample_readback.h"
#include "video_core/renderer_d3d12/multisample_upload.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cmath>
#include <cstdio>
using namespace NativeDX12;
int main(){try{
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 description{};adapter->GetDesc1(&description);
        if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    const auto vs=CompileMultisampleShader(R"(
float4 main(uint id:SV_VertexID):SV_Position{
    return float4(id==2?3:-1,id==1?3:-1,0,1);
})","vs_5_0");
    const auto ps=CompileMultisampleShader(R"(
cbuffer Params : register(b0){uint layer;uint floating;};
float4 main(float4 position:SV_Position,uint sample:SV_SampleIndex):SV_Target{
    float red=floating?(sample+1+16*layer)/8.0-2.0:(sample+1+16*layer)*(5.0/255.0);
    float green=floating?(8-sample)/4.0+2.0:(8-sample)*(23.0/255.0);
    return float4(red,green,(uint(position.x)+16*uint(position.y))*(floating?1.0/256.0:1.0/255.0),1);
})","ps_5_0");
    ComPtr<ID3D12RootSignature> root;ComPtr<ID3DBlob> signature,errors;
    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    D3D12_ROOT_PARAMETER constants{};constants.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.Constants={0,0,2};root_desc.NumParameters=1;root_desc.pParameters=&constants;
    Check(D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors));
    Check(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)));
    UINT tested=0;
    for(const auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT})
    for(UINT layers:{1U,3U})for(UINT samples:{2U,4U,8U}){
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
        Check(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality)));
        if(!quality.NumQualityLevels)continue;
        constexpr UINT Width=13,Height=11;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=Width;desc.Height=Height;desc.DepthOrArraySize=UINT16(layers);desc.MipLevels=1;
        desc.Format=format;desc.SampleDesc.Count=samples;
        desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_HEAP_PROPERTIES properties{};properties.Type=D3D12_HEAP_TYPE_DEFAULT;
        Resource color;color.state=D3D12_RESOURCE_STATE_RENDER_TARGET;
        Check(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,color.state,nullptr,IID_PPV_ARGS(&color.handle)));
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};heap_desc.NumDescriptors=1;heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        ComPtr<ID3D12DescriptorHeap> rtvs;Check(device->CreateDescriptorHeap(&heap_desc,IID_PPV_ARGS(&rtvs)));
        const auto rtv=rtvs->GetCPUDescriptorHandleForHeapStart();device->CreateRenderTargetView(color.handle.Get(),nullptr,rtv);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=root.Get();
        state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=TRUE;
        state.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        state.SampleMask=UINT_MAX;state.SampleDesc.Count=samples;state.NumRenderTargets=1;state.RTVFormats[0]=desc.Format;
        state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&pipeline)));
        Rasterizer rasterizer(device.Get());auto* list=rasterizer.RecordingList();
        list->SetGraphicsRootSignature(root.Get());list->SetPipelineState(pipeline.Get());
        D3D12_VIEWPORT viewport{0,0,float(Width),float(Height),0,1};D3D12_RECT scissor{0,0,Width,Height};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
        list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for(UINT layer=0;layer<layers;++layer){
            D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=desc.Format;
            view.ViewDimension=layers>1?D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:D3D12_RTV_DIMENSION_TEXTURE2DMS;
            if(layers>1){view.Texture2DMSArray.FirstArraySlice=layer;view.Texture2DMSArray.ArraySize=1;}
            device->CreateRenderTargetView(color.handle.Get(),&view,rtv);
            list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
            list->SetGraphicsRoot32BitConstant(0,layer,0);
            list->SetGraphicsRoot32BitConstant(0,format!=DXGI_FORMAT_R8G8B8A8_UNORM,1);
            list->DrawInstanced(3,1,0,0);
        }
        MultisampleColorReadback extraction(device.Get(),color);extraction.Record(list,color);
        auto& output=extraction.Output();output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        const u64 bytes=u64(Width)*Height*samples*layers*16;
        auto readback=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,bytes);rasterizer.Finish();
        void* mapped{};D3D12_RANGE range{0,SIZE_T(bytes)};Check(readback.handle->Map(0,&range,&mapped));
        const auto* values=static_cast<const float*>(mapped);bool valid=true;
        std::vector<std::byte> baseline(static_cast<size_t>(bytes));
        memcpy(baseline.data(),mapped,size_t(bytes));
        for(UINT layer=0;layer<layers;++layer)for(UINT sample=0;sample<samples;++sample)for(UINT pixel=0;pixel<Width*Height;++pixel){
            const bool floating=format!=DXGI_FORMAT_R8G8B8A8_UNORM;
            const float red=floating?float(sample+1+16*layer)/8-2:float(sample+1+16*layer)*5/255;
            const float green=floating?float(8-sample)/4+2:float(8-sample)*23/255;
            const float blue=float(pixel%Width+16*(pixel/Width))/(floating?256:255);
            const auto* v=values+((layer*samples+sample)*Width*Height+pixel)*4;
            const bool equal=std::abs(v[0]-red)<0.00001f&&std::abs(v[1]-green)<0.00001f&&std::abs(v[2]-blue)<0.00001f&&v[3]==1;
            if(valid&&!equal)fprintf(stderr,"samples=%u sample=%u pixel=%u got=(%g,%g,%g,%g) expected=(%g,%g,%g,1)\n",samples,sample,pixel,v[0],v[1],v[2],v[3],red,green,blue);
            valid&=equal;
        }
        D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
        if(!valid)throw std::runtime_error("MSAA sample values/order were not preserved");
        Resource restored;restored.state=D3D12_RESOURCE_STATE_RENDER_TARGET;
        Check(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,restored.state,nullptr,IID_PPV_ARGS(&restored.handle)));
        MultisampleColorReadback restored_extraction(device.Get(),restored);
        for(const bool cpu_upload:{false,true}){
            Resource cpu;
            if(cpu_upload){
                cpu=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
                void* data{};Check(cpu.handle->Map(0,&empty,&data));memcpy(data,baseline.data(),size_t(bytes));
                cpu.handle->Unmap(0,nullptr);
            }
            auto& input=cpu_upload?cpu:output;
            MultisampleColorUpload upload(device.Get(),input,restored);
            list=rasterizer.RecordingList();upload.Record(list,input,restored);
            restored_extraction.Record(list,restored);
            auto& extracted=restored_extraction.Output();extracted.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(readback.handle.Get(),0,extracted.handle.Get(),0,bytes);rasterizer.Finish();
            Check(readback.handle->Map(0,&range,&mapped));
            const bool roundtrip=memcmp(mapped,baseline.data(),size_t(bytes))==0;
            readback.handle->Unmap(0,&empty);
            if(!roundtrip)throw std::runtime_error("MSAA upload/extraction roundtrip changed samples");
        }
        rasterizer.ValidateDebugMessages();printf("PASS: format=%u, %ux MSAA, %u layers, 13x11 distinct pixels/samples, GPU and CPU upload roundtrips\n",UINT(format),samples,layers);++tested;
    }
    if(!tested)throw std::runtime_error("No supported MSAA sample count");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
