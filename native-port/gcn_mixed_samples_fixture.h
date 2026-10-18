// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace NativeDX12 {
template<class Program,class Initialize>
void RunMixedSamplesGcnFixture(GuestRasterizer& r,AmdGpu::Liverpool& gpu,
    const Program& vertex,Initialize initialize,std::byte* memory,std::byte* backing,u64 base,bool matrix){
    alignas(256) std::array<u32,128> red{},blue{};
    initialize(red,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4d49585352454431ull);
    initialize(blue,{0xBEEB03FF,15,0x7e000280,0x7e020280,0x7e0402ff,0x3f800000,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x4d495853424c5531ull);
    unsigned cases{};
    for(u32 color_log=0;color_log<4;++color_log)for(u32 depth_log=0;depth_log<4;++depth_log){
        if(!matrix&&!((color_log==0&&depth_log==2)||(color_log==2&&depth_log==0)))continue;
        r.ReadMemory(base,65536);r.Finish();r.UnmapMemory(base,65536);r.MapMemory(base,65536);
        r.NotifyHostMapping(base,65536,true,true);memset(memory,0xa7,65536);
        const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
        for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
        regs.ps_program.address=reinterpret_cast<u64>(blue.data())>>8;regs.ps_program.settings.num_vgprs=1;
        regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_target_mask.raw=regs.color_shader_mask.raw=15;regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& vp=regs.viewport_control;vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
        regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);cb.info.format=10;cb.info.number_type=0;
        cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;cb.attrib.num_fragments_log2=color_log;
        cb.pitch.tile_max=cb.slice.tile_max=1;gpu.last_cb_extent[0]={8,8};gpu.last_db_extent={8,8};
        auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=u32((base+8192)>>8);
        db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;db.z_info.tile_mode_index=5;db.z_info.num_samples=depth_log;
        db.depth_size.pitch_tile_max=db.depth_size.height_tile_max=1;db.depth_slice.tile_max=3;
        regs.depth_control.depth_func=AmdGpu::CompareFunc::Less;
        const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto cl=LayoutGuestTexture(color);
        const auto depth=DescribeGuestDepthBuffer(db,regs.depth_view,gpu.last_db_extent);const auto dl=LayoutGuestTexture(depth);
        if(cl.guest_bytes>4096||dl.guest_bytes>8192)throw std::runtime_error("Mixed sample fixture allocation overlap");
        constexpr u32 initial_depth=0x3f400000,blocked_depth=0x3e800000,draw_depth=0x3f000000;
        for(u32 sample=0;sample<depth.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
            const auto value=sample%2?blocked_depth:initial_depth;
            memcpy(memory+8192+GuestPixelOffset(depth,dl,0,0,x,y,sample),&value,4);
        }
        std::vector<std::byte> expected(backing,backing+65536);
        const auto check=[&](u32 rgba,bool written_depth,const char* phase){
            for(u32 sample=0;sample<color.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
                // Runtime's per-index MSAA blit uses mandatory robustness2.
                // A single-sample source instead broadcasts sample zero.
                const u32 value=written_depth&&depth.samples>1&&sample>=depth.samples?0:
                    written_depth&&depth.samples>1&&(sample%2)&&rgba==0xff0000ff?0xffff0000:rgba;
                memcpy(expected.data()+4096+GuestPixelOffset(color,cl,0,0,x,y,sample),&value,4);
            }
            if(written_depth)for(u32 sample=0;sample<depth.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
                const auto value=sample%2?blocked_depth:draw_depth;
                memcpy(expected.data()+8192+GuestPixelOffset(depth,dl,0,0,x,y,sample),&value,4);
            }
            r.ReadMemory(color.address,cl.guest_bytes);if(written_depth)r.ReadMemory(depth.address,dl.guest_bytes);
            r.Finish();r.ValidateDebugMessages();
            if(memcmp(backing,expected.data(),expected.size())){
                for(size_t i=0;i<expected.size();++i)if(backing[i]!=expected[i]){
                    fprintf(stderr,"MIXED color=%u depth=%u phase=%s offset=%zu got=%02x expected=%02x\n",color.samples,depth.samples,phase,i,unsigned(backing[i]),unsigned(expected[i]));break;}
                throw std::runtime_error("Mixed color/depth draw lost samples, contents or full physical padding");
            }
        };
        r.Draw(false);check(0xffff0000,false,"warm original blue");
        regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;
        regs.aa_config.msaa_num_samples=depth_log;regs.ps_program.address=reinterpret_cast<u64>(red.data())>>8;
        r.Draw(false);check(0xff0000ff,true,"mixed red");
        regs.ps_program.address=reinterpret_cast<u64>(blue.data())>>8;
        regs.depth_control.depth_func=AmdGpu::CompareFunc::LessEqual;
        r.Draw(false);check(0xffff0000,true,"mixed cached blue");++cases;
    }
    printf("PASS:%u actual GCN mixed color/depth counts,warm original/mixed red/cached blue,all color/depth samples and full65536 padding\n",cases);
}
} // namespace NativeDX12
