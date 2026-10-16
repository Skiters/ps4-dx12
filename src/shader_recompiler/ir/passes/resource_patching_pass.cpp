// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <limits>
#include <stdexcept>
#include "shader_recompiler/info.h"
#include "shader_recompiler/native_sampler.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/ir_emitter.h"
#include "shader_recompiler/ir/opcodes.h"
#include "shader_recompiler/ir/operand_helper.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "shader_recompiler/ir/passes/resource_pass.h"
#include "shader_recompiler/ir/reinterpret.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/resource.h"
#include "video_core/amdgpu/resource.h"

namespace Shader::Optimization {
namespace {

IR::Type BufferDataType(const IR::Inst& inst, const Profile& profile) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::LoadBufferU8:
    case IR::Opcode::StoreBufferU8:
        return IR::Type::U8;
    case IR::Opcode::LoadBufferU16:
    case IR::Opcode::StoreBufferU16:
        return IR::Type::U16;
    case IR::Opcode::LoadBufferU64:
    case IR::Opcode::StoreBufferU64:
    case IR::Opcode::BufferAtomicIAdd64:
    case IR::Opcode::BufferAtomicIncBounded64:
    case IR::Opcode::BufferAtomicDecBounded64:
    case IR::Opcode::BufferAtomicAnd64:
    case IR::Opcode::BufferAtomicOr64:
    case IR::Opcode::BufferAtomicXor64:
    case IR::Opcode::BufferAtomicSwap64:
    case IR::Opcode::BufferAtomicCmpSwap64:
    case IR::Opcode::BufferAtomicSMax64:
    case IR::Opcode::BufferAtomicSMin64:
    case IR::Opcode::BufferAtomicUMax64:
    case IR::Opcode::BufferAtomicUMin64:
        return IR::Type::U64;
    case IR::Opcode::BufferAtomicFMax32:
    case IR::Opcode::BufferAtomicFMin32:
        return profile.supports_buffer_fp32_atomic_min_max ? IR::Type::F32 : IR::Type::U32;
    case IR::Opcode::LoadBufferFormatF32:
    case IR::Opcode::StoreBufferFormatF32:
        // Formatted buffer loads can use a variety of types.
        return IR::Type::U32 | IR::Type::F32 | IR::Type::U16 | IR::Type::U8;
    default:
        return IR::Type::U32;
    }
}

u32 BufferAddressShift(const IR::Inst& inst, AmdGpu::DataFormat data_format) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::LoadBufferU8:
    case IR::Opcode::StoreBufferU8:
        return 0;
    case IR::Opcode::LoadBufferU16:
    case IR::Opcode::StoreBufferU16:
        return 1;
    case IR::Opcode::LoadBufferU64:
    case IR::Opcode::StoreBufferU64:
    case IR::Opcode::BufferAtomicIAdd64:
    case IR::Opcode::BufferAtomicIncBounded64:
    case IR::Opcode::BufferAtomicDecBounded64:
    case IR::Opcode::BufferAtomicAnd64:
    case IR::Opcode::BufferAtomicOr64:
    case IR::Opcode::BufferAtomicXor64:
    case IR::Opcode::BufferAtomicSwap64:
    case IR::Opcode::BufferAtomicCmpSwap64:
    case IR::Opcode::BufferAtomicSMax64:
    case IR::Opcode::BufferAtomicSMin64:
    case IR::Opcode::BufferAtomicUMax64:
    case IR::Opcode::BufferAtomicUMin64:
        return 3;
    case IR::Opcode::LoadBufferFormatF32:
    case IR::Opcode::StoreBufferFormatF32: {
        switch (data_format) {
        case AmdGpu::DataFormat::Format8:
            return 0;
        case AmdGpu::DataFormat::Format8_8:
        case AmdGpu::DataFormat::Format16:
            return 1;
        case AmdGpu::DataFormat::Format8_8_8_8:
        case AmdGpu::DataFormat::Format16_16:
        case AmdGpu::DataFormat::Format10_11_11:
        case AmdGpu::DataFormat::Format2_10_10_10:
        case AmdGpu::DataFormat::Format16_16_16_16:
        case AmdGpu::DataFormat::Format32:
        case AmdGpu::DataFormat::Format32_32:
        case AmdGpu::DataFormat::Format32_32_32:
        case AmdGpu::DataFormat::Format32_32_32_32:
            return 2;
        default:
            return 0;
        }
        break;
    }
    case IR::Opcode::ReadConstBuffer:
        // Provided address is already in dwords
        return 0;
    default:
        return 2;
    }
}

class Descriptors {
public:
    explicit Descriptors(Info& info_)
        : info{info_}, buffer_resources{info_.buffers}, image_resources{info_.images},
          sampler_resources{info_.samplers}, fmask_resources(info_.fmasks) {}

    u32 Add(const BufferResource& desc) {
        const u32 index{Add(buffer_resources, desc, [&desc](const auto& existing) {
            return desc.sharp_fetch == existing.sharp_fetch && desc.post_op == existing.post_op &&
                   desc.post_op_dw1_mask == existing.post_op_dw1_mask &&
                   desc.buffer_type == existing.buffer_type;
        })};
        auto& buffer = buffer_resources[index];
        buffer.used_types |= desc.used_types;
        buffer.is_written |= desc.is_written;
        buffer.is_formatted |= desc.is_formatted;
        return index;
    }

    u32 Add(const ImageResource& desc) {
        const u32 index{Add(image_resources, desc, [&desc](const auto& existing) {
            return desc.sharp_fetch == existing.sharp_fetch && desc.is_array == existing.is_array &&
                   desc.mip_fallback_mode == existing.mip_fallback_mode &&
                   desc.constant_mip_index == existing.constant_mip_index &&
                   desc.native_srgb_read == existing.native_srgb_read &&
                   desc.native_msaa_storage == existing.native_msaa_storage &&
                   desc.native_packed10_raw == existing.native_packed10_raw &&
                   desc.native_raw32 == existing.native_raw32 &&
                   desc.post_op == existing.post_op;
        })};
        auto& image = image_resources[index];
        image.is_atomic |= desc.is_atomic;
        image.is_written |= desc.is_written;
        return index;
    }

    u32 Add(const SamplerResource& desc) {
        const u32 index{Add(sampler_resources, desc, [this, &desc](const auto& existing) {
            return desc.sharp_fetch == existing.sharp_fetch && desc.post_op == existing.post_op &&
                   desc.post_op_tsharp_dw3_off == existing.post_op_tsharp_dw3_off &&
                   desc.is_depth == existing.is_depth;
        })};
        sampler_resources[index].native_manual_sampling |= desc.native_manual_sampling;
        return index;
    }

    u32 Add(const FMaskResource& desc) {
        u32 index = Add(fmask_resources, desc, [&desc](const auto& existing) {
            return desc.sharp_idx == existing.sharp_idx;
        });
        return index;
    }

private:
    template <typename Descriptors, typename Descriptor, typename Func>
    static u32 Add(Descriptors& descriptors, const Descriptor& desc, Func&& pred) {
        const auto it{std::ranges::find_if(descriptors, pred)};
        if (it != descriptors.end()) {
            return static_cast<u32>(std::distance(descriptors.begin(), it));
        }
        descriptors.push_back(desc);
        return static_cast<u32>(descriptors.size()) - 1;
    }

    const Info& info;
    BufferResourceList& buffer_resources;
    ImageResourceList& image_resources;
    SamplerResourceList& sampler_resources;
    FMaskResourceList& fmask_resources;
};

} // Anonymous namespace

using SharpSources = boost::container::small_vector<const IR::Inst*, 4>;

SharpLocation SharpLocationFromSource(const IR::Inst* inst) {
    SharpLocation location{};
    if (inst->GetOpcode() == IR::Opcode::GetUserData) {
        return static_cast<SharpLocation>(inst->Arg(0).ScalarReg());
    } else if (inst->GetOpcode() == IR::Opcode::ReadConstBuffer) {
        location = inst->Flags<IR::BufferInstInfo>().flatbuf_off_dw;
    } else {
        location = inst->Flags<SharpLocation>();
    }
    if (location == 0) {
        LOG_WARNING(Render_Recompiler, "Sharp source was not flatenned");
        return UNKNOWN_LOCATION;
    }
    return location;
}

template <typename T>
SharpFetch<T> ConstructSharpFetch(const SharpReference& sharp) {
    using Summary = SharpFetch<T>::Summary;
    SharpFetch<T> sharp_fetch{};
    for (u32 i = 0; i < sharp.num_dwords; i++) {
        auto dword = sharp.dwords[i];
        if (dword.IsImmediate()) {
            sharp_fetch.immediates[i] = dword.U32();
        } else {
            sharp_fetch.offsets[i] = SharpLocationFromSource(dword.Inst());
            sharp_fetch.load_mask |= (1 << i);
            if (sharp_fetch.offsets[i] == UNKNOWN_LOCATION) {
                sharp_fetch.summary = Summary::Invalid;
            }
        }
    }
    if (sharp_fetch.summary != Summary::Invalid) {
        const u32 base = sharp_fetch.offsets[0];
        for (u32 i = 1; i < sharp.num_dwords; ++i) {
            if (sharp_fetch.offsets[i] - base != i) {
                return sharp_fetch;
            }
        }
        sharp_fetch.summary = Summary::SingleLoad;
    }
    return sharp_fetch;
}

void PatchBufferSharp(const ResourceDiscovery& resource, Info& info, Descriptors& descriptors,
                      const Profile& profile) {
    IR::Inst& inst = *resource.user;

    const u32 buffer_binding = descriptors.Add(BufferResource{
        .sharp_fetch = ConstructSharpFetch<AmdGpu::Buffer>(resource.sharps[0]),
        .used_types = BufferDataType(inst, profile),
        .buffer_type = BufferType::Guest,
        .is_written = IsBufferStore(inst),
        .is_formatted = inst.GetOpcode() == IR::Opcode::LoadBufferFormatF32 ||
                        inst.GetOpcode() == IR::Opcode::StoreBufferFormatF32,
        .post_op = resource.sharps[0].post_op,
        .post_op_dw1_mask = resource.sharps[0].post_op_data.dw1_mask,
        .post_op_dw2_shift = resource.sharps[0].post_op_data.dw2_shift,
    });

    // Replace handle with binding index in buffer resource list.
    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
    inst.SetArg(0, ir.Imm32(buffer_binding));
}

