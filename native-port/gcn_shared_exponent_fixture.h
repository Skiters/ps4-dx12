// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gcn_readonly_numeric_fixture.h"

namespace NativeDX12 {
// Start with an ordinary readonly RGB9E5 I, then write through an actual word
// view. The GPU-modified seed must retain this original class, not recreate it
// from B. Expected colors/words are authored independently of production IR.
inline void RunSharedExponentGcnFixture(GuestRasterizer& r,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base,bool filtered=false,bool extremes=false,bool word_donor=false,u32 samples=1){
    constexpr u32 Bytes=65536,ImageOffset=16384,OutputOffset=57344;
    const u32 active_sample=samples-1;
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
        reader.type=u64(samples>1?AmdGpu::ImageType::Color2DMsaa:AmdGpu::ImageType::Color2D);
        if(samples>1)reader.last_level=std::countr_zero(samples);
        reader.width=reader.height=reader.pitch=31;
        reader.data_format=34;reader.num_format=7;reader.tiling_index=5;
        reader.dst_sel_x=4;reader.dst_sel_y=5;reader.dst_sel_z=6;reader.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(reader);const auto layout=LayoutGuestTexture(desc);
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)for(u32 sample=0;sample<samples;++sample)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y,sample),&Old,4);
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
        const auto dispatch=[&](const std::vector<u32>& words,u64 hash,int sample_index=-1){
            auto program=words;
            if(samples>1){const auto sample=sample_index<0?active_sample:u32(sample_index);
                for(size_t i=0;i<program.size();++i)if(program[i]==0xf0000f00||program[i]==0xf0200f00){
                    program.insert(program.begin()+i,{0x7e0402ff,sample});i+=3;
                }
                hash+=u64(samples)*0x100000000ull+u64(sample)*0x10000000000ull;
            }
            if(program.size()>192)throw std::runtime_error("Shared exponent shader overlaps metadata");
            code.words.fill(0);std::copy(program.begin(),program.end(),code.words.begin());
            AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(program.size()*4);binary.shader_hash=hash;
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
            dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0x7e1802ff,Seed,
                0x7e1a0280,0x7e1c0280,0x7e1e02ff,Seed,0xf0200f00,0x00040c00,0xbf810000},0x52474239574f5244);
            table[0]=reader;
        }
        dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xbf8c0000,
                  0xe0700040,0x80010400,0xe0700044,0x80010500,0xe0700048,0x80010600,0xe070004c,0x80010700,0xbf810000},0x5247423952454144);
        memcpy(expected.data()+OutputOffset+64,(word_donor?seed_color:old_color).data(),16);
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
        memcpy(expected.data()+OutputOffset,(modified||word_donor?seed_color:old_color).data(),16);
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
        if(samples>1)for(u32 sample=0;sample<samples;++sample){
            const u32 at=256+sample*16;
            dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xbf8c0000,
                0xe0700000|at,0x80010400,0xe0700000|(at+4),0x80010500,
                0xe0700000|(at+8),0x80010600,0xe0700000|(at+12),0x80010700,0xbf810000},0x5247423953414d50,sample);
            memcpy(expected.data()+OutputOffset+at,(sample==active_sample?final_color:old_color).data(),16);
        }
        if(samples>1){
            auto raw_reader=reader;raw_reader.data_format=4;raw_reader.num_format=4;table[0]=raw_reader;
            for(u32 sample=0;sample<samples;++sample){
                const u32 at=512+sample*16,value=sample==active_sample?Final:Old;
                dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xbf8c0000,
                    0xe0700000|at,0x80010400,0xe0700000|(at+4),0x80010500,
                    0xe0700000|(at+8),0x80010600,0xe0700000|(at+12),0x80010700,0xbf810000},0x5247423952415730,sample);
                const std::array<u32,4> raw{value,0,0,value}; // Pinned R32 alpha selects red.
                memcpy(expected.data()+OutputOffset+at,raw.data(),16);
            }
        }
        r.ReadMemory(output.base_address,output.GetSize());r.Finish();r.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),Bytes)){
            for(u32 c=0;c<9;++c){u32 got{},want{};memcpy(&got,backing+OutputOffset+c*4,4);memcpy(&want,expected.data()+OutputOffset+c*4,4);
                fprintf(stderr,"RGB9 output[%u]=%08x expected=%08x\n",c,got,want);}
            fprintf(stderr,"RGB9 writer=%u/%u reverse=%u modified=%u samples=%u active=%u\n",writer_class[0],writer_class[1],reverse,modified,samples,active_sample);
            throw std::runtime_error("Shared exponent GCN lost original I/B, decoded contents, barrier or padding");
        }
        ++cases;
    }
    printf("PASS:%u actual GCN %s donor, R32/packed writers, before/after %s, later SRV/all%u samples, separate B and full65536 bytes\n",cases,
        word_donor?"original written R32":"ordinary readonly RGB9E5",
        filtered?"bilinear decoded colors":extremes?"zero/minimum/maximum colors":"decoded colors",samples);
}
// Actual readonly multisample T# loads, including CPU dirty refresh and forced
// channels on identical guest code/hash. Each sample has an independent word.
inline void RunSharedExponentMsaaGcnFixture(GuestRasterizer& r,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base){
    constexpr u32 Bytes=65536,ImageOffset=16384,OutputOffset=57344;
    const std::array<u32,8> words{128u|(256u<<9)|(64u<<18)|(17u<<27),
        256u|(32u<<9)|(128u<<18)|(18u<<27),1u,0xffffffffu,
        256u|(128u<<9)|(64u<<18)|(19u<<27),32u|(64u<<9)|(128u<<18)|(17u<<27),1u<<9,1u<<18};
    const std::array<std::array<u32,4>,8> colors{{
        {0x3f800000,0x40000000,0x3f000000,0x3f800000},
        {0x40800000,0x3f000000,0x40000000,0x3f800000},
        {0x33800000,0,0,0x3f800000},
        {0x477f8000,0x477f8000,0x477f8000,0x3f800000},
        {0x41000000,0x40800000,0x40000000,0x3f800000},
        {0x3e800000,0x3f000000,0x3f800000,0x3f800000},
        {0,0x33800000,0,0x3f800000},{0,0,0x33800000,0x3f800000}}};
    struct alignas(256) Code{std::array<u32,256> data{};} code;
    alignas(256) AmdGpu::Image image;
    gpu.BindRasterizer(&r);
    struct Drain{GuestRasterizer& r;AmdGpu::Liverpool& g;
        ~Drain(){g.WaitGpuIdle();try{r.Finish();}catch(...){}g.BindRasterizer(nullptr);}} drain{r,gpu};
    unsigned cases{};
    for(u32 samples:{2u,4u,8u})for(u32 tile:{5u,13u})for(bool forced:{false,true}){
        r.ReadMemory(base,Bytes);r.Finish();r.UnmapMemory(base,Bytes);r.MapMemory(base,Bytes);
        r.NotifyHostMapping(base,Bytes,true,true);memset(memory,0xa7,Bytes);
        image={};image.base_address=(base+ImageOffset)>>8;
        image.type=u64(AmdGpu::ImageType::Color2DMsaa);image.last_level=std::countr_zero(samples);
        image.width=image.height=image.pitch=31;image.data_format=34;image.num_format=7;
        image.tiling_index=tile;
        image.dst_sel_x=forced?7:4;image.dst_sel_y=forced?0:5;
        image.dst_sel_z=forced?1:6;image.dst_sel_w=forced?4:7;
        const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
        if(desc.samples!=samples||layout.guest_bytes>OutputOffset-ImageOffset)
            throw std::runtime_error("Shared exponent MSAA fixture geometry exceeds physical allocation");
        AmdGpu::Buffer output{};output.base_address=base+OutputOffset;output.stride=4;output.num_records=1024;
        output.data_format=4;output.num_format=4;output.dst_sel_x=4;output.dst_sel_y=5;output.dst_sel_z=6;output.dst_sel_w=7;
        const auto dispatch=[&](std::vector<u32> shader,u64 hash,bool raw=false){
            code.data.fill(0);std::copy(shader.begin(),shader.end(),code.data.begin());
            AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(shader.size()*4);binary.shader_hash=hash;
            memcpy(code.data.data()+192,&binary,sizeof(binary));
            AmdGpu::ComputeProgram compute{};compute.address=reinterpret_cast<u64>(code.data.data())>>8;
            compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
            compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=12;
            compute.settings.num_vgprs=5;compute.settings.num_sgprs=3;
            const u64 pointer=reinterpret_cast<u64>(&image);memcpy(compute.user_data.data(),&pointer,8);
            memcpy(compute.user_data.data()+4,&output,sizeof(output));
            if(raw){auto input=output;input.base_address=base;input.num_records=Bytes/4;
                memcpy(compute.user_data.data()+8,&input,sizeof(input));}
            constexpr u32 count=sizeof(compute)/4;
            std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,count,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
            const auto regs=std::bit_cast<std::array<u32,count>>(compute);packets.insert(packets.end(),regs.begin(),regs.end());
            packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
            gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
        };
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)for(u32 s=0;s<samples;++s)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y,s),&words[s],4);
        std::vector<std::byte> expected(memory,memory+Bytes);
        dispatch({0xe0300000,0x80020c00,0xbf8c0000,0xe07000a0,0x80010c00,0xbf810000},0x5247394d42554600,true);
        const u32 fill=0xa7a7a7a7;memcpy(expected.data()+OutputOffset+160,&fill,4);
        for(u32 generation=0;generation<2;++generation){
            if(generation)for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)for(u32 s=0;s<samples;++s){
                const auto value=words[(s+1)%8];const auto offset=ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y,s);
                memcpy(memory+offset,&value,4);memcpy(expected.data()+offset,&value,4);
            }
            for(u32 sample=0;sample<samples;++sample){
                dispatch({0xc0c80100,0xbf8c0000,0x7e000281,0x7e020280,0x7e0402ff,sample,
                    0xf0000f00,0x00040400,0xbf8c0000,0xe0700000,0x80010400,
                    0xe0700004,0x80010500,0xe0700008,0x80010600,0xe070000c,0x80010700,0xbf810000},0x5247394d4c4f4100ull+sample);
                const auto& c=colors[(sample+generation)%8];
                const auto selected=forced?std::array<u32,4>{c[3],0,0x3f800000,c[0]}:c;
                memcpy(expected.data()+OutputOffset,selected.data(),16);
                r.ReadMemory(output.base_address,output.GetSize());r.Finish();r.ValidateDebugMessages();
                if(memcmp(backing,expected.data(),Bytes)){
                    const auto* actual=reinterpret_cast<const u32*>(backing+OutputOffset);
                    fprintf(stderr,"RGB9 MSAA samples=%u tile=%u forced=%u generation=%u sample=%u got=%08x/%08x/%08x/%08x expected=%08x/%08x/%08x/%08x\n",
                        samples,tile,forced,generation,sample,actual[0],actual[1],actual[2],actual[3],selected[0],selected[1],selected[2],selected[3]);
                    throw std::runtime_error("Shared exponent MSAA load lost sample, swizzle, CPU refresh or physical padding");
                }
                ++cases;
            }
        }
    }
    printf("PASS:%u actual queued RGB9E5 MSAA loads,2/4/8 distinct samples,2 tiles,forced swizzle,CPU dirty refresh and full65536 bytes\n",cases);
}
} // namespace NativeDX12
