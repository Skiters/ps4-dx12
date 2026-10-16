// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point_capture.h"
#include "shader_recompiler/ir/debug_print.h"
#include "shader_recompiler/ir/microinstruction.h"
#include <stdexcept>
#include <bit>

namespace Shader::Backend::SPIRV {

void EmitPrologue(EmitContext& ctx) {
    if (ctx.hw_stage == HwStage::Fragment) {
        ctx.DefineAmdPerVertexAttribs();
    }
    if (ctx.info.loads.Get(IR::Attribute::WorkgroupIndex)) {
        ctx.DefineWorkgroupIndex();
    }
    ctx.DefineBufferProperties();
}

void ConvertDepthMode(EmitContext& ctx) {
    const Id type{ctx.F32[1]};
    const Id position{ctx.OpLoad(ctx.F32[4], ctx.output_position)};
    const Id z{ctx.OpCompositeExtract(type, position, 2u)};
    const Id w{ctx.OpCompositeExtract(type, position, 3u)};
    const Id screen_depth{ctx.OpFMul(type, ctx.OpFAdd(type, z, w), ctx.Constant(type, 0.5f))};
    const Id vector{ctx.OpCompositeInsert(ctx.F32[4], screen_depth, position, 2u)};
    ctx.OpStore(ctx.output_position, vector);
}

void ConvertPositionToClipSpace(EmitContext& ctx) {
    ASSERT_MSG(ctx.profile.native_viewport_clip_conversion||!ctx.info.stores.GetAny(IR::Attribute::ViewportIndex),
               "Multi-viewport with shader clip space conversion not yet implemented.");

    const Id type{ctx.F32[1]};
    Id position{ctx.OpLoad(ctx.F32[4], ctx.output_position)};
    const Id x{ctx.OpCompositeExtract(type, position, 0u)};
    const Id y{ctx.OpCompositeExtract(type, position, 1u)};
    const Id z{ctx.OpCompositeExtract(type, position, 2u)};
    const Id w{ctx.OpCompositeExtract(type, position, 3u)};
    const Id xoffset_ptr{ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::PushConstant, type),
                                           ctx.push_data_block,
                                           ctx.ConstU32(PushData::XOffsetIndex))};
    Id xoffset{ctx.OpLoad(type, xoffset_ptr)};
    const Id yoffset_ptr{ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::PushConstant, type),
                                           ctx.push_data_block,
                                           ctx.ConstU32(PushData::YOffsetIndex))};
    Id yoffset{ctx.OpLoad(type, yoffset_ptr)};
    const Id xscale_ptr{ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::PushConstant, type),
                                          ctx.push_data_block,
                                          ctx.ConstU32(PushData::XScaleIndex))};
    Id xscale{ctx.OpLoad(type, xscale_ptr)};
    const Id yscale_ptr{ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::PushConstant, type),
                                          ctx.push_data_block,
                                          ctx.ConstU32(PushData::YScaleIndex))};
    Id yscale{ctx.OpLoad(type, yscale_ptr)};
    if(ctx.profile.native_viewport_clip_conversion){
        Id viewport=ctx.ConstU32(0u);
        if(ctx.info.stores.GetAny(IR::Attribute::ViewportIndex))
            viewport=ValidId(ctx.native_point_misc)?
                ctx.OpCompositeExtract(ctx.U32[1],ctx.OpLoad(ctx.U32[4],ctx.native_point_misc),2u):
                ctx.OpLoad(ctx.U32[1],ctx.output_viewport_index);
        const auto safe_viewport=ctx.OpUMin(ctx.U32[1],viewport,ctx.ConstU32(15u));
        const auto transform=[&](u32 component){return ctx.OpLoad(type,ctx.OpAccessChain(
            ctx.TypePointer(spv::StorageClass::Uniform,type),ctx.native_point_runtime,
            ctx.ConstU32(12u),safe_viewport,ctx.ConstU32(component)));};
        xoffset=transform(0);yoffset=transform(1);xscale=transform(2);yscale=transform(3);
    }
    const Id vport_w =
        ctx.Constant(type, float(std::min<u32>(ctx.profile.max_viewport_width / 2, 8_KB)));
    const Id wnd_x = ctx.OpFAdd(type, ctx.OpFMul(type, x, xscale), xoffset);
    const Id ndc_x = ctx.OpFSub(type, ctx.OpFDiv(type, wnd_x, vport_w), ctx.Constant(type, 1.f));
    const Id vport_h =
        ctx.Constant(type, float(std::min<u32>(ctx.profile.max_viewport_height / 2, 8_KB)));
    const Id wnd_y = ctx.OpFAdd(type, ctx.OpFMul(type, y, yscale), yoffset);
    const Id ndc_y = ctx.OpFSub(type, ctx.OpFDiv(type, wnd_y, vport_h), ctx.Constant(type, 1.f));
    const Id vector{ctx.OpCompositeConstruct(ctx.F32[4], std::array<Id, 4>({ndc_x, ndc_y, z, w}))};
    ctx.OpStore(ctx.output_position, vector);
}

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.hw_stage == HwStage::Vertex &&
        ctx.runtime_info.hw.vs.emulate_depth_negative_one_to_one) {
        ConvertDepthMode(ctx);
    }
    if (ctx.hw_stage == HwStage::Vertex && ctx.runtime_info.hw.vs.clip_disable) {
        ConvertPositionToClipSpace(ctx);
    }
}

void EmitDiscard(EmitContext& ctx) {
    ctx.OpDemoteToHelperInvocationEXT();
}