void PatchImageSharp(const ResourceDiscovery& resource, Info& info, Descriptors& descriptors,
                     const Profile& profile, bool storage_read = false, bool srgb_read = false,
                     bool packed10_raw = false, bool raw32 = false) {
    IR::Inst& inst = *resource.user;

    // Read image sharp.
    const auto inst_info = inst.Flags<IR::TextureInstInfo>();
    const bool is_atomic = IsImageAtomicInstruction(inst);
    const bool is_written = inst.GetOpcode() == IR::Opcode::ImageWrite || is_atomic || storage_read;
    // A read sharing a written sharp must select a mip UAV as well. Classify
    // that access before choosing ConstantIndex/DynamicIndex descriptor views.
    const bool needs_mip_storage_fallback =
        (inst_info.has_lod || (profile.native_coherent_images &&
            inst.GetOpcode() == IR::Opcode::ImageSampleRaw)) &&
        is_written && !profile.supports_image_load_store_lod;
    ImageResource image_res = {
        .sharp_fetch = ConstructSharpFetch<AmdGpu::Image>(resource.sharps[0]),
        .is_depth = bool(inst_info.is_depth),
        .is_atomic = is_atomic,
        .is_array = bool(inst_info.is_array),
        .is_written = is_written,
        .is_r128 = bool(inst_info.is_r128),
        .native_srgb_read = srgb_read,
        .native_packed10_raw = is_written && packed10_raw,
        .native_raw32 = is_written && raw32,
        .post_op = resource.sharps[0].post_op,
    };

    auto image = image_res.GetSharp(info);
    ASSERT(image.GetType() != AmdGpu::ImageType::Invalid);
    image_res.native_msaa_storage = profile.native_coherent_images && is_written &&
        (image.GetType() == AmdGpu::ImageType::Color2DMsaa ||
         image.GetType() == AmdGpu::ImageType::Color2DMsaaArray);

    if (image_res.native_msaa_storage) {
        image_res.mip_fallback_mode = MipStorageFallbackMode::None;
    } else if (needs_mip_storage_fallback && inst.GetOpcode() == IR::Opcode::ImageSampleRaw) {
        // Sample's packed address operands differ from IMAGE_LOAD_MIP's body.
        // Never decode them using the load/store layout.
        image_res.mip_fallback_mode = MipStorageFallbackMode::DynamicIndex;
    } else if (needs_mip_storage_fallback) {
        // If the mip level to IMAGE_(LOAD/STORE)_MIP is a constant, set up ImageResource
        // so that we will only bind a single level.
        // If index is dynamic, we will bind levels as an array
        const auto view_type = image.GetViewType(image_res.is_array);

        IR::Inst* body = inst.Arg(1).Inst();
        const auto lod_arg = [&] -> IR::Value {
            switch (view_type) {
            case AmdGpu::ImageType::Color1D: // x, [lod]
                return body->Arg(1);
            case AmdGpu::ImageType::Color1DArray: // x, slice, [lod]
            case AmdGpu::ImageType::Color2D:      // x, y, [lod]
                return body->Arg(2);
            case AmdGpu::ImageType::Color2DArray: // x, y, slice, [lod]
            case AmdGpu::ImageType::Cube:         // x, y, face, [lod]
            case AmdGpu::ImageType::Color3D:      // x, y, z, [lod]
                return body->Arg(3);
            case AmdGpu::ImageType::Color2DMsaa:
            case AmdGpu::ImageType::Color2DMsaaArray:
            default:
                UNREACHABLE_MSG("Invalid image type {}", view_type);
            }
        }();

        // Invalid native constants must retain their full value and use the
        // bounded array access, rather than truncating into a single UAV mip.
        const bool valid_constant = lod_arg.IsImmediate() &&
            (!profile.native_coherent_images ||
             lod_arg.U32() < u32(image.last_level - image.base_level + 1));
        if (valid_constant) {
            image_res.mip_fallback_mode = MipStorageFallbackMode::ConstantIndex;
            image_res.constant_mip_index = lod_arg.U32();
        } else {
            image_res.mip_fallback_mode = MipStorageFallbackMode::DynamicIndex;
        }
    }

    // Patch image instruction if image is FMask.
    if (AmdGpu::IsFmask(image.GetDataFmt())) {
        ASSERT_MSG(!is_written, "FMask storage instructions are not supported");

        IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
        switch (inst.GetOpcode()) {
        case IR::Opcode::ImageRead:
        case IR::Opcode::ImageSampleRaw: {
            IR::F32 fmaskx = ir.BitCast<IR::F32>(ir.Imm32(0x76543210));
            IR::F32 fmasky = ir.BitCast<IR::F32>(ir.Imm32(0xfedcba98));
            inst.ReplaceUsesWith(ir.CompositeConstruct(fmaskx, fmasky));
            return;
        }
        case IR::Opcode::ImageQueryLod:
            inst.ReplaceUsesWith(ir.Imm32(1));
            return;
        case IR::Opcode::ImageQueryDimensions: {
            IR::Value dims = ir.CompositeConstruct(ir.Imm32(static_cast<u32>(image.width)),  // x
                                                   ir.Imm32(static_cast<u32>(image.height)), // y
                                                   ir.Imm32(1), ir.Imm32(1)); // depth, mip
            inst.ReplaceUsesWith(dims);

            // Track FMask resource to do specialization.
            descriptors.Add(FMaskResource{
                .sharp_idx = SharpLocationFromSource(resource.sharps[0].dwords[0].Inst()),
            });
            return;
        }
        default:
            UNREACHABLE_MSG("Can't patch fmask instruction {}", inst.GetOpcode());
        }
    }

    u32 image_binding = descriptors.Add(image_res);

    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};

    if (inst.GetOpcode() == IR::Opcode::ImageSampleRaw) {
        auto& lod_prod = resource.sharps[1].post_op_data.lod_prod;
        const u32 sampler_binding = descriptors.Add(SamplerResource{
            .sharp_fetch = ConstructSharpFetch<AmdGpu::Sampler>(resource.sharps[1]),
            .post_op = resource.sharps[1].post_op,
            .post_op_tsharp_dw3_off =
                lod_prod.IsEmpty() ? UNKNOWN_LOCATION : SharpLocationFromSource(lod_prod.Inst()),
            .is_depth = bool(inst_info.is_depth), // true for the _C (compare) opcodes
            .native_manual_sampling = profile.native_coherent_images &&
                (image_res.is_written || AmdGpu::IsInteger(image.GetNumberFmt()) ||
                 (inst_info.is_gather && !inst_info.force_level0) ||
                 (inst_info.is_depth && (inst_info.has_bias || inst_info.has_derivatives))),
        });
        auto& tracked_sampler = info.samplers[sampler_binding];
        tracked_sampler.native_manual_sampling |= profile.native_coherent_images &&
            (NativeSampler::RequiresManualAddressing(tracked_sampler.GetSharp(info), image.GetViewType(image_res.is_array)) ||
             (!inst_info.is_gather && NativeSampler::RequiresManualAnisotropy(tracked_sampler.GetSharp(info))));
        inst.SetArg(0, ir.Imm32(image_binding | sampler_binding << 16));
    } else {
        inst.SetArg(0, ir.Imm32(image_binding));
    }
}

static IR::Value IsMbcntWithExec(IR::Value value, bool is_hi, IR::U1 exec) {
    if (value.IsImmediate()) {
        return {};
    }
    const IR::Inst* inst = value.Inst();
    if (inst->GetOpcode() != IR::Opcode::MaskedBitCount32 || inst->Arg(0).IsImmediate() ||
        inst->Arg(2).U1() != is_hi) {
        return {};
    }
    IR::Inst* prod = inst->Arg(0).Inst();
    if (prod->GetOpcode() != IR::Opcode::CompositeExtractU32x2 || prod->Arg(0).IsImmediate() ||
        prod->Arg(1).U32() != is_hi) {
        return {};
    }
    prod = prod->Arg(0).Inst();
    if (prod->GetOpcode() != IR::Opcode::UnpackUint2x32 || prod->Arg(0).IsImmediate()) {
        return {};
    }
    prod = prod->Arg(0).Inst();
    if (prod->GetOpcode() != IR::Opcode::Ballot || (prod->Arg(0) != exec && !is_hi)) {
        return {};
    }
    return inst->Arg(1);
}

static IR::Use FindUniqueUser(IR::Inst* inst, auto&& pred) {
    IR::Use picked{};
    for (auto& use : inst->Uses()) {
        if (pred(use.user)) {
            ASSERT(!picked.user);
            picked = use;
        }
    }
    ASSERT(picked.user);
    return picked;
}

