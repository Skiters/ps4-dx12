// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "shader_recompiler/native_dma.h"
#include <d3dcompiler.h>
namespace NativeDX12 {
// Compact page bits into a bounded queue on the GPU. If the queue fills, keep
// every unexported bit in the source bitmap for a later pass; never drop faults.
class DmaFaultCompactor {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
public:
    static constexpr UINT Capacity=8192;
    static constexpr UINT Bytes=(Capacity+1)*sizeof(u32);
    explicit DmaFaultCompactor(ID3D12Device* device){
        constexpr char source[]=R"(
RWByteAddressBuffer faults : register(u0);
RWByteAddressBuffer exported : register(u1);
[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {
    if(id.x>=FAULT_WORDS)return;
    uint bits=faults.Load(id.x*4);
    if(!bits)return;
    uint first;
    exported.InterlockedAdd(0,countbits(bits),first);
    if(first>=CAPACITY)return;
    uint available=min(countbits(bits),CAPACITY-first),consumed=0;
    for(uint i=0;i<available;++i){
        uint bit=firstbitlow(bits),mask=1u<<bit;
        exported.Store((first+i+1)*4,id.x*32+bit);
        consumed|=mask;bits&=~mask;
    }
    uint ignored;faults.InterlockedAnd(id.x*4,~consumed,ignored);
})";
        struct Library {HMODULE handle;~Library(){if(handle)FreeLibrary(handle);}} library{
            LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
        if(!library.handle)throw std::runtime_error("System DMA shader compiler unavailable");
        auto compile=reinterpret_cast<decltype(&D3DCompile)>(GetProcAddress(library.handle,"D3DCompile"));
        if(!compile)throw std::runtime_error("System DMA D3DCompile unavailable");
        const auto words=std::to_string(Shader::NativeDma::PageCount/32),capacity=std::to_string(Capacity);
        const D3D_SHADER_MACRO defines[]{{"FAULT_WORDS",words.c_str()},{"CAPACITY",capacity.c_str()},{nullptr,nullptr}};
        ComPtr<ID3DBlob> code,errors;
        const auto compiled=compile(source,sizeof(source)-1,"native-dma-faults",defines,nullptr,"main","cs_5_0",
            D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors);
        if(FAILED(compiled))throw std::runtime_error(errors?std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):"DMA compaction compilation failed");
        D3D12_ROOT_PARAMETER parameters[2]{};
        for(UINT i=0;i<2;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
            parameters[i].Descriptor.ShaderRegister=i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;}
        D3D12_ROOT_SIGNATURE_DESC desc{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> signature;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors)));
        Check(DX12_API_CALL(device->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root_))));
        D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=root_.Get();
        state.CS={code->GetBufferPointer(),code->GetBufferSize()};
        Check(DX12_API_CALL(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&pipeline_))));
    }
    void Dispatch(ID3D12GraphicsCommandList* list,Resource& faults,Resource& exported,Resource& zero){
        faults.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);faults.UavBarrier(list);
        exported.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(list->CopyBufferRegion(exported.handle.Get(),0,zero.handle.Get(),0,4));
        exported.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        DX12_API_CALL(list->SetComputeRootSignature(root_.Get()));DX12_API_CALL(list->SetPipelineState(pipeline_.Get()));
        DX12_API_CALL(list->SetComputeRootUnorderedAccessView(0,DX12_API_CALL(faults.handle->GetGPUVirtualAddress())));
        DX12_API_CALL(list->SetComputeRootUnorderedAccessView(1,DX12_API_CALL(exported.handle->GetGPUVirtualAddress())));
        DX12_API_CALL(list->Dispatch((Shader::NativeDma::PageCount/32+63)/64,1,1));
        faults.UavBarrier(list);exported.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    }
};
}
