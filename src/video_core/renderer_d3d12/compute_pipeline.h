#include "common/api_trace.h"
#pragma once
#include "resources.h"
#include <span>
#include <cstring>
namespace NativeDX12 {
// Native compute PSO with one explicitly specified root UAV buffer binding.
// This does not infer PS4 shader resource tables or guest workgroup semantics.
class ComputePipeline {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
public:
    ComputePipeline(ID3D12Device* device,std::span<const uint8_t> signed_dxil,
                    UINT uav_register=0,UINT register_space=0) {
        if(signed_dxil.size()<32||memcmp(signed_dxil.data(),"DXBC",4))
            throw std::invalid_argument("compute pipeline needs DXIL container");
        D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameter.Descriptor.ShaderRegister=uav_register;parameter.Descriptor.RegisterSpace=register_space;
        parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_PARAMETER parameters[2]{parameter,{}};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants={0,31,8}; // Mesa compute runtime CBV b0, space31.
        parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC desc{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;auto hr=DX12_API_CALL(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
        if(FAILED(hr))throw std::runtime_error(errors?std::string(static_cast<const char*>(DX12_API_CALL(errors->GetBufferPointer())),DX12_API_CALL(errors->GetBufferSize())):"root signature serialization failed");
        Check(DX12_API_CALL(device->CreateRootSignature(0,DX12_API_CALL(blob->GetBufferPointer()),DX12_API_CALL(blob->GetBufferSize()),IID_PPV_ARGS(&root_))));
        D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=root_.Get();
        state.CS={signed_dxil.data(),signed_dxil.size()};Check(DX12_API_CALL(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&pipeline_))));
    }
    void Dispatch(ID3D12GraphicsCommandList* list,Resource& output,UINT x,UINT y=1,UINT z=1,
                  UINT64 buffer_offset=0) {
        auto desc=DX12_API_CALL(output.handle->GetDesc());
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||!(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)||
           buffer_offset>=desc.Width||buffer_offset%4||!x||!y||!z||x>65535||y>65535||z>65535)
            throw std::invalid_argument("invalid compute UAV or dispatch dimensions");
        output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        DX12_API_CALL(list->SetComputeRootSignature(root_.Get()));DX12_API_CALL(list->SetPipelineState(pipeline_.Get()));
        DX12_API_CALL(list->SetComputeRootUnorderedAccessView(0,DX12_API_CALL(output.handle->GetGPUVirtualAddress())+buffer_offset));
        const UINT runtime[8]={x,y,z,0,0,0,0,0}; // Group counts and zero base group.
        DX12_API_CALL(list->SetComputeRoot32BitConstants(1,8,runtime,0));
        DX12_API_CALL(list->Dispatch(x,y,z));output.UavBarrier(list);
    }
};
inline Resource UavBuffer(ID3D12Device* device,UINT64 bytes) {
    if(!bytes||bytes%4)throw std::invalid_argument("UAV buffer must have dword size");
    D3D12_HEAP_PROPERTIES p{};p.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;
    d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    // The runtime creates default-heap buffers in COMMON. Callers transition
    // before copy/UAV use; keep our recorded state consistent with the device.
    Resource r;r.state=D3D12_RESOURCE_STATE_COMMON;
    Check(DX12_API_CALL(device->CreateCommittedResource(&p,D3D12_HEAP_FLAG_NONE,&d,r.state,nullptr,IID_PPV_ARGS(&r.handle))));return r;
}
}
