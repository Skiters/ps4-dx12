#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_compute.h"
#include "guest_graphics_state.h"
#include "guest_texture_cache.h"
#include "guest_image_readback.h"
#include "guest_attachment_retirement.h"
#include "guest_depth_metadata.h"
#include "guest_sampler.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "texture_resources.h"
#include "shader_recompiler/backend/dxil/dxil_compiler.h"
#include <dxgi1_6.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <d3d12sdklayers.h>
using namespace NativeDX12;
static ComPtr<ID3D12InfoQueue> debug_queue;
int wmain(int argc,wchar_t** argv){try {
    const std::array<u32,5> sequence{10,11,12,13,14};
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::TriangleFan,sequence)!=
       std::vector<u32>{10,11,12,10,12,13,10,13,14})
        throw std::runtime_error("Triangle-fan expansion mismatch");
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::LineLoop,sequence)!=
       std::vector<u32>{10,11,11,12,12,13,13,14,14,10})
        throw std::runtime_error("Line-loop expansion mismatch");
    const std::array<u32,9> restarted{0,1,2,99,4,5,6,7,8};
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::TriangleFan,restarted,true,99)!=
       std::vector<u32>{0,1,2,4,5,6,4,6,7,4,7,8})
        throw std::runtime_error("Restarted primitive expansion mismatch");
    const std::array<u32,13> strip_restart{0,1,2,3,99,4,5,6,7,8,99,0xffff,10};
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::TriangleStrip,strip_restart,true,99)!=
       std::vector<u32>{0,1,2,1,3,2,4,5,6,5,7,6,6,7,8})
        throw std::runtime_error("Restarted triangle strip winding/partial tail mismatch");
    const std::array<u32,9> partial_lists{0,1,99,0xffff,3,4,99,5,6};
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::TriangleList,partial_lists,true,99)!=
       std::vector<u32>{0xffff,3,4})
        throw std::runtime_error("Restart joined incomplete triangle lists");
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::LineList,partial_lists,true,99)!=
       std::vector<u32>{0,1,0xffff,3,5,6})
        throw std::runtime_error("Restart joined incomplete line lists");
    if(ExpandGuestPrimitive(AmdGpu::PrimitiveType::LineStrip,partial_lists,true,99)!=
       std::vector<u32>{0,1,0xffff,3,3,4,5,6})
        throw std::runtime_error("Custom strip restart lost a legal fixed-cut index");
    AmdGpu::Regs logic_regs{};logic_regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    logic_regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Xor;logic_regs.color_target_mask.raw=0xf;
    const std::array<DXGI_FORMAT,1> logic_format{DXGI_FORMAT_R8G8B8A8_UNORM};
    const auto logic_state=BuildGuestGraphicsState(logic_regs,logic_format);
    if(!logic_state.pipeline.BlendState.RenderTarget[0].LogicOpEnable||
       logic_state.pipeline.BlendState.RenderTarget[0].LogicOp!=D3D12_LOGIC_OP_XOR)
        throw std::runtime_error("Guest raster logic operation mapping mismatch");
    AmdGpu::Sampler sampler_desc{};sampler_desc.raw0=u64(AmdGpu::ClampMode::ClampHalfBorder)|
        (u64(AmdGpu::ClampMode::MirrorOnceBorder)<<3)|(u64(1)<<15);
    sampler_desc.raw1=u64(AmdGpu::BorderColor::Custom)<<62;
    const float custom_border[]{.25f,.5f,.75f,1.f};
    const auto native_sampler=DescribeGuestSampler(sampler_desc,false,custom_border);
    if(native_sampler.AddressU!=D3D12_TEXTURE_ADDRESS_MODE_BORDER||
       native_sampler.AddressV!=D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE||
       !std::equal(std::begin(custom_border),std::end(custom_border),native_sampler.BorderColor))
        throw std::runtime_error("Guest specialized sampler mapping mismatch");
    sampler_desc.raw0=(sampler_desc.raw0&~(u64(3)<<29))|
        (u64(AmdGpu::FilterMode::Max)<<29);
    const auto depth_sampler=DescribeGuestSampler(sampler_desc,true,custom_border);
    if(D3D12_DECODE_FILTER_REDUCTION(depth_sampler.Filter)!=D3D12_FILTER_REDUCTION_TYPE_COMPARISON)
        throw std::runtime_error("Guest depth min/max sampler did not follow Vulkan comparison semantics");
    // Read-only stencil tests must not force a full depth/stencil download.
    // Cover masks, both faces, explicit clears and disabled depth writes.
    GuestGraphicsState readonly;
    auto& readz=readonly.pipeline.DepthStencilState;
    readz.StencilEnable=TRUE;readz.StencilWriteMask=255;
    readz.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_EQUAL};
    readz.BackFace=readz.FrontFace;
    if(readonly.WritesDepthStencil())throw std::runtime_error("Read-only stencil scheduled a writeback");
    readz.BackFace.StencilPassOp=D3D12_STENCIL_OP_REPLACE;
    if(!readonly.WritesDepthStencil())throw std::runtime_error("Back-face stencil write was omitted");
    readz.StencilWriteMask=0;
    if(readonly.WritesDepthStencil())throw std::runtime_error("Zero stencil write mask scheduled a writeback");
    readonly.separate_stencil_faces=true;readonly.stencil_write_mask_front=0;readonly.stencil_write_mask_back=0xff;
    if(!readonly.WritesDepthStencil())throw std::runtime_error("Independent back-face stencil writes lost ownership/writeback");
    readonly.stencil_write_mask_back=0;readonly.stencil_write_mask_front=0xff;
    if(readonly.WritesDepthStencil())throw std::runtime_error("Mask of inactive front-face operation caused writeback");
    readonly.separate_stencil_faces=false;
    if(!readonly.WritesDepthStencil(false,true)||!readonly.WritesDepthStencil(true,false))
        throw std::runtime_error("Explicit attachment clear omitted from writeback");
    readz.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
    if(readonly.WritesDepthStencil())throw std::runtime_error("Disabled depth test scheduled a writeback");
    readz.DepthEnable=TRUE;
    if(!readonly.WritesDepthStencil())throw std::runtime_error("Depth write omitted from writeback");
    readonly.discard=true;
    if(readonly.WritesDepthStencil(true,true))throw std::runtime_error("Discarded draw scheduled a writeback");
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
    if(argc!=2&&argc!=3)throw std::invalid_argument("usage: guest-graphics-resources dxcompiler.dll [depth]");
    const bool cross_stage_alias=argc==3&&std::wstring_view(argv[2])==L"cross-stage-alias";
    const bool attachments_only=argc==3&&std::wstring_view(argv[2])==L"attachments";
    const bool readonly_alias=argc==3&&std::wstring_view(argv[2])==L"readonly-alias";
    const bool linear_color=argc==3&&std::wstring_view(argv[2])==L"linear-color";
    const bool bounds=argc==3&&std::wstring_view(argv[2]).starts_with(L"depth-bounds");
    const bool bounds_reject=bounds&&std::wstring_view(argv[2])==L"depth-bounds-reject";
    const bool use_depth=argc==3&&!attachments_only&&!readonly_alias&&!linear_color&&!cross_stage_alias;
    const bool stale_stencil=argc==3&&std::wstring_view(argv[2])==L"depth-stale-stencil";
    const bool bias=argc==3&&std::wstring_view(argv[2]).starts_with(L"depth-bias");
    const bool fractional_bias=bias&&std::wstring_view(argv[2])==L"depth-bias-fractional";
    const bool clear_depth=use_depth&&std::wstring_view(argv[2])==L"clear";
    const bool htile=use_depth&&std::wstring_view(argv[2])==L"htile";
    const bool use_stencil=use_depth&&std::wstring_view(argv[2]).starts_with(L"stencil");
    const bool clear_stencil=use_stencil&&std::wstring_view(argv[2])==L"stencil-clear";
    const bool stencil_replace=use_stencil&&std::wstring_view(argv[2])==L"stencil-replace";
    const bool stencil_ones=use_stencil&&std::wstring_view(argv[2])==L"stencil-ones";
    const bool stencil_faces=use_stencil&&(std::wstring_view(argv[2])==L"stencil-faces"||std::wstring_view(argv[2])==L"stencil-back-only");
    const bool stencil_back_only=stencil_faces&&std::wstring_view(argv[2])==L"stencil-back-only";
    AmdGpu::StencilFunc bitwise_op=AmdGpu::StencilFunc::Keep;
    if(use_stencil){const std::wstring_view mode=argv[2];
        if(mode==L"stencil-and")bitwise_op=AmdGpu::StencilFunc::And;
        if(mode==L"stencil-or")bitwise_op=AmdGpu::StencilFunc::Or;
        if(mode==L"stencil-xor")bitwise_op=AmdGpu::StencilFunc::Xor;
        if(mode==L"stencil-xnor")bitwise_op=AmdGpu::StencilFunc::Xnor;
        if(mode==L"stencil-nand")bitwise_op=AmdGpu::StencilFunc::Nand;
        if(mode==L"stencil-nor")bitwise_op=AmdGpu::StencilFunc::Nor;
    }
    auto vs=Shader::Backend::DX12::CompileDxil(argv[1],cross_stage_alias?R"(
ByteAddressBuffer tint_data:register(t2);RWTexture2D<float4> shared_image:register(u3);
cbuffer Push:register(b0,space30){float4 transform;};
cbuffer Runtime:register(b0,space31){uint first_vertex,base_instance,indexed,yz_mask,draw_id;float width,height;uint view_index;float depth_bias;};
struct Output {float4 position:SV_Position;float4 tint:COLOR0;};
Output main(uint id:SV_VertexID){Output result;
result.position=float4(float2(id==1?3:-1,id==2?3:-1)*float2(width/8.0,height/8.0)*transform.zw,depth_bias,1);
result.tint=asfloat(tint_data.Load4(0))*shared_image[uint2(0,0)].a;return result;}
)":R"(
ByteAddressBuffer tint_data:register(t2);
cbuffer Push:register(b0,space30){float4 transform;};
cbuffer Runtime:register(b0,space31){uint first_vertex,base_instance,indexed,yz_mask,draw_id;float width,height;uint view_index;float depth_bias;};
struct Output {float4 position:SV_Position;float4 tint:COLOR0;};
Output main(uint id:SV_VertexID){Output result;
result.position=float4(float2(id==1?3:-1,id==2?3:-1)*float2(width/8.0,height/8.0)*transform.zw,depth_bias,1);
result.tint=asfloat(tint_data.Load4(0));return result;}
)",Shader::Backend::DX12::Stage::Vertex);
    auto ps=Shader::Backend::DX12::CompileDxil(argv[1],cross_stage_alias?R"(
RWTexture2D<float4> image:register(u0);
float4 main(float4 position:SV_Position,float4 tint:COLOR0):SV_Target0{
    uint2 p=uint2(position.xy);float4 value=image[p];
    image[p]=float4(value.rgb*0.5,value.a);return value*tint;
}
)":attachments_only?R"(
float4 main(float4 position:SV_Position,float4 tint:COLOR0):SV_Target0{return float4(128.0/255.0,64.0/255.0,1,1)*tint;}
 )":stencil_faces?R"(
Texture2D<float4> image:register(t0);SamplerState filter:register(s1);
float4 main(float4 position:SV_Position,float4 tint:COLOR0,bool front:SV_IsFrontFace):SV_Target0{
    return front?float4(1,0,0,1):image.SampleLevel(filter,position.xy/8.0,0)*tint;
}
 )":R"(
Texture2D<float4> image:register(t0);SamplerState filter:register(s1);
float4 main(float4 position:SV_Position,float4 tint:COLOR0):SV_Target0{return image.SampleLevel(filter,position.xy/8.0,0)*tint;}
)",Shader::Backend::DX12::Stage::Pixel);
    if(!vs.success||!ps.success)throw std::runtime_error(vs.diagnostics+ps.diagnostics);
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1>a;auto hr=DX12_API_CALL(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&a)));if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 d{};Check(DX12_API_CALL(a->GetDesc1(&d)));if(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;}
    if(!device)throw std::runtime_error("No hardware device");
    DX12_API_CALL(device.As(&debug_queue));
    if(cross_stage_alias&&(!debug||!debug_queue))
        throw std::runtime_error("Cross-stage validation requires the D3D12 debug layer");
    auto* memory=static_cast<std::byte*>(VirtualAlloc(reinterpret_cast<void*>(0x1000000000ULL),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Cannot allocate 40-bit guest memory");
    struct Allocation{void* p;~Allocation(){VirtualFree(p,0,MEM_RELEASE);}} owner{memory};
    for(UINT i=0;i<256;i+=4){memory[i]=std::byte{128};memory[i+1]=std::byte{64};memory[i+2]=memory[i+3]=std::byte{255};}
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(reinterpret_cast<VAddr>(memory),65536);GuestTextureCache textures(device.Get());
    Shader::Info pixel,vertex;pixel.sw_stage=Shader::SwStage::Fragment;vertex.sw_stage=Shader::SwStage::Vertex;
    pixel.flattened_ud_buf.resize(12);AmdGpu::Image image{};image.base_address=reinterpret_cast<VAddr>(memory)>>8;
    image.width=image.height=image.pitch=7;image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=10;image.tiling_index=13;
    image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;memcpy(pixel.flattened_ud_buf.data(),&image,32);
    Shader::ImageResource image_resource{};image_resource.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
    image_resource.sharp_fetch.offsets[0]=0;pixel.images.push_back(image_resource);
    Shader::SamplerResource sampler{};sampler.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Sampler>::Summary::SingleLoad;
    sampler.sharp_fetch.offsets[0]=8;pixel.samplers.push_back(sampler);
    if(attachments_only){pixel.images.clear();pixel.samplers.clear();pixel.flattened_ud_buf.clear();}
    Shader::BufferResource buffer{};buffer.buffer_type=Shader::BufferType::Flatbuf;vertex.buffers.push_back(buffer);
    vertex.flattened_ud_buf.resize(4);const float tint[]{0.5f,1,0.25f,1};memcpy(vertex.flattened_ud_buf.data(),tint,16);
    if(readonly_alias){
        // Vertex raw reads and fragment sampled reads address the same guest
        // float RGBA storage through different native resource types.
        image.data_format=14;image.num_format=7;memcpy(pixel.flattened_ud_buf.data(),&image,32);
        const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
        for(UINT y=0;y<8;++y)for(UINT x=0;x<8;++x)
            memcpy(memory+GuestPixelOffset(desc,layout,0,0,x,y),tint,sizeof(tint));
        auto& source=vertex.buffers[0];source.buffer_type=Shader::BufferType::Guest;
        source.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Buffer>::Summary::SingleLoad;
        source.sharp_fetch.offsets[0]=0;
        AmdGpu::Buffer sharp{};sharp.base_address=reinterpret_cast<VAddr>(memory);sharp.num_records=16;
        memcpy(vertex.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    }
    if(cross_stage_alias){
        pixel.samplers.clear();pixel.images[0].is_written=true;
        vertex.flattened_ud_buf.resize(12);
        memcpy(vertex.flattened_ud_buf.data()+4,&image,sizeof(image));
        auto read_view=image_resource;read_view.is_written=true;read_view.sharp_fetch.offsets[0]=4;
        vertex.images.push_back(read_view);
    }
    const GuestResourceStage stages[]{{&pixel,{0,0}},{&vertex,{2,0}}};
    AmdGpu::Regs regs{};regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.SetMask(0,15);const DXGI_FORMAT formats[]{DXGI_FORMAT_R8G8B8A8_UNORM};
    if(use_depth){regs.depth_buffer.z_read_base=regs.depth_buffer.z_write_base=UINT(reinterpret_cast<VAddr>(memory+8192)>>8);
        regs.depth_buffer.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;}
    if(use_stencil){regs.depth_buffer.stencil_read_base=regs.depth_buffer.stencil_write_base=UINT(reinterpret_cast<VAddr>(memory+12288)>>8);
        regs.depth_buffer.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;}
    if(use_depth){regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;regs.depth_control.depth_func=AmdGpu::CompareFunc::Less;}
    if(stale_stencil)regs.depth_control.stencil_enable=1;
    if(bias){regs.polygon_control.enable_polygon_offset_front=1;regs.poly_offset.front_offset=fractional_bias?64.5f:64.f;}
    regs.depth_render_control.depth_clear_enable=clear_depth;
    // Stored depth is .5 while incoming depth is 0: this distinguishes the
    // hardware bounds test from an incorrect comparison against fragment Z.
    if(bounds){regs.depth_control.depth_bounds_enable=1;regs.depth_bounds_min=bounds_reject?0.6f:0.25f;regs.depth_bounds_max=0.75f;}
    if(use_stencil){regs.depth_control.stencil_enable=1;regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Equal;
        regs.stencil_ref_front.stencil_test_val=7;regs.stencil_ref_front.stencil_mask=255;regs.stencil_ref_front.stencil_write_mask=255;
        regs.stencil_control.stencil_zpass_front=AmdGpu::StencilFunc::AddClamp;}
    if(stencil_replace||stencil_ones){
        regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Always;
        regs.stencil_ref_front.stencil_op_val=42;
        regs.stencil_control.stencil_zpass_front=stencil_replace?AmdGpu::StencilFunc::ReplaceOp:AmdGpu::StencilFunc::Ones;
        regs.stencil_control.stencil_zfail_front=regs.stencil_control.stencil_zpass_front;
    }
    if(bitwise_op!=AmdGpu::StencilFunc::Keep){
        regs.depth_control.stencil_ref_func=AmdGpu::CompareFunc::Always;
        regs.stencil_ref_front.stencil_op_val=42;
        if(bitwise_op==AmdGpu::StencilFunc::Nand)regs.stencil_ref_front.stencil_write_mask=42;
        if(bitwise_op==AmdGpu::StencilFunc::Nor)regs.stencil_ref_front.stencil_write_mask=213;
        regs.stencil_control.stencil_zpass_front=bitwise_op;
        regs.stencil_control.stencil_zfail_front=bitwise_op;
    }
    if(stencil_faces){
        regs.depth_control.backface_enable=1;
        regs.depth_control.stencil_ref_func=regs.depth_control.stencil_bf_func=AmdGpu::CompareFunc::Always;
        regs.stencil_ref_front.stencil_op_val=42;regs.stencil_ref_back=regs.stencil_ref_front;
        regs.stencil_control.stencil_zpass_front=regs.stencil_control.stencil_zfail_front=
            stencil_back_only?AmdGpu::StencilFunc::Keep:AmdGpu::StencilFunc::And;
        regs.stencil_control.stencil_zpass_back=regs.stencil_control.stencil_zfail_back=AmdGpu::StencilFunc::Or;
    }
    auto state=BuildGuestGraphicsState(regs,formats,use_stencil?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:use_depth?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_UNKNOWN);
    if(stencil_faces){
        // Choose the back face and independently verify its classification
        // through SV_IsFrontFace in the pixel shader (front faces turn red).
        state.pipeline.RasterizerState.FrontCounterClockwise=FALSE;
        if(!state.separate_stencil_faces||!state.WritesDepthStencil())
            throw std::runtime_error("Independent stencil masks/writeback omitted");
    }
    ShaderPipeline pipeline(device.Get(),argv[1],{vs.bytecode,ps.bytecode},state.pipeline,state.depth_bounds,
        {state.stencil_read_mask_front,state.stencil_write_mask_front,state.stencil_read_mask_back,state.stencil_write_mask_back,state.separate_stencil_faces});
    AmdGpu::ColorBuffer color{};color.base_address=UINT(reinterpret_cast<VAddr>(memory+4096)>>8);
    color.info.format=10;color.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
    if(linear_color){color.info.linear_general=1;color.view.slice_start=37;color.pitch.tile_max=1;color.slice.tile_max=1;}
    AmdGpu::CbDbExtent extent{};extent.width=extent.height=8;auto color_desc=DescribeGuestColorBuffer(color,extent);
    const auto color_memory=std::span(reinterpret_cast<std::byte*>(color_desc.address),size_t(LayoutGuestTexture(color_desc).guest_bytes));
    if(linear_color&&(color_desc.address!=reinterpret_cast<VAddr>(memory+4096+37)||color_desc.first_layer||color_desc.tile_mode!=31||color_memory.size()!=512))
        throw std::runtime_error("Linear-general color base/stride/offset mapping mismatch");
    std::fill(color_memory.begin(),color_memory.end(),std::byte{0xee});GuestTextureCache::Entry* target{};
    color_memory.data()[-1]=std::byte{0xa5};color_memory.data()[color_memory.size()]=std::byte{0x5a};
    GuestTextureDesc depth_desc;GuestTextureCache::Entry* depth_target{};
    if(use_depth) {
        AmdGpu::DepthBuffer db{};db.z_read_base=db.z_write_base=UINT(reinterpret_cast<VAddr>(memory+8192)>>8);
        db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;db.z_info.tile_mode_index=5;
        if(use_stencil){db.stencil_read_base=db.stencil_write_base=UINT(reinterpret_cast<VAddr>(memory+12288)>>8);
            db.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;memset(memory+12288,clear_stencil?0:7,256);}
        depth_desc=DescribeGuestDepthBuffer(db,AmdGpu::DepthView{},extent);
        float initial[64];std::fill_n(initial,64,htile?0.f:0.5f);memcpy(memory+8192,initial,256);
    }
    const D3D12_VIEWPORT viewport{0,0,8,8,0,1};const D3D12_RECT scissor{0,0,8,8};
    const float blend[]{1,1,1,1};Shader::PushData push{};push.xscale=push.yscale=1;
    GraphicsRuntime runtime{};runtime.viewport_width=runtime.viewport_height=8;
    if(bias)runtime.depth_bias=0.25f;
    if(htile)runtime.depth_bias=0.25f;
    GuestDepthMetadata metadata;if(htile){metadata.Register(0x1000);metadata.Consume(0x1000,0);metadata.Clear(0x1000);}
    std::unique_ptr<GuestImageReadback> combined_readback;
    ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,stages,push,runtime,[&](ResourceBindings& bindings) {
        // Texture/attachment ownership resolution can retire earlier work
        // after metadata preparation but before this draw has bound constants.
        rasterizer.Finish();
        std::array<std::byte,256> unrelated{};
        rasterizer.UploadConstants(unrelated);rasterizer.UploadConstants(unrelated);
        target=&textures.Upload(color_desc,color_memory,rasterizer.RecordingList(),rasterizer.CompletedFence());
        const Rasterizer::ColorAttachment attachments[]{{&target->texture,target->Rtv()}};
        Rasterizer::DepthAttachment depth{};
        if(use_depth) {
            depth_target=&textures.Upload(depth_desc,std::span(memory+8192,256),rasterizer.RecordingList(),rasterizer.CompletedFence(),use_stencil?std::span(memory+12288,256):std::span<std::byte>{});
            depth={&depth_target->texture,depth_target->Dsv(),false,false,
                clear_depth||(htile&&metadata.Consume(0x1000,0)),0.75f,clear_stencil,7};
        }
        rasterizer.ExecuteGraphics(pipeline,bindings,state,attachments,use_depth?&depth:nullptr,std::span(&viewport,1),std::span(&scissor,1),blend,{3},{},nullptr,false);
        if(attachments_only)combined_readback=std::make_unique<GuestImageReadback>(device.Get(),*target,rasterizer.RecordingList());
    },textures);
    // The async path deliberately returns while descriptors and temporary
    // buffers are retained. Retire before inspecting or destroying the cache.
    if(combined_readback){
        const auto attachment_fence=rasterizer.Flush();
        combined_readback->Submitted(attachment_fence);
        GuestTextureCache::Entry* readback_targets[]{target,depth_target};
        RetireGuestAttachmentReadbacks(rasterizer,textures,attachment_fence,readback_targets);
        combined_readback->Resolve(rasterizer.CompletedFence(),color_memory);
        combined_readback.reset();
        const auto immediate=DecodeGuestSubresource(color_desc,target->layout,color_memory,0,0);
        if(immediate[0]!=std::byte{64}||immediate[1]!=std::byte{64}||immediate[2]!=std::byte{64}||immediate[3]!=std::byte{255})
            throw std::runtime_error("Combined draw/readback submission did not complete before resources retired");
    }else rasterizer.Finish();
    if(target->pending)throw std::runtime_error("Native attachment upload was not retired after draw");
    u64 fence{};
    {GuestImageReadback rb(device.Get(),*target,rasterizer.RecordingList());fence=rasterizer.Flush();rb.Submitted(fence);textures.Used(*target,fence);
    rasterizer.Finish();rb.Resolve(rasterizer.CompletedFence(),color_memory);}
    const auto* first_readback_pool=target->readback_pool.get();
    auto linear=DecodeGuestSubresource(color_desc,target->layout,color_memory,0,0);
    if(color_memory.data()[-1]!=std::byte{0xa5}||color_memory.data()[color_memory.size()]!=std::byte{0x5a})
        throw std::runtime_error("Attachment writeback overran guest storage");
    if(linear_color)for(UINT y=0;y<8;++y)for(UINT byte=32;byte<64;++byte)
        if(color_memory[y*64+byte]!=std::byte{0xee})throw std::runtime_error("Linear attachment row padding overwritten");
    uint8_t pixels[256]{};memcpy(pixels,linear.data(),256);
    const std::array<uint8_t,4> expected=readonly_alias?std::array<uint8_t,4>{64,255,16,255}:std::array<uint8_t,4>{64,64,64,255};
    for(size_t i=0;i<sizeof(pixels);++i)if(abs(int(pixels[i])-(bounds_reject?0xee:expected[i%4]))>1)throw std::runtime_error("Native multi-stage guest graphics resource result mismatch");
    if(cross_stage_alias){
        const auto source_desc=DescribeGuestTexture(image);
        const auto source_layout=LayoutGuestTexture(source_desc);
        const auto source_pixels=DecodeGuestSubresource(source_desc,source_layout,
            std::span(memory,size_t(source_layout.guest_bytes)),0,0);
        const std::array<uint8_t,4> written{64,32,128,255};
        for(size_t i=0;i<source_pixels.size();++i)
            if(abs(int(std::to_integer<uint8_t>(source_pixels[i]))-written[i%4])>1)
                throw std::runtime_error("Cross-stage shared UAV writes did not reach guest storage");
        size_t shared_entries=0;
        for(const auto& entry:textures.Entries())if(entry->desc.address==source_desc.address)++shared_entries;
        if(shared_entries!=1)throw std::runtime_error("Cross-stage image descriptors did not share one allocation");
    }
    if(use_depth) {
        GuestImageReadback depth_readback(device.Get(),*depth_target,rasterizer.RecordingList());
        auto depth_fence=rasterizer.Flush();depth_readback.Submitted(depth_fence);textures.Used(*depth_target,depth_fence);rasterizer.Finish();
        depth_readback.Resolve(rasterizer.CompletedFence(),std::span(memory+8192,256),use_stencil?std::span(memory+12288,256):std::span<std::byte>{});
        auto depth_linear=DecodeGuestSubresource(depth_desc,depth_target->layout,std::span(memory+8192,256),0,0);
        const float expected_depth=clear_depth?0.75f:htile?0.25f:bounds_reject?0.5f:bias?0.25f+regs.poly_offset.front_offset*std::ldexp(1.f,-25):0.f;
        for(size_t i=0;i<64;++i){float value;memcpy(&value,depth_linear.data()+i*4,4);
            if(bias?(std::abs(value-expected_depth)>1e-7f||value<=0.25f):value!=expected_depth)
                throw std::runtime_error("Native depth bias/clear/test/write/tiled readback mismatch");}
        if(use_stencil){auto sd=DescribeGuestStencil(depth_desc);auto sl=LayoutGuestTexture(sd);
            auto values=DecodeGuestSubresource(sd,sl,std::span(memory+12288,256),0,0);
            const auto expected_stencil=std::byte(stencil_faces?47:stencil_replace?42:stencil_ones?255:
                bitwise_op==AmdGpu::StencilFunc::And?2:bitwise_op==AmdGpu::StencilFunc::Or?47:
                bitwise_op==AmdGpu::StencilFunc::Xor||bitwise_op==AmdGpu::StencilFunc::Nand?45:
                bitwise_op==AmdGpu::StencilFunc::Xnor||bitwise_op==AmdGpu::StencilFunc::Nor?210:8);
            for(auto value:values)if(value!=expected_stencil)throw std::runtime_error("Native stencil clear/test/write/guest writeback mismatch: expected="+
                std::to_string(std::to_integer<UINT>(expected_stencil))+" actual="+std::to_string(std::to_integer<UINT>(value)));
            puts("PASS: tiled guest stencil -> native separate stencil plane -> test/operation -> exact guest readback");}
        puts("PASS: actual guest depth-buffer metadata -> typeless native D32 resource -> depth-tested draw -> verified 64 tiled guest depth writes");
    }
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan runtime");
    textures.Collect(rasterizer.CompletedFence());
    auto* first_upload=target->upload_pool.at(0).handle.Get();
    target->sampled_snapshot.assign(color_memory.begin(),color_memory.end());
    auto& reused=textures.Upload(color_desc,color_memory,rasterizer.RecordingList(),rasterizer.CompletedFence());
    if(&reused!=target||!reused.uploads.empty())throw std::runtime_error("Coherent native render target was uploaded again");
    fence=rasterizer.Flush();textures.Submitted(fence);rasterizer.Finish();
    GuestImageReadback reused_readback(device.Get(),reused,rasterizer.RecordingList());
    if(reused.readback_pool.get()!=first_readback_pool)throw std::runtime_error("Retired native image readback allocation was not reused");
    fence=rasterizer.Flush();reused_readback.Submitted(fence);textures.Used(reused,fence);rasterizer.Finish();
    reused_readback.Resolve(rasterizer.CompletedFence(),color_memory);
    auto reused_linear=DecodeGuestSubresource(color_desc,reused.layout,color_memory,0,0);
    if(reused_linear!=linear)throw std::runtime_error("Native render target reuse changed GPU contents");
    color_memory[0]^=std::byte{1};
    auto& changed=textures.Upload(color_desc,color_memory,rasterizer.RecordingList(),rasterizer.CompletedFence());
    if(changed.uploads.empty())throw std::runtime_error("Native render target ignored a CPU modification");
    if(changed.uploads.at(0).handle.Get()!=first_upload)
        throw std::runtime_error("Retired texture upload allocation was not reused");
    fence=rasterizer.Flush();textures.Submitted(fence);rasterizer.Finish();
    {GuestImageReadback changed_rb(device.Get(),changed,rasterizer.RecordingList());
    fence=rasterizer.Flush();changed_rb.Submitted(fence);textures.Used(changed,fence);rasterizer.Finish();
    changed_rb.Resolve(rasterizer.CompletedFence(),color_memory);}
    auto changed_pixels=DecodeGuestSubresource(color_desc,changed.layout,color_memory,0,0);
    linear[0]^=std::byte{1};
    if(changed_pixels!=linear)throw std::runtime_error("Reused upload buffer lost a CPU texture modification");
    if(use_depth){
        textures.Collect(rasterizer.CompletedFence());
        const auto depth_memory=std::span(memory+8192,256);
        const auto stencil_memory=use_stencil?std::span(memory+12288,256):std::span<std::byte>{};
        depth_target->RememberSnapshot(depth_memory,stencil_memory);
        auto& coherent=textures.Upload(depth_desc,depth_memory,rasterizer.RecordingList(),rasterizer.CompletedFence(),stencil_memory);
        if(&coherent!=depth_target||!coherent.uploads.empty())throw std::runtime_error("Coherent depth/stencil uploaded again");
        fence=rasterizer.Flush();textures.Submitted(fence);rasterizer.Finish();
        // Change only stencil first: depth matching must not hide a CPU stencil write.
        if(use_stencil){
            stencil_memory[0]^=std::byte{1};
            auto& altered=textures.Upload(depth_desc,depth_memory,rasterizer.RecordingList(),rasterizer.CompletedFence(),stencil_memory);
            if(altered.uploads.empty())throw std::runtime_error("CPU stencil modification was ignored");
            fence=rasterizer.Flush();textures.Submitted(fence);rasterizer.Finish();
        }
        const float cpu_depth=0.625f;memcpy(depth_memory.data(),&cpu_depth,4);
        auto& altered=textures.Upload(depth_desc,depth_memory,rasterizer.RecordingList(),rasterizer.CompletedFence(),stencil_memory);
        if(altered.uploads.empty())throw std::runtime_error("CPU depth modification was ignored");
        fence=rasterizer.Flush();textures.Submitted(fence);rasterizer.Finish();
        const std::vector<std::byte> expected_depth(depth_memory.begin(),depth_memory.end()),
            expected_stencil(stencil_memory.begin(),stencil_memory.end());
        auto observed_depth=expected_depth,observed_stencil=expected_stencil;
        GuestImageReadback rb(device.Get(),altered,rasterizer.RecordingList());
        fence=rasterizer.Flush();rb.Submitted(fence);textures.Used(altered,fence);rasterizer.Finish();
        rb.Resolve(rasterizer.CompletedFence(),observed_depth,observed_stencil);
        if(observed_depth!=expected_depth||observed_stencil!=expected_stencil)
            throw std::runtime_error("Depth/stencil reuse lost independent CPU plane modifications");
    }
    if(cross_stage_alias&&debug_queue){
        for(UINT64 i=0;i<debug_queue->GetNumStoredMessages();++i){
            SIZE_T size{};Check(debug_queue->GetMessage(i,nullptr,&size));
            std::vector<std::byte> bytes(size);
            auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
            Check(debug_queue->GetMessage(i,message,&size));
            if(message->Severity==D3D12_MESSAGE_SEVERITY_ERROR||
               message->Severity==D3D12_MESSAGE_SEVERITY_CORRUPTION)
                throw std::runtime_error(std::string("Cross-stage D3D12 validation: ")+message->pDescription);
        }
    }
    puts("PASS: distinct PS/VS guest metadata -> unified texture/sampler/flatbuf + push/runtime -> actual PS4 color-buffer native RTV -> draw -> tiled guest writeback, 64 verified pixels; no Vulkan");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());if(debug_queue)for(UINT64 i=0;i<DX12_API_CALL(debug_queue->GetNumStoredMessages());++i){SIZE_T size{};DX12_API_CALL(debug_queue->GetMessage(i,nullptr,&size));std::vector<std::byte> data(size);auto* message=reinterpret_cast<D3D12_MESSAGE*>(data.data());if(SUCCEEDED(DX12_API_CALL(debug_queue->GetMessage(i,message,&size))))fprintf(stderr,"D3D12: %s\n",message->pDescription);}return 1;}}
