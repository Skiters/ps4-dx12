// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_viewports.h"
#include <algorithm>
#include <cmath>
namespace NativeDX12 {
void IntersectGuestRenderArea(std::span<D3D12_RECT> scissors,UINT width,UINT height){
    if(width>16384||height>16384)throw std::out_of_range("Native render area exceeds attachment limits");
    for(auto& scissor:scissors){
        scissor.left=std::clamp(scissor.left,0L,LONG(width));
        scissor.top=std::clamp(scissor.top,0L,LONG(height));
        scissor.right=std::clamp(scissor.right,scissor.left,LONG(width));
        scissor.bottom=std::clamp(scissor.bottom,scissor.top,LONG(height));
    }
}

std::vector<D3D12_VIEWPORT> BuildGuestPointViewports(std::span<const D3D12_VIEWPORT> original) {
    if(original.empty()||original.size()>AmdGpu::NUM_VIEWPORTS)
        throw std::invalid_argument("Invalid expanded point viewport count");
    // The shader maps the original window coordinates into these viewports.
    // Hardware XY clipping must not truncate a center-clipped point's square
    // at the original viewport boundary; the original scissors still apply.
    return std::vector<D3D12_VIEWPORT>(original.size(),{0,0,16384,16384,0,1});
}
GuestViewports BuildGuestViewports(const AmdGpu::Regs& r) {
    const float vertex_ox=r.polygon_control.enable_window_offset?float(r.window_offset.window_x_offset):0.f;
    const float vertex_oy=r.polygon_control.enable_window_offset?float(r.window_offset.window_y_offset):0.f;
    const int ox=r.window_scissor.window_offset_disable?0:r.window_offset.window_x_offset;
    const int oy=r.window_scissor.window_offset_disable?0:r.window_offset.window_y_offset;
    const D3D12_RECT common{
        std::max({int(r.screen_scissor.top_left_x),int(s16(r.window_scissor.top_left_x))+ox,int(s16(r.generic_scissor.top_left_x))+ox}),
        std::max({int(r.screen_scissor.top_left_y),int(s16(r.window_scissor.top_left_y))+oy,int(s16(r.generic_scissor.top_left_y))+oy}),
        std::min({int(r.screen_scissor.bottom_right_x),int(r.window_scissor.bottom_right_x)+ox,int(r.generic_scissor.bottom_right_x)+ox}),
        std::min({int(r.screen_scissor.bottom_right_y),int(r.window_scissor.bottom_right_y)+oy,int(r.generic_scissor.bottom_right_y)+oy})};
    GuestViewports result;
    for(unsigned i=0;i<AmdGpu::NUM_VIEWPORTS;++i) {
        const auto& v=r.viewports[i];const auto& c=r.viewport_control;
        result.clip_transforms[i]={c.xoffset_enable?v.xoffset:0,c.yoffset_enable?v.yoffset:0,
            c.xscale_enable?v.xscale:1,c.yscale_enable?v.yscale:1};
        if(!v.xscale)continue;
        // Preserve index identity: compacting holes changes SV_ViewportArrayIndex.
        while(result.viewports.size()<i){result.viewports.push_back({0,0,1,1,0,1});result.scissors.push_back({0,0,0,0});}
        const float xo=(c.xoffset_enable?v.xoffset:0)+vertex_ox, xs=c.xscale_enable?v.xscale:1;
        const float yo=(c.yoffset_enable?v.yoffset:0)+vertex_oy, ys=c.yscale_enable?v.yscale:1;
        const float zo=c.zoffset_enable?v.zoffset:0, zs=c.zscale_enable?v.zscale:1;
        D3D12_VIEWPORT vp{xo-xs,yo-ys,2*xs,2*ys,
            r.clipper_control.clip_space==AmdGpu::ClipSpace::MinusWToW?zo-zs:zo,zo+zs};
        if(r.IsClipDisabled())vp={0,0,16384,16384,vp.MinDepth,vp.MaxDepth};
        if(r.IsClipDisabled())for(float f:result.clip_transforms[i])
            if(!std::isfinite(f))throw std::invalid_argument("Nonfinite guest clip viewport transform");
        for(float f:{vp.TopLeftX,vp.TopLeftY,vp.Width,vp.Height,vp.MinDepth,vp.MaxDepth})
            if(!std::isfinite(f))throw std::invalid_argument("Nonfinite guest viewport");
        if(vp.Width<=0||vp.Height==0)throw std::out_of_range("Invalid native guest viewport extent");
        if(vp.Height>0)result.flip_mask|=1u<<i;
        else {vp.TopLeftY+=vp.Height;vp.Height=-vp.Height;}
        vp.MinDepth=std::clamp(vp.MinDepth,0.f,1.f);vp.MaxDepth=std::clamp(vp.MaxDepth,0.f,1.f);
        if(vp.MinDepth>vp.MaxDepth){std::swap(vp.MinDepth,vp.MaxDepth);result.flip_mask|=1u<<(i+16);}
        if(vp.Width>16384||vp.Height>16384)throw std::out_of_range("Native guest viewport exceeds hardware extent");
        auto scissor=common;
        if(r.mode_control.vport_scissor_enable) {
            const auto& s=r.viewport_scissors[i];
            scissor.left=std::max(scissor.left,LONG(s16(s.top_left_x)));
            scissor.top=std::max(scissor.top,LONG(s16(s.top_left_y)));
            scissor.right=std::min(scissor.right,LONG(s.bottom_right_x));
            scissor.bottom=std::min(scissor.bottom,LONG(s.bottom_right_y));
        }
        scissor.right=std::max(scissor.right,scissor.left);scissor.bottom=std::max(scissor.bottom,scissor.top);
        result.viewports.push_back(vp);result.scissors.push_back(scissor);
    }
    if(result.viewports.empty()) {
        result.viewports.push_back({-1,-1,1,1,0,1});
        result.scissors.push_back({0,0,1,1});
        result.flip_mask=1;
    }
    return result;
}
}
