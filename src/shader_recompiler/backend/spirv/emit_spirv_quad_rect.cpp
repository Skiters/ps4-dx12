// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <sirit/sirit.h>
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point_capture.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/renderer_d3d12/guest_point_shader.h"
#include <limits>
#include <bit>

namespace Shader::Backend::SPIRV {

using Sirit::Id;

constexpr u32 SPIRV_VERSION_1_5 = 0x00010500;

struct QuadRectListEmitter : public Sirit::Module {
    explicit QuadRectListEmitter(const HwFragmentRuntimeInfo& fs_info_, u32 input_vertices_)
        : Sirit::Module{SPIRV_VERSION_1_5}, fs_info{fs_info_}, input_vertices{input_vertices_} {
        void_id = TypeVoid();
        bool_id = TypeBool();
        float_id = TypeFloat(32);
        uint_id = TypeUInt(32U);
        int_id = TypeInt(32U, true);
        bvec2_id = TypeVector(bool_id, 2);
        vec2_id = TypeVector(float_id, 2);
        vec3_id = TypeVector(float_id, 3);
        vec4_id = TypeVector(float_id, 4);

        float_one = Constant(float_id, 1.0f);
        float_min_one = Constant(float_id, -1.0f);
        int_zero = Constant(int_id, 0);

        const Id float_arr{TypeArray(float_id, Constant(uint_id, 1U))};
        gl_per_vertex_type = TypeStruct(vec4_id, float_id, float_arr, float_arr);
        Decorate(gl_per_vertex_type, spv::Decoration::Block);
        MemberDecorate(gl_per_vertex_type, 0U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::Position));
        MemberDecorate(gl_per_vertex_type, 1U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::PointSize));
        MemberDecorate(gl_per_vertex_type, 2U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::ClipDistance));
        MemberDecorate(gl_per_vertex_type, 3U, spv::Decoration::BuiltIn,
                       static_cast<u32>(spv::BuiltIn::CullDistance));
    }

    /// Emits tessellation control shader for interpolating the 4th vertex of rectange primitive
    void EmitRectListTCS() {
        DefineEntry(spv::ExecutionModel::TessellationControl);

        // Set passthrough tessellation factors
        const Id output_float_id{TypePointer(spv::StorageClass::Output, float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{OpAccessChain(output_float_id, gl_tess_level_outer, Int(i))};
            OpStore(ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{OpAccessChain(output_float_id, gl_tess_level_inner, Int(i))};
            OpStore(ptr, float_one);
        }

        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};

        // Emit interpolation block of the 4th vertex in rect.
        // Load positions
        std::array<Id, 3> pos;
        for (int i = 0; i < 3; i++) {
            pos[i] = OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, Int(i), int_zero));
        }

        std::array<Id, 3> point_coord_equal;
        for (int i = 0; i < 3; i++) {
            // point_coord_equal[i] = equal(gl_in[i].gl_Position.xy, gl_in[(i + 1) %
            // 3].gl_Position.xy);
            const Id pos_l_xy{OpVectorShuffle(vec2_id, pos[i], pos[i], 0, 1)};
            const Id pos_r_xy{OpVectorShuffle(vec2_id, pos[(i + 1) % 3], pos[(i + 1) % 3], 0, 1)};
            point_coord_equal[i] = OpFOrdEqual(bvec2_id, pos_l_xy, pos_r_xy);
        }

        std::array<Id, 3> bary_coord;
        std::array<Id, 3> is_edge_vertex;
        for (int i = 0; i < 3; i++) {
            // bool xy_equal = point_coord_equal[i].x && point_coord_equal[(i + 2) % 3].y;
            const Id xy_equal{
                OpLogicalAnd(bool_id, OpCompositeExtract(bool_id, point_coord_equal[i], 0),
                             OpCompositeExtract(bool_id, point_coord_equal[(i + 2) % 3], 1))};
            // bool yx_equal = point_coord_equal[i].y && point_coord_equal[(i + 2) % 3].x;
            const Id yx_equal{
                OpLogicalAnd(bool_id, OpCompositeExtract(bool_id, point_coord_equal[i], 1),
                             OpCompositeExtract(bool_id, point_coord_equal[(i + 2) % 3], 0))};
            // bary_coord[i] = (xy_equal || yx_equal) ? -1.f : 1.f;
            is_edge_vertex[i] = OpLogicalOr(bool_id, xy_equal, yx_equal);
            bary_coord[i] = OpSelect(float_id, is_edge_vertex[i], float_min_one, float_one);
        }

        const auto interpolate = [&](Id v0, Id v1, Id v2) {
            // return v0 * bary_coord.x + v1 * bary_coord.y + v2 * bary_coord.z;
            const Id p0{OpVectorTimesScalar(vec4_id, v0, bary_coord[0])};
            const Id p1{OpVectorTimesScalar(vec4_id, v1, bary_coord[1])};
            const Id p2{OpVectorTimesScalar(vec4_id, v2, bary_coord[2])};
            return OpFAdd(vec4_id, p0, OpFAdd(vec4_id, p1, p2));
        };

        // int vertex_index_id = is_edge_vertex[1] ? 1 : (is_edge_vertex[2] ? 2 : 0);
        Id vertex_index{OpSelect(int_id, is_edge_vertex[2], Int(2), Int(0))};
        vertex_index = OpSelect(int_id, is_edge_vertex[1], Int(1), vertex_index);

        // int index = (vertex_index_id + gl_InvocationID) % 3;
        const Id invocation_id{OpLoad(int_id, gl_invocation_id)};
        const Id invocation_3{OpIEqual(bool_id, invocation_id, Int(3))};
        const Id index{OpSMod(int_id, OpIAdd(int_id, vertex_index, invocation_id), Int(3))};

        // gl_out[gl_InvocationID].gl_Position = gl_InvocationID == 3 ? pos3 :
        // gl_in[index].gl_Position;
        const Id pos3{interpolate(pos[0], pos[1], pos[2])};
        const Id in_ptr{OpAccessChain(input_vec4, gl_in, index, Int(0))};
        const Id position{OpSelect(vec4_id, invocation_3, pos3, OpLoad(vec4_id, in_ptr))};
        OpStore(OpAccessChain(output_vec4, gl_out, invocation_id, Int(0)), position);

        // Set attributes
        for (int i = 0; i < inputs.size(); i++) {
            // vec4 in_paramN3 = interpolate(bary_coord, in_paramN[0], in_paramN[1], in_paramN[2]);
            const Id v0{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], Int(0)))};
            const Id v1{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], Int(1)))};
            const Id v2{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], Int(2)))};
            const Id in_param3{interpolate(v0, v1, v2)};
            // out_paramN[gl_InvocationID] = gl_InvocationID == 3 ? in_paramN3 : in_paramN[index];
            const Id in_param{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], index))};
            const Id out_param{OpSelect(vec4_id, invocation_3, in_param3, in_param)};
            OpStore(OpAccessChain(output_vec4, outputs[i], invocation_id), out_param);
        }

        OpReturn();
        OpFunctionEnd();
    }

    /// Emits a passthrough quad tessellation control shader that outputs 4 control points.
    void EmitQuadListTCS() {
        DefineEntry(spv::ExecutionModel::TessellationControl);
        const Id array_type{TypeArray(int_id, Int(4))};
        const Id values{ConstantComposite(array_type, Int(1), Int(2), Int(0), Int(3))};
        const Id indices{AddLocalVariable(TypePointer(spv::StorageClass::Function, array_type),
                                          spv::StorageClass::Function, values)};

        // Set passthrough tessellation factors
        const Id output_float{TypePointer(spv::StorageClass::Output, float_id)};
        for (int i = 0; i < 4; i++) {
            const Id ptr{OpAccessChain(output_float, gl_tess_level_outer, Int(i))};
            OpStore(ptr, float_one);
        }
        for (int i = 0; i < 2; i++) {
            const Id ptr{OpAccessChain(output_float, gl_tess_level_inner, Int(i))};
            OpStore(ptr, float_one);
        }

        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};
        const Id func_int{TypePointer(spv::StorageClass::Function, int_id)};
        const Id invocation_id{OpLoad(int_id, gl_invocation_id)};
        const Id index{OpLoad(int_id, OpAccessChain(func_int, indices, invocation_id))};

        // gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
        const Id in_position{OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, index, Int(0)))};
        OpStore(OpAccessChain(output_vec4, gl_out, invocation_id, Int(0)), in_position);

        for (int i = 0; i < inputs.size(); i++) {
            // out_paramN[gl_InvocationID] = in_paramN[gl_InvocationID];
            const Id in_param{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], index))};
            OpStore(OpAccessChain(output_vec4, outputs[i], invocation_id), in_param);
        }

        OpReturn();
        OpFunctionEnd();
    }

    /// Emits a passthrough quad tessellation evaluation shader that outputs 4 control points.
    void EmitPassthroughTES() {
        DefineEntry(spv::ExecutionModel::TessellationEvaluation);

        // const int index = int(gl_TessCoord.y) * 2 + int(gl_TessCoord.x);
        const Id input_float{TypePointer(spv::StorageClass::Input, float_id)};
        const Id tess_coord_x{OpLoad(float_id, OpAccessChain(input_float, gl_tess_coord, Int(0)))};
        const Id tess_coord_y{OpLoad(float_id, OpAccessChain(input_float, gl_tess_coord, Int(1)))};
        const Id index{OpIAdd(int_id, OpIMul(int_id, OpConvertFToS(int_id, tess_coord_y), Int(2)),
                              OpConvertFToS(int_id, tess_coord_x))};

        // gl_Position = gl_in[index].gl_Position;
        const Id input_vec4{TypePointer(spv::StorageClass::Input, vec4_id)};
        const Id output_vec4{TypePointer(spv::StorageClass::Output, vec4_id)};
        const Id position{OpLoad(vec4_id, OpAccessChain(input_vec4, gl_in, index, Int(0)))};
        OpStore(OpAccessChain(output_vec4, gl_per_vertex, Int(0)), position);

        // out_paramN = in_paramN[index];
        for (int i = 0; i < inputs.size(); i++) {
            const Id param{OpLoad(vec4_id, OpAccessChain(input_vec4, inputs[i], index))};
            OpStore(outputs[i], param);
        }

        OpReturn();
        OpFunctionEnd();
    }

