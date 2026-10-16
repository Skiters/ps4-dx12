// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/runtime_module_trace.h"
#include "compute_pipeline.h"
#include <d3dcompiler.h>

namespace NativeDX12 {
inline ComPtr<ID3DBlob> CompileMultisampleShader(const char* source,const char* target){
    struct Library{HMODULE value;~Library(){if(value)FreeLibrary(value);}};
    // Returned compiler blobs retain vtables implemented by this DLL.
    static const Library library{
        Common::RuntimeModuleTrace::Load(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
    if(!library.value)throw std::runtime_error("System multisample shader compiler unavailable");
    const auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library.value,"D3DCompile"));
    if(!compile)throw std::runtime_error("Multisample shader compiler entry unavailable");
    ComPtr<ID3DBlob> code,errors;
    const auto result=compile(source,strlen(source),"native-msaa",nullptr,nullptr,"main",target,
        D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
    if(FAILED(result))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"MSAA shader compilation failed");
    return code;
}

// Sample-preserving extraction, not a resolve. Output is float4 in
// layer/sample-major order: (((layer * samples + sample) * height + y) * width + x) * 16 bytes.
// Retain this operation and its output until the submission fence retires.
// The source descriptor is immutable, including across in-flight submissions.
class MultisampleColorReadback {
    Resource source_; // Retain placed-allocation lease as well as the COM resource.
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12DescriptorHeap> descriptors_;
    Resource output_;
    UINT width_{},height_{},samples_{},layers_{};
public:
    MultisampleColorReadback(ID3D12Device* device,const Resource& source):source_(source){
        const auto desc=DX12_API_CALL(source.handle->GetDesc());
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
           desc.MipLevels!=1||desc.SampleDesc.Count<2||desc.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE||
           (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_R16G16B16A16_FLOAT&&
            desc.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT))
            throw std::invalid_argument("MSAA extraction needs a supported float color view");
        width_=UINT(desc.Width);height_=desc.Height;samples_=desc.SampleDesc.Count;layers_=desc.DepthOrArraySize;
        const uint64_t bytes=uint64_t(width_)*height_*samples_*layers_*16;
        if(!width_||!height_||bytes>UINT_MAX||uint64_t(samples_)*layers_>65535)
            throw std::out_of_range("MSAA extraction byte-address/dispatch extent");
        output_=UavBuffer(device,bytes);
        D3D12_DESCRIPTOR_HEAP_DESC heap{};heap.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap.NumDescriptors=1;heap.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(DX12_API_CALL(device->CreateDescriptorHeap(&heap,IID_PPV_ARGS(&descriptors_))));
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=desc.Format;
        view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMS;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if(layers_>1){view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;view.Texture2DMSArray.ArraySize=layers_;}
        DX12_API_CALL(device->CreateShaderResourceView(source_.handle.Get(),&view,DX12_API_CALL(descriptors_->GetCPUDescriptorHandleForHeapStart())));
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable={1,&range};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_))));
        const auto code=CompileMultisampleShader(layers_>1?R"(
Texture2DMSArray<float4> source : register(t0);
RWByteAddressBuffer output : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID){
    uint width,height,layers,samples;source.GetDimensions(width,height,layers,samples);
    if(id.x>=width||id.y>=height||id.z>=layers*samples)return;
    output.Store4(((id.z*height+id.y)*width+id.x)*16,
        asuint(source.Load(int3(id.xy,id.z/samples),id.z%samples)));
})":R"(
Texture2DMS<float4> source : register(t0);
RWByteAddressBuffer output : register(u0);
[numthreads(8,8,1)] void main(uint3 id : SV_DispatchThreadID){
    uint width,height,samples;source.GetDimensions(width,height,samples);
    if(id.x>=width||id.y>=height||id.z>=samples)return;
    output.Store4(((id.z*height+id.y)*width+id.x)*16,asuint(source.Load(id.xy,id.z)));
})","cs_5_0");
        D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=root_.Get();
        state.CS={code->GetBufferPointer(),code->GetBufferSize()};
        Check(DX12_API_CALL(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&pipeline_))));
    }
    void Record(ID3D12GraphicsCommandList* list,Resource& source){
        if(source.handle.Get()!=source_.handle.Get())throw std::invalid_argument("MSAA extraction source changed");
        source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        output_.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        ID3D12DescriptorHeap* heaps[]{descriptors_.Get()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
        DX12_API_CALL(list->SetComputeRootSignature(root_.Get()));DX12_API_CALL(list->SetPipelineState(pipeline_.Get()));
        DX12_API_CALL(list->SetComputeRootDescriptorTable(0,DX12_API_CALL(descriptors_->GetGPUDescriptorHandleForHeapStart())));
        DX12_API_CALL(list->SetComputeRootUnorderedAccessView(1,DX12_API_CALL(output_.handle->GetGPUVirtualAddress())));
        DX12_API_CALL(list->Dispatch((width_+7)/8,(height_+7)/8,samples_*layers_));output_.UavBarrier(list);
    }
    Resource& Output(){return output_;}
};
}
