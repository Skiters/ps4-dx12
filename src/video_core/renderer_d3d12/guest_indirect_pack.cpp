// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_indirect_pack.h"
#include "guest_indirect_pack_shader.h"
#include "guest_indirect_dispatch_shader.h"
#include <cstring>
namespace NativeDX12 {
static_assert(offsetof(GraphicsRuntime,first_vertex)==0&&offsetof(GraphicsRuntime,base_instance)==4&&
    offsetof(GraphicsRuntime,indexed)==8&&offsetof(GraphicsRuntime,draw_id)==16);
GuestIndirectPackPass::GuestIndirectPackPass(ID3D12Device* device){
    D3D12_ROOT_PARAMETER parameters[8]{};
    for(u32 i=0;i<7;++i){
        parameters[i].ParameterType=i<3?D3D12_ROOT_PARAMETER_TYPE_SRV:D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[i].Descriptor={i<3?i:i-3,0};
    }
    parameters[7].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[7].Constants={0,0,8};
    D3D12_ROOT_SIGNATURE_DESC desc{8,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;Check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=root_.Get();
    pipeline.CS={NativeGuestIndirectPack,sizeof(NativeGuestIndirectPack)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pipeline_)));
    pipeline.CS={NativeGuestIndirectDispatch,sizeof(NativeGuestIndirectDispatch)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&dispatch_pipeline_)));
    D3D12_INDIRECT_ARGUMENT_DESC argument{};argument.Type=D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    const D3D12_COMMAND_SIGNATURE_DESC signature{sizeof(D3D12_DISPATCH_ARGUMENTS),1,&argument,0};
    Check(device->CreateCommandSignature(&signature,nullptr,IID_PPV_ARGS(&dispatch_signature_)));
}
void GuestIndirectPackPass::Record(Rasterizer& renderer,const GuestIndirectPackPlan& plan,
    Resource& source,u64 offset,Resource* count,u64 count_offset,const GraphicsRuntime& runtime,
    Resource& commands,Resource& constants,Resource& output_count){
    std::lock_guard lock(renderer.ExecutionMutex());
    if(plan!=GuestIndirectPackPlan::Make(plan.indexed,plan.source_stride,plan.count))
        throw std::invalid_argument("Inconsistent GPU indirect pack plan");
    ComPtr<ID3D12Device> device;Check(root_->GetDevice(IID_PPV_ARGS(&device)));
    const auto validate=[&](Resource& value,u64 start,u64 bytes,bool writing){
        if(!value.handle)throw std::invalid_argument("Missing GPU indirect pack buffer");
        const auto desc=value.handle->GetDesc();ComPtr<ID3D12Device> owner;
        Check(value.handle->GetDevice(IID_PPV_ARGS(&owner)));
        if(owner.Get()!=device.Get()||desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||start%4||
           start>desc.Width||bytes>desc.Width-start||
           (writing?!(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS):bool(desc.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)))
            throw std::invalid_argument("GPU indirect pack buffer device/extent/usage mismatch");
    };
    validate(source,offset,plan.source_bytes,false);
    if(count){validate(*count,count_offset,4,false);
        if(count->handle.Get()==source.handle.Get()&&count!=&source)
            throw std::invalid_argument("Shared GPU pack input requires one resource state");}
    else if(count_offset)throw std::invalid_argument("GPU indirect count offset without a count buffer");
    Resource* outputs[]{&commands,&constants,&output_count};
    const u64 sizes[]{plan.command_bytes,plan.runtime_bytes,4};
    for(u32 i=0;i<3;++i){
        validate(*outputs[i],0,sizes[i],true);
        if(outputs[i]->handle.Get()==source.handle.Get()||(count&&outputs[i]->handle.Get()==count->handle.Get()))
            throw std::invalid_argument("GPU indirect pack input/output alias");
        for(u32 j=0;j<i;++j)if(outputs[i]->handle.Get()==outputs[j]->handle.Get())
            throw std::invalid_argument("GPU indirect pack outputs require separate resources");
    }
    const u64 base=constants.handle->GetGPUVirtualAddress();
    if(base%256||base>UINT64_MAX-plan.runtime_bytes)throw std::out_of_range("GPU runtime constant address overflow/alignment");
    // A private immutable upload survives ownership callbacks and Finish; do
    // not borrow a resettable upload arena for an outstanding pack dispatch.
    auto seed=Buffer(device.Get(),plan.runtime_stride,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));
    memset(mapped,0,plan.runtime_stride);memcpy(mapped,&runtime,sizeof(runtime));seed.handle->Unmap(0,nullptr);
    auto dispatch=renderer.AcquireTransientBuffer(sizeof(D3D12_DISPATCH_ARGUMENTS));
    renderer.RetainUntilFinish([source,count_value=count?*count:seed,commands,constants,output_count,seed,
        dispatch,root=root_,pipeline=pipeline_,builder=dispatch_pipeline_,signature=dispatch_signature_](u64){});
    const auto source_state=source.State(0),count_state=count?count->State(0):D3D12_RESOURCE_STATE_COMMON;
    auto* list=renderer.RecordingList();
    if(!(source.State(0)&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if(count&&!(count->State(0)&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))count->Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    for(auto* output:outputs)output->Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatch.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(root_.Get());list->SetPipelineState(dispatch_pipeline_.Get());
    list->SetComputeRootShaderResourceView(0,source.handle->GetGPUVirtualAddress()+offset);
    list->SetComputeRootShaderResourceView(1,count?count->handle->GetGPUVirtualAddress()+count_offset:seed.handle->GetGPUVirtualAddress());
    list->SetComputeRootShaderResourceView(2,seed.handle->GetGPUVirtualAddress());
    for(u32 i=0;i<3;++i)list->SetComputeRootUnorderedAccessView(3+i,outputs[i]->handle->GetGPUVirtualAddress());
    list->SetComputeRootUnorderedAccessView(6,dispatch.handle->GetGPUVirtualAddress());
    const u32 parameters[]{plan.count,plan.source_stride,plan.indexed?1u:0u,plan.runtime_stride,
        plan.command_stride,u32(base),u32(base>>32),count?1u:0u};
    list->SetComputeRoot32BitConstants(7,8,parameters,0);list->Dispatch(1,1,1);
    output_count.UavBarrier(list);dispatch.UavBarrier(list);
    dispatch.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    list->SetPipelineState(pipeline_.Get());
    // u3 is unused by the packing entry; do not leave its root UAV pointing
    // at a resource which is now an indirect command input.
    list->SetComputeRootUnorderedAccessView(6,commands.handle->GetGPUVirtualAddress());
    list->ExecuteIndirect(dispatch_signature_.Get(),1,dispatch.handle.Get(),0,nullptr,0);
    for(auto* output:outputs)output->UavBarrier(list);
    // These can also be prepared guest shader buffers. The packing prepass
    // must return their authoritative state to the subsequent guest draw.
    if(source.State(0)!=source_state)source.Transition(list,source_state);
    if(count&&count!=&source&&count->State(0)!=count_state)count->Transition(list,count_state);
    commands.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    output_count.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    constants.Transition(list,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}
} // namespace NativeDX12
