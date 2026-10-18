// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/amdgpu/pm4_cmds.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <cstdio>

namespace NativeDX12 {
// Inspect original operands only inside the real Liverpool queue callback.
class ReadonlyNumericProbeRasterizer final:public GuestRasterizer {
    AmdGpu::Liverpool& gpu_;GuestShaderFrontend inspector_;bool inspect_;
public:
    ReadonlyNumericProbeRasterizer(ID3D12Device* d,AmdGpu::Liverpool& g,const Shader::Profile& p,
        const std::wstring& mesa,const std::wstring& dxc,bool inspect):
        GuestRasterizer(d,g,p,mesa,dxc),gpu_(g),inspector_(p),inspect_(inspect){}
    void DispatchDirect()override{
        if(inspect_){auto& stage=inspector_.BuildCompute(gpu_);
            for(const auto& image:stage.info.images){const auto sharp=image.GetSharp(stage.info);
                printf("OPERAND image=%llx data=%u number=%u width=%u writable=%u\n",u64(sharp.base_address)<<8,
                    u32(sharp.data_format),u32(sharp.num_format),u32(sharp.width)+1,unsigned(image.is_written));}
            fflush(stdout);}
        GuestRasterizer::DispatchDirect();
    }
};
inline void RunReadonlyNumericGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    std::byte* memory,std::byte* backing,u64 base){
    constexpr u64 ImageOffset=16384,OutputOffset=57344,AllocationBytes=65536;
    constexpr u32 ImageWord=0xff0000ff,Marker=0x13579bdf;
    struct alignas(256) Code {std::array<u32,256> words{};};
    Code arena_warm{},warm{},image_write{},marker{},pair{};alignas(256) std::array<AmdGpu::Image,2> table{};
    AmdGpu::ComputeProgram compute{};
    const auto initialize=[](Code& code,const std::vector<u32>& words,u64 hash){
        constexpr u32 Metadata=192;
        if(words.size()>Metadata)throw std::runtime_error("Readonly numeric fixture overlaps shader metadata");
        code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
        AmdGpu::BinaryInfo info{};info.signature=info.signature_ref;info.length=u32(words.size()*4);info.shader_hash=hash;
        memcpy(code.words.data()+Metadata,&info,sizeof(info));};
    const auto bind=[&](const Code& code){compute={};compute.address=reinterpret_cast<u64>(code.words.data())>>8;
        compute.num_thread_x.full=compute.num_thread_y.full=compute.num_thread_z.full=1;
        compute.dim_x=compute.dim_y=compute.dim_z=1;compute.settings.num_user_regs=12;
        compute.settings.num_vgprs=3;compute.settings.num_sgprs=3;
        const u64 pointer=reinterpret_cast<u64>(table.data());memcpy(compute.user_data.data(),&pointer,8);};
    gpu.BindRasterizer(&rasterizer);
    struct Drain {GuestRasterizer& r;AmdGpu::Liverpool& g;~Drain(){g.WaitGpuIdle();try{r.Finish();}catch(...){}g.BindRasterizer(nullptr);}} drain{rasterizer,gpu};
    const auto dispatch=[&]{constexpr u32 words=sizeof(compute)/4;
        std::vector<u32> packets{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::SetShReg,words,AmdGpu::PM4ShaderType::ShaderCompute}.raw,0x200};
        const auto data=std::bit_cast<std::array<u32,words>>(compute);packets.insert(packets.end(),data.begin(),data.end());
        packets.insert(packets.end(),{AmdGpu::PM4Type3Header{AmdGpu::PM4ItOpcode::DispatchDirect,3,AmdGpu::PM4ShaderType::ShaderCompute}.raw,1,1,1,1});
        gpu.SubmitGfx(packets,{});gpu.WaitGpuIdle();};
    const auto channels=[](u32 number){
        if(number==4)return std::array<u32,4>{255,0,0,255};
        if(number==5)return std::array<u32,4>{0xffffffff,0,0,0xffffffff};
        // Raw0xff is signed-1, hence SNORM8 is -1/127, not the -1
        // endpoint encoded by0x80/0x81. This oracle is independent of GPU output.
        const u32 one=number==1?std::bit_cast<u32>(-1.f/127.f):std::bit_cast<u32>(1.f);
        return std::array<u32,4>{one,0,0,one};};
    for(const auto numbers:{std::array<u32,2>{0,4},std::array<u32,2>{1,5},std::array<u32,2>{9,4}})
    for(const bool reverse:{false,true})for(const bool formatted:{false,true})for(const bool modified:{false,true}){
        printf("CASE: actual GCN readonly numeric pair=%u/%u reverse=%u formatted=%u GpuModified=%u\n",numbers[0],numbers[1],reverse,formatted,modified);fflush(stdout);
        rasterizer.Finish();rasterizer.UnmapMemory(base,AllocationBytes);rasterizer.MapMemory(base,AllocationBytes);
        rasterizer.NotifyHostMapping(base,AllocationBytes,true,true);memset(memory,0xa7,AllocationBytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;image.type=u64(AmdGpu::ImageType::Color2D);
        image.width=image.height=31;image.pitch=31;image.tiling_index=5;image.data_format=10;image.num_format=4;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        const auto desc=DescribeGuestTexture(image);const auto layout=LayoutGuestTexture(desc);
        if(layout.guest_bytes!=4096||GuestPixelOffset(desc,layout,0,0,1,0)!=4)
            throw std::runtime_error("Readonly numeric fixture requires original 4KiB padded image/word1");
        for(u32 y=0;y<desc.height;++y)for(u32 x=0;x<desc.width;++x)
            memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,0,0,x,y),&ImageWord,4);
        std::vector<std::byte> expected(backing,backing+AllocationBytes);
        AmdGpu::Buffer input{};input.base_address=base+ImageOffset;input.stride=4;input.num_records=1024;
        input.data_format=4;input.num_format=4;input.dst_sel_x=4;input.dst_sel_y=5;input.dst_sel_z=6;input.dst_sel_w=7;
        auto output=input;output.base_address=base+OutputOffset;
        table={image,image};
        // Materialize a complete native B arena with an actual raw GPU read.
        // A four-byte shifted writer alone has no independently protectable
        // page envelope and is not a legal ownership seed for this fixture.
        auto full=input;full.base_address=base;full.num_records=AllocationBytes/4;
        initialize(arena_warm,{0xe0300000,0x80020c00,0xbf8c0000,0xe0700040,0x80010c00,0xbf810000},0x524e554d4152454eULL);
        bind(arena_warm);memcpy(compute.user_data.data()+4,&output,sizeof(output));memcpy(compute.user_data.data()+8,&full,sizeof(full));dispatch();
        rasterizer.ReadMemory(output.base_address,output.GetSize());rasterizer.Finish();
        const u32 arena_word=0xa7a7a7a7;memcpy(expected.data()+OutputOffset+64,&arena_word,4);
        // SMRD3 loads an original T# from a real user-data table into S16..23.
        // All descriptors fit the16 original user registers; no out-of-bounds
        // GetCsRegs access or artificial public renderer inspection is used.
        initialize(warm,{0x7e000281,0x7e020280,0xf0000f00,0x00000400,0xbf8c0000,
            0xe0700000,0x80020400,0xe0700004,0x80020500,0xe0700008,0x80020600,0xe070000c,0x80020700,0xbf810000},0x524e554d5741524eULL);
        bind(warm);memcpy(compute.user_data.data(),&image,sizeof(image));memcpy(compute.user_data.data()+8,&output,sizeof(output));dispatch();
        rasterizer.ReadMemory(output.base_address,output.GetSize());rasterizer.Finish();
        rasterizer.ValidateDebugMessages();
        const auto warm_words=channels(4);memcpy(expected.data()+OutputOffset,warm_words.data(),16);
        if(memcmp(backing,expected.data(),AllocationBytes)){
            for(u64 i=0;i<AllocationBytes;++i)if(backing[i]!=expected[i]){fprintf(stderr,"Readonly seed mismatch byte=%llu got=%02x expected=%02x\n",i,std::to_integer<unsigned>(backing[i]),std::to_integer<unsigned>(expected[i]));break;}
            u32 got[4]{};memcpy(got,backing+OutputOffset,16);fprintf(stderr,"Warm channels=%08x %08x %08x %08x\n",got[0],got[1],got[2],got[3]);
            throw std::runtime_error("Readonly numeric seed changed physical bytes");}
        if(modified){
            // Change ORIGINAL GpuModified history through a real image_store,
            // without changing the independently known word. SafeToDownload
            // excludes a merely CPU-initialized readonly I from formatted I->B.
            initialize(image_write,{0x7e1802ff,Marker,0xe0700000,0x80020c00,
                0x7e000281,0x7e020280,0x7e0802ff,255,0x7e0a0280,0x7e0c0280,0x7e0e02ff,255,
                0xf0200f00,0x00000400,0xbf810000},0x524e554d57524954ULL);
            auto separate=input;separate.base_address+=4;separate.num_records=1;
            bind(image_write);memcpy(compute.user_data.data(),&image,sizeof(image));memcpy(compute.user_data.data()+8,&separate,sizeof(separate));dispatch();
        }
        // A shifted raw B writer leaves the original same-base readonly I valid.
        auto shifted=input;shifted.base_address+=4;shifted.num_records=1;
        initialize(marker,{0x7e1802ff,Marker,0xe0700000,0x80020c00,0xbf810000},0x524e554d4d41524bULL);
        bind(marker);memcpy(compute.user_data.data()+8,&shifted,sizeof(shifted));dispatch();
        if(!rasterizer.IsGpuBufferModified(shifted.base_address,4))throw std::runtime_error("Readonly numeric marker did not write B");
        table[0].num_format=numbers[reverse?1:0];table[1].num_format=numbers[reverse?0:1];
        const u64 identity=u64(numbers[0])|(u64(numbers[1])<<4)|(u64(reverse)<<8)|(u64(formatted)<<9)|(u64(modified)<<10);
        initialize(pair,{0xc0c80100,0xc0cc0108,0xbf8c0000,formatted?0xe0000004u:0xe0300004u,0x80020c00,
            0x7e000281,0x7e020280,0xf0000f00,0x00040400,0xf0000f00,0x00060800,0xbf8c0000,
            0xe0700000,0x80010c00,0xe0700004,0x80010400,0xe0700008,0x80010500,0xe070000c,0x80010600,
            0xe0700010,0x80010700,0xe0700014,0x80010800,0xe0700018,0x80010900,0xe070001c,0x80010a00,
            0xe0700020,0x80010b00,0xbf810000},0x524e554d50414900ULL+identity);
        bind(pair);memcpy(compute.user_data.data()+4,&output,sizeof(output));memcpy(compute.user_data.data()+8,&input,sizeof(input));dispatch();
        const u32 final_word=formatted&&modified?ImageWord:Marker;memcpy(expected.data()+ImageOffset+4,&final_word,4);
        memcpy(expected.data()+OutputOffset,&final_word,4);
        const auto a=channels(u32(table[0].num_format)),b=channels(u32(table[1].num_format));
        memcpy(expected.data()+OutputOffset+4,a.data(),16);memcpy(expected.data()+OutputOffset+20,b.data(),16);
        rasterizer.ReadMemory(output.base_address,output.GetSize());rasterizer.ReadMemory(input.base_address,input.GetSize());rasterizer.Finish();
        if(memcmp(backing,expected.data(),AllocationBytes)){
            for(u64 i=0;i<AllocationBytes;++i)if(backing[i]!=expected[i]){fprintf(stderr,"Readonly numeric mismatch byte=%llu got=%02x expected=%02x\n",i,std::to_integer<unsigned>(backing[i]),std::to_integer<unsigned>(expected[i]));break;}
            throw std::runtime_error("Readonly numeric views lost typed I/B/order or full physical padding");}
    }
    puts("PASS:24 actual GCN readonly UNORM/UINT/SNORM/SINT/sRGB pairs, both descriptor orders, original GpuModified0/1, raw B versus eligible formatted I residency and complete65536 physical bytes");
}
} // namespace NativeDX12