private:
    Id Int(s32 value) {
        return Constant(int_id, value);
    }

    Id AddInput(Id type) {
        const Id input{AddGlobalVariable(TypePointer(spv::StorageClass::Input, type),
                                         spv::StorageClass::Input)};
        interfaces.push_back(input);
        return input;
    }

    Id AddOutput(Id type) {
        const Id output{AddGlobalVariable(TypePointer(spv::StorageClass::Output, type),
                                          spv::StorageClass::Output)};
        interfaces.push_back(output);
        return output;
    }

    void DefineEntry(spv::ExecutionModel model) {
        AddCapability(spv::Capability::Shader);
        AddCapability(spv::Capability::Tessellation);
        const Id void_function{TypeFunction(void_id)};
        main = OpFunction(void_id, spv::FunctionControlMask::MaskNone, void_function);
        if (model == spv::ExecutionModel::TessellationControl) {
            AddExecutionMode(main, spv::ExecutionMode::OutputVertices, 4U);
        } else {
            AddExecutionMode(main, spv::ExecutionMode::Quads);
            AddExecutionMode(main, spv::ExecutionMode::SpacingEqual);
            AddExecutionMode(main, spv::ExecutionMode::VertexOrderCw);
        }
        DefineInputs(model);
        DefineOutputs(model);
        AddEntryPoint(model, main, "main", interfaces);
        AddLabel(OpLabel());
    }

    void DefineOutputs(spv::ExecutionModel model) {
        if (model == spv::ExecutionModel::TessellationControl) {
            const Id gl_per_vertex_array{TypeArray(gl_per_vertex_type, Constant(uint_id, 4U))};
            gl_out = AddOutput(gl_per_vertex_array);

            const Id arr2_id{TypeArray(float_id, Constant(uint_id, 2U))};
            gl_tess_level_inner = AddOutput(arr2_id);
            Decorate(gl_tess_level_inner, spv::Decoration::BuiltIn, spv::BuiltIn::TessLevelInner);
            Decorate(gl_tess_level_inner, spv::Decoration::Patch);

            const Id arr4_id{TypeArray(float_id, Constant(uint_id, 4U))};
            gl_tess_level_outer = AddOutput(arr4_id);
            Decorate(gl_tess_level_outer, spv::Decoration::BuiltIn, spv::BuiltIn::TessLevelOuter);
            Decorate(gl_tess_level_outer, spv::Decoration::Patch);
        } else {
            gl_per_vertex = AddOutput(gl_per_vertex_type);
        }
        outputs.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            outputs.emplace_back(AddOutput(model == spv::ExecutionModel::TessellationControl
                                               ? TypeArray(vec4_id, Int(4))
                                               : vec4_id));
            Decorate(outputs.back(), spv::Decoration::Location, input.param_index);
        }
    }

    void DefineInputs(spv::ExecutionModel model) {
        if (model == spv::ExecutionModel::TessellationEvaluation) {
            gl_tess_coord = AddInput(vec3_id);
            Decorate(gl_tess_coord, spv::Decoration::BuiltIn, spv::BuiltIn::TessCoord);
        } else {
            gl_invocation_id = AddInput(int_id);
            Decorate(gl_invocation_id, spv::Decoration::BuiltIn, spv::BuiltIn::InvocationId);
        }
        const Id gl_per_vertex_array{TypeArray(gl_per_vertex_type, Constant(uint_id, input_vertices))};
        gl_in = AddInput(gl_per_vertex_array);
        const Id float_arr{TypeArray(vec4_id, Int(input_vertices))};
        inputs.reserve(fs_info.num_inputs);
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault()) {
                continue;
            }
            inputs.emplace_back(AddInput(float_arr));
            Decorate(inputs.back(), spv::Decoration::Location, input.param_index);
        }
    }

