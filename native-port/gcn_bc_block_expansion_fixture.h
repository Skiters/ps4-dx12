// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/renderer_d3d12/guest_image_ownership.h"
#include "video_core/amdgpu/liverpool.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {

// Authored, unrun real-GCN word-image -> compressed block-image acceptance.
inline void RunBcBlockExpansionGcnFixture(GuestRasterizer& rasterizer, AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile, std::byte* memory, std::byte* backing, u64 base) {
    constexpr u64 AllocationBytes=65536,ImageOffset=16384,OutputOffset=40960,OutputBytes=20480;
    constexpr u32 BufferValue=0x13579bdf,PoisonValue=0xaaaaaaaa,MarkerValue=0x2468ace0;
    constexpr u32 Endpoints=0x07e0f800,GreenIndices=0x55555555;
    constexpr std::array<std::array<u32,2>,4> Blocks{{{0,0},{1,0},{0,8},{2,0}}};
    constexpr std::array<std::array<u32,2>,4> Texels{{{0,0},{7,2},{1,35},{9,1}}};
    struct alignas(256) Code {std::array<u32,128> words{};};
    Code warm{},seed{},before{},writer{},marker{},raw{},sample{},uint_probe{},formatted{},after{};
    GuestShaderFrontend frontend(profile);
    struct Drain {GuestRasterizer& rasterizer;bool armed=true;
        ~Drain(){if(armed)try{rasterizer.Finish();}catch(...){}}} drain{rasterizer};
    const auto initialize=[](Code& code,const std::vector<u32>& words,u64 hash){
        constexpr u32 MetadataWord=96;
        if(words.size()>MetadataWord||MetadataWord*4+sizeof(AmdGpu::BinaryInfo)>sizeof(code.words))
            throw std::runtime_error("BC expansion GCN overlaps binary metadata");
        code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
        AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;
        binary.length=u32(words.size()*4);binary.shader_hash=hash;
        std::memcpy(code.words.data()+MetadataWord,&binary,sizeof(binary));
    };
    const auto bind=[&](const Code& code,u32 registers,u32 threads=1)->AmdGpu::ComputeProgram&{
        auto& program=gpu.GetCsRegs();program={};
        program.address=reinterpret_cast<u64>(code.words.data())>>8;
        program.num_thread_x.full=u16(threads);program.num_thread_y.full=program.num_thread_z.full=1;
        program.dim_x=program.dim_y=program.dim_z=1;
        program.settings.num_user_regs=registers;program.settings.num_vgprs=3;program.settings.num_sgprs=3;
        return program;
    };
    const auto move=[](std::vector<u32>& words,u32 reg,u32 value){
        words.insert(words.end(),{0x7e0002ffu|(reg<<17),value});
    };
    struct BufferExpectation {AmdGpu::Buffer sharp;bool written,formatted;};
    const auto verify=[](const auto& stage,std::initializer_list<BufferExpectation> buffers,
                         const AmdGpu::Image* image=nullptr,bool written=false){
        if(stage.spirv.empty()||stage.info.translation_failed||stage.info.uses_dma||
           stage.info.images.size()!=(image?1u:0u))
            throw std::runtime_error("BC expansion GCN changed image/DMA operands");
        auto next=buffers.begin();
        for(const auto& buffer:stage.info.buffers)if(!buffer.IsSpecial()){
            if(next==buffers.end())throw std::runtime_error("BC expansion GCN gained a guest buffer");
            const auto sharp=buffer.GetSharp(stage.info);
            if(sharp.base_address!=next->sharp.base_address||sharp.GetSize()!=next->sharp.GetSize()||
               buffer.is_written!=next->written||buffer.is_formatted!=next->formatted)
                throw std::runtime_error("BC expansion GCN lost bounds, access or first-use buffer order");
            ++next;
        }
        if(next!=buffers.end())throw std::runtime_error("BC expansion GCN removed a guest buffer");
        if(image){const auto& resource=stage.info.images.front();const auto sharp=resource.GetSharp(stage.info);
            if(resource.is_written!=written||resource.native_raw32||std::memcmp(&sharp,image,sizeof(sharp)))
                throw std::runtime_error("BC expansion GCN changed original image sharp/access");}
    };
    for(const bool bc3:{false,true}){
        const u64 identity=bc3?0x100:0;
        const u32 components=bc3?4:2,block_bytes=components*4,index_word=bc3?3:1;
        rasterizer.ReadMemory(base,AllocationBytes);rasterizer.InvalidateMemory(base,AllocationBytes);
        std::memset(memory,bc3?0x7e:0x6d,AllocationBytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;
        image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=bc3?14:11;image.num_format=4;
        image.width=23;image.height=31;image.pitch=31;image.tiling_index=5;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        auto bc=image;bc.width=95;bc.height=127;bc.pitch=127;bc.data_format=bc3?37:35;bc.num_format=0;
        auto source_desc=DescribeGuestTexture(image);source_desc.writable=true;
        const auto source_layout=LayoutGuestTexture(source_desc);
        const auto bc_desc=DescribeGuestTexture(bc);
        const auto bc_layout=LayoutGuestTexture(bc_desc);
        if(base%4096||source_layout.guest_bytes!=(bc3?16384u:8192u)||
           source_layout.logical_bytes!=u64(24)*32*block_bytes||
           bc_layout.guest_bytes!=source_layout.guest_bytes||bc_layout.logical_bytes!=source_layout.logical_bytes||
           bc_layout.block_extent!=4||bc_layout.expanded_bytes_per_pixel||bc_desc.writable||
           bc_layout.format!=(bc3?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_BC1_UNORM)||
           ImageOffset+source_layout.guest_bytes>OutputOffset||256+source_layout.guest_bytes>OutputBytes)
            throw std::runtime_error("BC expansion needs padded page-aligned equal physical blocks and readonly native BC");
        if(source_layout.mips.size()!=1||bc_layout.mips.size()!=1)
            throw std::runtime_error("BC expansion fixture unexpectedly gained mip levels");
        const auto& a=source_layout.mips.front();const auto& b=bc_layout.mips.front();
        if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||
           a.macro!=b.macro||a.thickness!=b.thickness)
            throw std::runtime_error("BC expansion source and destination differ in physical mip layout");
        // Structural controls exercise the public ownership/view contract only;
        // no fabricated GPU-owned or modified flags manufacture a live witness.
        GuestTextureCache::Entry shape;shape.desc=bc_desc;shape.layout=bc_layout;
        const auto owner=DescribeGuestImageOwnership(shape);
        if(!owner||owner->desc.data_format!=source_desc.data_format||!owner->desc.writable||
           owner->desc.width!=24||owner->desc.height!=32||owner->desc.pitch!=32||
           shape.Srv().Format!=bc_layout.format||shape.desc.writable)
            throw std::runtime_error("BC readonly view lost canonical UINT block ownership geometry");
        auto odd=shape.desc;--odd.width;shape.desc=odd;shape.layout=LayoutGuestTexture(odd);
        if(!shape.layout.expanded_bytes_per_pixel||DescribeGuestImageOwnership(shape))
            throw std::runtime_error("Odd decoded BC must not retain exact compressed blocks");
        shape.desc=bc_desc;shape.layout=bc_layout;shape.layout.bytes_per_pixel=bc3?8:16;
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("Wrong block size acquired BC ownership");
        auto other_codec=bc_desc;other_codec.data_format=bc3?39:38;
        if(CompatibleGuestSampledStorage(bc_desc,other_codec))
            throw std::runtime_error("Equal block size must not create cross-codec sampled views");
        bool rejected=false;auto writable_bc=bc_desc;writable_bc.writable=true;
        try{(void)LayoutGuestTexture(writable_bc);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("BC ownership incorrectly authorized shader writes");
        const auto words=[&](bool green){return bc3?std::array<u32,4>{0x0000ffff,0,Endpoints,green?GreenIndices:0}:
            std::array<u32,4>{Endpoints,green?GreenIndices:0,0,0};};
        const auto put_block=[&](std::byte* bytes,u32 x,u32 y,bool green){const auto value=words(green);
            std::memcpy(bytes+GuestPixelOffset(source_desc,source_layout,0,0,x,y),value.data(),block_bytes);};
        for(u32 y=0;y<32;++y)for(u32 x=0;x<24;++x)put_block(memory+ImageOffset,x,y,false);
        std::vector<std::byte> expected(backing,backing+AllocationBytes);
        const auto expect=[&](u64 offset,u32 word){std::memcpy(expected.data()+offset,&word,4);};
        AmdGpu::Buffer target{};target.base_address=base+ImageOffset;target.stride=4;
        target.num_records=u32(source_layout.guest_bytes/4);target.data_format=4;target.num_format=4;
        target.dst_sel_x=4;target.dst_sel_y=5;target.dst_sel_z=6;target.dst_sel_w=7;
        auto output=target;output.base_address=base+OutputOffset;output.num_records=OutputBytes/4;
        auto full=target;full.base_address=base;full.num_records=AllocationBytes/4;
        initialize(warm,{0xe0300000,0x80000400,0xbf8c0000,0xe0700060,0x80010400,0xbf810000},
            0x4243455850000001ULL+identity);
        auto& warm_program=bind(warm,8);std::memcpy(warm_program.user_data.data(),&full,sizeof(full));
        std::memcpy(warm_program.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{full,false,false},{output,true,false}});rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.Finish();
        u32 warm_value{};std::memcpy(&warm_value,expected.data(),4);expect(OutputOffset+96,warm_value);
        if(std::memcmp(backing,expected.data(),AllocationBytes))throw std::runtime_error("BC warm arena changed physical bytes");
        std::vector<u32> seed_words;move(seed_words,1,BufferValue);
        seed_words.insert(seed_words.end(),{0xe0700000,0x80000100});
        for(u32 index=0;index<3;++index){move(seed_words,2,Blocks[index][0]);move(seed_words,3,Blocks[index][1]);
            const auto value=words(true);for(u32 component=0;component<components;++component)move(seed_words,8+component,value[component]);
            seed_words.insert(seed_words.end(),{bc3?0xf0200f00u:0xf0200300u,0x00010802});}
        seed_words.push_back(0xbf810000);initialize(seed,seed_words,0x4243455850000002ULL+identity);
        auto& seed_program=bind(seed,12);std::memcpy(seed_program.user_data.data(),&target,sizeof(target));
        std::memcpy(seed_program.user_data.data()+4,&image,sizeof(image));
        verify(frontend.BuildCompute(gpu),{{target,true,false}},&image,true);rasterizer.DispatchDirect();
        std::vector<u32> before_words{0xe0300000,0x80000c00,0xbf8c0000,0xe0700064,0x80030c00};
        for(u32 index=0;index<Blocks.size();++index){move(before_words,0,Blocks[index][0]);move(before_words,1,Blocks[index][1]);
            before_words.insert(before_words.end(),{bc3?0xf0000f00u:0xf0000300u,0x00010400,0xbf8c0000});
            for(u32 component=0;component<components;++component)before_words.insert(before_words.end(),{
                0xe0700000u+index*16+component*4,0x80030000u|((4+component)<<8)});
            const auto value=words(index<3);for(u32 c=0;c<components;++c)expect(OutputOffset+index*16+c*4,value[c]);}
        before_words.push_back(0xbf810000);initialize(before,before_words,0x4243455850000003ULL+identity);
        auto& before_program=bind(before,16);std::memcpy(before_program.user_data.data(),&target,sizeof(target));
        std::memcpy(before_program.user_data.data()+4,&image,sizeof(image));
        std::memcpy(before_program.user_data.data()+12,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{target,false,false},{output,true,false}},&image);rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.ReadMemory(target.base_address,target.GetSize());
        rasterizer.Finish();expect(OutputOffset+100,BufferValue);expect(ImageOffset,BufferValue);
        if(rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||std::memcmp(backing,expected.data(),AllocationBytes))
            throw std::runtime_error("BC seed lost distinct clean B, native UINT I or physical padding");
        const auto physical_before=expected;
        auto logical=std::vector<std::byte>(expected.begin()+ImageOffset,expected.begin()+ImageOffset+source_layout.guest_bytes);
        for(u32 y=0;y<32;++y)for(u32 x=0;x<24;++x)put_block(logical.data(),x,y,false);
        for(u32 index=0;index<3;++index)put_block(logical.data(),Blocks[index][0],Blocks[index][1],true);
        const u64 poison_offset=u64(index_word)*4,marker_offset=GuestPixelOffset(source_desc,source_layout,0,0,8,0);
        if(!poison_offset||marker_offset<=poison_offset+4||marker_offset+4>=source_layout.guest_bytes)
            throw std::runtime_error("BC interior writers are not disjoint original-base exclusions");
        const auto write=[&](Code& code,u64 offset,u32 value,u64 hash){auto sharp=target;sharp.base_address+=offset;sharp.num_records=1;
            initialize(code,{0x7e0202ff,value,0xe0700000,0x80000100,0xbf810000},hash+identity);
            auto& program=bind(code,4);std::memcpy(program.user_data.data(),&sharp,sizeof(sharp));
            verify(frontend.BuildCompute(gpu),{{sharp,true,false}});rasterizer.DispatchDirect();};
        write(writer,poison_offset,PoisonValue,0x4243455850000004ULL);
        write(marker,marker_offset,MarkerValue,0x4243455850000005ULL);
        initialize(raw,{0xe0300000,0x80000400,0xe0300000u|u32(poison_offset),0x80000500,
            0xe0300000u|u32(marker_offset),0x80000600,0xbf8c0000,
            0xe0700064,0x80010400,0xe0700068,0x80010500,0xe070006c,0x80010600,0xbf810000},
            0x4243455850000006ULL+identity);
        auto& raw_program=bind(raw,8);std::memcpy(raw_program.user_data.data(),&target,sizeof(target));
        std::memcpy(raw_program.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{target,false,false},{output,true,false}});rasterizer.DispatchDirect();
        expect(OutputOffset+104,PoisonValue);expect(OutputOffset+108,MarkerValue);
        const auto sample_bc=[&](Code& code,u32 output_offset,u64 hash){std::vector<u32> code_words;
            for(u32 index=0;index<Texels.size();++index){move(code_words,0,Texels[index][0]);move(code_words,1,Texels[index][1]);
                code_words.insert(code_words.end(),{0xf0000f00,0x00000400,0xbf8c0000});
                for(u32 component=0;component<4;++component)code_words.insert(code_words.end(),{
                    0xe0700000u+output_offset+index*16+component*4,0x80020000u|((4+component)<<8)});
                const bool green=index<3;const std::array<float,4> rgba{green?0.f:1.f,green?1.f:0.f,0.f,1.f};
                for(u32 component=0;component<4;++component)expect(OutputOffset+output_offset+index*16+component*4,std::bit_cast<u32>(rgba[component]));}
            code_words.push_back(0xbf810000);initialize(code,code_words,hash+identity);
            auto& program=bind(code,12);std::memcpy(program.user_data.data(),&bc,sizeof(bc));
            std::memcpy(program.user_data.data()+8,&output,sizeof(output));
            verify(frontend.BuildCompute(gpu),{{output,true,false}},&bc);rasterizer.DispatchDirect();};
        sample_bc(sample,128,0x4243455850000007ULL);
        if(!rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||
           std::memcmp(backing,physical_before.data(),AllocationBytes))
            throw std::runtime_error("BC expansion committed dirty B or readonly I prematurely");
        // Force the reverse BC -> UINT image transfer before expanding again.
        // All components of each probed compressed block must remain exact.
        std::vector<u32> uint_words;
        for(u32 index=0;index<Blocks.size();++index){move(uint_words,0,Blocks[index][0]);move(uint_words,1,Blocks[index][1]);
            uint_words.insert(uint_words.end(),{bc3?0xf0000f00u:0xf0000300u,0x00000400,0xbf8c0000});
            for(u32 component=0;component<components;++component)uint_words.insert(uint_words.end(),{
                0xe0700000u+index*16+component*4,0x0c020000u|((4+component)<<8)});
            const auto value=words(index<3);for(u32 c=0;c<components;++c)expect(OutputOffset+OutputBytes-64+index*16+c*4,value[c]);}
        uint_words.push_back(0xbf810000);initialize(uint_probe,uint_words,0x424345585000000aULL+identity);
        auto& uint_program=bind(uint_probe,13);std::memcpy(uint_program.user_data.data(),&image,sizeof(image));
        std::memcpy(uint_program.user_data.data()+8,&output,sizeof(output));
        uint_program.user_data[12]=u32(OutputBytes)-64;
        verify(frontend.BuildCompute(gpu),{{output,true,false}},&image);rasterizer.DispatchDirect();
        sample_bc(after,192,0x4243455850000009ULL);
        // v0 is the 256-thread local X index. idxen uses stride4; s8 is a
        // byte offset selecting one 1-KiB chunk without changing sharp bounds.
        initialize(formatted,{0xe0002000,0x08000400,0xbf8c0000,0xe0702000,0x08010400,0xbf810000},
            0x4243455850000008ULL+identity);
        auto copied=target;copied.base_address=base+OutputOffset+256;
        for(u32 offset=0;offset<source_layout.guest_bytes;offset+=1024){auto& program=bind(formatted,9,256);
            std::memcpy(program.user_data.data(),&target,sizeof(target));
            std::memcpy(program.user_data.data()+4,&copied,sizeof(copied));program.user_data[8]=offset;
            verify(frontend.BuildCompute(gpu),{{target,false,true},{copied,true,false}});rasterizer.DispatchDirect();}
        std::memcpy(expected.data()+OutputOffset+256,logical.data(),logical.size());
        if(!rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||
           std::memcmp(backing,physical_before.data(),AllocationBytes))
            throw std::runtime_error("BC readonly retile changed physical B or retired its original dirty union");
        // Only the original two interior DWORD stores publish into physical B.
        // The retile's clean words (including B's distinct first DWORD) stay private.
        std::memcpy(expected.data()+ImageOffset+poison_offset,logical.data()+poison_offset,4);
        std::memcpy(expected.data()+ImageOffset+marker_offset,logical.data()+marker_offset,4);
        rasterizer.ReadMemory(base,AllocationBytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(std::memcmp(backing,expected.data(),AllocationBytes))
            throw std::runtime_error("BC expansion decoded colors, exact compressed DWORDs, dirty union or full64KiB padding mismatch");
        std::printf("PASS: real GCN %s UINT block expansion, exact old-I colors/words and full64KiB physical oracle\n",bc3?"BC3":"BC1");
    }
    rasterizer.Finish();drain.armed=false;
}
} // namespace NativeDX12
