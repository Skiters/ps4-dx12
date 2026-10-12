// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <sirit/sirit.h>
#include <array>
#include <limits>
#include <span>
#include "common/types.h"
namespace Shader::Backend::SPIRV {
struct PointOutput {Sirit::Id type,variable,value;};
struct PointDistance {Sirit::Id value;bool emulated;};
inline Sirit::Id DefinePointRuntime(Sirit::Module& ctx){
    const auto u32_type=ctx.TypeUInt(32),f32_type=ctx.TypeFloat(32);
    const auto extents=ctx.TypeArray(ctx.TypeVector(f32_type,4),ctx.Constant(u32_type,16u));
    ctx.Decorate(extents,spv::Decoration::ArrayStride,16u);
    const auto type=ctx.TypeStruct(u32_type,u32_type,u32_type,u32_type,u32_type,
        f32_type,f32_type,u32_type,f32_type,u32_type,extents,extents,extents);
    ctx.Decorate(type,spv::Decoration::Block);
    for(u32 member=0;member<9;++member)ctx.MemberDecorate(type,member,spv::Decoration::Offset,member*4);
    ctx.MemberDecorate(type,9u,spv::Decoration::Offset,36u);
    ctx.MemberDecorate(type,10u,spv::Decoration::Offset,48u);
    ctx.MemberDecorate(type,11u,spv::Decoration::Offset,304u);
    ctx.MemberDecorate(type,12u,spv::Decoration::Offset,560u);
    const auto variable=ctx.AddGlobalVariable(ctx.TypePointer(spv::StorageClass::Uniform,type),spv::StorageClass::Uniform);
    ctx.Decorate(variable,spv::Decoration::DescriptorSet,31u);ctx.Decorate(variable,spv::Decoration::Binding,0u);
    return variable;
}
// Expand a completed guest vertex without executing the guest shader again.
// Capture all outputs before EmitVertex: SPIR-V makes them undefined afterwards.
inline void EmitPointStrip(Sirit::Module& ctx,Sirit::Id runtime,Sirit::Id position,
    Sirit::Id size,Sirit::Id viewport,bool clip_depth,
    Sirit::Id output_position,std::span<const PointOutput> outputs,
    std::span<const PointDistance> distances){
    using Sirit::Id;
    const auto f32=ctx.TypeFloat(32),u32_type=ctx.TypeUInt(32),bool_type=ctx.TypeBool(),vec4=ctx.TypeVector(f32,4);
    const auto zero=ctx.Constant(u32_type,0u),one=ctx.Constant(u32_type,1u),fzero=ctx.Constant(f32,0.f);
    const auto uniform_float=ctx.TypePointer(spv::StorageClass::Uniform,f32);
    const auto safe_viewport=ctx.OpUMin(u32_type,viewport,ctx.Constant(u32_type,15u));
    const auto extent=[&](u32 component){return ctx.OpLoad(f32,ctx.OpAccessChain(uniform_float,runtime,
        ctx.Constant(u32_type,10u),safe_viewport,ctx.Constant(u32_type,component)));};
    const auto width=extent(0),height=extent(1),left=extent(2),top=extent(3);
    const auto depth=[&](u32 component){return ctx.OpLoad(f32,ctx.OpAccessChain(uniform_float,runtime,
        ctx.Constant(u32_type,11u),safe_viewport,ctx.Constant(u32_type,component)));};
    const auto min_depth=depth(0),max_depth=depth(1);
    const auto flips=ctx.OpLoad(u32_type,ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Uniform,u32_type),
        runtime,ctx.Constant(u32_type,9u)));
    const auto selected_flip=ctx.OpBitwiseAnd(u32_type,ctx.OpShiftRightLogical(u32_type,flips,safe_viewport),one);
    const auto first_flip=ctx.OpBitwiseAnd(u32_type,flips,one);
    const auto same_orientation=ctx.OpIEqual(bool_type,selected_flip,first_flip);
    const auto flip_y=ctx.OpINotEqual(bool_type,selected_flip,zero);
    const auto selected_z=ctx.OpBitwiseAnd(u32_type,ctx.OpShiftRightLogical(u32_type,flips,
        ctx.OpIAdd(u32_type,safe_viewport,ctx.Constant(u32_type,16u))),one);
    const auto flip_z=ctx.OpINotEqual(bool_type,selected_z,zero);
    const auto x=ctx.OpCompositeExtract(f32,position,0u),y=ctx.OpCompositeExtract(f32,position,1u);
    const auto z=ctx.OpCompositeExtract(f32,position,2u),w=ctx.OpCompositeExtract(f32,position,3u);
    const auto minus_w=ctx.OpFNegate(f32,w);
    auto visible=ctx.OpFOrdGreaterThan(bool_type,w,fzero);
    const auto require=[&](Id condition){visible=ctx.OpLogicalAnd(bool_type,visible,condition);};
    require(ctx.OpULessThan(bool_type,viewport,ctx.Constant(u32_type,16u)));
    require(ctx.OpFOrdGreaterThan(bool_type,size,fzero));
    require(ctx.OpFOrdLessThanEqual(bool_type,size,ctx.Constant(f32,std::numeric_limits<float>::max())));
    require(ctx.OpFOrdGreaterThan(bool_type,width,fzero));require(ctx.OpFOrdGreaterThan(bool_type,height,fzero));
    require(ctx.OpFOrdGreaterThanEqual(bool_type,x,minus_w));require(ctx.OpFOrdLessThanEqual(bool_type,x,w));
    require(ctx.OpFOrdGreaterThanEqual(bool_type,y,minus_w));require(ctx.OpFOrdLessThanEqual(bool_type,y,w));
    if(clip_depth){require(ctx.OpFOrdGreaterThanEqual(bool_type,z,fzero));require(ctx.OpFOrdLessThanEqual(bool_type,z,w));}
    for(const auto& distance:distances)require(distance.emulated?
        ctx.OpLogicalNot(bool_type,ctx.OpFOrdLessThan(bool_type,distance.value,fzero)):
        ctx.OpFOrdGreaterThanEqual(bool_type,distance.value,fzero));
    const auto body=ctx.OpLabel(),done=ctx.OpLabel();ctx.OpSelectionMerge(done,spv::SelectionControlMask::MaskNone);
    ctx.OpBranchConditional(visible,body,done);ctx.AddLabel(body);
    const auto scaled=ctx.OpFMul(f32,size,w);
    const auto dx=ctx.OpFDiv(f32,scaled,width),raw_dy=ctx.OpFDiv(f32,scaled,height);
    const auto dy=ctx.OpSelect(f32,same_orientation,raw_dy,ctx.OpFNegate(f32,raw_dy));
    const auto native_z=ctx.OpSelect(f32,flip_z,ctx.OpFSub(f32,w,z),z);
    const auto window_z=ctx.OpFAdd(f32,ctx.OpFMul(f32,min_depth,w),
        ctx.OpFMul(f32,ctx.OpFSub(f32,max_depth,min_depth),native_z));
    const auto half=ctx.Constant(f32,0.5f),window_to_clip=ctx.Constant(f32,2.f/16384.f);
    for(const auto corner:{std::array<float,2>{-1,-1},std::array<float,2>{1,-1},
                           std::array<float,2>{-1,1},std::array<float,2>{1,1}}){
        const auto cx=ctx.OpFAdd(f32,x,ctx.OpFMul(f32,dx,ctx.Constant(f32,corner[0])));
        const auto cy=ctx.OpFAdd(f32,y,ctx.OpFMul(f32,dy,ctx.Constant(f32,corner[1])));
        const auto native_y=ctx.OpSelect(f32,flip_y,ctx.OpFNegate(f32,cy),cy);
        // Keep window coordinates multiplied by w: this preserves reciprocal-w
        // interpolation and avoids dividing/remultiplying the point center.
        const auto window_x=ctx.OpFAdd(f32,ctx.OpFMul(f32,left,w),
            ctx.OpFMul(f32,width,ctx.OpFMul(f32,ctx.OpFAdd(f32,cx,w),half)));
        const auto window_y=ctx.OpFAdd(f32,ctx.OpFMul(f32,top,w),
            ctx.OpFMul(f32,height,ctx.OpFMul(f32,ctx.OpFSub(f32,w,native_y),half)));
        const auto out_x=ctx.OpFSub(f32,ctx.OpFMul(f32,window_x,window_to_clip),w);
        const auto out_y=ctx.OpFSub(f32,w,ctx.OpFMul(f32,window_y,window_to_clip));
        ctx.OpStore(output_position,ctx.OpCompositeConstruct(vec4,out_x,out_y,window_z,w));
        for(const auto& output:outputs)ctx.OpStore(output.variable,output.value);
        ctx.OpEmitVertex();
    }
    ctx.OpEndPrimitive();ctx.OpBranch(done);ctx.AddLabel(done);
    ctx.OpStore(output_position,position);
    for(const auto& output:outputs)ctx.OpStore(output.variable,output.value);
}
}
