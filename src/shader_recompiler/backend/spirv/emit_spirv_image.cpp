// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <boost/container/static_vector.hpp>
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/ir/microinstruction.h"

namespace Shader::Backend::SPIRV {

template <bool is_float>
static Id FixGuestImageCoords(EmitContext& ctx, Id coords, AmdGpu::ImageType image_type) {
    const auto coord_type = is_float ? ctx.F32 : ctx.U32;
    const auto zero = is_float ? ctx.f32_zero_value : ctx.u32_zero_value;

    switch (image_type) {
    case AmdGpu::ImageType::Color1D: {
        // Lowered to 2D with height 1
        const auto x = coords;
        return ctx.OpCompositeConstruct(coord_type[2], x, zero);
    }
    case AmdGpu::ImageType::Color1DArray: {
        // Lowered to 2D array with height 1
        const auto x = ctx.OpCompositeExtract(coord_type[1], coords, 0U);
        const auto slice = ctx.OpCompositeExtract(coord_type[1], coords, 1U);
        return ctx.OpCompositeConstruct(coord_type[3], x, zero, slice);
    }
    case AmdGpu::ImageType::Cube: {
        // Lowered to 2D array
        if (is_float) {
            const auto x = ctx.OpCompositeExtract(coord_type[1], coords, 0U);
            const auto y = ctx.OpCompositeExtract(coord_type[1], coords, 1U);
            const auto face = ctx.OpCompositeExtract(coord_type[1], coords, 2U);

            // AMD cube math results in coordinates in the range [1.0, 2.0]. We need
            // to convert this to the range [0.0, 1.0] to get correct results.
            const auto one = ctx.ConstF32(1.f);
            const auto fixed_x = ctx.OpFSub(coord_type[1], x, one);
            const auto fixed_y = ctx.OpFSub(coord_type[1], y, one);
            const auto fixed_face = ctx.OpFma(
                coord_type[1],
                ctx.OpFloor(coord_type[1], ctx.OpFDiv(coord_type[1], face, ctx.ConstF32(8.f))),
                ctx.ConstF32(-2.f), face);
            return ctx.OpCompositeConstruct(coord_type[3], fixed_x, fixed_y, fixed_face);
        }
        return coords;
    }
    default:
        return coords;
    }
}

template <bool is_float>
static Id FixImageCoords(EmitContext& ctx, Id coords, AmdGpu::ImageType image_type,
                         bool native_slice_view = false) {
    const Id fixed = FixGuestImageCoords<is_float>(ctx, coords, image_type);
    if (!native_slice_view) return fixed;
    const auto type = is_float ? ctx.F32 : ctx.U32;
    const Id zero = is_float ? ctx.f32_zero_value : ctx.u32_zero_value;
    return ctx.OpCompositeConstruct(type[3], ctx.OpCompositeExtract(type[1], fixed, 0U),
        ctx.OpCompositeExtract(type[1], fixed, 1U), zero);
}
template <bool is_float>
static Id PadImageOperand(EmitContext& ctx, Id operand, AmdGpu::ImageType image_type) {
    if (image_type == AmdGpu::ImageType::Color1D || image_type == AmdGpu::ImageType::Color1DArray) {
        const auto coord_type = is_float ? ctx.F32 : ctx.U32;
        const auto zero = is_float ? ctx.f32_zero_value : ctx.u32_zero_value;
        return ctx.OpCompositeConstruct(coord_type[2], operand, zero);
    }
    return operand;
}

struct ImageOperands {
    void Add(spv::ImageOperandsMask new_mask, Id value) {
        if (!Sirit::ValidId(value)) {
            return;
        }
        mask = static_cast<spv::ImageOperandsMask>(static_cast<u32>(mask) |
                                                   static_cast<u32>(new_mask));
        operands.push_back(value);
    }
    void Add(spv::ImageOperandsMask new_mask, Id value1, Id value2) {
        mask = static_cast<spv::ImageOperandsMask>(static_cast<u32>(mask) |
                                                   static_cast<u32>(new_mask));
        operands.push_back(value1);
        operands.push_back(value2);
    }

    void AddOffset(EmitContext& ctx, AmdGpu::ImageType image_type, const IR::Value& offset,
                   bool can_use_runtime_offsets = false) {
        if (offset.IsEmpty()) {
            return;
        }
        spv::ImageOperandsMask op_mask{};
        Id value{};
        if (offset.IsImmediate()) {
            op_mask = spv::ImageOperandsMask::ConstOffset;
            value = ctx.ConstS32(static_cast<s32>(offset.U32()));
        } else {
            IR::Inst* const inst{offset.Inst()};
            if (inst->AreAllArgsImmediates()) {
                switch (inst->GetOpcode()) {
                case IR::Opcode::CompositeConstructU32x2:
                    op_mask = spv::ImageOperandsMask::ConstOffset;
                    value = ctx.ConstS32(static_cast<s32>(inst->Arg(0).U32()),
                                         static_cast<s32>(inst->Arg(1).U32()));
                    break;
                case IR::Opcode::CompositeConstructU32x3:
                    op_mask = spv::ImageOperandsMask::ConstOffset;
                    value = ctx.ConstS32(static_cast<s32>(inst->Arg(0).U32()),
                                         static_cast<s32>(inst->Arg(1).U32()),
                                         static_cast<s32>(inst->Arg(2).U32()));
                    break;
                default:
                    break;
                }
            }
        }

        if (op_mask != spv::ImageOperandsMask::ConstOffset) {
            if (!can_use_runtime_offsets) {
                LOG_WARNING(Render_DX12,
                            "Runtime offset provided to unsupported image sample instruction");
                return;
            }
            op_mask = spv::ImageOperandsMask::Offset;
            value = ctx.Def(offset);
        }

        value = PadImageOperand<false>(ctx, value, image_type);
        Add(op_mask, value);
    }

