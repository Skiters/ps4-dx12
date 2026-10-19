#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_pipeline.h"
#include "guest_graphics_state.h"
#include "rasterizer.h"
#include "texture_resources.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
namespace DXIL=Shader::Backend::DX12;
static ComPtr<ID3D12InfoQueue> debug_queue;
static void CheckGuestColorWriteMasks(){
    // Independent singleton-mask oracle for the pinned ApplyMask scatter.
    // Constant selectors retain their source bit through CompMapping::Map;
    // the four-component format19 rotation is deliberately not its inverse.
    using SwapMasks=std::array<std::array<u8,4>,4>;
    const SwapMasks single{{{1,2,4,8},{2,2,4,8},{4,2,4,8},{1,2,4,8}}};
    const SwapMasks dual{{{1,2,4,8},{1,8,4,8},{2,1,4,8},{8,1,4,8}}};
    const SwapMasks triple{{{1,2,4,8},{1,2,8,8},{4,2,1,8},{8,2,1,8}}};
    const SwapMasks triple_rb{{{4,2,1,8},{8,2,1,8},{1,2,4,8},{1,2,8,8}}};
    const SwapMasks rgba{{{1,2,4,8},{4,2,1,8},{8,4,2,1},{8,1,2,4}}};
    const SwapMasks rgba_reverse{{{8,4,2,1},{8,1,2,4},{1,2,4,8},{4,2,1,8}}};
    const SwapMasks rgba_rb{{{4,2,1,8},{1,2,4,8},{2,4,8,1},{2,1,8,4}}};
    const SwapMasks rgba_rotate{{{2,4,8,1},{2,1,8,4},{4,2,1,8},{1,2,4,8}}};
    struct Case {u32 format;DXGI_FORMAT host;const SwapMasks* masks;};
    const Case cases[]{
        {1,DXGI_FORMAT_R8_UNORM,&single},{2,DXGI_FORMAT_R16_UNORM,&single},
        {3,DXGI_FORMAT_R8G8_UNORM,&dual},{4,DXGI_FORMAT_R32_FLOAT,&single},
        {5,DXGI_FORMAT_R16G16_UNORM,&dual},{6,DXGI_FORMAT_R11G11B10_FLOAT,&triple},
        {7,DXGI_FORMAT_R11G11B10_FLOAT,&triple_rb},{8,DXGI_FORMAT_R10G10B10A2_UNORM,&rgba_reverse},
        {9,DXGI_FORMAT_R10G10B10A2_UNORM,&rgba},{10,DXGI_FORMAT_R8G8B8A8_UNORM,&rgba},
        {11,DXGI_FORMAT_R32G32_FLOAT,&dual},{12,DXGI_FORMAT_R16G16B16A16_UNORM,&rgba},
        {13,DXGI_FORMAT_R32G32B32A32_FLOAT,&triple},{14,DXGI_FORMAT_R32G32B32A32_FLOAT,&rgba},
        {16,DXGI_FORMAT_B5G6R5_UNORM,&triple_rb},{17,DXGI_FORMAT_B5G5R5A1_UNORM,&rgba_rb},
        {18,DXGI_FORMAT_B5G5R5A1_UNORM,&rgba},{19,DXGI_FORMAT_B4G4R4A4_UNORM,&rgba_rotate},
    };
    AmdGpu::Regs regs{};regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_shader_mask.SetMask(0,15);
    for(const auto& c:cases)for(u32 swap=0;swap<4;++swap)for(u32 mask=0;mask<16;++mask){
        regs.color_buffers[0].info.format=c.format;
        regs.color_buffers[0].info.comp_swap=static_cast<decltype(regs.color_buffers[0].info.comp_swap)>(swap);
        regs.color_target_mask.SetMask(0,mask);
        u8 expected{};for(u32 bit=0;bit<4;++bit)if(mask&(1u<<bit))expected|=(*c.masks)[swap][bit];
        const std::array<DXGI_FORMAT,1> formats{c.host};
        const auto state=BuildGuestGraphicsState(regs,formats);
        if(state.pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask!=expected)
            throw std::runtime_error("Guest color write-mask scatter mismatch: format="+std::to_string(c.format)+
                " swap="+std::to_string(swap)+" mask="+std::to_string(mask));
    }
    const std::array<DXGI_FORMAT,2> formats{DXGI_FORMAT_UNKNOWN,DXGI_FORMAT_B4G4R4A4_UNORM};
    regs.color_buffers[1].info.format=19;regs.color_target_mask.SetMask(0,15);regs.color_target_mask.SetMask(1,1);
    auto state=BuildGuestGraphicsState(regs,formats);
    if(state.pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask||
       state.pipeline.BlendState.RenderTarget[1].RenderTargetWriteMask!=2)
        throw std::runtime_error("Guest color write-mask changed an attachment hole or wrong MRT slot");
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Disable;
    if(BuildGuestGraphicsState(regs,formats).pipeline.BlendState.RenderTarget[1].RenderTargetWriteMask)
        throw std::runtime_error("Disabled guest color writes survived component mapping");
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    regs.color_buffers[1].info.format=0;regs.color_target_mask.SetMask(1,5);
    if(BuildGuestGraphicsState(regs,formats).pipeline.BlendState.RenderTarget[1].RenderTargetWriteMask!=5)
        throw std::runtime_error("Native-only target without guest CB format lost its explicit mask");
}
int wmain(int argc,wchar_t** argv) {try {
    if(argc!=2)throw std::invalid_argument("usage: native-pipeline-test dxcompiler.dll");
    CheckGuestColorWriteMasks();
    ComPtr<ID3D12Debug> debug;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    auto vertex=DXIL::CompileDxil(argv[1],R"(
float4 main(uint id:SV_VertexID):SV_Position {
    return float4(id==1?3:-1,id==2?3:-1,0,1);
})",DXIL::Stage::Vertex);
    auto pixel=DXIL::CompileDxil(argv[1],R"(
cbuffer Tint:register(b3,space2) {float4 tint;};
Texture2D<float4> image:register(t5,space4);
SamplerState filter:register(s7,space6);
float4 main():SV_Target0 {return image.SampleLevel(filter,float2(0.5,0.5),0)*tint;}
)",DXIL::Stage::Pixel);
    if(!vertex.success||!pixel.success)throw std::runtime_error(vertex.diagnostics+pixel.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> adapter;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);
        DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(adapter->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;
    }
    if(!device)throw std::runtime_error("No hardware D3D12 device");
    device.As(&debug_queue);
    AmdGpu::Regs regs{};regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.SetMask(0,15);
    regs.color_shader_mask.SetMask(0,15);
    regs.blend_control[0].enable=1;
    const auto original_format=regs.color_buffers[0].info.format;
    regs.color_buffers[0].info.format=u32(AmdGpu::DataFormat::Format32_32_32);
    regs.blend_control[0].color_src_factor=AmdGpu::BlendControl::BlendFactor::DstAlpha;
    regs.blend_control[0].color_dst_factor=AmdGpu::BlendControl::BlendFactor::OneMinusDstAlpha;
    const DXGI_FORMAT rgb96_formats[]{DXGI_FORMAT_R32G32B32A32_FLOAT};
    const auto rgb96_blend=BuildGuestGraphicsState(regs,rgb96_formats);
    if(rgb96_blend.pipeline.BlendState.RenderTarget[0].SrcBlend!=D3D12_BLEND_ONE||
       rgb96_blend.pipeline.BlendState.RenderTarget[0].DestBlend!=D3D12_BLEND_ZERO)
        throw std::runtime_error("RGB96 guest destination alpha must remain one");
    regs.color_buffers[0].info.format=original_format;
    regs.blend_control[0].color_src_factor=AmdGpu::BlendControl::BlendFactor::SrcAlpha;
    regs.blend_control[0].color_dst_factor=AmdGpu::BlendControl::BlendFactor::OneMinusSrcAlpha;
    const DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM};
    auto guest_state=BuildGuestGraphicsState(regs,formats);const auto& fixed=guest_state.pipeline;
    if(guest_state.topology!=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST||!fixed.BlendState.RenderTarget[0].BlendEnable||
        fixed.BlendState.RenderTarget[0].SrcBlendAlpha!=D3D12_BLEND_SRC_ALPHA)
        throw std::runtime_error("Guest blend/topology state mapping mismatch");
    regs.color_shader_mask.SetMask(0,7);
    const auto masked_alpha=BuildGuestGraphicsState(regs,formats);
    if(masked_alpha.pipeline.BlendState.RenderTarget[0].SrcBlend!=D3D12_BLEND_ONE||
       masked_alpha.pipeline.BlendState.RenderTarget[0].DestBlend!=D3D12_BLEND_ZERO||
       masked_alpha.pipeline.BlendState.RenderTarget[0].SrcBlendAlpha!=D3D12_BLEND_SRC_ALPHA)
        throw std::runtime_error("PS4 masked alpha must default to one for color blending only");
    regs.color_shader_mask.SetMask(0,15);
    regs.blend_control[0].color_src_factor=AmdGpu::BlendControl::BlendFactor::ConstantAlpha;
    regs.blend_control[0].color_dst_factor=AmdGpu::BlendControl::BlendFactor::OneMinusConstantAlpha;
    const auto constant_alpha=BuildGuestGraphicsState(regs,formats);
    if(!constant_alpha.blend_constant_alpha_rgb||
       constant_alpha.pipeline.BlendState.RenderTarget[0].SrcBlend!=D3D12_BLEND_BLEND_FACTOR||
       constant_alpha.pipeline.BlendState.RenderTarget[0].DestBlend!=D3D12_BLEND_INV_BLEND_FACTOR)
        throw std::runtime_error("PS4 constant-alpha RGB blend mapping mismatch");
    regs.blend_control[0].color_src_factor=AmdGpu::BlendControl::BlendFactor::SrcAlpha;
    regs.blend_control[0].color_dst_factor=AmdGpu::BlendControl::BlendFactor::OneMinusSrcAlpha;
    regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;
    regs.depth_buffer.z_read_base=regs.depth_buffer.z_write_base=1;
    regs.depth_buffer.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;
    regs.depth_control.depth_func=AmdGpu::CompareFunc::LessEqual;
    auto depth_state=BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT);
    if(depth_state.pipeline.DepthStencilState.DepthFunc!=D3D12_COMPARISON_FUNC_LESS_EQUAL||
        depth_state.pipeline.DepthStencilState.DepthWriteMask!=D3D12_DEPTH_WRITE_MASK_ALL)
        throw std::runtime_error("Guest depth state mapping mismatch");
    regs.depth_control.enable_color_writes_on_depth_fail=1;
    regs.depth_control.disable_color_writes_on_depth_pass=1;
    const auto conditional_color=BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT);
    if(!conditional_color.pipeline.DepthStencilState.DepthEnable||
       !conditional_color.pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask)
        throw std::runtime_error("DB compatibility flags incorrectly disabled a Vulkan-compatible draw");
    regs.depth_control.enable_color_writes_on_depth_fail=0;
    regs.depth_control.disable_color_writes_on_depth_pass=0;
    // Actual game failure: stencil enable survives a depth-only attachment.
    regs.depth_control.stencil_enable=1;
    auto depth_only=BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT);
    if(depth_only.pipeline.DepthStencilState.StencilEnable||!depth_only.pipeline.DepthStencilState.DepthEnable)
        throw std::runtime_error("Stale stencil enable bit changed depth-only semantics");
    regs.depth_buffer.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;
    if(BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT).pipeline.DepthStencilState.StencilEnable)
        throw std::runtime_error("Stencil format without an address incorrectly enabled the test");
    regs.depth_buffer.stencil_read_base=regs.depth_buffer.stencil_write_base=2;
    if(!BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT_S8X24_UINT).pipeline.DepthStencilState.StencilEnable)
        throw std::runtime_error("Valid stencil plane was disabled");
    regs.depth_control.depth_enable=0;regs.stencil_ref_front.stencil_write_mask=0xff;
    regs.stencil_ref_front.stencil_op_val=0x55;
    regs.stencil_control.stencil_zpass_front=AmdGpu::StencilFunc::And;
    const auto bitwise_stencil=BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT_S8X24_UINT);
    if(bitwise_stencil.pipeline.DepthStencilState.FrontFace.StencilPassOp!=D3D12_STENCIL_OP_KEEP)
        throw std::runtime_error("Arbitrary bitwise stencil compatibility fallback mismatch");
    regs.depth_control.depth_enable=1;regs.stencil_ref_front={};regs.stencil_control={};
    bool bad_format=false;try{BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_D32_FLOAT);}
        catch(const std::runtime_error&){bad_format=true;}
    if(!bad_format)throw std::runtime_error("Valid stencil accepted a mismatched native attachment");
    regs.depth_buffer={};regs.depth_control.depth_bounds_enable=1;
    auto unbound=BuildGuestGraphicsState(regs,formats);
    if(unbound.pipeline.DepthStencilState.DepthEnable||unbound.pipeline.DepthStencilState.StencilEnable||
       unbound.depth_bounds||unbound.WritesDepthStencil()||GuestDepthTests(regs).HasAttachment())
        throw std::runtime_error("Unbound planes retained active guest depth/stencil state");
    regs.depth_control.stencil_enable=regs.depth_control.depth_bounds_enable=0;
    regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=0;
    regs.polygon_control.enable_polygon_offset_back=1;
    regs.poly_offset.back_offset=-12;regs.poly_offset.back_scale=32;regs.poly_offset.depth_bias=0.125f;
    auto biased=BuildGuestGraphicsState(regs,formats);
    if(biased.dynamic_depth_bias||biased.pipeline.RasterizerState.DepthBias!=-12||
       biased.pipeline.RasterizerState.SlopeScaledDepthBias!=2||biased.pipeline.RasterizerState.DepthBiasClamp!=0.125f)
        throw std::runtime_error("Guest back-face depth bias mapping mismatch");
    regs.polygon_control.enable_polygon_offset_front=1;
    regs.poly_offset.front_offset=1.5f;regs.poly_offset.front_scale=8;
    biased=BuildGuestGraphicsState(regs,formats);
    if(!biased.dynamic_depth_bias||biased.depth_bias!=1.5f||biased.slope_depth_bias!=0.5f||
       !(biased.pipeline.Flags&D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS))
        throw std::runtime_error("Guest fractional/front-face depth bias mapping mismatch");
    regs.poly_offset.front_offset=std::numeric_limits<float>::infinity();bool invalid_bias=false;
    try{BuildGuestGraphicsState(regs,formats);}catch(const std::runtime_error&){invalid_bias=true;}
    if(!invalid_bias)throw std::runtime_error("Non-finite guest depth bias accepted");
    regs.polygon_control.enable_polygon_offset_front=regs.polygon_control.enable_polygon_offset_back=0;
    regs.polygon_control.enable_polygon_mode=1;
    regs.polygon_control.polygon_mode_front=AmdGpu::PolygonMode::Point;
    regs.polygon_control.polygon_mode_back=AmdGpu::PolygonMode::Fill;
    bool point_rejected=false;
    try{BuildGuestGraphicsState(regs,formats);}catch(const std::runtime_error&){point_rejected=true;}
    if(!point_rejected)throw std::runtime_error("Guest point polygons silently approximated before native rasterization integration");
    const auto previous_primitive=regs.primitive_type;
    regs.primitive_type=AmdGpu::PrimitiveType::PointList;
    regs.stage_enable.raw=AmdGpu::ShaderStageEnable::VgtStages::Vs;
    const auto unit_points=BuildGuestGraphicsState(regs,formats);
    if(unit_points.topology!=D3D_PRIMITIVE_TOPOLOGY_POINTLIST||
       unit_points.pipeline.RasterizerState.FillMode!=D3D12_FILL_MODE_SOLID)
        throw std::runtime_error("Native unit points were affected by polygon fill mode");
    regs.vs_output_control.vs_out_misc_enable=regs.vs_output_control.use_vtx_point_size=1;
    const auto sized_points=BuildGuestGraphicsState(regs,formats);
    if(sized_points.topology!=D3D_PRIMITIVE_TOPOLOGY_POINTLIST||
       sized_points.pipeline.RasterizerState.FillMode!=D3D12_FILL_MODE_SOLID||
       sized_points.pipeline.RasterizerState.CullMode!=D3D12_CULL_MODE_NONE)
        throw std::runtime_error("Sized guest point state did not preserve point rasterization rules");
    regs.vs_output_control.vs_out_misc_enable=regs.vs_output_control.use_vtx_point_size=0;
    regs.stage_enable.raw=AmdGpu::ShaderStageEnable::VgtStages::EsGs;
    regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.vgt_gs_out_prim_type.outprim_type=AmdGpu::GsOutputPrimitiveType::PointList;
    regs.polygon_control.cull_front=regs.polygon_control.cull_back=1;
    const auto geometry_points=BuildGuestGraphicsState(regs,formats);
    if(geometry_points.topology!=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST||
       geometry_points.pipeline.RasterizerState.FillMode!=D3D12_FILL_MODE_SOLID||
       geometry_points.pipeline.RasterizerState.CullMode!=D3D12_CULL_MODE_NONE||geometry_points.rasterizer_discard)
        throw std::runtime_error("Guest GS point output inherited polygon culling/fill rules");
    regs.vgt_gs_out_prim_type.outprim_type=AmdGpu::GsOutputPrimitiveType::LineStrip;
    const auto geometry_lines=BuildGuestGraphicsState(regs,formats);
    if(geometry_lines.discard||geometry_lines.rasterizer_discard||
       geometry_lines.pipeline.RasterizerState.CullMode!=D3D12_CULL_MODE_NONE||
       geometry_lines.pipeline.RasterizerState.FillMode!=D3D12_FILL_MODE_SOLID)
        throw std::runtime_error("Guest GS lines inherited triangle input culling/fill rules");
    regs.vgt_gs_out_prim_type.outprim_type=AmdGpu::GsOutputPrimitiveType::TriangleStrip;
    const auto geometry_triangles=BuildGuestGraphicsState(regs,formats);
    if(geometry_triangles.discard||!geometry_triangles.rasterizer_discard||
       geometry_triangles.pipeline.StreamOutput.RasterizedStream!=D3D12_SO_NO_RASTERIZED_STREAM)
        throw std::runtime_error("Guest GS triangles escaped dual-face culling");
    regs.stage_enable.raw=AmdGpu::ShaderStageEnable::VgtStages::LsHs;
    regs.primitive_type=AmdGpu::PrimitiveType::PatchPrimitive;
    regs.tess_config.type=AmdGpu::TessellationType::Isoline;
    regs.tess_config.topology=AmdGpu::TessellationTopology::Line;
    const auto tessellation_lines=BuildGuestGraphicsState(regs,formats,DXGI_FORMAT_UNKNOWN,3);
    if(tessellation_lines.discard||tessellation_lines.rasterizer_discard||
       tessellation_lines.pipeline.RasterizerState.CullMode!=D3D12_CULL_MODE_NONE||
       tessellation_lines.pipeline.RasterizerState.FillMode!=D3D12_FILL_MODE_SOLID)
        throw std::runtime_error("Guest tessellation isolines inherited polygon culling/fill rules");
    regs.stage_enable.raw=AmdGpu::ShaderStageEnable::VgtStages::Vs;
    regs.polygon_control.cull_front=regs.polygon_control.cull_back=0;
    regs.primitive_type=previous_primitive;
    regs.polygon_control.enable_polygon_mode=0;
    regs.primitive_type=AmdGpu::PrimitiveType::RectList;
    if(BuildGuestGraphicsState(regs,formats).topology!=D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST)
        throw std::runtime_error("Guest rectangle patch topology mismatch");
    regs.primitive_type=AmdGpu::PrimitiveType::QuadList;
    if(BuildGuestGraphicsState(regs,formats).topology!=D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST)
        throw std::runtime_error("Guest quad patch topology mismatch");
    regs.primitive_type=AmdGpu::PrimitiveType::RectList;
    for(unsigned faces=1;faces<4;++faces){
        regs.polygon_control.cull_front=faces&1;regs.polygon_control.cull_back=(faces>>1)&1;
        const auto rectangle=BuildGuestGraphicsState(regs,formats);
        if(rectangle.discard||rectangle.rasterizer_discard||
           rectangle.pipeline.RasterizerState.CullMode!=D3D12_CULL_MODE_NONE)
            throw std::runtime_error("Guest screen-aligned rectangles inherited polygon culling");
    }
    regs.polygon_control.cull_front=regs.polygon_control.cull_back=1;regs.primitive_type=AmdGpu::PrimitiveType::LineList;
    const auto culled_lines=BuildGuestGraphicsState(regs,formats);
    if(culled_lines.discard||culled_lines.rasterizer_discard)
        throw std::runtime_error("Guest polygon culling incorrectly killed lines");
    regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    const auto culled_triangles=BuildGuestGraphicsState(regs,formats);
    if(culled_triangles.discard||!culled_triangles.rasterizer_discard||
       culled_triangles.pipeline.StreamOutput.RasterizedStream!=D3D12_SO_NO_RASTERIZED_STREAM)
        throw std::runtime_error("Guest dual-face culling must retain shaders and discard only rasterization");
    ShaderPipeline pipeline(device.Get(),argv[1],{vertex.bytecode,pixel.bytecode},fixed);
    ResourceBindings bindings(device.Get(),pipeline.Layout());
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};
    Check(DX12_API_CALL(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))));
    ComPtr<ID3D12CommandAllocator> allocator;Check(DX12_API_CALL(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))));
    ComPtr<ID3D12GraphicsCommandList> list;Check(DX12_API_CALL(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))));
    auto target=Texture2D(device.Get(),8,8,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto source=Texture2D(device.Get(),1,1,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);
    auto upload=TextureTransfer::Create(device.Get(),source,false);
    const uint8_t texel[4]{128,64,255,255};upload.WriteRows(texel,sizeof(texel),4);upload.Upload(list.Get(),source);
    source.Transition(list.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    auto constants=Buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* data{};D3D12_RANGE empty{};Check(DX12_API_CALL(constants.handle->Map(0,&empty,&data)));memset(data,0,256);
    const float tint[4]{0.5f,1.f,0.25f,1.f};memcpy(data,tint,sizeof(tint));DX12_API_CALL(constants.handle->Unmap(0,nullptr));
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    bindings.Srv(5,4,source.handle.Get(),srv);bindings.Cbv(3,2,constants.handle.Get(),0,256);
    bool missing_rejected=false;try{pipeline.Bind(list.Get(),bindings);}catch(const std::logic_error&){missing_rejected=true;}
    if(!missing_rejected)throw std::runtime_error("Uninitialized sampler was accepted");
    D3D12_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    bindings.Sampler(7,6,sampler);pipeline.Bind(list.Get(),bindings);
    bool sealed_rejected=false;try{bindings.Sampler(7,6,sampler);}catch(const std::logic_error&){sealed_rejected=true;}
    if(!sealed_rejected)throw std::runtime_error("In-flight descriptor mutation was accepted");
    // Binding ownership must keep descriptors' resources alive even after local owners release them.
    source.handle.Reset();constants.handle.Reset();
    Descriptors rtv(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);auto slot=rtv.Allocate(0);
    auto handle=rtv.Cpu(slot);DX12_API_CALL(device->CreateRenderTargetView(target.handle.Get(),nullptr,handle));
    D3D12_VIEWPORT viewport{0,0,8,8,0,1};D3D12_RECT scissor{0,0,8,8};
    DX12_API_CALL(list->RSSetViewports(1,&viewport));DX12_API_CALL(list->RSSetScissorRects(1,&scissor));DX12_API_CALL(list->OMSetRenderTargets(1,&handle,FALSE,nullptr));
    const float clear[4]{};DX12_API_CALL(list->ClearRenderTargetView(handle,clear,0,nullptr));
    Check(DX12_API_CALL(list->Close()));Timeline timeline(device.Get());timeline.Wait(timeline.Submit(queue.Get(),list.Get()));
    Rasterizer rasterizer(device.Get());const Rasterizer::ColorAttachment attachments[]{{&target}};
    const float blend[4]{1,1,1,1};
    rasterizer.ExecuteGraphics(pipeline,bindings,guest_state,attachments,nullptr,
        std::span(&viewport,1),std::span(&scissor,1),blend,{3});
    auto readback=TextureTransfer::Create(device.Get(),target,true);readback.Readback(rasterizer.RecordingList(),target);rasterizer.Finish();
    uint8_t pixels[8*8*4]{};readback.ReadRows(pixels,sizeof(pixels),8*4);
    const uint8_t expected[]{64,64,64,255};
    for(size_t i=0;i<sizeof(pixels);++i)if(abs(int(pixels[i])-expected[i%4])>1)
        throw std::runtime_error("Native sampled draw mismatch at byte "+std::to_string(i));
    // Actual GPU regression: a shader outputting alpha=0 must still draw RGB
    // when the PS4 export mask disables alpha and the blend uses source alpha.
    auto zero_alpha_pixel=DXIL::CompileDxil(argv[1],
        "float4 main():SV_Target0 { return float4(0.25,0.25,0.25,0); }",DXIL::Stage::Pixel);
    if(!zero_alpha_pixel.success)throw std::runtime_error(zero_alpha_pixel.diagnostics);
    ShaderPipeline masked_pipeline(device.Get(),argv[1],{vertex.bytecode,zero_alpha_pixel.bytecode},masked_alpha.pipeline);
    ResourceBindings masked_bindings(device.Get(),masked_pipeline.Layout());
    target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
    DX12_API_CALL(rasterizer.RecordingList()->ClearRenderTargetView(handle,clear,0,nullptr));
    rasterizer.ExecuteGraphics(masked_pipeline,masked_bindings,masked_alpha,attachments,nullptr,
        std::span(&viewport,1),std::span(&scissor,1),blend,{3});
    readback.Readback(rasterizer.RecordingList(),target);rasterizer.Finish();
    readback.ReadRows(pixels,sizeof(pixels),8*4);
    const uint8_t masked_expected[]{64,64,64,0};
    for(size_t i=0;i<sizeof(pixels);++i)if(abs(int(pixels[i])-masked_expected[i%4])>1)
        throw std::runtime_error("Masked alpha made a GPU color pass invisible at byte "+std::to_string(i));
    // Unequal RGB/A constants catch the previous alpha-to-RGB workaround
    // corrupting simultaneous constant-color and constant-alpha factors.
    auto blend_pixel=DXIL::CompileDxil(argv[1],
        "struct Out {float4 a:SV_Target0;float4 b:SV_Target1;}; Out main(){Out o;o.a=o.b=0.5;return o;}",DXIL::Stage::Pixel);
    if(!blend_pixel.success)throw std::runtime_error(blend_pixel.diagnostics);
    AmdGpu::Regs blend_regs{};blend_regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    blend_regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    blend_regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    using BF=AmdGpu::BlendControl::BlendFactor;
    for(UINT slot=0;slot<2;++slot){
        blend_regs.color_target_mask.SetMask(slot,15);blend_regs.color_shader_mask.SetMask(slot,15);
        blend_regs.color_buffers[slot].base_address=slot+1;
        blend_regs.color_buffers[slot].info.format=10;
        blend_regs.color_buffers[slot].info.number_type=u32(AmdGpu::NumberFormat::Unorm);
        auto& bc=blend_regs.blend_control[slot];bc.enable=1;bc.separate_alpha_blend=1;
        bc.alpha_src_factor=BF::One;bc.alpha_dst_factor=BF::Zero;
    }
    blend_regs.blend_control[0].color_src_factor=BF::ConstantColor;
    blend_regs.blend_control[0].color_dst_factor=BF::ConstantAlpha;
    blend_regs.blend_control[1].color_src_factor=BF::OneMinusConstantAlpha;
    blend_regs.blend_control[1].color_dst_factor=BF::OneMinusConstantColor;
    D3D12_FEATURE_DATA_D3D12_OPTIONS13 blend_options{};
    const bool native_alpha=SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS13,
        &blend_options,sizeof(blend_options)))&&blend_options.AlphaBlendFactorSupported;
    auto second_target=Texture2D(device.Get(),8,8,DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
    Descriptors second_rtv(device.Get(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);auto second_slot=second_rtv.Allocate(0);
    const auto second_handle=second_rtv.Cpu(second_slot);
    device->CreateRenderTargetView(second_target.handle.Get(),nullptr,second_handle);
    const DXGI_FORMAT blend_formats[]{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM};
    const auto ordered_fallback=BuildGuestGraphicsState(
        blend_regs,blend_formats,DXGI_FORMAT_UNKNOWN,0,false);
    if(ordered_fallback.ordered_color_mask!=3||ordered_fallback.pipeline.NumRenderTargets||
       ordered_fallback.blend_constant_alpha_rgb)
        throw std::runtime_error("Mixed constant blend did not select exact ordered RGBA8 fallback");
    const Rasterizer::ColorAttachment blend_targets[]{{&target},{&second_target}};
    const float blend_values[]{.2f,.4f,.6f,.8f},background[]{.25f,.25f,.25f,.25f};
    const auto draw_blend=[&](const GuestGraphicsState& state,const std::array<std::array<float,4>,2>& expected){
        ShaderPipeline p(device.Get(),argv[1],{vertex.bytecode,blend_pixel.bytecode},state.pipeline);
        ResourceBindings table(device.Get(),p.Layout());
        target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        second_target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        rasterizer.RecordingList()->ClearRenderTargetView(handle,background,0,nullptr);
        rasterizer.RecordingList()->ClearRenderTargetView(second_handle,background,0,nullptr);
        rasterizer.ExecuteGraphics(p,table,state,blend_targets,nullptr,std::span(&viewport,1),
            std::span(&scissor,1),blend_values,{3});
        for(size_t slot=0;slot<2;++slot){
            auto& output=slot?second_target:target;
            auto rb=TextureTransfer::Create(device.Get(),output,true);
            rb.Readback(rasterizer.RecordingList(),output);rasterizer.Finish();rb.ReadRows(pixels,sizeof(pixels),32);
            for(size_t i=0;i<sizeof(pixels);++i)if(std::abs(float(pixels[i])-expected[slot][i%4]*255.f)>2.f)
                throw std::runtime_error("Constant blend GPU result differs at target "+std::to_string(slot)+" byte "+std::to_string(i));
        }
    };
    if(native_alpha){
        const auto state=BuildGuestGraphicsState(blend_regs,blend_formats,DXGI_FORMAT_UNKNOWN,0,true);
        if(state.blend_constant_alpha_rgb)throw std::runtime_error("Native alpha factor rewrote RGB constants");
        draw_blend(state,{{{{.3f,.4f,.5f,.5f}},{{.3f,.25f,.2f,.5f}}}});
        puts("PASS: native ALPHA_FACTOR and INV_ALPHA_FACTOR with simultaneous constant-color MRT blending");
    }else puts("SKIP: GPU lacks OPTIONS13 alpha blend factors; legacy alpha-only path still tested");
    // The baseline path must also apply its dynamic constant conversion in
    // ExecuteGraphics, not just in one caller of that shared submission path.
    for(UINT slot=0;slot<2;++slot){auto& bc=blend_regs.blend_control[slot];
        bc.color_src_factor=BF::ConstantAlpha;bc.color_dst_factor=BF::OneMinusConstantAlpha;}
    draw_blend(BuildGuestGraphicsState(blend_regs,blend_formats),{{{{.45f,.45f,.45f,.5f}},{{.45f,.45f,.45f,.5f}}}});
    // Compare custom-restart lowering against hardware strip assembly. The
    // legal index 0xffff catches accidental conversion to a 16-bit strip cut;
    // flat color also catches an odd triangle's changed leading vertex.
    auto strip_vs=DXIL::CompileDxil(argv[1],R"(
struct Out {float4 position:SV_Position;nointerpolation float4 color:COLOR0;};
Out main(uint id:SV_VertexID){
    id=id==65535?2:id;
    float2 positions[7]={float2(-1,-1),float2(-1,1),float2(1,-1),float2(1,1),
        float2(-1,-1),float2(-1,0),float2(0,-1)};
    Out o;o.position=float4(positions[id],0,1);o.color=float4(float(id%4+1)/4,float(id/4+1)/2,0,1);return o;
})",DXIL::Stage::Vertex);
    auto strip_ps=DXIL::CompileDxil(argv[1],
        "float4 main(float4 position:SV_Position,nointerpolation float4 color:COLOR0):SV_Target0{return color;}",DXIL::Stage::Pixel);
    if(!strip_vs.success||!strip_ps.success)throw std::runtime_error(strip_vs.diagnostics+strip_ps.diagnostics);
    const auto draw_strip=[&](std::span<const u32> indices,bool expanded){
        AmdGpu::Regs r{};r.primitive_type=AmdGpu::PrimitiveType::TriangleStrip;r.enable_primitive_restart=1;
        r.index_buffer_type.index_type=AmdGpu::IndexType::Index32;
        r.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;r.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        r.color_target_mask.SetMask(0,15);
        auto state=BuildGuestGraphicsState(r,formats);
        if(expanded){state.topology=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;}
        ShaderPipeline p(device.Get(),argv[1],{strip_vs.bytecode,strip_ps.bytecode},state.pipeline);
        ResourceBindings table(device.Get(),p.Layout());
        const UINT bytes=UINT(indices.size()*(expanded?2:4));
        auto ib=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE empty{};Check(ib.handle->Map(0,&empty,&mapped));
        if(expanded){for(size_t i=0;i<indices.size();++i){const u16 value=u16(indices[i]);memcpy(static_cast<std::byte*>(mapped)+i*2,&value,2);}}
        else memcpy(mapped,indices.data(),bytes);
        ib.handle->Unmap(0,nullptr);table.Retain(ib.handle.Get());
        Rasterizer::IndexBinding binding{&ib,0,bytes,expanded?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT};
        target.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        rasterizer.RecordingList()->ClearRenderTargetView(handle,clear,0,nullptr);
        rasterizer.ExecuteGraphics(p,table,state,attachments,nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{UINT(indices.size())},{},&binding);
        readback.Readback(rasterizer.RecordingList(),target);rasterizer.Finish();
        std::array<uint8_t,256> result{};readback.ReadRows(result.data(),result.size(),32);return result;
    };
    const std::array<u32,8> hardware_strip{0,1,0xffff,3,0xffffffff,4,5,6};
    const std::array<u32,8> guest_strip{0,1,0xffff,3,99,4,5,6};
    const auto reference_strip=draw_strip(hardware_strip,false);
    if(draw_strip(ExpandGuestPrimitive(AmdGpu::PrimitiveType::TriangleStrip,guest_strip,true,99),true)!=reference_strip)
        throw std::runtime_error("Custom primitive restart changed GPU coverage/winding/flat color");
    if(reference_strip[3]!=255)throw std::runtime_error("Strip reference produced no coverage");
    puts("PASS: custom restart triangle-list expansion matches native GPU strip, including index 0xffff and flat interpolation");
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan loader");
    puts("PASS: PS4 register blend/depth/culling/topology mapping, reflected CBV/SRV/sampler, native Rasterizer draw submission; 64 exact pixels; descriptor ownership, no Vulkan");
    return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());
    if(debug_queue)for(UINT64 i=0;i<debug_queue->GetNumStoredMessages();++i){
        SIZE_T size{};debug_queue->GetMessage(i,nullptr,&size);std::vector<std::byte> data(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());
        if(SUCCEEDED(debug_queue->GetMessage(i,message,&size)))fprintf(stderr,"D3D12: %s\n",message->pDescription);
    }
    return 1;}}