static void RemoveAppendBufferLaneOffset(IR::Inst& vx) {
    // Attempt to detect either of the following patterns and
    // remove the lane id addition/subtraction to ds instruction.
    //
    // ds_append       vX gds
    // v_mbcnt_hi_u32_b32 vY, exec_hi, vX
    // v_mbcnt_lo_u32_b32 idx, exec_lo, vY
    //
    // v_mbcnt_hi_u32_b32 vY, exec_hi, 0
    // v_mbcnt_lo_u32_b32 vZ, exec_lo, vY
    // ds_append       vX gds
    // v_add_i32       idx, vcc, vX, vZ
    //
    // ds_consume      vX gds
    // v_mbcnt_hi_u32_b32 vY, exec_hi, 0
    // v_mbcnt_lo_u32_b32 vZ, exec_lo, vY
    // v_sub_i32       vK, vcc, vX, vZ
    // v_subrev_i32    idx, vcc, 1, vK / v_add_i32    idx, vcc, -1, vK
    //
    // ds_consume      vX gds
    // v_mbcnt_hi_u32_b32 vY, exec_hi, 0
    // v_mbcnt_lo_u32_b32 vZ, exec_lo, vY
    // v_add_i32       vK, vcc, -1, vX
    // v_sub_i32       idx, vcc, vK, vZ

    IR::U1 exec{vx.Arg(1)};
    const auto uses = vx.Uses();
    for (auto use : uses) {
        const auto& [user, operand] = use;
        if (user->GetOpcode() == IR::Opcode::MaskedBitCount32) {
            // First pattern
            ASSERT(vx.GetOpcode() == IR::Opcode::DataAppend);
            IR::Inst* vy = user;
            auto vx_2 = IsMbcntWithExec(IR::Value{vy}, true, exec);
            ASSERT(!vx_2.IsEmpty() && vx_2 == IR::Value{&vx});
            const auto [idx, operand] = FindUniqueUser(vy, [](const IR::Inst* inst) {
                return inst->GetOpcode() == IR::Opcode::MaskedBitCount32;
            });
            auto vy_2 = IsMbcntWithExec(IR::Value{idx}, false, exec);
            ASSERT(!vy_2.IsEmpty() && vy_2 == IR::Value{vy});
            idx->ReplaceUsesWithAndRemove(IR::Value{&vx});
            continue;
        }
        if (user->GetOpcode() != IR::Opcode::IAdd32 && user->GetOpcode() != IR::Opcode::ISub32) {
            continue;
        }
        const auto other = user->Arg(1 - operand);
        if (other.IsImmediate()) {
            ASSERT_MSG(other.U32() == 1u || other.U32() == std::numeric_limits<u32>::max() &&
                                                vx.GetOpcode() == IR::Opcode::DataConsume,
                       "Unexpected constant offset {} to DataConsume result", other.U32());
            use = FindUniqueUser(user, [](const IR::Inst* inst) {
                return inst->GetOpcode() == IR::Opcode::IAdd32 ||
                       inst->GetOpcode() == IR::Opcode::ISub32;
            });
        }

        // Other patterns
        auto vz = user->Arg(1 - operand);
        auto vy = IsMbcntWithExec(vz, false, exec);
        ASSERT(!vy.IsEmpty() && !vy.IsImmediate());
        auto zero_const = IsMbcntWithExec(vy, true, exec);
        ASSERT(!zero_const.IsEmpty() && zero_const.IsImmediate() && zero_const.U32() == 0u);
        user->SetArg(1 - operand, IR::Value{u32{0u}});
    }
}

void PatchGlobalDataShareAccess(IR::Inst& inst, Info& info, Descriptors& descriptors,
                                const Profile& profile) {
    const u32 binding = descriptors.Add(BufferResource{
        .used_types = IR::Type::U32,
        .buffer_type = BufferType::GdsBuffer,
        .is_written = true,
    });

    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};

    if (inst.GetOpcode() == IR::Opcode::DataAppend || inst.GetOpcode() == IR::Opcode::DataConsume) {
        RemoveAppendBufferLaneOffset(inst);
        const IR::Value replacement =
            inst.GetOpcode() == IR::Opcode::DataAppend
                ? ir.BufferAtomicIAdd(ir.Imm32(binding), inst.Arg(0), ir.Imm32(1u), {})
                : ir.BufferAtomicISub(ir.Imm32(binding), inst.Arg(0), ir.Imm32(1u), {});
        inst.ReplaceUsesWithAndRemove(replacement);
        return;
    }

    // Convert shared memory opcode to storage buffer atomic to GDS buffer.
    auto& buffer = info.buffers[binding];
    const IR::U32 offset = IR::U32{inst.Arg(0)};
    const IR::U32 address_words = ir.ShiftRightLogical(offset, ir.Imm32(1));
    const IR::U32 address_dwords = ir.ShiftRightLogical(offset, ir.Imm32(2));
    const IR::U32 address_qwords = ir.ShiftRightLogical(offset, ir.Imm32(3));
    const IR::U32 handle = ir.Imm32(binding);
    switch (inst.GetOpcode()) {
    case IR::Opcode::SharedAtomicIAdd32:
        inst.ReplaceUsesWith(ir.BufferAtomicIAdd(handle, address_dwords, inst.Arg(1), {}));
        break;
    case IR::Opcode::SharedAtomicIAdd64:
        inst.ReplaceUsesWith(ir.BufferAtomicIAdd(handle, address_qwords, IR::U64{inst.Arg(1)}, {}));
        break;
    case IR::Opcode::SharedAtomicISub32:
        inst.ReplaceUsesWith(ir.BufferAtomicISub(handle, address_dwords, inst.Arg(1), {}));
        break;
    case IR::Opcode::SharedAtomicSMin32:
    case IR::Opcode::SharedAtomicUMin32: {
        const bool is_signed = inst.GetOpcode() == IR::Opcode::SharedAtomicSMin32;
        inst.ReplaceUsesWith(
            ir.BufferAtomicIMin(handle, address_dwords, inst.Arg(1), is_signed, {}));
        break;
    }
    case IR::Opcode::SharedAtomicSMax32:
    case IR::Opcode::SharedAtomicUMax32: {
        const bool is_signed = inst.GetOpcode() == IR::Opcode::SharedAtomicSMax32;
        inst.ReplaceUsesWith(
            ir.BufferAtomicIMax(handle, address_dwords, inst.Arg(1), is_signed, {}));
        break;
    }
    case IR::Opcode::SharedAtomicInc32:
        inst.ReplaceUsesWith(ir.BufferAtomicInc(handle, address_dwords, {}));
        break;
    case IR::Opcode::SharedAtomicDec32:
        inst.ReplaceUsesWith(ir.BufferAtomicDec(handle, address_dwords, {}));
        break;
    case IR::Opcode::SharedAtomicAnd32:
        inst.ReplaceUsesWith(ir.BufferAtomicAnd(handle, address_dwords, inst.Arg(1), {}));
        break;
    case IR::Opcode::SharedAtomicOr32:
        inst.ReplaceUsesWith(ir.BufferAtomicOr(handle, address_dwords, inst.Arg(1), {}));
        break;
    case IR::Opcode::SharedAtomicXor32:
        inst.ReplaceUsesWith(ir.BufferAtomicXor(handle, address_dwords, inst.Arg(1), {}));
        break;
    case IR::Opcode::SharedAtomicCmpSwap32:
        // Args are (address, value, cmp_value)
        inst.ReplaceUsesWith(
            ir.BufferAtomicCmpSwap(handle, address_dwords, inst.Arg(1), inst.Arg(2), {}));
        break;
    case IR::Opcode::LoadSharedU16: {
        inst.ReplaceUsesWith(ir.LoadBufferU16(handle, address_words, {}));
        buffer.used_types |= IR::Type::U16;
        break;
    }
    case IR::Opcode::LoadSharedU32:
        inst.ReplaceUsesWith(ir.LoadBufferU32(1, handle, address_dwords, {}));
        break;
    case IR::Opcode::LoadSharedU64: {
        inst.ReplaceUsesWith(ir.LoadBufferU64(handle, address_qwords, {}));
        buffer.used_types |= IR::Type::U64;
        break;
    }
    case IR::Opcode::WriteSharedU16: {
        ir.StoreBufferU16(handle, address_words, IR::U16{inst.Arg(1)}, {});
        inst.Invalidate();
        buffer.used_types |= IR::Type::U16;
        break;
    }
    case IR::Opcode::WriteSharedU32:
        ir.StoreBufferU32(1, handle, address_dwords, inst.Arg(1), {});
        inst.Invalidate();
        break;
    case IR::Opcode::WriteSharedU64: {
        ir.StoreBufferU64(handle, address_qwords, IR::U64{inst.Arg(1)}, {});
        inst.Invalidate();
        buffer.used_types |= IR::Type::U64;
        break;
    }
    default:
        UNREACHABLE_MSG("Unexpected opcode {}", inst.GetOpcode());
    }
}

IR::U32 CalculateBufferAddress(IR::IREmitter& ir, const IR::Inst& inst, const Info& info,
                               const AmdGpu::Buffer& buffer, u32 stride) {
    const auto inst_info = inst.Flags<IR::BufferInstInfo>();
    const u32 inst_offset = inst_info.inst_offset.Value();
    const auto is_inst_typed = inst_info.inst_data_fmt != AmdGpu::DataFormat::FormatInvalid;
    const auto data_format = is_inst_typed
                                 ? AmdGpu::RemapDataFormat(inst_info.inst_data_fmt.Value())
                                 : buffer.GetDataFmt();
    const u32 shift = BufferAddressShift(inst, data_format);
    const u32 mask = (1 << shift) - 1;
    const IR::U32 soffset = IR::GetBufferSOffsetArg(&inst);

    // If address calculation is of the form "index * const_stride + offset" with offset constant
    // and both const_stride and offset are divisible with the element size, apply shift directly.
    if (inst_info.index_enable && !inst_info.voffset_enable && soffset.IsImmediate() &&
        !buffer.swizzle_enable && !buffer.add_tid_enable && (stride & mask) == 0) {
        const u32 total_offset = soffset.U32() + inst_offset;
        if ((total_offset & mask) == 0) {
            // buffer_offset = index * (const_stride >> shift) + (offset >> shift)
            const IR::U32 index = IR::GetBufferIndexArg(&inst);
            return ir.IAdd(ir.IMul(index, ir.Imm32(stride >> shift)),
                           ir.Imm32(total_offset >> shift));
        }
    }

    // index = (inst_idxen ? vgpr_index : 0) + (const_add_tid_enable ? thread_id[5:0] : 0)
    IR::U32 index = ir.Imm32(0U);
    if (inst_info.index_enable) {
        const IR::U32 vgpr_index = IR::GetBufferIndexArg(&inst);
        index = ir.IAdd(index, vgpr_index);
    }
    if (buffer.add_tid_enable) {
        ASSERT_MSG(info.sw_stage == SwStage::Compute,
                   "Thread ID buffer addressing is not supported outside of compute.");
        const IR::U32 thread_id{ir.LaneId()};
        index = ir.IAdd(index, thread_id);
    }
    // offset = (inst_offen ? vgpr_offset : 0) + inst_offset
    IR::U32 offset = ir.Imm32(inst_offset);
    offset = ir.IAdd(offset, soffset);
    if (inst_info.voffset_enable) {
        const IR::U32 voffset = IR::GetBufferVOffsetArg(&inst);
        offset = ir.IAdd(offset, voffset);
    }
    const IR::U32 const_stride = ir.Imm32(stride);
    IR::U32 buffer_offset;
    if (buffer.swizzle_enable) {
        const IR::U32 const_index_stride = ir.Imm32(buffer.GetIndexStride());
        const IR::U32 const_element_size = ir.Imm32(buffer.GetElementSize());
        // index_msb = index / const_index_stride
        const IR::U32 index_msb{ir.IDiv(index, const_index_stride)};
        // index_lsb = index % const_index_stride
        const IR::U32 index_lsb{ir.IMod(index, const_index_stride)};
        // offset_msb = offset / const_element_size
        const IR::U32 offset_msb{ir.IDiv(offset, const_element_size)};
        // offset_lsb = offset % const_element_size
        const IR::U32 offset_lsb{ir.IMod(offset, const_element_size)};
        // buffer_offset =
        //     (index_msb * const_stride + offset_msb * const_element_size) * const_index_stride
        //     + index_lsb * const_element_size + offset_lsb
        const IR::U32 buffer_offset_msb = ir.IMul(
            ir.IAdd(ir.IMul(index_msb, const_stride), ir.IMul(offset_msb, const_element_size)),
            const_index_stride);
        const IR::U32 buffer_offset_lsb =
            ir.IAdd(ir.IMul(index_lsb, const_element_size), offset_lsb);
        buffer_offset = ir.IAdd(buffer_offset_msb, buffer_offset_lsb);
    } else {
        // buffer_offset = index * const_stride + offset
        buffer_offset = ir.IAdd(ir.IMul(index, const_stride), offset);
    }
    if (shift != 0) {
        buffer_offset = ir.ShiftRightLogical(buffer_offset, ir.Imm32(shift));
    }
    return buffer_offset;
}

