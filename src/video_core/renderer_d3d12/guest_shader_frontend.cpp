// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_shader_frontend.h"
#include "guest_point_shader.h"
#include "guest_point_transport.h"
#include "guest_graphics_state.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "video_core/amdgpu/liverpool.h"
#include <bit>
#include <stdexcept>
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "guest_shader_debug.h"
#endif

namespace NativeDX12 {
namespace {
void ConfigureOrderedColorTargets(Shader::RuntimeInfo& runtime, const AmdGpu::Regs& regs,
                                  bool native_alpha_factor, u32 mrt_mask = ~0u) {
    const u32 ordered_mask = GuestOrderedColorMask(regs, mrt_mask,native_alpha_factor);
    for (u32 i = 0; i < Shader::MaxColorBuffers; ++i) {
        auto& target = runtime.hw.fs.ordered_color_targets[i];
        target = {};
        if ((ordered_mask & (1u << i)) == 0) continue;
        target.enabled = true;
        target.blend = regs.blend_control[i];
        target.logic_op = regs.color_control.rop3;
        target.data_format = regs.color_buffers[i].GetDataFmt();
        target.number_format = regs.color_buffers[i].GetNumberFmt();
        target.write_mask = u8(regs.color_buffers[i].Swizzle().ApplyMask(
            regs.color_target_mask.GetMask(i)));
        target.shader_mask = u8(regs.color_shader_mask.GetMask(i));
        target.num_samples = u8(regs.color_buffers[i].NumSamples());
    }
}
} // namespace

GuestShaderStage* GuestShaderFrontend::FindStage(u64 id){
    for(auto& [key,variants]:programs_)for(auto& [index,stage]:variants)if(stage->id==id&&!stage->requires_point_capture)return stage.get();
    for(auto& [key,stage]:auxiliary_)if(stage->id==id)return stage.get();return nullptr;
}
GuestShaderStage& GuestShaderFrontend::Translate(const AmdGpu::Regs& regs,
    const AmdGpu::ComputeProgram* compute,
    Shader::HwStage hardware, Shader::SwStage logical, Shader::ShaderParams params,
    const VideoCore::GuestRuntimeContext& context, Shader::Backend::Bindings& bindings,
    std::span<const Shader::NativeImageWrite> writes,
    std::span<const Shader::NativeImageWrite> signed_packed_views,
    std::span<const Shader::NativeImageWrite> raw32_views) {
    auto runtime = VideoCore::BuildGuestRuntimeInfo(regs, compute, hardware, logical, context);
    if(hardware==Shader::HwStage::Vertex)
        runtime.hw.vs.native_polygon_point_transport=GuestPolygonPointGeometryState(regs);
    if(hardware==Shader::HwStage::Vertex&&runtime.hw.vs.native_polygon_point_transport){
        // Auxiliary rectangle/quad TCS/TES still use their declared original
        // locations. Direct VS and original TES can compact the GS interface.
        runtime.hw.vs.native_polygon_point_compact_parameters=
            regs.primitive_type!=AmdGpu::PrimitiveType::RectList&&
            regs.primitive_type!=AmdGpu::PrimitiveType::QuadList;
        const auto* fragment=context.stage_infos[u32(Shader::SwStage::Fragment)];
        if(regs.num_interp>regs.ps_inputs.size())throw std::runtime_error("Invalid polygon fragment input count");
        if(fragment)for(u32 i=0;i<regs.num_interp;++i){
            const auto& input=regs.ps_inputs[i];
            if(!fragment->loads.GetAny(Shader::IR::Attribute::Param0+i)||(input.use_default&&!input.flat_shade))continue;
            if(input.input_offset>=32)throw std::runtime_error("Invalid polygon fragment parameter");
            runtime.hw.vs.native_polygon_point_live_parameters|=1u<<input.input_offset;
        }
    }
    if(logical==Shader::SwStage::Fragment){
        ConfigureOrderedColorTargets(runtime, regs,profile_.native_alpha_blend_factor);
        runtime.hw.fs.native_point_input_compaction=GuestPolygonPointGeometryState(regs)||
            (regs.stage_enable.IsStageEnabled(u32(Shader::HwStage::Geometry))&&
            regs.vgt_gs_out_prim_type.GetPrimitiveType(0)==AmdGpu::GsOutputPrimitiveType::PointList)||
            (regs.stage_enable.raw==AmdGpu::ShaderStageEnable::VgtStages::Vs&&
             regs.primitive_type==AmdGpu::PrimitiveType::PointList);
        runtime.hw.fs.native_point_input_liveness=runtime.hw.fs.native_point_input_compaction;
        runtime.hw.fs.native_point_capture=profile_.native_point_capture&&
            regs.stage_enable.IsStageEnabled(u32(Shader::HwStage::Geometry))&&
            regs.vgt_gs_out_prim_type.GetPrimitiveType(0)==AmdGpu::GsOutputPrimitiveType::PointList;
    }
    if(logical==Shader::SwStage::Vertex)runtime.sw.vs.native_capture_base_instance=
        profile_.native_point_capture&&!profile_.native_point_capture_stream_input;
    if(logical==Shader::SwStage::Vertex)runtime.sw.vs.native_point_expansion=
        hardware==Shader::HwStage::Vertex&&regs.primitive_type==AmdGpu::PrimitiveType::PointList&&
        regs.stage_enable.raw==AmdGpu::ShaderStageEnable::VgtStages::Vs;
    if(logical==Shader::SwStage::Geometry&&runtime.hw.gs.out_primitive[0]==AmdGpu::GsOutputPrimitiveType::PointList){
        runtime.hw.gs.native_point_expansion=true;
        runtime.hw.gs.native_point_capture=profile_.native_point_capture;
        runtime.hw.gs.native_point_capture_stream_input=profile_.native_point_capture_stream_input;
        if(runtime.hw.gs.native_point_capture_stream_input&&!runtime.hw.gs.native_point_capture)
            throw std::invalid_argument("Buffered geometry inputs require point capture");
        runtime.hw.gs.native_point_clip_depth=!regs.clipper_control.clip_disable&&regs.clipper_control.ZclipEnable();
        runtime.hw.gs.native_point_clip_disable=regs.clipper_control.clip_disable;
        runtime.hw.gs.native_point_negative_one_to_one=regs.clipper_control.clip_space==AmdGpu::ClipSpace::MinusWToW;
    }
    if((logical==Shader::SwStage::Vertex&&runtime.sw.vs.native_point_expansion)||
       (logical==Shader::SwStage::Geometry&&runtime.hw.gs.native_point_expansion)){
        u32 live_parameters{};
        const auto* fragment=context.stage_infos[u32(Shader::SwStage::Fragment)];
        if(regs.num_interp>regs.ps_inputs.size())throw std::runtime_error("Invalid point fragment input count");
        if(fragment)for(u32 input=0;input<regs.num_interp;++input){
            const auto& control=regs.ps_inputs[input];
            if(!fragment->loads.GetAny(Shader::IR::Attribute::Param0+input)||
               (control.use_default&&!control.flat_shade))continue;
            if(control.input_offset>=32)throw std::runtime_error("Invalid point fragment parameter index");
            live_parameters|=1u<<control.input_offset;
            if(logical==Shader::SwStage::Geometry){
                const auto parameter=Shader::IR::Attribute::Param0+input;
                const u8 mask=fragment->fs_interpolation[input].primary==Shader::Qualifier::PerVertex?15:
                    fragment->loads.flags[Shader::Info::AttributeFlags::Index(parameter)];
                auto& consumed=runtime.hw.gs.native_point_component_masks[control.input_offset];
                consumed|=mask;
                runtime.hw.gs.native_point_live_components[control.input_offset]=u8(std::popcount(u32(consumed)));
            }
        }
        if(logical==Shader::SwStage::Vertex)runtime.sw.vs.native_point_live_parameters=live_parameters;
        else runtime.hw.gs.native_point_live_parameters=live_parameters;
        if(fragment){
            const auto transport=BuildNativePointTransport(*fragment,regs.num_interp,[&](u32 input){
                const auto& control=regs.ps_inputs[input];
                return Shader::HwFragmentRuntimeInfo::PsInput{u8(control.input_offset),bool(control.use_default),
                    bool(control.flat_shade),u8(control.default_value)};
            });
            if(logical==Shader::SwStage::Vertex)runtime.sw.vs.native_point_outputs=transport.outputs;
            else runtime.hw.gs.native_point_outputs=transport.outputs;
        }
    }
    auto stage_profile=profile_;
    if(hardware==Shader::HwStage::Compute&&stage_profile.native_compute_wave_size)
        stage_profile.subgroup_size=stage_profile.native_compute_wave_size;
    auto& variants = programs_[Key{params.hash, hardware, logical}];
    if (!variants.empty()) {
        for (auto& [index,variant] : variants) {
            // Alias promotion can change SRV/UAV classes and mip binding
            // counts between variants. Re-evaluate each candidate's metadata;
            // the first variant is not a universal resource-layout template.
            variant->info.pgm_base = params.Base();
            variant->info.user_data = params.user_data;
            variant->info.RefreshFlatBuf();
            variant->info.native_pipeline_writes.assign(writes.begin(), writes.end());
            variant->info.native_pipeline_signed_packed_views.assign(signed_packed_views.begin(),signed_packed_views.end());
            variant->info.native_pipeline_raw32_views.assign(raw32_views.begin(),raw32_views.end());
            auto requested_runtime=runtime;
            if (logical == Shader::SwStage::Fragment) {
                ConfigureOrderedColorTargets(requested_runtime, regs,profile_.native_alpha_blend_factor,
                                             variant->info.mrt_mask);
            }
            if(logical==Shader::SwStage::Fragment&&runtime.hw.fs.native_point_input_compaction){
                const auto transport=BuildNativePointTransport(variant->info,regs.num_interp,
                    [&](u32 input){return runtime.hw.fs.inputs[input];});
                requested_runtime.hw.fs.native_point_input_locations=transport.locations;
                requested_runtime.hw.fs.native_point_input_masks=transport.masks;
                requested_runtime.hw.fs.native_point_input_offsets=transport.offsets;
                requested_runtime.hw.fs.native_point_input_widths=transport.widths;
            }
            const Shader::StageSpecialization requested(variant->info, requested_runtime, stage_profile, bindings);
            if (variant->specialization == requested) {
                if(variant->requires_point_capture)
                    throw Shader::NativePointCapture::Required("Cached guest point specialization requires capture/replay");
#ifdef SHADPS4_NATIVE_DX12_CORE
                if(!variant->debug_collected)DumpShaderSource(*variant,index,params.code);
                CollectShaderDebug(*variant,index,params.code);
#endif
                // Descriptor-array mip fallbacks consume more than one image
                // register. Reuse the exact emitted allocation, not an estimate
                // based on the number of metadata image records.
                bindings = variant->bindings_end;
                return *variant;
            }
        }
    }
    const auto index=variants.empty()?size_t{0}:variants.rbegin()->first+1;
    if(!variants.empty()&&!index)throw std::overflow_error("Native shader permutation index overflow");
    auto stage = std::make_unique<GuestShaderStage>();
    stage->info = Shader::Info(hardware, logical, params);
#ifdef SHADPS4_NATIVE_DX12_CORE
    DumpShaderSource(*stage,index,params.code);
#endif
    stage->info.native_pipeline_writes.assign(writes.begin(), writes.end());
    stage->info.native_pipeline_signed_packed_views.assign(signed_packed_views.begin(),signed_packed_views.end());
    stage->info.native_pipeline_raw32_views.assign(raw32_views.begin(),raw32_views.end());
    stage->runtime = runtime;
    stage->bindings_start = bindings;
    auto mutable_runtime = runtime;
    auto allocated = bindings;
    // Always release temporary IR, including failed translation/emission.
    struct PoolScope { Shader::Pools& pools; ~PoolScope() { pools.ReleaseContents(); } } scope{pools_};
    const auto program = Shader::TranslateProgram(params.code, pools_, stage->info,
        mutable_runtime, stage_profile);
    if (stage->info.translation_failed)
        throw std::runtime_error("Guest shader translation failed");
    if (logical == Shader::SwStage::Fragment) {
        ConfigureOrderedColorTargets(mutable_runtime, regs,profile_.native_alpha_blend_factor,
                                     stage->info.mrt_mask);
    }
    if(logical==Shader::SwStage::Fragment&&mutable_runtime.hw.fs.native_point_input_compaction){
        const auto transport=BuildNativePointTransport(stage->info,regs.num_interp,
            [&](u32 input){return mutable_runtime.hw.fs.inputs[input];});
        mutable_runtime.hw.fs.native_point_input_locations=transport.locations;
        mutable_runtime.hw.fs.native_point_input_masks=transport.masks;
        mutable_runtime.hw.fs.native_point_input_offsets=transport.offsets;
        mutable_runtime.hw.fs.native_point_input_widths=transport.widths;
    }
    try{stage->spirv = Shader::Backend::SPIRV::EmitSPIRV(stage_profile, mutable_runtime, program, allocated);}
    catch(const Shader::NativePointCapture::Required&){stage->requires_point_capture=true;}
    if (stage->spirv.empty()&&!stage->requires_point_capture) throw std::runtime_error("Guest shader produced no intermediate");
    stage->runtime = mutable_runtime;
    stage->specialization = Shader::StageSpecialization(stage->info, mutable_runtime, stage_profile, bindings);
    stage->id = ReserveStageId();
    stage->bindings_end = allocated;
    bindings = allocated;
    auto& result = *stage;
#ifdef SHADPS4_NATIVE_DX12_CORE
    if(!stage->requires_point_capture){LoadShaderPatch(*stage,index);CollectShaderDebug(*stage,index,params.code);}
#endif
    variants.emplace(index,std::move(stage));
    if(result.requires_point_capture)
        throw Shader::NativePointCapture::Required("Guest point specialization requires capture/replay");
    return result;
}

GuestGraphicsStages GuestShaderFrontend::BuildGraphics(AmdGpu::Liverpool& gpu,
    u16 vertex_sgpr_offset, u32 instance_sgpr_offset, bool is_indirect) {
    return BuildGraphics(gpu.regs, vertex_sgpr_offset, instance_sgpr_offset, is_indirect);
}

GuestGraphicsStages GuestShaderFrontend::BuildGraphics(const AmdGpu::Regs& regs,
    u16 vertex_sgpr_offset, u32 instance_sgpr_offset, bool is_indirect) {
    using Shader::HwStage;
    using Shader::SwStage;

    VideoCore::GuestRuntimeContext context{profile_};
    context.indirect.vertex_sgpr_offset = vertex_sgpr_offset;
    context.indirect.instance_sgpr_offset = instance_sgpr_offset;
    context.indirect.is_indirect = is_indirect;
    // D3D12 clip depth is 0..W; original IR lowers PS4 -W..W when required.
    context.depth_clip_control = false;
    for (u32 cb = 0; cb < Shader::MaxColorBuffers; ++cb) {
        const auto& source = regs.color_buffers[cb];
        if (!source || !regs.color_target_mask.GetMask(cb)) continue;
        auto& target = context.color_buffers[cb];
        target.data_format = source.GetDataFmt();
        target.num_format = source.GetNumberFmt();
        target.num_conversion = source.GetNumberConversion();
        target.export_format = regs.color_export_format.GetFormat(cb);
        target.swizzle = source.Swizzle();
        const auto& blend = regs.blend_control[cb];
        target.blend_self_scale = blend.enable && !source.info.blend_bypass &&
            (blend.color_func == AmdGpu::BlendControl::BlendFunc::Min ||
             blend.color_func == AmdGpu::BlendControl::BlendFunc::Max) &&
            blend.color_src_factor == AmdGpu::BlendControl::BlendFactor::SrcColor &&
            blend.color_dst_factor == AmdGpu::BlendControl::BlendFactor::DstColor;
    }
    Shader::Backend::Bindings bindings{};
    GuestGraphicsStages result;
    std::vector<std::pair<HwStage, SwStage>> active_stages;
    const auto bind = [&](HwStage hardware, SwStage logical, bool required = true) {
        const auto index = static_cast<u32>(hardware);
        const auto* program = regs.ProgramForStage(index);
        if (!regs.stage_enable.IsStageEnabled(index) || !program || !program->Address<u32*>()) {
            if (required) throw std::runtime_error("Missing required guest graphics shader");
            return;
        }
        auto& stage = Translate(regs, nullptr, hardware, logical, AmdGpu::GetParams(*program), context, bindings);
        active_stages.emplace_back(hardware, logical);
        const auto logical_index = static_cast<u32>(logical);
        result.stages[logical_index] = &stage;
        context.stage_infos[logical_index] = &stage.info;
    };
    bind(HwStage::Fragment, SwStage::Fragment, false);
    if (const auto* fragment = result.stages[static_cast<u32>(SwStage::Fragment)])
        result.mrt_mask = fragment->info.mrt_mask;
    result.ordered_color_mask = GuestOrderedColorMask(regs, result.mrt_mask,
                                                       profile_.native_alpha_blend_factor);
    switch (regs.stage_enable.raw) {
    case AmdGpu::ShaderStageEnable::VgtStages::Vs:
        bind(HwStage::Vertex, SwStage::Vertex); break;
    case AmdGpu::ShaderStageEnable::VgtStages::LsHs:
        bind(HwStage::Hull, SwStage::TessellationControl);
        bind(HwStage::Vertex, SwStage::TessellationEval);
        bind(HwStage::Local, SwStage::Vertex); break;
    case AmdGpu::ShaderStageEnable::VgtStages::EsGs:
        if (regs.vgt_gs_mode.onchip || regs.vgt_strmout_config.raw)
            throw std::runtime_error("Native on-chip geometry/stream output unsupported");
        bind(HwStage::Export, SwStage::Vertex);
        bind(HwStage::Geometry, SwStage::Geometry); break;
    case AmdGpu::ShaderStageEnable::VgtStages::LsHsEsGs:
        if (regs.vgt_gs_mode.onchip || regs.vgt_strmout_config.raw)
            throw std::runtime_error("Native on-chip geometry/stream output unsupported");
        bind(HwStage::Hull, SwStage::TessellationControl);
        bind(HwStage::Export, SwStage::TessellationEval);
        bind(HwStage::Local, SwStage::Vertex);
        bind(HwStage::Geometry, SwStage::Geometry); break;
    default: throw std::runtime_error("Unsupported guest graphics stage combination");
    }
    // Discover writes across the whole pipeline before final resource emission.
    // Preserve stage order because tessellation/runtime reflection has dependencies.
    std::vector<Shader::NativeImageWrite> pipeline_writes;
    std::vector<Shader::NativeImageWrite> signed_packed_views;
    std::vector<Shader::NativeImageWrite> raw32_views;
    for (const auto* stage : result.stages) {
        if (!stage) continue;
        for (const auto& image : stage->info.images){
            if (image.is_written)
                pipeline_writes.push_back({image.GetSharp(stage->info), image.is_array, image.is_depth});
            const auto sharp=image.GetSharp(stage->info);
            if(!image.is_depth&&(u32(sharp.GetDataFmt())==4||u32(sharp.GetDataFmt())==5||u32(sharp.GetDataFmt())==6||u32(sharp.GetDataFmt())==9||u32(sharp.GetDataFmt())==10||u32(sharp.GetDataFmt())==34))
                raw32_views.push_back({sharp,image.is_array,image.is_depth});
            if(u32(sharp.GetDataFmt())==9&&(sharp.GetNumberFmt()==AmdGpu::NumberFormat::Snorm||
               sharp.GetNumberFmt()==AmdGpu::NumberFormat::Sint))
                signed_packed_views.push_back({sharp,image.is_array,image.is_depth});
        }
    }
    if (!pipeline_writes.empty()) {
        bindings = {};
        context.stage_infos.fill(nullptr);
        for (const auto& [hardware, logical] : active_stages) {
            const auto* program = regs.ProgramForStage(static_cast<u32>(hardware));
            auto& stage = Translate(regs, nullptr, hardware, logical, AmdGpu::GetParams(*program),
                                    context, bindings, pipeline_writes,signed_packed_views,raw32_views);
            result.stages[static_cast<u32>(logical)] = &stage;
            context.stage_infos[static_cast<u32>(logical)] = &stage.info;
        }
    }
    if(regs.primitive_type==AmdGpu::PrimitiveType::RectList||
       regs.primitive_type==AmdGpu::PrimitiveType::QuadList) {
        using namespace Shader::Backend::SPIRV;
        const auto* fragment=result.stages[u32(SwStage::Fragment)];
        const Shader::HwFragmentRuntimeInfo fragment_runtime=fragment?fragment->runtime.hw.fs:Shader::HwFragmentRuntimeInfo{};
        const auto add_auxiliary=[&](AuxShaderType type,SwStage logical) {
            const auto index=u32(logical);
            if(result.stages[index])throw std::runtime_error("Guest rectangle/quad has conflicting tessellation stages");
            const u32 input_vertices=type==AuxShaderType::RectListTCS?3:4;
            auto words=EmitAuxilaryTessShader(type,fragment_runtime,input_vertices);
            auto found=auxiliary_.find(words);
            if(found==auxiliary_.end()) {
                auto stage=std::make_unique<GuestShaderStage>();
                stage->id=ReserveStageId();stage->auxiliary=true;stage->info.sw_stage=logical;
                // The auxiliary TES forwards complete parameter vectors. The
                // polygon-point GS consumes that interface, not the original
                // VS's export mask (which may be sparse or differently sized).
                if(logical==SwStage::TessellationEval){
                    for(u32 i=0;i<fragment_runtime.num_inputs;++i){
                        const auto& input=fragment_runtime.inputs[i];
                        if(input.IsDefault())continue;
                        for(u32 component=0;component<4;++component)
                            stage->info.stores.Set(Shader::IR::Attribute::Param0+input.param_index,component);
                    }
                }
                stage->spirv=words;
                found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
            }
            result.stages[index]=found->second.get();
        };
        add_auxiliary(regs.primitive_type==AmdGpu::PrimitiveType::RectList?
            AuxShaderType::RectListTCS:AuxShaderType::QuadListTCS,SwStage::TessellationControl);
        add_auxiliary(AuxShaderType::PassthroughTES,SwStage::TessellationEval);
    }
    const auto* vertex=result.stages[u32(SwStage::Vertex)];
    if(vertex&&GuestPolygonPointGeometryState(regs)){
        const auto* producer=result.stages[u32(SwStage::TessellationEval)]?
            result.stages[u32(SwStage::TessellationEval)]:vertex;
        const auto* fragment=result.stages[u32(SwStage::Fragment)];
        const auto& poly=regs.polygon_control;
        auto words=EmitGuestPolygonPoints(producer->info,fragment?&fragment->info:nullptr,
            fragment?fragment->runtime.hw.fs:Shader::HwFragmentRuntimeInfo{},
            {bool(poly.cull_front)&&regs.primitive_type!=AmdGpu::PrimitiveType::RectList,
             bool(poly.cull_back)&&regs.primitive_type!=AmdGpu::PrimitiveType::RectList,
             poly.front_face==AmdGpu::FrontFace::CounterClockwise,
             poly.provoking_vtx_last==AmdGpu::ProvokingVtxLast::Last,
             producer==vertex&&profile_.needs_clip_distance_emulation&&
                 producer->info.stores.GetAny(Shader::IR::Attribute::ClipDistance),
             regs.clipper_control.ZclipEnable(),
             u32(regs.primitive_type==AmdGpu::PrimitiveType::TriangleFan||regs.primitive_type==AmdGpu::PrimitiveType::Polygon),
             poly.NeedsBias(),regs.depth_buffer.z_info.format!=AmdGpu::DepthBuffer::ZFormat::Z16,
             poly.enable_polygon_offset_front?regs.poly_offset.front_offset:regs.poly_offset.back_offset,
             (poly.enable_polygon_offset_front?regs.poly_offset.front_scale:regs.poly_offset.back_scale)/16.f,
             regs.poly_offset.depth_bias,!producer->auxiliary});
        auto found=auxiliary_.find(words);
        if(found==auxiliary_.end()){
            auto stage=std::make_unique<GuestShaderStage>();stage->id=ReserveStageId();stage->auxiliary=true;
            stage->info.sw_stage=SwStage::Geometry;stage->info.hw_stage=HwStage::Geometry;stage->spirv=words;
            found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
        }
        result.stages[u32(SwStage::Geometry)]=found->second.get();
    }
    if(vertex&&vertex->runtime.sw.vs.native_point_expansion&&vertex->info.stores.GetAny(Shader::IR::Attribute::PointSize)){
        const bool emulated_clip=profile_.needs_clip_distance_emulation&&vertex->info.stores.GetAny(Shader::IR::Attribute::ClipDistance);
        auto words=EmitGuestPointGeometry(vertex->info,!regs.clipper_control.clip_disable&&regs.clipper_control.ZclipEnable(),emulated_clip,
            vertex->runtime.sw.vs.native_point_live_parameters,vertex->runtime.sw.vs.native_point_outputs);
        auto found=auxiliary_.find(words);
        if(found==auxiliary_.end()){
            auto stage=std::make_unique<GuestShaderStage>();stage->id=ReserveStageId();stage->auxiliary=true;
            stage->info.sw_stage=SwStage::Geometry;stage->info.hw_stage=HwStage::Geometry;stage->spirv=words;
            found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
        }
        result.stages[u32(SwStage::Geometry)]=found->second.get();
    }
    return result;
}

GuestPointCaptureStages GuestShaderFrontend::BuildPointCaptureGraphics(const AmdGpu::Regs& regs,
    u16 vertex_sgpr_offset,u32 instance_sgpr_offset,bool is_indirect){
    using Shader::SwStage;
    const bool tessellated=regs.stage_enable.raw==AmdGpu::ShaderStageEnable::VgtStages::LsHsEsGs;
    if((regs.stage_enable.raw!=AmdGpu::ShaderStageEnable::VgtStages::EsGs&&!tessellated)||
       regs.vgt_gs_out_prim_type.GetPrimitiveType(0)!=AmdGpu::GsOutputPrimitiveType::PointList)
        throw std::runtime_error("Native point capture requires original export/geometry point output");
    struct RestoreProfile {Shader::Profile& target;Shader::Profile original;~RestoreProfile(){target=original;}};
    RestoreProfile restore{profile_,profile_};profile_.native_point_capture=true;
    const u32 patch_vertices=tessellated?regs.ls_hs_config.hs_input_control_points:
        regs.primitive_type==AmdGpu::PrimitiveType::RectList?3u:
        regs.primitive_type==AmdGpu::PrimitiveType::QuadList?4u:0u;
    if(tessellated&&(!patch_vertices||patch_vertices>32))
        throw std::invalid_argument("Native captured tessellation requires 1..32 input control points");
    profile_.native_point_capture_stream_input|=patch_vertices!=0;
    auto original=BuildGraphics(regs,vertex_sgpr_offset,instance_sgpr_offset,is_indirect);
    const auto* geometry=original.stages[u32(SwStage::Geometry)];
    if(!geometry||geometry->auxiliary||!geometry->runtime.hw.gs.native_point_capture)
        throw std::logic_error("Missing native point capture producer");
    GuestPointCaptureStages result;result.producer=original;
    result.producer.stages[u32(SwStage::Fragment)]=nullptr;result.producer.mrt_mask=0;
    result.producer.ordered_color_mask=0;
    result.replay.mrt_mask=original.mrt_mask;
    result.replay.ordered_color_mask=original.ordered_color_mask;
    result.replay.stages[u32(SwStage::Fragment)]=original.stages[u32(SwStage::Fragment)];
    const auto auxiliary=[&](Shader::SwStage logical,std::vector<u32> words){
        auto found=auxiliary_.find(words);
        if(found==auxiliary_.end()){
            auto stage=std::make_unique<GuestShaderStage>();stage->id=ReserveStageId();stage->auxiliary=true;
            stage->info.sw_stage=logical;stage->info.hw_stage=logical==SwStage::Vertex?Shader::HwStage::Vertex:Shader::HwStage::Geometry;
            stage->spirv=words;found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
        }
        return found->second.get();
    };
    result.replay.stages[u32(SwStage::Vertex)]=auxiliary(SwStage::Vertex,EmitNativePointReplayVertex());
    result.replay.stages[u32(SwStage::Geometry)]=auxiliary(SwStage::Geometry,
        EmitNativePointReplayGeometry(geometry->info,geometry->runtime.hw.gs,profile_));
    if(patch_vertices){
        result.stream_producer=BuildPrimitiveStream(original);
        result.stream_patch_vertices=patch_vertices;
        result.stream_tessellated=tessellated;
        result.producer=BuildBufferedPointCapture(original);
    }
    return result;
}

GuestGraphicsStages GuestShaderFrontend::BuildPrimitiveStream(const GuestGraphicsStages& source){
    using Shader::SwStage;
    const auto* geometry=source.stages[u32(SwStage::Geometry)];
    if(!geometry||geometry->auxiliary)
        throw std::invalid_argument("Primitive stream requires original guest GS input metadata");
    // A replacement SPIR-V binary can read a different component while the
    // original IR reflection remains unchanged. Preserve its declared payload.
    auto words=EmitNativePrimitiveStream(geometry->runtime.hw.gs,geometry->is_patched?nullptr:&geometry->info);
    auto found=auxiliary_.find(words);
    if(found==auxiliary_.end()){
        auto stage=std::make_unique<GuestShaderStage>();stage->id=ReserveStageId();stage->auxiliary=true;
        stage->info.hw_stage=Shader::HwStage::Geometry;stage->info.sw_stage=SwStage::Geometry;
        stage->spirv=words;found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
    }
    // Retain the exact original VS/HS/DS stage objects and their bindings.
    // Only original GS effects move to the later buffered-input pass.
    auto result=source;result.stages[u32(SwStage::Geometry)]=found->second.get();
    result.stages[u32(SwStage::Fragment)]=nullptr;result.mrt_mask=result.ordered_color_mask=0;
    return result;
}
GuestGraphicsStages GuestShaderFrontend::BuildBufferedPointCapture(const GuestGraphicsStages& source){
    using Shader::SwStage;
    auto* geometry=source.stages[u32(SwStage::Geometry)];
    if(!geometry||geometry->auxiliary||!geometry->runtime.hw.gs.native_point_capture||
       !geometry->runtime.hw.gs.native_point_capture_stream_input)
        throw std::invalid_argument("Buffered point consumer requires original captured-input GS");
    auto words=EmitNativePointReplayVertex();auto found=auxiliary_.find(words);
    if(found==auxiliary_.end()){
        auto stage=std::make_unique<GuestShaderStage>();stage->id=ReserveStageId();stage->auxiliary=true;
        stage->info.hw_stage=Shader::HwStage::Vertex;stage->info.sw_stage=SwStage::Vertex;stage->spirv=words;
        found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
    }
    GuestGraphicsStages result;
    result.stages[u32(SwStage::Vertex)]=found->second.get();
    result.stages[u32(SwStage::Geometry)]=geometry;
    // Original producer stages ran in BuildPrimitiveStream. No original VS,
    // HS, DS or FS effects may be repeated in this GS-only capture pass.
    return result;
}
GuestShaderStage& GuestShaderFrontend::BuildCompute(AmdGpu::Liverpool& gpu) {
    VideoCore::GuestRuntimeContext context{profile_};
    Shader::Backend::Bindings bindings{};
    return Translate(gpu.regs, &gpu.GetCsRegs(), Shader::HwStage::Compute, Shader::SwStage::Compute,
        AmdGpu::GetParams(gpu.GetCsRegs()), context, bindings);
}
}