    void AddDerivatives(EmitContext& ctx, AmdGpu::ImageType image_type, Id derivatives_dx,
                        Id derivatives_dy) {
        if (!Sirit::ValidId(derivatives_dx) || !Sirit::ValidId(derivatives_dy)) {
            return;
        }
        derivatives_dx = PadImageOperand<true>(ctx, derivatives_dx, image_type);
        derivatives_dy = PadImageOperand<true>(ctx, derivatives_dy, image_type);
        Add(spv::ImageOperandsMask::Grad, derivatives_dx, derivatives_dy);
    }

    spv::ImageOperandsMask mask{};
    boost::container::static_vector<Id, 4> operands;
};

Id EmitImageHandle(EmitContext& ctx, Id, Id) {
    UNREACHABLE_MSG("Unreachable instruction");
}

Id EmitImageSampleRaw(EmitContext& ctx, IR::Inst* inst, u32 handle, Id address1, Id address2,
                      Id address3, Id address4) {
    UNREACHABLE_MSG("Unreachable instruction");
}

Id EmitImageSampleImplicitLod(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id bias,
                              const IR::Value& offset, const IR::Value& lod_clamp) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.Add(spv::ImageOperandsMask::Bias, bias);
    operands.AddOffset(ctx, texture.view_type, offset);
    if (!lod_clamp.IsEmpty()) {
        ctx.AddCapability(spv::Capability::MinLod);
        operands.Add(spv::ImageOperandsMask::MinLod, ctx.Def(lod_clamp));
    }
    const Id sample = ctx.OpImageSampleImplicitLod(result_type, sampled_image, fixed_coords,
                                                   operands.mask, operands.operands);
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], sample) : sample;
}

Id EmitImageSampleExplicitLod(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id lod,
                              const IR::Value& offset) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.Add(spv::ImageOperandsMask::Lod, lod);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = ctx.OpImageSampleExplicitLod(result_type, sampled_image, fixed_coords,
                                                   operands.mask, operands.operands);
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], sample) : sample;
}

Id EmitImageSampleDrefImplicitLod(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id dref,
                                  Id bias, const IR::Value& offset, const IR::Value& lod_clamp) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(1);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.Add(spv::ImageOperandsMask::Bias, bias);
    operands.AddOffset(ctx, texture.view_type, offset);
    if (!lod_clamp.IsEmpty()) {
        ctx.AddCapability(spv::Capability::MinLod);
        operands.Add(spv::ImageOperandsMask::MinLod, ctx.Def(lod_clamp));
    }
    const Id sample = ctx.OpImageSampleDrefImplicitLod(result_type, sampled_image, fixed_coords,
                                                       dref, operands.mask, operands.operands);
    const Id sample_typed = texture.is_integer ? ctx.OpBitcast(ctx.F32[1], sample) : sample;
    return ctx.OpCompositeConstruct(ctx.F32[4], sample_typed, ctx.f32_zero_value,
                                    ctx.f32_zero_value, ctx.f32_zero_value);
}

Id EmitImageSampleDrefExplicitLod(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id dref,
                                  Id lod, const IR::Value& offset) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(1);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.Add(spv::ImageOperandsMask::Lod, lod);
    operands.AddOffset(ctx, texture.view_type, offset);
    const Id sample = ctx.OpImageSampleDrefExplicitLod(result_type, sampled_image, fixed_coords,
                                                       dref, operands.mask, operands.operands);
    const Id sample_typed = texture.is_integer ? ctx.OpBitcast(ctx.F32[1], sample) : sample;
    return ctx.OpCompositeConstruct(ctx.F32[4], sample_typed, ctx.f32_zero_value,
                                    ctx.f32_zero_value, ctx.f32_zero_value);
}

Id EmitImageGather(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords,
                   const IR::Value& offset) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    const u32 comp = inst->Flags<IR::TextureInstInfo>().gather_comp.Value();
    ImageOperands operands;
    operands.AddOffset(ctx, texture.view_type, offset, true);
    const Id texels = ctx.OpImageGather(result_type, sampled_image, fixed_coords,
                                        ctx.ConstU32(comp), operands.mask, operands.operands);
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], texels) : texels;
}

Id EmitImageGatherDref(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords,
                       const IR::Value& offset, Id dref) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.AddOffset(ctx, texture.view_type, offset, true);
    const Id texels = ctx.OpImageDrefGather(result_type, sampled_image, fixed_coords, dref,
                                            operands.mask, operands.operands);
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], texels) : texels;
}

