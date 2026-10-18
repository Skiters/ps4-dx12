// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gcn_readonly_numeric_fixture.h"

namespace NativeDX12 {
// Real T# loads, image_load/image_store and S_BARRIER through Liverpool/GCN.
// Independent endpoint constants check the shared word BEFORE and AFTER store.
inline void RunWritableWordGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base){
    constexpr u32 OldWord=0xc00805ff,NewWord=0x5ff00200;
    constexpr u32 ImageOffset=16384,OutputOffset=57344,Bytes=65536;
    struct alignas(256) Code{std::array<u32,256> words{};} code;
    alignas(256) std::array<AmdGpu::Image,2> table{};
    gpu.BindRasterizer(&rasterizer);
    struct Drain{GuestRasterizer& r;AmdGpu::Liverpool& g;
        ~Drain(){g.WaitGpuIdle();try{r.Finish();}catch(...){}g.BindRasterizer(nullptr);}} drain{rasterizer,gpu};
    for(u32 number:{1u,5u})for(bool reverse:{false,true})for(u32 warm=0;warm<2;++warm){
        rasterizer.ReadMemory(base,Bytes);rasterizer.Finish();
        rasterizer.UnmapMemory(base,Bytes);rasterizer.MapMemory(base,Bytes);
        rasterizer.NotifyHostMapping(base,Bytes,true,true);memset(memory,0xa7,Bytes);
        AmdGpu::Image scalar{};scalar.base_address=(base+ImageOffset)>>8;
        scalar.type=u64(AmdGpu::ImageType::Color2D);scalar.width=scalar.height=scalar.pitch=31;
        scalar.data_format=4;scalar.num_format=4;scalar.tiling_index=5;
        scalar.dst_sel_x=4;scalar.dst_sel_y=5;scalar.dst_sel_z=6;scalar.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(scalar);const auto layout=LayoutGuestTexture(desc);
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y),&OldWord,4);
        std::vector<std::byte> expected(memory,memory+Bytes);
        auto packed=scalar;packed.data_format=9;packed.num_format=number;
        table=reverse?std::array<AmdGpu::Image,2>{scalar,packed}:std::array<AmdGpu::Image,2>{packed,scalar};
        AmdGpu::Buffer output{};output.base_address=base+OutputOffset;output.stride=4;output.num_records=1024;
        output.data_format=4;output.num_format=4;output.dst_sel_x=4;output.dst_sel_y=5;output.dst_sel_z=6;output.dst_sel_w=7;
        const u32 read_operand=reverse?0x00060000:0x00040000,write_operand=reverse?0x00040000:0x00060000;
        std::vector<u32> words{0xc0c80100,0xc0cc0108,0xbf8c0000,0x7e000281,0x7e020280,
            0xf0000f00,read_operand|0x0400,0xbf8c0000,
            0x7e1802ff,NewWord,0x7e1a0280,0x7e1c0280,0x7e1e02ff,NewWord,
            0xf0200f00,write_operand|0x0c00,0xbf8c0000,0xbf8a0000,
            0xf0000f00,read_operand|0x0800,0xbf8c0000};
        for(u32 channel=0;channel<8;++channel){words.push_back(0xe0700000|channel*4);words.push_back(0x80010000|((channel+4)<<8));}
        words.push_back(0xbf810000);code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
        AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);
        binary.shader_hash=0x57524f5244414c00ull+number+u64(reverse)*16;
        memcpy(code.words.data()+192,&binary,sizeof(binary));
        AmdGpu::ComputeProgram compute{};compute.address=reinterpret_cast<u64>(code.words.data())>>8;
        compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
        compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=8;
        compute.settings.num_vgprs=3;compute.settings.num_sgprs=3;
        const u64 pointer=reinterpret_cast<u64>(table.data());memcpy(compute.user_data.data(),&pointer,8);
        memcpy(compute.user_data.data()+4,&output,sizeof(output));
        constexpr u32 count=sizeof(compute)/4;
        std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,count,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
        const auto registers=std::bit_cast<std::array<u32,count>>(compute);packets.insert(packets.end(),registers.begin(),registers.end());
        packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
        gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
        const std::array<u32,4> before=number==1?std::array<u32,4>{0x3f800000,0xbf800000,0,0xbf800000}:
            std::array<u32,4>{511,u32(-511),0,u32(-1)};
        const std::array<u32,4> after=number==1?std::array<u32,4>{0xbf800000,0,0x3f800000,0x3f800000}:
            std::array<u32,4>{u32(-512),0,511,1};
        memcpy(expected.data()+OutputOffset,before.data(),16);memcpy(expected.data()+OutputOffset+16,after.data(),16);
        memcpy(expected.data()+ImageOffset+GuestPixelOffset(desc,layout,0,0,1,0),&NewWord,4);
        rasterizer.ReadMemory(base,Bytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),Bytes)){
            for(u32 byte=0;byte<Bytes;++byte)if(backing[byte]!=expected[byte]){
                fprintf(stderr,"Writable word number=%u reverse=%u warm=%u byte=%u got=%02x expected=%02x\n",
                    number,reverse,warm,byte,std::to_integer<unsigned>(backing[byte]),std::to_integer<unsigned>(expected[byte]));break;}
            throw std::runtime_error("Actual GCN writable word alias lost contents/barrier/padding");
        }
    }
    puts("PASS:8 actual GCN signed packed10/R32 stores, before/after S_BARRIER, both descriptor orders, warm reuse and full65536 bytes");
}
} // namespace NativeDX12
