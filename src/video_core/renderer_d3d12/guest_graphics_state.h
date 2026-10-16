// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "video_core/amdgpu/regs.h"
#include <span>
#include <vector>
namespace NativeDX12 {
// Vulkan::Rasterizer::FilterDraw handles CB operations before primitive None,
// then rejects None before any depth/stencil override copies or shader work.
enum class GuestDrawOperation { Rasterize, Skip, FastClear, Resolve };
inline GuestDrawOperation ClassifyGuestDraw(const AmdGpu::Regs& regs) {
    using Mode = AmdGpu::ColorControl::OperationMode;
    if (regs.color_control.mode == Mode::EliminateFastClear ||
        regs.color_control.mode == Mode::FmaskDecompress) return GuestDrawOperation::FastClear;
    if (regs.color_control.mode == Mode::Resolve) return GuestDrawOperation::Resolve;
    if (regs.primitive_type == AmdGpu::PrimitiveType::None) return GuestDrawOperation::Skip;
    return GuestDrawOperation::Rasterize;
}
// DB enable bits can remain set after the game unbinds one of the planes.
// Match the guest attachment validity checks before translating host state.
struct GuestDepthActivity {
    bool depth{},stencil{},bounds{};
    bool HasAttachment()const{return depth||stencil;}
};
inline GuestDepthActivity GuestDepthTests(const AmdGpu::Regs& regs) {
    GuestDepthActivity result;
    result.depth=regs.depth_control.depth_enable&&regs.depth_buffer.DepthValid();
    result.stencil=regs.depth_control.stencil_enable&&regs.depth_buffer.StencilValid();
    result.bounds=regs.depth_control.depth_bounds_enable&&regs.depth_buffer.DepthValid()&&result.HasAttachment();
    return result;
}
struct GuestGraphicsState {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    D3D_PRIMITIVE_TOPOLOGY topology=D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    UINT stencil_reference{};
    UINT stencil_reference_back{};
    UINT8 stencil_read_mask_front{},stencil_write_mask_front{};
    UINT8 stencil_read_mask_back{},stencil_write_mask_back{};
    bool separate_stencil_faces{};
    bool depth_bounds{};
    float depth_bounds_min{},depth_bounds_max{1.f};
    bool dynamic_depth_bias{};
    float depth_bias{},depth_bias_clamp{},slope_depth_bias{};
    bool blend_constant_alpha_rgb{};
    bool rasterizer_discard{};
    bool discard{};
    bool WritesDepth(bool clear_depth=false)const {
        if(discard)return false;
        if(rasterizer_discard)return clear_depth;
        const auto& z=pipeline.DepthStencilState;
        return clear_depth||(z.DepthEnable&&z.DepthWriteMask==D3D12_DEPTH_WRITE_MASK_ALL);
    }
    bool WritesStencil(bool clear_stencil=false)const {
        if(discard)return false;
        if(rasterizer_discard)return clear_stencil;
        const auto& z=pipeline.DepthStencilState;
        const auto writes=[](const D3D12_DEPTH_STENCILOP_DESC& face){
            return face.StencilFailOp!=D3D12_STENCIL_OP_KEEP||
                face.StencilDepthFailOp!=D3D12_STENCIL_OP_KEEP||
                face.StencilPassOp!=D3D12_STENCIL_OP_KEEP;
        };
        const auto front_mask=separate_stencil_faces?stencil_write_mask_front:z.StencilWriteMask;
        const auto back_mask=separate_stencil_faces?stencil_write_mask_back:z.StencilWriteMask;
        return clear_stencil||
            (z.StencilEnable&&((front_mask&&writes(z.FrontFace))||(back_mask&&writes(z.BackFace))));
    }
    bool WritesDepthStencil(bool clear_depth=false,bool clear_stencil=false)const {
        return WritesDepth(clear_depth)||WritesStencil(clear_stencil);
    }
};
// Attachment formats come from the actual native texture/depth cache. Preserve
// PS4 RT slot numbers (including holes); no signature-derived placeholder formats.
GuestGraphicsState BuildGuestGraphicsState(const AmdGpu::Regs&,
    std::span<const DXGI_FORMAT> targets,DXGI_FORMAT depth=DXGI_FORMAT_UNKNOWN,
    UINT patch_control_points=0,bool native_alpha_factor=false);
bool GuestPrimitiveNeedsExpansion(AmdGpu::PrimitiveType type);
std::vector<u32> ExpandGuestPrimitive(AmdGpu::PrimitiveType type,std::span<const u32> indices,
    bool restart=false,u32 restart_index=~0u);
}
