#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "rasterizer.h"
#include <d3d12sdklayers.h>
#include <cstdlib>
#include <cstdio>
#include "shader_pipeline.h"
#include "guest_graphics_state.h"
#include <cmath>
#include <set>
#include <algorithm>
namespace NativeDX12 {
void Rasterizer::ValidateDebugMessages() {
    if(!std::getenv("SHADPS4_NATIVE_D3D12_VALIDATION"))return;
    ComPtr<ID3D12InfoQueue> info;
    if(FAILED(DX12_API_CALL(device_.As(&info))))return;
    bool failed=false;
    for(UINT64 i=0;i<DX12_API_CALL(info->GetNumStoredMessages());++i){
        SIZE_T size{};if(FAILED(DX12_API_CALL(info->GetMessage(i,nullptr,&size))))continue;
        std::vector<std::byte> data(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        if(SUCCEEDED(DX12_API_CALL(info->GetMessage(i,message,&size)))&&message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR){
            fprintf(stderr,"Native DX12 validation: %s\n",message->pDescription);failed=true;
        }
    }
    DX12_API_CALL(info->ClearStoredMessages());
    if(failed){fflush(stderr);throw std::runtime_error("D3D12 debug validation rejected a guest command");}
}
void Rasterizer::ExecuteGraphics(const ShaderPipeline& pipeline,ResourceBindings& bindings,
    const GuestGraphicsState& state,std::span<const ColorAttachment> colors,const DepthAttachment* depth,
    std::span<const D3D12_VIEWPORT> viewports,std::span<const D3D12_RECT> scissors,
    std::span<const float,4> blend,const DrawArguments& draw,
    std::span<const VertexBinding> vertices,const IndexBinding* indices,bool wait) {
    std::lock_guard lock(execution_mutex_);
    if(pipeline.IsCompute())throw std::invalid_argument("Native graphics submission has compute PSO");
    if(state.discard||!draw.count||!draw.instances)return;
    if(state.topology==D3D_PRIMITIVE_TOPOLOGY_UNDEFINED||colors.size()!=pipeline.TargetFormats().size()||
       viewports.empty()||viewports.size()>16||scissors.size()!=viewports.size()||vertices.size()>32)
        throw std::invalid_argument("Native draw topology/attachment/viewport/input count");
    for(const auto& v:viewports)if(!std::isfinite(v.TopLeftX)||!std::isfinite(v.TopLeftY)||
        !std::isfinite(v.Width)||!std::isfinite(v.Height)||!std::isfinite(v.MinDepth)||!std::isfinite(v.MaxDepth)||
        v.Width<=0||v.Height<=0||v.MinDepth<0||v.MaxDepth>1||v.MinDepth>v.MaxDepth)
        throw std::invalid_argument("Native draw viewport bounds");
    for(const auto& r:scissors)if(r.right<r.left||r.bottom<r.top)throw std::invalid_argument("Native draw inverted scissor");
    std::set<ID3D12Resource*> attachments;
    for(size_t i=0;i<colors.size();++i) {
        const auto& color=colors[i];const auto expected=pipeline.TargetFormats()[i];
        if(!color.resource){if(expected!=DXGI_FORMAT_UNKNOWN)throw std::invalid_argument("Native draw missing color target");continue;}
        if(!color.resource->handle)throw std::invalid_argument("Null native draw color resource");
        const auto d=DX12_API_CALL(color.resource->handle->GetDesc());
        if(bindings.References(color.resource->handle.Get()))throw std::runtime_error("Native draw attachment also bound as shader resource needs alias handling");
        const auto format=color.view.Format==DXGI_FORMAT_UNKNOWN?d.Format:color.view.Format;
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
           d.SampleDesc.Count!=pipeline.SampleCount()||format!=expected||!attachments.insert(color.resource->handle.Get()).second)
            throw std::invalid_argument("Native draw color target format/sample/usage/alias mismatch");
    }
    if(depth&&depth->resource) {
        if(!depth->resource->handle)throw std::invalid_argument("Null native draw depth resource");
        if(bindings.References(depth->resource->handle.Get())&&
           (!depth->read_only_depth||(!depth->read_only_stencil&&depth->view.Format==DXGI_FORMAT_D32_FLOAT_S8X24_UINT)))
            throw std::runtime_error("Native writable depth/stencil feedback is not a valid D3D12 attachment state");
        auto d=DX12_API_CALL(depth->resource->handle->GetDesc());auto format=depth->view.Format==DXGI_FORMAT_UNKNOWN?d.Format:depth->view.Format;
        if(!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)||d.SampleDesc.Count!=pipeline.SampleCount()||format!=pipeline.DepthFormat()||
           !attachments.insert(depth->resource->handle.Get()).second)throw std::invalid_argument("Native draw depth target mismatch");
    }else if(pipeline.DepthFormat()!=DXGI_FORMAT_UNKNOWN)throw std::invalid_argument("Native draw missing depth target");
    std::vector<D3D12_VERTEX_BUFFER_VIEW> vertex_views;
    for(const auto& b:vertices) {
        if(!b.resource||!b.resource->handle||!b.bytes||!b.stride)throw std::invalid_argument("Invalid native vertex buffer");
        const auto d=DX12_API_CALL(b.resource->handle->GetDesc());
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||b.offset>d.Width||b.bytes>d.Width-b.offset||b.stride>2048)
            throw std::out_of_range("Native vertex buffer bounds/stride");
        vertex_views.push_back({DX12_API_CALL(b.resource->handle->GetGPUVirtualAddress())+b.offset,b.bytes,b.stride});
    }
    D3D12_INDEX_BUFFER_VIEW index_view{};
    if(indices) {
        if(!indices->resource||!indices->resource->handle||(indices->format!=DXGI_FORMAT_R16_UINT&&indices->format!=DXGI_FORMAT_R32_UINT))
            throw std::invalid_argument("Invalid native index buffer");
        const auto d=DX12_API_CALL(indices->resource->handle->GetDesc());const UINT element=indices->format==DXGI_FORMAT_R16_UINT?2:4;
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||indices->offset%element||indices->offset>d.Width||
           indices->bytes>d.Width-indices->offset||(u64(draw.first)+draw.count)*element>indices->bytes)
            throw std::out_of_range("Native indexed draw bounds");
        index_view={DX12_API_CALL(indices->resource->handle->GetGPUVirtualAddress())+indices->offset,indices->bytes,indices->format};
        bindings.Retain(indices->resource->handle.Get());bindings.RetainAllocation(indices->allocation);
    }
    if(colors.size()>8)throw std::invalid_argument("Native draw exceeds eight color targets");
    if(!color_descriptors_)color_descriptors_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,8,false);
    if(depth&&depth->resource&&!depth_descriptors_)depth_descriptors_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
    auto& rtvs=*color_descriptors_;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles;D3D12_CPU_DESCRIPTOR_HANDLE depth_handle{};
    for(const auto& color:colors) {
        auto handle=rtvs.Cpu(UINT(handles.size()));handles.push_back(handle);
        if(!color.resource) {
            D3D12_RENDER_TARGET_VIEW_DESC null_view{};null_view.Format=DXGI_FORMAT_R8G8B8A8_UNORM;null_view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
            DX12_API_CALL(device_->CreateRenderTargetView(nullptr,&null_view,handle));
        }else DX12_API_CALL(device_->CreateRenderTargetView(color.resource->handle.Get(),color.view.Format==DXGI_FORMAT_UNKNOWN?nullptr:&color.view,handle));
    }
    if(depth&&depth->resource) {
        depth_handle=depth_descriptors_->Cpu(0);
        DX12_API_CALL(device_->CreateDepthStencilView(depth->resource->handle.Get(),depth->view.Format==DXGI_FORMAT_UNKNOWN?nullptr:&depth->view,depth_handle));
    }
    try {
        auto* list=RecordingList();BarrierBatch dependencies;
        for(const auto& color:colors)if(color.resource)dependencies.Transition(*color.resource,D3D12_RESOURCE_STATE_RENDER_TARGET);
        if(depth&&depth->resource) {
            const bool sampled=bindings.References(depth->resource->handle.Get());
            dependencies.Transition(*depth->resource,sampled?
                D3D12_RESOURCE_STATES(D3D12_RESOURCE_STATE_DEPTH_READ|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE):
                (depth->read_only_depth&&depth->read_only_stencil?D3D12_RESOURCE_STATE_DEPTH_READ:D3D12_RESOURCE_STATE_DEPTH_WRITE));
        }
        // Read usages combine. An IA buffer can also be fetched by a shader;
        // switching it to INDEX_BUFFER alone would invalidate that SRV state.
        std::map<Resource*,D3D12_RESOURCE_STATES> inputs;
        for(const auto& v:vertices)inputs[v.resource]|=D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if(indices)inputs[indices->resource]|=D3D12_RESOURCE_STATE_INDEX_BUFFER;
        for(const auto& [resource,input_state]:inputs){
            const auto shader_read=bindings.References(resource->handle.Get())?
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COMMON;
            dependencies.Transition(*resource,D3D12_RESOURCE_STATES(input_state|shader_read));
        }
        dependencies.Flush(list);pipeline.Bind(list,bindings);
        DX12_API_CALL(list->RSSetViewports(UINT(viewports.size()),viewports.data()));DX12_API_CALL(list->RSSetScissorRects(UINT(scissors.size()),scissors.data()));
        ComPtr<ID3D12GraphicsCommandList4> render_list;D3D12_FEATURE_DATA_D3D12_OPTIONS5 render_options{};
        const bool render_pass=SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&render_list)))&&
            SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&render_options,sizeof(render_options)))&&
            render_options.RenderPassesTier!=D3D12_RENDER_PASS_TIER_0;
        std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> render_targets;
        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC render_depth{};
        if(render_pass){
            render_targets.reserve(handles.size());for(const auto handle:handles){D3D12_RENDER_PASS_RENDER_TARGET_DESC target{};
                target.cpuDescriptor=handle;target.BeginningAccess.Type=D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                target.EndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;render_targets.push_back(target);}
            if(depth&&depth->resource){render_depth.cpuDescriptor=depth_handle;
                render_depth.DepthBeginningAccess.Type=depth->clear_depth?D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                render_depth.DepthBeginningAccess.Clear.ClearValue.Format=depth->view.Format;
                render_depth.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth=depth->clear_value;
                render_depth.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil=depth->stencil_value;
                render_depth.DepthEndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                render_depth.StencilBeginningAccess.Type=depth->clear_stencil?D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                render_depth.StencilBeginningAccess.Clear.ClearValue=render_depth.DepthBeginningAccess.Clear.ClearValue;
                render_depth.StencilEndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;}
            DX12_API_CALL(render_list->BeginRenderPass(UINT(render_targets.size()),render_targets.data(),
                depth&&depth->resource?&render_depth:nullptr,D3D12_RENDER_PASS_FLAG_NONE));
        }else{
            if(depth&&depth->resource&&(depth->clear_depth||depth->clear_stencil))
                DX12_API_CALL(list->ClearDepthStencilView(depth_handle,D3D12_CLEAR_FLAGS((depth->clear_depth?D3D12_CLEAR_FLAG_DEPTH:0)|
                    (depth->clear_stencil?D3D12_CLEAR_FLAG_STENCIL:0)),depth->clear_value,depth->stencil_value,0,nullptr));
            DX12_API_CALL(list->OMSetRenderTargets(UINT(handles.size()),handles.data(),FALSE,depth_handle.ptr?&depth_handle:nullptr));
        }
        std::array<float,4> native_blend{};std::copy(blend.begin(),blend.end(),native_blend.begin());
        if(state.blend_constant_alpha_rgb)native_blend[0]=native_blend[1]=native_blend[2]=native_blend[3];
        DX12_API_CALL(list->OMSetBlendFactor(native_blend.data()));
        if(state.separate_stencil_faces){ComPtr<ID3D12GraphicsCommandList8> extended;
            Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&extended))));
            DX12_API_CALL(extended->OMSetFrontAndBackStencilRef(state.stencil_reference,state.stencil_reference_back));}
        else DX12_API_CALL(list->OMSetStencilRef(state.stencil_reference));
        DX12_API_CALL(list->IASetPrimitiveTopology(state.topology));
        if(state.dynamic_depth_bias){
            ComPtr<ID3D12GraphicsCommandList9> extended;Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&extended))));
            DX12_API_CALL(extended->RSSetDepthBias(state.depth_bias,state.depth_bias_clamp,state.slope_depth_bias));
        }
        if(state.depth_bounds) {
            if(!depth||!depth->resource)throw std::runtime_error("Native depth bounds attachment missing");
            ComPtr<ID3D12GraphicsCommandList1> extended;Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&extended))));
            DX12_API_CALL(extended->OMSetDepthBounds(state.depth_bounds_min,state.depth_bounds_max));
        }
        if(!vertex_views.empty())DX12_API_CALL(list->IASetVertexBuffers(0,UINT(vertex_views.size()),vertex_views.data()));
        if(indices){DX12_API_CALL(list->IASetIndexBuffer(&index_view));DX12_API_CALL(list->DrawIndexedInstanced(draw.count,draw.instances,draw.first,draw.base_vertex,draw.first_instance));}
        else DX12_API_CALL(list->DrawInstanced(draw.count,draw.instances,draw.first,draw.first_instance));
        if(render_pass)DX12_API_CALL(render_list->EndRenderPass());
        // Keep unrelated UAVs concurrent.  Microsoft recommends the minimum
        // necessary UAV barriers; guest page ownership handles later CPU use.
        for(const auto base:dirty_mappings_) {
            auto& resource=mappings_.at(base).gpu;
            if(bindings.References(resource.handle.Get()))resource.UavBarrier(list);
        }
        if(wait){Finish();ValidateDebugMessages();}
    }catch(...) {try{Finish();}catch(...){}throw;}
}
}