void PatchBufferArgs(IR::Inst& inst, Info& info) {
    const auto handle = inst.Arg(0);
    const auto buffer_res = info.buffers[handle.U32()];
    const auto buffer = buffer_res.GetSharp(info);

    // Address of constant buffer reads can be calculated at IR emission time.
    if (inst.GetOpcode() == IR::Opcode::ReadConstBuffer) {
        return;
    }

    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
    inst.SetArg(IR::LoadBufferArgs::Address,
                CalculateBufferAddress(ir, inst, info, buffer, buffer.stride));
}

void PatchImageSampleArgs(IR::Inst& inst, Info& info, const ImageResource& image_res,
                          const AmdGpu::Image& image, const Profile& profile) {
    const auto handle = inst.Arg(0);
    const auto& sampler_res = info.samplers[(handle.U32() >> 16) & 0xFFFF];
    const auto sampler = sampler_res.GetSharp(info);

    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
    const auto inst_info = inst.Flags<IR::TextureInstInfo>();
    const auto view_type = image.GetViewType(image_res.is_array);

    IR::Inst* body1 = inst.Arg(2).Inst();
    IR::Inst* body2 = inst.Arg(3).Inst();
    IR::Inst* body3 = inst.Arg(4).Inst();
    IR::F32 body4 = IR::F32{inst.Arg(5)};
    const auto get_addr_reg = [&](u32 index) -> IR::F32 {
        if (index <= 3) {
            return IR::F32{body1->Arg(index)};
        }
        if (index >= 4 && index <= 7) {
            return IR::F32{body2->Arg(index - 4)};
        }
        if (index >= 8 && index <= 11) {
            return IR::F32{body3->Arg(index - 8)};
        }
        if (index == 12) {
            return body4;
        }
        UNREACHABLE();
    };
    u32 addr_reg = 0;

    // Load first address components as denoted in 8.2.4 VGPR Usage Sea Islands Series Instruction
    // Set Architecture
    const IR::Value offset = [&] -> IR::Value {
        if (!inst_info.has_offset) {
            return IR::U32{};
        }

        // The offsets are six-bit signed integers: X=[5:0], Y=[13:8], and Z=[21:16].
        IR::Value arg = get_addr_reg(addr_reg++);
        if (const IR::Inst* offset_inst = arg.TryInst()) {
            ASSERT(offset_inst->GetOpcode() == IR::Opcode::BitCastF32U32);
            arg = offset_inst->Arg(0);
        }

        const auto read = [&](u32 off) -> IR::U32 {
            if (arg.IsImmediate()) {
                const u32 imm =
                    arg.Type() == IR::Type::F32 ? std::bit_cast<u32>(arg.F32()) : arg.U32();
                const u16 comp = (imm >> off) & 0x3F;
                return ir.Imm32(s32(comp << 26) >> 26);
            }
            return ir.BitFieldExtract(IR::U32{arg}, ir.Imm32(off), ir.Imm32(6), true);
        };

        switch (view_type) {
        case AmdGpu::ImageType::Color1D:
        case AmdGpu::ImageType::Color1DArray:
            return read(0);
        case AmdGpu::ImageType::Color2D:
        case AmdGpu::ImageType::Color2DMsaa:
        case AmdGpu::ImageType::Color2DArray:
        case AmdGpu::ImageType::Cube:
            return ir.CompositeConstruct(read(0), read(8));
        case AmdGpu::ImageType::Color3D:
            return ir.CompositeConstruct(read(0), read(8), read(16));
        default:
            UNREACHABLE();
        }
    }();
    const IR::F32 bias = inst_info.has_bias ? get_addr_reg(addr_reg++) : IR::F32{};
    const IR::F32 dref = inst_info.is_depth ? get_addr_reg(addr_reg++) : IR::F32{};
    const auto [derivatives_dx, derivatives_dy] = [&] -> std::pair<IR::Value, IR::Value> {
        if (!inst_info.has_derivatives) {
            return {};
        }
        switch (view_type) {
        case AmdGpu::ImageType::Color1D:
        case AmdGpu::ImageType::Color1DArray:
            // du/dx, du/dy
            addr_reg = addr_reg + 2;
            return {get_addr_reg(addr_reg - 2), get_addr_reg(addr_reg - 1)};
        case AmdGpu::ImageType::Color2D:
        case AmdGpu::ImageType::Color2DMsaa:
        case AmdGpu::ImageType::Color2DArray:
        case AmdGpu::ImageType::Cube:
            // (du/dx, dv/dx), (du/dy, dv/dy)
            addr_reg = addr_reg + 4;
            return {ir.CompositeConstruct(get_addr_reg(addr_reg - 4), get_addr_reg(addr_reg - 3)),
                    ir.CompositeConstruct(get_addr_reg(addr_reg - 2), get_addr_reg(addr_reg - 1))};
        case AmdGpu::ImageType::Color3D:
            // (du/dx, dv/dx, dw/dx), (du/dy, dv/dy, dw/dy)
            addr_reg = addr_reg + 6;
            return {ir.CompositeConstruct(get_addr_reg(addr_reg - 6), get_addr_reg(addr_reg - 5),
                                          get_addr_reg(addr_reg - 4)),
                    ir.CompositeConstruct(get_addr_reg(addr_reg - 3), get_addr_reg(addr_reg - 2),
                                          get_addr_reg(addr_reg - 1))};
        default:
            UNREACHABLE();
        }
    }();

    const bool is_msaa = view_type == AmdGpu::ImageType::Color2DMsaa ||
                         view_type == AmdGpu::ImageType::Color2DMsaaArray;
    const bool unnormalized = sampler.force_unnormalized || inst_info.is_unnormalized;
    const bool needs_dimentions = (!is_msaa && unnormalized) || (is_msaa && !unnormalized);
    const auto dimensions =
        needs_dimentions ? ir.ImageQueryDimension(handle, ir.Imm32(0u), ir.Imm1(false), inst_info)
                         : IR::Value{};
    const auto get_coord = [&](u32 coord_idx, u32 dim_idx) -> IR::Value {
        const auto coord = get_addr_reg(coord_idx);
        if (is_msaa) {
            // For MSAA images preserve the unnormalized coord or manually unnormalize it
            if (unnormalized) {
                return ir.ConvertFToU(32, coord);
            } else {
                const auto dim =
                    ir.ConvertUToF(32, 32, IR::U32{ir.CompositeExtract(dimensions, dim_idx)});
                return ir.ConvertFToU(32, ir.FPMul(coord, dim));
            }
        }
        const bool array_layer = (view_type == AmdGpu::ImageType::Color2DArray && dim_idx == 2) ||
                                 (view_type == AmdGpu::ImageType::Color1DArray && dim_idx == 1);
        if (unnormalized && !array_layer) {
            // Normalize the coordinate for sampling, dividing by its corresponding dimension.
            const auto dim =
                ir.ConvertUToF(32, 32, IR::U32{ir.CompositeExtract(dimensions, dim_idx)});
            return ir.FPDiv(coord, dim);
        }
        return coord;
    };

    // Now we can load body components as noted in Table 8.9 Image Opcodes with Sampler
    const IR::Value coords = [&] -> IR::Value {
        switch (view_type) {
        case AmdGpu::ImageType::Color1D: // x
            addr_reg = addr_reg + 1;
            return get_coord(addr_reg - 1, 0);
        case AmdGpu::ImageType::Color1DArray: // x, slice
        case AmdGpu::ImageType::Color2D:      // x, y
        case AmdGpu::ImageType::Color2DMsaa:  // x, y
            addr_reg = addr_reg + 2;
            return ir.CompositeConstruct(get_coord(addr_reg - 2, 0), get_coord(addr_reg - 1, 1));
        case AmdGpu::ImageType::Color2DArray: // x, y, slice
        case AmdGpu::ImageType::Cube:         // x, y, face
        case AmdGpu::ImageType::Color3D:      // x, y, z
            addr_reg = addr_reg + 3;
            return ir.CompositeConstruct(get_coord(addr_reg - 3, 0), get_coord(addr_reg - 2, 1),
                                         get_coord(addr_reg - 1, 2));
        default:
            UNREACHABLE();
        }
    }();

    ASSERT(!inst_info.has_lod || !inst_info.has_lod_clamp);
    const bool explicit_lod = inst_info.has_lod || inst_info.force_level0;
    const IR::F32 lod = inst_info.has_lod        ? get_addr_reg(addr_reg++)
                        : inst_info.force_level0 ? ir.Imm32(0.0f)
                                                 : IR::F32{};
    const IR::F32 lod_clamp = inst_info.has_lod_clamp ? get_addr_reg(addr_reg++) : IR::F32{};

    if (profile.native_coherent_images &&
        (image_res.is_written || AmdGpu::IsInteger(image.GetNumberFmt()) ||
         NativeSampler::RequiresManualAddressing(sampler, view_type) ||
         (!inst_info.is_gather && NativeSampler::RequiresManualAnisotropy(sampler)) || (inst_info.is_gather &&
         (inst_info.has_lod || inst_info.has_bias || inst_info.has_lod_clamp ||
          (!inst_info.force_level0 && sampler.mip_filter == AmdGpu::MipFilter::Point &&
           (view_type == AmdGpu::ImageType::Color2D || view_type == AmdGpu::ImageType::Color2DArray)))) ||
         (inst_info.is_depth && (inst_info.has_bias || inst_info.has_derivatives))) && !is_msaa) {
        // D3D12 cannot Sample a UAV or bind an overlapping SRV concurrently.
        // Evaluate filtering with typed loads from the shared mip UAVs.
        // Comparison bias/grad also use typed loads from read-only SRVs:
        // their hardware sample operations require SM6.8, above our SM6.6 target.
        // Explicit-LOD Gather uses the same SRV loads because native Gather
        // has no LOD operand; emitting it would silently discard the guest LOD.
        const auto linear_filter=[](AmdGpu::Filter filter){
            return filter==AmdGpu::Filter::Bilinear||filter==AmdGpu::Filter::AnisoLinear;
        };
        const auto point_filter=[](AmdGpu::Filter filter){
            return filter==AmdGpu::Filter::Point||filter==AmdGpu::Filter::AnisoPoint;
        };
        const bool mag_linear = linear_filter(sampler.xy_mag_filter.Value());
        const bool min_linear = linear_filter(sampler.xy_min_filter.Value());
        const bool anisotropic = AmdGpu::IsAnisoFilter(sampler.xy_mag_filter.Value())||
                                 AmdGpu::IsAnisoFilter(sampler.xy_min_filter.Value());
        const bool integer_sample = !inst_info.is_depth && AmdGpu::IsInteger(image.GetNumberFmt());
        const bool integer_components = integer_sample &&
            image.GetNumberConversion() == AmdGpu::NumberConversion::None;
        const bool one_dimensional = view_type == AmdGpu::ImageType::Color1D ||
                                     view_type == AmdGpu::ImageType::Color1DArray;
        const bool volume = view_type == AmdGpu::ImageType::Color3D;
        const bool cube = view_type == AmdGpu::ImageType::Cube;
        const u32 spatial_dimensions = one_dimensional ? 1 : volume ? 3 : 2;
        const std::array clamps{sampler.clamp_x.Value(), one_dimensional ?
            AmdGpu::ClampMode::ClampLastTexel : sampler.clamp_y.Value(), volume ?
            sampler.clamp_z.Value() : AmdGpu::ClampMode::ClampLastTexel};
        const auto supported_clamp = [](AmdGpu::ClampMode mode) {
            return mode == AmdGpu::ClampMode::ClampLastTexel ||
                   mode == AmdGpu::ClampMode::MirrorOnceLastTexel ||
                   mode == AmdGpu::ClampMode::ClampHalfBorder ||
                   mode == AmdGpu::ClampMode::MirrorOnceHalfBorder ||
                   mode == AmdGpu::ClampMode::MirrorOnceBorder ||
                   mode == AmdGpu::ClampMode::ClampBorder ||
                   mode == AmdGpu::ClampMode::Wrap || mode == AmdGpu::ClampMode::Mirror;
        };
        const bool array_view = view_type == AmdGpu::ImageType::Color2DArray ||
                                view_type == AmdGpu::ImageType::Color1DArray || cube;
        if ((view_type != AmdGpu::ImageType::Color2D && !array_view && !one_dimensional && !volume) ||
            (volume && inst_info.is_depth) ||
            (volume && (sampler.z_filter.Value() < 1 || sampler.z_filter.Value() > 2)) ||
            (!explicit_lod && !inst_info.has_derivatives && info.sw_stage != SwStage::Fragment) ||
            (inst_info.is_gather && (one_dimensional || volume ||
                (sampler.mip_filter == AmdGpu::MipFilter::Linear &&
                 (!inst_info.force_level0 || sampler.MinLod() != 0.f || image.min_lod != 0)))) ||
            sampler.filter_mode.Value() > AmdGpu::FilterMode::Max ||
            !std::ranges::all_of(clamps, supported_clamp) ||
            (!inst_info.is_gather && !mag_linear && !point_filter(sampler.xy_mag_filter.Value())) ||
            (!inst_info.is_gather && !min_linear && !point_filter(sampler.xy_min_filter.Value())) ||
            (anisotropic && sampler.max_aniso.Value() > AmdGpu::AnisoRatio::Sixteen) ||
            (integer_sample && !inst_info.is_gather && !explicit_lod && anisotropic &&
             sampler.max_aniso.Value() != AmdGpu::AnisoRatio::One) ||
            (AmdGpu::IsInteger(image.GetNumberFmt()) && inst_info.is_depth) ||
            (integer_sample && !inst_info.is_gather &&
             (mag_linear || min_linear || (volume && sampler.z_filter.Value() != 1) ||
              sampler.mip_filter == AmdGpu::MipFilter::Linear ||
              sampler.filter_mode != AmdGpu::FilterMode::Blend)) ||
            (integer_sample && (sampler.force_degamma ||
                sampler.border_color_type == AmdGpu::BorderColor::Custom))) {
            throw std::runtime_error("Native manual sampling requires supported dimensions, filtering and shader stage");
        }
        const auto image_only = ir.Imm32(handle.U32() & 0xFFFF);
        auto manual_coords = coords;
        if (cube) {
            const auto face = IR::F32{ir.CompositeExtract(coords, 2)};
            const auto cube_index = ir.FPFloor(ir.FPDiv(face, ir.Imm32(8.f)));
            manual_coords = ir.CompositeConstruct(
                ir.FPSub(IR::F32{ir.CompositeExtract(coords, 0)}, ir.Imm32(1.f)),
                ir.FPSub(IR::F32{ir.CompositeExtract(coords, 1)}, ir.Imm32(1.f)),
                ir.FPSub(face, ir.FPMul(cube_index, ir.Imm32(2.f))));
        }
        IR::U32 array_layer;
        if (array_view) {
            const auto dimensions = ir.ImageQueryDimension(image_only, ir.Imm32(0U), ir.Imm1(false), inst_info);
            const auto count = IR::U32{ir.CompositeExtract(dimensions, spatial_dimensions)};
            const auto last_layer = ir.ConvertUToF(32, 32, ir.ISub(count, ir.Imm32(1U)));
            const auto layer = IR::F32{ir.CompositeExtract(manual_coords, spatial_dimensions)};
            // Array coordinates are view-relative, unnormalized and never
            // filtered across slices. Clamp before integer conversion.
            array_layer = IR::U32{ir.ConvertFToU(32,
                ir.FPRoundEven(ir.FPMin(ir.FPMax(layer, ir.Imm32(0.f)), last_layer)))};
        }
        IR::Value custom_border;
        const auto half_border = [](AmdGpu::ClampMode mode) {
            return mode == AmdGpu::ClampMode::ClampHalfBorder ||
                   mode == AmdGpu::ClampMode::MirrorOnceHalfBorder;
        };
        const auto full_border = [](AmdGpu::ClampMode mode) {
            return mode == AmdGpu::ClampMode::ClampBorder ||
                   mode == AmdGpu::ClampMode::MirrorOnceBorder;
        };
        bool border_addressing = false;
        for (u32 axis = 0; axis < spatial_dimensions; ++axis)
            border_addressing |= full_border(clamps[axis]) ||
                (half_border(clamps[axis]) && (inst_info.is_gather ||
                 (axis == 2 ? sampler.z_filter.Value() == 2 : mag_linear || min_linear)));
        if (border_addressing &&
            sampler.border_color_type == AmdGpu::BorderColor::Custom) {
            info.samplers[handle.U32() >> 16].native_custom_border = true;
            // Per-draw data, indexed by sampler binding rather than guest table index.
            // Changing a table entry must not require shader recompilation.
            auto buffer = std::ranges::find(info.buffers, BufferType::BorderColors,
                                           &BufferResource::buffer_type);
            const u32 binding = static_cast<u32>(buffer - info.buffers.begin());
            if (buffer == info.buffers.end()) {
                info.buffers.push_back(BufferResource{
                    .used_types = IR::Type::F32,
                    .buffer_type = BufferType::BorderColors,
                });
            }
            custom_border = ir.LoadBufferF32(4, ir.Imm32(binding),
                                            ir.Imm32((handle.U32() >> 16) * 4), {});
        }
        const bool anisotropic_footprint = !inst_info.is_gather && !explicit_lod && anisotropic &&
            sampler.max_aniso.Value() != AmdGpu::AnisoRatio::One;
        const u32 maximum_taps = anisotropic_footprint ? u32(sampler.MaxAniso()) : 1;
        IR::F32 tap_count = ir.Imm32(1.f);
        std::array<IR::F32, 3> major_gradient{};
        IR::F32 calculated_lod = lod;
        if (!explicit_lod) {
            const auto dimensions = ir.ImageQueryDimension(image_only, ir.Imm32(0U), ir.Imm1(false), inst_info);
            std::array<IR::F32, 3> normalized_dx{}, normalized_dy{};
            const auto length_squared = [&](const IR::Value& gradient, bool x_direction,
                                            std::array<IR::F32, 3>& normalized) {
                IR::F32 sum = ir.Imm32(0.0f);
                for (u32 axis = 0; axis < spatial_dimensions; ++axis) {
                    const auto coordinate = IR::F32{one_dimensional && !array_view ? manual_coords : ir.CompositeExtract(manual_coords, axis)};
                    IR::F32 component = inst_info.has_derivatives ? IR::F32{one_dimensional ? gradient : ir.CompositeExtract(gradient, axis)} :
                        x_direction ? ir.FPDerivativeX(coordinate) : ir.FPDerivativeY(coordinate);
                    const auto extent = ir.ConvertUToF(32, 32, IR::U32{ir.CompositeExtract(dimensions, axis)});
                    const bool texel_gradient = unnormalized && inst_info.has_derivatives;
                    normalized[axis] = texel_gradient ? IR::F32{ir.FPDiv(component, extent)} : component;
                    if (!texel_gradient)
                        component = IR::F32{ir.FPMul(component, extent)};
                    sum = IR::F32{ir.FPAdd(sum, ir.FPMul(component, component))};
                }
                return sum;
            };
            const auto dx_length = length_squared(derivatives_dx, true, normalized_dx);
            const auto dy_length = length_squared(derivatives_dy, false, normalized_dy);
            const auto footprint = IR::F32{ir.FPMax(dx_length, dy_length)};
            calculated_lod = IR::F32{ir.FPMul(ir.FPLog2(footprint), ir.Imm32(0.5f))};
            if (anisotropic_footprint) {
                const auto major = ir.FPSqrt(footprint);
                const auto minor = ir.FPSqrt(IR::F32{ir.FPMin(dx_length, dy_length)});
                const auto maximum = ir.Imm32(float(maximum_taps));
                const auto ratio = ir.FPDiv(major, ir.FPMax(minor, ir.Imm32(std::numeric_limits<float>::min())));
                const auto positive_minor = ir.FPGreaterThan(minor, ir.Imm32(0.f));
                const auto bounded_ratio = ir.FPMin(ir.FPMax(ratio, ir.Imm32(1.f)), maximum);
                const auto eta = IR::F32{ir.Select(ir.FPGreaterThan(major, ir.Imm32(0.f)),
                    ir.Select(positive_minor, bounded_ratio, maximum), ir.Imm32(1.f))};
                tap_count = IR::F32{ir.FPCeil(eta)};
                calculated_lod = ir.FPLog2(IR::F32{ir.FPDiv(major, eta)});
                const auto x_major = ir.FPGreaterThan(dx_length, dy_length);
                for (u32 axis = 0; axis < spatial_dimensions; ++axis)
                    major_gradient[axis] = IR::F32{ir.Select(x_major, normalized_dx[axis], normalized_dy[axis])};
            }
        }
        IR::F32 effective_bias = ir.Imm32(NativeSampler::ClampLodBias(sampler.LodBias()));
        const auto last_mip = ir.Imm32(u32(image.last_level - image.base_level));
        if (inst_info.has_bias)
            effective_bias = IR::F32{ir.FPMin(ir.FPMax(ir.FPAdd(effective_bias, bias),
                ir.Imm32(NativeSampler::MinLodBias)), ir.Imm32(NativeSampler::MaxLodBias))};
        calculated_lod = IR::F32{ir.FPAdd(calculated_lod, effective_bias)};
        const bool linear_mip = sampler.mip_filter == AmdGpu::MipFilter::Linear;
        auto clamped_lod = ir.FPMax(ir.FPMin(calculated_lod, ir.Imm32(sampler.MaxLod())), ir.Imm32(sampler.MinLod()));
        if (sampler.mip_filter == AmdGpu::MipFilter::None)
            clamped_lod = ir.Imm32(0.0f);
        const float resource_minimum = std::max(0.0f,
            std::min(float(image.min_lod) / 256.0f, float(image.last_level)) - float(image.base_level));
        clamped_lod = ir.FPMax(clamped_lod, ir.Imm32(resource_minimum));
        if (inst_info.has_lod_clamp)
            clamped_lod = ir.FPMax(clamped_lod, lod_clamp);
        clamped_lod = ir.FPMin(ir.FPMax(clamped_lod, ir.Imm32(0.0f)), ir.ConvertUToF(32, 32, last_mip));
        // LZ supplies zero before sampler/resource clamps. It must not bypass
        // the resource minimum, including a fractional minimum between mips.
        const auto mip_floor = ir.FPFloor(linear_mip ? clamped_lod : ir.FPAdd(clamped_lod, ir.Imm32(0.5f)));
        const auto first_mip = IR::U32{ir.ConvertFToU(32, mip_floor)};
        const auto combine = [&](IR::F32 a, IR::F32 b, IR::F32 weight) -> IR::F32 {
            if (inst_info.is_depth || sampler.filter_mode == AmdGpu::FilterMode::Blend)
                return IR::F32{ir.FPAdd(a, ir.FPMul(ir.FPSub(b, a), weight))};
            const auto reduced = sampler.filter_mode == AmdGpu::FilterMode::Min ?
                ir.FPMin(a, b) : ir.FPMax(a, b);
            // Reduction filters exclude taps with zero interpolation weight.
            return IR::F32{ir.Select(ir.FPLessThanEqual(weight, ir.Imm32(0.0f)), a,
                ir.Select(ir.FPLessThanEqual(ir.Imm32(1.0f), weight), b, reduced))};
        };
        const auto sample_level = [&](IR::U32 mip, bool linear, const IR::Value& sample_coords) {
        const bool linear_z = volume && sampler.z_filter.Value() == 2;
        const auto size = ir.ImageQueryDimension(image_only, mip, ir.Imm1(false), inst_info);
        std::array<IR::U32, 3> base{}, extent{};
        std::array<IR::F32, 3> weight{};
        for (u32 axis = 0; axis < spatial_dimensions; ++axis) {
            extent[axis] = IR::U32{ir.CompositeExtract(size, axis)};
            auto coordinate = IR::F32{one_dimensional && !array_view ? sample_coords : ir.CompositeExtract(sample_coords, axis)};
            auto position = ir.FPMul(coordinate,
                                     ir.ConvertUToF(32, 32, extent[axis]));
            if (!offset.IsEmpty())
                position = ir.FPAdd(position, ir.ConvertSToF(32, 32, one_dimensional ? offset : ir.CompositeExtract(offset, axis)));
            // Address offsets participate in mirror folding in texel space.
            if (clamps[axis] == AmdGpu::ClampMode::MirrorOnceLastTexel ||
                clamps[axis] == AmdGpu::ClampMode::MirrorOnceHalfBorder ||
                clamps[axis] == AmdGpu::ClampMode::MirrorOnceBorder)
                position = ir.FPAbs(position);
            const bool footprint = inst_info.is_gather || (axis == 2 ? linear_z : linear);
            if (half_border(clamps[axis]))
                position = ir.FPMin(ir.FPMax(position, ir.Imm32(0.f)),
                    ir.ConvertUToF(32, 32, footprint ? extent[axis] : IR::U32{ir.ISub(extent[axis], ir.Imm32(1U))}));
            if (footprint) position = ir.FPSub(position, ir.Imm32(0.5f));
            if (clamps[axis] == AmdGpu::ClampMode::ClampLastTexel ||
                clamps[axis] == AmdGpu::ClampMode::MirrorOnceLastTexel ||
                half_border(clamps[axis]) || full_border(clamps[axis])) {
                // Bound conversion while preserving the edge filter footprint.
                position = ir.FPMin(ir.FPMax(position, ir.Imm32(-1.0f)),
                                    ir.ConvertUToF(32, 32, extent[axis]));
            } else {
                const auto period = ir.ConvertUToF(32, 32,
                    clamps[axis] == AmdGpu::ClampMode::Mirror ?
                        IR::U32{ir.IAdd(extent[axis], extent[axis])} : extent[axis]);
                position = ir.FPSub(position, ir.FPMul(ir.FPFloor(ir.FPDiv(position, period)), period));
            }
            const auto rounded = ir.FPFloor(position);
            weight[axis] = IR::F32{ir.FPSub(position, rounded)};
            base[axis] = IR::U32{ir.ConvertFToI(32, true, rounded)};
        }
        const auto load = [&](u32 x, u32 y, u32 z = 0) {
            std::array<IR::U32, 3> address{};
            auto inside = ir.Imm1(true);
            for (u32 axis = 0; axis < spatial_dimensions; ++axis) {
                const auto index = ir.IAdd(base[axis], ir.Imm32(axis == 0 ? x : axis == 1 ? y : z));
                if (full_border(clamps[axis]) || (half_border(clamps[axis]) &&
                    (inst_info.is_gather || (axis == 2 ? linear_z : linear))))
                    inside = ir.LogicalAnd(inside, ir.ILessThan(index, extent[axis], false));
                if (clamps[axis] == AmdGpu::ClampMode::ClampLastTexel ||
                    clamps[axis] == AmdGpu::ClampMode::MirrorOnceLastTexel ||
                    half_border(clamps[axis]) || full_border(clamps[axis])) {
                    address[axis] = ir.SMin(ir.SMax(index, ir.Imm32(0U)),
                                           ir.ISub(extent[axis], ir.Imm32(1U)));
                } else if (clamps[axis] == AmdGpu::ClampMode::Wrap) {
                    address[axis] = ir.IMod(index, extent[axis]);
                } else {
                    const auto period = ir.IAdd(extent[axis], extent[axis]);
                    const auto wrapped = ir.IMod(index, period);
                    address[axis] = ir.UMin(wrapped, ir.ISub(ir.ISub(period, ir.Imm32(1U)), wrapped));
                }
            }
            const IR::Value texel_coords = one_dimensional ?
                (array_view ? ir.CompositeConstruct(address[0], array_layer) : IR::Value{address[0]}) :
                (volume ? ir.CompositeConstruct(address[0], address[1], address[2]) :
                 array_view ? ir.CompositeConstruct(address[0], address[1], array_layer) :
                              ir.CompositeConstruct(address[0], address[1]));
            auto value = ir.ImageRead(image_only, texel_coords,
                                      !image_res.is_written || image_res.mip_fallback_mode == MipStorageFallbackMode::DynamicIndex ? mip : IR::U32{},
                                      {}, inst_info);
            value = ApplyReadNumberConversionVec4(ir, value, image.GetNumberConversion());
            if (image_res.is_written) value = ApplySwizzle(ir, value, image.DstSelect(), integer_components);
            if (sampler.force_degamma && image.GetNumberFmt() != AmdGpu::NumberFormat::Srgb)
                value = ApplyForceDegamma(ir, value);
            if (border_addressing) {
                std::array<IR::F32, 4> channels{};
                for (u32 channel = 0; channel < 4; ++channel) {
                    const bool one = sampler.border_color_type == AmdGpu::BorderColor::White ||
                        (channel == 3 && sampler.border_color_type == AmdGpu::BorderColor::OpaqueBlack);
                    const auto border = custom_border.IsEmpty() ? (integer_components ?
                        IR::Value{ir.BitCast<IR::F32>(ir.Imm32(one ? 1U : 0U))} :
                        IR::Value{ir.Imm32(one ? 1.0f : 0.0f)}) :
                        ir.CompositeExtract(custom_border, channel);
                    channels[channel] = IR::F32{ir.Select(inside, ir.CompositeExtract(value, channel), border)};
                }
                value = ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
            }
            if (inst_info.is_depth) {
                const auto depth = IR::F32{ir.CompositeExtract(value, 0)};
                const auto passes = [&]() -> IR::U1 {
                    switch (sampler.depth_compare_func.Value()) {
                    case AmdGpu::DepthCompare::Never: return ir.Imm1(false);
                    case AmdGpu::DepthCompare::Less: return ir.FPLessThan(dref, depth);
                    case AmdGpu::DepthCompare::Equal: return ir.FPEqual(dref, depth);
                    case AmdGpu::DepthCompare::LessEqual: return ir.FPLessThanEqual(dref, depth);
                    case AmdGpu::DepthCompare::Greater: return ir.FPGreaterThan(dref, depth);
                    case AmdGpu::DepthCompare::NotEqual: return ir.FPNotEqual(dref, depth, false);
                    case AmdGpu::DepthCompare::GreaterEqual: return ir.FPGreaterThanEqual(dref, depth);
                    case AmdGpu::DepthCompare::Always: return ir.Imm1(true);
                    }
                    UNREACHABLE();
                }();
                // Percentage-closer filtering compares individual taps before
                // interpolation; filtering raw depths first is not equivalent.
                value = ir.CompositeConstruct(IR::F32{ir.Select(passes, ir.Imm32(1.f), ir.Imm32(0.f))},
                    ir.Imm32(0.f), ir.Imm32(0.f), ir.Imm32(0.f));
            }
            return value;
        };
        if (inst_info.is_gather) {
            // Gather preserves all four taps, including zero-weight neighbors.
            // XYZW order is lower-left, lower-right, upper-right, upper-left.
            const u32 component = inst_info.is_depth ? 0 : inst_info.gather_comp.Value();
            return ir.CompositeConstruct(ir.CompositeExtract(load(0, 1), component),
                ir.CompositeExtract(load(1, 1), component),
                ir.CompositeExtract(load(1, 0), component),
                ir.CompositeExtract(load(0, 0), component));
        }
        auto result = load(0, 0);
        if (linear && one_dimensional) {
            const auto next = load(1, 0);
            std::array<IR::F32, 4> channels{};
            for (u32 channel = 0; channel < 4; ++channel)
                channels[channel] = combine(IR::F32{ir.CompositeExtract(result, channel)},
                    IR::F32{ir.CompositeExtract(next, channel)}, weight[0]);
            return ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
        }
        {
            const auto plane = [&](u32 z, IR::Value a) {
            if (!linear) return a;
            const auto b = load(1, 0, z), c = load(0, 1, z), d = load(1, 1, z);
            std::array<IR::F32, 4> channels{};
            const auto mix = [&](IR::F32 a, IR::F32 b, IR::F32 w) {
                return combine(a, b, w);
            };
            for (u32 channel = 0; channel < 4; ++channel)
                channels[channel] = IR::F32{mix(
                    IR::F32{mix(IR::F32{ir.CompositeExtract(a, channel)}, IR::F32{ir.CompositeExtract(b, channel)}, weight[0])},
                    IR::F32{mix(IR::F32{ir.CompositeExtract(c, channel)}, IR::F32{ir.CompositeExtract(d, channel)}, weight[0])}, weight[1])};
            return ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
            };
            result = plane(0, result);
            if (linear_z) {
                const auto next = plane(1, load(0, 0, 1));
                std::array<IR::F32, 4> channels{};
                for (u32 channel = 0; channel < 4; ++channel)
                    channels[channel] = combine(IR::F32{ir.CompositeExtract(result, channel)},
                        IR::F32{ir.CompositeExtract(next, channel)}, weight[2]);
                result = ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
            }
        }
        return result;
        };
        const auto sample_filtered = [&](IR::U32 mip, const IR::Value& sample_coords) {
            const auto magnified = sample_level(mip, mag_linear, sample_coords);
            if (mag_linear == min_linear) return magnified;
            const auto minified = sample_level(mip, min_linear, sample_coords);
            const auto magnifying = ir.FPLessThanEqual(clamped_lod, ir.Imm32(0.0f));
            std::array<IR::F32, 4> channels{};
            for (u32 channel = 0; channel < 4; ++channel)
                channels[channel] = IR::F32{ir.Select(magnifying,
                    ir.CompositeExtract(magnified, channel), ir.CompositeExtract(minified, channel))};
            return ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
        };
        if (inst_info.is_gather) {
            inst.ReplaceUsesWith(sample_level(first_mip, false, manual_coords));
            return;
        }
        const auto sample_mips = [&](const IR::Value& sample_coords) {
        auto result = sample_filtered(first_mip, sample_coords);
        if (linear_mip) {
            const auto next_mip = ir.UMin(ir.IAdd(first_mip, ir.Imm32(1U)), last_mip);
            const auto next = sample_filtered(next_mip, sample_coords);
            const auto fraction = ir.FPSub(clamped_lod, mip_floor);
            std::array<IR::F32, 4> channels{};
            for (u32 channel = 0; channel < 4; ++channel) {
                const auto a = IR::F32{ir.CompositeExtract(result, channel)};
                const auto b = IR::F32{ir.CompositeExtract(next, channel)};
                channels[channel] = combine(a, b, IR::F32{fraction});
            }
            result = ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
        }
        return result;
        };
        const auto tap_coordinates = [&](u32 index) {
            if (!anisotropic_footprint) return manual_coords;
            const auto ordinal = ir.FPMin(ir.Imm32(float(index + 1)), tap_count);
            const auto distance = ir.FPSub(ir.FPDiv(ordinal, ir.FPAdd(tap_count, ir.Imm32(1.f))), ir.Imm32(0.5f));
            auto shifted = manual_coords;
            for (u32 axis = 0; axis < spatial_dimensions; ++axis) {
                const auto coordinate = IR::F32{one_dimensional && !array_view ? manual_coords : ir.CompositeExtract(manual_coords, axis)};
                const auto displaced = ir.FPAdd(coordinate, ir.FPMul(major_gradient[axis], distance));
                shifted = one_dimensional && !array_view ? IR::Value{displaced} : ir.CompositeInsert(shifted, displaced, axis);
            }
            return shifted;
        };
        auto result = sample_mips(tap_coordinates(0));
        for (u32 tap = 1; tap < maximum_taps; ++tap) {
            const auto next = sample_mips(tap_coordinates(tap));
            const auto active = ir.FPLessThan(ir.Imm32(float(tap)), tap_count);
            std::array<IR::F32, 4> channels{};
            for (u32 channel = 0; channel < 4; ++channel) {
                const auto reduced = combine(IR::F32{ir.CompositeExtract(result, channel)},
                    IR::F32{ir.CompositeExtract(next, channel)}, ir.Imm32(1.f / float(tap + 1)));
                channels[channel] = IR::F32{ir.Select(active, reduced, ir.CompositeExtract(result, channel))};
            }
            result = ir.CompositeConstruct(channels[0], channels[1], channels[2], channels[3]);
        }
        inst.ReplaceUsesWith(result);
        return;
    }

    auto texel = [&] -> IR::Value {
        if (is_msaa) {
            return ir.ImageRead(handle, coords, {}, ir.Imm32(0U), inst_info);
        }
        if (inst_info.is_gather) {
            if (inst_info.is_depth) {
                return ir.ImageGatherDref(handle, coords, offset, dref, inst_info);
            }
            return ir.ImageGather(handle, coords, offset, inst_info);
        }
        if (inst_info.has_derivatives) {
            return ir.ImageGradient(handle, coords, derivatives_dx, derivatives_dy, offset,
                                    lod_clamp, inst_info);
        }
        if (inst_info.is_depth) {
            if (explicit_lod) {
                return ir.ImageSampleDrefExplicitLod(handle, coords, dref, lod, offset, inst_info);
            }
            return ir.ImageSampleDrefImplicitLod(handle, coords, dref, bias, offset, inst_info, lod_clamp);
        }
        if (explicit_lod) {
            return ir.ImageSampleExplicitLod(handle, coords, lod, offset, inst_info);
        }
        return ir.ImageSampleImplicitLod(handle, coords, bias, offset, inst_info, lod_clamp);
    }();

    auto converted = ApplyReadNumberConversionVec4(ir, texel, image.GetNumberConversion());
    if (sampler.force_degamma && image.GetNumberFmt() != AmdGpu::NumberFormat::Srgb) {
        converted = ApplyForceDegamma(ir, texel);
    }
    inst.ReplaceUsesWith(converted);
}

