// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "shader_recompiler/backend/spirv/spirv_emit_context.h"

namespace Shader::Backend::SPIRV {

// Khronos Data Format Specification sRGB EOTF. Alpha never uses this curve.
inline Id DecodeSrgb(EmitContext& ctx, Id value) {
    const Id low = ctx.OpFDiv(ctx.F32[1], value, ctx.ConstF32(12.92f));
    const Id high = ctx.OpPow(ctx.F32[1],
        ctx.OpFDiv(ctx.F32[1], ctx.OpFAdd(ctx.F32[1], value, ctx.ConstF32(.055f)),
                   ctx.ConstF32(1.055f)), ctx.ConstF32(2.4f));
    return ctx.OpSelect(ctx.F32[1],
        ctx.OpFOrdLessThanEqual(ctx.U1[1], value, ctx.ConstF32(.04045f)), low, high);
}

inline Id EncodeSrgb(EmitContext& ctx, Id value) {
    const Id finite = ctx.OpSelect(ctx.F32[1], ctx.OpIsNan(ctx.U1[1], value),
                                   ctx.f32_zero_value, value);
    const Id clamped = ctx.OpFClamp(ctx.F32[1], finite, ctx.f32_zero_value, ctx.ConstF32(1.f));
    const Id low = ctx.OpFMul(ctx.F32[1], clamped, ctx.ConstF32(12.92f));
    const Id high = ctx.OpFSub(ctx.F32[1], ctx.OpFMul(ctx.F32[1], ctx.ConstF32(1.055f),
        ctx.OpPow(ctx.F32[1], clamped, ctx.ConstF32(1.f / 2.4f))), ctx.ConstF32(.055f));
    return ctx.OpSelect(ctx.F32[1],
        ctx.OpFOrdLessThanEqual(ctx.U1[1], clamped, ctx.ConstF32(.0031308f)), low, high);
}

} // namespace Shader::Backend::SPIRV
