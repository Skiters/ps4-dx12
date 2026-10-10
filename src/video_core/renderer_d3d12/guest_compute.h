// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "shader_pipeline.h"
#include "shader_recompiler/info.h"
#include <functional>
namespace NativeDX12 {
class GuestTextureCache;
struct ComputeInvocation {
    u32 groups[3]{1,1,1};
    u32 base[3]{};
    u64 shared_bytes{};
    std::span<const float> clip_planes;
    u32 logical_groups[3]{};
};
bool ExecuteGuestCopyShader(Rasterizer&,const Shader::Info&,const ComputeInvocation&);
// GPU thread only. Resource metadata comes from the original guest translator;
// descriptor registers come from the exact specialized stage and signed DXIL.
// This synchronous path returns guest GPU writes before temporary storage dies.
void ExecuteGuestBufferCompute(ID3D12Device*,Rasterizer&,const ShaderPipeline&,
    const Shader::Info&,Shader::Backend::Bindings,const ComputeInvocation&,GuestTextureCache* textures=nullptr,
    VAddr border_colors=0);
struct GuestResourceStage {const Shader::Info* info;Shader::Backend::Bindings bindings;};
struct GraphicsRuntime {
    u32 first_vertex{},base_instance{},indexed{},yz_flip_mask{},draw_id{};
    float viewport_width{},viewport_height{};
    u32 view_index{};float depth_bias{};
};
// Shared native bindings for all linked graphics stages. submit must record the
// actual native draw using the supplied immutable resource table. Completion
// and guest writeback are owned here, including exceptional retirement.
void ExecuteGuestGraphicsResources(ID3D12Device*,Rasterizer&,const ShaderPipeline&,
    std::span<const GuestResourceStage>,const Shader::PushData&,const GraphicsRuntime&,
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache&,VAddr border_colors=0);
}
