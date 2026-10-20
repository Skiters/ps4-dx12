// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_index_expand.h"
#include "guest_index_expand_shader.h"
#include <cstring>
namespace NativeDX12 {
GuestIndexExpandPass::GuestIndexExpandPass(ID3D12Device* device){
    D3D12_ROOT_PARAMETER params[3]{};
    params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[0].Descriptor={0,0};
    params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[1].Descriptor={0,0};
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={0,0,8};
    const D3D12_ROOT_SIGNATURE_DESC desc{3,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;Check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=root_.Get();
    pipeline.CS={NativeGuestIndexExpand,sizeof(NativeGuestIndexExpand)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pipeline_)));
    zero_=Buffer(device,4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* data{};D3D12_RANGE none{};Check(zero_.handle->Map(0,&none,&data));memset(data,0,4);zero_.handle->Unmap(0,nullptr);
}
void GuestIndexExpandPass::Record(Rasterizer& renderer,const GuestIndexExpansion& plan,
    Resource* source,u64 offset,u32 available,u32 element,Resource& output){
    std::lock_guard lock(renderer.ExecutionMutex());
    const AmdGpu::PrimitiveType types[]{AmdGpu::PrimitiveType::TriangleFan,AmdGpu::PrimitiveType::LineLoop,AmdGpu::PrimitiveType::QuadStrip};
    if(plan.mode>2||(element!=2&&element!=4)||available>plan.input_count||!plan.output_count||
       GuestIndexExpansion::Make(types[plan.mode],plan.input_count).output_count!=plan.output_count||
       (!source&&(available||offset))||offset%element)
        throw std::invalid_argument("Invalid GPU index expansion plan/source");
    auto& input=source?*source:zero_;
    if(!input.handle||!output.handle||input.handle.Get()==output.handle.Get())
        throw std::invalid_argument("GPU index expansion needs distinct resources");
    const auto in=input.handle->GetDesc(),out=output.handle->GetDesc();
    const u64 end=offset+u64(available)*element;
    if(in.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||(in.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||
       (offset%4)+u64(available)*element>UINT32_MAX||end<offset||end>UINT64_MAX-3||((end+3)&~u64(3))>in.Width||
       out.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||out.Width<u64(plan.output_count)*4||
       !(out.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::out_of_range("GPU index expansion resource extent/usage");
    ComPtr<ID3D12Device> owner,device;Check(root_->GetDevice(IID_PPV_ARGS(&device)));
    for(auto* resource:{&input,&output}){Check(resource->handle->GetDevice(IID_PPV_ARGS(&owner)));
        if(owner.Get()!=device.Get())throw std::invalid_argument("GPU index expansion device mismatch");}
    renderer.RetainUntilFinish([input,output,root=root_,pipeline=pipeline_](u64){});
    auto* list=renderer.RecordingList();const auto previous=input.State(0);
    if(!(previous&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))input.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(root_.Get());list->SetPipelineState(pipeline_.Get());
    list->SetComputeRootShaderResourceView(0,input.handle->GetGPUVirtualAddress()+(offset&~u64(3)));
    list->SetComputeRootUnorderedAccessView(1,output.handle->GetGPUVirtualAddress());
    const u32 groups=(plan.output_count+127)/128,rows=(groups+65534)/65535,columns=(groups+rows-1)/rows;
    const u32 params[]{plan.mode,plan.input_count,plan.output_count,available,element,u32(offset%4),columns*128,0};
    list->SetComputeRoot32BitConstants(2,8,params,0);list->Dispatch(columns,rows,1);
    output.UavBarrier(list);output.Transition(list,D3D12_RESOURCE_STATE_INDEX_BUFFER);
    if(input.State(0)!=previous)input.Transition(list,previous);
}
}
