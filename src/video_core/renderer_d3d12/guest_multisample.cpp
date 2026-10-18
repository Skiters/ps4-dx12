// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_multisample.h"
#include "guest_texture_cache.h"
#include "guest_msaa_shaders.h"
#include "video_core/amdgpu/pixel_format.h"
#include <d3dcompiler.h>
#include <string>
#include <numeric>
#include <bit>
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
void ValidateRawRgba8Transfer(const GuestTextureDesc& d,const GuestTextureLayout& layout,DXGI_FORMAT backing){
    if((d.data_format==10||d.data_format==34)&&d.UsesRaw32()&&
       (layout.format!=DXGI_FORMAT_R32_UINT||layout.bytes_per_pixel!=4||layout.HostBytesPerPixel()!=4||
        layout.expanded_bytes_per_pixel||
        (backing!=DXGI_FORMAT_R32_TYPELESS&&backing!=DXGI_FORMAT_R32_UINT)))
        throw std::invalid_argument("Native raw word MSAA needs R32 backing and layout");
}
void ValidateRawPackedTransfer(const GuestTextureDesc& d,const GuestTextureLayout& layout,DXGI_FORMAT backing){
    const bool packed10=d.data_format==9&&d.UsesRawPacked10();
    const bool packed16=d.data_format>=16&&d.data_format<=19&&d.writable;
    if((packed10||packed16)&&
       (layout.format!=DXGI_FORMAT_R32_UINT||layout.bytes_per_pixel!=(packed16?2u:4u)||
        layout.HostBytesPerPixel()!=4||layout.expanded_bytes_per_pixel||
        (backing!=DXGI_FORMAT_R32_TYPELESS&&backing!=DXGI_FORMAT_R32_UINT)))
        throw std::invalid_argument("Native raw packed MSAA needs its exact R32 word layout");
}
void ValidatePackedFloatTransfer(const Resource& texture,const GuestTextureDesc& d,
    const GuestTextureLayout& layout){
    if(d.depth_target||(d.data_format!=6&&d.data_format!=34)||d.UsesRaw32())return;
    const auto format=d.data_format==6?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
    if(d.number_format!=7||layout.format!=format||layout.bytes_per_pixel!=4||
       layout.HostBytesPerPixel()!=4||layout.expanded_bytes_per_pixel||
       texture.handle->GetDesc().Format!=format)
        throw std::invalid_argument("Native packed-float transfer needs its exact guest word layout");
    if(!texture.AllowsCast(format)||!texture.AllowsCast(DXGI_FORMAT_R32_UINT))
        throw std::runtime_error("Native packed-float transfer needs declared typed and R32 UINT views");
}
void ValidateRgb32Transfer(const Resource& texture,const GuestTextureDesc& d,
    const GuestTextureLayout& layout){
    if(d.data_format!=13)return;
    const auto format=d.number_format==4?DXGI_FORMAT_R32G32B32A32_UINT:
        d.number_format==5?DXGI_FORMAT_R32G32B32A32_SINT:
        d.number_format==7?DXGI_FORMAT_R32G32B32A32_FLOAT:DXGI_FORMAT_UNKNOWN;
    if(format==DXGI_FORMAT_UNKNOWN||layout.format!=format||layout.bytes_per_pixel!=12||
       layout.HostBytesPerPixel()!=16||layout.expanded_bytes_per_pixel||
       texture.handle->GetDesc().Format!=DXGI_FORMAT_R32G32B32A32_TYPELESS)
        throw std::invalid_argument("Native RGB32 transfer needs 12-byte guest words and typeless RGBA32 backing");
}
DXGI_FORMAT TransferFormat(const GuestTextureDesc& d,const GuestTextureLayout& layout){
    if(d.depth_target)return LinearFormat(layout.format);
    if(d.data_format>=16&&d.data_format<=19)
        return layout.expanded_bytes_per_pixel==16?DXGI_FORMAT_R32G32B32A32_UINT:
            d.writable?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R16_UINT;
    switch(d.data_format){
    case 1:return DXGI_FORMAT_R8_UINT;
    case 2:return DXGI_FORMAT_R16_UINT;
    case 3:return DXGI_FORMAT_R8G8_UINT;
    case 4:case 6:case 34:return DXGI_FORMAT_R32_UINT;
    case 5:return DXGI_FORMAT_R16G16_UINT;
    case 9:return layout.expanded_bytes_per_pixel==16?DXGI_FORMAT_R32G32B32A32_UINT:
        layout.format==DXGI_FORMAT_R32_UINT?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R10G10B10A2_UINT;
    // Raw storage already packs RGBA into one DWORD. Match its R32 allocation
    // for buffer, sample SRV and RTV access; no color conversion is needed.
    case 10:return d.UsesRaw32()?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R8G8B8A8_UINT;
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
    case DXGI_FORMAT_R10G10B10A2_UINT:
    // Stencil SRVs expose unsigned values in G even though the unused depth
    // channel is typeless. Match the SRV before its G-to-X component mapping.
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:return "uint4";
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
    ValidateRawRgba8Transfer(d,layout,texture_desc.Format);
    ValidateRawPackedTransfer(d,layout,texture_desc.Format);
    ValidatePackedFloatTransfer(texture,d,layout);
    ValidateRgb32Transfer(texture,d,layout);
    if(d.samples<2||d.levels!=1||d.volume_depth||d.one_dimensional||layer>=d.layers||plane>1||
       (plane&&!d.stencil_address)||texture_desc.SampleDesc.Count!=d.samples||
       texture_desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       buffer_desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||layout.block_extent!=1)
        throw std::invalid_argument("Invalid native guest MSAA transfer resources");
    if(!upload&&!(buffer_desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Native MSAA readback buffer does not allow UAV writes");
    const bool depth=d.depth_target,stencil=plane!=0;
    const auto interpreted=stencil?DescribeGuestStencil(d):d;
    const auto physical=stencil?LayoutGuestTexture(interpreted):layout;
    const auto fp=Footprint(interpreted,physical);
    const auto bytes=LinearBytes(interpreted,fp);
    if(bytes>UINT32_MAX||buffer_desc.Width<bytes||buffer_desc.Width%4||
       uint64_t(d.samples)*d.layers>65535)
        throw std::out_of_range("Native guest MSAA linear addressing exceeds buffer");
    const auto buffer_format=TransferFormat(interpreted,physical);
    const bool packed=!d.writable&&!layout.expanded_bytes_per_pixel&&
        d.data_format>=16&&d.data_format<=19;
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
    bool stencil_bit_upload=false;
    if(stencil&&upload){
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&options,sizeof(options))));
        stencil_bit_upload=!options.PSSpecifiedStencilRefSupported;
    }
    const Key key{buffer_format,upload?attachment_format:texture_format,d.samples,depth,stencil,upload};
    auto& program=programs_[key];
    if(!program){
        auto compiled=std::make_shared<Program>();
        compiled->stencil_bit_upload=stencil_bit_upload;
        D3D12_DESCRIPTOR_RANGE ranges[2]{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},
                                        {D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,1}};
        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[0].DescriptorTable={upload?1u:2u,ranges};
        parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants={0,0,stencil_bit_upload?5u:4u};
        D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&compiled->root))));
        const D3D_SHADER_MACRO macros[]{{"VALUE_TYPE",ValueType(buffer_format)},
            {"TEXTURE_TYPE",ValueType(texture_format)},
            {"PACKED_MODE",!packed?"0":d.data_format==16?"1":d.data_format==19?"3":"2"},
            {"NATIVE_DEPTH",depth?"1":"0"},{"NATIVE_STENCIL",stencil?"1":"0"},
            {"STENCIL_BIT_UPLOAD",stencil_bit_upload?"1":"0"},{nullptr,nullptr}};
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
            if(stencil_bit_upload)for(UINT bit=0;bit<8;++bit){
                state.DepthStencilState.StencilWriteMask=UINT8(1u<<bit);
                Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&compiled->stencil_bits[bit]))));
            }
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
        // Placed attachments require initialization before a shader draw.
        // This transfer replaces every sample of one plane in one slice.
        // Initialize only that fully replaced plane; a partial upload must
        // preserve the already initialized pixels outside its viewport.
        const auto desc=source_texture.handle->GetDesc();
        if(desc.Width==constants[0]&&desc.Height==constants[1]){
            if(depth&&desc.MipLevels==1){
                // The DSV covers only this layer. Clearing one flag preserves
                // the other plane, including depth when stencil uploads next.
                // This is host initialization, not guest fast-clear metadata.
                const auto flags=stencil?D3D12_CLEAR_FLAG_STENCIL:D3D12_CLEAR_FLAG_DEPTH;
                DX12_API_CALL(list->ClearDepthStencilView(attachment->Cpu(0),flags,1.0f,0,0,nullptr));
            }else if(!depth){
                D3D12_DISCARD_REGION region{0,nullptr,layer*UINT(desc.MipLevels),1};
                DX12_API_CALL(list->DiscardResource(source_texture.handle.Get(),&region));
            }
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
        if(program->stencil_bit_upload){
            // Full-plane initialization above already set zero. For a partial
            // rectangle, clear only its covered samples before setting bits;
            // every other layer/texel and the complete depth plane stay intact.
            if(desc.Width!=constants[0]||desc.Height!=constants[1]){
                DX12_API_CALL(list->OMSetStencilRef(0));
                DX12_API_CALL(list->SetGraphicsRoot32BitConstant(1,0,4));
                DX12_API_CALL(list->DrawInstanced(3,1,0,0));
            }
            DX12_API_CALL(list->OMSetStencilRef(255));
            for(UINT bit=0;bit<8;++bit){
                DX12_API_CALL(list->SetPipelineState(program->stencil_bits[bit].Get()));
                DX12_API_CALL(list->SetGraphicsRoot32BitConstant(1,1u<<bit,4));
                DX12_API_CALL(list->DrawInstanced(3,1,0,0));
            }
        }else DX12_API_CALL(list->DrawInstanced(3,1,0,0));
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
        ValidatePackedFloatTransfer(sample,d,layout);
        ValidateRgb32Transfer(sample,d,layout);
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
UINT GuestMultisample::SelectBackingSamples(ID3D12Device* device,DXGI_FORMAT format,UINT requested){
    if(!device||format==DXGI_FORMAT_UNKNOWN||!std::has_single_bit(requested)||requested>16)
        throw std::invalid_argument("Invalid native requested sample backing");
    // Pinned LiverpoolToVK::NumSamples chooses a supported physical count;
    // the owning image keeps the requested count as a separate identity.
    for(UINT samples=requested;samples>1;samples/=2){
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{format,samples,D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE,0};
        Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,&quality,sizeof(quality))));
        if(quality.NumQualityLevels)return samples;
    }
    return 1;
}
std::shared_ptr<GuestMultisample::CopyLease> GuestMultisample::PrepareCopy(
    Resource& source,Resource& destination,const GuestTextureDesc& d,
    const GuestTextureLayout& layout,UINT layer){
    const auto src=source.handle->GetDesc(),dst=destination.handle->GetDesc();
    ValidateRawRgba8Transfer(d,layout,src.Format);
    ValidateRawPackedTransfer(d,layout,src.Format);
    ValidatePackedFloatTransfer(source,d,layout);
    ValidatePackedFloatTransfer(destination,d,layout);
    ValidateRgb32Transfer(source,d,layout);
    ValidateRgb32Transfer(destination,d,layout);
    if(d.depth_target||d.levels!=1||d.volume_depth||d.one_dimensional||layer>=d.layers||
       layout.block_extent!=1||src.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       dst.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||src.Width!=dst.Width||src.Height!=dst.Height||
       layer>=src.DepthOrArraySize||layer>=dst.DepthOrArraySize||
       !(dst.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)||src.Format!=dst.Format)
        throw std::invalid_argument("Invalid native sample backing copy resources");
    const auto format=!d.writable&&d.data_format>=16&&d.data_format<=19?layout.format:TransferFormat(d,layout);
    const bool source_msaa=src.SampleDesc.Count>1;
    // Expanded RGB storage physically has A. Keep its format-defined one even
    // for a robust zero fetch; raw UINT transfer views carry float-one bits.
    const bool missing_alpha=layout.HostBytesPerPixel()==16&&
        AmdGpu::NumComponents(AmdGpu::DataFormat(d.data_format))<4;
    const UINT robust_alpha=!source_msaa||!missing_alpha?0:
        std::string_view(ValueType(format))=="uint4"&&d.number_format!=4&&d.number_format!=5?0x3f800000u:1u;
    auto& program=copy_programs_[{format,dst.SampleDesc.Count,source_msaa,robust_alpha}];
    if(!program){
        auto compiled=std::make_shared<Program>();
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
        D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable={1,&range};parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC signature{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&compiled->root))));
        const auto alpha_literal=std::to_string(robust_alpha);
        const D3D_SHADER_MACRO macros[]{{"VALUE_TYPE",ValueType(format)},
            {"SOURCE_MSAA",source_msaa?"1":"0"},{"ROBUST_ALPHA",alpha_literal.c_str()},{nullptr,nullptr}};
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
bool GuestMultisample::CanReinterpretColorAsMsDepth(const GuestTextureDesc& source,
    const GuestTextureLayout& source_layout,const GuestTextureDesc& target,const GuestTextureLayout& target_layout){
    // The pinned helper accepts 2/4/8/16 samples. Its vec4 indices above three
    // are undefined; the native shader writes zero for those samples while
    // retaining the defined channel transfers, S and the full draw operation.
    return !source.depth_target&&!source.stencil_address&&source.samples==1&&
        !source.volume_depth&&!source.one_dimensional&&source.levels&&source.layers&&
        !source.UsesRaw32()&&!source.UsesRawPacked10()&&source_layout.block_extent==1&&
        !source_layout.expanded_bytes_per_pixel&&source_layout.format!=DXGI_FORMAT_UNKNOWN&&
        std::string_view(ValueType(source_layout.format))=="float4"&&
        target.depth_target&&!target.writable&&!target.render_target&&
        !target.volume_depth&&!target.one_dimensional&&target.levels==1&&target.layers&&
        target.samples>1&&target.samples<=16&&std::has_single_bit(target.samples)&&
        ((target.data_format==2&&target.number_format==0&&target_layout.format==DXGI_FORMAT_R16_UNORM)||
         (target.data_format==4&&target.number_format==7&&target_layout.format==DXGI_FORMAT_R32_FLOAT));
}
std::shared_ptr<GuestMultisample::ColorDepthLease> GuestMultisample::PrepareColorAsMsDepth(
    Resource& source,Resource& destination,const GuestTextureDesc& source_desc,
    const GuestTextureLayout& source_layout,const GuestTextureDesc& target_desc,const GuestTextureLayout& target_layout){
    if(!source.handle||!destination.handle||!CanReinterpretColorAsMsDepth(source_desc,source_layout,target_desc,target_layout))
        throw std::invalid_argument("Color-channel depth transfer has no typed source/sample contract");
    const auto src=source.handle->GetDesc(),dst=destination.handle->GetDesc();
    if(source.handle==destination.handle||src.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
       dst.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||src.SampleDesc.Count!=1||
       dst.SampleDesc.Count!=target_desc.samples||dst.MipLevels!=1||
       src.Width<source_desc.width||src.Height<source_desc.height||src.MipLevels<source_desc.levels||
       src.DepthOrArraySize<source_desc.layers||dst.DepthOrArraySize<target_desc.layers||
       dst.Width<target_desc.width||dst.Height<target_desc.height||
       (src.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||!(dst.Flags&D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        throw std::invalid_argument("Color-channel depth transfer resource geometry changed");
    const auto depth_format=target_desc.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        target_layout.bytes_per_pixel==2?DXGI_FORMAT_D16_UNORM:DXGI_FORMAT_D32_FLOAT;
    // Writable sRGB I uses a linear UAV layout, but the pinned blit samples
    // ImageInfo::pixel_format, not that storage view. Restore its typed SRV.
    const auto color_format=source_desc.data_format==10&&source_desc.number_format==9?
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:source_layout.format;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{color_format};
    Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
    if(!(support.Support1&D3D12_FORMAT_SUPPORT1_SHADER_LOAD))
        throw std::runtime_error("Color-channel depth transfer source lacks typed sample loads");
    auto& program=color_depth_programs_[{depth_format,target_desc.samples,dst.SampleDesc.Quality}];
    if(!program){
        auto compiled=std::make_shared<Program>();
        D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
        D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable={1,&range};parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC signature{1,&parameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ComPtr<ID3DBlob> blob,errors;
        Check(DX12_API_CALL(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors)));
        Check(DX12_API_CALL(device_->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&compiled->root))));
        const auto vs=Compile("struct O{float4 p:SV_Position;float2 uv:TEXCOORD0;};"
            "O main(uint id:SV_VertexID){O o;o.uv=float2(id==2?2:0,id==1?2:0);"
            "o.p=float4(o.uv.x*2-1,1-o.uv.y*2,0,1);return o;}","vs_5_1");
        const auto ps=Compile(GuestColorToMsDepthSource,"ps_5_1");
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};state.pRootSignature=compiled->root.Get();
        state.VS={vs->GetBufferPointer(),vs->GetBufferSize()};state.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
        state.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;state.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable=TRUE;state.RasterizerState.MultisampleEnable=TRUE;
        state.SampleMask=UINT_MAX;state.SampleDesc=dst.SampleDesc;state.DSVFormat=depth_format;
        state.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.DepthStencilState.DepthEnable=TRUE;state.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
        state.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
        Check(DX12_API_CALL(device_->CreateGraphicsPipelineState(&state,IID_PPV_ARGS(&compiled->pipeline))));
        program=std::move(compiled);
    }
    auto lease=std::make_shared<ColorDepthLease>();lease->source=source;lease->destination=destination;
    lease->program=program;lease->width=target_desc.width;lease->height=target_desc.height;
    lease->descriptors=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
    lease->attachment=std::make_shared<Descriptors>(device_.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=color_format;
    srv.Shader4ComponentMapping=source_desc.data_format==13?
        D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0,1,2,D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1):
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    // Original views select mip/layer zero and do not inherit guest swizzles.
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;srv.Texture2DArray.MipLevels=1;srv.Texture2DArray.ArraySize=1;
    DX12_API_CALL(device_->CreateShaderResourceView(source.handle.Get(),&srv,lease->descriptors->Cpu(0)));
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};dsv.Format=depth_format;
    dsv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;dsv.Texture2DMSArray.ArraySize=1;
    DX12_API_CALL(device_->CreateDepthStencilView(destination.handle.Get(),&dsv,lease->attachment->Cpu(0)));
    return lease;
}
void GuestMultisample::ColorDepthLease::Record(ID3D12GraphicsCommandList* list,Resource& src,Resource& dst)const{
    if(!list||src.handle.Get()!=source.handle.Get()||dst.handle.Get()!=destination.handle.Get())
        throw std::invalid_argument("Color-channel depth transfer resource changed");
    // Destination was initialized by Upload. Preserve S and every other slice;
    // do not discard the allocation or clear an unrelated plane.
    if(src.state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)src.UavBarrier(list);
    src.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);dst.Transition(list,D3D12_RESOURCE_STATE_DEPTH_WRITE);
    ID3D12DescriptorHeap* heaps[]{descriptors->Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    DX12_API_CALL(list->SetGraphicsRootSignature(program->root.Get()));DX12_API_CALL(list->SetPipelineState(program->pipeline.Get()));
    DX12_API_CALL(list->SetGraphicsRootDescriptorTable(0,descriptors->Gpu(0)));
    D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
    DX12_API_CALL(list->RSSetViewports(1,&viewport));DX12_API_CALL(list->RSSetScissorRects(1,&scissor));
    DX12_API_CALL(list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST));
    const auto target=attachment->Cpu(0);DX12_API_CALL(list->OMSetRenderTargets(0,nullptr,FALSE,&target));
    DX12_API_CALL(list->DrawInstanced(3,1,0,0));
}
} // namespace NativeDX12
