// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/guest_shader_runtime.h"
#include "shader_recompiler/recompiler.h"
#include "shader_recompiler/specialization.h"
#include <map>
#include <memory>
#include <tuple>
#include <vector>

namespace NativeDX12 {
struct GuestShaderStage {
    u64 id{};
    bool auxiliary{};
    Shader::Info info;
    Shader::RuntimeInfo runtime{};
    Shader::Backend::Bindings bindings_start{};
    Shader::Backend::Bindings bindings_end{};
    Shader::StageSpecialization specialization;
    std::vector<u32> spirv;
    std::vector<u32> patch_spirv;
    bool is_patched{},debug_collected{},requires_point_capture{};
    std::span<const u32> EffectiveSpirv()const{return is_patched?std::span<const u32>(patch_spirv):std::span<const u32>(spirv);}
};
struct GuestGraphicsStages {
    std::array<GuestShaderStage*, Shader::MaxStageTypes> stages{};
    u32 mrt_mask{};
    u32 ordered_color_mask{};
};
struct GuestPointCaptureStages {
    GuestGraphicsStages producer,replay,stream_producer;
    u32 stream_patch_vertices{};
    bool stream_tessellated{};
};
// Owns original guest reflection and specialization. Stage objects have stable
// addresses because StageSpecialization retains an Info pointer.
// GPU command thread only; outputs remain owned by this frontend.
class GuestShaderFrontend {
    Shader::Profile profile_;
    Shader::Pools pools_;
    using Key = std::tuple<u64, Shader::HwStage, Shader::SwStage>;
    std::map<Key, std::map<size_t,std::unique_ptr<GuestShaderStage>>> programs_;
    std::map<std::vector<u32>, std::unique_ptr<GuestShaderStage>> auxiliary_;
    u64 next_id_ = 1;
    GuestShaderStage& Translate(const AmdGpu::Regs&, const AmdGpu::ComputeProgram*, Shader::HwStage, Shader::SwStage,
        Shader::ShaderParams, const VideoCore::GuestRuntimeContext&, Shader::Backend::Bindings&,
        std::span<const Shader::NativeImageWrite> writes = {},
        std::span<const Shader::NativeImageWrite> signed_packed_views = {},
        std::span<const Shader::NativeImageWrite> raw32_views = {});
public:
    explicit GuestShaderFrontend(const Shader::Profile& profile) : profile_(profile) {}
    const Shader::Profile& Profile()const{return profile_;}
    GuestShaderStage* FindStage(u64);
    u64 ReserveStageId(){if(!next_id_)throw std::overflow_error("Native shader identity exhausted");return next_id_++;}
#ifdef SHADPS4_NATIVE_DX12_CORE
    void SerializeStage(Serialization::Archive&,const GuestShaderStage&)const;
    GuestShaderStage& RestoreStage(Serialization::Archive&);
#endif
    GuestGraphicsStages BuildGraphics(AmdGpu::Liverpool&, u16 vertex_sgpr_offset = 0,
        u32 instance_sgpr_offset = 0, bool is_indirect = false);
    GuestGraphicsStages BuildGraphics(const AmdGpu::Regs&, u16 vertex_sgpr_offset = 0,
        u32 instance_sgpr_offset = 0, bool is_indirect = false);
    GuestPointCaptureStages BuildPointCaptureGraphics(const AmdGpu::Regs&,
        u16 vertex_sgpr_offset=0,u32 instance_sgpr_offset=0,bool is_indirect=false);
    GuestGraphicsStages BuildPrimitiveStream(const GuestGraphicsStages&);
    GuestGraphicsStages BuildBufferedPointCapture(const GuestGraphicsStages&);
    GuestShaderStage& BuildCompute(AmdGpu::Liverpool&);
};
}