Id EmitImageQueryDimensions(EmitContext& ctx, IR::Inst* inst, u32 handle, Id lod, bool has_mips) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    Id image_ptr = texture.id;
    Id array_lod_valid{};
    if (texture.native_msaa_storage) {
        const Id pointer_type = ctx.TypePointer(spv::StorageClass::UniformConstant, texture.image_type);
        image_ptr = ctx.OpAccessChain(pointer_type, image_ptr, std::array{ctx.u32_zero_value});
    } else if (texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex) {
        const Id pointer_type = ctx.TypePointer(spv::StorageClass::UniformConstant, texture.image_type);
        Id index = Sirit::ValidId(lod) ? lod : ctx.u32_zero_value;
        if (ctx.profile.native_coherent_images) {
            array_lod_valid = ctx.OpULessThan(ctx.U1[1], index,
                ctx.ConstU32(ctx.info.images[handle & 0xFFFF].NumBindings(ctx.info)));
            // Query a valid descriptor, then mask sizes for an invalid guest
            // mip. Masking only the result would still access another binding.
            index = ctx.OpSelect(ctx.U32[1], array_lod_valid, index, ctx.u32_zero_value);
        }
        image_ptr = ctx.OpAccessChain(pointer_type, image_ptr, std::array{index});
        if (ctx.profile.native_coherent_images) {
            ctx.Decorate(image_ptr, spv::Decoration::NonUniform);
        }
    }
    const Id image = ctx.OpLoad(texture.image_type, image_ptr);
    const Id zero = ctx.u32_zero_value;
    const auto mips{[&] {
        if (!has_mips) return zero;
        // A D3D12 UAV describes one mip and cannot report the resource's mip
        // count. Preserve the guest view's count, included in specialization.
        if (texture.is_storage && ctx.profile.native_coherent_images) {
            const auto sharp = ctx.info.images[handle & 0xFFFF].GetSharp(ctx.info);
            if (texture.native_msaa_storage) return ctx.ConstU32(1U);
            return ctx.ConstU32(u32(sharp.last_level - sharp.base_level + 1));
        }
        return ctx.OpImageQueryLevels(ctx.U32[1], image);
    }};
    const bool uses_lod{texture.view_type != AmdGpu::ImageType::Color2DMsaa &&
        texture.view_type != AmdGpu::ImageType::Color2DMsaaArray && !texture.is_storage};
    const auto query{[&](Id type) {
        const Id physical_type = texture.native_slice_view ? ctx.U32[3] : type;
        Id base = uses_lod ? ctx.OpImageQuerySizeLod(physical_type, image, lod)
                          : ctx.OpImageQuerySize(physical_type, image);
        if (texture.native_slice_view)
            base = ctx.OpCompositeConstruct(type, ctx.OpCompositeExtract(ctx.U32[1], base, 0U),
                ctx.OpCompositeExtract(ctx.U32[1], base, 1U));
        if (uses_lod) return base;
        if (Sirit::ValidId(array_lod_valid)) {
            const u32 count = type.value == ctx.U32[2].value ? 2 : 3;
            std::array<Id, 3> components{};
            for (u32 i = 0; i < count; ++i)
                components[i] = ctx.OpSelect(ctx.U32[1], array_lod_valid,
                    ctx.OpCompositeExtract(ctx.U32[1], base, i), zero);
            return count == 2 ? ctx.OpCompositeConstruct(type, components[0], components[1])
                              : ctx.OpCompositeConstruct(type, components[0], components[1], components[2]);
        }
        if (!ctx.profile.native_coherent_images || !texture.is_storage || !Sirit::ValidId(lod) ||
            texture.mip_fallback_mode != MipStorageFallbackMode::None ||
            texture.view_type == AmdGpu::ImageType::Color2DMsaa) return base;
        // The UAV is bound at the guest base mip. Reconstruct a queried mip's
        // dimensions, preserving array layers while reducing 3D depth.
        const auto sharp = ctx.info.images[handle & 0xFFFF].GetSharp(ctx.info);
        const auto valid = ctx.OpULessThan(ctx.U1[1], lod,
            ctx.ConstU32(u32(sharp.last_level - sharp.base_level + 1)));
        const auto shift = ctx.OpUMin(ctx.U32[1], lod, ctx.ConstU32(31U));
        const bool volume = texture.view_type == AmdGpu::ImageType::Color3D;
        const u32 count = type.value == ctx.U32[2].value ? 2 : 3;
        std::array<Id, 3> components{};
        for (u32 i = 0; i < count; ++i) {
            auto value = ctx.OpCompositeExtract(ctx.U32[1], base, i);
            if (i < 2 || volume)
                value = ctx.OpUMax(ctx.U32[1], ctx.OpShiftRightLogical(ctx.U32[1], value, shift),
                                   ctx.ConstU32(1U));
            components[i] = ctx.OpSelect(ctx.U32[1], valid, value, zero);
        }
        return count == 2 ? ctx.OpCompositeConstruct(type, components[0], components[1])
                          : ctx.OpCompositeConstruct(type, components[0], components[1], components[2]);
    }};
    switch (texture.view_type) {
    case AmdGpu::ImageType::Color1D: {
        const auto width = ctx.OpCompositeExtract(ctx.U32[1], query(ctx.U32[2]), 0U);
        return ctx.OpCompositeConstruct(ctx.U32[4], width, zero, zero, mips());
    }
    case AmdGpu::ImageType::Color1DArray: {
        const auto base = query(ctx.U32[3]);
        const auto width = ctx.OpCompositeExtract(ctx.U32[1], base, 0U);
        const auto slices = ctx.OpCompositeExtract(ctx.U32[1], base, 2U);
        return ctx.OpCompositeConstruct(ctx.U32[4], width, slices, zero, mips());
    }
    case AmdGpu::ImageType::Color2D:
    case AmdGpu::ImageType::Color2DMsaa:
        return ctx.OpCompositeConstruct(ctx.U32[4], query(ctx.U32[2]), zero, mips());
    case AmdGpu::ImageType::Color2DArray:
    case AmdGpu::ImageType::Color2DMsaaArray:
    case AmdGpu::ImageType::Cube:
    case AmdGpu::ImageType::Color3D:
        return ctx.OpCompositeConstruct(ctx.U32[4], query(ctx.U32[3]), mips());
    default:
        UNREACHABLE_MSG("SPIR-V Instruction");
    }
}

