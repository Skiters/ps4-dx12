// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_shader_frontend.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include <map>
#include "persistent_cache.h"

namespace NativeDX12 {
struct GuestDxilStages {
    std::array<Shader::Backend::DX12::SpirvConvertResult, Shader::MaxStageTypes> stages;
};
// Compiles a complete graphics stage set together, preserving HS/DS linkage.
// Results contain signed DXIL only. Cache keys refer to stable frontend variants.
class GuestShaderCache {
    std::wstring converter_;
    Shader::Backend::DX12::SpirvConvertOptions options_;
    std::map<std::array<u64, Shader::MaxStageTypes>, GuestDxilStages> graphics_;
    std::map<u64, Shader::Backend::DX12::SpirvConvertResult> compute_;
    PersistentCache disk_;
    CacheKey configuration_;
    u64 disk_hits_{};
public:
    explicit GuestShaderCache(std::wstring converter,
        Shader::Backend::DX12::SpirvConvertOptions options = {});
    u64 PersistentHits()const{return disk_hits_;}
    std::span<const std::byte> ConfigurationIdentity()const{return configuration_.bytes;}
    const GuestDxilStages& Graphics(const GuestGraphicsStages&);
    const Shader::Backend::DX12::SpirvConvertResult& Compute(const GuestShaderStage&);
    void ValidateStage(const GuestShaderStage&);
};
}
