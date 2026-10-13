// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_multisample.h"
#include "guest_texture_cache.h"
#include "guest_msaa_shaders.h"
#include <d3dcompiler.h>
#include <string>
#include <numeric>
namespace NativeDX12 {
namespace {
ComPtr<ID3DBlob> Compile(std::string_view source,const char* target,const D3D_SHADER_MACRO* macros=nullptr){
    ComPtr<ID3DBlob> code,errors;
    const auto result=DX12_API_CALL(D3DCompile(source.data(),source.size(),"guest-msaa",macros,nullptr,
        "main",target,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3|
        D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&code,&errors));
    if(FAILED(result))throw std::runtime_error(errors?std::string(
        static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize()):
        "Native guest MSAA shader compilation failed");
    return code;
}
DXGI_FORMAT LinearFormat(DXGI_FORMAT format){
    return format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB?DXGI_FORMAT_R8G8B8A8_UNORM:format;
}
DXGI_FORMAT TransferFormat(const GuestTextureDesc& d,const GuestTextureLayout& layout){
    if(d.depth_target)return LinearFormat(layout.format);
    if(d.data_format>=16&&d.data_format<=19)return d.writable?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R16_UINT;
    switch(d.data_format){
    case 1:return DXGI_FORMAT_R8_UINT;
    case 2:return DXGI_FORMAT_R16_UINT;
    case 3:return DXGI_FORMAT_R8G8_UINT;
    case 4:case 6:case 34:return DXGI_FORMAT_R32_UINT;
    case 5:return DXGI_FORMAT_R16G16_UINT;
    case 9:return layout.expanded_bytes_per_pixel==16?DXGI_FORMAT_R32G32B32A32_UINT:
        layout.format==DXGI_FORMAT_R32_UINT?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R10G10B10A2_UINT;
    case 10:return DXGI_FORMAT_R8G8B8A8_UINT;
    case 11:return DXGI_FORMAT_R32G32_UINT;
    case 12:return DXGI_FORMAT_R16G16B16A16_UINT;
    case 13:return DXGI_FORMAT_R32G32B32A32_UINT;
    case 14:return DXGI_FORMAT_R32G32B32A32_UINT;
    default:return LinearFormat(layout.format);
    }
}
const char* ValueType(DXGI_FORMAT format){
    switch(format){
    case DXGI_FORMAT_R8_UINT:case DXGI_FORMAT_R8G8_UINT:case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R16_UINT:case DXGI_FORMAT_R16G16_UINT:case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R32_UINT:case DXGI_FORMAT_R32G32_UINT:case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R10G10B10A2_UINT:return "uint4";
    case DXGI_FORMAT_R8_SINT:case DXGI_FORMAT_R8G8_SINT:case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16_SINT:case DXGI_FORMAT_R16G16_SINT:case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32_SINT:case DXGI_FORMAT_R32G32_SINT:case DXGI_FORMAT_R32G32B32A32_SINT:
        return "int4";
    default:return "float4";
    }
}
}
D3D12_PLACED_SUBRESOURCE_FOOTPRINT GuestMultisample::Footprint(
    const GuestTextureDesc& d,const GuestTextureLayout& layout){
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
    const auto host_bytes=layout.HostBytesPerPixel();
    const auto alignment=std::lcm(512u,host_bytes);
    fp.Footprint={LinearFormat(layout.format),d.width,d.height,1,
        ((d.width*host_bytes+alignment-1)/alignment)*alignment};
    return fp;
}
uint64_t GuestMultisample::LinearBytes(const GuestTextureDesc& d,
    const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& fp){
    return uint64_t(fp.Footprint.RowPitch)*d.height*d.samples;
}
std::shared_ptr<GuestMultisample::Lease> GuestMultisample::Prepare(Resource& texture,Resource& linear,
    const GuestTextureDesc& d,const GuestTextureLayout& layout,UINT layer,UINT plane,bool upload){
    const auto texture_desc=texture.handle->GetDesc(),buffer_desc=linear.handle->GetDesc();
    if(d.samples<2||d.levels!=1||d.volume_depth||d.one_dimensional||layer>=d.layers||plane>1||
       (plane&&!d.stencil_address)||texture_desc.SampleDesc.Count!=d.samples||
       texture_desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       buffer_desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||layout.block_extent!=1)
        throw std::invalid_argument("Invalid native guest MSAA transfer resources");
    const bool depth=d.depth_target,stencil=plane!=0;
    if(!depth&&(d.data_format==6||d.data_format==34)&&!texture.AllowsCast(DXGI_FORMAT_R32_UINT))
        throw std::runtime_error("Native packed-float MSAA texture has no raw integer view");
    const auto interpreted=stencil?DescribeGuestStencil(d):d;
    const auto physical=stencil?LayoutGuestTexture(interpreted):layout;
    const auto fp=Footprint(interpreted,physical);
    const auto bytes=LinearBytes(interpreted,fp);
    if(bytes>UINT32_MAX||buffer_desc.Width<bytes||buffer_desc.Width%4||
       uint64_t(d.samples)*d.layers>65535)
        throw std::out_of_range("Native guest MSAA linear addressing exceeds buffer");
    const auto buffer_format=TransferFormat(interpreted,physical);
    const bool packed=!d.writable&&d.data_format>=16&&d.data_format<=19;
    const auto attachment_format=depth?(d.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        layout.bytes_per_pixel==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT):
        packed?layout.format:TransferFormat(d,layout);
    const auto texture_format=depth?(stencil?DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        d.stencil_address?DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        layout.bytes_per_pixel==2?DXGI_FORMAT_R16_UNORM:DXGI_FORMAT_R32_FLOAT):
        packed?layout.format:TransferFormat(d,layout);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{buffer_format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
    if(!(support.Support1&D3D12_FORMAT_SUPPORT1_BUFFER)||
       (upload&&!(support.Support1&D3D12_FORMAT_SUPPORT1_SHADER_LOAD))||
       (!upload&&!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)))
        throw std::runtime_error("Native guest MSAA transfer format lacks typed buffer access: "+
            std::to_string(buffer_format));
    D3D12_FEATURE_DATA_FORMAT_SUPPORT texture_support{upload?attachment_format:texture_format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&texture_support,sizeof(texture_support))));
    if(!upload&&!(texture_support.Support1&D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD))
        throw std::runtime_error("Native guest MSAA transfer format lacks sample loads");
    if(upload){
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{attachment_format,d.samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
        if(!quality.NumQualityLevels)throw std::runtime_error("Native guest MSAA transfer view sample count is unsupported");
    }
    if(stencil&&upload){
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options))));
        if(!options.PSSpecifiedStencilRefSupported)
            throw std::runtime_error("Native guest MSAA stencil upload requires pixel shader stencil reference");
    }
    const Key key{buffer_format,upload?attachment_format:texture_format,d.samples,depth,stencil,upload};
    auto& program=programs_[key];
    if(!program){
        auto compiled=std::make_shared<Program>();
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
                                        {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable={upload?1u:2u,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants={0,0,4};
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&compiled->root))));
        const D3D_SHADER_MACRO macros[]{{"VALUE_TYPE",ValueType(buffer_format)},
            {"TEXTURE_TYPE",ValueType(texture_format)},
            {"PACKED_MODE",!packed?"0":d.data_format==16?"1":d.data_format==19?"3":"2"},
            {"NATIVE_DEPTH",depth?"1":"0"},{"NATIVE_STENCIL",stencil?"1":"0"},{nullptr,nullptr}};
        if(upload){
            const auto vs=Compile("float4 main(uint id:SV_VertexID):SV_Position{return float4(id==2?3:-1,id==1?3:-1,0,1);}","vs_5_1");
            const auto ps=Compile(GuestMsaaUploadSource,"ps_5_1",macros);
            D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=compiled->root.Get();
            state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
            state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
            state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=TRUE;
            state.SampleMask=UINT_MAX;state.SampleDesc=texture_desc.SampleDesc;
            state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            if(depth){
                state.DSVFormat=attachment_format;
                state.DepthStencilState.DepthEnable=!stencil;
                state.DepthStencilState.DepthWriteMask=stencil?D3D12_DEPTH_WRITE_MASK_ZERO:D3D12_DEPTH_WRITE_MASK_ALL;
                state.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
                state.DepthStencilState.StencilEnable=stencil;
                state.DepthStencilState.StencilReadMask=state.DepthStencilState.StencilWriteMask=255;
                state.DepthStencilState.FrontFace=state.DepthStencilState.BackFace={
                    D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_REPLACE,D3D12_COMPARISON_FUNC_ALWAYS};
            }else{
                state.NumRenderTargets=1;state.RTVFormats[0]=attachment_format;
                state.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
            }
            Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&compiled->pipeline))));
        }else{
            const auto cs=Compile(GuestMsaaReadbackSource,"cs_5_1",macros);
            D3D12_COMPUTE_PIPELINE_STATE_DESC state{};state.pRootSignature=compiled->root.Get();
            state.CS={cs->GetBufferPointer(),cs->GetBufferSize()};
            Check(DX12_API_CALL(device_->CreateComputePipelineState(&state,IID_PPV_ARGS(&compiled->pipeline))));
        }
        program=std::move(compiled);
    }
    auto lease=std::make_shared<Lease>();lease->program=program;lease->texture=texture;lease->linear=linear;
    lease->upload=upload;lease->depth=depth;lease->stencil=stencil;lease->layer=layer;
    lease->constants={d.width,d.height,d.samples,fp.Footprint.RowPitch/physical.HostBytesPerPixel()};
    lease->descriptors=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,upload?1:2,true);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if(upload){
        srv.Format=buffer_format;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
        srv.Buffer.NumElements=UINT(bytes/physical.HostBytesPerPixel());
        DX12_API_CALL(device_->CreateShaderResourceView(linear.handle.Get(),&srv,lease->descriptors->Cpu(0)));
        lease->attachment=std::make_shared<Descriptors>(device_.Get(),depth?D3D12_DESCRIPTOR_HEAP_TYPE_DSV:D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);
        if(depth){
            D3D12_DEPTH_STENCIL_VIEW_DESC view{};view.Format=attachment_format;
            view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
            view.Texture2DMSArray={layer,1};
            DX12_API_CALL(device_->CreateDepthStencilView(texture.handle.Get(),&view,lease->attachment->Cpu(0)));
        }else{
            D3D12_RENDER_TARGET_VIEW_DESC view{};view.Format=attachment_format;
            view.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;view.Texture2DMSArray={layer,1};
            DX12_API_CALL(device_->CreateRenderTargetView(texture.handle.Get(),&view,lease->attachment->Cpu(0)));
        }
    }else{
        srv.Format=texture_format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
        srv.Texture2DMSArray={layer,1};
        if(stencil)srv.Shader4ComponentMapping=D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(1,1,1,1);
        DX12_API_CALL(device_->CreateShaderResourceView(texture.handle.Get(),&srv,lease->descriptors->Cpu(0)));
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=buffer_format;
        uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=UINT(bytes/physical.HostBytesPerPixel());
        DX12_API_CALL(device_->CreateUnorderedAccessView(linear.handle.Get(),nullptr,&uav,lease->descriptors->Cpu(1)));
    }
    return lease;
}
void GuestMultisample::Lease::Record(ID3D12GraphicsCommandList* list,Resource& source_texture,Resource& buffer)const{
    if(source_texture.handle.Get()!=texture.handle.Get()||buffer.handle.Get()!=linear.handle.Get())
        throw std::invalid_argument("Native guest MSAA transfer resource changed");
    ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};
    if(upload){
        if(buffer.state!=D3D12_RESOURCE_STATE_GENERIC_READ)buffer.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        source_texture.Transition(list,depth?D3D12_RESOURCE_STATE_DEPTH_WRITE:D3D12_RESOURCE_STATE_RENDER_TARGET);
        // This host transfer replaces every sample of the selected color
        // slice. Placed RTs require initialization before a shader draw;
        // discard only that fully replaced subresource, never another layer
        // or a depth/stencil plane and never guest fast-clear metadata.
        const auto desc=source_texture.handle->GetDesc();
        if(!depth&&desc.Width==constants[0]&&desc.Height==constants[1]){
            D3D12_DISCARD_REGION region{0,nullptr,layer*UINT(desc.MipLevels),1};
            DX12_API_CALL(list->DiscardResource(source_texture.handle.Get(),&region));
        }
        DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
        DX12_API_CALL(list->SetGraphicsRootSignature(program->root.Get()));
        DX12_API_CALL(list->SetPipelineState(program->pipeline.Get()));
        DX12_API_CALL(list->SetGraphicsRootDescriptorTable(0,descriptors->Gpu(0)));
        DX12_API_CALL(list->SetGraphicsRoot32BitConstants(1,4,constants.data(),0));
        D3D12_VIEWPORT viewport{0,0,float(constants[0]),float(constants[1]),0,1};
        D3D12_RECT scissor{0,0,LONG(constants[0]),LONG(constants[1])};
        DX12_API_CALL(list->RSSetViewports(1,&viewport));DX12_API_CALL(list->RSSetScissorRects(1,&scissor));
        DX12_API_CALL(list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
        const auto handle=attachment->Cpu(0);
        DX12_API_CALL(list->OMSetRenderTargets(depth?0:1,depth?nullptr:&handle,FALSE,depth?&handle:nullptr));
        DX12_API_CALL(list->DrawInstanced(3,1,0,0));
    }else{
        source_texture.Transition(list,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        buffer.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        buffer.UavBarrier(list);
        DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
        DX12_API_CALL(list->SetComputeRootSignature(program->root.Get()));
        DX12_API_CALL(list->SetPipelineState(program->pipeline.Get()));
        DX12_API_CALL(list->SetComputeRootDescriptorTable(0,descriptors->Gpu(0)));
        DX12_API_CALL(list->SetComputeRoot32BitConstants(1,4,constants.data(),0));
        DX12_API_CALL(list->Dispatch((constants[0]+7)/8,(constants[1]+7)/8,constants[2]));
        buffer.UavBarrier(list);
    }
}
std::shared_ptr<GuestMultisample::StorageLease> GuestMultisample::PrepareStorage(
    Resource& texture,std::span<Resource> samples,Resource& linear,const GuestTextureDesc& d,
    const GuestTextureLayout& layout){
    if(d.depth_target||samples.size()!=d.samples||d.samples<2)
        throw std::invalid_argument("Invalid native MSAA storage sample resources");
    auto lease=std::make_shared<StorageLease>();lease->texture=texture;lease->linear=linear;
    lease->samples.assign(samples.begin(),samples.end());lease->footprint=Footprint(d,layout);
    for(const auto& sample:samples){
        const auto desc=sample.handle->GetDesc();
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.SampleDesc.Count!=1||
           desc.Width!=d.width||desc.Height!=d.height||desc.DepthOrArraySize!=d.layers||
           desc.MipLevels!=1||desc.Format!=texture.handle->GetDesc().Format||
           !(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
            throw std::invalid_argument("Native MSAA storage sample layout changed");
    }
    lease->reads.reserve(d.layers);lease->writes.reserve(d.layers);
    for(UINT layer=0;layer<d.layers;++layer){
        lease->reads.push_back(Prepare(texture,linear,d,layout,layer,0,false));
        lease->writes.push_back(Prepare(texture,linear,d,layout,layer,0,true));
    }
    return lease;
}
void GuestMultisample::StorageLease::Record(ID3D12GraphicsCommandList* list,Resource& ms,
    std::span<Resource> storage,Resource& buffer,bool writeback)const{
    if(ms.handle.Get()!=texture.handle.Get()||buffer.handle.Get()!=linear.handle.Get()||storage.size()!=samples.size())
        throw std::invalid_argument("Native MSAA storage transfer resources changed");
    for(size_t i=0;i<storage.size();++i)if(storage[i].handle.Get()!=samples[i].handle.Get())
        throw std::invalid_argument("Native MSAA storage sample resource changed");
    const auto sample_bytes=uint64_t(footprint.Footprint.RowPitch)*footprint.Footprint.Height;
    for(UINT layer=0;layer<reads.size();++layer){
        if(!writeback)reads[layer]->Record(list,ms,buffer);
        buffer.Transition(list,writeback?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_COPY_SOURCE);
        for(UINT sample=0;sample<storage.size();++sample){
            auto& resource=storage[sample];
            resource.Transition(list,writeback?D3D12_RESOURCE_STATE_COPY_SOURCE:D3D12_RESOURCE_STATE_COPY_DEST,layer);
            D3D12_TEXTURE_COPY_LOCATION tex{};tex.pResource=resource.handle.Get();
            tex.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;tex.SubresourceIndex=layer;
            D3D12_TEXTURE_COPY_LOCATION bytes{};bytes.pResource=buffer.handle.Get();
            bytes.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;bytes.PlacedFootprint=footprint;
            bytes.PlacedFootprint.Offset=sample*sample_bytes;
            DX12_API_CALL(list->CopyTextureRegion(writeback?&bytes:&tex,0,0,0,writeback?&tex:&bytes,nullptr));
        }
        if(writeback)writes[layer]->Record(list,ms,buffer);
    }
    if(!writeback)for(auto& sample:storage)sample.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}
std::shared_ptr<GuestMultisample::CopyLease> GuestMultisample::PrepareCopy(
    Resource& source,Resource& destination,const GuestTextureDesc& d,
    const GuestTextureLayout& layout,UINT layer){
    const auto src=source.handle->GetDesc(),dst=destination.handle->GetDesc();
    if(d.depth_target||d.levels!=1||d.volume_depth||d.one_dimensional||layer>=d.layers||
       layout.block_extent!=1||src.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       dst.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||src.Width!=dst.Width||src.Height!=dst.Height||
       layer>=src.DepthOrArraySize||layer>=dst.DepthOrArraySize||
       !(dst.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||src.Format!=dst.Format)
        throw std::invalid_argument("Invalid native sample backing copy resources");
    const auto format=!d.writable&&d.data_format>=16&&d.data_format<=19?layout.format:TransferFormat(d,layout);
    if((d.data_format==6||d.data_format==34)&&
       (!source.AllowsCast(DXGI_FORMAT_R32_UINT)||!destination.AllowsCast(DXGI_FORMAT_R32_UINT)))
        throw std::runtime_error("Native packed-float sample copy requires raw views on both resources");
    const bool source_msaa=src.SampleDesc.Count>1;
    auto& program=copy_programs_[{format,dst.SampleDesc.Count,source_msaa}];
    if(!program){
        auto compiled=std::make_shared<Program>();
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
        D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable={1,&range};parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC signature{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&compiled->root))));
        const D3D_SHADER_MACRO macros[]{{"VALUE_TYPE",ValueType(format)},
            {"SOURCE_MSAA",source_msaa?"1":"0"},{nullptr,nullptr}};
        const auto vs=Compile("float4 main(uint id:SV_VertexID):SV_Position{return float4(id==2?3:-1,id==1?3:-1,0,1);}","vs_5_1");
        const auto ps=Compile(GuestMsaaCopySource,"ps_5_1",macros);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=compiled->root.Get();
        state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=dst.SampleDesc.Count>1;
        state.SampleMask=UINT_MAX;state.SampleDesc=dst.SampleDesc;
        state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.NumRenderTargets=1;state.RTVFormats[0]=format;
        state.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
        Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&compiled->pipeline))));
        program=std::move(compiled);
    }
    auto lease=std::make_shared<CopyLease>();lease->source=source;lease->destination=destination;
    lease->program=program;lease->width=d.width;lease->height=d.height;lease->layer=layer;
    lease->descriptors=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
    lease->attachment=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=format;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if(source_msaa){srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;srv.Texture2DMSArray={layer,1};}
    else{srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray.MipLevels=1;
        srv.Texture2DArray.FirstArraySlice=layer;srv.Texture2DArray.ArraySize=1;}
    DX12_API_CALL(device_->CreateShaderResourceView(source.handle.Get(),&srv,lease->descriptors->Cpu(0)));
    D3D12_RENDER_TARGET_VIEW_DESC rtv{};rtv.Format=format;
    if(dst.SampleDesc.Count>1){rtv.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;rtv.Texture2DMSArray={layer,1};}
    else{rtv.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DARRAY;rtv.Texture2DArray.FirstArraySlice=layer;rtv.Texture2DArray.ArraySize=1;}
    DX12_API_CALL(device_->CreateRenderTargetView(destination.handle.Get(),&rtv,lease->attachment->Cpu(0)));
    return lease;
}
void GuestMultisample::CopyLease::Record(ID3D12GraphicsCommandList* list,
    Resource& src,Resource& dst)const{
    if(src.handle.Get()!=source.handle.Get()||dst.handle.Get()!=destination.handle.Get())
        throw std::invalid_argument("Native sample backing copy resource changed");
    src.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);dst.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
    const auto desc=dst.handle->GetDesc();
    if(desc.Width==width&&desc.Height==height){
        D3D12_DISCARD_REGION region{0,nullptr,layer*UINT(desc.MipLevels),1};
        DX12_API_CALL(list->DiscardResource(dst.handle.Get(),&region));
    }
    ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    DX12_API_CALL(list->SetGraphicsRootSignature(program->root.Get()));DX12_API_CALL(list->SetPipelineState(program->pipeline.Get()));
    DX12_API_CALL(list->SetGraphicsRootDescriptorTable(0,descriptors->Gpu(0)));
    D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
    DX12_API_CALL(list->RSSetViewports(1,&viewport));DX12_API_CALL(list->RSSetScissorRects(1,&scissor));
    DX12_API_CALL(list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
    const auto target=attachment->Cpu(0);DX12_API_CALL(list->OMSetRenderTargets(1,&target,FALSE,nullptr));
    DX12_API_CALL(list->DrawInstanced(3,1,0,0));
}
} // namespace NativeDX12