void PatchImageArgs(IR::Inst& inst, Info& info, const Profile& profile) {
    // Nothing to patch for dimension query.
    if (inst.GetOpcode() == IR::Opcode::ImageQueryDimensions) {
        return;
    }

    const auto image_handle = inst.Arg(0);
    const auto binding_index = image_handle.U32() & 0xFFFF;
    const auto& image_res = info.images[binding_index];
    auto image = image_res.GetSharp(info);

    // Sample instructions must be handled separately using address register data.
    if (inst.GetOpcode() == IR::Opcode::ImageSampleRaw) {
        return PatchImageSampleArgs(inst, info, image_res, image, profile);
    }

    IR::IREmitter ir{*inst.GetParent(), IR::Block::InstructionList::s_iterator_to(inst)};
    auto inst_info = inst.Flags<IR::TextureInstInfo>();
    const auto view_type = image.GetViewType(image_res.is_array);

    // Now that we know the image type, adjust texture coordinate vector.
    IR::Inst* body = inst.Arg(1).Inst();
    const auto [coords, arg] = [&] -> std::pair<IR::Value, IR::Value> {
        switch (view_type) {
        case AmdGpu::ImageType::Color1D: // x, [lod]
            return {body->Arg(0), body->Arg(1)};
        case AmdGpu::ImageType::Color1DArray: // x, slice, [lod]
        case AmdGpu::ImageType::Color2D:      // x, y, [lod]
            return {ir.CompositeConstruct(body->Arg(0), body->Arg(1)), body->Arg(2)};
        case AmdGpu::ImageType::Color2DMsaa: // x, y. (sample is passed on different argument)
        {
            auto skip_lod = false;
            if (inst_info.has_lod) {
                const auto mipid = body->Arg(2);
                if (mipid.IsImmediate() && mipid.U32() == 0) {
                    // if image_x_mip refers to a MSAA image, and mipid is 0, it is safe to be
                    // skipped and fragid is taken from the next arg
                    LOG_WARNING(Render_Recompiler, "Encountered a _mip instruction with MSAA "
                                                   "image, and mipid is 0, skipping LoD");
                    inst_info.has_lod.Assign(false);
                    skip_lod = true;
                } else {
                    UNREACHABLE_MSG(
                        "Encountered a _mip instruction with MSAA image, and mipid is non-zero");
                }
            }
            return {ir.CompositeConstruct(body->Arg(0), body->Arg(1)), body->Arg(skip_lod ? 3 : 2)};
        }
        case AmdGpu::ImageType::Color2DArray:     // x, y, slice, [lod]
        case AmdGpu::ImageType::Color2DMsaaArray: // x, y, slice. (sample is passed on different
                                                  // argument)
        case AmdGpu::ImageType::Cube:             // x, y, face, [lod]
        case AmdGpu::ImageType::Color3D:          // x, y, z, [lod]
            return {ir.CompositeConstruct(body->Arg(0), body->Arg(1), body->Arg(2)), body->Arg(3)};
        default:
            UNREACHABLE_MSG("Unknown image type {}", view_type);
        }
    }();

    const auto has_ms = view_type == AmdGpu::ImageType::Color2DMsaa ||
                        view_type == AmdGpu::ImageType::Color2DMsaaArray;
    ASSERT(!inst_info.has_lod || !has_ms);
    // If we are binding a single mip level as fallback, drop the argument
    const auto lod =
        (inst_info.has_lod && image_res.mip_fallback_mode != MipStorageFallbackMode::ConstantIndex)
            ? IR::U32{arg}
            : IR::U32{};
    const auto ms = has_ms ? IR::U32{arg} : IR::U32{};

    const auto is_storage = image_res.is_written;
    if (inst.GetOpcode() == IR::Opcode::ImageRead) {
        auto texel = ir.ImageRead(image_handle, coords, lod, ms, inst_info);
        const auto conversion = image.GetNumberConversion();
        const bool converted_storage = profile.native_coherent_images && is_storage &&
                                       conversion != AmdGpu::NumberConversion::None;
        // Convert fetched components first. A forced ZERO/ONE is a value in
        // the guest result domain and must not itself be normalized or biased.
        if (converted_storage) texel = ApplyReadNumberConversionVec4(ir, texel, conversion);
        if (is_storage) {
            // Storage image requires shader swizzle.
            texel = ApplySwizzle(ir, texel, image.DstSelect(),
                profile.native_coherent_images && !converted_storage &&
                AmdGpu::IsInteger(image.GetNumberFmt()));
        }
        const auto converted = converted_storage ? texel : ApplyReadNumberConversionVec4(ir, texel, conversion);
        inst.ReplaceUsesWith(converted);
    } else {
        inst.SetArg(1, coords);
        if (inst.GetOpcode() == IR::Opcode::ImageWrite) {
            inst.SetArg(2, lod);
            inst.SetArg(3, ms);

            auto texel = inst.Arg(4);
            if (is_storage) {
                // Storage image requires shader swizzle.
                texel = ApplySwizzle(ir, texel, image.DstSelect().Inverse(),
                    profile.native_coherent_images && AmdGpu::IsInteger(image.GetNumberFmt()) &&
                    image.GetNumberConversion() == AmdGpu::NumberConversion::None);
            }
            const auto converted =
                ApplyWriteNumberConversionVec4(ir, texel, image.GetNumberConversion());
            inst.SetArg(4, converted);
        }
    }
}

