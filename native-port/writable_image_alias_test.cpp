// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/guest_compute.h"
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "ordered_stage_bindings_fixture.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/backend/spirv/emit_spirv_instructions.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/specialization.h"
#include "shader_recompiler/ir/basic_block.h"
#include "shader_recompiler/ir/passes/resource_pass.h"
#include "shader_recompiler/ir/passes/ir_passes.h"
#include "common/logging/log.h"
#include <dxgi1_6.h>
#include <d3d12sdklayers.h>
#include <fstream>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
namespace Common::Log { std::array<Level,size_t(Class::Count)> g_class_levels{}; }
namespace Shader::Gcn {
bool ParseFetchShader(const Shader::Info&,FetchShaderData&){throw std::logic_error("Unexpected fetch shader");}
}
static std::array<u32,4> ClassifyPhysicalAliases(Shader::Info& info,std::array<u32,4>& write_swizzle,
    std::array<float,4>& read_constants,std::array<float,4>& write_constants,bool prefix_mips=false){
    using namespace Shader;
    Common::ObjectPool<IR::Inst> pool{256};IR::Block block(pool);
    const auto add=[&](IR::Opcode op,std::initializer_list<IR::Value> args){block.AppendNewInst(op,args);return IR::Value{&block.back()};};
    const IR::Value zero{u32(0)};
    const u32 read_lod=u32(info.images[1].GetSharp(info).base_level-info.images[0].GetSharp(info).base_level);
    const auto lod=prefix_mips?add(IR::Opcode::IAdd32,{zero,zero}):IR::Value{read_lod};
    const auto coords=add(IR::Opcode::CompositeConstructU32x4,{zero,zero,zero,lod});
    const auto color=add(IR::Opcode::CompositeConstructF32x4,{IR::Value{0.f},IR::Value{0.f},IR::Value{0.f},IR::Value{0.f}});
    const auto read=add(IR::Opcode::ImageRead,{zero,coords,{}, {}});
    const auto consumer=add(IR::Opcode::CompositeExtractF32x4,{read,zero});
    const auto write=add(IR::Opcode::ImageWrite,{zero,coords,{}, {},color});
    IR::TextureInstInfo flags{};flags.is_array.Assign(true);flags.has_lod.Assign(prefix_mips);write.Inst()->SetFlags(flags);
    // Full-array probes use their real slice coordinate. Only a single-slice
    // T# deliberately exercises non-array lowering and its native zero slice.
    flags.is_array.Assign(info.images[0].GetSharp(info).NumViewLayers(true)>1);
    flags.has_lod.Assign(prefix_mips||read_lod!=0);read.Inst()->SetFlags(flags);
    Optimization::ResourceDiscoveryList resources;
    for(u32 i=0;i<2;++i){Optimization::ResourceDiscovery usage{};usage.user=i?write.Inst():read.Inst();usage.sharps[0].num_dwords=8;
        for(u32 d=0;d<8;++d)usage.sharps[0].dwords[d]=add(IR::Opcode::GetUserData,{IR::Value{static_cast<IR::ScalarReg>(i*8+d)}});
        resources.push_back(usage);
    }
    info.images.clear();Profile profile{};profile.native_coherent_images=true;
    Optimization::ResourcePatchingPass(info,resources,profile);
    if(info.images.size()!=2||!info.images[0].is_written||!info.images[1].is_written)
        throw std::runtime_error("Physical image alias was not classified as common UAV storage");
    const auto* shuffle=consumer.Inst()->Arg(0).Inst();
    if(shuffle->GetOpcode()!=IR::Opcode::CompositeShuffleF32x4)
        throw std::runtime_error("Storage read swizzle was not lowered by the resource pass");
    const auto* inverse=write.Inst()->Arg(4).Inst();
    if(inverse->GetOpcode()!=IR::Opcode::CompositeShuffleF32x4)
        throw std::runtime_error("Storage write swizzle was not lowered by the resource pass");
    write_swizzle={inverse->Arg(2).U32(),inverse->Arg(3).U32(),inverse->Arg(4).U32(),inverse->Arg(5).U32()};
    for(u32 i=0;i<4;++i){read_constants[i]=shuffle->Arg(0).Inst()->Arg(i).F32();write_constants[i]=inverse->Arg(0).Inst()->Arg(i).F32();}
    return {shuffle->Arg(2).U32(),shuffle->Arg(3).U32(),shuffle->Arg(4).U32(),shuffle->Arg(5).U32()};
}
static void CheckStorageIdentitySpecialization(const Shader::Info& info){
    Shader::Profile profile{};profile.native_coherent_images=true;
    Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;
    const Shader::Backend::Bindings bindings{};
    const Shader::StageSpecialization baseline(info,runtime,profile,bindings);
    if(baseline.native_image_aliases.size()!=2||baseline.native_image_aliases[1]!=0)
        throw std::runtime_error("T# control bits split the shader's physical alias group");
    auto plain=info;const auto original=info.images[1].GetSharp(info);
    std::memcpy(plain.flattened_ud_buf.data(),&original,sizeof(original));
    const Shader::StageSpecialization plain_key(plain,runtime,profile,bindings);
    if(!(baseline==plain_key))
        throw std::runtime_error("Non-storage T# fields caused an unnecessary shader variant");
    // Each changed storage field must still break alias classification. These
    // are metadata-only controls; the full dispatch below uses valid geometry.
    const std::array<void(*)(AmdGpu::Image&),10> different{
        +[](AmdGpu::Image& image){image.base_address^=1;},
        +[](AmdGpu::Image& image){image.width+=1;},
        +[](AmdGpu::Image& image){image.height+=1;},
        +[](AmdGpu::Image& image){image.depth+=1;},
        +[](AmdGpu::Image& image){image.pitch+=1;},
        +[](AmdGpu::Image& image){image.tiling_index^=1;},
        +[](AmdGpu::Image& image){image.pow2pad^=1;},
        +[](AmdGpu::Image& image){image.alt_tile_mode^=1;},
        +[](AmdGpu::Image& image){image.type=u64(AmdGpu::ImageType::Color3D);},
        +[](AmdGpu::Image& image){image.data_format=11;}
    };
    for(const auto change:different){
        auto broken=info;auto image=original;change(image);
        std::memcpy(broken.flattened_ud_buf.data()+8,&image,sizeof(image));
        const Shader::StageSpecialization separate(broken,runtime,profile,bindings);
        if(Shader::NativeImageStorageAliases(info.images[0].GetSharp(info),image)||
           separate.native_image_aliases.size()!=2||separate.native_image_aliases[1]!=1||
           baseline==separate)
            throw std::runtime_error("A physical storage change reused a shared image shader variant");
    }
    auto prefix=info;auto extended=original;extended.last_level+=1;
    std::memcpy(prefix.flattened_ud_buf.data()+8,&extended,sizeof(extended));
    const Shader::StageSpecialization prefix_key(prefix,runtime,profile,bindings);
    if(!Shader::NativeImageStorageAliases(info.images[0].GetSharp(info),extended)||
       prefix_key.native_image_aliases[1]!=0||baseline==prefix_key)
        throw std::runtime_error("Mip-prefix aliases lost their original view-count specialization");
    auto msaa=original;msaa.type=u64(AmdGpu::ImageType::Color2DMsaaArray);
    auto other_samples=msaa;other_samples.last_level+=1;
    if(Shader::NativeImageStorageAliases(msaa,other_samples))
        throw std::runtime_error("Sample-count changes were confused with mip prefixes");
}
static std::vector<u32> EmitAliasShader(Shader::Info& info,bool typed,bool mixed,bool dynamic,bool nonuniform,bool classified,std::array<u32,4> read_swizzle,std::array<u32,4> write_swizzle,std::array<float,4> read_constants,std::array<float,4> write_constants){
    Shader::Profile profile{};profile.supported_spirv=0x00010500;profile.native_coherent_images=true;
    Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;
    if(dynamic)info.loads.Set(Shader::IR::Attribute::WorkgroupId,0);
    if(nonuniform)info.loads.Set(Shader::IR::Attribute::LocalInvocationId,0);
    Shader::Backend::Bindings binding{};
    if(dynamic){
        auto single=info;single.images[0].mip_fallback_mode=Shader::MipStorageFallbackMode::None;
        const Shader::StageSpecialization before(single,runtime,profile,binding);
        AmdGpu::Image sharp;memcpy(&sharp,single.flattened_ud_buf.data(),sizeof(sharp));
        sharp.last_level=2;memcpy(single.flattened_ud_buf.data(),&sharp,sizeof(sharp));
        const Shader::StageSpecialization after(single,runtime,profile,binding);
        if(before==after)throw std::runtime_error("Storage mip count did not invalidate shader specialization");
        auto aliases=info;
        const Shader::StageSpecialization aliased(aliases,runtime,profile,binding);
        AmdGpu::Image target;memcpy(&target,aliases.flattened_ud_buf.data()+8,sizeof(target));
        target.base_address+=256;memcpy(aliases.flattened_ud_buf.data()+8,&target,sizeof(target));
        const Shader::StageSpecialization disjoint(aliases,runtime,profile,binding);
        if(aliased==disjoint)throw std::runtime_error("Breaking a physical image alias reused a storage shader variant");
        AmdGpu::Image source;memcpy(&source,aliases.flattened_ud_buf.data(),sizeof(source));source.base_address+=256;
        memcpy(aliases.flattened_ud_buf.data(),&source,sizeof(source));
        const Shader::StageSpecialization moved(aliases,runtime,profile,binding);
        if(!(aliased==moved))throw std::runtime_error("Moving an intact alias group unnecessarily changed shader specialization");
        auto signed_view=info;
        const Shader::StageSpecialization unsigned_key(signed_view,runtime,profile,binding);
        for(u32 i=0;i<2;++i){AmdGpu::Image view;memcpy(&view,signed_view.flattened_ud_buf.data()+8*i,sizeof(view));
            view.num_format=5;memcpy(signed_view.flattened_ud_buf.data()+8*i,&view,sizeof(view));}
        const Shader::StageSpecialization signed_key(signed_view,runtime,profile,binding);
        if(unsigned_key==signed_key)throw std::runtime_error("UINT and SINT images reused one shader specialization");
        auto external=info;external.images.resize(1);external.images[0].is_written=false;
        AmdGpu::Image external_sharp=external.images[0].GetSharp(external);
        const Shader::StageSpecialization no_external(external,runtime,profile,binding);
        external.native_pipeline_writes.push_back({external_sharp,true,false});
        const Shader::StageSpecialization matching_external(external,runtime,profile,binding);
        if(no_external==matching_external)throw std::runtime_error("External writer did not invalidate a sampled variant");
        external.native_pipeline_writes[0].sharp.base_address+=256;
        const Shader::StageSpecialization unrelated_external(external,runtime,profile,binding);
        if(!(no_external==unrelated_external))throw std::runtime_error("Unrelated external writer changed specialization");
        external_sharp.base_address+=256;
        memcpy(external.flattened_ud_buf.data(),&external_sharp,sizeof(external_sharp));
        const Shader::StageSpecialization relocated_external(external,runtime,profile,binding);
        if(!(matching_external==relocated_external))throw std::runtime_error("Relocating an external alias changed specialization");
        external.native_pipeline_writes.push_back(external.native_pipeline_writes[0]);
        const Shader::StageSpecialization duplicate_external(external,runtime,profile,binding);
        if(!(matching_external==duplicate_external))throw std::runtime_error("Duplicate external writer changed specialization");
        auto srgb_view=info;
        memcpy(&target,srgb_view.flattened_ud_buf.data()+8,sizeof(target));target.num_format=9;
        memcpy(srgb_view.flattened_ud_buf.data()+8,&target,sizeof(target));
        if(!Shader::NativeImageViewAliases(srgb_view.images[0],srgb_view.images[1],srgb_view))
            throw std::runtime_error("sRGB and linear views did not share physical storage");
    }
    Shader::Backend::SPIRV::EmitContext c(profile,runtime,info,binding);
    c.AddCapability(spv::Capability::StorageImageReadWithoutFormat);
    c.AddCapability(spv::Capability::StorageImageWriteWithoutFormat);
    const auto fn=c.OpFunction(c.void_id,spv::FunctionControlMask::MaskNone,c.TypeFunction(c.void_id));c.AddLabel();
    if(classified){
        using namespace Shader::Backend::SPIRV;
        const u32 read_layer=1-u32(info.images[0].GetSharp(info).base_array);
        if(info.images[0].native_srgb_read){
            const auto sample=[&](){
                const auto decoded=EmitImageRead(c,nullptr,0,c.ConstU32(0U,0U,read_layer),{},{});
                return EmitCompositeShuffleF32x4(c,c.ConstF32(read_constants[0],read_constants[1],read_constants[2],read_constants[3]),decoded,
                    read_swizzle[0],read_swizzle[1],read_swizzle[2],read_swizzle[3]);
            };
            EmitImageWrite(c,nullptr,1,c.ConstU32(0U,0U,1U),{},{},sample());
            c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
            EmitImageWrite(c,nullptr,1,c.ConstU32(1U,0U,1U),{},{},sample());
            c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
            c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
        }
        const auto read_float=[&](){
            const auto value=EmitImageRead(c,nullptr,0,c.ConstU32(0U,0U,read_layer),{},{});
            const auto mapped=EmitCompositeShuffleF32x4(c,c.ConstF32(read_constants[0],read_constants[1],read_constants[2],read_constants[3]),value,
                read_swizzle[0],read_swizzle[1],read_swizzle[2],read_swizzle[3]);
            return c.OpCompositeExtract(c.F32[1],mapped,0);
        };
        const auto read=[&](){return c.OpBitcast(c.U32[1],read_float());};
        const bool swapped_write=write_swizzle[0]==5;
        const auto write=[&](u32 x,Sirit::Id value){
            const auto logical=c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],value,swapped_write?c.ConstU32(x?9U:3U):value,value,value));
            const auto physical=EmitCompositeShuffleF32x4(c,c.ConstF32(write_constants[0],write_constants[1],write_constants[2],write_constants[3]),logical,
                write_swizzle[0],write_swizzle[1],write_swizzle[2],write_swizzle[3]);
            EmitImageWrite(c,nullptr,1,c.ConstU32(x,0U,1U),{},{},physical);
        };
        const bool floating=!c.images[0].is_integer;
        write(0,floating?c.OpBitcast(c.U32[1],c.OpFAdd(c.F32[1],read_float(),c.ConstF32(2.f))):c.OpIAdd(c.U32[1],read(),c.ConstU32(1U)));
        c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
        write(1,floating?c.OpConvertFToU(c.U32[1],c.OpFMul(c.F32[1],read_float(),c.ConstF32(2.f))):c.OpIAdd(c.U32[1],read(),c.ConstU32(100U)));
        const auto read_levels=info.images[0].GetSharp(info).NumLevels();
        const auto write_levels=info.images[1].GetSharp(info).NumLevels();
        if(read_levels!=write_levels){
            // Both resources came from real dynamic-mip resource patching.
            // Their arrays and query results must retain each original range,
            // despite binding one larger native allocation.
            const auto read_dims=EmitImageQueryDimensions(c,nullptr,0,c.u32_zero_value,true);
            const auto write_dims=EmitImageQueryDimensions(c,nullptr,1,c.u32_zero_value,true);
            write(2,c.OpCompositeExtract(c.U32[1],read_dims,3));
            write(3,c.OpCompositeExtract(c.U32[1],write_dims,3));
            const u32 short_handle=read_levels<write_levels?0:1;
            const auto invalid=EmitImageRead(c,nullptr,short_handle,c.ConstU32(0U,0U,0U),c.ConstU32(1U),{});
            write(4,c.OpBitcast(c.U32[1],c.OpCompositeExtract(c.F32[1],invalid,0)));
            const auto poison=c.ConstU32(0xdeadbeefU,0xdeadbeefU,0xdeadbeefU,0xdeadbeefU);
            EmitImageWrite(c,nullptr,short_handle,c.ConstU32(0U,0U,0U),c.ConstU32(1U),{},c.OpBitcast(c.F32[4],poison));
        }
        c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
        c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
    }
    if(nonuniform){
        using namespace Shader::Backend::SPIRV;
        const auto lane=c.OpCompositeExtract(c.U32[1],c.OpLoad(c.U32[3],c.local_invocation_id),0);
        const auto mip=c.OpBitwiseAnd(c.U32[1],lane,c.ConstU32(1U));
        const auto x=c.OpShiftRightLogical(c.U32[1],lane,c.ConstU32(1U));
        const auto coord=[&](u32 y,u32 layer){return c.OpCompositeConstruct(c.U32[3],x,c.ConstU32(y),c.ConstU32(layer));};
        const auto read=EmitImageRead(c,nullptr,0,coord(0,1),mip,{});
        const auto dimensions=EmitImageQueryDimensions(c,nullptr,0,mip,true);
        const auto value=c.OpIAdd(c.U32[1],c.OpCompositeExtract(c.U32[1],c.OpBitcast(c.U32[4],read),0),
            c.OpIMul(c.U32[1],c.OpCompositeExtract(c.U32[1],dimensions,0),c.ConstU32(100U)));
        const auto color=c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],value,value,value,value));
        EmitImageWrite(c,nullptr,0,coord(0,2),mip,{},color);
        c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
        const auto observed=EmitImageRead(c,nullptr,0,coord(0,2),mip,{});
        EmitImageWrite(c,nullptr,0,coord(1,2),mip,{},observed);
        c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
        c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,8U,1U,1U);return c.Assemble();
    }
    if(dynamic){
        using namespace Shader::Backend::SPIRV;
        const auto zero=c.OpCompositeExtract(c.U32[1],c.OpLoad(c.U32[3],c.workgroup_id),0);
        const auto one=c.OpIAdd(c.U32[1],zero,c.ConstU32(1U));
        auto read=EmitImageRead(c,nullptr,0,c.ConstU32(0U,0U,1U),zero,{});
        auto value=c.OpIAdd(c.U32[1],c.OpCompositeExtract(c.U32[1],c.OpBitcast(c.U32[4],read),0),c.ConstU32(100U));
        EmitImageWrite(c,nullptr,0,c.ConstU32(0U,0U,2U),one,{},c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],value,value,value,value)));
        c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
        read=EmitImageRead(c,nullptr,0,c.ConstU32(0U,0U,2U),one,{});
        value=c.OpIAdd(c.U32[1],c.OpCompositeExtract(c.U32[1],c.OpBitcast(c.U32[4],read),0),c.ConstU32(10U));
        EmitImageWrite(c,nullptr,0,c.ConstU32(1U,0U,2U),zero,{},c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],value,value,value,value)));
        c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
        const auto implicit_read=EmitImageRead(c,nullptr,0,c.ConstU32(1U,0U,2U),{},{});
        EmitImageWrite(c,nullptr,0,c.ConstU32(0U,1U,2U),{},{},implicit_read);
        for(u32 test=0;test<4;++test){
            const auto bad=c.ConstU32(test%2==0?2U:UINT32_MAX);
            const auto invalid=test<2?c.OpIAdd(c.U32[1],zero,bad):bad;
            const auto missing=EmitImageRead(c,nullptr,0,c.ConstU32(0U,0U,1U),invalid,{});
            EmitImageWrite(c,nullptr,0,c.ConstU32(1U+test,1U,2U),{},{},missing);
            EmitImageWrite(c,nullptr,0,c.ConstU32(0U,0U,1U),invalid,{},implicit_read);
        }
        const auto dimensions=EmitImageQueryDimensions(c,nullptr,0,one,true);
        auto packed=c.OpCompositeExtract(c.U32[1],dimensions,0);
        for(u32 component=1,scale=10;component<4;++component,scale*=10)
            packed=c.OpIAdd(c.U32[1],packed,c.OpIMul(c.U32[1],c.OpCompositeExtract(c.U32[1],dimensions,component),c.ConstU32(scale)));
        EmitImageWrite(c,nullptr,0,c.ConstU32(2U,0U,2U),zero,{},c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],packed,packed,packed,packed)));
        const auto base_dimensions=EmitImageQueryDimensions(c,nullptr,1,one,true);
        packed=c.OpCompositeExtract(c.U32[1],base_dimensions,0);
        for(u32 component=1,scale=10;component<4;++component,scale*=10)
            packed=c.OpIAdd(c.U32[1],packed,c.OpIMul(c.U32[1],c.OpCompositeExtract(c.U32[1],base_dimensions,component),c.ConstU32(scale)));
        EmitImageWrite(c,nullptr,0,c.ConstU32(3U,0U,2U),zero,{},c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],packed,packed,packed,packed)));
        for(u32 test=0;test<4;++test){
            const auto level=test==0?zero:c.OpIAdd(c.U32[1],zero,c.ConstU32(test==1?32U:test==2?2U:UINT32_MAX));
            const auto dimensions=EmitImageQueryDimensions(c,nullptr,test<2?1:0,level,test>=2);
            auto result=c.OpCompositeExtract(c.U32[1],dimensions,0);
            for(u32 component=1,scale=10;component<4;++component,scale*=10)
                result=c.OpIAdd(c.U32[1],result,c.OpIMul(c.U32[1],c.OpCompositeExtract(c.U32[1],dimensions,component),c.ConstU32(scale)));
            EmitImageWrite(c,nullptr,0,c.ConstU32(4U+test,0U,2U),zero,{},c.OpBitcast(c.F32[4],c.OpCompositeConstruct(c.U32[4],result,result,result,result)));
        }
        c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
        c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
    }
    const auto first=c.OpLoad(c.images[0].image_type,c.images[0].id);
    const auto second=c.OpLoad(c.images[1].image_type,c.images[1].id);
    if(mixed){
        const std::array lod{c.ConstU32(0U)};
        const auto sample=c.OpImageFetch(c.U32[4],first,c.ConstU32(0U,0U,0U),spv::ImageOperandsMask::Lod,lod);
        const auto value=c.OpIAdd(c.U32[1],c.OpCompositeExtract(c.U32[1],sample,0),c.ConstU32(17U));
        c.OpImageWrite(second,c.ConstU32(0U,0U,0U),c.OpCompositeConstruct(c.U32[4],value,value,value,value));
        c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
        c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
    }
    const auto third=c.OpLoad(c.images[2].image_type,c.images[2].id);
    const auto origin=c.ConstU32(0U,0U,0U);
    const auto next=typed?c.ConstU32(0x3fc00000U):c.OpIAdd(c.U32[1],c.OpCompositeExtract(c.U32[1],c.OpImageRead(c.U32[4],first,origin),0),c.ConstU32(1U));
    c.OpImageWrite(first,origin,c.OpCompositeConstruct(c.U32[4],next,next,next,next));
    c.OpMemoryBarrier(c.ConstU32(u32(spv::Scope::Device)),
        c.ConstU32(u32(spv::MemorySemanticsMask::AcquireRelease|spv::MemorySemanticsMask::ImageMemory)));
    if(typed){
        const auto observed=c.OpCompositeExtract(c.F32[1],c.OpImageRead(c.F32[4],second,c.ConstU32(0U,0U,1U)),0);
        const auto added=c.OpFAdd(c.F32[1],observed,c.ConstF32(2.f));
        c.OpImageWrite(second,c.ConstU32(1U,0U,2U),c.OpCompositeConstruct(c.F32[4],added,added,added,added));
        const auto negative=c.OpConvertFToS(c.S32[1],c.OpFMul(c.F32[1],observed,c.ConstF32(-4.f)));
        c.OpImageWrite(third,origin,c.OpCompositeConstruct(c.S32[4],negative,negative,negative,negative));
    }else{
    const auto observed=c.OpCompositeExtract(c.U32[1],c.OpImageRead(c.U32[4],second,c.ConstU32(0U,0U,1U)),0);
    const auto store=[&](Sirit::Id image,Sirit::Id position,u32 delta){
        const auto value=c.OpIAdd(c.U32[1],observed,c.ConstU32(delta));
        c.OpImageWrite(image,position,c.OpCompositeConstruct(c.U32[4],value,value,value,value));};
    store(second,c.ConstU32(1U,0U,2U),10);store(third,origin,100);
    }
    c.OpReturn();c.OpFunctionEnd();c.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",c.interfaces);
    c.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);return c.Assemble();
}
int RunNativePackedImage(const wchar_t*,const wchar_t*,std::wstring_view);
int RunNativePacked10(const wchar_t*,const wchar_t*,std::wstring_view);
int wmain(int argc,wchar_t** argv){try {
    if(argc!=4&&argc!=5)throw std::invalid_argument("Expected fixture, converter and DXC");
    if(argc==5&&std::wstring_view(argv[4]).starts_with(L"packed-"))return RunNativePackedImage(argv[2],argv[3],argv[4]);
    if(argc==5&&(std::wstring_view(argv[4]).starts_with(L"packed10-")||
        std::wstring_view(argv[4]).starts_with(L"raw32-")))return RunNativePacked10(argv[2],argv[3],argv[4]);
    const bool typed=argc==5&&std::wstring_view(argv[4])==L"typed";
    const bool mixed=argc==5&&std::wstring_view(argv[4])==L"mixed";
    const bool nonuniform=argc==5&&std::wstring_view(argv[4])==L"nonuniform";
    const bool dynamic=nonuniform||(argc==5&&std::wstring_view(argv[4])==L"dynamic");
    const bool classified_view=argc==5&&(std::wstring_view(argv[4])==L"classified-view"||std::wstring_view(argv[4])==L"classified-mip");
    const bool classified_mip=argc==5&&std::wstring_view(argv[4])==L"classified-mip";
    const bool classified_float_one=argc==5&&std::wstring_view(argv[4])==L"classified-float-one";
    const bool classified_signed_one=argc==5&&std::wstring_view(argv[4])==L"classified-signed-one";
    const bool classified_typed=classified_float_one||(argc==5&&std::wstring_view(argv[4])==L"classified-typed");
    const bool classified_write_swizzle=argc==5&&std::wstring_view(argv[4])==L"classified-write-swizzle";
    const bool classified_swizzle=classified_write_swizzle||(argc==5&&std::wstring_view(argv[4])==L"classified-swizzle");
    const bool classified_one=classified_float_one||classified_signed_one||(argc==5&&std::wstring_view(argv[4])==L"classified-one");
    const bool classified_srgb_swizzle=argc==5&&std::wstring_view(argv[4])==L"classified-srgb-swizzle";
    const bool classified_srgb=classified_srgb_swizzle||(argc==5&&std::wstring_view(argv[4])==L"classified-srgb");
    const bool classified_identity=argc==5&&std::wstring_view(argv[4])==L"classified-identity";
    const bool prefix_read=argc==5&&std::wstring_view(argv[4])==L"classified-prefix-read";
    const bool prefix_write=argc==5&&std::wstring_view(argv[4])==L"classified-prefix-write";
    const bool classified=prefix_read||prefix_write||classified_identity||classified_srgb||classified_one||classified_swizzle||classified_typed||classified_view||(argc==5&&std::wstring_view(argv[4])==L"classified");
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);const auto bytes=file.tellg();
    if(bytes<=0||bytes%4)throw std::runtime_error("Invalid shader fixture");
    std::vector<u32> spirv(size_t(bytes)/4);file.seekg(0);file.read(reinterpret_cast<char*>(spirv.data()),bytes);
    Shader::Backend::DX12::SpirvConvertOptions options;options.stage=Shader::Backend::DX12::SpirvStage::Compute;
    auto dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],spirv,options);
    if(!dxil.success)throw std::runtime_error(dxil.diagnostics);
    ComPtr<ID3D12Debug> debug;if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))debug->EnableDebugLayer();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));ComPtr<ID3D12Device> device;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        if(factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))))break;
    }
    if(!device)throw std::runtime_error("No hardware device");
    if(argc==5&&std::wstring_view(argv[4])==L"single-interior-reads"){
        Shader::Info empty;empty.hw_stage=Shader::HwStage::Compute;empty.sw_stage=Shader::SwStage::Compute;
        Shader::Profile profile{};profile.supported_spirv=0x00010500;
        Shader::RuntimeInfo runtime{};runtime.sw_stage=Shader::SwStage::Compute;Shader::Backend::Bindings bindings{};
        Shader::Backend::SPIRV::EmitContext context(profile,runtime,empty,bindings);
        const auto fn=context.OpFunction(context.void_id,spv::FunctionControlMask::MaskNone,context.TypeFunction(context.void_id));
        context.AddLabel();context.OpReturn();context.OpFunctionEnd();
        context.AddEntryPoint(spv::ExecutionModel::GLCompute,fn,"main",context.interfaces);
        context.AddExecutionMode(fn,spv::ExecutionMode::LocalSize,1U,1U,1U);
        auto converted=Shader::Backend::DX12::ConvertSpirv(argv[2],context.Assemble(),options);
        if(!converted.success)throw std::runtime_error(converted.diagnostics);
        ShaderPipeline pipeline(device.Get(),argv[3],converted.bytecode);
        return RunOrderedInteriorFormattedReads(device.Get(),&pipeline);
    }
    if(argc==5&&std::wstring_view(argv[4])==L"ordered-interior-reads")return RunOrderedInteriorFormattedReads(device.Get());
    if(argc==5&&std::wstring_view(argv[4])==L"ordered-bc-word"){
        RunOrderedBcWordStages(device.Get());
        RunOrderedBcWordStages(device.Get(),true);
        puts("PASS: ordered BC sampled I and later UINT writable B retain exact guest storage");
        return 0;
    }
    if(argc==5&&std::wstring_view(argv[4])==L"ordered-stages")return RunOrderedStageBindingsFixture(device.Get());
    struct Allocation{void* p;~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);}} owner{
        VirtualAlloc(reinterpret_cast<void*>(0x1200000000ULL),65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)};
    if(!owner.p)throw std::runtime_error("Cannot allocate guest texture");
    const auto address=reinterpret_cast<VAddr>(owner.p);
    AmdGpu::Image sharp{};sharp.base_address=address>>8;sharp.width=sharp.height=sharp.pitch=7;
    sharp.depth=2;sharp.type=u64(AmdGpu::ImageType::Color2DArray);sharp.last_level=1;
    sharp.data_format=4;sharp.num_format=4;sharp.tiling_index=13;
    if(classified_swizzle)sharp.data_format=11;
    if(classified_srgb)sharp.data_format=10;
    sharp.dst_sel_x=4;sharp.dst_sel_y=5;sharp.dst_sel_z=6;sharp.dst_sel_w=7;
    sharp.last_array=2;
    const auto desc=DescribeGuestTexture(sharp);const auto layout=LayoutGuestTexture(desc);
    auto memory=std::span(static_cast<std::byte*>(owner.p),size_t(layout.guest_bytes));
    std::fill(memory.begin(),memory.end(),std::byte{0xee});
    const u32 initial=classified_srgb?0x800a4080U:classified_typed?0x3fc00000U:40;memcpy(memory.data()+GuestPixelOffset(desc,layout,classified_mip?1:0,1,0,0),&initial,4);
    if(classified_swizzle){const u32 green=7;memcpy(memory.data()+GuestPixelOffset(desc,layout,0,1,0,0)+4,&green,4);}
    std::vector<std::byte> expected(memory.begin(),memory.end());
    Rasterizer rasterizer(device.Get());rasterizer.MapMemory(address,65536);
    GuestTextureCache textures(device.Get());
    Shader::Info info;info.hw_stage=Shader::HwStage::Compute;info.sw_stage=Shader::SwStage::Compute;info.flattened_ud_buf.resize(24);
    for(u32 i=0;i<(dynamic||mixed||classified?2u:3u);++i){auto view=sharp;
        if(typed&&i)view.num_format=i==1?7:5;
        if(i==0&&!dynamic&&!classified)view.base_array=view.last_array=1;
        if(classified_view&&i==0)view.base_array=view.last_array=1;
        if(classified_mip&&i==1)view.base_level=1;
        if(classified_typed&&i==0)view.num_format=7;
        if((prefix_read&&i==0)||(prefix_write&&i==1))view.last_level=0;
        if(classified_one&&i==0)view.dst_sel_x=1;
        if(classified_signed_one)view.num_format=5;
        if(classified_srgb)view.num_format=i==0?9:0;
        if(classified_srgb_swizzle&&i==0){view.dst_sel_x=7;view.dst_sel_w=4;}
        if(classified_swizzle&&i==(classified_write_swizzle?1u:0u)){view.dst_sel_x=5;view.dst_sel_y=4;}
        if(classified_identity&&i==0){
            // Pinned ImageInfo ignores cache/perf/counter and unused T# bits.
            // They must not leave a same-storage read as an overlapping SRV.
            view.mtype_l2=3;view.mtype=2;view.mtype2=1;view.atc=1;
            view.perf_modulation=7;view.interlaced=1;
            view.min_lod_warn=4095;view.counter_bank_id=255;view.lod_hw_cnt_en=1;
            view.compression_en=1;view.alpha_is_on_msb=1;view.color_transform=1;
            std::array<u32,8> words{};std::memcpy(words.data(),&view,sizeof(view));
            words[7]|=0x80000000U;std::memcpy(&view,words.data(),sizeof(view));
        }
        if(i==2){view.base_level=1;view.base_array=view.last_array=2;}
        memcpy(info.flattened_ud_buf.data()+i*8,&view,sizeof(view));
        Shader::ImageResource image{};image.is_written=!mixed||i!=0;image.is_array=true;
        if(dynamic&&i==0)image.mip_fallback_mode=Shader::MipStorageFallbackMode::DynamicIndex;
        image.sharp_fetch.summary=Shader::SharpFetch<AmdGpu::Image>::Summary::SingleLoad;
        image.sharp_fetch.offsets[0]=i*8;info.images.push_back(image);
    }
    std::array<u32,4> read_swizzle{4,5,6,7};
    std::array<u32,4> write_swizzle{4,5,6,7};
    std::array<float,4> read_constants{0.f,1.f,0.f,0.f},write_constants=read_constants;
    if(classified)read_swizzle=ClassifyPhysicalAliases(info,write_swizzle,read_constants,write_constants,prefix_read||prefix_write);
    if(classified_identity)CheckStorageIdentitySpecialization(info);
    if(classified_srgb&&(!info.images[0].native_srgb_read||info.images[1].native_srgb_read))
        throw std::runtime_error("sRGB decode was not restricted to the promoted read view");
    if(argc==5){dxil=Shader::Backend::DX12::ConvertSpirv(argv[2],EmitAliasShader(info,typed,mixed,dynamic,nonuniform,classified,read_swizzle,write_swizzle,read_constants,write_constants),options);
        if(!dxil.success)throw std::runtime_error(dxil.diagnostics);}
    ShaderPipeline pipeline(device.Get(),argv[3],dxil.bytecode);
    if(classified){
        const u32 first=classified_srgb_swizzle?0x37010d80U:classified_srgb?0x80010d37U:classified_float_one?0x40400000U:classified_one?2U:classified_write_swizzle?3U:classified_swizzle?8U:classified_typed?0x40600000U:41,second=classified_srgb_swizzle?0x37000137U:classified_srgb?0x8000010aU:classified_float_one?2U:classified_one?101U:classified_write_swizzle?9U:classified_swizzle?108U:classified_typed?7U:141;
        memcpy(expected.data()+GuestPixelOffset(desc,layout,classified_mip?1:0,1,0,0),&first,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,classified_mip?1:0,1,1,0),&second,4);
        if(prefix_read||prefix_write){
            const u32 read_levels=prefix_read?1:2,write_levels=prefix_write?1:2,zero=0;
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,1,2,0),&read_levels,4);
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,1,3,0),&write_levels,4);
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,1,4,0),&zero,4);
        }
        if(classified_swizzle){
            const u32 first_green=classified_write_swizzle?41U:first,second_green=classified_write_swizzle?103U:second;
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,1,0,0)+4,&first_green,4);
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,1,1,0)+4,&second_green,4);
        }
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        if(memcmp(memory.data(),expected.data(),expected.size())||textures.Entries().size()!=1)
            throw std::runtime_error("Classified physical image aliases lost intra-dispatch writes");
    }else if(nonuniform){
        for(u32 lane=0;lane<8;++lane){
            const u32 mip=lane%2,x=lane/2,input=40+lane,output=input+(mip?4:8)*100;
            const auto source=GuestPixelOffset(desc,layout,mip,1,x,0);
            memcpy(memory.data()+source,&input,4);memcpy(expected.data()+source,&input,4);
            for(u32 y=0;y<2;++y)memcpy(expected.data()+GuestPixelOffset(desc,layout,mip,2,x,y),&output,4);
        }
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        if(memcmp(memory.data(),expected.data(),expected.size())||textures.Entries().size()!=1)
            throw std::runtime_error("Divergent lanes lost independent mip loads, queries or stores");
    }else if(dynamic){
        const u32 upper=140,lower=150;
        const u32 missing=0;
        for(u32 x=1;x<=4;++x)memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,x,1),&missing,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,0,1),&lower,4);
        const u32 dimensions=2344; // mip 1: 4 x 4, three array layers, two mip levels.
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,2,0),&dimensions,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,3,0),&dimensions,4);
        const u32 base_dimensions=388,invalid_dimensions=0;
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,4,0),&base_dimensions,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,5,0),&invalid_dimensions,4);
        const u32 invalid_array_dimensions=2000;
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,6,0),&invalid_array_dimensions,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,7,0),&invalid_array_dimensions,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,1,2,0,0),&upper,4);
        memcpy(expected.data()+GuestPixelOffset(desc,layout,0,2,1,0),&lower,4);
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        if(memcmp(memory.data(),expected.data(),expected.size())||textures.Entries().size()!=1)
            throw std::runtime_error("Storage image mip reads/writes lost descriptor selection or coherence");
    }else if(mixed){
        for(u32 scenario=0;scenario<3;++scenario){
            auto source=sharp,target=sharp;
            source.base_level=scenario==0?1:0;source.base_array=source.last_array=1;
            target.base_level=0;target.base_array=target.last_array=scenario==0?1:2;
            if(scenario==2){
                auto invalid=target;invalid.base_array=invalid.last_array=1;
                memcpy(info.flattened_ud_buf.data(),&source,sizeof(source));
                memcpy(info.flattened_ud_buf.data()+8,&invalid,sizeof(invalid));
                bool rejected=false;try{ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);}
                catch(const std::runtime_error& e){rejected=std::string_view(e.what()).find("subresource")!=std::string_view::npos;}
                if(!rejected)throw std::runtime_error("Overlapping SRV/UAV subresources were not rejected");
            }
            memcpy(info.flattened_ud_buf.data(),&source,sizeof(source));
            memcpy(info.flattened_ud_buf.data()+8,&target,sizeof(target));
            u32 value;memcpy(&value,expected.data()+GuestPixelOffset(desc,layout,u32(source.base_level),1,0,0),4);value+=17;
            ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
            memcpy(expected.data()+GuestPixelOffset(desc,layout,0,u32(target.base_array),0,0),&value,4);
            if(memcmp(memory.data(),expected.data(),expected.size())||textures.Entries().size()!=1)
                throw std::runtime_error("Mixed SRV/UAV views lost shared subresource data");
        }
    }else{
    for(u32 repeat=0;repeat<3;++repeat){
        const u32 mip=repeat%2;
        for(u32 i=0;i<3;++i){AmdGpu::Image view;
            memcpy(&view,info.flattened_ud_buf.data()+i*8,sizeof(view));view.base_level=i==2?1-mip:mip;
            memcpy(info.flattened_ud_buf.data()+i*8,&view,sizeof(view));}
        ExecuteGuestBufferCompute(device.Get(),rasterizer,pipeline,info,{},ComputeInvocation{},&textures);
        u32 value;memcpy(&value,expected.data()+GuestPixelOffset(desc,layout,mip,1,0,0),4);++value;
        for(auto [level,layer,x,word]:{std::array<u32,4>{mip,1,0,typed?0x3fc00000U:value},
            {mip,2,1,typed?0x40600000U:value+10},{1-mip,2,0,typed?u32(-6):value+100}})
            memcpy(expected.data()+GuestPixelOffset(desc,layout,level,layer,x,0),&word,4);
        if(memcmp(memory.data(),expected.data(),expected.size()))
            throw std::runtime_error("Writable mip/layer aliases lost ordered writes or changed untouched storage");
        if(textures.Entries().size()!=1)throw std::runtime_error("Writable views allocated separate native textures");
    }
    }
    rasterizer.ValidateDebugMessages();
    if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Vulkan runtime loaded");
    puts("PASS: shared UAV texture views preserve intra-dispatch alias reads, mip/layer writes and untouched PS4 storage");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
