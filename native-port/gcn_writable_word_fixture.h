// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gcn_readonly_numeric_fixture.h"
#include "video_core/renderer_d3d12/guest_subresource_rebase.h"

namespace NativeDX12 {
// Real T# loads, image_load/image_store and S_BARRIER through Liverpool/GCN.
// Independent endpoint constants check the shared word BEFORE and AFTER store.
inline void RunWritableWordGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base,bool cached_seed=false,u32 scalar_number=4,u32 scalar_format=4,bool packed_writer=false){
    const u32 OldWord=scalar_format==6?0x702003c0:0xc00805ff;
    const u32 NewWord=scalar_format==6?(packed_writer?0xc01e0380:0x881e0380):0x5ff00200;
    const u32 SeedWord=scalar_format==6?0x801c0440:scalar_format==5?0x3c00c000:0x4007fe00;
    // Independent typed R16G16 endpoints; pinned RemapSwizzle leaves RG unchanged.
    const auto scalar_channels=[&](u32 word){
        // Exact finite R11/G11/B10 values authored independently of native codecs.
        if(scalar_format==6){
            if(word==OldWord)return std::array<u32,4>{0x3f800000,0x40000000,0x3f000000,0x3f800000};
            if(word==SeedWord)return std::array<u32,4>{0x40800000,0x3f000000,0x40000000,0x3f800000};
            return std::array<u32,4>{0x3f000000,0x3f800000,packed_writer?0x44000000u:0x40800000u,0x3f800000};
        }
        if(scalar_format!=5)return std::array<u32,4>{word,0,0,word};
        u32 lo=word&65535,hi=word>>16;
        const auto channel=[&](u32 half){
            if(scalar_number==4)return half;
            if(scalar_number==5)return u32(s32(s16(half)));
            if(scalar_number==0)return std::bit_cast<u32>(float(half)/65535.f);
            if(scalar_number==1)return std::bit_cast<u32>(std::max(-1.f,float(s16(half))/32767.f));
            // Three authored finite half words, not a copy of the GPU codec.
            if(word==OldWord)return half==lo?0x38bfe000u:0xc0010000u;
            if(word==SeedWord)return half==lo?0xc0000000u:0x3f800000u;
            return half==lo?0x38000000u:0x43fe0000u;
        };
        const u32 r=channel(lo),g=channel(hi);
        const u32 one=scalar_number==4||scalar_number==5?1:0x3f800000;
        return std::array<u32,4>{r,g,0,one};
    };
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
        scalar.data_format=scalar_format;scalar.num_format=scalar_number;scalar.tiling_index=5;
        scalar.dst_sel_x=4;scalar.dst_sel_y=5;scalar.dst_sel_z=6;scalar.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(scalar);const auto layout=LayoutGuestTexture(desc);
        for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y),&OldWord,4);
        std::vector<std::byte> expected(memory,memory+Bytes);
        auto packed=scalar;packed.data_format=9;packed.num_format=number;
        table=reverse?std::array<AmdGpu::Image,2>{scalar,packed}:std::array<AmdGpu::Image,2>{packed,scalar};
        AmdGpu::Buffer output{};output.base_address=base+OutputOffset;output.stride=4;output.num_records=1024;
        output.data_format=4;output.num_format=4;output.dst_sel_x=4;output.dst_sel_y=5;output.dst_sel_z=6;output.dst_sel_w=7;
        const u32 packed_operand=reverse?0x00060000:0x00040000,scalar_operand=reverse?0x00040000:0x00060000;
        const u32 read_operand=packed_writer?scalar_operand:packed_operand,write_operand=packed_writer?packed_operand:scalar_operand;
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
            const auto seed_values=scalar_channels(SeedWord);
            if(warm)seed.insert(seed.end(),{0x7e1802ff,seed_values[0],0x7e1a02ff,seed_values[1],0x7e1c02ff,seed_values[2],0x7e1e02ff,seed_values[3],
                0xf0200f00,scalar_operand|0x0c00,0xbf8c0000,0xbf8a0000});
            seed.insert(seed.end(),{0xf0000f00,scalar_operand|0x0400,0xbf8c0000});
            for(u32 channel=0;channel<4;++channel){seed.push_back(0xe0700050|channel*4);seed.push_back(0x80010000|((channel+4)<<8));}
            seed.push_back(0xbf810000);dispatch(seed,0x5752434143484900ull+warm+u64(reverse)*16+(u64(scalar_number)<<32)+(u64(scalar_format)<<40)+(u64(packed_writer)<<48));
            const auto warm_channels=scalar_channels(warm?SeedWord:OldWord);
            memcpy(expected.data()+OutputOffset+80,warm_channels.data(),16);
        }
        const auto write_values=packed_writer?(number==1?
            std::array<u32,4>{std::bit_cast<u32>(-128.f/511.f),std::bit_cast<u32>(-128.f/511.f),std::bit_cast<u32>(1.f/511.f),0xbf800000}:
            std::array<u32,4>{u32(-128),u32(-128),1,u32(-1)}):scalar_channels(NewWord);
        std::vector<u32> words{0xc0c80100,0xc0cc0108,0xbf8c0000,0x7e000281,0x7e020280,
            0xf0000f00,read_operand|0x0400,0xbf8c0000,
            0x7e1802ff,write_values[0],0x7e1a02ff,write_values[1],0x7e1c02ff,write_values[2],0x7e1e02ff,write_values[3],
            0xf0200f00,write_operand|0x0c00,0xbf8c0000,0xbf8a0000,
            0xf0000f00,read_operand|0x0800,0xbf8c0000};
        for(u32 channel=0;channel<8;++channel){words.push_back(0xe0700000|channel*4);words.push_back(0x80010000|((channel+4)<<8));}
        if(cached_seed)words.insert(words.end(),{0xe0300004,0x80021400,0xbf8c0000,0xe0700020,0x80011400});
        words.push_back(0xbf810000);dispatch(words,0x57524f5244414c00ull+number+u64(reverse)*16+(u64(cached_seed)<<16)+(u64(scalar_number)<<32)+(u64(scalar_format)<<40)+(u64(packed_writer)<<48));
        const std::array<u32,4> before=packed_writer?scalar_channels(cached_seed&&warm?SeedWord:OldWord):scalar_format==6?(cached_seed&&warm?
            (number==1?std::array<u32,4>{std::bit_cast<u32>(64.f/511.f),std::bit_cast<u32>(-255.f/511.f),std::bit_cast<u32>(1.f/511.f),0xbf800000}:
                std::array<u32,4>{64,u32(-255),1,u32(-2)}):
            (number==1?std::array<u32,4>{std::bit_cast<u32>(-64.f/511.f),0,std::bit_cast<u32>(-254.f/511.f),0x3f800000}:
                std::array<u32,4>{u32(-64),0,u32(-254),1})):cached_seed&&warm&&scalar_format==5?(number==1?std::array<u32,4>{0,std::bit_cast<u32>(48.f/511.f),std::bit_cast<u32>(-64.f/511.f),0}:
            std::array<u32,4>{0,48,u32(-64),0}):cached_seed&&warm?(number==1?std::array<u32,4>{0xbf800000,0x3f800000,0,0x3f800000}:
            std::array<u32,4>{u32(-512),511,0,1}):number==1?std::array<u32,4>{0x3f800000,0xbf800000,0,0xbf800000}:
            std::array<u32,4>{511,u32(-511),0,u32(-1)};
        const std::array<u32,4> after=packed_writer?scalar_channels(NewWord):scalar_format==6?(number==1?
            std::array<u32,4>{std::bit_cast<u32>(-128.f/511.f),std::bit_cast<u32>(-128.f/511.f),std::bit_cast<u32>(129.f/511.f),0xbf800000}:
            std::array<u32,4>{u32(-128),u32(-128),129,u32(-2)}):number==1?std::array<u32,4>{0xbf800000,0,0x3f800000,0x3f800000}:
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
    printf("SCALAR donor data=%u number=%u packed-writer=%u\n",scalar_format,scalar_number,packed_writer);
    puts(cached_seed?"PASS:8 actual GCN cached mutable32 I(GpuModified0/1) views, separate raw B, before/after barrier and full65536 bytes":
        "PASS:8 actual GCN signed packed10/R32 stores, before/after S_BARRIER, both descriptor orders, warm reuse and full65536 bytes");
}
// Cold/warm parent-array stores observed through a physically shifted child.
// Only the output buffer is downloaded between dispatches: the second dispatch
// must see the first image store through I, not a CPU re-upload of guest B.
inline void RunShiftedWritableGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base){
    constexpr u32 Bytes=65536,ImageOffset=16384,OutputOffset=57344,Old=0x11223344;
    struct alignas(256) Code {std::array<u32,256> words{};} code;
    alignas(256) std::array<AmdGpu::Image,2> table{};
    gpu.BindRasterizer(&rasterizer);
    struct Drain {GuestRasterizer& r;AmdGpu::Liverpool& g;
        ~Drain(){g.WaitGpuIdle();try{r.Finish();}catch(...){}g.BindRasterizer(nullptr);}} drain{rasterizer,gpu};
    for(u32 tile:{8u,5u})for(bool reverse:{false,true}){
        rasterizer.ReadMemory(base,Bytes);rasterizer.Finish();
        rasterizer.UnmapMemory(base,Bytes);rasterizer.MapMemory(base,Bytes);
        rasterizer.NotifyHostMapping(base,Bytes,true,true);memset(memory,0xa7,Bytes);
        AmdGpu::Image parent{};parent.base_address=(base+ImageOffset)>>8;
        parent.type=u64(AmdGpu::ImageType::Color2DArray);parent.width=parent.height=parent.pitch=31;
        parent.depth=1;parent.last_array=1;parent.data_format=4;parent.num_format=4;parent.tiling_index=tile;
        parent.dst_sel_x=4;parent.dst_sel_y=5;parent.dst_sel_z=6;parent.dst_sel_w=7;
        auto full=DescribeGuestTexture(parent);full.writable=true;const auto layout=LayoutGuestTexture(full);
        if(layout.guest_bytes>OutputOffset-ImageOffset)throw std::runtime_error("Shifted fixture image exceeds arena");
        for(u32 layer=0;layer<2;++layer)for(u32 y=0;y<32;++y)for(u32 x=0;x<32;++x)
            memcpy(memory+ImageOffset+GuestPixelOffset(full,layout,0,layer,x,y),&Old,4);
        auto child=parent;child.type=u64(AmdGpu::ImageType::Color2D);child.depth=child.last_array=0;
        child.base_address=(base+ImageOffset+layout.mips[0].slice_bytes)>>8;
        auto local=DescribeGuestTexture(child);local.writable=true;
        const auto mapped=MapGuestWritableShaderSubview(full,layout,local,LayoutGuestTexture(local),true);
        if(!mapped||mapped->first_layer!=1||mapped->address!=full.address||mapped->layers!=2)
            throw std::runtime_error("Shifted shader mapping lost its parent coordinates");
        auto bad=local;bad.alt_tile=!bad.alt_tile;
        if(MapGuestWritableShaderSubview(full,layout,bad,LayoutGuestTexture(bad),true)||
           MapGuestWritableShaderSubview(full,layout,local,LayoutGuestTexture(local),false))
            throw std::runtime_error("Shifted shader mapping admitted incompatible addressing or a non-array binding");
        table=reverse?std::array<AmdGpu::Image,2>{parent,child}:std::array<AmdGpu::Image,2>{child,parent};
        const u32 read=reverse?0x00060000:0x00040000,write=reverse?0x00040000:0x00060000;
        AmdGpu::Buffer output{};output.base_address=base+OutputOffset;output.stride=4;output.num_records=1024;
        output.data_format=4;output.num_format=4;output.dst_sel_x=4;output.dst_sel_y=5;output.dst_sel_z=6;output.dst_sel_w=7;
        std::vector<std::byte> expected(memory,memory+Bytes);
        for(u32 warm=0;warm<2;++warm){
            const u32 value=warm?0x778899aa:0x55667788;
            // v0/v1/v2 = x1/y0/layer1. Child read ignores v2; array store uses it.
            std::vector<u32> words{0xc0c80100,0xc0cc0108,0xbf8c0000,
                0x7e000281,0x7e020280,0x7e040281,
                0xf0000f00,read|0x0400,0xbf8c0000,
                0x7e1802ff,value,0x7e1a0280,0x7e1c0280,0x7e1e0281,
                0xf0204f00,write|0x0c00,0xbf8c0000,0xbf8a0000,
                0xf0000f00,read|0x0800,0xbf8c0000};
            for(u32 lane=0;lane<8;++lane){words.push_back(0xe0700000|lane*4);words.push_back(0x80010000|((lane+4)<<8));}
            words.push_back(0xbf810000);code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
            AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);
            binary.shader_hash=0x5348494654554156ull+warm+u64(reverse)*16+u64(tile)*256;
            memcpy(code.words.data()+192,&binary,sizeof(binary));
            AmdGpu::ComputeProgram compute{};compute.address=reinterpret_cast<u64>(code.words.data())>>8;
            compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
            compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=8;
            compute.settings.num_vgprs=5;compute.settings.num_sgprs=3;
            const u64 pointer=reinterpret_cast<u64>(table.data());memcpy(compute.user_data.data(),&pointer,8);
            memcpy(compute.user_data.data()+4,&output,sizeof(output));
            constexpr u32 count=sizeof(compute)/4;
            std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,count,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
            const auto registers=std::bit_cast<std::array<u32,count>>(compute);packets.insert(packets.end(),registers.begin(),registers.end());
            packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
            gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();
            const std::array<u32,8> values{warm?0x55667788:Old,0,0,1,value,0,0,1};
            memcpy(expected.data()+OutputOffset,values.data(),sizeof(values));
            rasterizer.ReadMemory(output.base_address,output.GetSize());rasterizer.Finish();rasterizer.ValidateDebugMessages();
            if(memcmp(backing,expected.data(),Bytes))
                throw std::runtime_error("Shifted parent/child UAV lost before/after barrier values, retained I or untouched B/padding");
        }
    }
    puts("PASS shifted parent/child GCN storage, cold/warm, both descriptor orders, linear/micro and full backing oracle");
}
} // namespace NativeDX12