Id EmitImageQueryLod(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    // LOD queries use only spatial coordinates, even for an array view.
    return ctx.OpImageQueryLod(ctx.F32[2], sampled_image,
        FixImageCoords<true>(ctx, coords, texture.view_type));
}

Id EmitImageGradient(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id derivatives_dx,
                     Id derivatives_dy, const IR::Value& offset, const IR::Value& lod_clamp) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id image = ctx.OpLoad(texture.image_type, texture.id);
    const Id result_type = texture.data_types->Get(4);
    const Id sampler = ctx.OpLoad(ctx.sampler_type, ctx.samplers[handle >> 16]);
    const Id sampled_image = ctx.OpSampledImage(texture.sampled_type, image, sampler);
    const Id fixed_coords = FixImageCoords<true>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    operands.AddDerivatives(ctx, texture.view_type, derivatives_dx, derivatives_dy);
    operands.AddOffset(ctx, texture.view_type, offset);
    if (!lod_clamp.IsEmpty()) {
        ctx.AddCapability(spv::Capability::MinLod);
        operands.Add(spv::ImageOperandsMask::MinLod, ctx.Def(lod_clamp));
    }
    const Id sample = ctx.OpImageSampleExplicitLod(result_type, sampled_image, fixed_coords,
                                                   operands.mask, operands.operands);
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], sample) : sample;
}

// Keep descriptor addressing in range. Invalid guest mips use an out-of-bounds
// texel on a valid UAV: typed loads return zero and typed stores have no effect.
static Id BoundNativeStorageMip(EmitContext& ctx, u32 handle, Id lod, Id& coords) {
    Id index = Sirit::ValidId(lod) ? lod : ctx.u32_zero_value;
    if (!ctx.profile.native_coherent_images) return index;
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id valid = ctx.OpULessThan(ctx.U1[1], index,
        ctx.ConstU32(ctx.info.images[handle & 0xFFFF].NumBindings(ctx.info)));
    const auto type = texture.view_type;
    const u32 components = texture.native_slice_view ? 3 : type == AmdGpu::ImageType::Color1D ||
                           type == AmdGpu::ImageType::Color2D ||
                           type == AmdGpu::ImageType::Color2DMsaa ? 2 : 3;
    const Id x = ctx.OpCompositeExtract(ctx.U32[1], coords, 0U);
    const Id bounded_x = ctx.OpSelect(ctx.U32[1], valid, x, ctx.ConstU32(0xffffffffU));
    coords = ctx.OpCompositeInsert(ctx.U32[components], bounded_x, coords, 0U);
    return ctx.OpSelect(ctx.U32[1], valid, index, ctx.u32_zero_value);
}
static Id NativeStorageSample(EmitContext& ctx, u32 handle, Id ms) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const auto samples = ctx.info.images[handle & 0xFFFF].GetSharp(ctx.info).NumSamples();
    const Id index = Sirit::ValidId(ms) ? ctx.OpUMod(ctx.U32[1], ms, ctx.ConstU32(samples)) : ctx.u32_zero_value;
    const Id pointer_type = ctx.TypePointer(spv::StorageClass::UniformConstant, texture.image_type);
    const Id pointer = ctx.OpAccessChain(pointer_type, texture.id, std::array{index});
    ctx.Decorate(pointer, spv::Decoration::NonUniform);
    return pointer;
}

static std::array<u32, 8> Packed16Fields(u32 format) {
    switch (format) {
    case 16: return {11, 5, 5, 6, 0, 5, 0, 0};
    case 17: return {10, 5, 5, 5, 0, 5, 15, 1};
    case 18: return {11, 5, 6, 5, 1, 5, 0, 1};
    case 19: return {4, 4, 8, 4, 12, 4, 0, 4};
    default: throw std::invalid_argument("Invalid native packed image format");
    }
}

