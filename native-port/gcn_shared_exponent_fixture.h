// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gcn_readonly_numeric_fixture.h"

namespace NativeDX12 {
// Start with an ordinary readonly RGB9E5 I, then write through an actual word
// view. The GPU-modified seed must retain this original class, not recreate it
// from B. Expected colors/words are authored independently of production IR.
inline void RunSharedExponentGcnFixture(GuestRasterizer& r,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base,bool filtered=false,bool extremes=false,bool word_donor=false){
    constexpr u32 Bytes=65536,ImageOffset=16384,OutputOffset=57344;
    const u32 Old=extremes?0:128u|(256u<<9)|(64u<<18)|(17u<<27); // 1,2,0.5
    const u32 Seed=extremes?1:256u|(32u<<9)|(128u<<18)|(18u<<27); // 4,0.5,2
    const u32 Final=extremes?0xffffffff:1u|(2u<<9)|(4u<<18)|(24u<<27); // 1,2,4
    const std::array<u32,4> old_color=extremes?std::array<u32,4>{0,0,0,0x3f800000}:
        std::array<u32,4>{0x3f800000,0x40000000,0x3f000000,0x3f800000};
    const std::array<u32,4> seed_color=extremes?std::array<u32,4>{0x33800000,0,0,0x3f800000}:
        std::array<u32,4>{0x40800000,0x3f000000,0x40000000,0x3f800000};
    const std::array<u32,4> final_color=extremes?std::array<u32,4>{0x477f8000,0x477f8000,0x477f8000,0x3f800000}:
        std::array<u32,4>{0x3f800000,0x40000000,0x40800000,0x3f800000};
    struct alignas(256) Code{std::array<u32,256> words{};} code;
    alignas(256) std::array<AmdGpu::Image,2> table;
    gpu.BindRasterizer(&r);
    struct Drain{GuestRasterizer& r;AmdGpu::Liverpool& g;
        ~Drain(){g.WaitGpuIdle();try{r.Finish();}catch(...){}g.BindRasterizer(nullptr);}} drain{r,gpu};
    unsigned cases{};
    for(const auto writer_class:{std::array<u32,2>{4,4},std::array<u32,2>{4,5},std::array<u32,2>{4,7},
                                 std::array<u32,2>{9,1},std::array<u32,2>{9,5}})
    for(bool reverse:{false,true})for(bool modified:{false,true}){
        r.ReadMemory(base,Bytes);r.Finish();r.UnmapMemory(base,Bytes);r.MapMemory(base,Bytes);
        r.NotifyHostMapping(base,Bytes,true,true);memset(memory,0xa7,Bytes);
        AmdGpu::Image reader{};reader.base_address=(base+ImageOffset)>>8;
        reader.type=u64(AmdGpu::ImageType::Color2D);reader.width=reader.height=reader.pitch=31;
        reader.data_format=34;reader.num_format=7;reader.tiling_index=5;
        reader.dst_sel_x=4;reader.dst_sel_y=5;reader.dst_sel_z=6;reader.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(reader);const auto layout=LayoutGuestTexture(desc);
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y),&Old,4);
        if(filtered){
            // Four different per-texel exponents. At (.75,1.25), weights are
            // 3/16,1/16,9/16,3/16; averaging packed words cannot produce this.
            const u32 zero=0,lower_left=256u|(128u<<9)|(64u<<18)|(19u<<27);
            const u32 lower_right=32u|(64u<<9)|(128u<<18)|(17u<<27);
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,0,0),&zero,4);
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,0,1),&lower_left,4);
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,1,1),&lower_right,4);
        }
        std::vector<std::byte> expected(memory,memory+Bytes);
        auto writer=reader;writer.data_format=writer_class[0];writer.num_format=writer_class[1];
        AmdGpu::Buffer output{};output.base_address=base+OutputOffset;output.stride=4;output.num_records=1024;
        output.data_format=4;output.num_format=4;output.dst_sel_x=4;output.dst_sel_y=5;output.dst_sel_z=6;output.dst_sel_w=7;
        auto input=output;input.base_address=base;input.num_records=Bytes/4;
        const auto dispatch=[&](const std::vector<u32>& words,u64 hash){
            if(words.size()>192)throw std::runtime_error("Shared exponent shader overlaps metadata");
            code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
            AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
            memcpy(code.words.data()+192,&binary,sizeof(binary));
            AmdGpu::ComputeProgram compute{};compute.address=reinterpret_cast<u64>(code.words.data())>>8;
            compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
            compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=12;
            compute.settings.num_vgprs=5;compute.settings.num_sgprs=4;
            const u64 pointer=reinterpret_cast<u64>(table.data());memcpy(compute.user_data.data(),&pointer,8);
            memcpy(compute.user_data.data()+4,&output,sizeof(output));memcpy(compute.user_data.data()+8,&input,sizeof(input));
            constexpr u32 count=sizeof(compute)/4;
            std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,count,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
            const auto regs=std::bit_cast<std::array<u32,count>>(compute);packets.insert(packets.end(),regs.begin(),regs.end());
            packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
            gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
        };
        // B exists independently before the ordinary readonly I is created.
        dispatch({0xe0300000,0x80020c00,0xbf8c0000,0xe07000a0,0x80010c00,0xbf810000},0x5247423942554600);
        const u32 fill=0xa7a7a7a7;memcpy(expected.data()+OutputOffset+160,&fill,4);
        input.base_address=base+ImageOffset;input.num_records=layout.guest_bytes/4;table={reader,reader};
        if(word_donor){
            auto initial_writer=reader;initial_writer.data_format=4;initial_writer.num_format=4;table[0]=initial_writer;
            // Original R32 writer I must subsequently expose a typed RGB9E5
            // SRV without requiring a nonexistent typed RGB9E5 UAV.
            dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0x7e1802ff,Old,
                0x7e1a0280,0x7e1c0280,0x7e1e02ff,Old,0xf0200f00,0x00040c00,0xbf810000},0x52474239574f5244);
            table[0]=reader;
        }
        dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xbf8c0000,
                  0xe0700040,0x80010400,0xe0700044,0x80010500,0xe0700048,0x80010600,0xe070004c,0x80010700,0xbf810000},0x5247423952454144);
        memcpy(expected.data()+OutputOffset+64,old_color.data(),16);
        if(modified){
            auto seed_writer=reader;seed_writer.data_format=4;seed_writer.num_format=4;table[0]=seed_writer;
            dispatch({0xc0c80100,0xbf8c0000,0xe0300004,0x80021400,0xbf8c0000,0xe0700060,0x80011400,
                0x7e000281,0x7e020280,0x7e1802ff,Seed,0x7e1a0280,0x7e1c0280,0x7e1e02ff,Seed,
                0xf0200f00,0x00040c00,0xbf810000},0x5247423953454544);
            memcpy(expected.data()+OutputOffset+96,&Old,4);
        }
        table=reverse?std::array<AmdGpu::Image,2>{writer,reader}:std::array<AmdGpu::Image,2>{reader,writer};
        const u32 read=reverse?0x00060000:0x00040000,write=reverse?0x00040000:0x00060000;
        const auto values=writer_class[0]==4?std::array<u32,4>{Final,0,0,Final}:
            writer_class[1]==1?std::array<u32,4>{std::bit_cast<u32>((extremes?-1.f:1.f)/511.f),std::bit_cast<u32>((extremes?-1.f:1.f)/511.f),std::bit_cast<u32>((extremes?-1.f:1.f)/511.f),0xbf800000}:
                std::array<u32,4>{u32(extremes?-1:1),u32(extremes?-1:1),u32(extremes?-1:1),u32(-1)};
        std::vector<u32> words{0xc0c80100,0xc0cc0108,0xbf8c0000};
        if(filtered){AmdGpu::Sampler sampler{};
            sampler.clamp_x.Assign(AmdGpu::ClampMode::ClampLastTexel);
            sampler.clamp_y.Assign(AmdGpu::ClampMode::ClampLastTexel);
            sampler.clamp_z.Assign(AmdGpu::ClampMode::ClampLastTexel);
            sampler.xy_mag_filter.Assign(AmdGpu::Filter::Bilinear);
            sampler.xy_min_filter.Assign(AmdGpu::Filter::Bilinear);
            sampler.mip_filter.Assign(AmdGpu::MipFilter::Point);
            words.insert(words.end(),{0xbea003ff,u32(sampler.raw0),0xbea103ff,u32(sampler.raw0>>32),
                0xbea203ff,u32(sampler.raw1),0xbea303ff,u32(sampler.raw1>>32)});}
        const auto read_image=[&](u32 destination){
            if(filtered)words.insert(words.end(),{0x7e0002ff,std::bit_cast<u32>(0.75f/32),
                0x7e0202ff,std::bit_cast<u32>(1.25f/32),0x7e040280,0xf09c0f00,read|destination|0x01000000});
            else words.insert(words.end(),{0x7e000281,0x7e020280,0xf0000f00,read|destination});
        };
        read_image(0x0400);
        words.insert(words.end(),{0xbf8c0000,0x7e000281,0x7e020280,0x7e1802ff,values[0],0x7e1a02ff,values[1],
            0x7e1c02ff,values[2],0x7e1e02ff,values[3],0xf0200f00,write|0x0c00,0xbf8c0000,0xbf8a0000,
        });
        read_image(0x0800);words.push_back(0xbf8c0000);
        for(u32 c=0;c<8;++c){words.push_back(0xe0700000|c*4);words.push_back(0x80010000|((c+4)<<8));}
        words.insert(words.end(),{0xe0300004,0x80021400,0xbf8c0000,0xe0700020,0x80011400,0xbf810000});
        dispatch(words,0x5247423950524f00ull+writer_class[0]+u64(writer_class[1])*64+u64(reverse)*1024+u64(modified)*2048+u64(filtered)*4096+u64(extremes)*8192+u64(word_donor)*16384);
        memcpy(expected.data()+OutputOffset,(modified?seed_color:old_color).data(),16);
        memcpy(expected.data()+OutputOffset+16,final_color.data(),16);memcpy(expected.data()+OutputOffset+32,&Old,4);
        if(filtered){
            const std::array<float,4> before=modified?std::array<float,4>{4.796875f,2.375f,1.4375f,1.f}:
                std::array<float,4>{4.609375f,2.46875f,1.34375f,1.f};
            const std::array<float,4> after{4.609375f,2.46875f,1.5625f,1.f};
            memcpy(expected.data()+OutputOffset,before.data(),16);memcpy(expected.data()+OutputOffset+16,after.data(),16);
        }
        // A later independent shader has no writer and must use the ordinary
        // typed SRV on the same current native I after the raw UAV store.
        table[0]=reader;
        dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xbf8c0000,
            0xe0700070,0x80010400,0xe0700074,0x80010500,0xe0700078,0x80010600,0xe070007c,0x80010700,0xbf810000},0x52474239504f5354);
        memcpy(expected.data()+OutputOffset+112,final_color.data(),16);
        r.ReadMemory(output.base_address,output.GetSize());r.Finish();r.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),Bytes)){
            for(u32 c=0;c<9;++c){u32 got{},want{};memcpy(&got,backing+OutputOffset+c*4,4);memcpy(&want,expected.data()+OutputOffset+c*4,4);
                fprintf(stderr,"RGB9 output[%u]=%08x expected=%08x\n",c,got,want);}
            fprintf(stderr,"RGB9 writer=%u/%u reverse=%u modified=%u\n",writer_class[0],writer_class[1],reverse,modified);
            throw std::runtime_error("Shared exponent GCN lost original I/B, decoded contents, barrier or padding");
        }
        ++cases;
    }
    printf("PASS:%u actual GCN %s donor, R32/packed writers, before/after %s, later typed SRV, separate B and full65536 bytes\n",cases,
        word_donor?"original written R32":"ordinary readonly RGB9E5",
        filtered?"bilinear decoded colors":extremes?"zero/minimum/maximum colors":"decoded colors");
}
} // namespace NativeDX12
