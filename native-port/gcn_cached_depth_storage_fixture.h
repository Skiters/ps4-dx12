// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/cb_db_extent.h"
#include "video_core/amdgpu/pm4_cmds.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace NativeDX12 {
template<class Initialize>
void RunCachedDepthStorageGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const std::array<u32,128>& plain_vertex,const std::vector<u32>& vertex_words,bool ordered,std::byte* memory,std::byte* backing,u64 base,
    const Initialize& initialize,bool cmask=false,u32 cmask_format=10){
    alignas(256) std::array<u32,128> pixel{},writer{},probe{},ordered_vertex{},ordered_pixel{};
    initialize(pixel,{0xBEEB03FF,15,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x43534453504c0001ULL);
    // Observe the formatted buffer BEFORE the writable image's original bind.
    // Both real operations execute later in one guest compute invocation.
    initialize(writer,{0x7e000280,0x7e020280,0xe0000000,0x80000400,0xbf8c0000,
        0x7e1002ff,0x3f000000,0xf0200100,0x00010800,
        0xe0700000,0x80030400,0xbf810000},0x4353445357520001ULL);
    initialize(probe,{0x7e000280,0x7e020280,0xe0300000,0x80000400,
        0xf0000100,0x00010500,0x7e000281,0xf0000100,0x00010600,0xbf8c0000,
        0xe0700000,0x80030400,0xe0700004,0x80030500,0xe0700008,0x80030600,
        0xbf810000},0x4353445350520001ULL);
    // Pinned GetStages orders Fragment before Vertex, each buffers then images.
    // The early PS buffer must retain B while the later VS image becomes UAV.
    auto ordered_words=vertex_words;ordered_words.resize(ordered_words.size()-3);
    ordered_words.insert(ordered_words.end(),{0x7e000280,0x7e020280,0x7e1002ff,0x3f000000,
        0xf0200100,0x00010800,0xf80008cf,0x07060504,0xbf810000});
    initialize(ordered_vertex,ordered_words,0x43534453564f0002ULL);
    initialize(ordered_pixel,{0xBEEB03FF,15,0x7e000280,0xe0000000,0x80000400,0xbf8c0000,
        0xe0700000,0x80010400,0x7e0002ff,0x3f800000,0x7e020280,0x7e040280,
        0x7e0602ff,0x3f800000,0xf800000f,0x03020100,0xbf810000},0x43534453504f0002ULL);
    gpu.BindRasterizer(&rasterizer);
    struct ResetQueue {AmdGpu::Liverpool& gpu;~ResetQueue(){gpu.WaitGpuIdle();gpu.BindRasterizer(nullptr);}} reset{gpu};
    AmdGpu::ComputeProgram compute{};
    const auto dispatch=[&]{
        // GetCsRegs is valid only inside Liverpool's active queue coroutine.
        // Submit real SH registers and DISPATCH_DIRECT through that queue.
        constexpr u32 words=sizeof(compute)/sizeof(u32);
        std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,
            words,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
        const auto data=std::bit_cast<std::array<u32,words>>(compute);
        packets.insert(packets.end(),data.begin(),data.end());
        packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,
            3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
        gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
    };
    unsigned cases{};
    for(const bool stencil:{false,true})for(const bool htile:{false,true})for(const bool dirty:{false,true}){
        printf("CASE: actual GCN cached depth -> storage S=%u HTILE=%u Dirty=%u ordered=%u\n",unsigned(stencil),unsigned(htile),unsigned(dirty),unsigned(ordered));fflush(stdout);
        // End the previous allocation identity rather than reuse cached native
        // history at a recycled physical address as a new seed.
        rasterizer.Finish();rasterizer.UnmapMemory(base,65536);rasterizer.MapMemory(base,65536);
        rasterizer.NotifyHostMapping(base,65536,true,true);memset(memory,0xee,65536);
        const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
        for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],0.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(plain_vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
        regs.ps_program.address=reinterpret_cast<u64>(pixel.data())>>8;
        regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=AmdGpu::ColorControl::LogicOp::Copy;regs.color_target_mask.raw=regs.color_shader_mask.raw=15;
        regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& vp=regs.viewport_control;vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
        regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);cb.info.format=10;cb.info.number_type=0;
        cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;cb.pitch.tile_max=cb.slice.tile_max=1;gpu.last_cb_extent[0]={8,8};
        auto& db=regs.depth_buffer;db.z_read_base=db.z_write_base=u32((base+8192)>>8);
        db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Z32Float;db.z_info.tile_mode_index=5;db.z_info.tile_surface_enable=htile;
        db.depth_size.pitch_tile_max=db.depth_size.height_tile_max=3;db.depth_slice.tile_max=15;gpu.last_db_extent={32,32};
        if(stencil){db.stencil_read_base=db.stencil_write_base=u32((base+16384)>>8);db.stencil_info.format=AmdGpu::DepthBuffer::StencilFormat::Stencil8;}
        regs.depth_htile_data_base.address=u32((base+32768)>>8);
        regs.depth_control.depth_enable=regs.depth_control.depth_write_enable=1;regs.depth_control.depth_func=AmdGpu::CompareFunc::Never;
        regs.depth_render_control.depth_clear_enable=1;regs.depth_clear=0.875f;
        AmdGpu::Buffer positions{};positions.base_address=base;positions.stride=32;positions.num_records=3;
        positions.data_format=4;positions.num_format=4;positions.dst_sel_x=4;positions.dst_sel_y=5;positions.dst_sel_z=6;positions.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&positions,sizeof(positions));
        const auto depth=DescribeGuestDepthBuffer(db,regs.depth_view,gpu.last_db_extent);const auto layout=LayoutGuestTexture(depth);
        if(layout.guest_bytes!=4096)throw std::runtime_error("Cached depth GCN seed physical extent changed");
        // B starts distinct even when I remains clean after the real GPU clear.
        const float physical_depth=0.125f;
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)memcpy(memory+8192+GuestPixelOffset(depth,layout,0,0,x,y),&physical_depth,4);
        if(stencil)memset(memory+16384,53,1024);
        rasterizer.Draw(false);rasterizer.Finish();
        const float buffer_depth=dirty?0.25f:0.875f;
        if(dirty){for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)memcpy(memory+8192+GuestPixelOffset(depth,layout,0,0,x,y),&buffer_depth,4);
            if(stencil)memset(memory+16384,53,1024);}
        if(cmask){
            // Rebind the original depth words as RGBA8; only one CMASK tile
            // and a disjoint clipped guest draw may change those words.
            regs.depth_control={};regs.depth_render_control={};
            db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Invalid;
            cb={};cb.base_address=u32((base+8192)>>8);cb.info.format=cmask_format;cb.info.number_type=cmask_format==6?7:0;
            cb.attrib.tile_mode_index=AmdGpu::TileMode(5);cb.pitch.tile_max=3;cb.slice.tile_max=15;
            cb.info.fast_clear=1;cb.cmask_base_address=u32((base+36864)>>8);cb.cmask_slice.tile_max=15;
            cb.clear_word0=cmask_format==6?0x3c0u<<11:0xff00ff00u;gpu.last_cb_extent[0]={32,32};
            regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=32;
            regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=32;
            regs.generic_scissor.top_left_x=1;regs.generic_scissor.top_left_y=2;
            regs.generic_scissor.bottom_right_x=4;regs.generic_scissor.bottom_right_y=5;
            const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto cl=LayoutGuestTexture(color);
            if(cl.guest_bytes!=4096||GuestCmaskBytes(cb)!=2048)
                throw std::runtime_error("Native CMASK fixture extent changed");
            memset(memory+36864,0xff,2048);
            // Independent P8_32x32_16x16 address for tile(x=8,y=0):
            // pipe1, element0 => byte256, low nibble. No planner oracle.
            memory[36864+256]=std::byte{0xf0};
            std::vector<std::byte> physical(backing,backing+65536);physical[36864+256]=std::byte{0xff};
            rasterizer.Draw(false);rasterizer.Finish();
            if(memcmp(backing,physical.data(),65536))
                throw std::runtime_error("Native CMASK clear published copied I into physical B or changed metadata padding");
            // ReadMemory(B) deliberately does not publish independent I. Use
            // actual guest image_loads of a bit-compatible UINT word view.
            std::vector<std::pair<u32,u32>> points{{0,0},{1,2},{3,4},{4,5},{31,31}};
            for(u32 y=0;y<32;y+=8)for(u32 x=0;x<32;x+=8)points.emplace_back(x+4,y+4);
            alignas(256) std::array<u32,1024> code{};std::vector<u32> words;
            for(u32 i=0;i<points.size();++i){const auto [x,y]=points[i];
                words.insert(words.end(),{0x7e0002ff,x,0x7e0202ff,y,cmask_format==6?0xf0000700u:0xf0000100u,0x00000400,0xbf8c0000});
                for(u32 c=0;c<(cmask_format==6?3u:1u);++c)
                    words.insert(words.end(),{0xe0700000u+i*16+c*4,0x80020400u+(c<<8)});}
            words.push_back(0xbf810000);
            if(words.size()>=512)throw std::runtime_error("CMASK probe overlaps shader metadata");
            std::copy(words.begin(),words.end(),code.begin());AmdGpu::BinaryInfo binary{};
            binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=0x434d41534b505201ULL;
            memcpy(code.data()+512,&binary,sizeof(binary));
            AmdGpu::Image image{};image.base_address=(base+8192)>>8;image.type=u64(AmdGpu::ImageType::Color2D);
            image.width=image.height=image.pitch=31;image.data_format=cmask_format==6?6:4;image.num_format=cmask_format==6?7:4;image.tiling_index=5;
            image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
            AmdGpu::Buffer observation{};observation.base_address=base+57344;observation.stride=4;observation.num_records=1024;
            observation.data_format=4;observation.num_format=4;observation.dst_sel_x=4;observation.dst_sel_y=5;
            observation.dst_sel_z=6;observation.dst_sel_w=7;
            compute={};compute.address=reinterpret_cast<u64>(code.data())>>8;
            compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;compute.dim_x=compute.dim_y=compute.dim_z=1;
            compute.settings.num_user_regs=12;compute.settings.num_vgprs=3;compute.settings.num_sgprs=3;
            memcpy(compute.user_data.data(),&image,sizeof(image));memcpy(compute.user_data.data()+8,&observation,sizeof(observation));
            auto expected=physical;
            for(u32 i=0;i<points.size();++i){const auto [x,y]=points[i];
                const bool draw=x>=1&&x<4&&y>=2&&y<5,clear=x>=8&&x<16&&y<8;
                if(cmask_format==6){
                    // Independent unsigned mini-float arithmetic; no production
                    // decoder or retile helper participates in this oracle.
                    const auto uf=[](u32 word,u32 mantissa){const auto exponent=word>>mantissa;
                        const auto m=word&((1u<<mantissa)-1);return exponent?
                            std::ldexp(float((1u<<mantissa)|m),int(exponent)-15-int(mantissa)):
                            std::ldexp(float(m),-14-int(mantissa));};
                    const u32 old=std::bit_cast<u32>(0.875f);
                    const float values[]{draw?1.f:0.f,clear?1.f:0.f,0.f};
                    for(u32 c=0;c<3;++c){const auto f=draw||clear?values[c]:
                        uf(c==0?old&0x7ff:c==1?(old>>11)&0x7ff:old>>22,c==2?5:6);
                        const auto bits=std::bit_cast<u32>(f);memcpy(expected.data()+57344+i*16+c*4,&bits,4);}
                }else{const u32 word=draw?0xff0000ffu:clear?0xff00ff00u:std::bit_cast<u32>(0.875f);
                    memcpy(expected.data()+57344+i*16,&word,4);}
            }
            dispatch();rasterizer.ReadMemory(observation.base_address,4096);rasterizer.Finish();
            if(memcmp(backing,expected.data(),65536)){
                for(u32 i=0;i<points.size();++i){u32 actual{},want{};memcpy(&actual,backing+57344+i*16,4);
                    memcpy(&want,expected.data()+57344+i*16,4);if(actual!=want)fprintf(stderr,
                    "CMASK probe(%u,%u) actual=%08x expected=%08x\n",points[i].first,points[i].second,actual,want);}
                for(size_t i=0;i<expected.size();++i)if(backing[i]!=expected[i]){fprintf(stderr,
                    "Native CMASK mismatch byte=%zu actual=%02x expected=%02x S=%u HTILE=%u Dirty=%u\n",i,
                    unsigned(backing[i]),unsigned(expected[i]),unsigned(stencil),unsigned(htile),unsigned(dirty));break;}
                throw std::runtime_error("Native depth/CMASK/draw lost independent I/B, source S or full physical padding");
            }
            ++cases;continue;
        }
        AmdGpu::Image image{};image.base_address=(base+8192)>>8;image.type=u64(AmdGpu::ImageType::Color2D);
        image.width=image.height=image.pitch=31;image.data_format=4;image.num_format=7;image.tiling_index=5;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        AmdGpu::Buffer input{};input.base_address=base+8192;input.stride=4;input.num_records=32768/4;
        input.data_format=4;input.num_format=7;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        auto observation=input;observation.base_address=base+57344;observation.num_records=1024;observation.num_format=4;
        const auto bind=[&](const auto& code){auto& cs=compute;cs={};cs.address=reinterpret_cast<u64>(code.data())>>8;
            cs.num_thread_x.full=cs.num_thread_y.full=cs.num_thread_z.full=1;cs.dim_x=cs.dim_y=cs.dim_z=1;
            cs.settings.num_user_regs=16;cs.settings.num_vgprs=3;cs.settings.num_sgprs=3;
            memcpy(cs.user_data.data(),&input,sizeof(input));memcpy(cs.user_data.data()+4,&image,sizeof(image));
            memcpy(cs.user_data.data()+12,&observation,sizeof(observation));};
        std::vector<std::byte> expected(backing,backing+65536);
        const auto check=[&](const char* phase){rasterizer.ReadMemory(observation.base_address,4096);rasterizer.Finish();
            if(memcmp(backing,expected.data(),65536)){
                for(size_t i=0;i<expected.size();++i)if(backing[i]!=expected[i]){fprintf(stderr,"Cached storage %s first mismatch=%zu actual=%02x expected=%02x S=%u HTILE=%u\n",phase,i,std::to_integer<unsigned>(backing[i]),std::to_integer<unsigned>(expected[i]),unsigned(stencil),unsigned(htile));break;}
                throw std::runtime_error("Actual GCN cached-depth storage lost I/B/source-S/physical padding contract");}};
        const u32 initial=std::bit_cast<u32>(buffer_depth);memcpy(expected.data()+57344,&initial,4);
        if(ordered){
            regs.depth_control={};db.z_info.format=AmdGpu::DepthBuffer::ZFormat::Invalid;
            regs.depth_render_control={};
            regs.vs_program.address=reinterpret_cast<u64>(ordered_vertex.data())>>8;
            regs.vs_program.settings.num_user_regs=12;regs.vs_program.settings.num_vgprs=3;
            memcpy(regs.vs_program.user_data.data(),&positions,sizeof(positions));
            memcpy(regs.vs_program.user_data.data()+4,&image,sizeof(image));
            regs.ps_program.address=reinterpret_cast<u64>(ordered_pixel.data())>>8;
            regs.ps_program.settings.num_user_regs=8;regs.ps_program.settings.num_vgprs=3;
            memcpy(regs.ps_program.user_data.data(),&input,sizeof(input));
            memcpy(regs.ps_program.user_data.data()+4,&observation,sizeof(observation));
            const auto color=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto cl=LayoutGuestTexture(color);
            const u32 red=0xff0000ff;
            for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)memcpy(expected.data()+4096+GuestPixelOffset(color,cl,0,0,x,y),&red,4);
            rasterizer.Draw(false);rasterizer.ReadMemory(base+4096,4096);
        }else{bind(writer);dispatch();}
        check("formatted-before-image");
        const u32 values[]{initial,std::bit_cast<u32>(0.5f),std::bit_cast<u32>(0.875f)};
        memcpy(expected.data()+57344,values,sizeof(values));bind(probe);dispatch();check("raw-B-and-native-I");++cases;
    }
    if(cases!=8)throw std::runtime_error("Cached depth storage GCN coverage missing");
}
} // namespace NativeDX12