static Id UnpackNativePacked16(EmitContext& ctx, Id texel, u32 format) {
    const auto fields = Packed16Fields(format);
    const auto word = ctx.OpCompositeExtract(ctx.U32[1], texel, 0U);
    std::array<Id, 4> channels{};
    for (u32 i = 0; i < 4; ++i) {
        const u32 bits = fields[i * 2 + 1];
        if (!bits) { channels[i] = ctx.ConstF32(1.0f); continue; }
        const auto component = ctx.OpBitFieldUExtract(ctx.U32[1], word,
            ctx.ConstU32(fields[i * 2]), ctx.ConstU32(bits));
        channels[i] = ctx.OpFDiv(ctx.F32[1], ctx.OpConvertUToF(ctx.F32[1], component),
            ctx.ConstF32(float((1U << bits) - 1U)));
    }
    return ctx.OpCompositeConstruct(ctx.F32[4], channels[0], channels[1], channels[2], channels[3]);
}

static Id PackNativePacked16(EmitContext& ctx, Id color, u32 format) {
    const auto fields = Packed16Fields(format);
    Id word = ctx.ConstU32(0U);
    for (u32 i = 0; i < 4; ++i) {
        const u32 bits = fields[i * 2 + 1];
        if (!bits) continue;
        const auto value = ctx.OpCompositeExtract(ctx.F32[1], color, i);
        const auto clamped = ctx.OpFClamp(ctx.F32[1], value, ctx.ConstF32(0.0f), ctx.ConstF32(1.0f));
        const auto scaled = ctx.OpFMul(ctx.F32[1], clamped, ctx.ConstF32(float((1U << bits) - 1U)));
        const auto component = ctx.OpConvertFToU(ctx.U32[1], ctx.OpRoundEven(ctx.F32[1], scaled));
        word = ctx.OpBitwiseOr(ctx.U32[1], word,
            ctx.OpShiftLeftLogical(ctx.U32[1], component, ctx.ConstU32(fields[i * 2])));
    }
    const auto zero = ctx.ConstU32(0U);
    return ctx.OpCompositeConstruct(ctx.U32[4], word, zero, zero, zero);
}
// Shared exponent is per texel. Decode before filtering, not after averaging
// packed mantissas and exponent bits from unrelated texels.
static Id UnpackNativeSharedExponent(EmitContext& ctx, Id texel) {
    const Id word = ctx.OpCompositeExtract(ctx.U32[1], texel, 0U);
    const Id exponent = ctx.OpBitFieldUExtract(ctx.U32[1], word, ctx.ConstU32(27U), ctx.ConstU32(5U));
    // m * 2^(e-24): e+103 is the normal IEEE exponent for the exact scale.
    const Id scale = ctx.OpBitcast(ctx.F32[1], ctx.OpShiftLeftLogical(ctx.U32[1],
        ctx.OpIAdd(ctx.U32[1], exponent, ctx.ConstU32(103U)), ctx.ConstU32(23U)));
    std::array<Id, 3> channels;
    for (u32 i = 0; i < 3; ++i) {
        const Id mantissa = ctx.OpBitFieldUExtract(ctx.U32[1], word, ctx.ConstU32(i * 9), ctx.ConstU32(9U));
        channels[i] = ctx.OpFMul(ctx.F32[1], ctx.OpConvertUToF(ctx.F32[1], mantissa), scale);
    }
    return ctx.OpCompositeConstruct(ctx.F32[4], channels[0], channels[1], channels[2], ctx.ConstF32(1.0f));
}

