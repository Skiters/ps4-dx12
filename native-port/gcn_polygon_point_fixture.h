// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_graphics_state.h"
namespace NativeDX12 {
template<class Program,class Initialize>
void RunPolygonPointFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const Program& vertex,Initialize initialize,std::byte* memory,std::byte* backing,u64 base,bool routed=false,
    bool depth_clamped=false){
    alignas(256) std::array<u32,128> pixel{};
    initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3e800000,0xf800080f,0x00000000,0xbf810000},
        0x504f4c59504f494eull);
    alignas(256) std::array<u32,128> routed_vertex{};
    std::vector<u32> routed_words{0xBEEB03FF,15};
    for(u32 i=0;i<7;++i){routed_words.push_back(0xe0302000u|i*4);routed_words.push_back((128u<<24)|((4+i)<<8));}
    routed_words.insert(routed_words.end(),{0xbf8c007f,0xf80000dd,0x0a090008,0xf80008cf,0x07060504,0xbf810000});
    initialize(routed_vertex,routed_words,0x504f4c59524f5554ull);
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
            const float position[]{test.xy[j][0],test.xy[j][1],depth_clamped?(reverse?1.5f:-.5f):.5f,1.f};
            memcpy(memory+i*32,position,sizeof(position));
            const float size=3.f;const u32 layer=1,viewport=1;
            memcpy(memory+i*32+16,&size,4);memcpy(memory+i*32+20,&layer,4);memcpy(memory+i*32+24,&viewport,4);}
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(routed?routed_vertex.data():vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=routed?2:1;
        if(routed)regs.vs_output_control.vs_out_misc_enable=regs.vs_output_control.use_vtx_point_size=
            regs.vs_output_control.use_vtx_render_target_idx=regs.vs_output_control.use_vtx_viewport_idx=1;
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
        if(depth_clamped){
            regs.clipper_control.zclip_near_disable=regs.clipper_control.zclip_far_disable=1;
            vp.zoffset_enable=1;regs.viewports[0].zscale=.5f;regs.viewports[0].zoffset=.25f;
        }
        if(routed){regs.viewports[1]=regs.viewports[0];regs.viewports[1].yscale=4;}
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);cb.info.format=4;cb.info.number_type=7;
        cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;cb.pitch.tile_max=cb.slice.tile_max=1;
        cb.view.slice_max=routed?1:0;
        gpu.last_cb_extent[0]={8,8};
        const auto desc=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto layout=LayoutGuestTexture(desc);
        for(u32 layer=0;layer<desc.layers;++layer)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){const float seed=0;
            memcpy(memory+4096+GuestPixelOffset(desc,layout,0,layer,x,y),&seed,4);}
        GuestTextureDesc depth_desc{};GuestTextureLayout depth_layout{};
        if(depth_clamped){
            auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=u32((base+8192)>>8);
            db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;db.z_info.tile_mode_index=5;
            db.depth_size.pitch_tile_max=1;db.depth_size.height_tile_max=0;db.depth_slice.tile_max=1;
            gpu.last_db_extent={8,8};
            regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;
            regs.depth_control.depth_func=AmdGpu::CompareFunc::Always;
            depth_desc=DescribeGuestDepthBuffer(db,regs.depth_view,gpu.last_db_extent);
            depth_layout=LayoutGuestTexture(depth_desc);
            for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){const float seed=1.f;
                memcpy(memory+8192+GuestPixelOffset(depth_desc,depth_layout,0,0,x,y),&seed,4);}
        }
        auto& blend=regs.blend_control[0];blend.enable=1;
        blend.color_src_factor=blend.color_dst_factor=AmdGpu::BlendControl::BlendFactor::One;
        blend.color_func=AmdGpu::BlendControl::BlendFunc::Add;
        // Additive output detects duplicate clipping vertices and quad overlap.
        std::vector<std::byte> expected(backing,backing+65536);
        const bool front=reverse==routed;
        if(cull==0||(cull==1&&!front)||(cull==2&&front))
            for(const auto& point:test.pixels){const int radius=routed?1:0;
                for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
                    const int x=int(point[0])+dx,y=(routed?8-int(point[1]):int(point[1]))+dy;
                    if(x<0||x>=8||y<0||y>=8)continue;
                    const auto offset=4096+GuestPixelOffset(desc,layout,0,routed?1:0,u32(x),u32(y));
                    float value;memcpy(&value,expected.data()+offset,4);value+=.25f;
                    memcpy(expected.data()+offset,&value,4);
                    if(depth_clamped){const float z=reverse?.75f:.25f;
                        memcpy(expected.data()+8192+GuestPixelOffset(depth_desc,depth_layout,0,0,u32(x),u32(y)),&z,4);}
                }}
        rasterizer.Draw(false);rasterizer.ReadMemory(desc.address,layout.guest_bytes);
        if(depth_clamped)rasterizer.ReadMemory(depth_desc.address,depth_layout.guest_bytes);
        rasterizer.Finish();
        rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),expected.size()))
            throw std::runtime_error("Polygon points changed clipped corners, culling, additive count or padding");
    }
}
}
