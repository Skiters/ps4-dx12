// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {

// Authored only: real GCN keeps the buffer/image versions observably different
// within one invocation, and checks the whole allocation after CPU readback.
inline void RunMixedFormattedGcnFixture(GuestRasterizer& rasterizer, AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile, std::byte* memory, std::byte* backing, u64 base) {
    constexpr u64 AllocationBytes=65536, ImageOffset=16384, ObservationOffset=57344;
    constexpr u32 BufferValue=0x13579bdf, ImageValue=0x2468ace0, ImageNext=0x6a5b4c3d;
    constexpr u32 ImageFar=0x3142a6b7, BufferFinal=0x4567a9cd, ImageFinal=0x5273a8c9;
    alignas(256) std::array<u32,128> seed{}, command{}, probe{};
    GuestShaderFrontend reflection(profile);
    struct Drain {
        GuestRasterizer& rasterizer;bool armed=true;
        ~Drain(){if(armed)try{rasterizer.Finish();}catch(...){}}
    } drain{rasterizer};
    const auto initialize=[](auto& program,const std::vector<u32>& words,u64 hash){
        constexpr u32 MetadataWord=64;
        if(words.size()>MetadataWord||MetadataWord*4+sizeof(AmdGpu::BinaryInfo)>sizeof(program))
            throw std::runtime_error("Mixed formatted GCN overlaps binary metadata");
        program.fill(0);std::copy(words.begin(),words.end(),program.begin());
        AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;
        binary.length=u32(words.size()*4);binary.shader_hash=hash;
        std::memcpy(program.data()+MetadataWord,&binary,sizeof(binary));
    };
    const auto bind=[&](const auto& code,u32 users)->AmdGpu::ComputeProgram&{
        auto& program=gpu.GetCsRegs();program={};
        program.address=reinterpret_cast<u64>(code.data())>>8;
        program.num_thread_x.full=program.num_thread_y.full=program.num_thread_z.full=1;
        program.dim_x=program.dim_y=program.dim_z=1;
        program.settings.num_user_regs=users;
        program.settings.num_vgprs=3;program.settings.num_sgprs=3;
        return program;
    };
    const auto initial=[](u32 x,u32 y){return 0x10203040U^(y<<12)^x;};
    for(const bool small_image:{false,true})for(const bool clean:{false,true})
    for(const bool read_first:{false,true})for(const bool interior:{false,true})
    for(const bool image_write:{false,true}){
        const u64 identity=u64(small_image)|(u64(clean)<<1)|(u64(read_first)<<2)|
            (u64(interior)<<3)|(u64(image_write)<<4);
        rasterizer.ReadMemory(base,AllocationBytes);rasterizer.InvalidateMemory(base,AllocationBytes);
        std::memset(memory,0xa7,AllocationBytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;
        image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=4;image.num_format=4;
        image.tiling_index=5;image.width=small_image?23:119;image.height=small_image?31:47;
        image.pitch=small_image?31:127;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
        const auto far_offset=GuestPixelOffset(desc,layout,0,0,0,8);
        if(!layout.guest_bytes||layout.guest_bytes%4096||ImageOffset+layout.guest_bytes>ObservationOffset||
           (small_image?layout.guest_bytes>16384:layout.guest_bytes<=16384)||
           far_offset<256||far_offset%4||far_offset+16>layout.guest_bytes||
           GuestPixelOffset(desc,layout,0,0,0,0)!=0||GuestPixelOffset(desc,layout,0,0,1,0)!=4)
            throw std::runtime_error("Mixed formatted GCN needs distinct complete I/B geometry");
        for(u32 y=0;y<desc.height;++y)for(u32 x=0;x<desc.width;++x){
            const u32 value=initial(x,y);
            std::memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y),&value,4);
        }
        std::vector<std::byte> expected(backing,backing+AllocationBytes), gpu_bytes=expected;
        const auto word=[](auto& bytes,u64 offset,u32 value){std::memcpy(bytes.data()+offset,&value,4);};
        AmdGpu::Buffer full{};full.base_address=base+ImageOffset;full.stride=4;
        full.num_records=u32(layout.guest_bytes/4);full.data_format=4;full.num_format=4;
        full.dst_sel_x=4;full.dst_sel_y=5;full.dst_sel_z=6;full.dst_sel_w=7;
        auto writer=full;writer.base_address+=interior?far_offset:0;writer.num_records=4;
        auto observation=full;observation.base_address=base+ObservationOffset;observation.num_records=4096/4;
        initialize(seed,{
            0x7e0202ff,BufferValue,0xe0700000,0x80000100,
            0x7e000280,0x7e020280,0x7e1002ff,ImageValue,0xf0200100,0x00010800,
            0x7e000281,0x7e1002ff,ImageNext,0xf0200100,0x00010800,
            0x7e000280,0x7e020288,0x7e1002ff,ImageFar,0xf0200100,0x00010800,
            0xbf810000
        },0x4d49585345450000ULL+identity);
        auto& seed_program=bind(seed,12);
        std::memcpy(seed_program.user_data.data(),&full,sizeof(full));
        std::memcpy(seed_program.user_data.data()+4,&image,sizeof(image));
        rasterizer.DispatchDirect();word(gpu_bytes,ImageOffset,BufferValue);
        if(!rasterizer.IsGpuBufferModified(full.base_address,layout.guest_bytes))
            throw std::runtime_error("Mixed formatted GCN seed lost the independent buffer");
        if(clean){
            rasterizer.ReadMemory(full.base_address,layout.guest_bytes);rasterizer.Finish();
            word(expected,ImageOffset,BufferValue);
            if(rasterizer.IsGpuBufferModified(full.base_address,layout.guest_bytes)||
               std::memcmp(backing,expected.data(),AllocationBytes))
                throw std::runtime_error("Mixed formatted GCN clean-B precondition changed");
        }
        const bool retile=interior?(!read_first||!clean||!small_image):
            read_first&&(!clean||!small_image);
        const u32 reader_value=retile?ImageValue:BufferValue;
        const u32 old_writer=interior?(retile?ImageFar:initial(0,8)):reader_value;
        const u32 image_first=interior||retile?ImageValue:BufferValue;
        const u32 image_next=interior||retile?ImageNext:initial(1,0);
        const u32 image_far=interior||retile?ImageFar:initial(0,8);
        const u32 reader_group=read_first?0:1,writer_group=read_first?1:0;
        const std::array<u32,2> read_op{0xe0000000,0x80000400|(reader_group<<16)};
        const std::array<u32,2> writer_op{0xe0300000,0x80000500|(writer_group<<16)};
        std::vector<u32> words;
        const auto append=[&](const auto& pair){words.insert(words.end(),pair.begin(),pair.end());};
        append(read_first?read_op:writer_op);append(read_first?writer_op:read_op);
        words.insert(words.end(),{
            0xbf8c0000,0x7e1002ff,BufferFinal,
            0xe0700000,0x80000800|(writer_group<<16),
            0xe0700004,0x80000400|(writer_group<<16),
            // The image still observes its own prewrite texel1 after B changed.
            0x7e000281,0x7e020280,0xf0000100,0x00020600,0xbf8c0000,
            0xe0700008,0x80000600|(writer_group<<16),
            0xe070000c,0x80000500|(writer_group<<16),
            0xe0700010,0x80000800|(writer_group<<16)
        });
        if(image_write)words.insert(words.end(),{
            0x7e000280,0x7e020280,0x7e1002ff,ImageFinal,0xf0200100,0x00020800
        });
        words.push_back(0xbf810000);initialize(command,words,0x4d4958434d440000ULL+identity);
        auto& command_program=bind(command,16);
        std::memcpy(command_program.user_data.data()+reader_group*4,&full,sizeof(full));
        std::memcpy(command_program.user_data.data()+writer_group*4,&writer,sizeof(writer));
        std::memcpy(command_program.user_data.data()+8,&image,sizeof(image));
        const auto& stage=reflection.BuildCompute(gpu);
        size_t guest_index{};
        for(const auto& buffer:stage.info.buffers)if(!buffer.IsSpecial()){
            const auto sharp=buffer.GetSharp(stage.info);
            const bool is_writer=guest_index==(read_first?1U:0U);
            const auto& source=is_writer?writer:full;
            if(guest_index>=2||sharp.base_address!=source.base_address||sharp.GetSize()!=source.GetSize()||
               buffer.is_written!=is_writer||buffer.is_formatted==is_writer)
                throw std::runtime_error("Mixed formatted GCN lost descriptor order or exact writer bounds");
            ++guest_index;
        }
        if(guest_index!=2||stage.info.images.size()!=1||stage.info.uses_dma||
           stage.info.images[0].is_written!=image_write)
            throw std::runtime_error("Mixed formatted GCN resource domain changed");
        rasterizer.DispatchDirect();
        if(retile){word(gpu_bytes,ImageOffset,ImageValue);word(gpu_bytes,ImageOffset+4,ImageNext);
            word(gpu_bytes,ImageOffset+far_offset,ImageFar);}
        const u64 writer_offset=writer.base_address-base;
        word(gpu_bytes,writer_offset,BufferFinal);word(gpu_bytes,writer_offset+4,reader_value);
        word(gpu_bytes,writer_offset+8,image_next);word(gpu_bytes,writer_offset+12,old_writer);
        if(!clean)std::memcpy(expected.data()+ImageOffset,gpu_bytes.data()+ImageOffset,size_t(layout.guest_bytes));
        else std::memcpy(expected.data()+writer_offset,gpu_bytes.data()+writer_offset,16);
        // Observe I before requesting CPU coherence for B. Two image probes also
        // ensure the retained/current image survives the intervening B readback.
        initialize(probe,{
            0x7e000280,0x7e020280,0xf0000100,0x00000400,
            0x7e000281,0xf0000100,0x00000500,
            0x7e000280,0x7e020288,0xf0000100,0x00000600,0xbf8c0000,
            0xe0700000,0x80020400,0xe0700004,0x80020500,0xe0700008,0x80020600,0xbf810000
        },0x4d495850524f0000ULL+identity);
        for(unsigned pass=0;pass<2;++pass){
            auto& probe_program=bind(probe,12);
            std::memcpy(probe_program.user_data.data(),&image,sizeof(image));
            std::memcpy(probe_program.user_data.data()+8,&observation,sizeof(observation));
            rasterizer.DispatchDirect();
            word(expected,ObservationOffset,image_write?ImageFinal:image_first);
            word(expected,ObservationOffset+4,image_next);word(expected,ObservationOffset+8,image_far);
            rasterizer.ReadMemory(base,AllocationBytes);rasterizer.Finish();
            if(std::memcmp(backing,expected.data(),AllocationBytes))
                throw std::runtime_error("Mixed formatted GCN lost I/B order, input seed, exact bounds or physical padding");
        }
        std::printf("PASS: mixed formatted %s %s %s %s %s; full I/B/physical oracle\n",
            small_image?"small":"large",clean?"clean":"dirty",read_first?"R/W":"W/R",
            interior?"interiorW":"baseW",image_write?"imageRW":"imageR");
    }
    rasterizer.Finish();drain.armed=false;
}
} // namespace NativeDX12
