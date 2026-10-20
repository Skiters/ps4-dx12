// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <d3d12.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
namespace NativeDX12 {
struct GraphicsRuntime {
    u32 first_vertex{},base_instance{},indexed{},yz_flip_mask{},draw_id{};
    float viewport_width{},viewport_height{};
    u32 view_index{};float depth_bias{};
    // Keep Mesa's 36-byte prefix unchanged. Expanded points perform the final
    // viewport transform themselves so their footprint can cross its edges.
    u32 native_point_flip_mask{};
    // Split capture draws advance base_instance for InstanceIndex, while
    // guest BaseInstance/SGPR remains that of the original logical draw.
    u32 capture_base_instance{};
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
static_assert(offsetof(GraphicsRuntime,capture_base_instance)==40);
static_assert(offsetof(GraphicsRuntime,viewport_extents)==48);
static_assert(offsetof(GraphicsRuntime,viewport_depths)==304);
static_assert(offsetof(GraphicsRuntime,viewport_clip_transforms)==560);
static_assert(sizeof(GraphicsRuntime)==816);
} // namespace NativeDX12
