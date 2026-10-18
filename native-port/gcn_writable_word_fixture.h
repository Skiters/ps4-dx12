// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gcn_readonly_numeric_fixture.h"

namespace NativeDX12 {
// Real T# loads, image_load/image_store and S_BARRIER through Liverpool/GCN.
// Independent endpoint constants check the shared word BEFORE and AFTER store.
inline void RunWritableWordGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base,bool cached_seed=false,u32 scalar_number=4){
    constexpr u32 OldWord=0xc00805ff,NewWord=0x5ff00200,SeedWord=0x4007fe00;
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
        scalar.data_format=4;scalar.num_format=scalar_number;scalar.tiling_index=5;
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
        auto input=output;input.base_address=base+ImageOffset;input.num_records=layout.guest_bytes/4;
        const auto dispatch=[&](const std::vector<u32>& words,u64 hash){
            if(words.size()>192)throw std::runtime_error("Writable word shader overlaps metadata");
            code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
            AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
            memcpy(code.words.data()+192,&binary,sizeof(binary));
            AmdGpu::ComputeProgram compute{};compute.address=reinterpret_cast<u64>(code.words.data())>>8;
            compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
            compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=12;
            compute.settings.num_vgprs=5;compute.settings.num_sgprs=3;
            const u64 pointer=reinterpret_cast<u64>(table.data());memcpy(compute.user_data.data(),&pointer,8);
            memcpy(compute.user_data.data()+4,&output,sizeof(output));memcpy(compute.user_data.data()+8,&input,sizeof(input));
            constexpr u32 count=sizeof(compute)/4;
            std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,count,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
            const auto registers=std::bit_cast<std::array<u32,count>>(compute);packets.insert(packets.end(),registers.begin(),registers.end());
            packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
            gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
        };
        if(cached_seed){
            // Establish B before I, then create ordinary R32 I WITHOUT a raw
            // alias hint. A real changed store makes only I contain SeedWord.
            const auto image_input=input;input.base_address=base;input.num_records=Bytes/4;
            dispatch({0xe0300000,0x80020c00,0xbf8c0000,0xe0700040,0x80010c00,0xbf810000},0x5752434143484201ull);
            input=image_input;const u32 arena_word=0xa7a7a7a7;
            memcpy(expected.data()+OutputOffset+64,&arena_word,4);
            std::vector<u32> seed{0xc0c80100,0xc0cc0108,0xbf8c0000,
                0xe0300004,0x80021400,0xbf8c0000,0xe0700060,0x80011400,
                0x7e000281,0x7e020280};
            memcpy(expected.data()+OutputOffset+96,&OldWord,4);
            if(warm)seed.insert(seed.end(),{0x7e1802ff,SeedWord,0x7e1a0280,0x7e1c0280,0x7e1e02ff,SeedWord,
                0xf0200f00,write_operand|0x0c00,0xbf8c0000,0xbf8a0000});
            seed.insert(seed.end(),{0xf0000f00,write_operand|0x0400,0xbf8c0000});
            for(u32 channel=0;channel<4;++channel){seed.push_back(0xe0700050|channel*4);seed.push_back(0x80010000|((channel+4)<<8));}
            seed.push_back(0xbf810000);dispatch(seed,0x5752434143484900ull+warm+u64(reverse)*16+(u64(scalar_number)<<32));
            const u32 word=warm?SeedWord:OldWord;const u32 warm_channels[]{word,0,0,word};
            memcpy(expected.data()+OutputOffset+80,warm_channels,16);
        }
        std::vector<u32> words{0xc0c80100,0xc0cc0108,0xbf8c0000,0x7e000281,0x7e020280,
            0xf0000f00,read_operand|0x0400,0xbf8c0000,
            0x7e1802ff,NewWord,0x7e1a0280,0x7e1c0280,0x7e1e02ff,NewWord,
            0xf0200f00,write_operand|0x0c00,0xbf8c0000,0xbf8a0000,
            0xf0000f00,read_operand|0x0800,0xbf8c0000};
        for(u32 channel=0;channel<8;++channel){words.push_back(0xe0700000|channel*4);words.push_back(0x80010000|((channel+4)<<8));}
        if(cached_seed)words.insert(words.end(),{0xe0300004,0x80021400,0xbf8c0000,0xe0700020,0x80011400});
        words.push_back(0xbf810000);dispatch(words,0x57524f5244414c00ull+number+u64(reverse)*16+(u64(cached_seed)<<16)+(u64(scalar_number)<<32));
        const std::array<u32,4> before=cached_seed&&warm?(number==1?std::array<u32,4>{0xbf800000,0x3f800000,0,0x3f800000}:
            std::array<u32,4>{u32(-512),511,0,1}):number==1?std::array<u32,4>{0x3f800000,0xbf800000,0,0xbf800000}:
            std::array<u32,4>{511,u32(-511),0,u32(-1)};
        const std::array<u32,4> after=number==1?std::array<u32,4>{0xbf800000,0,0x3f800000,0x3f800000}:
            std::array<u32,4>{u32(-512),0,511,1};
        memcpy(expected.data()+OutputOffset,before.data(),16);memcpy(expected.data()+OutputOffset+16,after.data(),16);
        if(cached_seed)memcpy(expected.data()+OutputOffset+32,&OldWord,4);
        // The cached case has independent I/B. Pinned vk_rasterizer ReadMemory
        // downloads B only; image_store does not publish I into physical B.
        // NewWord in I is proven by the actual post-barrier image_load above.
        if(!cached_seed)memcpy(expected.data()+ImageOffset+GuestPixelOffset(desc,layout,0,0,1,0),&NewWord,4);
        // Observe shader output and the physical alias WITHOUT requesting a
        // separate host image-coherence operation on an exclusive native I.
        rasterizer.ReadMemory(cached_seed?output.base_address:base,cached_seed?output.GetSize():Bytes);
        rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(memcmp(backing,expected.data(),Bytes)){
            for(u32 word=0;word<9;++word){u32 got{},want{};memcpy(&got,backing+OutputOffset+word*4,4);memcpy(&want,expected.data()+OutputOffset+word*4,4);
                fprintf(stderr,"Writable output[%u]=%08x expected=%08x\n",word,got,want);}
            for(u32 byte=0;byte<Bytes;++byte)if(backing[byte]!=expected[byte]){
                fprintf(stderr,"Writable word number=%u scalar=%u reverse=%u warm=%u byte=%u got=%02x expected=%02x\n",
                    number,scalar_number,reverse,warm,byte,std::to_integer<unsigned>(backing[byte]),std::to_integer<unsigned>(expected[byte]));break;}
            throw std::runtime_error("Actual GCN writable word alias lost contents/barrier/padding");
        }
    }
    printf("SCALAR donor number=%u\n",scalar_number);
    puts(cached_seed?"PASS:8 actual GCN cached ordinary R32 I(GpuModified0/1) to signed packed10 writer, separate raw B, before/after barrier and full65536 bytes":
        "PASS:8 actual GCN signed packed10/R32 stores, before/after S_BARRIER, both descriptor orders, warm reuse and full65536 bytes");
}
} // namespace NativeDX12
