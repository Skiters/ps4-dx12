// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_pipeline_cache.h"
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_graphics_state.h"
#include "video_core/renderer_d3d12/guest_image_readback.h"
#include "video_core/renderer_d3d12/guest_viewports.h"
#include "video_core/renderer_d3d12/guest_point_shader.h"
#include "video_core/renderer_d3d12/guest_point_capture.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "common/serdes.h"
#include "common/path_util.h"
#include "video_core/cache_storage.h"
#include "core/emulator_settings.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <cstdio>
#include <fstream>

using namespace NativeDX12;
static ComPtr<ID3D12Device> HardwareDevice(){
    ComPtr<ID3D12Debug> debug;Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))return device;
    }
    throw std::runtime_error("No hardware D3D12 device");
}
static bool OriginalFront(const AmdGpu::Regs& regs,unsigned viewport,bool reverse){
    const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};double area{};
    const auto& vp=regs.viewports[viewport];
    for(unsigned i=0;i<3;++i){const unsigned a=reverse?(3-i)%3:i,b=reverse?(2-i+3)%3:(i+1)%3;
        const double ax=vp.xoffset+vp.xscale*xy[a][0],ay=vp.yoffset+vp.yscale*xy[a][1];
        const double bx=vp.xoffset+vp.xscale*xy[b][0],by=vp.yoffset+vp.yscale*xy[b][1];area-=0.5*(ax*by-bx*ay);}
    return regs.polygon_control.front_face==AmdGpu::FrontFace::CounterClockwise?area>0:area<0;
}
// Compile-only preparation uses the production GS EmitVertex adaptation. The
// upstream VS/PS remain actual GCN; this explicit GS is not a guest GCN GS test.
static GuestShaderStage PreparedPointGeometry(const Shader::Profile& profile,bool unit,u64 id,const Shader::Info& vertex){
    using namespace Shader::Backend::SPIRV;
    GuestShaderStage stage;stage.id=id;stage.auxiliary=true;
    stage.info.hw_stage=Shader::HwStage::Geometry;stage.info.sw_stage=Shader::SwStage::Geometry;
    stage.info.stores.Set(Shader::IR::Attribute::Position0);
    if(!unit)stage.info.stores.Set(Shader::IR::Attribute::PointSize);
    auto& geometry=stage.runtime.hw.gs;geometry.native_point_expansion=geometry.native_point_clip_depth=true;
    geometry.in_primitive=AmdGpu::PrimitiveType::PointList;geometry.in_vertex_data_size=4;
    geometry.out_primitive.fill(AmdGpu::GsOutputPrimitiveType::PointList);
    geometry.output_vertices=geometry.num_invocations=1;
    Shader::Backend::Bindings bindings{};EmitContext ctx(profile,stage.runtime,stage.info,bindings);
    ctx.AddCapability(spv::Capability::Geometry);
    const auto input_misc=ctx.DefineVariable(ctx.TypeArray(ctx.U32[4],ctx.ConstU32(1u)),{},spv::StorageClass::Input);
    ctx.Decorate(input_misc,spv::Decoration::Location,NativePointMiscLocation(vertex,false));
    const auto main=ctx.OpFunction(ctx.TypeVoid(),spv::FunctionControlMask::MaskNone,ctx.TypeFunction(ctx.TypeVoid()));
    ctx.AddExecutionMode(main,spv::ExecutionMode::InputPoints);
    ctx.AddExecutionMode(main,unit?spv::ExecutionMode::OutputPoints:spv::ExecutionMode::OutputTriangleStrip);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,unit?1u:4u);
    ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,1u);
    ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);
    ctx.AddLabel(ctx.OpLabel());
    const auto position=ctx.OpLoad(ctx.F32[4],ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,ctx.F32[4]),
        ctx.gl_in,ctx.ConstU32(0u),ctx.ConstU32(0u)));
    ctx.OpStore(ctx.output_position,position);
    if(!unit){const auto misc=ctx.OpLoad(ctx.U32[4],ctx.OpAccessChain(
        ctx.TypePointer(spv::StorageClass::Input,ctx.U32[4]),input_misc,ctx.ConstU32(0u)));
        ctx.OpStore(ctx.output_point_size,ctx.OpBitcast(ctx.F32[1],ctx.OpCompositeExtract(ctx.U32[1],misc,0u)));}
    EmitEmitVertex(ctx);ctx.OpReturn();ctx.OpFunctionEnd();stage.spirv=ctx.Assemble();return stage;
}
int main(int argc,char** argv){try{
    const bool compile_shaders=argc==5&&std::string_view(argv[4])=="compile-shaders";
    if(argc!=4&&!compile_shaders)throw std::invalid_argument("Expected converter, DXC, winding mode and optional compile-shaders");
    auto mode=std::string_view(argv[3]);
    const auto artifact_mode=std::string(mode);
    const bool stream_inputs=mode.starts_with("stream-");
    if(stream_inputs){
        mode.remove_prefix(7);
    }
    const bool point_capture_two=mode.starts_with("capture2-");
    const bool point_capture_three=mode.starts_with("capture3-");
    const u32 capture_instances=point_capture_three?3u:1u;
    const bool point_capture=mode.starts_with("capture-")||point_capture_two||point_capture_three;
    if(point_capture){
        mode.remove_prefix(point_capture_two||point_capture_three?9:8);
    }
    if(stream_inputs&&(!point_capture||(mode!="point-size-gcn-loop-bound"&&mode!="point-unit-gcn-loop-bound")))
        throw std::invalid_argument("Buffered input fixture requires a capture GCN loop mode");
    if(compile_shaders)if(const auto* compiler_log=std::getenv("NATIVE_POINT_COMPILER_LOG")){
        Common::Log::Setup(compiler_log);
        Common::Log::UpdateLogLevels("*:critical");
        Common::Log::UpdateLogFlushLevel("critical");
    }
    const bool clip_disabled=mode=="point-size-viewport-clip-disabled"||mode=="point-size-gcn-viewport-clip-disabled"||mode=="point-unit-gcn-viewport-clip-disabled";
    const bool gcn_clip_viewport=clip_disabled&&mode!="point-size-viewport-clip-disabled";
    const bool gcn_center_distances=mode=="point-size-gcn-loop-clip-bound"||mode=="point-size-gcn-loop-cull-bound"||
        mode=="point-size-gcn-loop-clip-cull-bound"||mode=="point-unit-gcn-loop-clip-bound"||
        mode=="point-unit-gcn-loop-clip-cull-bound"||mode=="point-unit-gcn-loop-sparse-distances";
    const bool gcn_sparse_distances=mode=="point-unit-gcn-loop-sparse-distances";
    const bool gcn_dual_distances=mode=="point-size-gcn-loop-clip-cull-bound"||mode=="point-unit-gcn-loop-clip-cull-bound"||gcn_sparse_distances;
    const bool gcn_cull_only=mode=="point-size-gcn-loop-cull-bound";
    const bool native_sparse_distances=mode=="point-size-native-sparse-distances";
    const bool native_dual_distances=mode=="point-size-native-clip-cull"||native_sparse_distances;
    const bool late_viewport=mode=="base-late-viewport"||mode=="point-size-viewport-late";
    const bool point_packed_clip=mode=="point-size-vs-packed31-clip";
    const bool point_packed_gcn=mode=="point-size-gcn-saturated-packed32";
    const bool point_packed_centroid=mode=="point-size-vs-packed32-centroid";
    const bool point_packed_sample=mode=="point-size-vs-packed32-sample";
    const bool point_packed_mixed=mode=="point-size-vs-packed32-mixed"||point_packed_centroid||point_packed_sample;
    const bool point_packed=mode=="point-size-vs-packed32"||mode=="point-unit-vs-packed32"||point_packed_clip||point_packed_mixed||point_packed_gcn;
    const u32 point_packed_inputs=point_packed_clip?31:32;
    const bool vs_unit=mode=="point-unit-vs-live-mixed"||mode=="point-unit-vs-packed32";
    const bool vs_qualified=mode=="point-size-vs-live-aliased"||mode=="point-size-vs-live-mixed"||mode=="point-unit-vs-live-mixed";
    const bool vs_mixed=mode=="point-size-vs-live-mixed"||mode=="point-unit-vs-live-mixed";
    const bool point_saturated=mode=="point-size-saturated"||mode=="point-size-saturated-live"||mode=="point-size-saturated-clip"||vs_qualified||point_packed;
    const bool gcn_saturated=mode=="point-size-gcn-saturated"||mode=="point-size-gcn-saturated-live"||point_packed_gcn;
    const bool unit_declared=mode=="point-unit-gcn-declared"||mode=="point-unit-gcn-declared-live";
    const bool unit_loop=mode=="point-unit-gcn-loop-bound";
    const bool loop_alias_default=mode=="point-size-gcn-loop-live-alias-default";
    const bool loop_reserved=mode=="point-size-gcn-loop-live-mixed-reserved";
    const bool loop_mixed=mode=="point-size-gcn-loop-live-mixed"||loop_reserved;
    const bool loop_alias=mode=="point-size-gcn-loop-live-aliased"||loop_alias_default||loop_mixed;
    const bool loop_live_xw=mode=="point-size-gcn-loop-live-xw"||loop_alias;
    const bool loop_live=mode=="point-size-gcn-loop-live-x"||mode=="point-size-gcn-loop-live-z"||mode=="point-size-gcn-loop-live-w"||loop_live_xw;
    const u32 live_lane=mode=="point-size-gcn-loop-live-z"?2:mode=="point-size-gcn-loop-live-w"?3:0;
    const bool gcn_declared=mode=="point-size-gcn-declared"||mode=="point-size-gcn-declared-live"||unit_declared;
    const bool gcn_loop=mode=="point-size-gcn-loop-bound"||unit_loop||loop_live||gcn_center_distances;
    const bool declared_live=mode=="point-size-gcn-declared-live"||mode=="point-unit-gcn-declared-live"||loop_live;
    const bool point_live=mode=="point-size-saturated-live"||mode=="point-size-saturated-clip"||mode=="point-size-gcn-saturated-live"||declared_live||vs_qualified||point_packed;
    const bool point_clip=mode=="point-size-clip"||mode=="point-size-native-clip"||mode=="point-size-native-cull"||mode=="point-size-saturated-clip"||gcn_center_distances||native_dual_distances||point_packed_clip;
    const bool point_layer=mode=="point-size-layer";
    const bool point_edge=mode=="point-size-viewport-edge";
    const bool point_depth=late_viewport||point_edge;
    const bool point_viewport=mode=="point-size-viewport"||mode=="point-size-viewport-late"||point_edge||clip_disabled;
    const bool geometry_points=mode=="point-size-geometry"||mode=="point-unit-geometry";
    const bool gcn_geometry=mode=="point-size-gcn-geometry"||mode=="point-unit-gcn-geometry"||gcn_saturated||gcn_declared||gcn_loop||gcn_clip_viewport;
    const bool geometry_unit=mode=="point-unit-geometry"||mode=="point-unit-gcn-geometry"||unit_declared||unit_loop||mode=="point-unit-gcn-viewport-clip-disabled"||mode=="point-unit-gcn-loop-clip-bound"||mode=="point-unit-gcn-loop-clip-cull-bound"||gcn_sparse_distances||vs_unit;
    const bool sized_points=mode=="point-size"||point_clip||point_layer||point_viewport||geometry_points||gcn_geometry||point_saturated;
    const bool expanded_points=point_capture||(sized_points&&!geometry_unit);
    if(!sized_points&&mode!="base"&&!late_viewport)throw std::invalid_argument("Invalid winding mode");
    EmulatorSettings.SetPipelineCacheEnabled(false);EmulatorSettings.SetShaderCollect(false);EmulatorSettings.SetDumpShaders(false);
    _putenv_s("SHADPS4_NATIVE_SHADER_CACHE","off");
    // PS4 buffer sharps hold only 40 address bits. An arbitrary host allocation
    // can silently truncate before reaching the renderer's range validation.
    std::byte* memory=nullptr;
    for(u64 hint=0x200000000;!memory&&hint<0x210000000;hint+=0x1000000)
        memory=static_cast<std::byte*>(VirtualAlloc(reinterpret_cast<void*>(hint),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!memory)throw std::runtime_error("Winding guest allocation failed");
    struct Allocation{void* value;~Allocation(){VirtualFree(value,0,MEM_RELEASE);}} allocation{memory};
    const auto address=reinterpret_cast<u64>(memory);
    alignas(256) std::array<u32,1024> vertex_code{},pixel_code{},export_code{},geometry_code{},copy_code{};
    const auto initialize=[](auto& program,const std::vector<u32>& words,u64 hash){
        if(words.size()>768)throw std::runtime_error("GCN program overlaps binary metadata");
        std::copy(words.begin(),words.end(),program.begin());program[1]=383;AmdGpu::BinaryInfo binary{};
        binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
        memcpy(program.data()+768,&binary,sizeof(binary));
    };
    std::vector<u32> vertex_words{0xBEEB03FF,15};
    for(u32 i=0;i<(native_dual_distances?13u:point_clip?9u:point_layer||point_viewport?6u:5u);++i){vertex_words.push_back(0xe0302000U|i*4);vertex_words.push_back((128U<<24)|((4+i)<<8));}
    vertex_words.push_back(0xbf8c007f);
    if(point_saturated)vertex_words.insert(vertex_words.end(),{
        0x7e1c0281,0xe0004000u|(50u<<18),0x80010e00}); // One guest VS atomic, not per corner.
    if(late_viewport)vertex_words.insert(vertex_words.end(),{0xf80000cf,0x07060504});
    vertex_words.insert(vertex_words.end(),{(late_viewport?0x800u:0u)|(point_viewport?0xf80000d9u:point_layer?0xf80000d5u:sized_points?0xf80000d1u:0xf80000d8u),
        point_viewport?0x09000008u:point_layer?0x00090008u:sized_points?0x00000008u:0x08000000u});
    if(point_clip)vertex_words.insert(vertex_words.end(),{0xf80000ef,0x0c0b0a09});
    if(native_dual_distances)vertex_words.insert(vertex_words.end(),{0xf80000ff,0x100f0e0d});
    if(point_saturated){
        if(vs_qualified){const u32 values[]{0x3e000000,0x3e800000,0x3f000000,0x3f400000};
            for(u32 component=0;component<4;++component)
                vertex_words.insert(vertex_words.end(),{0x7e0002ffu|((10+component)<<17),values[component]});
        }else vertex_words.insert(vertex_words.end(),{0x7e1a02ff,0x3f000000});
        for(u32 parameter=0;parameter<32;++parameter){
            if(point_packed)for(u32 component=0;component<4;++component)
                vertex_words.insert(vertex_words.end(),{0x7e0002ffu|((10+component)<<17),
                    std::bit_cast<u32>(float(parameter*4+component+1)/256.f)});
            vertex_words.insert(vertex_words.end(),{0xf800020fu|(parameter<<4),(vs_qualified&&parameter==0)||point_packed?0x0d0c0b0au:0x0d0d0d0du});
        }
    }
    if(!late_viewport)vertex_words.insert(vertex_words.end(),{0xf80008cf,0x07060504});
    vertex_words.push_back(0xbf810000);
    initialize(vertex_code,vertex_words,0x57494e44494e4701ULL);
    auto pixel_words=point_depth?std::vector<u32>{0xBEEB03FF,15,0x7e0402ff,0x3f800000,
        0x7e0602ff,0x3f800000,0xf800080f,0x03020001,0xbf810000}:
        std::vector<u32>{0xBEEB03FF,15,point_live?(0xc8060002u|(live_lane<<8)):0x7e020280u,0x7e0402ff,0x3f800000,
        0x7e0602ff,0x3f800000,0xf800080f,0x03020100,0xbf810000};
    if(loop_live_xw||vs_qualified){pixel_words[3]=loop_alias||vs_qualified?0xc80a0702u:0xc80a0302u;pixel_words.erase(pixel_words.begin()+4);} // v_interp_mov v2, p0/p1.w
    if(loop_alias_default)pixel_words[2]=0xc8050000; // v_interp_p2 v1, default input 0.x
    if(loop_reserved){pixel_words[4]=0xc80e0802;pixel_words.erase(pixel_words.begin()+5);} // v_interp_mov v3, p2.x
    if(loop_mixed||vs_mixed){
        pixel_words[3]=0xc8090700; // v_interp_p2 v2, smooth input 1.w, I in v0
        pixel_words[pixel_words.size()-2]=0x03020105; // FrontFace saved in v5
        pixel_words.insert(pixel_words.begin()+2,0x7e0a0302); // v_mov_b32 v5, initial v2
    }
    if(point_packed){
        pixel_words={0xBEEB03FF,15};
        if(point_packed_mixed)pixel_words.push_back(0x7e0a0302); // Save initial v2 front face in v5.
        pixel_words.push_back(0x7e020280); // v1 = 0.
        for(u32 input=0;input<point_packed_inputs;++input)
            pixel_words.insert(pixel_words.end(),{(point_packed_mixed&&input%2?0xc8090000u:0xc80a0002u)|
                (input<<10)|((input%4)<<8),0x06020501u});
        pixel_words.insert(pixel_words.end(),{0x7e0802ffu,std::bit_cast<u32>(1.f/point_packed_inputs),
            0x10020901u,0x7e0402ffu,0x3f800000u,0x7e0602ffu,0x3f800000u,
            0xf800080fu,point_packed_mixed?0x03020105u:0x03020100u,0xbf810000u});
    }
    initialize(pixel_code,pixel_words,0x57494e44494e4702ULL);
    AmdGpu::Regs regs{};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.primitive_type=sized_points?AmdGpu::PrimitiveType::PointList:AmdGpu::PrimitiveType::TriangleList;
    regs.vs_program.address=reinterpret_cast<u64>(vertex_code.data())>>8;regs.vs_program.settings.num_user_regs=4;
    regs.vs_program.settings.num_vgprs=native_dual_distances?4:point_clip?3:2;regs.ps_program.address=reinterpret_cast<u64>(pixel_code.data())>>8;
    if(point_saturated){regs.vs_program.settings.num_user_regs=8;regs.vs_program.settings.num_vgprs=4;}
    regs.ps_program.settings.num_vgprs=0;regs.vs_output_control.vs_out_misc_enable=regs.vs_output_control.use_vtx_viewport_idx=1;
    if(sized_points){regs.vs_output_control.use_vtx_viewport_idx=0;regs.vs_output_control.use_vtx_point_size=1;}
    if(vs_unit)regs.vs_output_control.use_vtx_point_size=0;
    if(point_clip){regs.vs_output_control.vs_out_ccdist0_enable=1;regs.vs_output_control.clip_distance_enable=15;}
    if(gcn_cull_only||mode=="point-size-native-cull"){regs.vs_output_control.clip_distance_enable=0;regs.vs_output_control.cull_distance_enable=15;}
    if(gcn_dual_distances||native_dual_distances){regs.vs_output_control.vs_out_ccdist1_enable=1;regs.vs_output_control.cull_distance_enable=240;}
    if(native_sparse_distances||gcn_sparse_distances){regs.vs_output_control.clip_distance_enable=5;regs.vs_output_control.cull_distance_enable=160;}
    if(point_layer)regs.vs_output_control.use_vtx_render_target_idx=1;
    if(point_viewport)regs.vs_output_control.use_vtx_viewport_idx=1;
    if(point_live){regs.num_interp=1;regs.ps_inputs[0].input_offset=declared_live||vs_qualified?0:point_clip?1:2;regs.ps_inputs[0].flat_shade=1;}
    if(loop_alias||vs_qualified){
        regs.num_interp=2;regs.ps_inputs[1].input_offset=0;regs.ps_inputs[1].flat_shade=1;
        if(loop_alias_default){regs.ps_inputs[0].use_default=1;regs.ps_inputs[0].flat_shade=0;regs.ps_inputs[0].default_value=0;}
        if(loop_mixed||vs_mixed){regs.ps_inputs[1].flat_shade=0;regs.ps_input_addr.persp_center_ena=regs.ps_input_ena.persp_center_ena=1;regs.ps_program.settings.num_vgprs=1;}
        if(loop_reserved){regs.num_interp=3;regs.ps_inputs[2].input_offset=1;regs.ps_inputs[2].flat_shade=1;}
    }
    if(point_packed){
        regs.num_interp=point_packed_inputs;regs.ps_program.settings.num_vgprs=1;
        for(u32 input=0;input<point_packed_inputs;++input){
            regs.ps_inputs[input].input_offset=input;regs.ps_inputs[input].flat_shade=!point_packed_mixed||input%2==0;
        }
        if(point_packed_centroid)regs.ps_input_addr.persp_centroid_ena=regs.ps_input_ena.persp_centroid_ena=1;
        else if(point_packed_sample)regs.ps_input_addr.persp_sample_ena=regs.ps_input_ena.persp_sample_ena=1;
        else if(point_packed_mixed)regs.ps_input_addr.persp_center_ena=regs.ps_input_ena.persp_center_ena=1;
    }
    regs.ps_input_addr.front_face_ena=regs.ps_input_ena.front_face_ena=1;
    if(point_depth)regs.ps_input_addr.pos_z_float_ena=regs.ps_input_ena.pos_z_float_ena=1;
    regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
    regs.color_buffers[0].base_address=u32((address+4096)>>8);
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.raw=regs.color_shader_mask.raw=15;regs.color_buffers[0].info.format=10;
    regs.color_buffers[0].info.number_type=u32(AmdGpu::NumberFormat::Unorm);
    regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
    if(clip_disabled)regs.clipper_control.clip_disable=1;
    auto& vp=regs.viewport_control;vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
    for(unsigned i=0;i<2;++i){regs.viewports[i].xscale=4;regs.viewports[i].xoffset=4+8*i;
        regs.viewports[i].yoffset=4;regs.viewports[i].zscale=1;}
    if(point_viewport)regs.viewports[1].xscale=3;
    if(late_viewport){regs.viewports[1].zscale=-1;regs.viewports[1].zoffset=1;}
    if(point_edge){vp.zoffset_enable=1;
        for(auto& viewport:regs.viewports){viewport.zoffset=0.25f;viewport.zscale=0.25f;}
        regs.viewports[1].zoffset=0.5f;regs.viewports[1].zscale=-0.25f;}
    regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=16;
    regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
    auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=u32((address+8192)>>8);
    db.stencil_read_base=db.stencil_write_base=u32((address+12288)>>8);db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;
    db.z_info.tile_mode_index=5;db.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;
    db.depth_size.pitch_tile_max=1;db.depth_slice.tile_max=1;
    regs.depth_control.stencil_enable=regs.depth_control.backface_enable=1;
    regs.depth_control.stencil_ref_func=regs.depth_control.stencil_bf_func=AmdGpu::CompareFunc::Always;
    regs.stencil_ref_front.stencil_mask=regs.stencil_ref_front.stencil_write_mask=255;regs.stencil_ref_back=regs.stencil_ref_front;
    regs.stencil_control.stencil_zpass_front=AmdGpu::StencilFunc::AddClamp;
    regs.stencil_control.stencil_zpass_back=AmdGpu::StencilFunc::SubClamp;
    if(point_layer)regs.depth_control.stencil_enable=regs.depth_control.backface_enable=0;
    AmdGpu::Buffer input{};input.base_address=address;input.stride=point_clip?64:32;input.num_records=6;input.data_format=4;input.num_format=4;
    input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
    if(point_saturated){AmdGpu::Buffer counter=input;counter.base_address=address+32768;counter.stride=4;counter.num_records=4;
        memcpy(regs.vs_program.user_data.data()+4,&counter,sizeof(counter));}
    if(gcn_geometry){
        // Component stride in the copy shader is 64 bytes per output vertex;
        // ring elimination maps coherent stores using this exact guest layout.
        const u32 base_components=geometry_unit?4:5;
        const u32 input_components=gcn_center_distances?base_components+(gcn_dual_distances?8:4):
            gcn_clip_viewport?(geometry_unit?5:6):base_components;
        const u32 parameters=loop_reserved?2:gcn_saturated?32:declared_live?1:0;
        const u32 components=input_components+parameters*4;
        const u32 declared_vertices=point_capture&&gcn_loop?256:gcn_center_distances?64:geometry_unit?(declared_live?128:unit_declared||unit_loop?256:2):
            loop_live?(loop_reserved?36:loop_alias_default?48:loop_live_xw?42:live_lane==2?36:48):gcn_loop||declared_live?64:gcn_declared?128:2;
        std::vector<u32> es{0xBEEB03FF,31},gs{0xBEEB03FF,31},copy{0xBEEB03FF,31};
        u32 scalar_copy_offset{};
        if(gcn_declared||gcn_loop)copy.push_back(0xb00a0000); // s_movk s10, 0
        const auto copy_load=[&](u32 offset,u32 destination){
            if(gcn_declared||gcn_loop){
                const auto delta=offset-scalar_copy_offset;
                if(delta>32767)throw std::runtime_error("COPY scalar increment exceeds its signed immediate");
                if(delta)copy.push_back(0xb78a0000u|delta); // s_addk s10, delta
                scalar_copy_offset=offset;
                copy.insert(copy.end(),{0xe0300000u,0x0a000000u|(destination<<8)});
            }else copy.insert(copy.end(),{0xe0300000u|offset,0x80000000u|(destination<<8)});
        };
        for(u32 component=0;component<4;++component){
            es.push_back(0xe0302000u|component*4);es.push_back(0x80000000u|((4+component)<<8));}
        es.push_back(0xbf8c007f);
        es.insert(es.end(),{0x7e100281,0xe0004000u|(50u<<18),0x80010800});
        for(u32 component=0;component<4;++component){
            es.push_back(0xe0704000u|component*4);es.push_back(0x80400000u|((4+component)<<8));}
        es.push_back(0xbf810000);
        for(u32 component=0;component<input_components;++component){
            const auto input_offset=gcn_center_distances&&geometry_unit&&component>=4?component*4+4:
                gcn_clip_viewport&&geometry_unit&&component==4?20:component*4;
            gs.push_back(0xe0300000u|input_offset);gs.push_back(0x80000000u|((4+component)<<8));
            copy_load(component*declared_vertices*64,4+component);}
        gs.push_back(0xbf8c007f);
        // Viewport index uses v9 in the sized case; the counter must not replace it.
        gs.insert(gs.end(),{gcn_center_distances?0x7e220281u:gcn_clip_viewport?0x7e140281u:0x7e120281u,
            0xe0004004u|(50u<<18),gcn_center_distances?0x80011100u:gcn_clip_viewport?0x80010a00u:0x80010900u});
        if(loop_live){
            const u32 values[]{0x3e000000,0x3e800000,0x3f000000,0x3f400000};
            for(u32 component=0;component<4;++component)
                gs.insert(gs.end(),{0x7e0002ffu|((10+component)<<17),values[component]});
            if(loop_reserved)gs.insert(gs.end(),{0x7e1c02ff,0x3f000000}); // parameter 1 differs from parameter 0.x
        }else if(parameters)gs.insert(gs.end(),{0x7e1a02ff,0x3f000000});
        const auto emit_point=[&]{
            for(u32 component=0;component<components;++component){
                const u32 source=component<input_components?4+component:
                    loop_reserved&&component>=input_components+4?14:loop_live?10+(component-input_components)%4:13;
                const u32 offset=component*declared_vertices*4;
                if(offset>4095)gs.insert(gs.end(),{0xbe8f03ffu,offset,0xe0704000u,0x0f400000u|(source<<8)});
                else gs.insert(gs.end(),{0xe0704000u|offset,0x80400000u|(source<<8)});
            }
            gs.push_back(0xbf900022); // MSG_GS, Emit; no guest code replay.
        };
        if(gcn_loop){
            gs.push_back(0xbe8c0308); // s_mov_b32 s12, user-data s8
            const auto start=gs.size();emit_point();
            gs.insert(gs.end(),{0xb78cffff,0xbf07800c}); // decrement; compare != 0
            const auto relative=s32(start)-s32(gs.size()+1);
            gs.push_back(0xbf850000u|u16(relative)); // s_cbranch_scc1
        }else for(u32 point=0;point<2;++point)emit_point();
        gs.insert(gs.end(),{0xbf900003,0xbf810000});
        copy.push_back(0xbf8c007f);
        if(gcn_clip_viewport)copy.insert(copy.end(),{geometry_unit?0xf80000d8u:0xf80000d9u,
            geometry_unit?0x08000000u:0x09000008u});
        else if(!geometry_unit)copy.insert(copy.end(),{0xf80000d1,0x00000008});
        if(gcn_center_distances){
            copy.insert(copy.end(),{0xf80000ef,geometry_unit?0x0b0a0908u:0x0c0b0a09u});
            if(gcn_dual_distances)copy.insert(copy.end(),{0xf80000ff,geometry_unit?0x0f0e0d0cu:0x100f0e0du});
        }
        if(parameters)for(u32 parameter=0;parameter<parameters;++parameter){
            for(u32 component=0;component<4;++component){
                const u32 offset=(input_components+parameter*4+component)*declared_vertices*64;
                // MUBUF's immediate offset is only twelve bits. The original
                // copy parser follows this explicit scalar offset as well.
                if(gcn_declared||gcn_loop)copy_load(offset,10+component);
                else copy.insert(copy.end(),{0xb00a0000u|offset,0xe0300000u,0x0a000000u|((10+component)<<8)});
            }
            copy.insert(copy.end(),{0xf800020fu|(parameter<<4),0x0d0c0b0a});
        }
        copy.insert(copy.end(),{0xf80008cf,0x07060504,0xbf810000});
        initialize(export_code,es,0x57494e44494e4703ULL);
        initialize(geometry_code,gs,0x57494e44494e4704ULL);
        initialize(copy_code,copy,0x57494e44494e4705ULL);
        regs.stage_enable.raw=AmdGpu::ShaderStageEnable::VgtStages::EsGs;
        regs.es_program=regs.vs_program;regs.es_program.address=reinterpret_cast<u64>(export_code.data())>>8;
        regs.es_program.settings.num_user_regs=8;
        regs.gs_program.address=reinterpret_cast<u64>(geometry_code.data())>>8;
        regs.gs_program.settings.num_user_regs=gcn_loop?9:8;regs.gs_program.settings.num_vgprs=gcn_center_distances?4:parameters?4:2;
        regs.vs_program.address=reinterpret_cast<u64>(copy_code.data())>>8;
        regs.vs_output_control.use_vtx_point_size=!geometry_unit;
        regs.vgt_esgs_ring_itemsize=4;regs.vgt_gs_vert_itemsize[0]=components;regs.vgt_gs_max_vert_out=declared_vertices;
        regs.vgt_gs_out_prim_type.outprim_type=AmdGpu::GsOutputPrimitiveType::PointList;
        if(point_capture_two){regs.vgt_gs_instance_cnt.enable=1;regs.vgt_gs_instance_cnt.count=2;}
        memcpy(regs.gs_program.user_data.data(),&input,sizeof(input));
        AmdGpu::Buffer counter=input;counter.base_address=address+32768;counter.stride=4;counter.num_records=4;
        memcpy(regs.es_program.user_data.data()+4,&counter,sizeof(counter));
        memcpy(regs.gs_program.user_data.data()+4,&counter,sizeof(counter));
        if(gcn_loop)regs.gs_program.user_data[8]=2;
    }
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.max_viewport_width=profile.max_viewport_height=16384;
    profile.needs_clip_distance_emulation=mode=="point-size-clip"||mode=="point-size-saturated-clip"||point_packed_clip;
    profile.native_viewport_clip_conversion=clip_disabled;
    profile.native_point_capture=point_capture;
    profile.native_point_capture_stream_input=stream_inputs;
    GuestShaderFrontend frontend(profile);auto guest=frontend.BuildGraphics(regs);
    GuestShaderStage prepared_geometry;
    if(geometry_points){prepared_geometry=PreparedPointGeometry(profile,geometry_unit,frontend.ReserveStageId(),
        guest.stages[u32(Shader::SwStage::Vertex)]->info);
        guest.stages[u32(Shader::SwStage::Geometry)]=&prepared_geometry;}
    if(!compile_shaders){
        const auto cache=std::filesystem::current_path()/"gpu-point-regression-cache";
        std::filesystem::create_directories(cache);Common::FS::SetUserPath(Common::FS::PathType::CacheDir,cache);
        _wputenv_s(L"SHADPS4_NATIVE_SHADER_CACHE",cache.c_str());
        EmulatorSettings.SetPipelineCacheEnabled(true);EmulatorSettings.SetPipelineCacheArchived(false);
        Storage::DataBase::Instance().Open();
        if(!Storage::DataBase::Instance().IsOpened())throw std::runtime_error("Cannot open isolated winding metadata database");
        Storage::DataBase::Instance().FinishPreload();
    }
    std::vector<GuestResourceStage> resources;GuestShaderFrontend restored(profile);
    for(const auto* stage:guest.stages)if(stage){
        if(stage->spirv.empty()||stage->info.translation_failed)throw std::runtime_error("Winding GCN translation failed");
        if(compile_shaders)continue;
        Serialization::Archive ar;frontend.SerializeStage(ar,*stage);
        // Persistence IO is asynchronous. Restoration reads the next preload
        // session, not a file that may still be in the writer's queue.
        Storage::DataBase::Instance().Close();Storage::DataBase::Instance().Open();
        Serialization::Archive saved{ar.TakeOff()};
        const auto& restored_stage=restored.RestoreStage(saved);
        Storage::DataBase::Instance().FinishPreload();
        if(restored_stage.spirv!=stage->spirv||!saved.IsEoS())throw std::runtime_error("Winding metadata mismatch");
        if(gcn_geometry&&stage->info.hw_stage==Shader::HwStage::Geometry){
            if(restored_stage.runtime.hw.gs.vs_copy.data()||restored_stage.runtime.hw.gs.vs_copy.size()||
               restored_stage.specialization.runtime_info.hw.gs.vs_copy.data()||
               restored_stage.specialization.runtime_info.hw.gs.vs_copy.size()||
               restored_stage.runtime.hw.gs.vs_copy_hash!=stage->runtime.hw.gs.vs_copy_hash||
               stage->runtime.hw.gs.vs_copy.empty())
                throw std::runtime_error("Geometry metadata retained a borrowed copy pointer or mutated its live source");
        }
        resources.push_back({&stage->info,stage->bindings_start});
    }
    Shader::Backend::DX12::SpirvConvertOptions options;options.native_storage_buffer_uavs=true;options.runtime_viewport_flips=true;
    if(compile_shaders){
        using namespace Shader::Backend::DX12;
        if(stream_inputs){
            // Exercise the full original input domain without allocating an
            // impossible extra varying: parameter31.w, adjacency vertex5,
            // position, guest PrimitiveId and invocation ID use production
            // attribute readers. This is a compiler check, not GPU evidence.
            using namespace Shader::Backend::SPIRV;
            auto info=guest.stages[u32(Shader::SwStage::Geometry)]->info;
            auto runtime=guest.stages[u32(Shader::SwStage::Geometry)]->runtime;
            runtime.hw.gs.in_vertex_data_size=4*(Shader::IR::NumParams+1);
            runtime.hw.gs.in_primitive=AmdGpu::PrimitiveType::AdjTriangleList;
            runtime.hw.gs.output_vertices=1;runtime.hw.gs.num_invocations=1;
            info.gs_copy_data.num_attrs=1;
            info.stores.Set(Shader::IR::Attribute::Param0);
            Shader::Backend::Bindings probe_bindings{};
            EmitContext ctx(profile,runtime,info,probe_bindings);
            const auto main=ctx.OpFunction(ctx.void_id,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(ctx.void_id));
            ctx.AddExecutionMode(main,spv::ExecutionMode::InputTrianglesAdjacency);
            ctx.AddExecutionMode(main,spv::ExecutionMode::OutputPoints);
            ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,1u);
            ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,1u);
            ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);
            ctx.AddLabel(ctx.OpLabel());
            const auto values=ctx.OpCompositeConstruct(ctx.F32[4],
                EmitGetAttribute(ctx,Shader::IR::Attribute::Param31,3,5),
                EmitGetAttribute(ctx,Shader::IR::Attribute::Param0,0,0),
                EmitGetAttribute(ctx,Shader::IR::Attribute::Position0,2,5),
                ctx.OpBitcast(ctx.F32[1],EmitGetAttributeU32(ctx,Shader::IR::Attribute::PrimitiveId,0)));
            ctx.OpStore(ctx.output_position,values);ctx.OpStore(ctx.output_params[0].id,values);
            EmitEmitVertex(ctx);ctx.OpReturn();ctx.OpFunctionEnd();
            const auto words=ctx.Assemble();const LinkedSpirvStage probe{SpirvStage::Geometry,words};
            const auto result=ConvertLinkedStages(std::filesystem::path(argv[1]).wstring(),std::span(&probe,1),options);
            if(!result.success)throw std::runtime_error("Full buffered GS input compilation failed: "+result.diagnostics);
            printf("COMPILED: buffered GS full32-parameter/adjacency/patch-ID domain, signed DXIL %zu bytes\n",result.stages[0].bytecode.size());
        }
        if(point_saturated){
            const auto& vs=*guest.stages[u32(Shader::SwStage::Vertex)];
            const auto& fs=*guest.stages[u32(Shader::SwStage::Fragment)];
            printf("COMPILER INTERFACE: point parameter mask=%08x VS param0/1/2/31=%u/%u/%u/%u FS input0=%u MRT0=%u\n",
                vs.runtime.sw.vs.native_point_live_parameters,u32(vs.info.stores.GetAny(Shader::IR::Attribute::Param0)),
                u32(vs.info.stores.GetAny(Shader::IR::Attribute::Param1)),u32(vs.info.stores.GetAny(Shader::IR::Attribute::Param2)),
                u32(vs.info.stores.GetAny(Shader::IR::Attribute::Param31)),u32(fs.info.loads.GetAny(Shader::IR::Attribute::Param0)),
                u32(fs.info.stores.GetAny(Shader::IR::Attribute::RenderTarget0)));
        }
        std::vector<LinkedSpirvStage> inputs;
        for(const auto* stage:guest.stages)if(stage){
            if(point_capture&&stage->info.sw_stage==Shader::SwStage::Fragment)continue;
            const auto kind=stage->info.sw_stage==Shader::SwStage::Vertex?SpirvStage::Vertex:
                stage->info.sw_stage==Shader::SwStage::Geometry?SpirvStage::Geometry:SpirvStage::Pixel;
            inputs.push_back({kind,stage->spirv});
        }
        const auto directory=std::filesystem::current_path()/("compiled-point-"+artifact_mode);
        std::filesystem::create_directories(directory);
        for(size_t i=0;i<inputs.size();++i){
            std::ofstream spirv(directory/(std::to_string(i)+".spv"),std::ios::binary);
            spirv.write(reinterpret_cast<const char*>(inputs[i].words.data()),inputs[i].words.size_bytes());
            if(!spirv)throw std::runtime_error("Cannot save compiler SPIR-V input");
        }
        if(std::getenv("NATIVE_POINT_COMPILER_NO_FLIPS"))options.runtime_viewport_flips=false;
        const auto result=ConvertLinkedStages(std::filesystem::path(argv[1]).wstring(),inputs,options);
        if(!result.success)throw std::runtime_error("Point shader compilation failed: "+result.diagnostics);
        for(size_t i=0;i<inputs.size();++i){
            std::ofstream dxil(directory/(std::to_string(i)+".dxil"),std::ios::binary);
            dxil.write(reinterpret_cast<const char*>(result.stages[i].bytecode.data()),result.stages[i].bytecode.size());
            if(!dxil)throw std::runtime_error("Cannot save compiled point shader");
            printf("COMPILED: %s stage %zu, signed DXIL %zu bytes\n",argv[3],i,result.stages[i].bytecode.size());
        }
        if(point_capture){
            const auto& fragment=*guest.stages[u32(Shader::SwStage::Fragment)];
            const LinkedSpirvStage fragment_input{SpirvStage::Pixel,fragment.spirv};
            const auto fragment_result=ConvertLinkedStages(std::filesystem::path(argv[1]).wstring(),
                std::span(&fragment_input,1),options);
            if(!fragment_result.success)throw std::runtime_error("Captured fragment compilation failed: "+fragment_result.diagnostics);
            std::ofstream fragment_spirv(directory/"fragment.spv",std::ios::binary);
            fragment_spirv.write(reinterpret_cast<const char*>(fragment.spirv.data()),fragment.spirv.size()*4);
            std::ofstream fragment_dxil(directory/"fragment.dxil",std::ios::binary);
            const auto& bytecode=fragment_result.stages[0].bytecode;
            fragment_dxil.write(reinterpret_cast<const char*>(bytecode.data()),bytecode.size());
            if(!fragment_spirv||!fragment_dxil)throw std::runtime_error("Cannot save captured fragment");
            printf("COMPILED: %s captured fragment, signed DXIL %zu bytes\n",argv[3],bytecode.size());
            const auto* geometry=guest.stages[u32(Shader::SwStage::Geometry)];
            const auto replay_vertex=EmitNativePointReplayVertex();
            const auto replay_geometry=EmitNativePointReplayGeometry(geometry->info,geometry->runtime.hw.gs,frontend.Profile());
            const LinkedSpirvStage replay_inputs[]{{SpirvStage::Vertex,replay_vertex},
                {SpirvStage::Geometry,replay_geometry},{SpirvStage::Pixel,fragment.spirv}};
            for(size_t i=0;i<std::size(replay_inputs);++i){
                std::ofstream input(directory/("replay-"+std::to_string(i)+".spv"),std::ios::binary);
                input.write(reinterpret_cast<const char*>(replay_inputs[i].words.data()),replay_inputs[i].words.size_bytes());
                if(!input)throw std::runtime_error("Cannot save point replay SPIR-V input");
            }
            const auto replay_result=ConvertLinkedStages(std::filesystem::path(argv[1]).wstring(),replay_inputs,options);
            if(!replay_result.success)throw std::runtime_error("Point replay compilation failed: "+replay_result.diagnostics);
            for(size_t i=0;i<std::size(replay_inputs);++i){
                std::ofstream replay_spirv(directory/("replay-"+std::to_string(i)+".spv"),std::ios::binary);
                replay_spirv.write(reinterpret_cast<const char*>(replay_inputs[i].words.data()),replay_inputs[i].words.size_bytes());
                std::ofstream replay_dxil(directory/("replay-"+std::to_string(i)+".dxil"),std::ios::binary);
                const auto& code=replay_result.stages[i].bytecode;
                replay_dxil.write(reinterpret_cast<const char*>(code.data()),code.size());
                if(!replay_spirv||!replay_dxil)throw std::runtime_error("Cannot save point replay compiler artifacts");
                printf("COMPILED: %s replay stage %zu, signed DXIL %zu bytes\n",argv[3],i,code.size());
            }
        }
        return 0;
    }
    auto device=HardwareDevice();ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
    GuestShaderCache shaders(std::filesystem::path(argv[1]).wstring(),options);
    GuestPipelineCache pipelines(device.Get(),std::filesystem::path(argv[2]).wstring(),frontend,shaders);
    std::optional<GuestPointCaptureStages> capture;
    std::unique_ptr<NativePointCapturePass> capture_pass;
    std::vector<GuestResourceStage> capture_resources,replay_resources;
    if(point_capture){
        if(gcn_loop){
            auto ordinary_profile=profile;ordinary_profile.native_point_capture=false;
            GuestShaderFrontend ordinary(ordinary_profile);
            for(unsigned attempt=0;attempt<2;++attempt){
                bool required=false;try{ordinary.BuildGraphics(regs);}catch(const Shader::NativePointCapture::Required&){required=true;}
                if(!required)throw std::runtime_error("Oversized point GS did not request production capture fallback");
            }
        }
        capture=frontend.BuildPointCaptureGraphics(regs);
        capture_pass=std::make_unique<NativePointCapturePass>(device.Get());
        for(const auto* stage:capture->producer.stages)if(stage)capture_resources.push_back({&stage->info,stage->bindings_start});
        for(const auto* stage:capture->replay.stages)if(stage)replay_resources.push_back({&stage->info,stage->bindings_start});
    }
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,65536);GuestTextureCache textures(device.Get());
    GuestTextureDesc color;color.address=address+4096;color.width=16;color.height=8;color.pitch=24;color.tile_mode=8;
    color.render_target=color.writable=true;if(point_layer)color.layers=color.view_layers=3;
    const auto cl=LayoutGuestTexture(color);
    AmdGpu::CbDbExtent extent{};extent.width=16;extent.height=8;const auto depth=DescribeGuestDepthBuffer(db,{},extent);
    const auto dl=LayoutGuestTexture(depth);const auto stencil=DescribeGuestStencil(depth);const auto sl=LayoutGuestTexture(stencil);
    const std::array<DXGI_FORMAT,1> formats{cl.format};const float blend[]{1,1,1,1};Shader::PushData push{};push.xscale=push.yscale=1;
    for(unsigned mask=0;mask<4;++mask)for(unsigned ccw=0;ccw<2;++ccw)for(unsigned cull=0;cull<3;++cull)for(unsigned reverse=0;reverse<2;++reverse)
    for(unsigned edge=0;edge<(point_edge?4u:1u);++edge){
        for(unsigned i=0;i<2;++i){const float scale=point_viewport&&i==1?2.f:4.f;
            regs.viewports[i].yscale=mask&(1u<<i)?scale:-scale;}
        regs.polygon_control.front_face=ccw?AmdGpu::FrontFace::CounterClockwise:AmdGpu::FrontFace::Clockwise;
        regs.polygon_control.cull_front=cull==1;regs.polygon_control.cull_back=cull==2;
        const auto viewports=BuildGuestViewports(regs);auto state=BuildGuestGraphicsState(regs,formats,
            point_layer?DXGI_FORMAT_UNKNOWN:depth.stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:dl.format);
        if(sized_points&&!vs_unit){if(expanded_points)state.pipeline.RasterizerState.FrontCounterClockwise=!bool(viewports.flip_mask&1);
            const auto* geometry=guest.stages[u32(Shader::SwStage::Geometry)];
            if(!geometry||(gcn_geometry?(geometry->auxiliary||!geometry->runtime.hw.gs.native_point_expansion):!geometry->auxiliary))
                throw std::runtime_error("Point did not acquire the expected native geometry adaptation");}
        const auto original_state=state;
        if(capture){state.topology=D3D_PRIMITIVE_TOPOLOGY_POINTLIST;state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;}
        const auto& pipeline=pipelines.Graphics(capture?capture->replay:guest,state.pipeline);
        const u32 point_viewport_index=point_viewport&&ccw?1:0;
        const auto& point_vp=regs.viewports[point_viewport_index];
        const auto& native_vp=viewports.viewports[point_viewport_index];
        const auto point_viewports=expanded_points?BuildGuestPointViewports(viewports.viewports):std::vector<D3D12_VIEWPORT>{};
        const double center_x=clip_disabled&&reverse?point_vp.xoffset+1.125*point_vp.xscale:point_edge&&edge<2?
            native_vp.TopLeftX+(edge?native_vp.Width-0.25:0.25):point_vp.xoffset+0.25;
        const double center_y=point_edge&&edge>=2?
            native_vp.TopLeftY+(edge==3?native_vp.Height-0.25:0.25):point_vp.yoffset+0.25;
        for(unsigned warm=0;warm<2;++warm){
            std::fill_n(memory,65536,std::byte{0x5a});const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
            if(gcn_geometry||point_saturated){const u32 counters[]{77,91,0xdeadbeef,0x5a5a5a5a};memcpy(memory+32768,counters,sizeof(counters));}
            for(unsigned i=0;i<6;++i){const auto v=reverse?(3-i%3)%3:i%3;
                const float position[]{sized_points?(reverse&&!point_clip?1.125f:float((center_x-point_vp.xoffset)/point_vp.xscale)):xy[v][0],
                    sized_points?float((center_y-point_vp.yoffset)/point_vp.yscale):xy[v][1],point_edge?0.25f:0.5f,1.f};memcpy(memory+i*input.stride,position,sizeof(position));
                if(point_depth)for(u32 component=0;component<4;++component){
                    const float doubled=position[component]*2;memcpy(memory+i*input.stride+component*4,&doubled,4);}
                if(sized_points){const float size=ccw?4.f:1.f;memcpy(memory+i*input.stride+16,&size,4);
                    if(point_layer){const u32 layer=ccw?1u:2u;memcpy(memory+i*input.stride+20,&layer,4);}
                    if(point_viewport)memcpy(memory+i*input.stride+20,&point_viewport_index,4);
                    if(point_clip)for(u32 plane=0;plane<(gcn_dual_distances||native_dual_distances?8u:4u);++plane){
                        const float distance=reverse?-1.f:native_sparse_distances||gcn_sparse_distances?float(plane+1)/8.f:1.f;
                        memcpy(memory+i*input.stride+20+plane*4,&distance,4);}}
                else {const u32 index=i/3;memcpy(memory+i*32+16,&index,4);}}
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<16;++x){const u8 before[]{21,31,41,51};const float z=0.5f;
                memcpy(memory+4096+GuestPixelOffset(color,cl,0,0,x,y),before,4);
                memcpy(memory+8192+GuestPixelOffset(depth,dl,0,0,x,y),&z,4);memory[12288+GuestPixelOffset(stencil,sl,0,0,x,y)]=std::byte{7};}
            if(point_layer)for(u32 layer=1;layer<color.layers;++layer)for(u32 y=0;y<color.height;++y)for(u32 x=0;x<color.width;++x){
                const u8 before[]{21,31,41,51};memcpy(memory+4096+GuestPixelOffset(color,cl,0,layer,x,y),before,4);}
            std::vector<std::byte> expected(memory,memory+65536);std::vector<bool> half_unorm(65536);
            rasterizer.InvalidateMemory(address,65536);
            textures.Invalidate(address,65536,rasterizer.CompletedFence());
            const u32 stream_primitives=stream_inputs?2u:1u;
            if(gcn_geometry){const u32 counters[]{77+capture_instances*stream_primitives,91+capture_instances*stream_primitives*(point_capture_two?2u:1u)};memcpy(expected.data()+32768,counters,sizeof(counters));}
            if(point_saturated){const u32 counter=78;memcpy(expected.data()+32768,&counter,4);}
            for(unsigned y=0;y<8;++y)for(unsigned x=0;x<16;++x){const bool front=OriginalFront(regs,x/8,reverse);
                if(sized_points){const double half=ccw&&!geometry_unit?2.:0.5;
                    if((reverse&&!clip_disabled)||double(x)+0.5<center_x-half||double(x)+0.5>=center_x+half||
                       double(y)+0.5<center_y-half||double(y)+0.5>=center_y+half)continue;
                }else if((cull==1&&front)||(cull==2&&!front))continue;
                const u32 layer=point_layer?(ccw?1:2):0;
                const u8 result[]{u8(sized_points||front?255:0),u8(point_edge?(ccw?112:80):loop_alias_default?0:loop_live||vs_qualified?(live_lane==0?32:live_lane==2?128:191):point_packed_gcn?128:point_packed?(point_packed_clip?62:64):point_depth||point_live?128:0),u8(loop_live_xw||vs_qualified?191:255),u8(loop_reserved?128:255)};
                const auto pixel=4096+GuestPixelOffset(color,cl,0,layer,x,y);memcpy(expected.data()+pixel,result,4);
                // Only an explicitly written 0.5 UNORM channel has this tie.
                // D3D permits 0.6 integer ULP for FLOAT -> UNORM (spec3.2.3.6):
                // https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm
                for(u32 channel=0;channel<4;++channel)half_unorm[pixel+channel]=result[channel]==128;
                if(!point_layer)expected[12288+GuestPixelOffset(stencil,sl,0,0,x,y)]=std::byte(gcn_geometry?
                    7+2*capture_instances*stream_primitives*(point_capture_two?2u:1u):sized_points||front?8:6);}
            GraphicsRuntime runtime{};runtime.yz_flip_mask=viewports.flip_mask;runtime.SetViewports(viewports.viewports,expanded_points,viewports.clip_transforms);
            GuestTextureCache::Entry* target{};GuestTextureCache::Entry* ds{};
            auto color_memory=std::span(memory+4096,size_t(cl.guest_bytes)),depth_memory=std::span(memory+8192,size_t(dl.guest_bytes)),stencil_memory=std::span(memory+12288,size_t(sl.guest_bytes));
            Rasterizer::DrawArguments output_draw{sized_points?stream_primitives:6u,capture_instances};
            const auto submit=[&](ResourceBindings& bindings){
                target=&textures.Upload(color,color_memory,rasterizer.RecordingList(),rasterizer.CompletedFence());
                ds=&textures.Upload(depth,depth_memory,rasterizer.RecordingList(),rasterizer.CompletedFence(),stencil_memory);
                const Rasterizer::ColorAttachment attachment[]{ {&target->texture,target->Rtv()} };
                const Rasterizer::DepthAttachment dsv{&ds->texture,ds->Dsv()};
                rasterizer.ExecuteGraphics(pipeline,bindings,state,attachment,point_layer?nullptr:&dsv,expanded_points?point_viewports:viewports.viewports,viewports.scissors,blend,output_draw,{},nullptr,false);
            };
            if(capture){
                const auto& gs=capture->producer.stages[u32(Shader::SwStage::Geometry)]->runtime.hw.gs;
                const auto primitives=std::max(1u,NativePointCapturePass::PrimitiveBound(original_state.topology,output_draw.count));
                const auto plan=Shader::NativePointCapture::Plan::Make(primitives,gs.num_invocations,gs.output_vertices,output_draw.instances);
                const auto producer_state=NativePointCapturePass::ProducerState(original_state);
                const auto& producer_pipeline=pipelines.Graphics(capture->producer,producer_state.pipeline);
                Resource stream;
                if(stream_inputs){
                    // Independent fixture input: two generated primitives from
                    // patch7. The original ES still executes once per vertex;
                    // this tests buffered GS input, not a DS/SO producer.
                    const u32 stride=gs.in_vertex_data_size+1;
                    std::vector<u32> records(size_t(stream_primitives)*stride);
                    for(u32 primitive=0;primitive<stream_primitives;++primitive){
                        records[primitive*stride]=7;
                        memcpy(records.data()+primitive*stride+1,memory+primitive*input.stride,16);
                    }
                    stream=Buffer(device.Get(),records.size()*4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
                    void* data{};D3D12_RANGE none{};Check(stream.handle->Map(0,&none,&data));
                    memcpy(data,records.data(),records.size()*4);
                    D3D12_RANGE written{0,records.size()*4};stream.handle->Unmap(0,&written);
                }
                capture_pass->Execute(rasterizer,plan,output_draw,original_state.topology,[&](Resource& buffer,const Rasterizer::DrawArguments& draw,u64 offset,u64 bytes){
                    ExecuteGuestGraphicsResources(device.Get(),rasterizer,producer_pipeline,capture_resources,push,runtime,
                        [&](ResourceBindings& bindings){NativePointCapturePass::BindProducer(bindings,buffer,offset,bytes);
                            if(stream_inputs)NativePointCapturePass::BindInputStream(bindings,stream,0,stream.handle->GetDesc().Width);
                            rasterizer.ExecuteGraphics(producer_pipeline,bindings,producer_state,{},nullptr,
                                viewports.viewports,viewports.scissors,blend,draw,{},nullptr,false);},textures);
                    if(stream_inputs){
                        auto download=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                        buffer.UavBarrier(rasterizer.RecordingList());
                        buffer.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                        rasterizer.RecordingList()->CopyBufferRegion(download.handle.Get(),0,buffer.handle.Get(),offset,bytes);
                        rasterizer.Finish();void* data{};D3D12_RANGE range{0,SIZE_T(bytes)};
                        Check(download.handle->Map(0,&range,&data));
                        const auto* records=static_cast<const Shader::NativePointCapture::Record*>(data);
                        bool valid=true;
                        for(u32 primitive=0;primitive<stream_primitives;++primitive)
                            for(u32 invocation=0;invocation<gs.num_invocations;++invocation)
                                for(u32 emission=0;emission<gs.output_vertices;++emission){
                                    const auto& record=records[(primitive*gs.num_invocations+invocation)*gs.output_vertices+emission];
                                    // The guest loop emits twice; every other
                                    // slot stays invalid. Both primitives must
                                    // survive even though their patch ID agrees.
                                    valid&=record.valid==(emission<2?8u:0u);
                                    if(emission<2)valid&=memcmp(record.position.data(),memory+primitive*input.stride,16)==0;
                                }
                        D3D12_RANGE none{};download.handle->Unmap(0,&none);
                        if(!valid)throw std::runtime_error("Buffered GS lost primitive separation, original patch ID, position or sparse emission slots");
                        buffer.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    }
                    if(std::getenv("NATIVE_POINT_DEBUG_CAPTURE")){
                        const u64 count=std::min<u64>(bytes,Shader::NativePointCapture::RecordBytes*2);
                        auto download=Buffer(device.Get(),count,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
                        buffer.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                        rasterizer.RecordingList()->CopyBufferRegion(download.handle.Get(),0,buffer.handle.Get(),offset,count);
                        rasterizer.Finish();void* data{};D3D12_RANGE range{0,SIZE_T(count)};Check(download.handle->Map(0,&range,&data));
                        std::ofstream dump("point-runtime-records.bin",std::ios::binary);dump.write(static_cast<const char*>(data),count);
                        D3D12_RANGE empty{};download.handle->Unmap(0,&empty);
                        buffer.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    }
                },[&](Resource& buffer,const Rasterizer::DrawArguments& draw){
                    output_draw=draw;ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,replay_resources,push,runtime,
                        [&](ResourceBindings& bindings){NativePointCapturePass::BindReplay(bindings,buffer,plan.bytes);submit(bindings);},textures);
                });
            }else ExecuteGuestGraphicsResources(device.Get(),rasterizer,pipeline,resources,push,runtime,submit,textures);
            rasterizer.Finish();GuestImageReadback cr(device.Get(),*target,rasterizer.RecordingList()),dr(device.Get(),*ds,rasterizer.RecordingList());
            const auto fence=rasterizer.Flush();cr.Submitted(fence);dr.Submitted(fence);textures.Used(*target,fence);textures.Used(*ds,fence);rasterizer.Finish();
            cr.Resolve(rasterizer.CompletedFence(),color_memory);dr.Resolve(rasterizer.CompletedFence(),depth_memory,stencil_memory);
            textures.Collect(rasterizer.CompletedFence());
            bool mismatch=false;
            for(size_t byte=0;byte<65536;++byte)if(memory[byte]!=expected[byte]&&
                !(half_unorm[byte]&&memory[byte]==std::byte{127})){mismatch=true;break;}
            if(mismatch){
                std::ofstream actual("point-runtime-actual.bin",std::ios::binary),oracle("point-runtime-expected.bin",std::ios::binary);
                actual.write(reinterpret_cast<const char*>(memory),65536);oracle.write(reinterpret_cast<const char*>(expected.data()),65536);
                unsigned shown{};for(size_t byte=0;byte<65536&&shown<24;++byte)if(memory[byte]!=expected[byte]){
                    fprintf(stderr,"point mismatch byte=%zu actual=%u expected=%u\n",byte,unsigned(memory[byte]),unsigned(expected[byte]));++shown;}
                throw std::runtime_error("Actual facing/culling/stencil/guest padding mismatch: mask="+std::to_string(mask)+" ccw="+std::to_string(ccw)+" cull="+std::to_string(cull)+" reverse="+std::to_string(reverse)+" edge="+std::to_string(edge)+" warm="+std::to_string(warm));
            }
        }
    }
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};Check(messages->GetMessage(i,nullptr,&size));std::vector<std::byte> bytes(size);
        auto* message=reinterpret_cast<D3D12_MESSAGE*>(bytes.data());Check(messages->GetMessage(i,message,&size));
        if(message->Severity<=D3D12_MESSAGE_SEVERITY_ERROR)throw std::runtime_error(message->pDescription);}
    puts(sized_points?"PASS: GCN exported point size, auxiliary geometry, center clipping, viewport flips, front-facing stencil, ignored culling, cold/warm and full padding":
        "PASS: original framebuffer-area facing, mixed viewport flips, GCN front-face input, culling, front/back stencil, cold/warm and exact guest padding");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
