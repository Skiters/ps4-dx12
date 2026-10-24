// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_graphics_state.h"
namespace NativeDX12 {
template<class Program,class Initialize>
void RunPolygonPointFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const Program& vertex,Initialize initialize,std::byte* memory,std::byte* backing,u64 base){
    alignas(256) std::array<u32,128> pixel{};
    initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3e800000,0xf800080f,0x00000000,0xbf810000},
        0x504f4c59504f494eull);
    struct Case {float xy[3][2];std::vector<std::array<u32,2>> pixels;};
    // Independent pixel coordinates for viewport [.5,8.5] in both axes.
    // Includes a new clipping edge and an original vertex exactly on it.
    const Case cases[]{
        {{{-.5f,-.5f},{.5f,-.5f},{-.5f,.5f}},{{2,6},{6,6},{2,2}}},
        {{{-2.f,-.5f},{0.f,-.5f},{0.f,.5f}},{{0,4},{0,6},{4,6},{4,2}}},
        {{{-1.f,-.5f},{0.f,-.5f},{0.f,.5f}},{{0,6},{4,6},{4,2}}},
        {{{-2.f,-.5f},{-1.f,-.5f},{0.f,.5f}},{{0,4},{0,6},{4,2}}}};
    for(const auto& test:cases)for(bool reverse:{false,true})for(u32 cull=0;cull<3;++cull){
        rasterizer.ReadMemory(base,65536);rasterizer.Finish();rasterizer.UnmapMemory(base,65536);
        rasterizer.MapMemory(base,65536);rasterizer.NotifyHostMapping(base,65536,true,true);
        memset(memory,0xa7,65536);
        for(u32 i=0;i<3;++i){const auto j=reverse&&i?3-i:i;
            const float position[]{test.xy[j][0],test.xy[j][1],.5f,1.f};
            memcpy(memory+i*32,position,sizeof(position));}
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
        regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;regs.ps_program.settings.num_vgprs=1;
        regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_shader_mask.raw=15;regs.color_target_mask.raw=1;
        regs.polygon_control.enable_polygon_mode=1;
        regs.polygon_control.polygon_mode_front=regs.polygon_control.polygon_mode_back=AmdGpu::PolygonMode::Point;
        regs.polygon_control.cull_front=cull==1;regs.polygon_control.cull_back=cull==2;
        regs.polygon_control.front_face=AmdGpu::FrontFace::CounterClockwise;
        regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& vp=regs.viewport_control;vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
        regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;regs.viewports[0].xoffset=regs.viewports[0].yoffset=4.5f;
        regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);cb.info.format=4;cb.info.number_type=7;
        cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;cb.pitch.tile_max=cb.slice.tile_max=1;
        gpu.last_cb_extent[0]={8,8};
        const auto desc=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto layout=LayoutGuestTexture(desc);
        for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){const float seed=0;
            memcpy(memory+4096+GuestPixelOffset(desc,layout,0,0,x,y),&seed,4);}
        auto& blend=regs.blend_control[0];blend.enable=1;
        blend.color_src_factor=blend.color_dst_factor=AmdGpu::BlendControl::BlendFactor::One;
        blend.color_func=AmdGpu::BlendControl::BlendFunc::Add;
        // Additive output detects duplicate clipping vertices and quad overlap.
        std::vector<std::byte> expected(backing,backing+65536);
        if(cull==0||(cull==1&&reverse)||(cull==2&&!reverse))
            for(const auto& point:test.pixels){const float value=.25f;
                memcpy(expected.data()+4096+GuestPixelOffset(desc,layout,0,0,point[0],point[1]),&value,4);}
        rasterizer.Draw(false);rasterizer.ReadMemory(desc.address,layout.guest_bytes);rasterizer.Finish();
        rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),expected.size()))
            throw std::runtime_error("Polygon points changed clipped corners, culling, additive count or padding");
    }
}
}
