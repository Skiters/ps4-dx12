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

// Authored and unrun. Forward word-image -> BC mip chains; no reverse image view.
inline void RunBcMipExpansionGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile,std::byte* memory,std::byte* backing,u64 base) {
    constexpr u64 AllocationBytes=65536,ImageOffset=4096,OutputOffset=57344,OutputBytes=4096;
    constexpr u32 BufferValue=0x13579bdf,PoisonValue=0xaaaaaaaa,MarkerValue=0x2468ace0;
    constexpr u32 MaxSubresources=24,CopyOffset=2048,ChunkBytes=1024;
    struct alignas(256) Code {std::array<u32,128> words{};};
    std::array<Code,MaxSubresources> seed{},before{},sample{},after{};
    Code warm{},writer{},marker{},raw{},formatted{};
    GuestShaderFrontend frontend(profile);
    struct Drain {GuestRasterizer& rasterizer;bool armed=true;
        ~Drain(){if(armed)try{rasterizer.Finish();}catch(...){}}} drain{rasterizer};
    const auto initialize=[](Code& code,const std::vector<u32>& words,u64 hash){
        constexpr u32 MetadataWord=96;
        if(words.size()>MetadataWord||MetadataWord*4+sizeof(AmdGpu::BinaryInfo)>sizeof(code.words))
            throw std::runtime_error("BC mip GCN overlaps binary metadata");
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
    const auto move=[](std::vector<u32>& words,u32 reg,u32 value){words.insert(words.end(),{
        0x7e0002ffu|(reg<<17),value});};
    const auto coords=[&](std::vector<u32>& words,u32 start,u32 layer,u32 mip){
        // Color2DArray with DA=1: x,y,array-layer,integer LOD.
        move(words,start,0);move(words,start+1,0);move(words,start+2,layer);move(words,start+3,mip);
    };
    struct BufferExpectation {AmdGpu::Buffer sharp;bool written,formatted;};
    const auto verify=[&](const auto& stage,std::initializer_list<BufferExpectation> buffers,
                          const AmdGpu::Image* image=nullptr,bool written=false,u32 mip=0){
        if(stage.spirv.empty()||stage.info.translation_failed||stage.info.uses_dma||
           stage.info.images.size()!=(image?1u:0u))throw std::runtime_error("BC mip GCN lost image/DMA operands");
        auto next=buffers.begin();
        for(const auto& buffer:stage.info.buffers)if(!buffer.IsSpecial()){
            if(next==buffers.end())throw std::runtime_error("BC mip GCN gained a guest buffer");
            const auto sharp=buffer.GetSharp(stage.info);
            if(sharp.base_address!=next->sharp.base_address||sharp.GetSize()!=next->sharp.GetSize()||
               buffer.is_written!=next->written||buffer.is_formatted!=next->formatted)
                throw std::runtime_error("BC mip GCN changed sharp bounds/access/first-use order");
            ++next;
        }
        if(next!=buffers.end())throw std::runtime_error("BC mip GCN removed a guest buffer");
        if(image){const auto& resource=stage.info.images.front();const auto sharp=resource.GetSharp(stage.info);
            if(resource.is_written!=written||!resource.is_array||resource.native_raw32||
               std::memcmp(&sharp,image,sizeof(sharp)))throw std::runtime_error("BC mip GCN changed array sharp/access");
            if(written&&!profile.supports_image_load_store_lod&&
               (resource.mip_fallback_mode!=Shader::MipStorageFallbackMode::ConstantIndex||resource.constant_mip_index!=mip))
                throw std::runtime_error("BC mip IMAGE_STORE_MIP lost its explicit constant LOD");}
    };
    struct Case {bool bc3;u32 width,height,pitch,levels,layers,bytes;};
    constexpr std::array<Case,3> Cases{{{false,32,32,32,6,2,24576},
        {true,24,24,32,4,2,36864},{false,32,4,32,6,4,20480}}};
    for(u32 variant=0;variant<Cases.size();++variant){const auto c=Cases[variant];
        const u64 identity=u64(variant)<<12;
        const auto hash=[&](u32 phase,u32 index=0){return 0x42434d4950000000ULL+identity+u64(phase)*256+index;};
        const u32 components=c.bc3?4:2,block_bytes=components*4,index_word=c.bc3?3:1;
        if(c.levels*c.layers>MaxSubresources)throw std::runtime_error("BC mip code-array extent exceeded");
        rasterizer.ReadMemory(base,AllocationBytes);rasterizer.InvalidateMemory(base,AllocationBytes);
        std::memset(memory,0xa1+int(variant)*17,AllocationBytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;
        image.type=u64(AmdGpu::ImageType::Color2DArray);image.data_format=c.bc3?14:11;image.num_format=4;
        image.width=c.width-1;image.height=c.height-1;image.pitch=c.pitch-1;image.last_level=c.levels-1;
        image.depth=image.last_array=c.layers-1;image.tiling_index=5;
        image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        auto bc=image;bc.width=c.width*4-1;bc.height=c.height*4-1;bc.pitch=c.pitch*4-1;
        bc.data_format=c.bc3?37:35;bc.num_format=0;
        auto desc=DescribeGuestTexture(image);desc.writable=true;
        const auto layout=LayoutGuestTexture(desc);const auto sampled=DescribeGuestTexture(bc);
        const auto bc_layout=LayoutGuestTexture(sampled);
        if(base%4096||layout.guest_bytes!=c.bytes||layout.guest_bytes%4096||
           ImageOffset+layout.guest_bytes>OutputOffset||sampled.writable||bc_layout.expanded_bytes_per_pixel||
           bc_layout.block_extent!=4||bc_layout.format!=(c.bc3?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_BC1_UNORM)||
           bc_layout.guest_bytes!=layout.guest_bytes||bc_layout.logical_bytes!=layout.logical_bytes||
           bc_layout.mips.size()!=c.levels||layout.mips.size()!=c.levels)
            throw std::runtime_error("BC mip fixture needs native BC and page-aligned equal complete block storage");
        for(u32 mip=0;mip<c.levels;++mip){const auto& a=layout.mips[mip];const auto& b=bc_layout.mips[mip];
            if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||
               a.macro!=b.macro||a.thickness!=b.thickness||
               std::max(1u,c.width>>mip)!=(std::max(1u,c.width*4>>mip)+3)/4||
               std::max(1u,c.height>>mip)!=(std::max(1u,c.height*4>>mip)+3)/4)
                throw std::runtime_error("BC mip physical layout or per-axis block correspondence changed");}
        GuestTextureCache::Entry shape;shape.desc=sampled;shape.layout=bc_layout;
        const auto owner=DescribeGuestImageOwnership(shape);
        if(!owner||owner->desc.levels!=c.levels||owner->desc.layers!=c.layers||
           owner->desc.width!=c.width||owner->desc.height!=c.height||shape.Srv().Format!=bc_layout.format)
            throw std::runtime_error("BC mip canonical readonly ownership lost full chain/layers");
        // Negative shape controls have no fake GPU/history state and no commands.
        auto negative=sampled;negative.width=negative.height=96;negative.pitch=128;negative.levels=5;
        shape.desc=negative;shape.layout=LayoutGuestTexture(negative);
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("24->96 overlong fifth mip acquired ownership");
        negative=sampled;negative.width=128;negative.height=12;negative.pitch=128;negative.levels=2;
        shape.desc=negative;shape.layout=LayoutGuestTexture(negative);
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("32x3->128x12 mismatched second mip acquired ownership");
        negative=sampled;negative.width=negative.height=negative.pitch=128;negative.levels=7;
        shape.desc=negative;shape.layout=LayoutGuestTexture(negative);
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("Overlong canonical UINT chain acquired ownership");
        shape.desc=sampled;shape.layout=bc_layout;shape.layout.mips.back().offset+=256;
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("Mismatched physical mip offset acquired ownership");
        shape.layout=bc_layout;shape.layout.mips.back().pitch+=8;
        if(DescribeGuestImageOwnership(shape))throw std::runtime_error("Mismatched physical mip stride acquired ownership");
        const auto green=[](u32 mip,u32 layer){return ((mip+layer)&1u)==0;};
        const auto block=[&](bool selected_green,u32 tag){
            // Only the unused endpoint changes: every decoded texel stays pure
            // red/green, but all mip/layer block words have distinct exact tags.
            const u32 endpoints=((0x07e0u|(selected_green?0:tag))<<16)|(0xf800u|(selected_green?tag:0));
            const u32 indices=selected_green?0x55555555u:0;
            return c.bc3?std::array<u32,4>{0xffu|(tag<<8),0,endpoints,indices}:
                std::array<u32,4>{endpoints,indices,0,0};
        };
        const auto put=[&](std::byte* bytes,u32 mip,u32 layer,u32 x,u32 y,bool selected_green,u32 tag){
            const auto value=block(selected_green,tag);
            std::memcpy(bytes+GuestPixelOffset(desc,layout,mip,layer,x,y),value.data(),block_bytes);};
        for(u32 layer=0;layer<c.layers;++layer)for(u32 mip=0;mip<c.levels;++mip)
        for(u32 y=0;y<std::max(1u,c.height>>mip);++y)for(u32 x=0;x<std::max(1u,c.width>>mip);++x)
            put(memory+ImageOffset,mip,layer,x,y,green(mip,layer)^((x+y)%2==0),1+(mip+layer*c.levels+x*3+y*5)%31);
        std::vector<std::byte> expected(backing,backing+AllocationBytes);
        auto logical=std::vector<std::byte>(expected.begin()+ImageOffset,expected.begin()+ImageOffset+layout.guest_bytes);
        for(u32 layer=0;layer<c.layers;++layer)for(u32 mip=0;mip<c.levels;++mip)
            put(logical.data(),mip,layer,0,0,green(mip,layer),1+mip+layer*c.levels);
        const auto expect=[&](u64 offset,u32 word){std::memcpy(expected.data()+offset,&word,4);};
        AmdGpu::Buffer target{};target.base_address=base+ImageOffset;target.stride=4;
        target.num_records=u32(layout.guest_bytes/4);target.data_format=4;target.num_format=4;
        target.dst_sel_x=4;target.dst_sel_y=5;target.dst_sel_z=6;target.dst_sel_w=7;
        auto output=target;output.base_address=base+OutputOffset;output.num_records=OutputBytes/4;
        auto full=target;full.base_address=base;full.num_records=AllocationBytes/4;
        initialize(warm,{0xe0300000,0x80000400,0xbf8c0000,0xe070019c,0x80010400,0xbf810000},hash(1));
        auto& wp=bind(warm,8);std::memcpy(wp.user_data.data(),&full,sizeof(full));
        std::memcpy(wp.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{full,false,false},{output,true,false}});rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.Finish();
        u32 initial{};std::memcpy(&initial,expected.data(),4);expect(OutputOffset+412,initial);
        if(std::memcmp(backing,expected.data(),AllocationBytes))throw std::runtime_error("BC mip warm arena changed physical bytes");
        for(u32 layer=0;layer<c.layers;++layer)for(u32 mip=0;mip<c.levels;++mip){const u32 index=layer*c.levels+mip;
            std::vector<u32> words;if(!index){move(words,1,BufferValue);words.insert(words.end(),{0xe0700000,0x80000100});}
            coords(words,2,layer,mip);const auto value=block(green(mip,layer),index+1);
            for(u32 component=0;component<components;++component)move(words,8+component,value[component]);
            words.insert(words.end(),{c.bc3?0xf0244f00u:0xf0244300u,0x00010802,0xbf810000});
            initialize(seed[index],words,hash(2,index));auto& program=bind(seed[index],12);
            std::memcpy(program.user_data.data(),&target,sizeof(target));std::memcpy(program.user_data.data()+4,&image,sizeof(image));
            if(!index)verify(frontend.BuildCompute(gpu),{{target,true,false}},&image,true,mip);
            else verify(frontend.BuildCompute(gpu),{},&image,true,mip);
            rasterizer.DispatchDirect();}
        for(u32 layer=0;layer<c.layers;++layer)for(u32 mip=0;mip<c.levels;++mip){const u32 index=layer*c.levels+mip;
            std::vector<u32> words;coords(words,0,layer,mip);
            words.insert(words.end(),{c.bc3?0xf0044f00u:0xf0044300u,0x00000400,0xbf8c0000});
            const auto value=block(green(mip,layer),index+1);
            for(u32 component=0;component<components;++component){words.insert(words.end(),{
                0xe0700000u+index*16+component*4,0x80020000u|((4+component)<<8)});expect(OutputOffset+index*16+component*4,value[component]);}
            words.push_back(0xbf810000);initialize(before[index],words,hash(3,index));auto& program=bind(before[index],12);
            std::memcpy(program.user_data.data(),&image,sizeof(image));std::memcpy(program.user_data.data()+8,&output,sizeof(output));
            verify(frontend.BuildCompute(gpu),{{output,true,false}},&image,false,mip);rasterizer.DispatchDirect();}
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.ReadMemory(target.base_address,target.GetSize());
        rasterizer.Finish();expect(ImageOffset,BufferValue);
        if(rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||std::memcmp(backing,expected.data(),AllocationBytes))
            throw std::runtime_error("BC mip seed lost per-mip GPU I, clean distinct B or physical padding");
        const auto physical_before=expected;
        const u64 poison_offset=u64(index_word)*4;
        const u64 marker_offset=GuestPixelOffset(desc,layout,c.levels-1,c.layers-1,0,0)+u64(index_word)*4;
        if(!poison_offset||marker_offset<=poison_offset+4||marker_offset+4>layout.guest_bytes)
            throw std::runtime_error("BC mip interior dirty DWORDs overlap or exceed source");
        const auto write=[&](Code& code,u64 offset,u32 value,u32 phase){auto sharp=target;sharp.base_address+=offset;sharp.num_records=1;
            initialize(code,{0x7e0202ff,value,0xe0700000,0x80000100,0xbf810000},hash(phase));auto& program=bind(code,4);
            std::memcpy(program.user_data.data(),&sharp,sizeof(sharp));verify(frontend.BuildCompute(gpu),{{sharp,true,false}});rasterizer.DispatchDirect();};
        write(writer,poison_offset,PoisonValue,4);write(marker,marker_offset,MarkerValue,5);
        initialize(raw,{0xe0300000,0x80000400,0xe0300000u|u32(poison_offset),0x80000500,
            0xe0300000,0x08000600,0xbf8c0000,0xe0700190,0x80010400,
            0xe0700194,0x80010500,0xe0700198,0x80010600,0xbf810000},hash(6));
        auto& rp=bind(raw,9);std::memcpy(rp.user_data.data(),&target,sizeof(target));
        std::memcpy(rp.user_data.data()+4,&output,sizeof(output));rp.user_data[8]=u32(marker_offset);
        verify(frontend.BuildCompute(gpu),{{target,false,false},{output,true,false}});rasterizer.DispatchDirect();
        expect(OutputOffset+400,BufferValue);expect(OutputOffset+404,PoisonValue);expect(OutputOffset+408,MarkerValue);
        const auto sample_bc=[&](auto& programs,u32 output_offset,u32 phase){
            for(u32 layer=0;layer<c.layers;++layer)for(u32 mip=0;mip<c.levels;++mip){const u32 index=layer*c.levels+mip;
                std::vector<u32> words;coords(words,0,layer,mip);words.insert(words.end(),{0xf0044f00,0x00000400,0xbf8c0000});
                const bool g=green(mip,layer);const std::array<float,4> rgba{g?0.f:1.f,g?1.f:0.f,0.f,1.f};
                for(u32 component=0;component<4;++component){words.insert(words.end(),{
                    0xe0700000u+output_offset+index*16+component*4,0x80020000u|((4+component)<<8)});
                    expect(OutputOffset+output_offset+index*16+component*4,std::bit_cast<u32>(rgba[component]));}
                words.push_back(0xbf810000);initialize(programs[index],words,hash(phase,index));auto& program=bind(programs[index],12);
                std::memcpy(program.user_data.data(),&bc,sizeof(bc));std::memcpy(program.user_data.data()+8,&output,sizeof(output));
                verify(frontend.BuildCompute(gpu),{{output,true,false}},&bc,false,mip);rasterizer.DispatchDirect();}
        };
        sample_bc(sample,512,7);
        if(!rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||std::memcmp(backing,physical_before.data(),AllocationBytes))
            throw std::runtime_error("BC mip expansion published B or I before compressed observations");
        // Every byte of all mips/layers/padding passes through this real GCN
        // formatted reader. v0 local X indexes256 DWORDs; s8 selects the chunk.
        initialize(formatted,{0xe0002000,0x08000400,0xbf8c0000,0xe0702000,0x80010400,0xbf810000},hash(8));
        auto copied=target;copied.base_address=base+OutputOffset+CopyOffset;copied.num_records=ChunkBytes/4;
        for(u32 offset=0;offset<layout.guest_bytes;offset+=ChunkBytes){auto& program=bind(formatted,9,256);
            std::memcpy(program.user_data.data(),&target,sizeof(target));std::memcpy(program.user_data.data()+4,&copied,sizeof(copied));
            program.user_data[8]=offset;verify(frontend.BuildCompute(gpu),{{target,false,true},{copied,true,false}});rasterizer.DispatchDirect();
            std::memcpy(expected.data()+OutputOffset+CopyOffset,logical.data()+offset,ChunkBytes);
            // Reading the guest output would resolve every dirty span in its
            // shared cached Mapping, including source W. Observe GPU bytes in
            // a private snapshot/readback without requesting CPU guest access.
            auto snapshot=rasterizer.CapturePreparedShaderBuffer(output.base_address,OutputBytes);
            ComPtr<ID3D12Device> device;Check(snapshot.handle->GetDevice(IID_PPV_ARGS(&device)));
            auto readback=Buffer(device.Get(),OutputBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([snapshot,readback](u64){(void)snapshot;(void)readback;});
            auto* list=rasterizer.RecordingList();snapshot.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,OutputBytes);
            rasterizer.Finish();rasterizer.ValidateDebugMessages();
            void* mapped{};D3D12_RANGE range{0,SIZE_T(OutputBytes)};Check(readback.handle->Map(0,&range,&mapped));
            const bool valid=std::memcmp(mapped,expected.data()+OutputOffset,OutputBytes)==0;
            D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
            if(!valid||!rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||
               std::memcmp(backing,physical_before.data(),AllocationBytes))
                throw std::runtime_error("BC mip compressed DWORD/padding chunk or physical dirty-source guard mismatch");}
        sample_bc(after,1024,9);
        if(!rasterizer.IsGpuBufferModified(target.base_address,target.GetSize())||
           std::memcmp(backing,physical_before.data(),AllocationBytes))
            throw std::runtime_error("BC mip post-retile samples published source or observation bytes early");
        // No full-chain reverse UINT image view: reverse block-texel views are
        // constrained to one mip. The accepted single-mip fixture covers it.
        std::memcpy(expected.data()+ImageOffset+poison_offset,logical.data()+poison_offset,4);
        std::memcpy(expected.data()+ImageOffset+marker_offset,logical.data()+marker_offset,4);
        rasterizer.ReadMemory(base,AllocationBytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(std::memcmp(backing,expected.data(),AllocationBytes))throw std::runtime_error("BC mip final dirty union/array/guard oracle mismatch");
        std::printf("PASS: real GCN %s mip bridge %ux%u levels%u layers%u; every compressed DWORD/padding and64KiB dirty union\n",
            c.bc3?"BC3":"BC1",c.width,c.height,c.levels,c.layers);
    }
    rasterizer.Finish();drain.armed=false;
}
} // namespace NativeDX12