static Id UnpackNativePacked10(EmitContext& ctx, Id texel, AmdGpu::NumberFormat number, u32 format = 9) {
    const bool signed_value = number == AmdGpu::NumberFormat::Snorm || number == AmdGpu::NumberFormat::Sint;
    const bool integer = AmdGpu::IsInteger(number);
    const auto word = ctx.OpCompositeExtract(ctx.U32[1], texel, 0U);
    std::array<Id, 4> channels{};
    for (u32 i = 0; i < 4; ++i) {
        const u32 bits = format == 10 ? 8 : i == 3 ? 2 : 10;
        const u32 offset = i * (format == 10 ? 8 : 10);
        const auto raw = signed_value ?
            ctx.OpBitFieldSExtract(ctx.U32[1], word, ctx.ConstU32(offset), ctx.ConstU32(bits)) :
            ctx.OpBitFieldUExtract(ctx.U32[1], word, ctx.ConstU32(offset), ctx.ConstU32(bits));
        if (integer) { channels[i] = ctx.OpBitcast(ctx.F32[1], raw); continue; }
        const auto decoded = signed_value ? ctx.OpConvertSToF(ctx.F32[1], ctx.OpBitcast(ctx.S32[1], raw)) :
                                            ctx.OpConvertUToF(ctx.F32[1], raw);
        channels[i] = ctx.OpFDiv(ctx.F32[1], decoded,
            ctx.ConstF32(float((1U << (bits - u32(signed_value))) - 1U)));
        if (signed_value) channels[i] = ctx.OpFMax(ctx.F32[1], channels[i], ctx.ConstF32(-1.0f));
    }
    return ctx.OpCompositeConstruct(ctx.F32[4], channels[0], channels[1], channels[2], channels[3]);
}
static Id PackNativePacked10(EmitContext& ctx, Id color, AmdGpu::NumberFormat number, u32 format = 9) {
    const bool signed_value = number == AmdGpu::NumberFormat::Snorm || number == AmdGpu::NumberFormat::Sint;
    const bool integer = AmdGpu::IsInteger(number);
    Id word = ctx.u32_zero_value;
    for (u32 i = 0; i < 4; ++i) {
        const u32 bits = format == 10 ? 8 : i == 3 ? 2 : 10;
        const auto value = ctx.OpCompositeExtract(ctx.F32[1], color, i);
        Id component;
        if (integer) {
            if (signed_value) {
                const auto clamped = ctx.OpSClamp(ctx.S32[1], ctx.OpBitcast(ctx.S32[1], value),
                    ctx.ConstS32(-s32(1U << (bits - 1))), ctx.ConstS32(s32((1U << (bits - 1)) - 1)));
                component = ctx.OpBitcast(ctx.U32[1], clamped);
            } else component = ctx.OpUMin(ctx.U32[1], ctx.OpBitcast(ctx.U32[1], value),
                                         ctx.ConstU32((1U << bits) - 1));
        } else {
            const auto clamped = ctx.OpFClamp(ctx.F32[1], value,
                ctx.ConstF32(signed_value ? -1.0f : 0.0f), ctx.ConstF32(1.0f));
            const auto scaled = ctx.OpFMul(ctx.F32[1], clamped,
                ctx.ConstF32(float((1U << (bits - u32(signed_value))) - 1U)));
            const auto rounded = ctx.OpRoundEven(ctx.F32[1], scaled);
            component = signed_value ? ctx.OpBitcast(ctx.U32[1], ctx.OpConvertFToS(ctx.S32[1], rounded)) :
                                       ctx.OpConvertFToU(ctx.U32[1], rounded);
        }
        component = ctx.OpBitwiseAnd(ctx.U32[1], component, ctx.ConstU32((1U << bits) - 1));
        word = ctx.OpBitwiseOr(ctx.U32[1], word, ctx.OpShiftLeftLogical(ctx.U32[1], component, ctx.ConstU32(i * (format == 10 ? 8 : 10))));
    }
    return ctx.OpCompositeConstruct(ctx.U32[4], word, ctx.u32_zero_value, ctx.u32_zero_value, ctx.u32_zero_value);
}
Id EmitImageRead(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id lod, Id ms) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    const Id color_type = texture.data_types->Get(4);
    Id fixed_coords = FixImageCoords<false>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    Id texel;
    if (!texture.is_storage) {
        const Id image = ctx.OpLoad(texture.image_type, texture.id);
        if (texture.view_type == AmdGpu::ImageType::Color2DMsaa ||
            texture.view_type == AmdGpu::ImageType::Color2DMsaaArray) {
            // GCN hardware wraps out-of-range MSAA sample indices
            if (Sirit::ValidId(ms)) {
                const Id sample_count = ctx.OpImageQuerySamples(ctx.U32[1], image);
                const Id wrapped_ms = ctx.OpUMod(ctx.U32[1], ms, sample_count);
                operands.Add(spv::ImageOperandsMask::Sample, wrapped_ms);
            }
        } else {
            if (Sirit::ValidId(ms)) {
                LOG_ERROR(Render_Recompiler, "image is not MS but ms operand is provided");
            }
            operands.Add(spv::ImageOperandsMask::Lod, lod);
            operands.Add(spv::ImageOperandsMask::Sample, ms);
        }
        texel = ctx.OpImageFetch(color_type, image, fixed_coords, operands.mask, operands.operands);
        if (ctx.profile.native_coherent_images) {
            const auto sharp = ctx.info.images[handle & 0xFFFF].GetSharp(ctx.info);
            const u32 format = u32(sharp.GetDataFmt());
            if (format >= 16 && format <= 18 &&
                sharp.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) {
                // Native B5/B6 fetches can expand through eight-bit UNORM.
                // Recover the original integer component before normalizing to
                // the guest width. This is valid for unfiltered ImageFetch only;
                // applying it after a filtered sample would quantize the result.
                const auto mapping = sharp.DstSelect();
                const std::array<u32, 4> selectors{u32(mapping.r), u32(mapping.g),
                                                   u32(mapping.b), u32(mapping.a)};
                const std::array<u32, 4> widths{5, format == 16 ? 6U : 5U, 5,
                                               format == 16 ? 0U : 1U};
                for (u32 channel = 0; channel < 4; ++channel) {
                    const u32 selector = selectors[channel];
                    if (selector < 4 || selector > 7 || !widths[selector - 4]) continue;
                    const Id maximum = ctx.ConstF32(float((1U << widths[selector - 4]) - 1));
                    const Id value = ctx.OpCompositeExtract(ctx.F32[1], texel, channel);
                    const Id integer = ctx.OpRoundEven(ctx.F32[1],
                        ctx.OpFMul(ctx.F32[1], value, maximum));
                    texel = ctx.OpCompositeInsert(ctx.F32[4],
                        ctx.OpFDiv(ctx.F32[1], integer, maximum), texel, channel);
                }
            }
        }
    } else {
        Id image_ptr = texture.id;
        if (texture.native_msaa_storage) {
            image_ptr = NativeStorageSample(ctx, handle, ms);
        } else if (ctx.profile.supports_image_load_store_lod) {
            operands.Add(spv::ImageOperandsMask::Lod, lod);
        } else if (Sirit::ValidId(lod) ||
                   (ctx.profile.native_coherent_images &&
                    texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex)) {
            // A writable image cannot use sampled-image fetch semantics. D3D12
            // binds one UAV per mip, so select the same descriptor-array element
            // used by the storage write path before issuing the typed load.
            ASSERT(texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex);
            const Id single_image_ptr_type =
                ctx.TypePointer(spv::StorageClass::UniformConstant, texture.image_type);
            image_ptr = ctx.OpAccessChain(single_image_ptr_type, image_ptr, std::array{BoundNativeStorageMip(ctx, handle, lod, fixed_coords)});
            if (ctx.profile.native_coherent_images)
                ctx.Decorate(image_ptr, spv::Decoration::NonUniform);
        }
        const Id image = ctx.OpLoad(texture.image_type, image_ptr);
        texel = ctx.OpImageRead(color_type, image, fixed_coords, operands.mask, operands.operands);
    }
    if (ctx.profile.native_coherent_images && texture.is_storage) {
        const auto sharp = ctx.info.images[handle & 0xFFFF].GetSharp(ctx.info);
        if (sharp.GetDataFmt() == AmdGpu::DataFormat::Format32_32_32) {
            const Id one = sharp.GetNumberFmt() == AmdGpu::NumberFormat::Float ? ctx.ConstF32(1.0f) :
                sharp.GetNumberFmt() == AmdGpu::NumberFormat::Sint ? ctx.ConstS32(1) : ctx.ConstU32(1U);
            texel = ctx.OpCompositeInsert(color_type, one, texel, 3U);
        }
    }
    if(texture.native_packed_format==4){
        const auto word=ctx.OpCompositeExtract(ctx.U32[1],texel,0U);
        const auto one=AmdGpu::IsInteger(texture.native_packed_number_format)?
            ctx.OpBitcast(ctx.F32[1],ctx.ConstU32(1U)):ctx.ConstF32(1.0f);
        texel=ctx.OpCompositeConstruct(ctx.F32[4],ctx.OpBitcast(ctx.F32[1],word),
            ctx.f32_zero_value,ctx.f32_zero_value,one);
    }else if(texture.native_packed_format==34)
        texel=UnpackNativeSharedExponent(ctx,texel);
    else if(texture.native_packed_format==9||texture.native_packed_format==10)
        texel=UnpackNativePacked10(ctx,texel,texture.native_packed_number_format,texture.native_packed_format);
    else if(texture.native_packed_format)texel=UnpackNativePacked16(ctx,texel,texture.native_packed_format);
    if (ctx.profile.native_coherent_images && texture.is_storage &&
        ctx.info.images[handle & 0xFFFF].native_srgb_read) {
        // The shared UAV is UNORM. Restore the sRGB SRV's RGB decode before
        // guest component mapping; alpha remains linear.
        std::array<Id, 4> channels{};
        for (u32 i = 0; i < 4; ++i) {
            const auto value = ctx.OpCompositeExtract(ctx.F32[1], texel, i);
            if (i == 3) { channels[i] = value; continue; }
            const auto low = ctx.OpFDiv(ctx.F32[1], value, ctx.ConstF32(12.92f));
            const auto high = ctx.OpPow(ctx.F32[1],
                ctx.OpFDiv(ctx.F32[1], ctx.OpFAdd(ctx.F32[1], value, ctx.ConstF32(0.055f)),
                           ctx.ConstF32(1.055f)), ctx.ConstF32(2.4f));
            channels[i] = ctx.OpSelect(ctx.F32[1],
                ctx.OpFOrdLessThanEqual(ctx.U1[1], value, ctx.ConstF32(0.04045f)), low, high);
        }
        texel = ctx.OpCompositeConstruct(ctx.F32[4], channels[0], channels[1], channels[2], channels[3]);
    }
    if (texture.native_packed_format) return texel;
    return texture.is_integer ? ctx.OpBitcast(ctx.F32[4], texel) : texel;
}

