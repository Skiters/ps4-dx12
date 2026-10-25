// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/emulator_settings.h"
#include "shader_recompiler/ir/program.h"
#include "shader_recompiler/profile.h"
#include <unordered_map>

namespace Shader::Optimization {

static void AddFlatbuf(Info& info) {
    if (!info.has_flatbuf) {
        info.buffers.push_back({
            .used_types = IR::Type::U32,
            .buffer_type = BufferType::Flatbuf,
        });
        info.has_flatbuf = true;
    }
}

void Visit(Info& info, const IR::Inst& inst) {
    switch (inst.GetOpcode()) {
    case IR::Opcode::GetAttribute:
    case IR::Opcode::GetAttributeU1:
    case IR::Opcode::GetAttributeU32:
        info.loads.Set(inst.Arg(0).Attribute(), inst.Arg(1).U32());
        break;
    case IR::Opcode::SetAttribute:
        info.stores.Set(inst.Arg(0).Attribute(), inst.Arg(2).U32());
        break;
    case IR::Opcode::GetUserData:
        AddFlatbuf(info);
        break;
    case IR::Opcode::SetPatch: {
        const auto patch = inst.Arg(0).Patch();
        if(patch<=IR::Patch::TessellationLodInteriorV){
            const auto value=inst.Arg(1);
            info.tessellation_bounds.Record(u32(patch),value.IsImmediate()&&value.Type()==IR::Type::F32?
                std::optional<float>(value.F32()):std::nullopt);
        }
        if (patch <= IR::Patch::TessellationLodBottom) {
            info.stores_tess_level_outer = true;
        } else if (patch <= IR::Patch::TessellationLodInteriorV) {
            info.stores_tess_level_inner = true;
        } else {
            info.uses_patches |= 1U << IR::GenericPatchIndex(patch);
        }
        break;
    }
    case IR::Opcode::GetPatch: {
        const auto patch = inst.Arg(0).Patch();
        info.uses_patches |= 1U << IR::GenericPatchIndex(patch);
        break;
    }
    case IR::Opcode::LoadSharedU16:
    case IR::Opcode::WriteSharedU16:
        info.shared_types |= IR::Type::U16;
        break;
    case IR::Opcode::LoadSharedU32:
    case IR::Opcode::WriteSharedU32:
    case IR::Opcode::SharedAtomicIAdd32:
    case IR::Opcode::SharedAtomicISub32:
    case IR::Opcode::SharedAtomicSMin32:
    case IR::Opcode::SharedAtomicUMin32:
    case IR::Opcode::SharedAtomicSMax32:
    case IR::Opcode::SharedAtomicUMax32:
    case IR::Opcode::SharedAtomicInc32:
    case IR::Opcode::SharedAtomicDec32:
    case IR::Opcode::SharedAtomicAnd32:
    case IR::Opcode::SharedAtomicOr32:
    case IR::Opcode::SharedAtomicXor32:
    case IR::Opcode::SharedAtomicCmpSwap32:
        info.shared_types |= IR::Type::U32;
        break;
    case IR::Opcode::SharedAtomicIAdd64:
    case IR::Opcode::SharedAtomicISub64:
    case IR::Opcode::SharedAtomicSMin64:
    case IR::Opcode::SharedAtomicUMin64:
    case IR::Opcode::SharedAtomicSMax64:
    case IR::Opcode::SharedAtomicUMax64:
    case IR::Opcode::SharedAtomicInc64:
    case IR::Opcode::SharedAtomicDec64:
    case IR::Opcode::SharedAtomicAnd64:
    case IR::Opcode::SharedAtomicOr64:
    case IR::Opcode::SharedAtomicXor64:
    case IR::Opcode::SharedAtomicCmpSwap64:
        info.uses_shared_int64_atomics = true;
        [[fallthrough]];
    case IR::Opcode::LoadSharedU64:
    case IR::Opcode::WriteSharedU64:
        info.shared_types |= IR::Type::U64;
        break;
    case IR::Opcode::ConvertF16F32:
    case IR::Opcode::ConvertF32F16:
    case IR::Opcode::BitCastU16F16:
    case IR::Opcode::BitCastF16U16:
        info.uses_fp16 = true;
        break;
    case IR::Opcode::PackDouble2x32:
    case IR::Opcode::UnpackDouble2x32:
    case IR::Opcode::FPAbs64:
    case IR::Opcode::FPAdd64:
    case IR::Opcode::FPFma64:
    case IR::Opcode::FPMax64:
    case IR::Opcode::FPMin64:
    case IR::Opcode::FPMul64:
    case IR::Opcode::FPDiv64:
    case IR::Opcode::FPNeg64:
    case IR::Opcode::FPRecip64:
    case IR::Opcode::FPRecipSqrt64:
    case IR::Opcode::FPSaturate64:
    case IR::Opcode::FPClamp64:
    case IR::Opcode::FPRoundEven64:
    case IR::Opcode::FPFloor64:
    case IR::Opcode::FPCeil64:
    case IR::Opcode::FPTrunc64:
    case IR::Opcode::FPFract64:
    case IR::Opcode::FPFrexpSig64:
    case IR::Opcode::FPFrexpExp64:
    case IR::Opcode::FPOrdEqual64:
    case IR::Opcode::FPUnordEqual64:
    case IR::Opcode::FPOrdNotEqual64:
    case IR::Opcode::FPUnordNotEqual64:
    case IR::Opcode::FPOrdLessThan64:
    case IR::Opcode::FPUnordLessThan64:
    case IR::Opcode::FPOrdGreaterThan64:
    case IR::Opcode::FPUnordGreaterThan64:
    case IR::Opcode::FPOrdLessThanEqual64:
    case IR::Opcode::FPUnordLessThanEqual64:
    case IR::Opcode::FPOrdGreaterThanEqual64:
    case IR::Opcode::FPUnordGreaterThanEqual64:
    case IR::Opcode::FPIsNan64:
    case IR::Opcode::FPIsInf64:
    case IR::Opcode::ConvertS32F64:
    case IR::Opcode::ConvertF32F64:
    case IR::Opcode::ConvertF64F32:
    case IR::Opcode::ConvertF64S32:
    case IR::Opcode::ConvertF64U32:
        info.uses_fp64 = true;
        break;
    case IR::Opcode::ImageWrite:
        info.has_storage_images = true;
        break;
    case IR::Opcode::QuadBroadcast:
        info.uses_group_quad = true;
        break;
    case IR::Opcode::Shuffle:
    case IR::Opcode::ShuffleXor:
        info.uses_group_shuffle = true;
        break;
    case IR::Opcode::ReadLane:
    case IR::Opcode::ReadFirstLane:
    case IR::Opcode::WriteLane:
        info.uses_group_ballot = true;
        break;
    case IR::Opcode::Discard:
    case IR::Opcode::DiscardCond:
        info.has_discard = true;
        break;
    case IR::Opcode::BitwiseXor32:
        info.has_bitwise_xor = true;
        break;
    case IR::Opcode::ImageGather:
    case IR::Opcode::ImageGatherDref:
        info.has_image_gather = true;
        break;
    case IR::Opcode::ImageQueryDimensions:
    case IR::Opcode::ImageQueryLod:
        info.has_image_query = true;
        break;
    case IR::Opcode::ImageAtomicFMax32:
    case IR::Opcode::ImageAtomicFMin32:
        info.uses_image_atomic_float_min_max = true;
        break;
    case IR::Opcode::BufferAtomicFMax32:
    case IR::Opcode::BufferAtomicFMin32:
        info.uses_buffer_atomic_float_min_max = true;
        break;
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
        info.uses_buffer_int64_atomics = true;
        break;
    case IR::Opcode::DataAppend:
    case IR::Opcode::DataConsume:
    case IR::Opcode::Ballot:
    case IR::Opcode::InverseBallot:
    case IR::Opcode::BallotFindLsb:
        info.uses_group_ballot = true;
        [[fallthrough]];
    case IR::Opcode::LaneId:
        info.uses_lane_id = true;
        break;
    case IR::Opcode::Memtime:
        info.uses_shader_clock = true;
        break;
    case IR::Opcode::ReadConst:
        AddFlatbuf(info);
        if (inst.Flags<u32>() == 0) {
            info.readconst_types |= Info::ReadConstType::Immediate;
            info.readconst_types |= Info::ReadConstType::Dynamic;
            info.uses_dma = true;
        }
        break;
    case IR::Opcode::PackUfloat10_11_11:
        info.uses_pack_10_11_11 = true;
        break;
    case IR::Opcode::UnpackUfloat10_11_11:
        info.uses_unpack_10_11_11 = true;
        break;
    default:
        break;
    }
}

// Must-write dataflow on the structured CFG. Start at the lattice top and meet
// predecessor outputs; the entry boundary is empty even if it has a back edge.
// This proves writes on all terminating paths, including loop bypasses and early
// returns. It never uses a loop body that might not execute as an initial value.
static u8 GuaranteedTessellationFactors(const IR::Program& program) {
    const auto& blocks = program.post_order_blocks;
    if (blocks.empty()) return 0;
    const IR::Block* entry{};
    if (!program.syntax_list.empty()) {
        if (program.syntax_list.front().type != IR::AbstractSyntaxNode::Type::Block) return 0;
        entry = program.syntax_list.front().data.block;
    } else if (blocks.size() == 1) {
        entry = blocks.front(); // Standalone IR clients without a structured list.
    } else {
        return 0;
    }
    struct State { u8 writes{}, out{63}; bool pending{true}; };
    std::vector<State> states(blocks.size());
    std::unordered_map<const IR::Block*, size_t> indices;
    std::vector<size_t> work;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (!indices.emplace(blocks[i], i).second) return 0;
        work.push_back(i);
        for (const auto& inst : blocks[i]->Instructions()) {
            if (inst.GetOpcode() == IR::Opcode::SetPatch &&
                inst.Arg(0).Patch() <= IR::Patch::TessellationLodInteriorV) {
                states[i].writes |= u8(1u << u32(inst.Arg(0).Patch()));
            }
        }
    }
    if (!indices.contains(entry)) return 0;
    // Missing CFG nodes must not create a proof. Production PostOrder contains
    // exactly the entry-reachable graph; reject incomplete external IR clients.
    for (const auto* block : blocks) {
        for (const auto* pred : block->ImmPredecessors()) if (!indices.contains(pred)) return 0;
        for (const auto* succ : block->ImmSuccessors()) if (!indices.contains(succ)) return 0;
        if (block != entry && block->ImmPredecessors().empty()) return 0;
    }
    while (!work.empty()) {
        const size_t i = work.back(); work.pop_back();
        states[i].pending = false;
        u8 incoming = blocks[i] == entry ? 0 : 63;
        for (const auto* pred : blocks[i]->ImmPredecessors()) {
            incoming &= states[indices.at(pred)].out;
        }
        const u8 out = incoming | states[i].writes;
        if (out == states[i].out) continue;
        states[i].out = out;
        for (const auto* succ : blocks[i]->ImmSuccessors()) {
            const size_t next = indices.at(succ);
            if (!states[next].pending) { states[next].pending = true; work.push_back(next); }
        }
    }
    u8 result = 63;
    bool has_exit{};
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i]->ImmSuccessors().empty()) { result &= states[i].out; has_exit = true; }
    }
    return has_exit ? result : 0;
}

void CollectShaderInfoPass(IR::Program& program, const Profile& profile) {
    Info& info = program.info;
    info.tessellation_bounds={};
    info.tessellation_bounds.guaranteed = GuaranteedTessellationFactors(program);
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(info, inst);
        }
    }

    if (!EmulatorSettings.IsDirectMemoryAccessEnabled()) {
        info.uses_dma = false;
        info.readconst_types = Info::ReadConstType::None;
    }

    if (info.uses_dma) {
        info.buffers.push_back({
            .used_types = IR::Type::U64,
            .buffer_type = BufferType::BdaPagetable,
            .is_written = true,
        });
        info.buffers.push_back({
            .used_types = IR::Type::U32,
            .buffer_type = BufferType::FaultBuffer,
            .is_written = true,
        });
        LOG_ERROR(Render, "Enabling DMA for shader {:#x}", info.pgm_hash);
    }
}

} // namespace Shader::Optimization
