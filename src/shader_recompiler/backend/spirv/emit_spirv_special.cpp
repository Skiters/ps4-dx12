// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point_capture.h"
#include "shader_recompiler/ir/debug_print.h"
#include "shader_recompiler/ir/microinstruction.h"
#include <stdexcept>
#include <algorithm>
#include <bit>
#include <cmath>

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

namespace {
Id OrderedSnormDecode(EmitContext& ctx, Id word, u32 shift, u32 bits) {
    const Id code = ctx.OpBitFieldSExtract(ctx.U32[1], word, ctx.ConstU32(shift),
                                           ctx.ConstU32(bits));
    const Id value = ctx.OpFDiv(ctx.F32[1],
        ctx.OpConvertSToF(ctx.F32[1], ctx.OpBitcast(ctx.S32[1], code)),
        ctx.ConstF32(float((1u << (bits - 1)) - 1)));
    return ctx.OpFMax(ctx.F32[1], value, ctx.ConstF32(-1.f));
}

Id OrderedBlendFactor(EmitContext& ctx, AmdGpu::BlendControl::BlendFactor factor,
                      Id source, Id destination, Id constants, u32 component,
                      bool shader_alpha) {
    using F = AmdGpu::BlendControl::BlendFactor;
    const Id zero = ctx.f32_zero_value;
    const Id one = ctx.ConstF32(1.f);
    const auto channel = [&](Id vector, u32 index) {
        return ctx.OpCompositeExtract(ctx.F32[1], vector, index);
    };
    const Id src_alpha = shader_alpha ? channel(source, 3) : one;
    const Id dst_alpha = channel(destination, 3);
    const Id src_color = channel(source, component == 3 ? 3 : component);
    const Id dst_color = channel(destination, component == 3 ? 3 : component);
    const Id constant_color = channel(constants, component == 3 ? 3 : component);
    switch (factor) {
    case F::Zero: return zero;
    case F::One: return one;
    case F::SrcColor: return src_color;
    case F::OneMinusSrcColor: return ctx.OpFSub(ctx.F32[1], one, src_color);
    case F::SrcAlpha: return src_alpha;
    case F::OneMinusSrcAlpha: return ctx.OpFSub(ctx.F32[1], one, src_alpha);
    case F::DstAlpha: return dst_alpha;
    case F::OneMinusDstAlpha: return ctx.OpFSub(ctx.F32[1], one, dst_alpha);
    case F::DstColor: return dst_color;
    case F::OneMinusDstColor: return ctx.OpFSub(ctx.F32[1], one, dst_color);
    case F::SrcAlphaSaturate:
        return component == 3 ? one : ctx.OpFMin(ctx.F32[1], src_alpha,
            ctx.OpFSub(ctx.F32[1], one, dst_alpha));
    case F::ConstantColor: return constant_color;
    case F::OneMinusConstantColor: return ctx.OpFSub(ctx.F32[1], one, constant_color);
    case F::ConstantAlpha: return channel(constants, 3);
    case F::OneMinusConstantAlpha:
        return ctx.OpFSub(ctx.F32[1], one, channel(constants, 3));
    case F::Src1Color:
    case F::InvSrc1Color:
    case F::Src1Alpha:
    case F::InvSrc1Alpha:
        throw std::runtime_error("Ordered color reached unsupported dual-source blend factor");
    }
    throw std::runtime_error("Ordered color reached invalid blend factor");
}

Id OrderedBlendChannel(EmitContext& ctx, const HwFragmentRuntimeInfo::OrderedColorTarget& target,
                       Id source, Id destination, Id constants, u32 component) {
    const auto& blend = target.blend;
    const bool alpha = component == 3;
    const auto operation = alpha && blend.separate_alpha_blend ? blend.alpha_func : blend.color_func;
    const Id src = ctx.OpCompositeExtract(ctx.F32[1], source, component);
    const Id dst = ctx.OpCompositeExtract(ctx.F32[1], destination, component);
    if (operation == AmdGpu::BlendControl::BlendFunc::Min)
        return ctx.OpFMin(ctx.F32[1], src, dst);
    if (operation == AmdGpu::BlendControl::BlendFunc::Max)
        return ctx.OpFMax(ctx.F32[1], src, dst);
    const auto src_factor = alpha && blend.separate_alpha_blend ? blend.alpha_src_factor : blend.color_src_factor;
    const auto dst_factor = alpha && blend.separate_alpha_blend ? blend.alpha_dst_factor : blend.color_dst_factor;
    const bool shader_alpha = (target.shader_mask & AmdGpu::ColorBufferMask::ComponentA) != 0;
    const Id scaled_src = ctx.OpFMul(ctx.F32[1], src,
        OrderedBlendFactor(ctx, src_factor, source, destination, constants, component, shader_alpha));
    const Id scaled_dst = ctx.OpFMul(ctx.F32[1], dst,
        OrderedBlendFactor(ctx, dst_factor, source, destination, constants, component, shader_alpha));
    switch (operation) {
    case AmdGpu::BlendControl::BlendFunc::Add:
        return ctx.OpFAdd(ctx.F32[1], scaled_src, scaled_dst);
    case AmdGpu::BlendControl::BlendFunc::Subtract:
        return ctx.OpFSub(ctx.F32[1], scaled_src, scaled_dst);
    case AmdGpu::BlendControl::BlendFunc::ReverseSubtract:
        return ctx.OpFSub(ctx.F32[1], scaled_dst, scaled_src);
    default: break;
    }
    throw std::runtime_error("Ordered color reached invalid blend operation");
}

Id OrderedSnormCode(EmitContext& ctx, Id value, u32 bits) {
    const Id input = ctx.OpSelect(ctx.F32[1], ctx.OpIsNan(ctx.U1[1], value),
                                  ctx.ConstF32(0.f), value);
    const Id clamped = ctx.OpFClamp(ctx.F32[1], input, ctx.ConstF32(-1.f), ctx.ConstF32(1.f));
    const Id scaled = ctx.OpFMul(ctx.F32[1], clamped,
                                 ctx.ConstF32(float((1u << (bits - 1)) - 1)));
    const Id code = ctx.OpConvertFToS(ctx.S32[1], ctx.OpRoundEven(ctx.F32[1], scaled));
    return ctx.OpBitwiseAnd(ctx.U32[1], ctx.OpBitcast(ctx.U32[1], code),
                            ctx.ConstU32((1u << bits) - 1));
}

Id OrderedUnormCode(EmitContext& ctx, Id value, u32 bits) {
    const Id input = ctx.OpSelect(ctx.F32[1], ctx.OpIsNan(ctx.U1[1], value),
                                  ctx.ConstF32(0.f), value);
    const Id clamped = ctx.OpFClamp(ctx.F32[1], input, ctx.ConstF32(0.f), ctx.ConstF32(1.f));
    const Id scaled = ctx.OpFMul(ctx.F32[1], clamped, ctx.ConstF32(float((1u << bits) - 1)));
    return ctx.OpConvertFToU(ctx.U32[1], ctx.OpRoundEven(ctx.F32[1], scaled));
}

Id OrderedLogicWord(EmitContext& ctx, AmdGpu::ColorControl::LogicOp operation,
                    Id source, Id destination) {
    using L = AmdGpu::ColorControl::LogicOp;
    const auto invert = [&](Id value) { return ctx.OpNot(ctx.U32[1], value); };
    switch (operation) {
    case L::Clear: return ctx.u32_zero_value;
    case L::Nor: return invert(ctx.OpBitwiseOr(ctx.U32[1], source, destination));
    case L::AndInverted:
        return ctx.OpBitwiseAnd(ctx.U32[1], invert(source), destination);
    case L::CopyInverted: return invert(source);
    case L::AndReverse:
        return ctx.OpBitwiseAnd(ctx.U32[1], source, invert(destination));
    case L::Invert: return invert(destination);
    case L::BrushXor:
    case L::Xor: return ctx.OpBitwiseXor(ctx.U32[1], source, destination);
    case L::Nand: return invert(ctx.OpBitwiseAnd(ctx.U32[1], source, destination));
    case L::And: return ctx.OpBitwiseAnd(ctx.U32[1], source, destination);
    case L::Equiv: return invert(ctx.OpBitwiseXor(ctx.U32[1], source, destination));
    case L::Noop: return destination;
    case L::OrInverted:
        return ctx.OpBitwiseOr(ctx.U32[1], invert(source), destination);
    case L::Copy: return source;
    case L::OrReverse:
        return ctx.OpBitwiseOr(ctx.U32[1], source, invert(destination));
    case L::Or: return ctx.OpBitwiseOr(ctx.U32[1], source, destination);
    case L::Set: return ctx.ConstU32(~0u);
    }
    throw std::runtime_error("Ordered color reached invalid logic operation");
}

Id OrderedFloatCode(EmitContext& ctx, Id value,u32 mantissa,bool signed_result) {
    // Attachment conversion is RNE. Do not use the general PackHalf2x16
    // intrinsic here: its native lowering need not use attachment rounding.
    const auto u = [&](u32 bits) { return ctx.ConstU32(bits); };
    const u32 drop=23-mantissa,mask=(1u<<mantissa)-1,inf=31u<<mantissa;
    const Id raw = ctx.OpBitcast(ctx.U32[1], value);
    const Id magnitude = ctx.OpBitwiseAnd(ctx.U32[1], raw, u(0x7fffffffu));
    const Id sign = ctx.OpBitwiseAnd(ctx.U32[1],
        ctx.OpShiftRightLogical(ctx.U32[1], raw, u(16)), u(0x8000));
    const Id retained_lsb = ctx.OpBitwiseAnd(ctx.U32[1],
        ctx.OpShiftRightLogical(ctx.U32[1], magnitude, u(drop)), u(1));
    const Id rounded = ctx.OpIAdd(ctx.U32[1], magnitude,
        ctx.OpIAdd(ctx.U32[1], u((1u<<(drop-1))-1), retained_lsb));
    Id normal = ctx.OpISub(ctx.U32[1],
        ctx.OpShiftRightLogical(ctx.U32[1], rounded, u(drop)), u(112u<<mantissa));
    normal = ctx.OpSelect(ctx.U32[1],
        ctx.OpUGreaterThanEqual(ctx.U1[1], magnitude, u(0x47800000u-(1u<<(drop-1)))), u(inf), normal);
    const Id exponent = ctx.OpShiftRightLogical(ctx.U32[1], magnitude, u(23));
    // Every speculative shift is defined, including for zero/NaN inputs.
    const Id shift = ctx.OpUMax(ctx.U32[1], u(drop+1), ctx.OpUMin(ctx.U32[1], u(24),
        ctx.OpISub(ctx.U32[1], u(136-mantissa), exponent)));
    const Id significand = ctx.OpBitwiseOr(ctx.U32[1], u(0x800000),
        ctx.OpBitwiseAnd(ctx.U32[1], magnitude, u(0x7fffff)));
    const Id bias = ctx.OpISub(ctx.U32[1],
        ctx.OpShiftLeftLogical(ctx.U32[1], u(1), ctx.OpISub(ctx.U32[1], shift, u(1))), u(1));
    const Id sub_lsb = ctx.OpBitwiseAnd(ctx.U32[1],
        ctx.OpShiftRightLogical(ctx.U32[1], significand, shift), u(1));
    Id subnormal = ctx.OpShiftRightLogical(ctx.U32[1],
        ctx.OpIAdd(ctx.U32[1], significand, ctx.OpIAdd(ctx.U32[1], bias, sub_lsb)), shift);
    subnormal = ctx.OpSelect(ctx.U32[1],
        ctx.OpULessThan(ctx.U1[1], magnitude, u((112-mantissa)<<23)), u(0), subnormal);
    Id code = ctx.OpSelect(ctx.U32[1],
        ctx.OpUGreaterThanEqual(ctx.U1[1], magnitude, u(0x38800000)), normal, subnormal);
    if(!signed_result)code=ctx.OpSelect(ctx.U32[1],ctx.OpINotEqual(ctx.U1[1],
        ctx.OpBitwiseAnd(ctx.U32[1],raw,u(0x80000000)),u(0)),u(0),code);
    const Id nan = ctx.OpBitwiseOr(ctx.U32[1], u(inf|(1u<<(mantissa-1))),
        ctx.OpBitwiseAnd(ctx.U32[1], ctx.OpShiftRightLogical(ctx.U32[1], magnitude, u(drop)), u(mask)));
    code = ctx.OpSelect(ctx.U32[1],
        ctx.OpUGreaterThan(ctx.U1[1], magnitude, u(0x7f800000)), nan, code);
    return signed_result?ctx.OpBitwiseOr(ctx.U32[1], sign, code):code;
}
Id OrderedHalfCode(EmitContext& ctx,Id value){return OrderedFloatCode(ctx,value,10,true);}
Id OrderedUnsignedFloatDecode(EmitContext& ctx,Id code,u32 mantissa){
    const auto u=[&](u32 value){return ctx.ConstU32(value);};
    const Id fraction=ctx.OpBitwiseAnd(ctx.U32[1],code,u((1u<<mantissa)-1));
    const Id exponent=ctx.OpShiftRightLogical(ctx.U32[1],code,u(mantissa));
    Id bits=ctx.OpBitwiseOr(ctx.U32[1],
        ctx.OpShiftLeftLogical(ctx.U32[1],ctx.OpIAdd(ctx.U32[1],exponent,u(112)),u(23)),
        ctx.OpShiftLeftLogical(ctx.U32[1],fraction,u(23-mantissa)));
    bits=ctx.OpSelect(ctx.U32[1],ctx.OpIEqual(ctx.U1[1],exponent,u(31)),
        ctx.OpBitwiseOr(ctx.U32[1],u(0x7f800000),ctx.OpShiftLeftLogical(ctx.U32[1],fraction,u(23-mantissa))),bits);
    const Id sub=ctx.OpFMul(ctx.F32[1],ctx.OpConvertUToF(ctx.F32[1],fraction),ctx.ConstF32(std::ldexp(1.f,-14-int(mantissa))));
    return ctx.OpSelect(ctx.F32[1],ctx.OpIEqual(ctx.U1[1],exponent,u(0)),sub,ctx.OpBitcast(ctx.F32[1],bits));
}

template <typename Target>
void EmitOrderedColorSample(EmitContext& ctx, const Target& target, Id image_variable,
                            Id image_type, Id coordinates, Id constants, Id source) {
    const Id image = ctx.OpLoad(image_type, image_variable);
    const Id old_texel = ctx.OpImageRead(ctx.U32[4], image, coordinates);
    const Id old_word = ctx.OpCompositeExtract(ctx.U32[1], old_texel, 0);
    const bool logic = target.logic_op != AmdGpu::ColorControl::LogicOp::Copy;
    const bool rgba8 = target.data_format == AmdGpu::DataFormat::Format8_8_8_8;
    const bool unorm = target.number_format == AmdGpu::NumberFormat::Unorm;
    const bool scalar8=target.data_format==AmdGpu::DataFormat::Format8;
    const bool scalar16=target.data_format==AmdGpu::DataFormat::Format16;
    if(scalar8||scalar16){
        const u32 bits=scalar8?8:16;
        const bool floating=target.number_format==AmdGpu::NumberFormat::Float;
        const Id old_value=floating?ctx.OpCompositeExtract(ctx.F32[1],ctx.OpUnpackHalf2x16(ctx.F32[2],old_word),0):
            unorm?ctx.OpFDiv(ctx.F32[1],ctx.OpConvertUToF(ctx.F32[1],old_word),ctx.ConstF32(float((1u<<bits)-1))):
                OrderedSnormDecode(ctx,old_word,0,bits);
        const Id destination=ctx.OpCompositeConstruct(ctx.F32[4],old_value,ctx.f32_zero_value,
            ctx.f32_zero_value,ctx.ConstF32(1.f));
        Id result=old_word;
        if(target.write_mask&1u){
            const Id value=logic?ctx.OpCompositeExtract(ctx.F32[1],source,0):
                OrderedBlendChannel(ctx,target,source,destination,constants,0);
            result=floating?OrderedHalfCode(ctx,value):unorm?OrderedUnormCode(ctx,value,bits):OrderedSnormCode(ctx,value,bits);
            if(logic)result=OrderedLogicWord(ctx,target.logic_op,result,old_word);
        }
        result=ctx.OpBitwiseAnd(ctx.U32[1],result,ctx.ConstU32((1u<<bits)-1));
        ctx.OpImageWrite(image,coordinates,ctx.OpCompositeConstruct(ctx.U32[4],result,
            ctx.u32_zero_value,ctx.u32_zero_value,ctx.u32_zero_value));
        return;
    }
    if(target.data_format==AmdGpu::DataFormat::Format10_11_11&&target.number_format==AmdGpu::NumberFormat::Float){
        std::array<Id,4> channels{};
        for(u32 component=0;component<3;++component){
            const u32 bits=component==2?10:11,shift=component==2?22:component*11;
            const Id code=ctx.OpBitFieldUExtract(ctx.U32[1],old_word,ctx.ConstU32(shift),ctx.ConstU32(bits));
            channels[component]=OrderedUnsignedFloatDecode(ctx,code,bits-5);
        }
        channels[3]=ctx.ConstF32(1.f);const Id destination=ctx.OpCompositeConstruct(ctx.F32[4],channels);
        Id result=old_word;
        for(u32 component=0;component<3;++component)if(target.write_mask&(1u<<component)){
            const u32 bits=component==2?10:11,shift=component==2?22:component*11;
            const Id value=OrderedBlendChannel(ctx,target,source,destination,constants,component);
            result=ctx.OpBitFieldInsert(ctx.U32[1],result,OrderedFloatCode(ctx,value,bits-5,false),
                ctx.ConstU32(shift),ctx.ConstU32(bits));
        }
        ctx.OpImageWrite(image,coordinates,ctx.OpCompositeConstruct(ctx.U32[4],result,
            ctx.u32_zero_value,ctx.u32_zero_value,ctx.u32_zero_value));
        return;
    }
    const u32 float_components=target.data_format==AmdGpu::DataFormat::Format32?1:
        target.data_format==AmdGpu::DataFormat::Format32_32?2:
        target.data_format==AmdGpu::DataFormat::Format32_32_32_32?4:0;
    const bool r32_float = float_components&&target.number_format == AmdGpu::NumberFormat::Float;
    if (r32_float) {
        // The ordinary R32 typeless family exposes the same bits through a
        // rasterizer-ordered UINT view. Blend in floating point, without the
        // normalized encoders used by the packed targets below. Missing RGB/A
        // channels retain the format-defined (0,0,1) destination values.
        std::array<Id,4> before{},channels{},after{};
        for(u32 component=0;component<4;++component){
            before[component]=ctx.OpCompositeExtract(ctx.U32[1],old_texel,component);
            channels[component]=component<float_components?ctx.OpBitcast(ctx.F32[1],before[component]):
                component==3?ctx.ConstF32(1.f):ctx.f32_zero_value;
        }
        const Id destination=ctx.OpCompositeConstruct(ctx.F32[4],channels);
        for(u32 component=0;component<4;++component)
            after[component]=component>=float_components?ctx.u32_zero_value:
                (target.write_mask&(1u<<component))?ctx.OpBitcast(ctx.U32[1],
                    OrderedBlendChannel(ctx,target,source,destination,constants,component)):before[component];
        ctx.OpImageWrite(image,coordinates,ctx.OpCompositeConstruct(ctx.U32[4],after));
        return;
    }
    const bool half_word = target.data_format == AmdGpu::DataFormat::Format16_16 ||
                           target.data_format == AmdGpu::DataFormat::Format16_16_16_16;
    const u32 half_components = target.data_format == AmdGpu::DataFormat::Format16_16_16_16 ? 4 : 2;
    const bool rg16_float = half_word &&
                            target.number_format == AmdGpu::NumberFormat::Float;
    const bool rg16_unorm = half_word &&
                            target.number_format == AmdGpu::NumberFormat::Unorm;
    const bool rg16_snorm = half_word &&
                            target.number_format == AmdGpu::NumberFormat::Snorm;
    if (rg16_float || rg16_unorm || rg16_snorm) {
        std::array<Id, 2> words{old_word,ctx.OpCompositeExtract(ctx.U32[1],old_texel,1)};
        std::array<Id, 4> old_channels{ctx.f32_zero_value,ctx.f32_zero_value,ctx.f32_zero_value,ctx.ConstF32(1.f)};
        for (u32 component = 0; component < half_components; ++component) {
            const auto word=words[component/2];const auto shift=(component%2)*16;
            if(rg16_float){
                const Id unpacked=ctx.OpUnpackHalf2x16(ctx.F32[2],word);
                old_channels[component]=ctx.OpCompositeExtract(ctx.F32[1],unpacked,component%2);
                continue;
            }
            old_channels[component] = rg16_snorm ? OrderedSnormDecode(ctx, word, shift, 16) :
                ctx.OpFDiv(ctx.F32[1], ctx.OpConvertUToF(ctx.F32[1],
                    ctx.OpBitFieldUExtract(ctx.U32[1], word, ctx.ConstU32(shift), ctx.ConstU32(16u))),
                    ctx.ConstF32(65535.f));
        }
        const Id destination = ctx.OpCompositeConstruct(ctx.F32[4],old_channels);
        std::array<Id, 4> channels{};
        std::array<u32,2> raw_masks{};
        for (u32 component = 0; component < half_components; ++component) {
            const bool written = (target.write_mask & (1u << component)) != 0;
            channels[component] = !written ? ctx.f32_zero_value : logic ?
                ctx.OpCompositeExtract(ctx.F32[1], source, component) :
                OrderedBlendChannel(ctx, target, source, destination, constants, component);
            if (written) raw_masks[component/2] |= 0xffffu << ((component%2) * 16);
        }
        // Quantize after each ordered fragment, just as the attachment store
        // does. Merge untouched raw halves rather than round-tripping NaNs or
        // signed zero through float arithmetic and half conversion.
        const auto encode = [&](Id channel) {
            return rg16_float ? OrderedHalfCode(ctx, channel) : rg16_unorm ?
                OrderedUnormCode(ctx, channel, 16) : OrderedSnormCode(ctx, channel, 16);
        };
        std::array<Id,4> result{ctx.u32_zero_value,ctx.u32_zero_value,ctx.u32_zero_value,ctx.u32_zero_value};
        for(u32 word=0;word<half_components/2;++word){
            Id packed=ctx.OpBitwiseOr(ctx.U32[1],encode(channels[word*2]),
                ctx.OpShiftLeftLogical(ctx.U32[1],encode(channels[word*2+1]),ctx.ConstU32(16u)));
            if(logic)packed=OrderedLogicWord(ctx,target.logic_op,packed,words[word]);
            result[word]=ctx.OpBitwiseOr(ctx.U32[1],
                ctx.OpBitwiseAnd(ctx.U32[1],words[word],ctx.ConstU32(~raw_masks[word])),
                ctx.OpBitwiseAnd(ctx.U32[1],packed,ctx.ConstU32(raw_masks[word])));
        }
        ctx.OpImageWrite(image,coordinates,ctx.OpCompositeConstruct(ctx.U32[4],result));
        return;
    }
    Id destination{};
    if (!logic) {
        std::array<Id, 4> destination_channels{};
        for (u32 component = 0; component < 4; ++component) {
            if (unorm) {
                const u32 bits = rgba8 ? 8 : component == 3 ? 2 : 10;
                const Id code = ctx.OpBitFieldUExtract(ctx.U32[1], old_word,
                    ctx.ConstU32(rgba8 ? component * 8 : component * 10), ctx.ConstU32(bits));
                destination_channels[component] = ctx.OpFDiv(ctx.F32[1],
                    ctx.OpConvertUToF(ctx.F32[1], code), ctx.ConstF32(float((1u << bits) - 1)));
            } else {
                destination_channels[component] = OrderedSnormDecode(ctx, old_word,
                    rgba8 ? component * 8 : component * 10, rgba8 ? 8 : component == 3 ? 2 : 10);
            }
        }
        destination = ctx.OpCompositeConstruct(ctx.F32[4], destination_channels);
    }
    Id packed = ctx.u32_zero_value;
    u32 raw_mask{};
    for (u32 component = 0; component < 4; ++component) {
        const u32 bits = rgba8 ? 8 : component == 3 ? 2 : 10;
        const u32 shift = rgba8 ? component * 8 : component * 10;
        const Id value = logic ? ctx.OpCompositeExtract(ctx.F32[1], source, component) :
                                 OrderedBlendChannel(ctx, target, source, destination, constants, component);
        const Id code = unorm ? OrderedUnormCode(ctx, value, bits) : OrderedSnormCode(ctx, value, bits);
        packed = ctx.OpBitwiseOr(ctx.U32[1], packed,
            shift ? ctx.OpShiftLeftLogical(ctx.U32[1], code, ctx.ConstU32(shift)) : code);
        if (target.write_mask & (1u << component)) raw_mask |= ((1u << bits) - 1) << shift;
    }
    if (logic) packed = OrderedLogicWord(ctx, target.logic_op, packed, old_word);
    const Id mask = ctx.ConstU32(raw_mask);
    const Id result = ctx.OpBitwiseOr(ctx.U32[1],
        ctx.OpBitwiseAnd(ctx.U32[1], old_word, ctx.ConstU32(~raw_mask)),
        ctx.OpBitwiseAnd(ctx.U32[1], packed, mask));
    const Id texel = ctx.OpCompositeConstruct(ctx.U32[4], result, ctx.u32_zero_value,
                                               ctx.u32_zero_value, ctx.u32_zero_value);
    ctx.OpImageWrite(image, coordinates, texel);
}

void EmitOrderedColor(EmitContext& ctx) {
    const Id constants_ptr = ctx.OpAccessChain(
        ctx.TypePointer(spv::StorageClass::PushConstant, ctx.F32[4]), ctx.push_data_block,
        ctx.ConstU32(PushData::BlendConstantsIndex));
    const Id constants = ctx.OpLoad(ctx.F32[4], constants_ptr);
    const Id position = ctx.OpLoad(ctx.F32[4], ctx.frag_coord);
    const Id coordinates = ctx.OpCompositeConstruct(ctx.S32[3],
        ctx.OpConvertFToS(ctx.S32[1], ctx.OpCompositeExtract(ctx.F32[1], position, 0)),
        ctx.OpConvertFToS(ctx.S32[1], ctx.OpCompositeExtract(ctx.F32[1], position, 1)),
        ctx.OpBitcast(ctx.S32[1], ctx.OpLoad(ctx.U32[1], ctx.output_layer)));
    Id invocation_sample{};
    for (u32 index = 0; index < ctx.ordered_color_images.size(); ++index) {
        const auto& target = ctx.runtime_info.hw.fs.ordered_color_targets[index];
        if (!target.enabled) continue;
        const Id source = ctx.OpLoad(ctx.F32[4], ctx.frag_outputs[index].id);
        if (target.num_samples == 1) {
            EmitOrderedColorSample(ctx, target, ctx.ordered_color_images[index][0],
                                   ctx.ordered_color_image_types[index], coordinates, constants, source);
            continue;
        }
        if (!Sirit::ValidId(invocation_sample))
            invocation_sample = ctx.OpLoad(ctx.U32[1], ctx.sample_index);
        for (u32 sample = 0; sample < target.num_samples; ++sample) {
            const Id active = ctx.OpLabel();
            const Id merge = ctx.OpLabel();
            const Id selected = ctx.OpIEqual(ctx.U1[1], invocation_sample, ctx.ConstU32(sample));
            ctx.OpSelectionMerge(merge, spv::SelectionControlMask::MaskNone);
            ctx.OpBranchConditional(selected, active, merge);
            ctx.AddLabel(active);
            EmitOrderedColorSample(ctx, target, ctx.ordered_color_images[index][sample],
                                   ctx.ordered_color_image_types[index], coordinates, constants, source);
            ctx.OpBranch(merge);
            ctx.AddLabel(merge);
        }
    }
}
} // namespace

void EmitEpilogue(EmitContext& ctx) {
    if (ctx.hw_stage == HwStage::Fragment &&
        std::ranges::any_of(ctx.runtime_info.hw.fs.ordered_color_targets,
            [](const auto& target) { return target.enabled; })) {
        EmitOrderedColor(ctx);
    }
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