void EmitImageWrite(EmitContext& ctx, IR::Inst* inst, u32 handle, Id coords, Id lod, Id ms,
                    Id color) {
    const auto& texture = ctx.images[handle & 0xFFFF];
    Id image_ptr = texture.id;
    const Id color_type = texture.data_types->Get(4);
    Id fixed_coords = FixImageCoords<false>(ctx, coords, texture.view_type, texture.native_slice_view);
    ImageOperands operands;
    if (texture.native_msaa_storage) {
        image_ptr = NativeStorageSample(ctx, handle, ms);
    } else if (ctx.profile.supports_image_load_store_lod) {
        operands.Add(spv::ImageOperandsMask::Sample, ms);
        operands.Add(spv::ImageOperandsMask::Lod, lod);
    } else if (Sirit::ValidId(lod) ||
                   (ctx.profile.native_coherent_images &&
                    texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex)) {
        LOG_WARNING(Render, "Fallback for ImageWrite with LOD");
        ASSERT(texture.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex);
        const Id single_image_ptr_type =
            ctx.TypePointer(spv::StorageClass::UniformConstant, texture.image_type);
        image_ptr = ctx.OpAccessChain(single_image_ptr_type, image_ptr, std::array{BoundNativeStorageMip(ctx, handle, lod, fixed_coords)});
        if (ctx.profile.native_coherent_images)
            ctx.Decorate(image_ptr, spv::Decoration::NonUniform);
    }
    if (!texture.native_msaa_storage && !ctx.profile.supports_image_load_store_lod)
        operands.Add(spv::ImageOperandsMask::Sample, ms);
    const Id image = ctx.OpLoad(texture.image_type, image_ptr);
    const Id texel = texture.native_packed_format == 4 ?
        ctx.OpCompositeConstruct(ctx.U32[4],ctx.OpBitcast(ctx.U32[1],ctx.OpCompositeExtract(ctx.F32[1],color,0U)),
            ctx.u32_zero_value,ctx.u32_zero_value,ctx.u32_zero_value) :
        texture.native_packed_format == 9 || texture.native_packed_format == 10 ?
        PackNativePacked10(ctx, color, texture.native_packed_number_format,texture.native_packed_format) :
        texture.native_packed_format ? PackNativePacked16(ctx, color, texture.native_packed_format) :
        texture.is_integer ? ctx.OpBitcast(color_type, color) : color;
    ctx.OpImageWrite(image, fixed_coords, texel, operands.mask, operands.operands);
}

