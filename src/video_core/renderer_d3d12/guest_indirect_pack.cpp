// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_indirect_pack.h"
#include "guest_indirect_pack_shader.h"
#include "guest_indirect_dispatch_shader.h"
#include "guest_indirect_index_shader.h"
#include "compute_pipeline.h"
#include <cstring>
namespace NativeDX12 {
static_assert(sizeof(D3D12_INDEX_BUFFER_VIEW)==16&&DXGI_FORMAT_R32_UINT==42);
static_assert(offsetof(GraphicsRuntime,first_vertex)==0&&offsetof(GraphicsRuntime,base_instance)==4&&
    offsetof(GraphicsRuntime,indexed)==8&&offsetof(GraphicsRuntime,draw_id)==16);
GuestIndirectPackPass::GuestIndirectPackPass(ID3D12Device* device){
    D3D12_ROOT_PARAMETER parameters[10]{};
    for(u32 i=0;i<7;++i){
        parameters[i].ParameterType=i<3?D3D12_ROOT_PARAMETER_TYPE_SRV:D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[i].Descriptor={i<3?i:i-3,0};
    }
    parameters[7].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[7].Constants={0,0,20};
    parameters[8].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[8].Descriptor={3,0};
    parameters[9].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[9].Descriptor={4,0};
    D3D12_ROOT_SIGNATURE_DESC desc{10,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;Check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
    Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};pipeline.pRootSignature=root_.Get();
    pipeline.CS={NativeGuestIndirectPack,sizeof(NativeGuestIndirectPack)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pipeline_)));
    pipeline.CS={NativeGuestIndirectDispatch,sizeof(NativeGuestIndirectDispatch)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&dispatch_pipeline_)));
    pipeline.CS={NativeGuestIndirectIndex,sizeof(NativeGuestIndirectIndex)};
    Check(device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&index_pipeline_)));
    D3D12_INDIRECT_ARGUMENT_DESC arguments[2]{};
    arguments[0].Type=D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;arguments[0].Constant={7,8,1};
    arguments[1].Type=D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    const D3D12_COMMAND_SIGNATURE_DESC signature{sizeof(u32)+sizeof(D3D12_DISPATCH_ARGUMENTS),2,arguments,0};
    Check(device->CreateCommandSignature(&signature,root_.Get(),IID_PPV_ARGS(&dispatch_signature_)));
    dispatch_=UavBuffer(device,2*(sizeof(u32)+sizeof(D3D12_DISPATCH_ARGUMENTS)));
    null_template_=Buffer(device,1024,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};D3D12_RANGE none{};Check(null_template_.handle->Map(0,&none,&mapped));
    memset(mapped,0,1024);null_template_.handle->Unmap(0,nullptr);
}
void GuestIndirectPackPass::Record(Rasterizer& renderer,const GuestIndirectPackPlan& plan,
    Resource& source,u64 offset,Resource* count,u64 count_offset,const GraphicsRuntime& runtime,
    Resource& commands,Resource* constants,Resource& output_count,const GuestIndirectIndexDecode* decode){
    std::lock_guard lock(renderer.ExecutionMutex());
    if(plan!=GuestIndirectPackPlan::Make(plan.indexed,plan.source_stride,plan.count,plan.with_runtime,plan.index_views)||bool(constants)!=plan.with_runtime||bool(decode)!=plan.index_views)
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
    auto& index_source=decode&&decode->source?*decode->source:null_template_;
    const u64 index_bytes=decode?GuestIndirectIndexDecode::OutputBytes(decode->available,plan.count):0;
    if(decode){
        if(!decode->output||decode->swap_mode>3||decode->offset%2||
           (decode->restart&&decode->marker>0xffff)||
           (!decode->source&&(decode->available||decode->offset))||index_bytes>UINT32_MAX||
           u64((std::max(1u,decode->available)+127)/128)*plan.count>UINT32_MAX/128)
            throw std::invalid_argument("Invalid per-command indirect index decode");
        const u64 bytes=(decode->offset%4)+u64(decode->available)*2;
        if(bytes>UINT32_MAX-3)throw std::out_of_range("Indirect index source exceeds shader addressing");
        validate(index_source,decode->offset&~u64(3),(bytes+3)&~u64(3),false);
        for(auto* input:{&source,count})if(input&&input->handle.Get()==index_source.handle.Get()&&input!=&index_source)
            throw std::invalid_argument("Shared indirect index inputs require one authoritative resource state");
    }
    Resource* outputs[]{&commands,constants,&output_count,decode?decode->output:nullptr};
    const u64 sizes[]{plan.command_bytes,plan.runtime_bytes,4,index_bytes};
    for(u32 i=0;i<4;++i){
        if(!outputs[i])continue;
        validate(*outputs[i],0,sizes[i],true);
        if(outputs[i]->handle.Get()==source.handle.Get()||(count&&outputs[i]->handle.Get()==count->handle.Get())||
           (decode&&outputs[i]->handle.Get()==index_source.handle.Get()))
            throw std::invalid_argument("GPU indirect pack input/output alias");
        for(u32 j=0;j<i;++j)if(outputs[j]&&outputs[i]->handle.Get()==outputs[j]->handle.Get())
            throw std::invalid_argument("GPU indirect pack outputs require separate resources");
    }
    const u64 base=constants?constants->handle->GetGPUVirtualAddress():0;
    if(base%256||base>UINT64_MAX-plan.runtime_bytes)throw std::out_of_range("GPU runtime constant address overflow/alignment");
    const u64 index_base=decode?decode->output->handle->GetGPUVirtualAddress():0;
    if(index_base%4||index_base>UINT64_MAX-index_bytes)throw std::out_of_range("Indirect index arena address overflow");
    // A private immutable upload survives ownership callbacks and Finish; do
    // not borrow a resettable upload arena for an outstanding pack dispatch.
    auto seed=null_template_;
    if(plan.with_runtime){
        seed=Buffer(device.Get(),plan.runtime_stride,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));
        memset(mapped,0,plan.runtime_stride);memcpy(mapped,&runtime,sizeof(runtime));seed.handle->Unmap(0,nullptr);
    }
    // A reused queue-ordered packet avoids allocation/Finish after the caller
    // has borrowed authoritative prepared input Resource references.
    auto& dispatch=dispatch_;
    renderer.RetainUntilFinish([source,count_value=count?*count:seed,commands,constant_value=constants?*constants:Resource{},output_count,seed,
        dispatch,index_input=index_source,index_output=decode?*decode->output:Resource{},index_pipeline=index_pipeline_,
        root=root_,pipeline=pipeline_,builder=dispatch_pipeline_,signature=dispatch_signature_](u64){});
    const auto source_state=source.State(0),count_state=count?count->State(0):D3D12_RESOURCE_STATE_COMMON;
    const auto index_state=index_source.State(0);
    auto* list=renderer.RecordingList();
    if(!(source.State(0)&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if(count&&!(count->State(0)&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))count->Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    if(decode&&!(index_source.State(0)&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
        index_source.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    for(auto* output:outputs)if(output)output->Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    dispatch.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    list->SetComputeRootSignature(root_.Get());list->SetPipelineState(dispatch_pipeline_.Get());
    list->SetComputeRootShaderResourceView(0,source.handle->GetGPUVirtualAddress()+offset);
    list->SetComputeRootShaderResourceView(1,count?count->handle->GetGPUVirtualAddress()+count_offset:seed.handle->GetGPUVirtualAddress());
    list->SetComputeRootShaderResourceView(2,seed.handle->GetGPUVirtualAddress());
    for(u32 i=0;i<3;++i)list->SetComputeRootUnorderedAccessView(3+i,(outputs[i]?outputs[i]:&commands)->handle->GetGPUVirtualAddress());
    list->SetComputeRootUnorderedAccessView(6,dispatch.handle->GetGPUVirtualAddress());
    list->SetComputeRootShaderResourceView(8,index_source.handle->GetGPUVirtualAddress()+(decode?(decode->offset&~u64(3)):0));
    list->SetComputeRootUnorderedAccessView(9,(decode?decode->output:&commands)->handle->GetGPUVirtualAddress());
    const u32 parameters[]{plan.count,plan.source_stride,plan.indexed?1u:0u,plan.runtime_stride,
        plan.command_stride,u32(base),u32(base>>32),count?1u:0u,
        0,decode?decode->available:0,decode?decode->swap_mode:0,decode?1u:0u,
        decode?u32(decode->offset%4):0,u32(index_base),u32(index_base>>32),decode?std::max(1u,decode->available)*4:0,
        decode?(std::max(1u,decode->available)+127)/128:0,decode&&decode->restart?1u:0u,decode?decode->marker:0,0};
    list->SetComputeRoot32BitConstants(7,20,parameters,0);list->Dispatch(1,1,1);
    output_count.UavBarrier(list);dispatch.UavBarrier(list);
    dispatch.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    list->SetPipelineState(pipeline_.Get());
    // u3 is unused by the packing entry; do not leave its root UAV pointing
    // at a resource which is now an indirect command input.
    list->SetComputeRootUnorderedAccessView(6,commands.handle->GetGPUVirtualAddress());
    list->ExecuteIndirect(dispatch_signature_.Get(),1,dispatch.handle.Get(),0,nullptr,0);
    if(decode){
        list->SetPipelineState(index_pipeline_.Get());
        list->ExecuteIndirect(dispatch_signature_.Get(),1,dispatch.handle.Get(),16,nullptr,0);
    }
    for(auto* output:outputs)if(output)output->UavBarrier(list);
    // These can also be prepared guest shader buffers. The packing prepass
    // must return their authoritative state to the subsequent guest draw.
    if(source.State(0)!=source_state)source.Transition(list,source_state);
    if(count&&count!=&source&&count->State(0)!=count_state)count->Transition(list,count_state);
    if(decode && &index_source!=&source && &index_source!=count&&index_source.State(0)!=index_state)
        index_source.Transition(list,index_state);
    if(decode)decode->output->Transition(list,D3D12_RESOURCE_STATE_INDEX_BUFFER);
    commands.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    output_count.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    if(constants)constants->Transition(list,D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
}
} // namespace NativeDX12
