// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/cb_db_extent.h"

namespace NativeDX12 {
template<class Initialize>
void RunColorFeedbackGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const std::array<u32,128>& vertex,std::byte* memory,std::byte* backing,u64 base,
    const Initialize& initialize){
    alignas(256) std::array<u32,128> seed{},left{},right{};
    initialize(seed,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4645454447430001ULL);
    const auto sample=[&](auto& program,u32 x,u64 hash){
        // image_load R -> v4, multiply by exactly .5, export to R32_FLOAT.
        initialize(program,{0xBEEB03FF,15,0x7e000280U|x,0x7e020280,
            0xf0000100,0x00000400,0xbf8c0000,0x100808ff,0x3f000000,
            0x7e0a0280,0x7e0c0280,0x7e0e02ff,0x3f800000,
            0xf800000f,0x07060504,0xbf810000},hash);
    };
    sample(left,0,0x4645454447430002ULL);sample(right,4,0x4645454447430003ULL);
    auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
    regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
    regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
    regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
    regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
    regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
    regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
    regs.color_target_mask.raw=regs.color_shader_mask.raw=1;
    regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
    auto& vp=regs.viewport_control;
    vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
    regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;
    regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
    regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
    regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
    auto& cb=regs.color_buffers[0];cb.base_address=u32((base+16384)>>8);
    cb.info.format=4;cb.info.number_type=7;cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;
    cb.pitch.tile_max=cb.slice.tile_max=1;gpu.last_cb_extent[0]={8,8};
    const auto target=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);
    const auto layout=LayoutGuestTexture(target);
    AmdGpu::Image image{};image.base_address=(base+16384)>>8;
    image.type=u64(AmdGpu::ImageType::Color2D);image.width=image.height=7;image.pitch=15;
    image.data_format=4;image.num_format=7;image.tiling_index=u32(AmdGpu::TileMode::Thin1DThin);
    image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
    const auto sampled=DescribeGuestTexture(image);
    if(!CompatibleGuestColorSampledMapping(target,layout,sampled,LayoutGuestTexture(sampled)))
        throw std::runtime_error("Feedback fixture target/sample physical mapping differs");
    AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
    input.data_format=4;input.num_format=4;
    input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
    memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
    rasterizer.InvalidateMemory(base,65536);memset(memory,0xa5,65536);
    const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
    for(u32 i=0;i<3;++i){const float p[]{xy[i][0],xy[i][1],.5f,1.f};memcpy(memory+i*32,p,sizeof(p));}
    const std::vector<std::byte> physical(backing,backing+65536);
    const auto draw=[&](const auto& program,bool feedback){
        regs.ps_program.address=reinterpret_cast<u64>(program.data())>>8;
        regs.ps_program.settings.num_user_regs=feedback?8:0;regs.ps_program.settings.num_vgprs=1;
        if(feedback)memcpy(regs.ps_program.user_data.data(),&image,sizeof(image));
        rasterizer.Draw(false);
        if(memcmp(backing,physical.data(),65536))
            throw std::runtime_error("Feedback draw published physical CPU bytes before explicit readback");
    };
    draw(seed,false);
    // Read outside the written rectangle. Successive draws consume the last
    // GPU result; this requires no intra-draw fragment communication.
    regs.generic_scissor.top_left_x=4;draw(left,true);
    regs.generic_scissor.top_left_x=0;regs.generic_scissor.bottom_right_x=4;draw(right,true);
    regs.generic_scissor.top_left_x=4;regs.generic_scissor.bottom_right_x=8;draw(left,true);
    rasterizer.Finish();rasterizer.ReadMemory(base,65536);
    auto expected=physical;
    for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
        const float value=x<4?.25f:.125f;
        memcpy(expected.data()+16384+GuestPixelOffset(target,layout,0,0,x,y),&value,4);
    }
    if(memcmp(backing,expected.data(),65536))
        throw std::runtime_error("GCN feedback current GPU pixels or complete physical padding mismatch");
}
} // namespace NativeDX12