static Id SelectCubeResult(EmitContext& ctx, Id x, Id y, Id z, Id x_res, Id y_res, Id z_res) {
    const auto abs_x = ctx.OpFAbs(ctx.F32[1], x);
    const auto abs_y = ctx.OpFAbs(ctx.F32[1], y);
    const auto abs_z = ctx.OpFAbs(ctx.F32[1], z);

    const auto z_face_cond{ctx.OpLogicalAnd(ctx.U1[1],
                                            ctx.OpFOrdGreaterThanEqual(ctx.U1[1], abs_z, abs_x),
                                            ctx.OpFOrdGreaterThanEqual(ctx.U1[1], abs_z, abs_y))};
    const auto y_face_cond{ctx.OpFOrdGreaterThanEqual(ctx.U1[1], abs_y, abs_x)};

    return ctx.OpSelect(ctx.F32[1], z_face_cond, z_res,
                        ctx.OpSelect(ctx.F32[1], y_face_cond, y_res, x_res));
}

Id EmitCubeFaceIndex(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    if (ctx.profile.supports_native_cube_calc) {
        return ctx.OpCubeFaceIndexAMD(ctx.F32[1], ctx.OpCompositeConstruct(ctx.F32[3], x, y, z));
    }

    const auto x_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], x, ctx.f32_zero_value)};
    const auto y_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], y, ctx.f32_zero_value)};
    const auto z_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], z, ctx.f32_zero_value)};
    const auto x_face{ctx.OpSelect(ctx.F32[1], x_neg_cond, ctx.ConstF32(1.f), ctx.ConstF32(0.f))};
    const auto y_face{ctx.OpSelect(ctx.F32[1], y_neg_cond, ctx.ConstF32(3.f), ctx.ConstF32(2.f))};
    const auto z_face{ctx.OpSelect(ctx.F32[1], z_neg_cond, ctx.ConstF32(5.f), ctx.ConstF32(4.f))};

    return SelectCubeResult(ctx, x, y, z, x_face, y_face, z_face);
}

Id EmitCubeFaceCoordS(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const auto x_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], x, ctx.f32_zero_value)};
    const auto z_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], z, ctx.f32_zero_value)};
    const auto x_sc{ctx.OpSelect(ctx.F32[1], x_neg_cond, z, ctx.OpFNegate(ctx.F32[1], z))};
    const auto y_sc{x};
    const auto z_sc{ctx.OpSelect(ctx.F32[1], z_neg_cond, ctx.OpFNegate(ctx.F32[1], x), x)};

    return SelectCubeResult(ctx, x, y, z, x_sc, y_sc, z_sc);
}

Id EmitCubeFaceCoordT(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const auto y_neg_cond{ctx.OpFOrdLessThan(ctx.U1[1], y, ctx.f32_zero_value)};
    const auto x_z_tc{ctx.OpFNegate(ctx.F32[1], y)};
    const auto y_tc{ctx.OpSelect(ctx.F32[1], y_neg_cond, ctx.OpFNegate(ctx.F32[1], z), z)};

    return SelectCubeResult(ctx, x, y, z, x_z_tc, y_tc, x_z_tc);
}

Id EmitCubeFaceMajorAxis(EmitContext& ctx, IR::Inst* inst, Id x, Id y, Id z) {
    const auto two{ctx.ConstF32(2.f)};
    const auto x_major_axis{ctx.OpFMul(ctx.F32[1], x, two)};
    const auto y_major_axis{ctx.OpFMul(ctx.F32[1], y, two)};
    const auto z_major_axis{ctx.OpFMul(ctx.F32[1], z, two)};

    return SelectCubeResult(ctx, x, y, z, x_major_axis, y_major_axis, z_major_axis);
}

} // namespace Shader::Backend::SPIRV
