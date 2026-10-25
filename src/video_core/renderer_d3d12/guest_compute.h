// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "guest_graphics_runtime.h"
#include "shader_pipeline.h"
#include "shader_recompiler/info.h"
#include <functional>
namespace NativeDX12 {
class GuestTextureCache;
struct GuestTextureDesc;
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
// Replays use local non-indexed IDs; keep their runtime constants separate
// from the original indexed/instanced producer and from resettable arenas.
void BindGuestGraphicsRuntime(ResourceBindings&,const GraphicsRuntime&);
// Shared native bindings for all linked graphics stages. submit must record the
// actual native draw using the supplied immutable resource table. Completion
// and guest writeback are owned here, including exceptional retirement.
// replayable_bindings opts into independent descriptor forks inside submit;
// all forks must retire before the common resource completion may release them.
// logical_layout merges all participating domains for one preparation/writeback;
// submit projects it into each actual pipeline's layout before recording draws.
void ExecuteGuestGraphicsResources(ID3D12Device*,Rasterizer&,const ShaderPipeline&,
    std::span<const GuestResourceStage>,const Shader::PushData&,const GraphicsRuntime&,
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache&,VAddr border_colors=0,
    std::span<const Rasterizer::ShaderBufferRequest> read_buffers={},bool replayable_bindings=false,
    std::shared_ptr<ResourceLayout> logical_layout={},
    std::span<const GuestTextureDesc> writable_color_parents={});
}