void ResourcePatchingPass(Shader::Info& info, const ResourceDiscoveryList& resources,
                          const Profile& profile) {
    // Iterate over discovered resources and patch them after finding the sharp.
    // Pass 1: Track resource sharps
    Descriptors descriptors{info};
    const auto uses_raw32 = [&](const ResourceDiscovery& usage) {
        if(!profile.native_coherent_images)return false;
        const auto flags=usage.user->Flags<IR::TextureInstInfo>();
        if(flags.is_depth)return false;
        const ImageResource source{.sharp_fetch=ConstructSharpFetch<AmdGpu::Image>(usage.sharps[0]),
            .is_array=bool(flags.is_array),.is_r128=bool(flags.is_r128),.post_op=usage.sharps[0].post_op};
        const auto format=source.GetSharp(info).GetDataFmt();
        if(u32(format)!=4&&u32(format)!=10)return false;
        for(const auto& other:resources){
            if(!IsImageInstruction(*other.user))continue;
            const auto other_flags=other.user->Flags<IR::TextureInstInfo>();
            if(other_flags.is_depth)continue;
            const ImageResource candidate{.sharp_fetch=ConstructSharpFetch<AmdGpu::Image>(other.sharps[0]),
                .is_array=bool(other_flags.is_array),.is_r128=bool(other_flags.is_r128),.post_op=other.sharps[0].post_op};
            if(candidate.GetSharp(info).GetDataFmt()!=format&&NativeImageViewAliases(source,candidate,info))return true;
        }
        return std::ranges::any_of(info.native_pipeline_raw32_views,[&](const auto& view){
            return !view.is_depth&&view.sharp.GetDataFmt()!=format&&NativeImageViewAliases(source,view,info);
        });
    };
    const auto uses_raw_packed10 = [&](const ResourceDiscovery& usage) {
        if(!profile.native_coherent_images)return false;
        const auto flags=usage.user->Flags<IR::TextureInstInfo>();
        const ImageResource source{.sharp_fetch=ConstructSharpFetch<AmdGpu::Image>(usage.sharps[0]),
            .is_depth=bool(flags.is_depth),.is_array=bool(flags.is_array),.is_r128=bool(flags.is_r128),
            .post_op=usage.sharps[0].post_op};
        const auto sharp=source.GetSharp(info);
        if(u32(sharp.GetDataFmt())!=9)return false;
        const auto number=sharp.GetNumberFmt();
        const auto signed_packed=[](AmdGpu::Image image){return u32(image.GetDataFmt())==9&&
            (image.GetNumberFmt()==AmdGpu::NumberFormat::Snorm||image.GetNumberFmt()==AmdGpu::NumberFormat::Sint);};
        if(signed_packed(sharp))return true;
        if(number!=AmdGpu::NumberFormat::Unorm&&number!=AmdGpu::NumberFormat::Uint)return false;
        for(const auto& other:resources){
            if(!IsImageInstruction(*other.user))continue;
            const auto other_flags=other.user->Flags<IR::TextureInstInfo>();
            const ImageResource alias{.sharp_fetch=ConstructSharpFetch<AmdGpu::Image>(other.sharps[0]),
                .is_depth=bool(other_flags.is_depth),.is_array=bool(other_flags.is_array),.is_r128=bool(other_flags.is_r128),
                .post_op=other.sharps[0].post_op};
            if(signed_packed(alias.GetSharp(info))&&NativeImageViewAliases(source,alias,info))return true;
        }
        for(const auto& write:info.native_pipeline_writes)
            if(signed_packed(write.sharp)&&NativeImageViewAliases(source,write,info))return true;
        for(const auto& view:info.native_pipeline_signed_packed_views)
            if(signed_packed(view.sharp)&&NativeImageViewAliases(source,view,info))return true;
        return false;
    };
    const auto reads_written_image = [&](const ResourceDiscovery& usage) {
        const auto opcode = usage.user->GetOpcode();
        const bool query = opcode == IR::Opcode::ImageQueryDimensions;
        if (!profile.native_coherent_images ||
            (opcode != IR::Opcode::ImageRead && opcode != IR::Opcode::ImageSampleRaw && !query))
            return std::pair{false, false};
        const auto fetch = ConstructSharpFetch<AmdGpu::Image>(usage.sharps[0]);
        const auto flags = usage.user->Flags<IR::TextureInstInfo>();
        const ImageResource source{.sharp_fetch=fetch,.is_depth=bool(flags.is_depth),
            .is_array=bool(flags.is_array),.is_r128=bool(flags.is_r128),.post_op=usage.sharps[0].post_op};
        bool storage = false, original_storage = false;
        for (const auto& other : resources) {
            if (other.user->GetOpcode() != IR::Opcode::ImageWrite &&
                !IsImageAtomicInstruction(*other.user)) continue;
            const auto other_flags = other.user->Flags<IR::TextureInstInfo>();
            const auto other_fetch = ConstructSharpFetch<AmdGpu::Image>(other.sharps[0]);
            const ImageResource destination{.sharp_fetch=other_fetch,.is_depth=bool(other_flags.is_depth),
                .is_array=bool(other_flags.is_array),.is_r128=bool(other_flags.is_r128),.post_op=other.sharps[0].post_op};
            const bool same_source = flags.is_array == other_flags.is_array && flags.is_r128 == other_flags.is_r128 &&
                   flags.is_depth == other_flags.is_depth &&
                   usage.sharps[0].post_op == other.sharps[0].post_op &&
                   fetch == other_fetch;
            original_storage |= same_source;
            storage |= same_source || NativeImageViewAliases(source, destination, info);
        }
        for (const auto& write : info.native_pipeline_writes)
            storage |= NativeImageViewAliases(source, write, info);
        return std::pair{storage, !query && storage &&
            (!original_storage || opcode == IR::Opcode::ImageSampleRaw) &&
            source.GetSharp(info).GetNumberFmt() == AmdGpu::NumberFormat::Srgb};
    };
    for (const auto& usage : resources) {
        IR::Inst& inst = *usage.user;
        if (IsBufferInstruction(inst)) {
            PatchBufferSharp(usage, info, descriptors, profile);
        } else if (IsImageInstruction(inst)) {
            const auto [storage, srgb] = reads_written_image(usage);
            PatchImageSharp(usage, info, descriptors, profile, storage, srgb, uses_raw_packed10(usage), uses_raw32(usage));
        }
    }

    // Pass 2: Patch instruction args
    for (const auto& usage : resources) {
        IR::Inst& inst = *usage.user;
        if (IsBufferInstruction(inst)) {
            PatchBufferArgs(inst, info);
        } else if (IsImageInstruction(inst)) {
            PatchImageArgs(inst, info, profile);
        } else if (IsDataRingInstruction(inst)) {
            PatchGlobalDataShareAccess(inst, info, descriptors, profile);
        }
    }
}

} // namespace Shader::Optimization
