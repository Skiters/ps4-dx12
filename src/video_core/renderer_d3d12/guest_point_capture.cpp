// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_point_capture.h"
#include "guest_point_capture_clear.h"
#include "rasterizer.h"
#include "resource_bindings.h"
#include "diagnostics.h"
#include <algorithm>
#include <limits>
namespace NativeDX12 {
namespace {
bool Idle(const std::shared_ptr<Resource>& resource){
    return resource.use_count()==1&&(!resource->allocation||resource->allocation.use_count()==1);
}
bool HasBinding(const ResourceBindings& table,D3D12_DESCRIPTOR_RANGE_TYPE type,
    u32 binding=Shader::NativePointCapture::Binding){
    return std::any_of(table.Layout().Bindings().begin(),table.Layout().Bindings().end(),[&](const auto& b){
        return b.space==Shader::NativePointCapture::DescriptorSet&&b.reg==binding&&b.type==type;
    });
}
}
NativePointCapturePass::NativePointCapturePass(ID3D12Device* device){
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameters[0].Descriptor={0,Shader::NativePointCapture::DescriptorSet};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants={0,Shader::NativePointCapture::DescriptorSet,2};
    D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    ComPtr<ID3DBlob> blob,errors;
    Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
    Check(DX12_API_CALL(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&clear_root_))));
    D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=clear_root_.Get();
    state.CS={NativePointCaptureClear,sizeof(NativePointCaptureClear)};
    Check(DX12_API_CALL(device->CreateComputePipelineState(&state,IID_PPV_ARGS(&clear_pipeline_))));
}
std::shared_ptr<Resource> NativePointCapturePass::Acquire(Rasterizer& rasterizer,u64 bytes){
    rasterizer.CollectRetirements();
    std::shared_ptr<Resource>* best=nullptr;
    for(auto& candidate:buffers_)if(Idle(candidate)&&candidate->handle->GetDesc().Width>=bytes&&
       (!best||candidate->handle->GetDesc().Width<(*best)->handle->GetDesc().Width))best=&candidate;
    if(best){Diagnostics::Count("point-capture.buffer-reuse");return *best;}
    // Reclaim only idle leases; a queued root UAV or SRV must keep its arena.
    std::erase_if(buffers_,Idle);
    auto buffer=std::make_shared<Resource>(rasterizer.AcquireTransientBuffer(bytes));
    buffers_.push_back(buffer);Diagnostics::Count("point-capture.buffer-new");return buffer;
}
u32 NativePointCapturePass::PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY topology,u32 vertices){
    switch(topology){
    case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:return vertices;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST:return vertices/2;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:return vertices>1?vertices-1:0;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:return vertices/3;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:return vertices>2?vertices-2:0;
    case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:return vertices/4;
    case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:return vertices>3?vertices-3:0;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:return vertices/6;
    case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:return vertices>4?(vertices-4)/2:0;
    // RectList/QuadList use internal fixed tessellation even in EsGs mode.
    // Preserve their previous conservative reservation. A tighter formula
    // needs a separate proof of the auxiliary stages' GS primitive ordinal.
    case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
    case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:return vertices;
    default:throw std::invalid_argument("Unsupported point capture input topology");
    }
}
GuestGraphicsState NativePointCapturePass::ProducerState(const GuestGraphicsState& original){
    auto state=original;state.discard=false;state.rasterizer_discard=true;state.depth_bounds=false;
    state.dynamic_depth_bias=false;state.separate_stencil_faces=false;
    auto& pipeline=state.pipeline;pipeline.StreamOutput={};pipeline.StreamOutput.RasterizedStream=D3D12_SO_NO_RASTERIZED_STREAM;
    pipeline.NumRenderTargets=0;std::fill(std::begin(pipeline.RTVFormats),std::end(pipeline.RTVFormats),DXGI_FORMAT_UNKNOWN);
    pipeline.DSVFormat=DXGI_FORMAT_UNKNOWN;pipeline.DepthStencilState.DepthEnable=FALSE;
    pipeline.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;pipeline.DepthStencilState.StencilEnable=FALSE;
    pipeline.BlendState.AlphaToCoverageEnable=FALSE;pipeline.SampleDesc={1,0};
    return state;
}
GuestGraphicsState NativePointCapturePass::StreamConsumerState(const GuestGraphicsState& original,u32 vertices){
    auto state=ProducerState(original);
    // Serialized input duplicates strip/patch vertices per complete primitive;
    // feeding the records back as strips or patches changes GS invocations.
    switch(vertices){
    case 1:state.topology=D3D_PRIMITIVE_TOPOLOGY_POINTLIST;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;break;
    case 2:state.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;break;
    case 3:state.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;break;
    case 4:state.topology=D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;break;
    case 6:state.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;break;
    default:throw std::invalid_argument("Invalid original primitive width in buffered GS consumer");
    }
    state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;return state;
}
void NativePointCapturePass::BindProducer(ResourceBindings& table,Resource& buffer,u64 offset,u64 bytes){
    if(!HasBinding(table,D3D12_DESCRIPTOR_RANGE_TYPE_UAV))return; // No original emissions: no UAV survives DCE.
    if(offset%4||bytes%4||offset>buffer.handle->GetDesc().Width||bytes>buffer.handle->GetDesc().Width-offset)
        throw std::out_of_range("Native point producer view exceeds its capture allocation");
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_TYPELESS;
    view.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;view.Buffer.FirstElement=offset/4;
    view.Buffer.NumElements=UINT(bytes/4);view.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
    table.RetainAllocation(buffer.allocation);table.Uav(0,Shader::NativePointCapture::DescriptorSet,buffer.handle.Get(),view);
}
void NativePointCapturePass::BindReplay(ResourceBindings& table,Resource& buffer,u64 bytes,u64 offset){
    if(!HasBinding(table,D3D12_DESCRIPTOR_RANGE_TYPE_SRV))throw std::logic_error("Point replay lost capture SRV");
    if(!buffer.handle)throw std::invalid_argument("Missing native point replay buffer");
    const auto desc=buffer.handle->GetDesc();
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||!bytes||offset%Shader::NativePointCapture::RecordBytes||bytes%Shader::NativePointCapture::RecordBytes||
       bytes>UINT32_MAX||offset>desc.Width||bytes>desc.Width-offset)
        throw std::out_of_range("Native point replay view exceeds complete capture records");
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_UINT;view.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Buffer.FirstElement=offset/4;view.Buffer.NumElements=UINT(bytes/4);
    table.RetainAllocation(buffer.allocation);table.Srv(0,Shader::NativePointCapture::DescriptorSet,buffer.handle.Get(),view);
}
void NativePointCapturePass::BindInputStream(ResourceBindings& table,Resource& buffer,u64 offset,u64 bytes){
    constexpr u32 binding=Shader::NativePointCapture::StreamBinding;
    if(!HasBinding(table,D3D12_DESCRIPTOR_RANGE_TYPE_SRV,binding))
        throw std::logic_error("Buffered geometry lost its input stream SRV");
    if(!buffer.handle)throw std::invalid_argument("Missing buffered geometry input");
    const auto desc=buffer.handle->GetDesc();
    if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||offset%4||!bytes||bytes%4||
       offset>desc.Width||bytes>desc.Width-offset||bytes>std::numeric_limits<u32>::max())
        throw std::out_of_range("Buffered geometry input view exceeds its allocation");
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_UINT;
    view.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Buffer.FirstElement=offset/4;view.Buffer.NumElements=UINT(bytes/4);
    table.RetainAllocation(buffer.allocation);
    table.Srv(binding,Shader::NativePointCapture::DescriptorSet,buffer.handle.Get(),view);
}
void NativePointCapturePass::Execute(Rasterizer& rasterizer,const Shader::NativePointCapture::Plan& plan,
    const DrawArguments& original,D3D_PRIMITIVE_TOPOLOGY topology,
    const std::function<void(Resource&,const DrawArguments&,u64,u64)>& producer,
    const std::function<void(Resource&,const DrawArguments&)>& replay){
    if(!original.count||!original.instances)return;
    const auto checked=Shader::NativePointCapture::Plan::Make(plan.primitive_bound,plan.invocations,plan.emissions,plan.instances);
    if(checked.records!=plan.records||checked.bytes!=plan.bytes||plan.instances!=original.instances||
       plan.primitive_bound<std::max(1u,PrimitiveBound(topology,original.count))||
       u64(original.first_instance)+original.instances-1>std::numeric_limits<u32>::max())
        throw std::invalid_argument("Invalid native point capture plan or instance range");
    auto buffer=Acquire(rasterizer,plan.bytes);
    // A BeforeBind callback can Finish during guest ownership resolution.
    // Retain here for clear; descriptor allocation leases cover later passes.
    rasterizer.RetainUntilFinish([buffer](u64){});
    try{
        auto* list=rasterizer.RecordingList();buffer->Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        DX12_API_CALL(list->SetPipelineState(clear_pipeline_.Get()));DX12_API_CALL(list->SetComputeRootSignature(clear_root_.Get()));
        DX12_API_CALL(list->SetComputeRootUnorderedAccessView(0,buffer->handle->GetGPUVirtualAddress()));
        const u32 clear[]{u32(plan.records),u32(Shader::NativePointCapture::RecordBytes)};
        DX12_API_CALL(list->SetComputeRoot32BitConstants(1,2,clear,0));
        DX12_API_CALL(list->Dispatch(UINT((plan.records+255)/256),1,1));buffer->UavBarrier(list);
        const auto instance_bytes=plan.bytes/plan.instances;
        for(u32 instance=0;instance<original.instances;++instance){
            auto draw=original;draw.instances=1;draw.first_instance+=instance;
            producer(*buffer,draw,u64(instance)*instance_bytes,instance_bytes);
        }
        // Barrier orders every producer before replay. No CPU readback/Finish
        // is added to the normal path; all passes use the same direct queue.
        buffer->UavBarrier(rasterizer.RecordingList());
        buffer->Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATES(
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
        replay(*buffer,DrawArguments{u32(plan.records),1,0,0,0});
        rasterizer.RetainUntilFinish([buffer](u64){});
        Diagnostics::Count("point-capture.records",plan.records);
    }catch(...){try{rasterizer.Finish();}catch(...){}throw;}
}
}
