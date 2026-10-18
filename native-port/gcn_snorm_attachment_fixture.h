// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_graphics_state.h"
namespace NativeDX12 {
template<class Program,class Initialize>
void RunSnormAttachmentFixture(GuestRasterizer& r,AmdGpu::Liverpool& gpu,
    const Program& vertex,Initialize initialize,std::byte* memory,std::byte* backing,u64 base){
    struct Payload{std::array<float,4> value;std::array<s32,4> code;};
    // Independent authored codes; not computed using the production quantizer.
    const Payload payloads[]{
        {{{-2.f,-.25f,.5f,2.f}},{{-511,-128,256,1}}},
        {{{0.f,.25f,1.f/511.f,-.5f}},{{0,128,1,0}}},
        {{{1.f,-1.f,0.f,-1.f}},{{511,-511,0,-1}}}};
    alignas(256) std::array<std::array<u32,128>,3> pixels{};
    for(u32 i=0;i<3;++i){const auto& v=payloads[i].value;
        initialize(pixels[i],{0xBEEB03FF,15,0x7e0002ff,std::bit_cast<u32>(v[0]),0x7e0202ff,std::bit_cast<u32>(v[1]),
            0x7e0402ff,std::bit_cast<u32>(v[2]),0x7e0602ff,std::bit_cast<u32>(v[3]),0xf800000f,0x03020100,0xbf810000},
            0x534e4f524d525430ull+i);
    }
    const auto pack=[](const std::array<s32,4>& c){return(u32(c[0])&1023)|((u32(c[1])&1023)<<10)|
        ((u32(c[2])&1023)<<20)|((u32(c[3])&3)<<30);};
    unsigned cases{};
    for(u32 log=0;log<4;++log)for(u32 mask:{15u,1u,10u})for(bool logic:{false,true}){
        r.ReadMemory(base,65536);r.Finish();r.UnmapMemory(base,65536);r.MapMemory(base,65536);
        r.NotifyHostMapping(base,65536,true,true);memset(memory,0xa7,65536);
        const float xy[3][2]{{-1,-1},{3,-1},{-1,3}};
        for(unsigned i=0;i<3;++i){const float position[]{xy[i][0],xy[i][1],.5f,1.f};memcpy(memory+i*32,position,sizeof(position));}
        auto& regs=gpu.regs;regs={};regs.stage_enable.raw=AmdGpu::ShaderStageEnable::Vs;
        regs.primitive_type=AmdGpu::PrimitiveType::TriangleList;regs.num_indices=3;
        regs.vs_program.address=reinterpret_cast<u64>(vertex.data())>>8;
        regs.vs_program.settings.num_user_regs=4;regs.vs_program.settings.num_vgprs=1;
        regs.ps_program.settings.num_vgprs=1;
        regs.color_export_format.raw=u32(AmdGpu::ShaderExportFormat::ABGR_32);
        regs.color_control.mode=AmdGpu::ColorControl::OperationMode::Normal;
        regs.color_control.rop3=logic?AmdGpu::ColorControl::LogicOp::Xor:AmdGpu::ColorControl::LogicOp::Copy;
        regs.color_shader_mask.raw=15;regs.color_target_mask.raw=mask;
        regs.clipper_control.clip_space=AmdGpu::ClipSpace::ZeroToW;
        auto& vp=regs.viewport_control;vp.xscale_enable=vp.yscale_enable=vp.xoffset_enable=vp.yoffset_enable=vp.zscale_enable=1;
        regs.viewports[0].xscale=4;regs.viewports[0].yscale=-4;regs.viewports[0].xoffset=regs.viewports[0].yoffset=4;regs.viewports[0].zscale=1;
        regs.screen_scissor.bottom_right_x=regs.window_scissor.bottom_right_x=regs.generic_scissor.bottom_right_x=8;
        regs.screen_scissor.bottom_right_y=regs.window_scissor.bottom_right_y=regs.generic_scissor.bottom_right_y=8;
        regs.aa_config.msaa_num_samples=log;
        AmdGpu::Buffer input{};input.base_address=base;input.stride=32;input.num_records=3;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        memcpy(regs.vs_program.user_data.data(),&input,sizeof(input));
        auto& cb=regs.color_buffers[0];cb.base_address=u32((base+4096)>>8);cb.info.format=9;cb.info.number_type=1;
        cb.attrib.tile_mode_index=AmdGpu::TileMode::Thin1DThin;cb.attrib.num_fragments_log2=log;
        cb.pitch.tile_max=cb.slice.tile_max=1;cb.view.slice_start=cb.view.slice_max=1;gpu.last_cb_extent[0]={8,8};
        const auto desc=DescribeGuestColorBuffer(cb,gpu.last_cb_extent[0]);const auto layout=LayoutGuestTexture(desc);
        if(layout.format!=DXGI_FORMAT_R10G10B10A2_UINT||layout.HostBytesPerPixel()!=4||layout.guest_bytes>16384)
            throw std::runtime_error("SNORM fixture requires exact integer packed physical storage");
        const auto seed=pack({-512,-512,-19,-2});
        for(u32 sample=0;sample<desc.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x)
            memcpy(memory+4096+GuestPixelOffset(desc,layout,0,1,x,y,sample),&seed,4);
        std::vector<std::byte> expected(backing,backing+65536);
        u32 bitmask{};for(u32 channel=0;channel<4;++channel)if(mask&(1u<<channel))bitmask|=(channel==3?3u:1023u)<<(channel*10);
        // Retain exact negative endpoint codes in masked channels through warm
        // draw/export/readback. Check all samples, untouched layer0 and padding.
        for(u32 draw=0;draw<3;++draw){
            regs.ps_program.address=reinterpret_cast<u64>(pixels[draw].data())>>8;
            const auto source=pack(payloads[draw].code);
            for(u32 sample=0;sample<desc.samples;++sample)for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){
                const auto offset=4096+GuestPixelOffset(desc,layout,0,1,x,y,sample);u32 old;memcpy(&old,expected.data()+offset,4);
                const auto result=(old&~bitmask)|((logic?(source^old):source)&bitmask);memcpy(expected.data()+offset,&result,4);
            }
            r.Draw(false);r.ReadMemory(desc.address,layout.guest_bytes);r.Finish();r.ValidateDebugMessages();
            if(memcmp(backing,expected.data(),expected.size())){
                for(size_t i=0;i<expected.size();++i)if(backing[i]!=expected[i]){
                    fprintf(stderr,"SNORM samples=%u mask=%u logic=%u draw=%u byte=%zu actual=%02x expected=%02x\n",desc.samples,mask,unsigned(logic),draw,i,unsigned(backing[i]),unsigned(expected[i]));break;}
                throw std::runtime_error("SNORM packed export lost quantization, masked raw codes, logic ordering or padding");
            }
        }
        if(!logic){
            const auto formats=std::array{DXGI_FORMAT_R10G10B10A2_UINT};regs.blend_control[0].enable=1;
            bool rejected=false;try{(void)BuildGuestGraphicsState(regs,formats);}catch(const std::runtime_error& error){
                rejected=std::string_view(error.what()).find("SNORM blending")!=std::string_view::npos;
            }
            if(!rejected)throw std::runtime_error("SNORM blend accepted without post-blend normalized quantization");
            regs.blend_control[0].enable=0;
        }
        ++cases;
    }
    printf("PASS:%u actual GCN SNORM packed10 sample1/2/4/8,three payloads,clamp/nearest-even,masked endpoint bits,XOR,layer and full65536 padding;blend remains rejected\n",cases);
}
} // namespace NativeDX12
