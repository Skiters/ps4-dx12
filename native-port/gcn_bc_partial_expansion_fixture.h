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
// Authored, unrun partial common-mip expansion. Every observation is real GCN.
inline void RunBcPartialExpansionGcnFixture(GuestRasterizer& rasterizer,AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile,std::byte* memory,std::byte* backing,u64 base) {
    constexpr u64 Bytes=65536,ImageOffset=4096,OutputOffset=57344,OutputBytes=4096,ImageBytes=36864;
    constexpr u32 Levels=5,Layers=2,Subresources=Levels*Layers,Chunk=1024,CopyOffset=2048;
    constexpr u32 Endpoints=0x07e0f800,Green=0x55555555,BufferValue=0x13579bdf,Poison=0xaaaaaaaa;
    struct alignas(256) Code {std::array<u32,128> words{};};
    std::array<Code,Subresources> seed{},before{},covered{},after{};
    std::array<Code,Layers> complement{};
    Code output_init{},warm{},writer{},gap_writer{},raw{},formatted{};
    GuestShaderFrontend frontend(profile);
    struct Drain {GuestRasterizer& r;bool armed=true;~Drain(){if(armed)try{r.Finish();}catch(...){}}} drain{rasterizer};
    const auto initialize=[](Code& code,const std::vector<u32>& words,u64 hash){
        constexpr u32 Metadata=96;
        if(words.size()>Metadata||Metadata*4+sizeof(AmdGpu::BinaryInfo)>sizeof(code.words))
            throw std::runtime_error("Partial BC GCN metadata overlap");
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
    const auto verify=[&](const auto& stage,std::initializer_list<BufferExpectation> buffers,
                          const AmdGpu::Image* image=nullptr,bool written=false,u32 mip=0){
        if(stage.spirv.empty()||stage.info.translation_failed||stage.info.uses_dma||stage.info.images.size()!=(image?1u:0u))
            throw std::runtime_error("Partial BC GCN changed image/DMA operands");
        auto next=buffers.begin();for(const auto& resource:stage.info.buffers)if(!resource.IsSpecial()){
            if(next==buffers.end())throw std::runtime_error("Partial BC gained guest buffer");const auto sharp=resource.GetSharp(stage.info);
            if(sharp.base_address!=next->sharp.base_address||sharp.GetSize()!=next->sharp.GetSize()||
               resource.is_written!=next->written||resource.is_formatted!=next->formatted)
                throw std::runtime_error("Partial BC changed original bounds/access/order");++next;}
        if(next!=buffers.end())throw std::runtime_error("Partial BC removed guest buffer");
        if(image){const auto& resource=stage.info.images.front();const auto sharp=resource.GetSharp(stage.info);
            if(resource.is_written!=written||!resource.is_array||resource.native_raw32||std::memcmp(&sharp,image,sizeof(sharp)))
                throw std::runtime_error("Partial BC changed array image sharp/access");
            if(written&&!profile.supports_image_load_store_lod&&
               (resource.mip_fallback_mode!=Shader::MipStorageFallbackMode::ConstantIndex||resource.constant_mip_index!=mip))
                throw std::runtime_error("Partial BC lost explicit image mip");}
    };
    for(const bool gpu_seed:{false,true}){
        const auto hash=[&](u32 phase,u32 index=0){return 0x5042434558000000ULL+u64(gpu_seed)*4096+u64(phase)*256+index;};
        rasterizer.ReadMemory(base,Bytes);rasterizer.InvalidateMemory(base,Bytes);std::memset(memory,gpu_seed?0xb6:0xa5,Bytes);
        AmdGpu::Image image{};image.base_address=(base+ImageOffset)>>8;image.type=u64(AmdGpu::ImageType::Color2DArray);
        image.data_format=11;image.num_format=4;image.width=image.height=23;image.pitch=63;image.last_level=Levels-1;
        image.depth=image.last_array=Layers-1;image.tiling_index=5;image.dst_sel_x=4;image.dst_sel_y=5;image.dst_sel_z=6;image.dst_sel_w=7;
        auto bc=image;bc.width=bc.height=95;bc.pitch=255;bc.data_format=35;bc.num_format=0;
        auto desc=DescribeGuestTexture(image);desc.writable=true;const auto layout=LayoutGuestTexture(desc);
        const auto target_desc=DescribeGuestTexture(bc);const auto target_layout=LayoutGuestTexture(target_desc);
        if(base%4096||layout.guest_bytes!=ImageBytes||target_layout.guest_bytes!=ImageBytes||target_desc.writable||
           target_layout.expanded_bytes_per_pixel||target_layout.block_extent!=4||target_layout.format!=DXGI_FORMAT_BC1_UNORM||
           target_layout.logical_bytes!=layout.logical_bytes+Layers*3*8||ImageOffset+ImageBytes>OutputOffset)
            throw std::runtime_error("Partial BC needs page-aligned identical physical prefix with three extra blocks per layer");
        for(u32 mip=0;mip<Levels;++mip){const auto& a=layout.mips.at(mip);const auto& b=target_layout.mips.at(mip);
            if(a.pitch!=b.pitch||a.height!=b.height||a.offset!=b.offset||a.slice_bytes!=b.slice_bytes||a.macro!=b.macro||a.thickness!=b.thickness)
                throw std::runtime_error("Partial BC physical mip mapping changed");}
        GuestTextureCache::Entry shape;shape.desc=target_desc;shape.layout=target_layout;const auto owner=DescribeGuestImageOwnership(shape);
        if(!owner||owner->desc!=target_desc||owner->layout.guest_bytes!=ImageBytes||IsIndependentGuestBlockStorage(target_desc,target_layout)||
           !IsNativeReadonlyGuestBlockStorage(target_desc,target_layout)||shape.Srv().Format!=DXGI_FORMAT_BC1_UNORM)
            throw std::runtime_error("Partial BC must retain actual readonly BC rather than a false complete UINT owner");
        // Initialize every target block, including the three newly visible mip4
        // blocks, independently of the source's narrower logical coverage.
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<Levels;++mip)
        for(u32 y=0;y<(std::max(96u>>mip,1u)+3)/4;++y)for(u32 x=0;x<(std::max(96u>>mip,1u)+3)/4;++x){
            const std::array<u32,2> block{Endpoints,0};std::memcpy(memory+ImageOffset+GuestPixelOffset(target_desc,target_layout,mip,layer,x,y),block.data(),8);}
        std::vector<std::byte> expected(backing,backing+Bytes);
        const auto expect=[&](u64 offset,u32 value){std::memcpy(expected.data()+offset,&value,4);};
        auto logical=std::vector<std::byte>(expected.begin()+ImageOffset,expected.begin()+ImageOffset+ImageBytes);
        const auto old_block=[](u32 index){return std::array<u32,2>{Endpoints|u32(index+1),Green};};
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<Levels;++mip){const auto value=old_block(layer*Levels+mip);
            std::memcpy(logical.data()+GuestPixelOffset(desc,layout,mip,layer,0,0),value.data(),8);}
        AmdGpu::Buffer target{};target.base_address=base+ImageOffset;target.stride=4;target.num_records=ImageBytes/4;target.data_format=4;target.num_format=4;
        target.dst_sel_x=4;target.dst_sel_y=5;target.dst_sel_z=6;target.dst_sel_w=7;
        auto output=target;output.base_address=base+OutputOffset;output.num_records=OutputBytes/4;
        auto full=target;full.base_address=base;full.num_records=Bytes/4;
        // Establish every GPU output byte through an actual guest writer, so
        // untouched output bytes do not rely on an inherited arena snapshot.
        initialize(output_init,{0x7e0802ff,0x4c4c4c4c,0xe0702000,0x08000400,0xbf810000},hash(11));
        for(u32 offset=0;offset<OutputBytes;offset+=Chunk){auto& p=bind(output_init,9,256);
            std::memcpy(p.user_data.data(),&output,sizeof(output));p.user_data[8]=offset;
            verify(frontend.BuildCompute(gpu),{{output,true,false}});rasterizer.DispatchDirect();}
        std::memset(expected.data()+OutputOffset,0x4c,OutputBytes);
        initialize(warm,{0xe0300000,0x80000400,0xbf8c0000,0xe070019c,0x80010400,0xbf810000},hash(1));
        auto& wp=bind(warm,8);std::memcpy(wp.user_data.data(),&full,sizeof(full));std::memcpy(wp.user_data.data()+4,&output,sizeof(output));
        verify(frontend.BuildCompute(gpu),{{full,false,false},{output,true,false}});rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.Finish();u32 initial{};std::memcpy(&initial,expected.data(),4);expect(OutputOffset+412,initial);
        if(std::memcmp(backing,expected.data(),Bytes))throw std::runtime_error("Partial BC warm backing changed");
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<Levels;++mip){const u32 index=layer*Levels+mip;std::vector<u32> words;
            if(!index){move(words,1,BufferValue);words.insert(words.end(),{0xe0700000,0x80000100});}
            coords(words,2,0,0,layer,mip);const auto value=old_block(index);move(words,8,value[0]);move(words,9,value[1]);
            words.insert(words.end(),{0xf0244300,0x00010802,0xbf810000});initialize(seed[index],words,hash(2,index));auto& p=bind(seed[index],12);
            std::memcpy(p.user_data.data(),&target,sizeof(target));std::memcpy(p.user_data.data()+4,&image,sizeof(image));
            if(!index)verify(frontend.BuildCompute(gpu),{{target,true,false}},&image,true,mip);else verify(frontend.BuildCompute(gpu),{},&image,true,mip);
            rasterizer.DispatchDirect();}
        for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<Levels;++mip){const u32 index=layer*Levels+mip;std::vector<u32> words;
            coords(words,0,0,0,layer,mip);words.insert(words.end(),{0xf0044300,0x00000400,0xbf8c0000,
                0xe0700000u+index*16,0x80020400,0xe0700004u+index*16,0x80020500,0xbf810000});
            initialize(before[index],words,hash(3,index));auto& p=bind(before[index],12);std::memcpy(p.user_data.data(),&image,sizeof(image));
            std::memcpy(p.user_data.data()+8,&output,sizeof(output));verify(frontend.BuildCompute(gpu),{{output,true,false}},&image,false,mip);rasterizer.DispatchDirect();
            const auto value=old_block(index);expect(OutputOffset+index*16,value[0]);expect(OutputOffset+index*16+4,value[1]);}
        rasterizer.ReadMemory(output.base_address,OutputBytes);rasterizer.ReadMemory(target.base_address,ImageBytes);rasterizer.Finish();expect(ImageOffset,BufferValue);
        if(rasterizer.IsGpuBufferModified(target.base_address,ImageBytes)||std::memcmp(backing,expected.data(),Bytes))
            throw std::runtime_error("Partial BC old I and clean B/P precondition failed");
        const u64 covered_offset=4,gap_offset=GuestPixelOffset(target_desc,target_layout,4,1,1,0)+4;
        if(gap_offset<=4096||gap_offset+4>ImageBytes||GuestPixelOffset(desc,layout,4,1,0,0)+8>gap_offset)
            throw std::runtime_error("Partial BC uncovered mip4 block address changed");
        const auto write=[&](Code& code,u64 offset,u32 value,u32 phase){auto sharp=target;sharp.base_address+=offset;sharp.num_records=1;
            initialize(code,{0x7e0202ff,value,0xe0700000,0x80000100,0xbf810000},hash(phase));auto& p=bind(code,4);
            std::memcpy(p.user_data.data(),&sharp,sizeof(sharp));verify(frontend.BuildCompute(gpu),{{sharp,true,false}});rasterizer.DispatchDirect();};
        write(writer,covered_offset,Poison,4);write(gap_writer,gap_offset,Green,5);
        if(!gpu_seed){rasterizer.ReadMemory(target.base_address,ImageBytes);rasterizer.Finish();
            expect(ImageOffset+covered_offset,Poison);expect(ImageOffset+gap_offset,Green);}
        // In both branches the uncovered block is green: true gets GPU-only B,
        // false gets previously reconciled P. Only covered I is overlaid later.
        std::memcpy(logical.data()+gap_offset,&Green,4);
        const auto physical_before=expected;
        initialize(raw,{0xe0300000,0x80000400,0xe0300004,0x80000500,0xe0300000,0x08000600,0xbf8c0000,
            0xe0700190,0x80010400,0xe0700194,0x80010500,0xe0700198,0x80010600,0xbf810000},hash(6));
        auto& rp=bind(raw,9);std::memcpy(rp.user_data.data(),&target,sizeof(target));std::memcpy(rp.user_data.data()+4,&output,sizeof(output));rp.user_data[8]=u32(gap_offset);
        verify(frontend.BuildCompute(gpu),{{target,false,false},{output,true,false}});rasterizer.DispatchDirect();
        expect(OutputOffset+400,BufferValue);expect(OutputOffset+404,Poison);expect(OutputOffset+408,Green);
        initialize(formatted,{0xe0002000,0x08000400,0xbf8c0000,0xe0702000,0x80010400,0xbf810000},hash(8));
        auto copied=target;copied.base_address=base+OutputOffset+CopyOffset;copied.num_records=Chunk/4;
        // False branch also proves a resident, readonly-retiled B differs from
        // P while target GPU-page query remains false. Complement itself B==P.
        if(!gpu_seed){auto& p=bind(formatted,9,256);std::memcpy(p.user_data.data(),&target,sizeof(target));
            std::memcpy(p.user_data.data()+4,&copied,sizeof(copied));p.user_data[8]=0;
            verify(frontend.BuildCompute(gpu),{{target,false,true},{copied,true,false}});rasterizer.DispatchDirect();
            std::memcpy(expected.data()+OutputOffset+CopyOffset,logical.data(),Chunk);}
        if(rasterizer.IsGpuBufferModified(target.base_address,ImageBytes)!=gpu_seed||std::memcmp(backing,physical_before.data(),Bytes))
            throw std::runtime_error("Partial BC target GPU-page query branch or physical checkpoint changed");
        const auto read_gpu=[&]{auto snapshot=rasterizer.CapturePreparedShaderBuffer(output.base_address,OutputBytes);
            ComPtr<ID3D12Device> device;Check(snapshot.handle->GetDevice(IID_PPV_ARGS(&device)));
            auto readback=Buffer(device.Get(),OutputBytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
            rasterizer.RetainUntilFinish([snapshot,readback](u64){(void)snapshot;(void)readback;});auto* list=rasterizer.RecordingList();
            snapshot.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(readback.handle.Get(),0,snapshot.handle.Get(),0,OutputBytes);
            rasterizer.Finish();rasterizer.ValidateDebugMessages();void* mapped{};D3D12_RANGE range{0,SIZE_T(OutputBytes)};
            Check(readback.handle->Map(0,&range,&mapped));const bool valid=std::memcmp(mapped,expected.data()+OutputOffset,OutputBytes)==0;
            D3D12_RANGE empty{};readback.handle->Unmap(0,&empty);
            if(!valid||rasterizer.IsGpuBufferModified(target.base_address,ImageBytes)!=gpu_seed||std::memcmp(backing,physical_before.data(),Bytes))
                throw std::runtime_error("Partial BC GPU observations, physical bytes or exact query branch changed");};
        read_gpu(); // Validate raw B and the clean branch's retiled B!=P first.
        const auto sample_covered=[&](auto& programs,u32 output_start,u32 phase){for(u32 layer=0;layer<Layers;++layer)for(u32 mip=0;mip<Levels;++mip){
            const u32 index=layer*Levels+mip;std::vector<u32> words;coords(words,0,0,0,layer,mip);words.insert(words.end(),{0xf0044f00,0x00000400,0xbf8c0000});
            for(u32 component=0;component<4;++component){words.insert(words.end(),{0xe0700000u+output_start+index*16+component*4,
                0x80020000u|((4+component)<<8)});expect(OutputOffset+output_start+index*16+component*4,std::bit_cast<u32>(component==1||component==3?1.f:0.f));}
            words.push_back(0xbf810000);initialize(programs[index],words,hash(phase,index));auto& p=bind(programs[index],12);
            std::memcpy(p.user_data.data(),&bc,sizeof(bc));std::memcpy(p.user_data.data()+8,&output,sizeof(output));
            verify(frontend.BuildCompute(gpu),{{output,true,false}},&bc,false,mip);rasterizer.DispatchDirect();}};
        sample_covered(covered,512,7);
        for(u32 layer=0;layer<Layers;++layer){std::vector<u32> words;for(u32 index=0;index<3;++index){
            const u32 x=index==1?0:1,y=index==0?0:1;coords(words,0,x*4,y*4,layer,4);
            words.insert(words.end(),{0xf0044f00,0x00000400,0xbf8c0000});const bool green=layer==1&&index==0;
            for(u32 component=0;component<4;++component){words.insert(words.end(),{0xe0700500u+(layer*3+index)*16+component*4,
                0x80020000u|((4+component)<<8)});const bool one=component==3||(green?component==1:component==0);
                expect(OutputOffset+1280+(layer*3+index)*16+component*4,std::bit_cast<u32>(one?1.f:0.f));}}
            words.push_back(0xbf810000);initialize(complement[layer],words,hash(10,layer));auto& p=bind(complement[layer],12);
            std::memcpy(p.user_data.data(),&bc,sizeof(bc));std::memcpy(p.user_data.data()+8,&output,sizeof(output));
            verify(frontend.BuildCompute(gpu),{{output,true,false}},&bc,false,4);rasterizer.DispatchDirect();}
        read_gpu();
        for(u32 offset=0;offset<ImageBytes;offset+=Chunk){auto& p=bind(formatted,9,256);std::memcpy(p.user_data.data(),&target,sizeof(target));
            std::memcpy(p.user_data.data()+4,&copied,sizeof(copied));p.user_data[8]=offset;
            verify(frontend.BuildCompute(gpu),{{target,false,true},{copied,true,false}});rasterizer.DispatchDirect();
            std::memcpy(expected.data()+OutputOffset+CopyOffset,logical.data()+offset,Chunk);read_gpu();}
        sample_covered(after,1024,9);read_gpu();
        if(gpu_seed){std::memcpy(expected.data()+ImageOffset+covered_offset,logical.data()+covered_offset,4);
            std::memcpy(expected.data()+ImageOffset+gap_offset,logical.data()+gap_offset,4);}
        rasterizer.ReadMemory(base,Bytes);rasterizer.Finish();rasterizer.ValidateDebugMessages();
        if(std::memcmp(backing,expected.data(),Bytes))throw std::runtime_error("Partial BC full compressed/padding/physical dirty-union oracle mismatch");
        std::printf("PASS: real GCN partial BC1 mip4 complement from%s; full compressed bytes/padding and64KiB physical oracle\n",gpu_seed?"GPU B":"physical P");
    }
    rasterizer.Finish();drain.armed=false;
}
} // namespace NativeDX12