void EmitDiscardCond(EmitContext& ctx, Id condition) {
    const Id kill_label{ctx.OpLabel()};
    const Id merge_label{ctx.OpLabel()};
    ctx.OpSelectionMerge(merge_label, spv::SelectionControlMask::MaskNone);
    ctx.OpBranchConditional(condition, kill_label, merge_label);
    ctx.AddLabel(kill_label);
    ctx.OpDemoteToHelperInvocationEXT();
    ctx.OpBranch(merge_label);
    ctx.AddLabel(merge_label);
}

void EmitEmitVertex(EmitContext& ctx) {
    if(ctx.runtime_info.hw.gs.native_point_capture){EmitNativePointCapture(ctx);return;}
    if(ctx.runtime_info.hw.gs.native_point_expansion){
        const auto original_position=ctx.OpLoad(ctx.F32[4],ctx.output_position);
        if(ctx.runtime_info.hw.gs.native_point_negative_one_to_one)ConvertDepthMode(ctx);
        if(ctx.runtime_info.hw.gs.native_point_clip_disable)ConvertPositionToClipSpace(ctx);
        const auto position=ctx.OpLoad(ctx.F32[4],ctx.output_position);
        const auto size=ctx.info.stores.GetAny(IR::Attribute::PointSize)?
            ctx.OpLoad(ctx.F32[1],ctx.output_point_size):ctx.ConstF32(1.f);
        const bool routed_viewport=ctx.info.stores.GetAny(IR::Attribute::ViewportIndex);
        const auto viewport=routed_viewport?ctx.OpLoad(ctx.U32[1],ctx.output_viewport_index):ctx.ConstU32(0u);
        std::vector<PointOutput> outputs;
        std::vector<PointDistance> distances;
        for(u32 attribute=0;attribute<IR::NumParams;++attribute){
            if(!NativePointOutputWidth(ctx.runtime_info.hw.gs.native_point_outputs,attribute))continue;
            const auto& output=ctx.output_params[attribute];
            const auto type=ctx.F32[output.num_components];
            outputs.push_back({type,output.id,ctx.OpLoad(type,output.id)});
        }
        if(ctx.info.stores.GetAny(IR::Attribute::RenderTargetIndex))
            outputs.push_back({ctx.U32[1],ctx.output_layer,ctx.OpLoad(ctx.U32[1],ctx.output_layer)});
        if(routed_viewport)outputs.push_back({ctx.U32[1],ctx.output_viewport_index,viewport});
        for(const auto attribute:{IR::Attribute::ClipDistance,IR::Attribute::CullDistance}){
            if(!ctx.info.stores.GetAny(attribute))continue;
            const auto variable=attribute==IR::Attribute::ClipDistance?ctx.clip_distances:ctx.cull_distances;
            const auto mask=ctx.native_point_distance_component_masks[attribute==IR::Attribute::ClipDistance?0:1];
            const u32 components=mask?std::popcount(u32(mask)):8;
            const auto type=ctx.TypeArray(ctx.F32[1],ctx.ConstU32(components));
            const auto value=ctx.OpLoad(type,variable);
            if(!ctx.native_point_private_distances)outputs.push_back({type,variable,value});
            if(ctx.info.stores.GetAny(IR::Attribute::PointSize))for(u32 component=0;component<components;++component)
                distances.push_back({ctx.OpCompositeExtract(ctx.F32[1],value,component),false});
        }
        if(!ctx.info.stores.GetAny(IR::Attribute::PointSize)){
            // Hardware points already rasterize at unit size. Avoid spending
            // four GS outputs per point when the guest never exports a size.
            ctx.OpEmitVertex();
            for(const auto& output:outputs)ctx.OpStore(output.variable,output.value);
            ctx.OpStore(ctx.output_position,original_position);
            return;
        }
        EmitPointStrip(ctx,ctx.native_point_runtime,position,size,viewport,
            ctx.runtime_info.hw.gs.native_point_clip_depth,ctx.output_position,outputs,distances);
        // Guest code after this emission still sees the original output position.
        ctx.OpStore(ctx.output_position,original_position);
        return;
    }
    ctx.OpEmitVertex();
}

void EmitEmitPrimitive(EmitContext& ctx) {
    if(ctx.runtime_info.hw.gs.native_point_capture)return;
    ctx.OpEndPrimitive();
}

void EmitEmitVertex(EmitContext& ctx, const IR::Value& stream) {
    UNREACHABLE_MSG("Geometry streams");
}

void EmitEndPrimitive(EmitContext& ctx, const IR::Value& stream) {
    UNREACHABLE_MSG("Geometry streams");
}

void EmitDebugPrint(EmitContext& ctx, IR::Inst* inst, Id fmt, Id arg0, Id arg1, Id arg2, Id arg3) {
    IR::DebugPrintFlags flags = inst->Flags<IR::DebugPrintFlags>();
    std::array<Id, IR::DEBUGPRINT_NUM_FORMAT_ARGS> fmt_args = {arg0, arg1, arg2, arg3};
    auto fmt_args_span = std::span<Id>(fmt_args.begin(), fmt_args.begin() + flags.num_args);
    ctx.OpDebugPrintf(fmt, fmt_args_span);
}

Id EmitMemtime(EmitContext& ctx) {
    if (ctx.profile.supports_shader_subgroup_clock) {
        return ctx.OpReadClockKHR(ctx.U64, ctx.ConstU32(std::to_underlying(spv::Scope::Subgroup)));
    } else {
        if(ctx.profile.native_dma_buffers)
            throw std::runtime_error("Native S_MEMTIME requires a hardware shader clock implementation");
        return ctx.Constant(ctx.U64, 1U);
    }
}

} // namespace Shader::Backend::SPIRV
