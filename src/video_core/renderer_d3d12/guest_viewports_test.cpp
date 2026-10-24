// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_viewports.h"
#include <cstdio>
#include <limits>
using namespace NativeDX12;
void Require(bool x){if(!x)throw std::runtime_error("Guest viewport mapping mismatch");}
int main(){try {
    AmdGpu::Regs r{};auto& c=r.viewport_control;
    const auto fallback=BuildGuestViewports(r);
    Require(fallback.viewports.size()==1&&fallback.flip_mask==1&&fallback.viewports[0].TopLeftX==-1&&
        fallback.viewports[0].TopLeftY==-1&&fallback.viewports[0].Width==1&&fallback.viewports[0].Height==1&&
        fallback.scissors[0].left==0&&fallback.scissors[0].top==0&&fallback.scissors[0].right==1&&fallback.scissors[0].bottom==1);
    c.xscale_enable=c.yscale_enable=c.xoffset_enable=c.yoffset_enable=c.zscale_enable=c.zoffset_enable=1;
    auto& v=r.viewports[0];v.xscale=640;v.xoffset=640;v.yscale=-400;v.yoffset=400;v.zscale=1;
    r.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
    r.screen_scissor.bottom_right_x=r.window_scissor.bottom_right_x=r.generic_scissor.bottom_right_x=1280;
    r.screen_scissor.bottom_right_y=r.window_scissor.bottom_right_y=r.generic_scissor.bottom_right_y=800;
    auto p=BuildGuestViewports(r);Require(p.viewports.size()==1&&!p.flip_mask);
    Require(p.viewports[0].TopLeftX==0&&p.viewports[0].TopLeftY==0&&p.viewports[0].Width==1280&&p.viewports[0].Height==800);
    Require(p.scissors[0].right==1280&&p.scissors[0].bottom==800);
    v.yscale=400;v.zscale=-1;v.zoffset=1;
    p=BuildGuestViewports(r);Require(p.flip_mask==0x10001&&p.viewports[0].MinDepth==0&&p.viewports[0].MaxDepth==1);
    r.mode_control.vport_scissor_enable=1;r.viewport_scissors[0].top_left_x=100;
    r.viewport_scissors[0].bottom_right_x=900;r.viewport_scissors[0].bottom_right_y=500;
    p=BuildGuestViewports(r);Require(p.scissors[0].left==100&&p.scissors[0].right==900&&p.scissors[0].bottom==500);
    r.polygon_control.enable_window_offset=1;r.window_offset.window_x_offset=7;r.window_offset.window_y_offset=-3;
    p=BuildGuestViewports(r);Require(p.viewports[0].TopLeftX==7&&p.viewports[0].TopLeftY==-3);
    r.polygon_control.enable_window_offset=0;const auto saved=v;r.viewports[3]=saved;r.viewports[0].xscale=0;
    p=BuildGuestViewports(r);Require(p.viewports.size()==4&&p.scissors[0].right==0&&p.scissors[2].bottom==0);
    r.viewports[0]=saved;r.viewports[3].xscale=0;
    r.clipper_control.clip_disable=1;r.viewports[1]=saved;
    r.viewports[1].xoffset=1700;r.viewports[1].yscale=-250;
    p=BuildGuestViewports(r);
    Require(p.viewports.size()==2&&p.viewports[1].Width==16384&&p.viewports[1].Height==16384);
    Require(p.clip_transforms[0]==std::array<float,4>{640,400,640,400}&&
        p.clip_transforms[1]==std::array<float,4>{1700,400,640,-250});
    r.clipper_control.clip_disable=0;
    v.yscale=std::numeric_limits<float>::quiet_NaN();bool rejected=false;
    try{BuildGuestViewports(r);}catch(const std::invalid_argument&){rejected=true;}Require(rejected);
    std::array<D3D12_RECT,4> areas{{{-3,-2,1280,800},{100,200,900,700},{900,700,1200,800},{0,0,0,0}}};
    IntersectGuestRenderArea(areas,640,480);
    Require(areas[0].left==0&&areas[0].top==0&&areas[0].right==640&&areas[0].bottom==480);
    Require(areas[1].left==100&&areas[1].top==200&&areas[1].right==640&&areas[1].bottom==480);
    Require(areas[2].left==640&&areas[2].right==640&&areas[2].top==480&&areas[2].bottom==480);
    Require(areas[3].right==0&&areas[3].bottom==0);
    IntersectGuestRenderArea(areas,0,0);
    for(const auto& area:areas)Require(!area.left&&!area.top&&!area.right&&!area.bottom);
    rejected=false;try{IntersectGuestRenderArea(areas,16385,1);}catch(const std::out_of_range&){rejected=true;}Require(rejected);
    puts("PASS: native 1280x800 viewport orientation, reversed depth, intersected scissor and nonfinite rejection");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
