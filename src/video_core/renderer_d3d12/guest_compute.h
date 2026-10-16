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
    // Keep Mesa's 36-byte prefix unchanged. Expanded points perform the final
    // viewport transform themselves so their footprint can cross its edges.
    u32 native_point_flip_mask{};
    alignas(16) std::array<std::array<float,4>,16> viewport_extents{};
    alignas(16) std::array<std::array<float,4>,16> viewport_depths{};
    alignas(16) std::array<std::array<float,4>,16> viewport_clip_transforms{};
    void SetViewports(std::span<const D3D12_VIEWPORT> viewports,bool expanded_points,
        std::span<const std::array<float,4>> clip_transforms){
        if(viewports.empty()||viewports.size()>viewport_extents.size())
            throw std::invalid_argument("Invalid graphics runtime viewport count");
        if(clip_transforms.size()!=viewport_clip_transforms.size())
            throw std::invalid_argument("Invalid graphics runtime clip transform count");
        std::copy(clip_transforms.begin(),clip_transforms.end(),viewport_clip_transforms.begin());
        viewport_width=viewports.front().Width;viewport_height=viewports.front().Height;
        native_point_flip_mask=yz_flip_mask;
        viewport_extents={};viewport_depths={};
        for(size_t i=0;i<viewports.size();++i){
            const auto& v=viewports[i];
            viewport_extents[i]={v.Width,v.Height,v.TopLeftX,v.TopLeftY};
            viewport_depths[i]={v.MinDepth,v.MaxDepth,0,0};
        }
        if(expanded_points)yz_flip_mask=0;
    }
};
static_assert(offsetof(GraphicsRuntime,depth_bias)==32);
static_assert(offsetof(GraphicsRuntime,native_point_flip_mask)==36);
static_assert(offsetof(GraphicsRuntime,viewport_extents)==48);
static_assert(offsetof(GraphicsRuntime,viewport_depths)==304);
static_assert(offsetof(GraphicsRuntime,viewport_clip_transforms)==560);
static_assert(sizeof(GraphicsRuntime)==816);
// Shared native bindings for all linked graphics stages. submit must record the
// actual native draw using the supplied immutable resource table. Completion
// and guest writeback are owned here, including exceptional retirement.
void ExecuteGuestGraphicsResources(ID3D12Device*,Rasterizer&,const ShaderPipeline&,
    std::span<const GuestResourceStage>,const Shader::PushData&,const GraphicsRuntime&,
    const std::function<void(ResourceBindings&)>& submit,GuestTextureCache&,VAddr border_colors=0,
    std::span<const Rasterizer::ShaderBufferRequest> read_buffers={});
}
