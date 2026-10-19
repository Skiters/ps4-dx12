// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shader_recompiler/backend/spirv/spirv_emit_context.h"
#include "shader_recompiler/native_point_capture.h"
namespace Shader::Backend::SPIRV {
inline std::array<Sirit::Id,2> DefineNativePointCaptureReader(Sirit::Module& ctx,
    u32 binding=NativePointCapture::Binding){
    ctx.AddCapability(spv::Capability::SampledBuffer);
    const auto type=ctx.TypeImage(ctx.TypeUInt(32),spv::Dim::Buffer,false,false,false,1u,spv::ImageFormat::Unknown);
    const auto variable=ctx.AddGlobalVariable(ctx.TypePointer(spv::StorageClass::UniformConstant,type),spv::StorageClass::UniformConstant);
    ctx.Name(variable,"native_point_capture");
    ctx.Decorate(variable,spv::Decoration::DescriptorSet,NativePointCapture::DescriptorSet);
    ctx.Decorate(variable,spv::Decoration::Binding,binding);
    return {variable,type};
}
inline Id ReadNativePointStream(EmitContext& ctx,u32 vertex,u32 word){
    const auto& geometry=ctx.runtime_info.hw.gs;
    const u32 stride=NativePointCapture::StreamRecordWords(geometry.in_vertex_data_size);
    if(vertex>=ctx.native_point_stream_vertices||word>geometry.in_vertex_data_size)
        throw std::out_of_range("Captured geometry input exceeds its original vertex layout");
    const auto primitive=ctx.OpLoad(ctx.U32[1],ctx.primitive_id);
    const auto record=ctx.OpIAdd(ctx.U32[1],ctx.OpIMul(ctx.U32[1],primitive,
        ctx.ConstU32(ctx.native_point_stream_vertices)),ctx.ConstU32(vertex));
    const auto offset=ctx.OpIAdd(ctx.U32[1],ctx.OpIMul(ctx.U32[1],record,
        ctx.ConstU32(stride)),ctx.ConstU32(word));
    const auto image=ctx.OpLoad(ctx.native_point_stream_image_type,ctx.native_point_stream_buffer);
    return ctx.OpCompositeExtract(ctx.U32[1],ctx.OpImageFetch(ctx.U32[4],image,offset,
        spv::ImageOperandsMask::MaskNone,{}),0u);
}
inline Sirit::Id DefineNativePointCaptureBuffer(Sirit::Module& ctx,bool writable){
    const auto uint_type=ctx.TypeUInt(32),words=ctx.TypeRuntimeArray(uint_type);
    ctx.Decorate(words,spv::Decoration::ArrayStride,4u);
    const auto block=ctx.TypeStruct(words);ctx.Decorate(block,spv::Decoration::Block);
    ctx.MemberDecorate(block,0,spv::Decoration::Offset,0u);
    const auto variable=ctx.AddGlobalVariable(ctx.TypePointer(spv::StorageClass::StorageBuffer,block),spv::StorageClass::StorageBuffer);
    ctx.Name(variable,"native_point_capture");
    ctx.Decorate(variable,spv::Decoration::DescriptorSet,NativePointCapture::DescriptorSet);
    ctx.Decorate(variable,spv::Decoration::Binding,NativePointCapture::Binding);
    if(!writable)ctx.Decorate(variable,spv::Decoration::NonWritable);
    return variable;
}
inline void EmitNativePointCapture(EmitContext& ctx){
    const auto& geometry=ctx.runtime_info.hw.gs;
    const auto ordinal=ctx.OpLoad(ctx.U32[1],ctx.native_point_capture_counter);
    const auto valid=ctx.OpULessThan(ctx.U1[1],ordinal,ctx.ConstU32(geometry.output_vertices));
    // Saturate discarded emissions instead of allowing ordinal wraparound to
    // overwrite earlier records. Original guest code still executes once.
    ctx.OpStore(ctx.native_point_capture_counter,ctx.OpSelect(ctx.U32[1],valid,
        ctx.OpIAdd(ctx.U32[1],ordinal,ctx.ConstU32(1u)),ordinal));
    const auto write_label=ctx.OpLabel(),end_label=ctx.OpLabel();
    ctx.OpSelectionMerge(end_label,spv::SelectionControlMask::MaskNone);
    ctx.OpBranchConditional(valid,write_label,end_label);ctx.AddLabel(write_label);
    const auto primitive=ctx.OpLoad(ctx.U32[1],ctx.primitive_id);
    const auto invocation=ctx.OpLoad(ctx.U32[1],ctx.invocation_id);
    const auto call=ctx.OpIAdd(ctx.U32[1],ctx.OpIMul(ctx.U32[1],primitive,
        ctx.ConstU32(geometry.num_invocations)),invocation);
    const auto record=ctx.OpIAdd(ctx.U32[1],ctx.OpIMul(ctx.U32[1],call,
        ctx.ConstU32(geometry.output_vertices)),ordinal);
    const auto base=ctx.OpIMul(ctx.U32[1],record,ctx.ConstU32(NativePointCapture::RecordWords));
    const auto write=[&](u32 word,Id value){
        ctx.OpStore(ctx.OpAccessChain(ctx.native_point_capture_pointer,ctx.native_point_capture_buffer,
            ctx.u32_zero_value,ctx.OpIAdd(ctx.U32[1],base,ctx.ConstU32(word))),value);
    };
    const auto position=ctx.OpLoad(ctx.F32[4],ctx.output_position);
    for(u32 component=0;component<4;++component)
        write(NativePointCapture::PositionWord+component,ctx.OpBitcast(ctx.U32[1],
            ctx.OpCompositeExtract(ctx.F32[1],position,component)));
    write(NativePointCapture::SizeWord,ctx.info.stores.GetAny(IR::Attribute::PointSize)?
        ctx.OpBitcast(ctx.U32[1],ctx.OpLoad(ctx.F32[1],ctx.output_point_size)):ctx.ConstU32(0x3f800000u));
    write(NativePointCapture::LayerWord,ctx.info.stores.GetAny(IR::Attribute::RenderTargetIndex)?
        ctx.OpLoad(ctx.U32[1],ctx.output_layer):ctx.u32_zero_value);
    write(NativePointCapture::ViewportWord,ctx.info.stores.GetAny(IR::Attribute::ViewportIndex)?
        ctx.OpLoad(ctx.U32[1],ctx.output_viewport_index):ctx.u32_zero_value);
    const auto distance_pointer=ctx.TypePointer(spv::StorageClass::Private,ctx.F32[1]);
    for(u32 distance=0;distance<2;++distance){
        const auto attribute=distance?IR::Attribute::CullDistance:IR::Attribute::ClipDistance;
        const auto variable=distance?ctx.cull_distances:ctx.clip_distances;
        const auto offset=distance?NativePointCapture::CullWord:NativePointCapture::ClipWord;
        for(u32 component=0;component<8;++component){
            const auto value=ctx.info.stores.GetAny(attribute)?ctx.OpBitcast(ctx.U32[1],
                ctx.OpLoad(ctx.F32[1],ctx.OpAccessChain(distance_pointer,variable,ctx.ConstU32(component)))):
                ctx.u32_zero_value;
            write(offset+component,value);
        }
    }
    for(u32 source=0;source<ctx.info.gs_copy_data.num_attrs;++source){
        const auto& parameter=ctx.output_params[source];
        const auto value=ctx.OpLoad(ctx.F32[4],parameter.id);
        for(u32 component=0;component<4;++component)
            write(NativePointCapture::ParameterWord+source*4+component,ctx.OpBitcast(ctx.U32[1],
                ctx.OpCompositeExtract(ctx.F32[1],value,component)));
    }
    // Slot addressing uses the generated primitive ordinal above. The guest
    // and fragment replay retain the pre-tessellation patch identity instead.
    const auto guest_primitive=geometry.native_point_capture_stream_input?
        ReadNativePointStream(ctx,0,NativePointCapture::StreamPatchWord):primitive;
    write(NativePointCapture::ValidWord,ctx.OpIAdd(ctx.U32[1],guest_primitive,ctx.ConstU32(1u)));
    ctx.OpBranch(end_label);ctx.AddLabel(end_label);
}
}
