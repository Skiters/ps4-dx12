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
inline bool GuestOrderedColorSupported(const AmdGpu::ColorBuffer& color) {
    if(AmdGpu::IsInteger(color.GetNumberFmt())){
        switch(color.GetDataFmt()){
        case AmdGpu::DataFormat::Format8:case AmdGpu::DataFormat::Format16:
        case AmdGpu::DataFormat::Format8_8:case AmdGpu::DataFormat::Format32:
        case AmdGpu::DataFormat::Format16_16:case AmdGpu::DataFormat::Format2_10_10_10:
        case AmdGpu::DataFormat::Format8_8_8_8:case AmdGpu::DataFormat::Format32_32:
        case AmdGpu::DataFormat::Format16_16_16_16:
        case AmdGpu::DataFormat::Format32_32_32_32:return true;
        default:return false;
        }
    }
    return ((color.GetDataFmt() == AmdGpu::DataFormat::Format5_6_5 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format1_5_5_5 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format4_4_4_4) &&
            color.GetNumberFmt() == AmdGpu::NumberFormat::Unorm) ||
           ((color.GetDataFmt() == AmdGpu::DataFormat::Format8 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format16) &&
            (color.GetNumberFmt() == AmdGpu::NumberFormat::Unorm ||
             color.GetNumberFmt() == AmdGpu::NumberFormat::Snorm ||
             (color.GetDataFmt() == AmdGpu::DataFormat::Format8 &&
              color.GetNumberFmt() == AmdGpu::NumberFormat::Srgb) ||
             (color.GetDataFmt() == AmdGpu::DataFormat::Format16 &&
              color.GetNumberFmt() == AmdGpu::NumberFormat::Float))) ||
           (color.GetDataFmt() == AmdGpu::DataFormat::Format2_10_10_10 &&
            (color.GetNumberFmt() == AmdGpu::NumberFormat::Unorm ||
             color.GetNumberFmt() == AmdGpu::NumberFormat::Snorm)) ||
           (color.GetDataFmt() == AmdGpu::DataFormat::Format8_8_8_8 &&
            (color.GetNumberFmt() == AmdGpu::NumberFormat::Unorm ||
             color.GetNumberFmt() == AmdGpu::NumberFormat::Srgb ||
             color.GetNumberFmt() == AmdGpu::NumberFormat::Snorm)) ||
           ((color.GetDataFmt() == AmdGpu::DataFormat::Format8_8 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format16_16 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format16_16_16_16) &&
            (color.GetNumberFmt() == AmdGpu::NumberFormat::Unorm ||
             color.GetNumberFmt() == AmdGpu::NumberFormat::Snorm ||
             (color.GetDataFmt() == AmdGpu::DataFormat::Format8_8 &&
              color.GetNumberFmt() == AmdGpu::NumberFormat::Srgb))) ||
           ((color.GetDataFmt() == AmdGpu::DataFormat::Format32 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format10_11_11 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format32_32 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format32_32_32_32 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format16_16_16_16 ||
             color.GetDataFmt() == AmdGpu::DataFormat::Format16_16) &&
            color.GetNumberFmt() == AmdGpu::NumberFormat::Float);
}
inline u32 GuestOrderedColorMask(const AmdGpu::Regs& regs, u32 active_targets = ~0u,
                                bool native_alpha_factor = true) {
    u32 result{};
    if (regs.color_control.mode != AmdGpu::ColorControl::OperationMode::Normal) return result;
    const bool logic = regs.color_control.rop3 != AmdGpu::ColorControl::LogicOp::Copy;
    bool independent_logic{};
    if (logic) {
        u32 shared_mask{};
        bool found{}, shared_logic{};
        for (u32 i = 0; i < AmdGpu::NUM_COLOR_BUFFERS; ++i) {
            const auto& color = regs.color_buffers[i];
            if (!color || (active_targets & (1u << i)) == 0) continue;
            const u32 mask = color.Swizzle().ApplyMask(regs.color_target_mask.GetMask(i));
            const bool applies=color.GetNumberFmt()!=AmdGpu::NumberFormat::Float&&
                color.GetNumberFmt()!=AmdGpu::NumberFormat::Srgb;
            if (!found) {
                shared_mask = mask;
                shared_logic = applies;
                found = true;
            } else {
                independent_logic |= mask != shared_mask || applies != shared_logic;
            }
        }
    }
    bool mixed_constants{};
    if (!logic && !native_alpha_factor) {
        using F = AmdGpu::BlendControl::BlendFactor;
        const auto is_color = [](F factor) {
            return factor == F::ConstantColor || factor == F::OneMinusConstantColor;
        };
        const auto is_alpha = [](F factor) {
            return factor == F::ConstantAlpha || factor == F::OneMinusConstantAlpha;
        };
        bool color{}, alpha{};
        for (u32 i = 0; i < AmdGpu::NUM_COLOR_BUFFERS; ++i) {
            const auto& target = regs.color_buffers[i];
            const auto& blend = regs.blend_control[i];
            if (!target || (active_targets & (1u << i)) == 0 || !blend.enable ||
                target.info.blend_bypass ||
                !target.Swizzle().ApplyMask(regs.color_target_mask.GetMask(i))) continue;
            color |= is_color(blend.color_src_factor) || is_color(blend.color_dst_factor);
            alpha |= is_alpha(blend.color_src_factor) || is_alpha(blend.color_dst_factor);
        }
        mixed_constants = color && alpha;
    }
    for (u32 i = 0; i < AmdGpu::NUM_COLOR_BUFFERS; ++i) {
        if ((active_targets & (1u << i)) == 0) continue;
        const auto& color = regs.color_buffers[i];
        const auto& blend = regs.blend_control[i];
        const bool writes = color && color.Swizzle().ApplyMask(regs.color_target_mask.GetMask(i));
        const bool ordered_blend = !logic && !AmdGpu::IsInteger(color.GetNumberFmt()) &&
            blend.enable && !color.info.blend_bypass &&
            ((color.GetDataFmt() == AmdGpu::DataFormat::Format2_10_10_10 &&
              color.GetNumberFmt() == AmdGpu::NumberFormat::Snorm) || mixed_constants);
        const bool ordered_logic = logic && independent_logic &&
            color.GetNumberFmt() != AmdGpu::NumberFormat::Float &&
            color.GetNumberFmt() != AmdGpu::NumberFormat::Srgb;
        if (writes && (ordered_blend || ordered_logic) && GuestOrderedColorSupported(color))
            result |= 1u << i;
    }
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
    u32 ordered_color_mask{};
    bool rasterizer_discard{};
    bool polygon_points{};
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
inline bool GuestPolygonPointGeometryState(const AmdGpu::Regs& regs){
    using Stages=AmdGpu::ShaderStageEnable::VgtStages;
    using P=AmdGpu::PrimitiveType;
    const bool vertex_triangles=regs.stage_enable.raw==Stages::Vs&&
        (regs.primitive_type==P::TriangleList||regs.primitive_type==P::TriangleFan||
         regs.primitive_type==P::Polygon||regs.primitive_type==P::TriangleStrip);
    const bool tessellated_triangles=regs.stage_enable.raw==Stages::LsHs&&
        regs.primitive_type==P::PatchPrimitive&&regs.tess_config.type!=AmdGpu::TessellationType::Isoline;
    return regs.polygon_control.PolyMode()==AmdGpu::PolygonMode::Point&&
        (vertex_triangles||tessellated_triangles)&&
        (!regs.polygon_control.NeedsBias()||!regs.depth_render_override.disable_viewport_clamp)&&
        (regs.clipper_control.ZclipEnable()||!regs.depth_render_override.disable_viewport_clamp);
}
// Attachment formats come from the actual native texture/depth cache. Preserve
// PS4 RT slot numbers (including holes); no signature-derived placeholder formats.
GuestGraphicsState BuildGuestGraphicsState(const AmdGpu::Regs&,
    std::span<const DXGI_FORMAT> targets,DXGI_FORMAT depth=DXGI_FORMAT_UNKNOWN,
    UINT patch_control_points=0,bool native_alpha_factor=false);
bool GuestPrimitiveNeedsExpansion(AmdGpu::PrimitiveType type);
std::vector<u32> ExpandGuestPrimitive(AmdGpu::PrimitiveType type,std::span<const u32> indices,
    bool restart=false,u32 restart_index=~0u,u32 patch_points=0,bool strip_last_provoking=false);
}