private:
    HwFragmentRuntimeInfo fs_info;
    u32 input_vertices;
    Id main;
    Id void_id;
    Id bool_id;
    Id float_id;
    Id uint_id;
    Id int_id;
    Id bvec2_id;
    Id vec2_id;
    Id vec3_id;
    Id vec4_id;
    Id float_one;
    Id float_min_one;
    Id int_zero;
    Id gl_per_vertex_type;
    Id gl_in;
    union {
        Id gl_out;
        Id gl_per_vertex;
    };
    Id gl_tess_level_inner;
    Id gl_tess_level_outer;
    union {
        Id gl_tess_coord;
        Id gl_invocation_id;
    };
    std::vector<Id> inputs;
    std::vector<Id> outputs;
    std::vector<Id> interfaces;
};

std::vector<u32> EmitAuxilaryTessShader(AuxShaderType type, const HwFragmentRuntimeInfo& fs_info,
                                     u32 input_vertices) {
    QuadRectListEmitter ctx{fs_info, input_vertices};
    switch (type) {
    case AuxShaderType::RectListTCS:
        ctx.EmitRectListTCS();
        break;
    case AuxShaderType::QuadListTCS:
        ctx.EmitQuadListTCS();
        break;
    case AuxShaderType::PassthroughTES:
        ctx.EmitPassthroughTES();
        break;
    }
    return ctx.Assemble();
}

} // namespace Shader::Backend::SPIRV

