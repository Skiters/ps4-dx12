// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "multisample_readback.h"
namespace NativeDX12 {
// Inverse of MultisampleColorReadback's float4 sample-major representation.
// A sample-frequency pixel shader writes every sample independently. Ordinary
// CopyTextureRegion cannot upload individual samples of a multisampled texture.
// Caller retains this operation and both resource wrappers until GPU retirement.
class MultisampleColorUpload {
    Resource source_,destination_;
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12DescriptorHeap> rtvs_;
    UINT width_{},height_{},samples_{},layers_{},stride_{};
public:
    MultisampleColorUpload(ID3D12Device* device,const Resource& source,const Resource& destination)
        :source_(source),destination_(destination){
        const auto input=DX12_API_CALL(source.handle->GetDesc());
        const auto desc=DX12_API_CALL(destination.handle->GetDesc());
        if(input.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
           desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.MipLevels!=1||desc.SampleDesc.Count<2||
           !(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
           (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT&&
            desc.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT))
            throw std::invalid_argument("MSAA upload needs a float4 buffer and supported color target");
        width_=UINT(desc.Width);height_=desc.Height;samples_=desc.SampleDesc.Count;layers_=desc.DepthOrArraySize;
        const uint64_t bytes=uint64_t(width_)*height_*samples_*layers_*16;
        if(!width_||!height_||bytes>UINT_MAX||input.Width<bytes)
            throw std::out_of_range("MSAA upload source byte-address extent");
        D3D12_DESCRIPTOR_HEAP_DESC heap{};heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heap.NumDescriptors=layers_;
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&rtvs_))));
        stride_=DX12_API_CALL(device->GetDescriptorHandleIncrementSize(heap.Type));
        auto handle=DX12_API_CALL(rtvs_->GetCPUDescriptorHandleForHeapStart());
        for(UINT layer=0;layer<layers_;++layer){
            D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=desc.Format;
            view.ViewDimension=layers_>1?D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:D3D12_RTV_DIMENSION_TEXTURE2DMS;
            if(layers_>1){view.Texture2DMSArray.FirstArraySlice=layer;view.Texture2DMSArray.ArraySize=1;}
            DX12_API_CALL(device->CreateRenderTargetView(destination.handle.Get(),&view,handle));handle.ptr+=stride_;
        }
        D3D12_ROOT_PARAMETER parameters[2]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].Constants={0,0,4};
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_))));
        const auto vs=CompileMultisampleShader(R"(
float4 main(uint id:SV_VertexID):SV_Position{return float4(id==2?3:-1,id==1?3:-1,0,1);}
)","vs_5_0");
        const auto ps=CompileMultisampleShader(R"(
ByteAddressBuffer source:register(t0);
cbuffer Params:register(b0){uint width;uint height;uint samples;uint layer;};
float4 main(float4 position:SV_Position,uint sample:SV_SampleIndex):SV_Target{
    uint address=(((layer*samples+sample)*height+uint(position.y))*width+uint(position.x))*16;
    return asfloat(source.Load4(address));
})","ps_5_0");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=root_.Get();
        state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=TRUE;
        state.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        state.SampleMask=UINT_MAX;state.SampleDesc=desc.SampleDesc;state.NumRenderTargets=1;state.RTVFormats[0]=desc.Format;
        state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        Check(DX12_API_CALL(device->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&pipeline_))));
    }
    void Record(ID3D12GraphicsCommandList* list,Resource& source,Resource& destination){
        if(source.handle.Get()!=source_.handle.Get()||destination.handle.Get()!=destination_.handle.Get())
            throw std::invalid_argument("MSAA upload resources changed");
        // GENERIC_READ upload heaps already include pixel shader access and
        // must remain in their fixed state.
        if(!(source.state&D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
            source.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        destination.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
        // The following draw loop overwrites all samples of every array
        // slice. Initialize a placed render target without clearing guest
        // contents that will be supplied by the transfer itself.
        DX12_API_CALL(list->DiscardResource(destination.handle.Get(),nullptr));
        DX12_API_CALL(list->SetGraphicsRootSignature(root_.Get()));DX12_API_CALL(list->SetPipelineState(pipeline_.Get()));
        DX12_API_CALL(list->SetGraphicsRootShaderResourceView(0,DX12_API_CALL(source.handle->GetGPUVirtualAddress())));
        D3D12_VIEWPORT viewport{0,0,float(width_),float(height_),0,1};D3D12_RECT scissor{0,0,LONG(width_),LONG(height_)};
        DX12_API_CALL(list->RSSetViewports(1,&viewport));DX12_API_CALL(list->RSSetScissorRects(1,&scissor));
        DX12_API_CALL(list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
        auto handle=DX12_API_CALL(rtvs_->GetCPUDescriptorHandleForHeapStart());
        for(UINT layer=0;layer<layers_;++layer){
            const UINT constants[]{width_,height_,samples_,layer};
            DX12_API_CALL(list->SetGraphicsRoot32BitConstants(1,4,constants,0));
            DX12_API_CALL(list->OMSetRenderTargets(1,&handle,FALSE,nullptr));
            DX12_API_CALL(list->DrawInstanced(3,1,0,0));handle.ptr+=stride_;
        }
    }
};
}
