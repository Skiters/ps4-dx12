// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_index_expand.h"
#include "guest_index_expand_shader.h"
#include "guest_index_restart_shader.h"
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
    RecordInternal(renderer,plan,source,offset,available,element,output,false,0);
}
void GuestIndexExpandPass::RecordGenerated(Rasterizer& renderer,const GuestIndexExpansion& plan,u32 first,Resource& output){
    RecordInternal(renderer,plan,nullptr,0,0,4,output,true,first);
}
void GuestIndexExpandPass::RecordInternal(Rasterizer& renderer,const GuestIndexExpansion& plan,
    Resource* source,u64 offset,u32 available,u32 element,Resource& output,bool generated,u32 first){
    std::lock_guard lock(renderer.ExecutionMutex());
    const AmdGpu::PrimitiveType types[]{AmdGpu::PrimitiveType::TriangleFan,AmdGpu::PrimitiveType::LineLoop,
        AmdGpu::PrimitiveType::QuadStrip,AmdGpu::PrimitiveType::TriangleStrip,AmdGpu::PrimitiveType::TriangleStrip};
    if(plan.mode>4||plan.group_width||(element!=2&&element!=4)||available>plan.input_count||!plan.output_count||
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
    const u32 params[]{plan.mode,plan.input_count,plan.output_count,available,generated?0:element,u32(offset%4),columns*128,first};
    list->SetComputeRoot32BitConstants(2,8,params,0);list->Dispatch(columns,rows,1);
    output.UavBarrier(list);output.Transition(list,D3D12_RESOURCE_STATE_INDEX_BUFFER);
    if(input.State(0)!=previous)input.Transition(list,previous);
}
void GuestIndexExpandPass::RecordRestart(Rasterizer& renderer,const GuestIndexExpansion& plan,
    Resource* source,u64 offset,u32 available,u32 element,u32 marker,const Rasterizer::DrawArguments& draw,
    Resource& output,Resource& arguments,u32 swap_mode){
    std::lock_guard lock(renderer.ExecutionMutex());
    const AmdGpu::PrimitiveType types[]{AmdGpu::PrimitiveType::TriangleFan,AmdGpu::PrimitiveType::LineLoop,
        AmdGpu::PrimitiveType::QuadStrip,AmdGpu::PrimitiveType::TriangleStrip,AmdGpu::PrimitiveType::TriangleStrip};
    const bool valid_plan=plan.mode<=4?
        (!plan.group_width&&GuestIndexExpansion::Make(types[plan.mode],plan.input_count).output_count==plan.output_count):
        plan.mode==5?(!plan.group_width&&u64(plan.output_count)==(plan.input_count>1?u64(plan.input_count-1)*2:0)):
        plan.mode==6&&plan.group_width&&plan.group_width<=32&&plan.output_count==plan.input_count/plan.group_width*plan.group_width;
    if(!valid_plan||swap_mode>3||(element!=2&&element!=4)||available>plan.input_count||!plan.output_count||
       plan.input_count>UINT32_MAX/8||
       (!source&&(available||offset))||offset%element)
        throw std::invalid_argument("Invalid segmented GPU index expansion plan/source");
    auto& input=source?*source:zero_;
    if(!input.handle||!output.handle||!arguments.handle||input.handle==output.handle||
       input.handle==arguments.handle||output.handle==arguments.handle)
        throw std::invalid_argument("Segmented index expansion needs distinct source/output/arguments");
    const auto in=input.handle->GetDesc(),out=output.handle->GetDesc(),args=arguments.handle->GetDesc();
    const u64 end=offset+u64(available)*element;
    if(in.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||(in.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||
       (offset%4)+u64(available)*element>UINT32_MAX||end<offset||end>UINT64_MAX-3||((end+3)&~u64(3))>in.Width||
       out.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||out.Width<u64(plan.output_count)*4||
       !(out.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)||
       args.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||args.Width<20||!(args.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::out_of_range("Segmented index expansion resource extent/usage");
    ComPtr<ID3D12Device> owner,device;Check(root_->GetDevice(IID_PPV_ARGS(&device)));
    for(auto* resource:{&input,&output,&arguments}){Check(resource->handle->GetDevice(IID_PPV_ARGS(&owner)));
        if(owner.Get()!=device.Get())throw std::invalid_argument("Segmented index expansion device mismatch");}
    if(!restart_root_){
        D3D12_ROOT_PARAMETER params[6]{};
        for(UINT i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[i].Descriptor={i,0};}
        for(UINT i=0;i<3;++i){params[i+2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[i+2].Descriptor={i,0};}
        params[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[5].Constants={0,0,14};
        const D3D12_ROOT_SIGNATURE_DESC desc{6,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;Check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors));
        ComPtr<ID3D12RootSignature> root;Check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)));
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc_pipeline{};desc_pipeline.pRootSignature=root.Get();
        desc_pipeline.CS={NativeGuestIndexRestart,sizeof(NativeGuestIndexRestart)};
        ComPtr<ID3D12PipelineState> pipeline;Check(device->CreateComputePipelineState(&desc_pipeline,IID_PPV_ARGS(&pipeline)));
        restart_root_=std::move(root);restart_pipeline_=std::move(pipeline);
    }
    auto a=renderer.AcquireTransientBuffer(u64(plan.input_count)*8),b=renderer.AcquireTransientBuffer(u64(plan.input_count)*8);
    renderer.RetainUntilFinish([input,output,arguments,a,b,root=restart_root_,pipeline=restart_pipeline_](u64){});
    auto* list=renderer.RecordingList();const auto previous=input.State(0);
    if(!(previous&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))input.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    output.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);arguments.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    const u32 groups=(plan.input_count+127)/128,rows=(groups+65534)/65535,columns=(groups+rows-1)/rows;
    const auto dispatch=[&](UINT phase,UINT step,Resource& src,Resource& dst){
        src.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);dst.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        list->SetComputeRootSignature(restart_root_.Get());list->SetPipelineState(restart_pipeline_.Get());
        list->SetComputeRootShaderResourceView(0,input.handle->GetGPUVirtualAddress()+(offset&~u64(3)));
        list->SetComputeRootShaderResourceView(1,src.handle->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(2,dst.handle->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(3,output.handle->GetGPUVirtualAddress());
        list->SetComputeRootUnorderedAccessView(4,arguments.handle->GetGPUVirtualAddress());
        const u32 params[]{phase,step,plan.mode,plan.input_count,available,element,u32(offset%4),columns*128,
            marker,draw.instances,u32(draw.base_vertex),draw.first_instance,plan.group_width,swap_mode};
        list->SetComputeRoot32BitConstants(5,14,params,0);list->Dispatch(columns,rows,1);dst.UavBarrier(list);
    };
    dispatch(0,0,b,a);
    Resource* current=&a;Resource* next=&b;
    const auto run=[&](UINT phase,UINT step){dispatch(phase,step,*current,*next);std::swap(current,next);};
    // First scan propagates segment starts; second scan compacts emitted index
    // counts. Every dispatch reads an immutable prior level (no cross-group race).
    for(UINT step=1;step<plan.input_count;step<<=1)run(1,step);
    run(2,0);
    for(UINT step=1;step<plan.input_count;step<<=1)run(3,step);
    dispatch(4,0,*current,*next);
    output.UavBarrier(list);arguments.UavBarrier(list);
    output.Transition(list,D3D12_RESOURCE_STATE_INDEX_BUFFER);arguments.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    if(input.State(0)!=previous)input.Transition(list,previous);
}

Resource GuestIndexExpandPass::InstanceArguments(Rasterizer& renderer,Resource& arguments,const Rasterizer::DrawArguments& draw){
    std::lock_guard lock(renderer.ExecutionMutex());
    if(!arguments.handle||arguments.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
       arguments.handle->GetDesc().Width<20||draw.instances!=1||draw.first)
        throw std::invalid_argument("Captured restart requires one complete normalized indexed instance");
    ComPtr<ID3D12Device> device,owner;Check(root_->GetDevice(IID_PPV_ARGS(&device)));
    Check(arguments.handle->GetDevice(IID_PPV_ARGS(&owner)));
    if(owner.Get()!=device.Get())throw std::invalid_argument("Captured restart arguments belong to another device");
    auto output=renderer.AcquireTransientBuffer(20);
    auto tail=Buffer(device.Get(),16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    const u32 words[]{1,0,u32(draw.base_vertex),draw.first_instance};void* bytes{};D3D12_RANGE none{};
    Check(tail.handle->Map(0,&none,&bytes));memcpy(bytes,words,sizeof(words));tail.handle->Unmap(0,nullptr);
    renderer.RetainUntilFinish([arguments,output,tail](u64){});
    auto* list=renderer.RecordingList();const auto previous=arguments.State(0);
    arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(output.handle.Get(),0,arguments.handle.Get(),0,4);
    list->CopyBufferRegion(output.handle.Get(),4,tail.handle.Get(),0,16);
    output.Transition(list,D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);arguments.Transition(list,previous);
    return output;
}

}