namespace NativeDX12 {
// This private varying never reaches the fragment shader. Guest parameters
// occupy locations 0..31; the auxiliary GS consumes the exported point size.
std::vector<u32> EmitGuestPointGeometry(const Shader::Info& vertex,bool clip_depth,bool emulated_clip,u32 live_parameters,
    std::span<const Shader::NativePointOutput> transport){
    using Sirit::Id;
    struct Emitter:Sirit::Module {
        std::vector<Id> interfaces;
        Id Variable(Id type,spv::StorageClass storage,std::optional<u32> location={},
                    std::optional<spv::BuiltIn> builtin={}){
            const auto variable=AddGlobalVariable(TypePointer(storage,type),storage);interfaces.push_back(variable);
            if(location)Decorate(variable,spv::Decoration::Location,*location);
            if(builtin)Decorate(variable,spv::Decoration::BuiltIn,*builtin);
            return variable;
        }
        Emitter():Sirit::Module{0x00010500}{}
    } ctx;
    ctx.AddCapability(spv::Capability::Shader);ctx.AddCapability(spv::Capability::Geometry);
    const auto void_type=ctx.TypeVoid(),float_type=ctx.TypeFloat(32),uint_type=ctx.TypeUInt(32);
    const auto zero=ctx.Constant(uint_type,0u),one=ctx.Constant(uint_type,1u);
    const auto vec4=ctx.TypeVector(float_type,4),position_array=ctx.TypeArray(vec4,one);
    const auto input_position=ctx.Variable(position_array,spv::StorageClass::Input,{},spv::BuiltIn::Position);
    const auto output_position=ctx.Variable(vec4,spv::StorageClass::Output,{},spv::BuiltIn::Position);
    const auto misc_type=ctx.TypeVector(uint_type,4);
    const auto input_misc=ctx.Variable(ctx.TypeArray(misc_type,one),spv::StorageClass::Input,NativePointMiscLocation(vertex,emulated_clip,live_parameters,transport));
    const auto input_primitive=ctx.Variable(uint_type,spv::StorageClass::Input,{},spv::BuiltIn::PrimitiveId);
    const auto output_primitive=ctx.Variable(uint_type,spv::StorageClass::Output,{},spv::BuiltIn::PrimitiveId);
    const bool routed_viewport=vertex.stores.GetAny(Shader::IR::Attribute::ViewportIndex);
    Id output_viewport{},output_layer{};
    if(routed_viewport){
        ctx.AddCapability(spv::Capability::MultiViewport);
        output_viewport=ctx.Variable(uint_type,spv::StorageClass::Output,{},spv::BuiltIn::ViewportIndex);
    }
    struct Varying {Id type,input,output;};std::vector<Varying> varyings;
    if(emulated_clip){const auto type=ctx.TypeVector(float_type,Shader::MaxEmulatedClipDistances);
        varyings.push_back({type,ctx.Variable(ctx.TypeArray(type,one),spv::StorageClass::Input,0),
            ctx.Variable(type,spv::StorageClass::Output,0)});}
    const bool routed_layer=vertex.stores.GetAny(Shader::IR::Attribute::RenderTargetIndex);
    if(routed_layer)output_layer=ctx.Variable(uint_type,spv::StorageClass::Output,{},spv::BuiltIn::Layer);
    for(u32 param=0;param<Shader::IR::NumParams;++param){const auto attribute=Shader::IR::Attribute::Param0+param;
        const auto width=Shader::NativePointOutputWidth(transport,param);
        if(transport.empty()?(!(live_parameters&(1u<<param))||!vertex.stores.GetAny(attribute)):!width)continue;
        const auto components=transport.empty()?vertex.stores.NumComponents(attribute):width;
        const auto type=components==1?float_type:ctx.TypeVector(float_type,components);
        const u32 location=param+(emulated_clip?1:0);
        varyings.push_back({type,ctx.Variable(ctx.TypeArray(type,one),spv::StorageClass::Input,location),
            ctx.Variable(type,spv::StorageClass::Output,location)});
    }
    struct Distance {Id type,input;u32 components;};std::vector<Distance> distances;
    for(const auto attribute:{Shader::IR::Attribute::ClipDistance,Shader::IR::Attribute::CullDistance}){
        if(!vertex.stores.GetAny(attribute)||(emulated_clip&&attribute==Shader::IR::Attribute::ClipDistance))continue;
        const auto builtin=attribute==Shader::IR::Attribute::ClipDistance?spv::BuiltIn::ClipDistance:spv::BuiltIn::CullDistance;
        ctx.AddCapability(attribute==Shader::IR::Attribute::ClipDistance?spv::Capability::ClipDistance:spv::Capability::CullDistance);
        const u32 components=std::popcount(u32(vertex.stores.flags[Shader::Info::AttributeFlags::Index(attribute)]));
        const auto type=ctx.TypeArray(float_type,ctx.Constant(uint_type,components));
        distances.push_back({type,ctx.Variable(ctx.TypeArray(type,one),spv::StorageClass::Input,{},builtin),
            components});
    }
    const auto runtime=Shader::Backend::SPIRV::DefinePointRuntime(ctx);
    ctx.interfaces.push_back(runtime);
    const auto main=ctx.OpFunction(void_type,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(void_type));
    ctx.AddExecutionMode(main,spv::ExecutionMode::InputPoints);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputTriangleStrip);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,4u);
    ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,1u);
    ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);ctx.AddLabel(ctx.OpLabel());
    const auto position=ctx.OpLoad(vec4,ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,vec4),input_position,zero));
    const auto misc=ctx.OpLoad(misc_type,ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,misc_type),input_misc,zero));
    const auto size=ctx.OpBitcast(float_type,ctx.OpCompositeExtract(uint_type,misc,0u));
    const auto viewport=routed_viewport?ctx.OpCompositeExtract(uint_type,misc,2u):zero;
    std::vector<Shader::Backend::SPIRV::PointOutput> outputs;
    std::vector<Shader::Backend::SPIRV::PointDistance> clip_values;
    for(const auto& varying:varyings){const auto value=ctx.OpLoad(varying.type,
        ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,varying.type),varying.input,zero));
        outputs.push_back({varying.type,varying.output,value});}
    if(emulated_clip)for(u32 component=0;component<Shader::MaxEmulatedClipDistances;++component)
        clip_values.push_back({ctx.OpCompositeExtract(float_type,outputs.front().value,component),true});
    for(const auto& distance:distances){const auto value=ctx.OpLoad(distance.type,
        ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,distance.type),distance.input,zero));
        for(u32 component=0;component<distance.components;++component)
            clip_values.push_back({ctx.OpCompositeExtract(float_type,value,component),false});}
    outputs.push_back({uint_type,output_primitive,ctx.OpLoad(uint_type,input_primitive)});
    if(routed_viewport)outputs.push_back({uint_type,output_viewport,viewport});
    if(routed_layer)outputs.push_back({uint_type,output_layer,ctx.OpCompositeExtract(uint_type,misc,1u)});
    Shader::Backend::SPIRV::EmitPointStrip(ctx,runtime,position,size,viewport,
        clip_depth,output_position,outputs,clip_values);
    ctx.OpReturn();ctx.OpFunctionEnd();
    return ctx.Assemble();
}
}

