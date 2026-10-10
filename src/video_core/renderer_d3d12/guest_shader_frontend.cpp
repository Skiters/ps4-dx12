// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_shader_frontend.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "video_core/amdgpu/liverpool.h"
#include <stdexcept>

namespace NativeDX12 {
GuestShaderStage& GuestShaderFrontend::Translate(const AmdGpu::Regs& regs,
    const AmdGpu::ComputeProgram* compute,
    Shader::HwStage hardware, Shader::SwStage logical, Shader::ShaderParams params,
    const VideoCore::GuestRuntimeContext& context, Shader::Backend::Bindings& bindings,
    std::span<const Shader::NativeImageWrite> writes) {
    const auto runtime = VideoCore::BuildGuestRuntimeInfo(regs, compute, hardware, logical, context);
    auto& variants = programs_[Key{params.hash, hardware, logical}];
    if (!variants.empty()) {
        for (auto& variant : variants) {
            // Alias promotion can change SRV/UAV classes and mip binding
            // counts between variants. Re-evaluate each candidate's metadata;
            // the first variant is not a universal resource-layout template.
            variant->info.pgm_base = params.Base();
            variant->info.user_data = params.user_data;
            variant->info.RefreshFlatBuf();
            variant->info.native_pipeline_writes.assign(writes.begin(), writes.end());
            const Shader::StageSpecialization requested(variant->info, runtime, profile_, bindings);
            if (variant->specialization == requested) {
                // Descriptor-array mip fallbacks consume more than one image
                // register. Reuse the exact emitted allocation, not an estimate
                // based on the number of metadata image records.
                bindings = variant->bindings_end;
                return *variant;
            }
        }
    }
    auto stage = std::make_unique<GuestShaderStage>();
    stage->info = Shader::Info(hardware, logical, params);
    stage->info.native_pipeline_writes.assign(writes.begin(), writes.end());
    stage->runtime = runtime;
    stage->bindings_start = bindings;
    auto mutable_runtime = runtime;
    auto allocated = bindings;
    // Always release temporary IR, including failed translation/emission.
    struct PoolScope { Shader::Pools& pools; ~PoolScope() { pools.ReleaseContents(); } } scope{pools_};
    const auto program = Shader::TranslateProgram(params.code, pools_, stage->info,
        mutable_runtime, profile_);
    if (stage->info.translation_failed)
        throw std::runtime_error("Guest shader translation failed");
    stage->spirv = Shader::Backend::SPIRV::EmitSPIRV(profile_, mutable_runtime, program, allocated);
    if (stage->spirv.empty()) throw std::runtime_error("Guest shader produced no intermediate");
    stage->runtime = mutable_runtime;
    stage->specialization = Shader::StageSpecialization(stage->info, mutable_runtime, profile_, bindings);
    stage->id = next_id_++;
    stage->bindings_end = allocated;
    bindings = allocated;
    auto& result = *stage;
    variants.push_back(std::move(stage));
    return result;
}

GuestGraphicsStages GuestShaderFrontend::BuildGraphics(AmdGpu::Liverpool& gpu,
    u16 vertex_sgpr_offset, u32 instance_sgpr_offset) {
    return BuildGraphics(gpu.regs, vertex_sgpr_offset, instance_sgpr_offset);
}

GuestGraphicsStages GuestShaderFrontend::BuildGraphics(const AmdGpu::Regs& regs,
    u16 vertex_sgpr_offset, u32 instance_sgpr_offset) {
    using Shader::HwStage;
    using Shader::SwStage;

    VideoCore::GuestRuntimeContext context{profile_};
    context.indirect.vertex_sgpr_offset = vertex_sgpr_offset;
    context.indirect.instance_sgpr_offset = instance_sgpr_offset;
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
    for (const auto* stage : result.stages) {
        if (!stage) continue;
        for (const auto& image : stage->info.images)
            if (image.is_written)
                pipeline_writes.push_back({image.GetSharp(stage->info), image.is_array, image.is_depth});
    }
    if (!pipeline_writes.empty()) {
        bindings = {};
        context.stage_infos.fill(nullptr);
        for (const auto& [hardware, logical] : active_stages) {
            const auto* program = regs.ProgramForStage(static_cast<u32>(hardware));
            auto& stage = Translate(regs, nullptr, hardware, logical, AmdGpu::GetParams(*program),
                                    context, bindings, pipeline_writes);
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
                stage->id=next_id_++;stage->info.sw_stage=logical;
                stage->spirv=words;
                found=auxiliary_.emplace(std::move(words),std::move(stage)).first;
            }
            result.stages[index]=found->second.get();
        };
        add_auxiliary(regs.primitive_type==AmdGpu::PrimitiveType::RectList?
            AuxShaderType::RectListTCS:AuxShaderType::QuadListTCS,SwStage::TessellationControl);
        add_auxiliary(AuxShaderType::PassthroughTES,SwStage::TessellationEval);
    }
    return result;
}

GuestShaderStage& GuestShaderFrontend::BuildCompute(AmdGpu::Liverpool& gpu) {
    VideoCore::GuestRuntimeContext context{profile_};
    Shader::Backend::Bindings bindings{};
    return Translate(gpu.regs, &gpu.GetCsRegs(), Shader::HwStage::Compute, Shader::SwStage::Compute,
        AmdGpu::GetParams(gpu.GetCsRegs()), context, bindings);
}
}
