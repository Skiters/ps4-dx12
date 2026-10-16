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
// Authored, unrun selected UINT writers sharing a retained BC parent domain.
inline void RunBcWritableAliasGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile,std::byte* memory,std::byte* backing,u64 base) {
    constexpr u64 Bytes=65536,ImageOffset=4096,OutputOffset=57344,OutputBytes=4096;
    constexpr u32 Layers=4,MaxSubresources=24,Chunk=1024,CopyOffset=2048;
    constexpr u32 Endpoints=0x07e0f800,Green=0x55555555,BufferValue=0x13579bdf,Poison=0xaaaaaaaa;
    struct alignas(256) Code {std::array<u32,128> words{};};
    std::array<Code,MaxSubresources> seed{},source_probe{},parent_before{},parent_after{},parent_final{};
    std::array<Code,8> uint_probe{};
    Code output_init{},warm{},marker{},raw{},writer{},formatted{};
    GuestShaderFrontend frontend(profile);
    struct Drain {GuestRasterizer& r;bool armed=true;~Drain(){if(armed)try{r.Finish();}catch(...){}}} drain{rasterizer};
    const auto initialize=[](Code& code,const std::vector<u32>& words,u64 hash){
        constexpr u32 Metadata=96;
        if(words.size()>Metadata||Metadata*4+sizeof(AmdGpu::BinaryInfo)>sizeof(code.words))
            throw std::runtime_error("Writable BC GCN metadata overlap");
        code.words.fill(0);std::copy(words.begin(),words.end(),code.words.begin());
        AmdGpu::BinaryInfo binary{};binary.signature=binary.signature_ref;binary.length=u32(words.size()*4);binary.shader_hash=hash;
        std::memcpy(code.words.data()+Metadata,&binary,sizeof(binary));
    };
    const auto bind=[&](const Code& code,u32 registers,u32 threads=1)->AmdGpu::ComputeProgram&{
        auto& p=gpu.GetCsRegs();p={};p.address=reinterpret_cast<u64>(code.words.data())>>8;
        p.num_thread_x.full=u16(threads);p.num_thread_y.full=p.num_thread_z.full=1;p.dim_x=p.dim_y=p.dim_z=1;
        p.settings.num_user_regs=registers;p.settings.num_vgprs=3;p.settings.num_sgprs=3;return p;
    };
    const auto move=[](std::vector<u32>& words,u32 reg,u32 value){words.insert(words.end(),{0x7e0002ffu|(reg<<17),value});};
    const auto coords=[&](std::vector<u32>& words,u32 start,u32 x,u32 y,u32 layer,u32 mip){
        move(words,start,x);move(words,start+1,y);move(words,start+2,layer);move(words,start+3,mip);
    };
    struct BufferExpectation {AmdGpu::Buffer sharp;bool written,formatted;};
    struct ImageExpectation {AmdGpu::Image sharp;bool written;u32 relative_mip;};
    const auto verify=[&](const auto& stage,std::initializer_list<BufferExpectation> buffers,
                          std::initializer_list<ImageExpectation> images={}){
        if(stage.spirv.empty()||stage.info.translation_failed||stage.info.uses_dma||stage.info.images.size()!=images.size())
            throw std::runtime_error("Writable BC GCN changed image count or translation/DMA path");
        auto next=buffers.begin();for(const auto& resource:stage.info.buffers)if(!resource.IsSpecial()){
            if(next==buffers.end())throw std::runtime_error("Writable BC gained guest buffer");const auto sharp=resource.GetSharp(stage.info);
            if(sharp.base_address!=next->sharp.base_address||sharp.GetSize()!=next->sharp.GetSize()||
               resource.is_written!=next->written||resource.is_formatted!=next->formatted)
                throw std::runtime_error("Writable BC changed original buffer bounds/access/order");++next;}
        if(next!=buffers.end())throw std::runtime_error("Writable BC removed guest buffer");
        auto expected=images.begin();for(const auto& resource:stage.info.images){const auto sharp=resource.GetSharp(stage.info);
            if(resource.is_written!=expected->written||!resource.is_array||resource.native_raw32||
               std::memcmp(&sharp,&expected->sharp,sizeof(sharp)))throw std::runtime_error("Writable BC changed original image slot/order/access");
            if(resource.is_written&&!profile.supports_image_load_store_lod&&
               (resource.mip_fallback_mode!=Shader::MipStorageFallbackMode::ConstantIndex||resource.constant_mip_index!=expected->relative_mip))
                throw std::runtime_error("Writable BC lost explicit view-relative LOD");++expected;}
    };
    // Single BC1, single BC3, two prefix orders, and true duplicate-UAV aliasing.
    for(u32 variant=0;variant<5;++variant){const bool bc3=variant==1||variant==4,group=variant==2||variant==3,duplicate=variant==4;
        const u32 width=bc3?16:32,levels=bc3?5:6,components=bc3?4:2,block_bytes=components*4;
        const u64 image_bytes=bc3?32768:49152;
        const u32 first_mip=variant==3?3:bc3?2:1,second_mip=duplicate?first_mip:variant==3?1:3;
        const u32 first_layer=variant==2?1:0,second_layer=variant==2?2:1;
        const u32 slot_count=group||duplicate?2:1;
        const auto hash=[&](u32 phase,u32 index=0){return 0x574243414c000000ULL+u64(variant)*8192+u64(phase)*256+index;};
        rasterizer.ReadMemory(base,Bytes);rasterizer.InvalidateMemory(base,Bytes);std::memset(memory,0x81+int(variant)*13,Bytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;image.type=u64(AmdGpu::ImageType::Color2DArray);
        image.data_format=bc3?14:11;image.num_format=4;image.width=image.height=image.pitch=width-1;
        image.last_level=levels-1;image.depth=image.last_array=Layers-1;image.tiling_index=5;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        auto bc=image;bc.width=bc.height=bc.pitch=width*4-1;bc.data_format=bc3?37:35;bc.num_format=0;
        auto desc=DescribeGuestTexture(image);desc.writable=true;const auto layout=LayoutGuestTexture(desc);
        const auto parent_desc=DescribeGuestTexture(bc);const auto parent_layout=LayoutGuestTexture(parent_desc);
        if(base%4096||layout.guest_bytes!=image_bytes||parent_layout.guest_bytes!=image_bytes||
           parent_layout.logical_bytes!=layout.logical_bytes||parent_layout.expanded_bytes_per_pixel||
           parent_layout.format!=(bc3?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_BC1_UNORM)||ImageOffset+image_bytes>OutputOffset)
            throw std::runtime_error("Writable BC needs exact page-aligned parent word/block chain");
        for(u32 mip=0;mip<levels;++mip){const auto& a=layout.mips.at(mip);const auto& b=parent_layout.mips.at(mip);
            if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)
                throw std::runtime_error("Writable BC full parent physical mip correspondence changed");}
        std::array<AmdGpu::Image,2> selected{image,image};selected[0].base_level=selected[0].last_level=first_mip;
        selected[1].base_level=selected[1].last_level=second_mip;
        if(variant==2){selected[0].base_array=selected[0].last_array=first_layer;
            selected[1].base_array=selected[1].last_array=second_layer;}
        for(u32 slot=0;slot<slot_count;++slot){auto view=DescribeGuestTexture(selected[slot]);view.writable=true;const auto prefix=LayoutGuestTexture(view);
            if(view.first_mip!=view.levels-1||!view.view_layers||view.first_layer+view.view_layers>Layers||
               prefix.guest_bytes%4096||prefix.guest_bytes>image_bytes)
                throw std::runtime_error("Writable BC selected prefix violates retained page/full-layer limits");
            for(u32 mip=0;mip<view.levels;++mip){const auto& a=prefix.mips[mip];const auto& b=parent_layout.mips[mip];
                if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)
                    throw std::runtime_error("Writable BC selected prefix maps a different physical parent mip");}}
        const auto block=[&](bool green,u32 tag){const u32 endpoints=green?Endpoints|tag:Endpoints|(tag<<16);
            return bc3?std::array<u32,4>{0xffu|(tag<<8),0,endpoints,green?Green:0}:
                std::array<u32,4>{endpoints,green?Green:0,0,0};};
        const auto put=[&](std::byte* bytes,u32 mip,u32 layer,u32 x,bool green,u32 tag){const auto value=block(green,tag);
            std::memcpy(bytes+GuestPixelOffset(desc,layout,mip,layer,x,0),value.data(),block_bytes);};
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<levels;++mip)
        for(u32 y=0;y<std::max(width>>mip,1u);++y)for(u32 x=0;x<std::max(width>>mip,1u);++x){const auto value=block(false,0);
            std::memcpy(memory+ImageOffset+GuestPixelOffset(desc,layout,mip,layer,x,y),value.data(),block_bytes);}
        std::vector<std::byte> expected(backing,backing+Bytes);
        const auto expect=[&](u64 offset,u32 value){std::memcpy(expected.data()+offset,&value,4);};
        auto logical=std::vector<std::byte>(expected.begin()+ImageOffset,expected.begin()+ImageOffset+image_bytes);
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<levels;++mip)put(logical.data(),mip,layer,0,true,1+layer*levels+mip);
        AmdGpu::Buffer target{};target.base_address=base+ImageOffset;target.stride=4;target.num_records=u32(image_bytes/4);target.data_format=4;target.num_format=4;
        target.dst_sel_x=4;target.dst_sel_y=5;target.dst_sel_z=6;target.dst_sel_w=7;
        auto output=target;output.base_address=base+OutputOffset;output.num_records=OutputBytes/4;
        auto full=target;full.base_address=base;full.num_records=Bytes/4;
        initialize(output_init,{0x7e0802ff,0x4c4c4c4c,0xe0702000,0x08000400,0xbf810000},hash(1));
        for(u32 offset=0;offset<OutputBytes;offset+=Chunk){auto& p=bind(output_init,9,256);std::memcpy(p.user_data.data(),&output,sizeof(output));p.user_data[8]=offset;
            verify(frontend.BuildCompute(gpu),{{output,true,false}});rasterizer.DispatchDirect();}
        std::memset(expected.data()+OutputOffset,0x4c,OutputBytes);
        initialize(warm,{0xe0300000,0x80000400,0xbf8c0000,0xe070019c,0x80010400,0xbf810000},hash(2));
        auto& wp=bind(warm,8);std::memcpy(wp.user_data.data(),&full,sizeof(full));std::memcpy(wp.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{full,false,false},{output,true,false}});rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.Finish();u32 initial{};std::memcpy(&initial,expected.data(),4);expect(OutputOffset+412,initial);
        if(std::memcmp(backing,expected.data(),Bytes))throw std::runtime_error("Writable BC warm physical baseline changed");
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<levels;++mip){const u32 index=layer*levels+mip;std::vector<u32> words;
            if(!index){move(words,1,BufferValue);words.insert(words.end(),{0xe0700000,0x80000100});}
            coords(words,2,0,0,layer,mip);const auto value=block(true,index+1);for(u32 c=0;c<components;++c)move(words,8+c,value[c]);
            words.insert(words.end(),{bc3?0xf0244f00u:0xf0244300u,0x00010802,0xbf810000});initialize(seed[index],words,hash(3,index));
            auto& p=bind(seed[index],12);std::memcpy(p.user_data.data(),&target,sizeof(target));std::memcpy(p.user_data.data()+4,&image,sizeof(image));
            if(!index)verify(frontend.BuildCompute(gpu),{{target,true,false}},{{image,true,mip}});else verify(frontend.BuildCompute(gpu),{},{{image,true,mip}});
            rasterizer.DispatchDirect();}
        const auto probe_uint=[&](Code& code,const AmdGpu::Image& sharp,u32 layer,u32 mip,u32 output_start,u64 identity){
            std::vector<u32> words;for(u32 x=0;x<2;++x){coords(words,0,x,0,layer,mip);
                words.insert(words.end(),{bc3?0xf0044f00u:0xf0044300u,0x00000400,0xbf8c0000});
                for(u32 c=0;c<components;++c){words.insert(words.end(),{0xe0700000u+output_start+x*16+c*4,0x80020000u|((4+c)<<8)});
                    u32 value{};const u32 absolute=u32(sharp.base_level)+mip;std::memcpy(&value,logical.data()+GuestPixelOffset(desc,layout,absolute,layer,x,0)+c*4,4);
                    expect(OutputOffset+output_start+x*16+c*4,value);}}
            words.push_back(0xbf810000);initialize(code,words,identity);auto& p=bind(code,12);std::memcpy(p.user_data.data(),&sharp,sizeof(sharp));
            std::memcpy(p.user_data.data()+8,&output,sizeof(output));verify(frontend.BuildCompute(gpu),{{output,true,false}},{{sharp,false,mip}});rasterizer.DispatchDirect();};
        // Probe one valid block per source subresource, including terminal1x1.
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<levels;++mip){const u32 index=layer*levels+mip;std::vector<u32> words;
            coords(words,0,0,0,layer,mip);words.insert(words.end(),{bc3?0xf0044f00u:0xf0044300u,0x00000400,0xbf8c0000});
            const auto value=block(true,index+1);for(u32 c=0;c<components;++c){words.insert(words.end(),{0xe0700000u+index*16+c*4,0x80020000u|((4+c)<<8)});
                expect(OutputOffset+index*16+c*4,value[c]);}words.push_back(0xbf810000);initialize(source_probe[index],words,hash(4,index));
            auto& p=bind(source_probe[index],12);std::memcpy(p.user_data.data(),&image,sizeof(image));std::memcpy(p.user_data.data()+8,&output,sizeof(output));
            verify(frontend.BuildCompute(gpu),{{output,true,false}},{{image,false,mip}});rasterizer.DispatchDirect();}
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.ReadMemory(target.base_address,image_bytes);rasterizer.Finish();expect(ImageOffset,BufferValue);
        if(rasterizer.IsGpuBufferModified(target.base_address,image_bytes)||std::memcmp(backing,expected.data(),Bytes))
            throw std::runtime_error("Writable BC source failed independent I versus clean B/P precondition");
        const auto physical_before=expected;
        const auto sample_parent=[&](auto& programs,u32 output_start,u32 phase,bool changed){
            for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<levels;++mip){const u32 index=layer*levels+mip;std::vector<u32> words;
                coords(words,0,0,0,layer,mip);words.insert(words.end(),{0xf0044f00,0x00000400,0xbf8c0000});
                const bool red=changed&&((mip==first_mip&&layer==first_layer)||(group&&mip==second_mip&&layer==second_layer));
                for(u32 c=0;c<4;++c){words.insert(words.end(),{0xe0700000u+output_start+index*16+c*4,0x80020000u|((4+c)<<8)});
                    const bool one=c==3||(red?c==0:c==1);expect(OutputOffset+output_start+index*16+c*4,std::bit_cast<u32>(one?1.f:0.f));}
                words.push_back(0xbf810000);initialize(programs[index],words,hash(phase,index));auto& p=bind(programs[index],12);
                std::memcpy(p.user_data.data(),&bc,sizeof(bc));std::memcpy(p.user_data.data()+8,&output,sizeof(output));
                verify(frontend.BuildCompute(gpu),{{output,true,false}},{{bc,false,mip}});rasterizer.DispatchDirect();}};
        sample_parent(parent_before,512,5,false); // Establish a real retained BC parent before aliasing.
        const u64 poison_offset=bc3?12:4;auto poison=target;poison.base_address+=poison_offset;poison.num_records=1;
        initialize(marker,{0x7e0202ff,Poison,0xe0700000,0x80000100,0xbf810000},hash(6));auto& mp=bind(marker,4);
        std::memcpy(mp.user_data.data(),&poison,sizeof(poison));verify(frontend.BuildCompute(gpu),{{poison,true,false}});rasterizer.DispatchDirect();
        initialize(raw,{0xe0300000,0x80000400,0xe0300000u|u32(poison_offset),0x80000500,0xbf8c0000,
            0xe0700190,0x80010400,0xe0700194,0x80010500,0xbf810000},hash(7));auto& rp=bind(raw,8);
        std::memcpy(rp.user_data.data(),&target,sizeof(target));std::memcpy(rp.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{target,false,false},{output,true,false}});rasterizer.DispatchDirect();expect(OutputOffset+400,BufferValue);expect(OutputOffset+404,Poison);
        const auto read_gpu=[&]{auto snapshot=rasterizer.CapturePreparedShaderBuffer(output.base_address,OutputBytes);ComPtr<ID3D12Device> device;
            Check(snapshot.handle->GetDevice(IID_PPV_ARGS(&device)));auto readback=Buffer(device.Get(),OutputBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([snapshot,readback](u64){(void)snapshot;(void)readback;});auto* list=rasterizer.RecordingList();snapshot.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,OutputBytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();void* mapped{};
            D3D12_RANGE range{0,SIZE_T(OutputBytes)};Check(readback.handle->Map(0,&range,&mapped));const bool valid=std::memcmp(mapped,expected.data()+OutputOffset,OutputBytes)==0;
            D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);if(!valid||!rasterizer.IsGpuBufferModified(target.base_address,image_bytes)||
                std::memcmp(backing,physical_before.data(),Bytes))throw std::runtime_error("Writable BC GPU observations or unpublished B/P dirty witness changed");};
        read_gpu();
        std::vector<u32> write_words;coords(write_words,2,0,0,0,0);const auto first_value=block(false,25);
        for(u32 c=0;c<components;++c)move(write_words,8+c,first_value[c]);
        write_words.insert(write_words.end(),{bc3?0xf0244f00u:0xf0244300u,0x00000802,0xbf8c0000});
        put(logical.data(),first_mip,first_layer,0,false,25);
        if(group){coords(write_words,2,0,0,variant==2?0:second_layer,0);const auto second_value=block(false,26);for(u32 c=0;c<components;++c)move(write_words,8+c,second_value[c]);
            write_words.insert(write_words.end(),{0xf0244300,0x00020802,0xbf8c0000});put(logical.data(),second_mip,second_layer,0,false,26);}
        if(duplicate){coords(write_words,2,0,0,0,0);write_words.insert(write_words.end(),{0xf0044f00,0x00020802,0xbf8c0000});
            coords(write_words,2,1,0,0,0);write_words.insert(write_words.end(),{0xf0244f00,0x00020802,0xbf8c0000});put(logical.data(),first_mip,0,1,false,25);}
        write_words.push_back(0xbf810000);initialize(writer,write_words,hash(8));auto& aw=bind(writer,slot_count*8);
        std::memcpy(aw.user_data.data(),&selected[0],sizeof(image));if(slot_count==2)std::memcpy(aw.user_data.data()+8,&selected[1],sizeof(image));
        if(slot_count==1)verify(frontend.BuildCompute(gpu),{},{{selected[0],true,0}});
        else verify(frontend.BuildCompute(gpu),{},{{selected[0],true,0},{selected[1],true,0}});
        rasterizer.DispatchDirect();read_gpu(); // No guest buffer operand shares parent storage in this dispatch.
        sample_parent(parent_after,1024,9,true);read_gpu();
        // Fresh readonly UINT prefix views must see committed I, while later
        // complete BC binds still retain all untouched parent mips/layers.
        for(u32 slot=0;slot<slot_count;++slot){auto probe=selected[slot];probe.base_array=0;probe.last_array=Layers-1;
            for(u32 layer=0;layer<Layers;++layer)
                probe_uint(uint_probe[slot*Layers+layer],probe,layer,0,1536+(slot*Layers+layer)*32,hash(10,slot*Layers+layer));}
        read_gpu();sample_parent(parent_final,512,11,true);read_gpu();
        initialize(formatted,{0xe0002000,0x08000400,0xbf8c0000,0xe0702000,0x80010400,0xbf810000},hash(12));
        auto copied=target;copied.base_address=base+OutputOffset+CopyOffset;copied.num_records=Chunk/4;
        for(u32 offset=0;offset<image_bytes;offset+=Chunk){auto& p=bind(formatted,9,256);std::memcpy(p.user_data.data(),&target,sizeof(target));
            std::memcpy(p.user_data.data()+4,&copied,sizeof(copied));p.user_data[8]=offset;
            verify(frontend.BuildCompute(gpu),{{target,false,true},{copied,true,false}});rasterizer.DispatchDirect();
            std::memcpy(expected.data()+OutputOffset+CopyOffset,logical.data()+offset,Chunk);read_gpu();}
        std::memcpy(expected.data()+ImageOffset+poison_offset,logical.data()+poison_offset,4);
        rasterizer.ReadMemory(base,Bytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(std::memcmp(backing,expected.data(),Bytes))throw std::runtime_error("Writable BC encoded words, untouched mips/layers, physical padding or dirty-union mismatch");
        std::printf("PASS: real GCN writable %s %s; selected/grouped UAVs, exact parent blocks and64KiB physical oracle\n",bc3?"BC3":"BC1",
            duplicate?"duplicate same-mip coherent slots":group?(variant==2?"smaller-prefix first":"larger-prefix first"):"single nonzero mip");
    }
    rasterizer.Finish();drain.armed=false;
}
} // namespace NativeDX12
