#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "rasterizer.h"
#include <d3d12sdklayers.h>
#include <cstdlib>
#include <cstdio>
#include "shader_pipeline.h"
#include "guest_graphics_state.h"
#include "guest_indirect_arguments.h"
#include <cmath>
#include <set>
#include <algorithm>
namespace NativeDX12 {
namespace {
struct ColorSubresources {UINT mip,first,count;};
ColorSubresources ColorRange(const D3D12_RESOURCE_DESC& resource,const D3D12_RENDER_TARGET_VIEW_DESC& view){
    ColorSubresources range{0,0,resource.DepthOrArraySize};
    const bool multisampled=resource.SampleDesc.Count>1;
    switch(view.ViewDimension){
    case D3D12_RTV_DIMENSION_UNKNOWN:break;
    case D3D12_RTV_DIMENSION_TEXTURE2D:
        if(multisampled||resource.DepthOrArraySize!=1||view.Texture2D.PlaneSlice)
            throw std::invalid_argument("Native color view dimension or plane mismatch");
        range={view.Texture2D.MipSlice,0,1};break;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
        if(multisampled||view.Texture2DArray.PlaneSlice)
            throw std::invalid_argument("Native color array view sample or plane mismatch");
        range={view.Texture2DArray.MipSlice,view.Texture2DArray.FirstArraySlice,view.Texture2DArray.ArraySize};break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS:
        if(!multisampled||resource.DepthOrArraySize!=1)
            throw std::invalid_argument("Native multisample color view dimension mismatch");
        range={0,0,1};break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
        if(!multisampled)throw std::invalid_argument("Native multisample color array view sample mismatch");
        range={0,view.Texture2DMSArray.FirstArraySlice,view.Texture2DMSArray.ArraySize};break;
    default:throw std::invalid_argument("Native color view is not two dimensional");
    }
    if(range.mip>=resource.MipLevels||!range.count||range.first>=resource.DepthOrArraySize||
       range.count>resource.DepthOrArraySize-range.first)
        throw std::out_of_range("Native color view exceeds resource subresources");
    return range;
}
D3D12_DEPTH_STENCIL_VIEW_DESC DepthView(const Resource& resource,D3D12_DEPTH_STENCIL_VIEW_DESC view,
    DXGI_FORMAT format,bool read_only_depth,bool read_only_stencil){
    const auto desc=resource.handle->GetDesc();
    if(view.Format==DXGI_FORMAT_UNKNOWN){
        view.Format=format;
        if(desc.SampleDesc.Count>1){
            view.ViewDimension=desc.DepthOrArraySize>1?D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:D3D12_DSV_DIMENSION_TEXTURE2DMS;
            if(desc.DepthOrArraySize>1)view.Texture2DMSArray={0,desc.DepthOrArraySize};
        }else{
            view.ViewDimension=desc.DepthOrArraySize>1?D3D12_DSV_DIMENSION_TEXTURE2DARRAY:D3D12_DSV_DIMENSION_TEXTURE2D;
            if(desc.DepthOrArraySize>1)view.Texture2DArray={0,0,desc.DepthOrArraySize};
        }
    }
    const bool has_stencil=resource.SubresourceCount()>UINT(desc.MipLevels)*UINT(desc.DepthOrArraySize);
    view.Flags=D3D12_DSV_FLAGS((read_only_depth?D3D12_DSV_FLAG_READ_ONLY_DEPTH:0)|
        (read_only_stencil&&has_stencil?D3D12_DSV_FLAG_READ_ONLY_STENCIL:0));
    return view;
}
ColorSubresources DepthRange(const D3D12_RESOURCE_DESC& resource,const D3D12_DEPTH_STENCIL_VIEW_DESC& view){
    ColorSubresources range{0,0,1};const bool multisampled=resource.SampleDesc.Count>1;
    switch(view.ViewDimension){
    case D3D12_DSV_DIMENSION_TEXTURE2D:
        if(multisampled||resource.DepthOrArraySize!=1)throw std::invalid_argument("Native depth view dimension/sample mismatch");
        range.mip=view.Texture2D.MipSlice;break;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
        if(multisampled)throw std::invalid_argument("Native depth array view sample mismatch");
        range={view.Texture2DArray.MipSlice,view.Texture2DArray.FirstArraySlice,view.Texture2DArray.ArraySize};break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS:
        if(!multisampled||resource.DepthOrArraySize!=1)throw std::invalid_argument("Native multisample depth view mismatch");break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
        if(!multisampled)throw std::invalid_argument("Native multisample depth array view mismatch");
        range={0,view.Texture2DMSArray.FirstArraySlice,view.Texture2DMSArray.ArraySize};break;
    default:throw std::invalid_argument("Native depth view is not two dimensional");
    }
    if(range.mip>=resource.MipLevels||!range.count||range.first>=resource.DepthOrArraySize||
        range.count>resource.DepthOrArraySize-range.first)throw std::out_of_range("Native depth view exceeds resource subresources");
    return range;
}
}
void Rasterizer::ValidateDebugMessages() {
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
    std::span<const VertexBinding> vertices,const IndexBinding* indices,bool wait,const StreamDrawArguments* stream_draw,
    const StreamOutputBinding* stream_output) {
    std::lock_guard lock(execution_mutex_);
    if(pipeline.IsCompute())throw std::invalid_argument("Native graphics submission has compute PSO");
    if(state.discard||(stream_draw?!stream_draw->max_count:(!draw.count||!draw.instances)))return;
    bindings.Prepare();
    D3D12_STREAM_OUTPUT_BUFFER_VIEW stream_view{};
    if(stream_output){
        if(!stream_output->data||!stream_output->filled_size||!stream_output->data->handle||!stream_output->filled_size->handle||
           !state.rasterizer_discard||state.pipeline.StreamOutput.NumStrides!=1||!state.pipeline.StreamOutput.NumEntries||
           !state.pipeline.StreamOutput.pBufferStrides||!state.pipeline.StreamOutput.pBufferStrides[0]||
           state.pipeline.StreamOutput.RasterizedStream!=D3D12_SO_NO_RASTERIZED_STREAM)
            throw std::invalid_argument("Invalid internal stream-output targets or pipeline");
        const auto data=stream_output->data->handle->GetDesc(),counter=stream_output->filled_size->handle->GetDesc();
        if(data.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||counter.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
           stream_output->offset%4||stream_output->counter_offset%4||!stream_output->bytes||
           stream_output->bytes%state.pipeline.StreamOutput.pBufferStrides[0]||
           stream_output->bytes>UINT32_MAX||stream_output->offset>data.Width||stream_output->bytes>data.Width-stream_output->offset||
           stream_output->counter_offset>counter.Width||8>counter.Width-stream_output->counter_offset)
            throw std::out_of_range("Internal stream-output targets exceed their buffers");
        const auto is_output=[&](ID3D12Resource* resource){return resource==stream_output->data->handle.Get()||resource==stream_output->filled_size->handle.Get();};
        if(bindings.References(stream_output->data->handle.Get())||bindings.References(stream_output->filled_size->handle.Get())||
           (indices&&indices->resource&&is_output(indices->resource->handle.Get()))||
           (stream_draw&&stream_draw->resource&&is_output(stream_draw->resource->handle.Get()))||
           (stream_draw&&stream_draw->count_resource&&is_output(stream_draw->count_resource->handle.Get()))||
           (stream_draw&&stream_draw->runtime_constants&&is_output(stream_draw->runtime_constants->handle.Get()))||
           std::any_of(vertices.begin(),vertices.end(),[&](const auto& vertex){return vertex.resource&&is_output(vertex.resource->handle.Get());}))
            throw std::invalid_argument("Internal stream output overlaps draw input or shader resources");
        if(stream_output->data->handle.Get()==stream_output->filled_size->handle.Get()&&
           stream_output->counter_offset<stream_output->offset+stream_output->bytes&&
           stream_output->offset<stream_output->counter_offset+8)
            throw std::invalid_argument("Internal stream data overlaps its filled-size counter");
        stream_view={stream_output->data->handle->GetGPUVirtualAddress()+stream_output->offset,stream_output->bytes,
            stream_output->filled_size->handle->GetGPUVirtualAddress()+stream_output->counter_offset};
        RetainUntilFinish([data=stream_output->data->handle,counter=stream_output->filled_size->handle,
            data_allocation=stream_output->data->allocation,counter_allocation=stream_output->filled_size->allocation](u64){});
    }
    ComPtr<ID3D12CommandSignature> indirect_signature;
    if(stream_draw){
        if(!stream_draw->resource||!stream_draw->resource->handle)
            throw std::invalid_argument("Invalid internal primitive stream draw arguments");
        const auto desc=stream_draw->resource->handle->GetDesc();
        const bool runtime=stream_draw->runtime_constants!=nullptr;
        const auto layout=NativeIndirectLayout::Make(indices!=nullptr,stream_draw->offset,stream_draw->stride,stream_draw->max_count,runtime);
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||layout.offset>desc.Width||layout.bytes>desc.Width-layout.offset)
            throw std::out_of_range("Internal primitive stream draw arguments exceed their buffer");
        // This private command buffer is distinct from shader data. Do not
        // permit a simultaneous UAV binding to change its indirect arguments.
        if(bindings.References(stream_draw->resource->handle.Get()))
            throw std::invalid_argument("Internal draw arguments overlap shader bindings");
        ComPtr<ID3D12Device> owner;
        Check(stream_draw->resource->handle->GetDevice(IID_PPV_ARGS(&owner)));
        if(owner.Get()!=device_.Get())throw std::invalid_argument("Indirect arguments belong to another device");
        if(stream_draw->count_resource){
            const auto& count=*stream_draw->count_resource;
            if(!count.handle)throw std::invalid_argument("Missing GPU indirect count buffer");
            const auto counter=count.handle->GetDesc();
            if(counter.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||stream_draw->count_offset%4||
               stream_draw->count_offset>counter.Width||4>counter.Width-stream_draw->count_offset)
                throw std::out_of_range("GPU indirect count exceeds its buffer");
            if(bindings.References(count.handle.Get()))throw std::invalid_argument("Indirect count overlaps shader bindings");
            if(count.handle.Get()==stream_draw->resource->handle.Get()&&stream_draw->count_resource!=stream_draw->resource)
                throw std::invalid_argument("Shared indirect argument/count buffer requires one authoritative resource state");
            Check(count.handle->GetDevice(IID_PPV_ARGS(&owner)));
            if(owner.Get()!=device_.Get())throw std::invalid_argument("Indirect count belongs to another device");
            RetainUntilFinish([resource=count.handle,allocation=count.allocation](u64){});
        }
        auto* runtime_root=runtime?bindings.Layout().Root():nullptr;
        if(runtime){
            const auto& constants=*stream_draw->runtime_constants;
            if(bindings.Layout().GraphicsRuntimeParameter()==UINT_MAX||!constants.handle||
               constants.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||constants.handle->GetDesc().Width<256)
                throw std::invalid_argument("Indirect runtime requires a reflected root CBV and constant buffer");
            // This private per-command arena is read through indirect root
            // addresses, not through the prepared fallback descriptor table.
            if(bindings.References(constants.handle.Get()))throw std::invalid_argument("Indirect runtime arena overlaps shader bindings");
            Check(constants.handle->GetDevice(IID_PPV_ARGS(&owner)));
            if(owner.Get()!=device_.Get())throw std::invalid_argument("Indirect runtime belongs to another device");
            RetainUntilFinish([resource=constants.handle,allocation=constants.allocation](u64){});
        }
        auto& cached=indirect_draw_signatures_[{indices!=nullptr,layout.stride,runtime_root}];
        if(!cached.signature){
            D3D12_INDIRECT_ARGUMENT_DESC arguments[2]{};UINT count=0;
            if(runtime){auto& cbv=arguments[count++];cbv.Type=D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
                cbv.ConstantBufferView.RootParameterIndex=bindings.Layout().GraphicsRuntimeParameter();}
            auto& argument=arguments[count++];argument.Type=indices?
                D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
            const D3D12_COMMAND_SIGNATURE_DESC signature{layout.stride,count,arguments,0};
            Check(device_->CreateCommandSignature(&signature,runtime_root,IID_PPV_ARGS(&cached.signature)));
            cached.root=runtime_root;
        }
        indirect_signature=cached.signature;
        RetainUntilFinish([resource=stream_draw->resource->handle,
            allocation=stream_draw->resource->allocation,signature=indirect_signature](u64){});
    }
    if(state.topology==D3D_PRIMITIVE_TOPOLOGY_UNDEFINED||colors.size()!=pipeline.TargetFormats().size()||
       viewports.empty()||viewports.size()>16||scissors.size()!=viewports.size()||vertices.size()>32)
        throw std::invalid_argument("Native draw topology/attachment/viewport/input count");
    for(const auto& v:viewports)if(!std::isfinite(v.TopLeftX)||!std::isfinite(v.TopLeftY)||
        !std::isfinite(v.Width)||!std::isfinite(v.Height)||!std::isfinite(v.MinDepth)||!std::isfinite(v.MaxDepth)||
        v.Width<=0||v.Height<=0||v.MinDepth<0||v.MaxDepth>1||v.MinDepth>v.MaxDepth)
        throw std::invalid_argument("Native draw viewport bounds");
    for(const auto& r:scissors)if(r.right<r.left||r.bottom<r.top)throw std::invalid_argument("Native draw inverted scissor");
    std::set<ID3D12Resource*> attachments;
    std::vector<std::pair<ID3D12Resource*,ColorSubresources>> color_ranges;
    D3D12_DEPTH_STENCIL_VIEW_DESC depth_view{};ColorSubresources depth_range{};
    for(size_t i=0;i<colors.size();++i) {
        const auto& color=colors[i];const auto expected=pipeline.TargetFormats()[i];
        if(!color.resource){if(expected!=DXGI_FORMAT_UNKNOWN)throw std::invalid_argument("Native draw missing color target");continue;}
        if(!color.resource->handle)throw std::invalid_argument("Null native draw color resource");
        const auto d=DX12_API_CALL(color.resource->handle->GetDesc());
        const auto format=color.view.Format==DXGI_FORMAT_UNKNOWN?d.Format:color.view.Format;
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||
           d.SampleDesc.Count!=pipeline.SampleCount()||format!=expected)
            throw std::invalid_argument("Native draw color target format/sample/usage/alias mismatch");
        const auto range=ColorRange(d,color.view);
        for(UINT layer=range.first;layer<range.first+range.count;++layer)
            if(bindings.ShaderReferences(color.resource->handle.Get(),range.mip,layer,0))
                throw std::runtime_error("Native color attachment/shader access overlaps the same subresource");
        for(const auto& [resource,previous]:color_ranges)
            if(resource==color.resource->handle.Get()&&range.mip==previous.mip&&
                range.first<previous.first+previous.count&&previous.first<range.first+range.count)
                throw std::invalid_argument("Native color targets overlap the same subresource");
        color_ranges.emplace_back(color.resource->handle.Get(),range);
        attachments.insert(color.resource->handle.Get());
    }
    if(depth&&depth->resource) {
        if(!depth->resource->handle)throw std::invalid_argument("Null native draw depth resource");
        const auto resource_desc=depth->resource->handle->GetDesc();
        const bool has_stencil=depth->resource->SubresourceCount()>UINT(resource_desc.MipLevels)*UINT(resource_desc.DepthOrArraySize);
        if(depth->clear_stencil&&!has_stencil)throw std::invalid_argument("Native stencil clear has no stencil plane");
        depth_view=DepthView(*depth->resource,depth->view,pipeline.DepthFormat(),depth->read_only_depth,depth->read_only_stencil);
        depth_range=DepthRange(resource_desc,depth_view);
        for(UINT layer=depth_range.first;layer<depth_range.first+depth_range.count;++layer)
            if((bindings.ShaderAccess(depth->resource->handle.Get(),depth_range.mip,layer,0)&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)||
               (has_stencil&&(bindings.ShaderAccess(depth->resource->handle.Get(),depth_range.mip,layer,1)&D3D12_RESOURCE_STATE_UNORDERED_ACCESS))||
               (!depth->read_only_depth&&bindings.ShaderReferences(depth->resource->handle.Get(),depth_range.mip,layer,0))||
               (has_stencil&&!depth->read_only_stencil&&bindings.ShaderReferences(depth->resource->handle.Get(),depth_range.mip,layer,1)))
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
           indices->bytes%element||indices->bytes>d.Width-indices->offset)
            throw std::out_of_range("Native indexed draw bounds");
        index_view={DX12_API_CALL(indices->resource->handle->GetGPUVirtualAddress())+indices->offset,indices->bytes,indices->format};
        bindings.Retain(indices->resource->handle.Get());bindings.RetainAllocation(indices->allocation);
    }
    if(colors.size()>8)throw std::invalid_argument("Native draw exceeds eight color targets");
    if(!color_descriptors_)color_descriptors_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,8,false);
    if(depth&&depth->resource&&!depth_descriptors_)depth_descriptors_=std::make_unique<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,2,false);
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
        DX12_API_CALL(device_->CreateDepthStencilView(depth->resource->handle.Get(),&depth_view,depth_handle));
    }
    try {
        auto* list=RecordingList();BarrierBatch dependencies;
        // A beginning clear precedes every shader load. A plane read-only
        // during this draw must therefore be cleared through a separate
        // writable DSV before entering its combined depth/SRV read state.
        const bool preclear_depth=depth&&depth->resource&&depth->clear_depth&&depth->read_only_depth;
        const bool preclear_stencil=depth&&depth->resource&&depth->clear_stencil&&depth->read_only_stencil;
        if(preclear_depth||preclear_stencil){
            auto view=depth_view;view.Flags=D3D12_DSV_FLAG_NONE;
            const auto clear_handle=depth_descriptors_->Cpu(1);
            DX12_API_CALL(device_->CreateDepthStencilView(depth->resource->handle.Get(),&view,clear_handle));
            // No shader descriptors or DSV are bound during the clear. Both
            // native planes can be writable here, then receive draw states.
            depth->resource->Transition(list,D3D12_RESOURCE_STATE_DEPTH_WRITE);
            DX12_API_CALL(list->ClearDepthStencilView(clear_handle,D3D12_CLEAR_FLAGS(
                (preclear_depth?D3D12_CLEAR_FLAG_DEPTH:0)|(preclear_stencil?D3D12_CLEAR_FLAG_STENCIL:0)),
                depth->clear_value,depth->stencil_value,0,nullptr));
        }
        std::set<ID3D12Resource*> transitioned_colors;
        for(const auto& color:colors)if(color.resource&&transitioned_colors.insert(color.resource->handle.Get()).second){
            auto* native=color.resource->handle.Get();
            if(!bindings.ShaderPlanes(native)){dependencies.Transition(*color.resource,D3D12_RESOURCE_STATE_RENDER_TARGET);continue;}
            const auto desc=native->GetDesc();
            if(bindings.HasShaderUav(native))color.resource->UavBarrier(list);
            for(UINT sub=0;sub<color.resource->SubresourceCount();++sub){
                const auto mip=sub%desc.MipLevels,layer=(sub/desc.MipLevels)%desc.DepthOrArraySize;
                const auto plane=sub/(UINT(desc.MipLevels)*desc.DepthOrArraySize);
                bool attached=false;
                for(const auto& [resource,range]:color_ranges)if(resource==native&&plane==0&&mip==range.mip&&
                    layer>=range.first&&layer-range.first<range.count)attached=true;
                const auto access=bindings.ShaderAccess(native,mip,layer,plane);
                if((access&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)&&access!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                    throw std::runtime_error("Native shader SRV/UAV texture aliases overlap the same subresource");
                if(attached||access)dependencies.Transition(*color.resource,
                    attached?D3D12_RESOURCE_STATE_RENDER_TARGET:access,sub);
            }
        }
        if(depth&&depth->resource) {
            const auto desc=depth->resource->handle->GetDesc();
            const auto plane_size=UINT(desc.MipLevels)*UINT(desc.DepthOrArraySize);
            const auto count=depth->resource->SubresourceCount();
            const auto sampled_depth=bindings.ShaderReferences(depth->resource->handle.Get(),0,0,0);
            const auto sampled_stencil=count>plane_size&&bindings.ShaderReferences(depth->resource->handle.Get(),0,0,1);
            const bool whole_attachment=desc.MipLevels==1&&depth_range.first==0&&depth_range.count==desc.DepthOrArraySize;
            const bool single_layer=desc.DepthOrArraySize==1;
            const bool uniform_access=count==plane_size||depth->read_only_depth==depth->read_only_stencil;
            const bool uniform_shader=single_layer&&(count==plane_size||sampled_depth==sampled_stencil);
            const bool no_shader=bindings.ShaderPlanes(depth->resource->handle.Get())==0;
            if(whole_attachment&&uniform_access&&(uniform_shader||no_shader)&&!bindings.HasShaderUav(depth->resource->handle.Get())){
                const auto access=depth->read_only_depth?D3D12_RESOURCE_STATE_DEPTH_READ:D3D12_RESOURCE_STATE_DEPTH_WRITE;
                dependencies.Transition(*depth->resource,D3D12_RESOURCE_STATES(access|
                    (sampled_depth?(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE):0)));
            }else for(UINT subresource=0;subresource<count;++subresource){
                const auto mip=subresource%desc.MipLevels,layer=(subresource/desc.MipLevels)%desc.DepthOrArraySize;
                const auto plane=subresource/plane_size;
                const bool attached=mip==depth_range.mip&&layer>=depth_range.first&&layer-depth_range.first<depth_range.count;
                const auto shader_access=bindings.ShaderAccess(depth->resource->handle.Get(),mip,layer,plane);
                const bool sampled=shader_access!=D3D12_RESOURCE_STATE_COMMON;
                if((shader_access&D3D12_RESOURCE_STATE_UNORDERED_ACCESS)&&shader_access!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                    throw std::runtime_error("Native shader SRV/UAV depth aliases overlap the same subresource");
                if(!attached&&!sampled)continue;
                const bool read_only=plane==0?depth->read_only_depth:depth->read_only_stencil;
                const auto access=read_only?D3D12_RESOURCE_STATE_DEPTH_READ:D3D12_RESOURCE_STATE_DEPTH_WRITE;
                dependencies.Transition(*depth->resource,D3D12_RESOURCE_STATES((attached?access:0)|shader_access),subresource);
            }
        }
        // Read usages combine. An IA buffer can also be fetched by a shader;
        // switching it to INDEX_BUFFER alone would invalidate that SRV state.
        std::map<Resource*,D3D12_RESOURCE_STATES> inputs;
        for(const auto& v:vertices)inputs[v.resource]|=D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if(indices)inputs[indices->resource]|=D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if(stream_draw)inputs[stream_draw->resource]|=D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        if(stream_draw&&stream_draw->count_resource)inputs[stream_draw->count_resource]|=D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        if(stream_draw&&stream_draw->runtime_constants)inputs[stream_draw->runtime_constants]|=D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        for(const auto& [resource,input_state]:inputs){
            const auto shader_read=bindings.References(resource->handle.Get())?
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COMMON;
            // Preserve existing read bits, including the mandatory GENERIC_READ
            // state of upload-heap CPU index streams and null index buffers.
            const auto read_state=resource->state&D3D12_RESOURCE_STATE_GENERIC_READ;
            dependencies.Transition(*resource,D3D12_RESOURCE_STATES(read_state|input_state|shader_read));
        }
        if(stream_output){
            dependencies.Transition(*stream_output->data,D3D12_RESOURCE_STATE_STREAM_OUT);
            dependencies.Transition(*stream_output->filled_size,D3D12_RESOURCE_STATE_STREAM_OUT);
        }
        dependencies.Flush(list);pipeline.Bind(list,bindings);
        // Bind after BeforeBind/ownership callbacks: they may Finish and start
        // another command list. An earlier SOSetTargets would be lost there.
        if(stream_output)DX12_API_CALL(list->SOSetTargets(0,1,&stream_view));
        DX12_API_CALL(list->RSSetViewports(UINT(viewports.size()),viewports.data()));DX12_API_CALL(list->RSSetScissorRects(UINT(scissors.size()),scissors.data()));
        ComPtr<ID3D12GraphicsCommandList4> render_list;D3D12_FEATURE_DATA_D3D12_OPTIONS5 render_options{};
        D3D12_FEATURE_DATA_D3D12_OPTIONS18 render_validity{};
        // Guest vertex/pixel stores and atomics need ordinary UAV ordering.
        // Use the existing OM path for these draws rather than imposing render
        // pass UAV restrictions on shaders which the guest allows to write.
        const bool shader_uavs=std::any_of(bindings.Layout().Bindings().begin(),bindings.Layout().Bindings().end(),
            [](const auto& binding){return binding.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV;});
        const bool render_pass=!state.rasterizer_discard&&!shader_uavs&&SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&render_list)))&&
            SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&render_options,sizeof(render_options)))&&
            SUCCEEDED(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS18,&render_validity,sizeof(render_validity)))&&
            render_validity.RenderPassesValid&&
            render_options.RenderPassesTier!=D3D12_RENDER_PASS_TIER_0;
        std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> render_targets;
        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC render_depth{};
        if(render_pass){
            render_targets.reserve(handles.size());for(const auto handle:handles){D3D12_RENDER_PASS_RENDER_TARGET_DESC target{};
                target.cpuDescriptor=handle;target.BeginningAccess.Type=D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                target.EndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;render_targets.push_back(target);}
            if(depth&&depth->resource){render_depth.cpuDescriptor=depth_handle;
                render_depth.DepthBeginningAccess.Type=depth->clear_depth&&!preclear_depth?D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                render_depth.DepthBeginningAccess.Clear.ClearValue.Format=depth_view.Format;
                render_depth.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth=depth->clear_value;
                render_depth.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil=depth->stencil_value;
                render_depth.DepthEndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                render_depth.StencilBeginningAccess.Type=depth->clear_stencil&&!preclear_stencil?D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR:D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                render_depth.StencilBeginningAccess.Clear.ClearValue=render_depth.DepthBeginningAccess.Clear.ClearValue;
                render_depth.StencilEndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                if(depth_view.Format!=DXGI_FORMAT_D32_FLOAT_S8X24_UINT&&depth_view.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT){
                    render_depth.StencilBeginningAccess.Type=D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
                    render_depth.StencilEndingAccess.Type=D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
                }
            }
            D3D12_RENDER_PASS_FLAGS flags=D3D12_RENDER_PASS_FLAG_NONE;
            if(depth&&depth->resource){
                if(depth_view.Flags&D3D12_DSV_FLAG_READ_ONLY_DEPTH)flags|=D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH;
                if(depth_view.Flags&D3D12_DSV_FLAG_READ_ONLY_STENCIL)flags|=D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_STENCIL;
            }
            DX12_API_CALL(render_list->BeginRenderPass(UINT(render_targets.size()),render_targets.data(),
                depth&&depth->resource?&render_depth:nullptr,flags));
        }else{
            if(depth&&depth->resource&&((depth->clear_depth&&!preclear_depth)||(depth->clear_stencil&&!preclear_stencil)))
                DX12_API_CALL(list->ClearDepthStencilView(depth_handle,D3D12_CLEAR_FLAGS((depth->clear_depth&&!preclear_depth?D3D12_CLEAR_FLAG_DEPTH:0)|
                    (depth->clear_stencil&&!preclear_stencil?D3D12_CLEAR_FLAG_STENCIL:0)),depth->clear_value,depth->stencil_value,0,nullptr));
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
        if(state.depth_bounds&&!state.rasterizer_discard) {
            if(!depth||!depth->resource)throw std::runtime_error("Native depth bounds attachment missing");
            ComPtr<ID3D12GraphicsCommandList1> extended;Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&extended))));
            DX12_API_CALL(extended->OMSetDepthBounds(state.depth_bounds_min,state.depth_bounds_max));
        }
        if(!vertex_views.empty())DX12_API_CALL(list->IASetVertexBuffers(0,UINT(vertex_views.size()),vertex_views.data()));
        if(stream_draw){
            if(indices)DX12_API_CALL(list->IASetIndexBuffer(&index_view));
            DX12_API_CALL(list->ExecuteIndirect(indirect_signature.Get(),stream_draw->max_count,
                stream_draw->resource->handle.Get(),stream_draw->offset,
                stream_draw->count_resource?stream_draw->count_resource->handle.Get():nullptr,stream_draw->count_offset));
        }
        else if(indices){DX12_API_CALL(list->IASetIndexBuffer(&index_view));DX12_API_CALL(list->DrawIndexedInstanced(draw.count,draw.instances,draw.first,draw.base_vertex,draw.first_instance));}
        else DX12_API_CALL(list->DrawInstanced(draw.count,draw.instances,draw.first,draw.first_instance));
        if(render_pass)DX12_API_CALL(render_list->EndRenderPass());
        if(stream_output){
            const D3D12_STREAM_OUTPUT_BUFFER_VIEW empty{};DX12_API_CALL(list->SOSetTargets(0,1,&empty));
            // Unbinding alone does not publish the filled-size write before a
            // later SOSetTargets reloads it. Order the counter on the GPU so
            // split producer draws append instead of overwriting prior data.
            stream_output->filled_size->Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }
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
