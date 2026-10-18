// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_recompiler/info.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/passes/resource_pass.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "common/logging/log.h"
#include <cstdio>
#include <cstring>
#include <bit>
namespace Common::Log { std::array<Level,size_t(Class::Count)> g_class_levels{}; }
using namespace Shader;
static void Run(bool dynamic,bool reverse,bool same,bool native,bool loaded,bool alias=false,u32 number_format=4,u32 constant_lod=1,u32 external=0,bool query=false,bool controls=false){
    Common::ObjectPool<IR::Inst> pool{256};IR::Block block(pool);Info info;
    const auto add=[&](IR::Opcode op,std::initializer_list<IR::Value> args){block.AppendNewInst(op,args);return IR::Value{&block.back()};};
    const IR::Value zero{u32(0)};
    auto lod=dynamic?add(IR::Opcode::IAdd32,{zero,zero}):IR::Value{constant_lod};
    auto coords=add(IR::Opcode::CompositeConstructU32x4,{zero,zero,lod,zero});
    auto color=add(IR::Opcode::CompositeConstructF32x4,{IR::Value{1.f},IR::Value{1.f},IR::Value{1.f},IR::Value{1.f}});
    auto read=query?add(IR::Opcode::ImageQueryDimensions,{zero,lod,IR::Value{true}}):
        add(IR::Opcode::ImageRead,{zero,coords,{}, {}});
    const auto consumer=add(query?IR::Opcode::CompositeExtractU32x4:IR::Opcode::CompositeExtractF32x4,{read,zero});
    auto write=add(IR::Opcode::ImageWrite,{zero,coords,{}, {},color});
    IR::TextureInstInfo flags{};flags.has_lod.Assign(!query);read.Inst()->SetFlags(flags);write.Inst()->SetFlags(flags);
    AmdGpu::Image sharp{};sharp.base_address=0x100000;sharp.width=sharp.height=sharp.pitch=7;
    sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.last_level=1;sharp.data_format=4;sharp.num_format=number_format;sharp.tiling_index=13;
    if(external==5){sharp.type=u64(AmdGpu::ImageType::Color2DArray);sharp.depth=2;}
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    if(number_format==0||number_format==2||number_format==3){sharp.dst_sel_x=1;sharp.dst_sel_y=0;}
    Optimization::ResourceDiscovery r{},w{};r.user=read.Inst();w.user=write.Inst();
    u32 words[8];memcpy(words,&sharp,sizeof(sharp));
    r.sharps[0].num_dwords=w.sharps[0].num_dwords=8;
    for(u32 i=0;i<8;++i){r.sharps[0].dwords[i]=IR::Value{words[i]};w.sharps[0].dwords[i]=IR::Value{words[i]};}
    if(!same)w.sharps[0].dwords[0]=IR::Value{words[0]+256};
    if(loaded){
        info.flattened_ud_buf.resize(16);memcpy(info.flattened_ud_buf.data(),words,sizeof(words));
        memcpy(info.flattened_ud_buf.data()+8,words,sizeof(words));info.flattened_ud_buf[8]+=256;
        if(alias)info.flattened_ud_buf[8]-=256;
        if(alias&&controls){
            auto control=sharp;control.mtype=3;control.perf_modulation=7;
            control.counter_bank_id=255;control.min_lod_warn=4095;
            memcpy(info.flattened_ud_buf.data()+8,&control,sizeof(control));
            info.flattened_ud_buf[15]|=0x80000000U;
        }
        for(u32 i=0;i<8;++i){
            r.sharps[0].dwords[i]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(i)}});
            w.sharps[0].dwords[i]=same?r.sharps[0].dwords[i]:add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(i+8)}});
        }
    }
    Optimization::ResourceDiscoveryList resources;if(reverse){resources.push_back(w);resources.push_back(r);}else{resources.push_back(r);resources.push_back(w);}
    if(external){
        resources.clear();resources.push_back(r);
        auto other=sharp;if(external==2)other.base_address+=256;
        if(external==6)other.tiling_index=8;
        if(external==7)other.data_format=14;
        if(external==8){other.mtype=3;other.mtype_l2=2;other.perf_modulation=7;
            other.counter_bank_id=255;other.min_lod_warn=4095;}
        if(external==9){std::array<u32,8> raw{};memcpy(raw.data(),&other,sizeof(other));
            raw[7]|=0x80000000U;memcpy(&other,raw.data(),sizeof(other));}
        info.native_pipeline_writes.push_back({other,external==3||external==5,external==4});
    }
    Profile profile{};profile.native_coherent_images=native;
    Optimization::ResourcePatchingPass(info,resources,profile);
    if(external){
        const bool promoted=native&&(external==1||external==4||external==5||external==8||external==9);
        if(info.images.size()!=1||info.images[0].is_written!=promoted||
           info.images[0].native_srgb_read!=(promoted&&!query&&number_format==9))
            throw std::runtime_error("External stage write classification lost alias or sRGB semantics");
        if(promoted&&info.images[0].mip_fallback_mode!=
           (query?MipStorageFallbackMode::None:dynamic?MipStorageFallbackMode::DynamicIndex:MipStorageFallbackMode::ConstantIndex))
            throw std::runtime_error("External stage promotion lost mip view selection");
        return;
    }
    const bool unified=native&&same;
    const bool array_mips=dynamic||(native&&constant_lod>=2);
    if(native&&same&&number_format==9&&info.images[0].native_srgb_read)
        throw std::runtime_error("Original storage sRGB access unexpectedly gained SRV decoding");
    if(info.images.size()!=(unified?1u:2u))throw std::runtime_error("Read/write mip descriptors were not classified together");
    if(unified){const auto& image=info.images[0];
        if(!image.is_written||image.mip_fallback_mode!=(array_mips?MipStorageFallbackMode::DynamicIndex:MipStorageFallbackMode::ConstantIndex)||
            (!array_mips&&image.constant_mip_index!=constant_lod))throw std::runtime_error("Promoted mip read lost storage view selection");
        bool found=false;
        for(const auto& instruction:block)if(instruction.GetOpcode()==IR::Opcode::ImageRead){
            found=true;
            if(instruction.Arg(0).U32()!=0||instruction.Arg(2).IsEmpty()==array_mips)
                throw std::runtime_error("Patched read did not retain the correct mip operand");
        }
        if(!found)throw std::runtime_error("No patched image read");
        if(!dynamic&&array_mips)for(const auto& instruction:block){
            if(instruction.GetOpcode()!=IR::Opcode::ImageRead&&instruction.GetOpcode()!=IR::Opcode::ImageWrite)continue;
            if(!instruction.Arg(2).IsImmediate()||instruction.Arg(2).U32()!=constant_lod)
                throw std::runtime_error("Invalid constant mip was truncated before bounded access");
        }
    }
    if(alias&&native){
        for(const auto& image:info.images)
            if(!image.is_written||image.mip_fallback_mode!=(array_mips?MipStorageFallbackMode::DynamicIndex:MipStorageFallbackMode::ConstantIndex))
                throw std::runtime_error("Different T# sources aliasing storage did not both use UAVs");
    }
    if(native&&(number_format==0||number_format==2||number_format==3)){
        const auto* shuffle=consumer.Inst()->Arg(0).Inst();
        if(shuffle->GetOpcode()!=IR::Opcode::CompositeShuffleF32x4)
            throw std::runtime_error("Image constants were swizzled before numeric conversion");
        if(std::bit_cast<u32>(shuffle->Arg(0).Inst()->Arg(1).F32())!=0x3f800000U)
            throw std::runtime_error("Converted image ONE is not floating-point one");
        if(shuffle->Arg(2).U32()!=1||shuffle->Arg(3).U32()!=0||
            shuffle->Arg(0).Inst()->Arg(0).F32()!=0.f)
            throw std::runtime_error("Converted image forced channels do not select exact one and zero");
    }
}
static void RunPacked(u32 read_number,u32 write_number,bool native,bool external_hint,u32 read_format=9,u32 write_format=9){
    Common::ObjectPool<IR::Inst> pool{128};IR::Block block(pool);Info info;
    const auto add=[&](IR::Opcode op,std::initializer_list<IR::Value> args){block.AppendNewInst(op,args);return IR::Value{&block.back()};};
    const IR::Value zero{u32(0)};
    auto coords=add(IR::Opcode::CompositeConstructU32x4,{zero,zero,zero,zero});
    auto color=add(IR::Opcode::CompositeConstructF32x4,{IR::Value{1.f},IR::Value{1.f},IR::Value{1.f},IR::Value{1.f}});
    auto read=add(IR::Opcode::ImageRead,{zero,coords,{}, {}});
    auto write=add(IR::Opcode::ImageWrite,{zero,coords,{}, {},color});
    IR::TextureInstInfo flags{};read.Inst()->SetFlags(flags);write.Inst()->SetFlags(flags);
    AmdGpu::Image sharp{};sharp.base_address=0x100000;sharp.width=sharp.height=sharp.pitch=7;
    sharp.type=u64(AmdGpu::ImageType::Color2D);sharp.data_format=read_format;sharp.num_format=read_number;sharp.tiling_index=13;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    auto writer_sharp=sharp;writer_sharp.num_format=write_number;writer_sharp.data_format=write_format;
    info.flattened_ud_buf.resize(16);memcpy(info.flattened_ud_buf.data(),&sharp,sizeof(sharp));
    memcpy(info.flattened_ud_buf.data()+8,&writer_sharp,sizeof(writer_sharp));
    Optimization::ResourceDiscovery r{},w{};r.user=read.Inst();w.user=write.Inst();
    r.sharps[0].num_dwords=w.sharps[0].num_dwords=8;
    for(u32 index=0;index<8;++index){
        r.sharps[0].dwords[index]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(index)}});
        w.sharps[0].dwords[index]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(index+8)}});
    }
    Optimization::ResourceDiscoveryList resources;
    if(external_hint){
        auto hint=sharp;
        if(read_format==9){hint.num_format=1;info.native_pipeline_signed_packed_views.push_back({hint,false,false});}
        else info.native_pipeline_raw32_views.push_back({hint,false,false});
        resources.push_back(w);
    }
    else{resources.push_back(r);resources.push_back(w);}
    Profile profile{};profile.native_coherent_images=native;
    Optimization::ResourcePatchingPass(info,resources,profile);
    const auto signed_number=[](u32 number){return number==1||number==5;};
    const bool raw=native&&read_format==9&&(external_hint||signed_number(read_number)||signed_number(write_number));
    const bool raw32=native&&read_format!=write_format;
    if(info.images.size()!=(external_hint?1u:2u))throw std::runtime_error("Packed numeric access descriptions lost their T# identity");
    for(u32 index=0;index<info.images.size();++index){const auto& image=info.images[index];
        if(image.native_packed10_raw!=raw||image.native_raw32!=raw32||image.is_written!=(external_hint||index==1||native))
            throw std::runtime_error("Mixed packed numeric aliases lost common raw storage classification");
    }
}
static void RunReadonlyPacked(u32 number,bool native){
    Common::ObjectPool<IR::Inst> pool{128};IR::Block block(pool);Info info;
    const auto add=[&](IR::Opcode op,std::initializer_list<IR::Value> args){block.AppendNewInst(op,args);return IR::Value{&block.back()};};
    const IR::Value zero{u32(0)};auto coords=add(IR::Opcode::CompositeConstructU32x4,{zero,zero,zero,zero});
    auto read=add(IR::Opcode::ImageRead,{zero,coords,{}, {}}),raw_read=add(IR::Opcode::ImageRead,{zero,coords,{}, {}});
    IR::TextureInstInfo flags{};read.Inst()->SetFlags(flags);raw_read.Inst()->SetFlags(flags);
    AmdGpu::Image image{};image.base_address=0x100000;image.width=image.height=image.pitch=7;
    image.type=u64(AmdGpu::ImageType::Color2D);image.data_format=9;image.num_format=number;image.tiling_index=13;
    image.dst_sel_x=7;image.dst_sel_y=0;image.dst_sel_z=1;image.dst_sel_w=4;
    auto raw=image;raw.data_format=4;raw.num_format=4;
    info.flattened_ud_buf.resize(16);memcpy(info.flattened_ud_buf.data(),&image,sizeof(image));memcpy(info.flattened_ud_buf.data()+8,&raw,sizeof(raw));
    Optimization::ResourceDiscovery r{},u{};r.user=read.Inst();u.user=raw_read.Inst();r.sharps[0].num_dwords=u.sharps[0].num_dwords=8;
    for(u32 i=0;i<8;++i){r.sharps[0].dwords[i]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(i)}});
        u.sharps[0].dwords[i]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(i+8)}});}
    Optimization::ResourceDiscoveryList uses;uses.push_back(r);uses.push_back(u);Profile profile{};profile.native_coherent_images=native;
    Optimization::ResourcePatchingPass(info,uses,profile);
    if(info.images.size()!=2)throw std::runtime_error("Readonly packed/word views lost distinct original sharps");
    for(const auto& resource:info.images){const auto sharp=resource.GetSharp(info);
        if(resource.is_written||resource.mip_fallback_mode!=MipStorageFallbackMode::None||resource.native_raw32)
            throw std::runtime_error("Readonly packed decoding invented a writer or mip UAV");
        if(resource.native_packed10_raw!=(native&&u32(sharp.GetDataFmt())==9))
            throw std::runtime_error("Readonly packed decoding lost native/backend isolation");
    }
}
int main(){try{for(u32 number:{1u,5u})for(bool native:{false,true})RunReadonlyPacked(number,native);for(bool dynamic:{false,true})for(bool reverse:{false,true})for(bool same:{false,true})for(bool native:{false,true})for(bool loaded:{false,true})Run(dynamic,reverse,same,native,loaded);
    for(u32 read_number:{0U,1U,4U,5U})for(u32 write_number:{0U,1U,4U,5U})for(bool native:{false,true})RunPacked(read_number,write_number,native,false);
    for(u32 write_number:{0U,4U})for(bool native:{false,true})RunPacked(0,write_number,native,true);
    for(u32 scalar:{4U,5U,7U})for(u32 rgba:{0U,1U,4U,5U,9U})for(bool native:{false,true}){
        RunPacked(scalar,rgba,native,false,4,10);RunPacked(rgba,scalar,native,false,10,4);
        RunPacked(scalar,rgba,native,true,4,10);RunPacked(rgba,scalar,native,true,10,4);
    }
    for(bool dynamic:{false,true})for(bool reverse:{false,true})for(bool native:{false,true})Run(dynamic,reverse,false,native,true,true);
    for(bool dynamic:{false,true})for(bool reverse:{false,true})for(bool native:{false,true})Run(dynamic,reverse,false,native,true,true,4,1,0,false,true);
    for(u32 format:{0U,2U,3U})Run(false,false,true,true,true,false,format);
    Run(false,false,true,true,true,false,9);
    for(u32 level:{2U,256U,UINT32_MAX})for(bool reverse:{false,true})Run(false,reverse,true,true,true,false,4,level);
    for(u32 external:{1U,2U,3U,4U,5U,6U,7U,8U,9U})for(bool native:{false,true})for(bool dynamic:{false,true})for(u32 format:{4U,9U})Run(dynamic,false,false,native,true,false,format,1,external);
    for(u32 external:{1U,2U,3U,4U,5U,6U,7U,8U,9U})for(bool native:{false,true})for(u32 format:{4U,9U})Run(false,false,false,native,true,false,format,1,external,true);
    puts("PASS: image access classification preserves mip selection, resource identity and backend isolation");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