namespace NativeDX12 {
namespace {
struct PointReplayEmitter : Sirit::Module {
    std::vector<Sirit::Id> interfaces;
    PointReplayEmitter():Sirit::Module{0x00010500}{AddCapability(spv::Capability::Shader);}
    Sirit::Id Variable(Sirit::Id type,spv::StorageClass storage,
        std::optional<u32> location={},std::optional<spv::BuiltIn> builtin={}){
        const auto id=AddGlobalVariable(TypePointer(storage,type),storage);interfaces.push_back(id);
        if(location)Decorate(id,spv::Decoration::Location,*location);
        if(builtin)Decorate(id,spv::Decoration::BuiltIn,*builtin);
        return id;
    }
};
}
std::vector<u32> EmitNativePointReplayVertex(){
    PointReplayEmitter ctx;
    const auto u32_type=ctx.TypeUInt(32),f32=ctx.TypeFloat(32),vec4=ctx.TypeVector(f32,4),void_type=ctx.TypeVoid();
    const auto vertex=ctx.Variable(u32_type,spv::StorageClass::Input,{},spv::BuiltIn::VertexIndex);
    const auto record=ctx.Variable(u32_type,spv::StorageClass::Output,0);
    ctx.Decorate(record,spv::Decoration::Flat);
    const auto position=ctx.Variable(vec4,spv::StorageClass::Output,{},spv::BuiltIn::Position);
    const auto main=ctx.OpFunction(void_type,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(void_type));
    ctx.AddEntryPoint(spv::ExecutionModel::Vertex,main,"main",ctx.interfaces);ctx.AddLabel(ctx.OpLabel());
    ctx.OpStore(record,ctx.OpLoad(u32_type,vertex));
    const auto zero=ctx.Constant(f32,0.f),one=ctx.Constant(f32,1.f);
    ctx.OpStore(position,ctx.ConstantComposite(vec4,zero,zero,zero,one));
    ctx.OpReturn();ctx.OpFunctionEnd();return ctx.Assemble();
}
std::vector<u32> EmitNativePrimitiveStream(const Shader::HwGeometryRuntimeInfo& geometry,const Shader::Info* input_info){
    using namespace Shader::NativePointCapture;
    const auto stride=StreamRecordWords(geometry.in_vertex_data_size);
    u32 vertices{};spv::ExecutionMode input_mode{};
    switch(geometry.in_primitive){
    case AmdGpu::PrimitiveType::PointList:vertices=1;input_mode=spv::ExecutionMode::InputPoints;break;
    case AmdGpu::PrimitiveType::LineList:
    case AmdGpu::PrimitiveType::LineStrip:vertices=2;input_mode=spv::ExecutionMode::InputLines;break;
    case AmdGpu::PrimitiveType::TriangleList:
    case AmdGpu::PrimitiveType::TriangleStrip:
    case AmdGpu::PrimitiveType::RectList:vertices=3;input_mode=spv::ExecutionMode::Triangles;break;
    case AmdGpu::PrimitiveType::AdjLineList:vertices=4;input_mode=spv::ExecutionMode::InputLinesAdjacency;break;
    case AmdGpu::PrimitiveType::AdjTriangleList:vertices=6;input_mode=spv::ExecutionMode::InputTrianglesAdjacency;break;
    default:throw std::invalid_argument("Unsupported native primitive stream input topology");
    }
    PointReplayEmitter ctx;ctx.AddCapability(spv::Capability::Geometry);
    const auto uint_type=ctx.TypeUInt(32),float_type=ctx.TypeFloat(32),void_type=ctx.TypeVoid();
    const auto uint4=ctx.TypeVector(uint_type,4),float4=ctx.TypeVector(float_type,4);
    const auto count=ctx.Constant(uint_type,vertices),zero=ctx.Constant(uint_type,0u);
    const auto primitive=ctx.Variable(uint_type,spv::StorageClass::Input,{},spv::BuiltIn::PrimitiveId);
    const auto mask=[&](Shader::IR::Attribute attribute){
        return input_info?u32(input_info->loads.flags[Shader::Info::AttributeFlags::Index(attribute)])&15u:15u;
    };
    const auto position_mask=mask(Shader::IR::Attribute::Position0);
    const auto position=position_mask?ctx.Variable(ctx.TypeArray(float4,count),spv::StorageClass::Input,{},spv::BuiltIn::Position):Sirit::Id{};
    std::vector<Sirit::Id> parameters;
    for(u32 parameter=0;parameter<geometry.in_vertex_data_size/4-1;++parameter)
        parameters.push_back(mask(Shader::IR::Attribute::Param0+parameter)?
            ctx.Variable(ctx.TypeArray(float4,count),spv::StorageClass::Input,parameter):Sirit::Id{});
    const auto output=ctx.Variable(uint4,spv::StorageClass::Output,0);
    const auto main=ctx.OpFunction(void_type,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(void_type));
    ctx.AddExecutionMode(main,input_mode);ctx.AddExecutionMode(main,spv::ExecutionMode::OutputPoints);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,vertices*stride/4);
    ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,1u);
    ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);ctx.AddLabel(ctx.OpLabel());
    const auto patch=ctx.OpLoad(uint_type,primitive);
    for(u32 vertex=0;vertex<vertices;++vertex){
        std::vector<Sirit::Id> words;words.reserve(stride);words.push_back(patch);
        const auto append=[&](Sirit::Id input,u32 live){
            for(u32 component=0;component<4;++component)
                words.push_back(live&(1u<<component)?ctx.OpBitcast(uint_type,ctx.OpLoad(float_type,ctx.OpAccessChain(
                    ctx.TypePointer(spv::StorageClass::Input,float_type),input,ctx.Constant(uint_type,vertex),ctx.Constant(uint_type,component)))):zero);
        };
        append(position,position_mask);
        for(u32 parameter=0;parameter<parameters.size();++parameter)
            append(parameters[parameter],mask(Shader::IR::Attribute::Param0+parameter));
        words.resize(stride,zero);
        // Only one uint4 crosses the output signature. Serial points preserve
        // primitive/vertex/word order without consuming a spare guest varying
        // or repeating the original LS/HS/DS effects for payload slices.
        for(u32 word=0;word<stride;word+=4){
            ctx.OpStore(output,ctx.OpCompositeConstruct(uint4,words[word],words[word+1],words[word+2],words[word+3]));
            ctx.OpEmitVertex();
        }
    }
    ctx.OpReturn();ctx.OpFunctionEnd();return ctx.Assemble();
}
std::vector<u32> EmitNativePointReplayGeometry(const Shader::Info& source,
    const Shader::HwGeometryRuntimeInfo& geometry,const Shader::Profile& profile){
    using namespace Shader::NativePointCapture;
    using namespace Shader::Backend::SPIRV;
    using Shader::IR::Attribute;
    (void)Plan::Make(1,geometry.num_invocations,geometry.output_vertices,1);
    PointReplayEmitter ctx;ctx.AddCapability(spv::Capability::Geometry);
    const auto u32_type=ctx.TypeUInt(32),f32=ctx.TypeFloat(32),boolean=ctx.TypeBool();
    const auto vec4=ctx.TypeVector(f32,4),void_type=ctx.TypeVoid();
    const auto zero=ctx.Constant(u32_type,0u),one=ctx.Constant(u32_type,1u);
    const auto input=ctx.Variable(ctx.TypeArray(u32_type,one),spv::StorageClass::Input,0);
    ctx.Decorate(input,spv::Decoration::Flat);
    const auto output=ctx.Variable(u32_type,spv::StorageClass::Output,0);
    ctx.Decorate(output,spv::Decoration::Flat);
    const auto position_output=ctx.Variable(vec4,spv::StorageClass::Output,{},spv::BuiltIn::Position);
    const auto primitive_output=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::PrimitiveId);
    Sirit::Id viewport_output{},layer_output{};
    if(source.stores.GetAny(Attribute::ViewportIndex)){
        ctx.AddCapability(spv::Capability::MultiViewport);
        viewport_output=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::ViewportIndex);
    }
    if(source.stores.GetAny(Attribute::RenderTargetIndex))
        layer_output=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::Layer);
    const auto reader=DefineNativePointCaptureReader(ctx);
    const auto runtime=DefinePointRuntime(ctx);
    ctx.interfaces.push_back(reader[0]);ctx.interfaces.push_back(runtime);
    const auto main=ctx.OpFunction(void_type,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(void_type));
    ctx.AddExecutionMode(main,spv::ExecutionMode::InputPoints);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputTriangleStrip);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,4u);
    ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,1u);
    ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);ctx.AddLabel(ctx.OpLabel());
    const auto record=ctx.OpLoad(u32_type,ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,u32_type),input,zero));
    const auto image=ctx.OpLoad(reader[1],reader[0]);
    const auto base=ctx.OpIMul(u32_type,record,ctx.Constant(u32_type,RecordWords));
    const auto word=[&](u32 offset){
        const auto value=ctx.OpImageFetch(ctx.TypeVector(u32_type,4),image,
            ctx.OpIAdd(u32_type,base,ctx.Constant(u32_type,offset)),spv::ImageOperandsMask::MaskNone,{});
        return ctx.OpCompositeExtract(u32_type,value,0u);
    };
    const auto scalar=[&](u32 offset){return ctx.OpBitcast(f32,word(offset));};
    const auto primitive_tag=word(ValidWord);
    const auto valid=ctx.OpINotEqual(boolean,primitive_tag,zero);
    const auto body=ctx.OpLabel(),done=ctx.OpLabel();
    ctx.OpSelectionMerge(done,spv::SelectionControlMask::MaskNone);
    ctx.OpBranchConditional(valid,body,done);ctx.AddLabel(body);
    auto position=ctx.OpCompositeConstruct(vec4,scalar(PositionWord),scalar(PositionWord+1),
        scalar(PositionWord+2),scalar(PositionWord+3));
    const auto viewport=viewport_output.value?word(ViewportWord):zero;
    if(geometry.native_point_negative_one_to_one){
        const auto z=ctx.OpCompositeExtract(f32,position,2u),w=ctx.OpCompositeExtract(f32,position,3u);
        position=ctx.OpCompositeInsert(vec4,ctx.OpFMul(f32,ctx.OpFAdd(f32,z,w),ctx.Constant(f32,.5f)),position,2u);
    }
    if(geometry.native_point_clip_disable){
        const auto selected=ctx.OpUMin(u32_type,viewport,ctx.Constant(u32_type,15u));
        const auto transform=[&](u32 component){return ctx.OpLoad(f32,ctx.OpAccessChain(
            ctx.TypePointer(spv::StorageClass::Uniform,f32),runtime,ctx.Constant(u32_type,12u),selected,
            ctx.Constant(u32_type,component)));};
        for(u32 axis=0;axis<2;++axis){
            const auto window=ctx.OpFAdd(f32,ctx.OpFMul(f32,ctx.OpCompositeExtract(f32,position,axis),transform(axis+2)),transform(axis));
            const float half_extent=float(std::min(axis?profile.max_viewport_height/2:profile.max_viewport_width/2,8192u));
            if(!(half_extent>0))throw std::invalid_argument("Invalid point replay clip extent");
            position=ctx.OpCompositeInsert(vec4,ctx.OpFSub(f32,ctx.OpFDiv(f32,window,ctx.Constant(f32,half_extent)),
                ctx.Constant(f32,1.f)),position,axis);
        }
    }
    std::vector<PointOutput> outputs{{u32_type,output,record},{u32_type,primitive_output,
        ctx.OpISub(u32_type,primitive_tag,one)}};
    if(viewport_output.value)outputs.push_back({u32_type,viewport_output,viewport});
    if(layer_output.value)outputs.push_back({u32_type,layer_output,word(LayerWord)});
    std::vector<PointDistance> distances;
    for(const auto attribute:{Attribute::ClipDistance,Attribute::CullDistance}){
        const bool clip=attribute==Attribute::ClipDistance;
        const u32 mask=source.stores.flags[Shader::Info::AttributeFlags::Index(attribute)];
        for(u32 component=0;component<8;++component)if(mask&(1u<<component))
            distances.push_back({scalar((clip?ClipWord:CullWord)+component),clip&&profile.needs_clip_distance_emulation});
    }
    EmitPointStrip(ctx,runtime,position,scalar(SizeWord),viewport,geometry.native_point_clip_depth,
        position_output,outputs,distances);
    ctx.OpBranch(done);ctx.AddLabel(done);ctx.OpReturn();ctx.OpFunctionEnd();return ctx.Assemble();
}
}
