// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <sirit/sirit.h>
#include "shader_recompiler/backend/spirv/emit_spirv_quad_rect.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point.h"
#include "shader_recompiler/backend/spirv/emit_spirv_point_capture.h"
#include "shader_recompiler/runtime_info.h"
#include "video_core/renderer_d3d12/guest_point_shader.h"
#include "video_core/renderer_d3d12/guest_point_transport.h"
#include <limits>
#include <bit>
#include <cmath>

namespace Shader::Backend::SPIRV {

using Sirit::Id;

constexpr u32 SPIRV_VERSION_1_5 = 0x00010500;

struct QuadRectListEmitter : public Sirit::Module {
    explicit QuadRectListEmitter(const HwFragmentRuntimeInfo& fs_info_, u32 input_vertices_)
        : Sirit::Module{SPIRV_VERSION_1_5}, fs_info{fs_info_}, input_vertices{input_vertices_} {
        if(fs_info.num_inputs>std::size(fs_info.inputs))
            throw std::out_of_range("Invalid auxiliary tessellation fragment input count");
        for(u32 i=0;i<fs_info.num_inputs;++i)
            if(!fs_info.inputs[i].IsDefault()&&fs_info.inputs[i].param_index>=32)
                throw std::out_of_range("Invalid auxiliary tessellation parameter location");
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
        u32 seen_parameters{};
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault() || (seen_parameters & (1u << input.param_index))) {
                continue;
            }
            seen_parameters |= 1u << input.param_index;
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
        u32 seen_parameters{};
        for (int i = 0; i < fs_info.num_inputs; i++) {
            const auto& input = fs_info.inputs[i];
            if (input.IsDefault() || (seen_parameters & (1u << input.param_index))) {
                continue;
            }
            seen_parameters |= 1u << input.param_index;
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
std::vector<u32> EmitGuestPolygonPoints(const Shader::Info& vertex,const Shader::Info* fragment,
    const Shader::HwFragmentRuntimeInfo& fs,const GuestPolygonPointConfig& config){
    if(config.first_provoking>=3)throw std::out_of_range("Invalid polygon point provoking vertex");
    if(config.depth_bias_enabled&&(!std::isfinite(config.depth_bias)||
       !std::isfinite(config.slope_depth_bias)||!std::isfinite(config.depth_bias_clamp)))
        throw std::runtime_error("Non-finite polygon point depth bias");
    using Sirit::Id;
    using A=Shader::IR::Attribute;
    using Q=Shader::Qualifier;
    const bool sized=vertex.stores.GetAny(A::PointSize),routed_viewport=vertex.stores.GetAny(A::ViewportIndex),
        routed_layer=vertex.stores.GetAny(A::RenderTargetIndex);
    PointReplayEmitter ctx;ctx.AddCapability(spv::Capability::Geometry);
    const auto u32_type=ctx.TypeUInt(32),f32=ctx.TypeFloat(32),b1=ctx.TypeBool();
    const auto vec4=ctx.TypeVector(f32,4),void_type=ctx.TypeVoid();
    const auto zero=ctx.Constant(u32_type,0u),one=ctx.Constant(u32_type,1u),three=ctx.Constant(u32_type,3u);
    const auto fzero=ctx.Constant(f32,0.f),fone=ctx.Constant(f32,1.f);
    const auto position=ctx.Variable(ctx.TypeArray(vec4,three),spv::StorageClass::Input,{},spv::BuiltIn::Position);
    const auto output_position=ctx.Variable(vec4,spv::StorageClass::Output,{},spv::BuiltIn::Position);
    const auto primitive=ctx.Variable(u32_type,spv::StorageClass::Input,{},spv::BuiltIn::PrimitiveId);
    const auto output_primitive=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::PrimitiveId);
    Id input_misc{},output_viewport{},output_layer{};
    const auto uint4=ctx.TypeVector(u32_type,4);
    if(sized||routed_viewport||routed_layer)
        input_misc=ctx.Variable(ctx.TypeArray(uint4,three),spv::StorageClass::Input,
            NativePolygonPointMiscLocation(vertex,config.emulated_clip,NativePolygonPointLiveParameters(fragment,fs),config.compact_parameters));
    if(routed_viewport){ctx.AddCapability(spv::Capability::MultiViewport);
        output_viewport=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::ViewportIndex);}
    if(routed_layer)output_layer=ctx.Variable(u32_type,spv::StorageClass::Output,{},spv::BuiltIn::Layer);
    struct Varying {Id type,input;u32 width;Q qualifier;u32 location,offset,mask;};
    struct Output {Id type,variable;u32 width;};
    std::vector<Varying> varyings;
    std::array<Output,32> varying_outputs{};
    std::array<Id,32> varying_inputs{};
    u32 output_components=5+u32(routed_viewport)+u32(routed_layer);
    // The same guest parameter may be consumed with flat, smooth and linear
    // interpolation at once. Keep separate clipping values, but reuse the
    // existing PS transport packing for their final point-constant payload.
    const auto input_count=fs.num_inputs-u32(fs.clip_distance_emulation);
    NativePointTransport transport;
    if(fragment&&fs.native_point_input_compaction)
        transport=BuildNativePointTransport(*fragment,input_count,[&](u32 i){return fs.inputs[i];});
    const auto add_varying=[&](u32 parameter,u32 location,u32 offset,u32 mask,Q qualifier,u32 output_width){
        const u32 width=vertex.stores.NumComponents(A::Param0+parameter);
        const auto type=width==1?f32:ctx.TypeVector(f32,width);
        const u32 input_location=(config.compact_parameters?
            NativePolygonPointParameterLocation(vertex,parameter,NativePolygonPointLiveParameters(fragment,fs)):
            parameter)+u32(config.emulated_clip);
        const u32 output_location=location+u32(config.emulated_clip);
        if(input_location>=32||output_location>=32)
            throw std::runtime_error("Polygon point interface exceeds native varying locations");
        if(!Sirit::ValidId(varying_inputs[parameter]))
            varying_inputs[parameter]=ctx.Variable(ctx.TypeArray(type,three),spv::StorageClass::Input,input_location);
        auto& output=varying_outputs[location];
        if(!output.width){
            output.width=output_width;output.type=output_width==1?f32:ctx.TypeVector(f32,output_width);
            output.variable=ctx.Variable(output.type,spv::StorageClass::Output,output_location);
            output_components+=output_width;
        }
        varyings.push_back({type,varying_inputs[parameter],width,qualifier,location,offset,mask});
    };
    if(fragment&&fs.native_point_input_compaction){
        for(const auto& group:transport.outputs){
            if(!group.mask||!vertex.stores.GetAny(A::Param0+group.source))continue;
            std::optional<Q> qualifier;
            for(u32 i=0;i<input_count;++i)if(fragment->loads.GetAny(A::Param0+i)&&!fs.inputs[i].IsDefault()&&
                fs.inputs[i].param_index==group.source&&transport.locations[i]==group.location&&
                transport.offsets[i]==group.component){qualifier=fragment->fs_interpolation[i].primary;break;}
            if(qualifier==Q::PerVertex)
                throw std::runtime_error("Polygon point fill needs per-vertex barycentric transport");
            if(!qualifier)throw std::logic_error("Polygon point transport lost its interpolation source");
            const auto interpolation=*qualifier==Q::Flat?Q::Flat:*qualifier==Q::NoPerspective?Q::NoPerspective:Q::Smooth;
            add_varying(group.source,group.location,group.component,group.mask,interpolation,
                Shader::NativePointOutputWidth(transport.outputs,group.location));
        }
    }else for(u32 parameter=0;parameter<Shader::IR::NumParams;++parameter){
        if(!vertex.stores.GetAny(A::Param0+parameter))continue;
        std::optional<Q> qualifier;
        if(fragment)for(u32 input=0;input<input_count;++input){
            if(!fragment->loads.GetAny(A::Param0+input)||fs.inputs[input].param_index!=parameter||
                (fs.inputs[input].is_default&&!fs.inputs[input].is_flat))continue;
            const auto q=fs.inputs[input].is_flat?Q::Flat:fragment->fs_interpolation[input].primary;
            if(q==Q::PerVertex)throw std::runtime_error("Polygon point fill needs per-vertex barycentric transport");
            const auto interpolation=q==Q::Flat?Q::Flat:q==Q::NoPerspective?Q::NoPerspective:Q::Smooth;
            if(qualifier&&*qualifier!=interpolation)
                throw std::runtime_error("Polygon point fill requires the compact fragment transport");
            qualifier=interpolation;
        }
        if(!qualifier)continue;
        const u32 width=vertex.stores.NumComponents(A::Param0+parameter);
        add_varying(parameter,parameter,0,(1u<<width)-1,*qualifier,width);
    }
    const u32 clip_mask=vertex.stores.flags[Shader::Info::AttributeFlags::Index(A::ClipDistance)];
    const u32 cull_mask=vertex.stores.flags[Shader::Info::AttributeFlags::Index(A::CullDistance)];
    Id clips{},culls{},output_clips{};
    const auto distances=ctx.TypeArray(f32,ctx.Constant(u32_type,8u));
    if(config.emulated_clip){
        static_assert(Shader::MaxEmulatedClipDistances==4);
        if(clip_mask&~15u)throw std::runtime_error("Polygon point emulated clip payload exceeds four distances");
        clips=ctx.Variable(ctx.TypeArray(vec4,three),spv::StorageClass::Input,0);
        output_clips=ctx.Variable(vec4,spv::StorageClass::Output,0);output_components+=4;
    }else if(clip_mask){ctx.AddCapability(spv::Capability::ClipDistance);
        clips=ctx.Variable(ctx.TypeArray(distances,three),spv::StorageClass::Input,{},spv::BuiltIn::ClipDistance);}
    if(cull_mask){ctx.AddCapability(spv::Capability::CullDistance);
        culls=ctx.Variable(ctx.TypeArray(distances,three),spv::StorageClass::Input,{},spv::BuiltIn::CullDistance);}
    const u32 planes=(config.clip_depth?6:4)+std::popcount(clip_mask),capacity=3+planes;
    const u32 size_field=1+u32(varyings.size()),clip_field=size_field+u32(sized);
    const u32 fields=clip_field+u32(clip_mask!=0)*2;
    // D3D GS instancing appends each instance's output in instance-ID order
    // (functional spec13.2.1). Partition only the completed polygon vertices;
    // each instance clips the same immutable producer payload, without rerunning
    // any guest VS/TES effects. The1024-scalar limit applies per instance.
    const u32 points_per_instance=std::min(capacity,1024/(4*output_components));
    if(!points_per_instance)throw std::runtime_error("Polygon point vertex exceeds native geometry output capacity");
    const u32 instances=(capacity+points_per_instance-1)/points_per_instance;
    if(instances>32)throw std::runtime_error("Polygon point output exceeds native geometry instance capacity");
    const auto invocation=instances>1?
        ctx.Variable(u32_type,spv::StorageClass::Input,{},spv::BuiltIn::InvocationId):Id{};
    const auto runtime=Shader::Backend::SPIRV::DefinePointRuntime(ctx);ctx.interfaces.push_back(runtime);
    const auto main=ctx.OpFunction(void_type,spv::FunctionControlMask::MaskNone,ctx.TypeFunction(void_type));
    ctx.AddExecutionMode(main,spv::ExecutionMode::Triangles);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputTriangleStrip);
    ctx.AddExecutionMode(main,spv::ExecutionMode::OutputVertices,points_per_instance*4);
    ctx.AddExecutionMode(main,spv::ExecutionMode::Invocations,instances);
    ctx.AddEntryPoint(spv::ExecutionModel::Geometry,main,"main",ctx.interfaces);ctx.AddLabel(ctx.OpLabel());
    const auto fn_u32=ctx.TypePointer(spv::StorageClass::Function,u32_type);
    const auto fn_float=ctx.TypePointer(spv::StorageClass::Function,f32);
    const auto fn_vec=ctx.TypePointer(spv::StorageClass::Function,vec4);
    const auto array=ctx.TypeArray(vec4,ctx.Constant(u32_type,capacity*fields));
    const auto fn_array=ctx.TypePointer(spv::StorageClass::Function,array);
    std::array<Id,2> data{ctx.AddLocalVariable(fn_array,spv::StorageClass::Function),ctx.AddLocalVariable(fn_array,spv::StorageClass::Function)};
    const auto count=ctx.AddLocalVariable(fn_u32,spv::StorageClass::Function);
    const auto next_count=ctx.AddLocalVariable(fn_u32,spv::StorageClass::Function);
    const auto cursor=ctx.AddLocalVariable(fn_u32,spv::StorageClass::Function);
    const auto area_sum=ctx.AddLocalVariable(fn_float,spv::StorageClass::Function);
    const auto slope_x=ctx.AddLocalVariable(fn_float,spv::StorageClass::Function);
    const auto slope_y=ctx.AddLocalVariable(fn_float,spv::StorageClass::Function);
    const auto max_depth=ctx.AddLocalVariable(fn_float,spv::StorageClass::Function);
    const auto ci=[&](u32 value){return ctx.Constant(u32_type,value);};
    const auto pointer=[&](Id buffer,Id vertex_index,u32 field){return ctx.OpAccessChain(fn_vec,buffer,
        ctx.OpIAdd(u32_type,ctx.OpIMul(u32_type,vertex_index,ci(fields)),ci(field)));};
    const auto load=[&](Id buffer,Id index,u32 field){return ctx.OpLoad(vec4,pointer(buffer,index,field));};
    const auto component=[&](Id value,u32 channel){return ctx.OpCompositeExtract(f32,value,channel);};
    const auto input=[&](Id type,Id variable,Id index){return ctx.OpLoad(type,
        ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,type),variable,index));};
    const auto distance_input=[&](Id variable,u32 index,u32 channel){return ctx.OpLoad(f32,
        ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Input,f32),variable,ci(index),ci(channel)));};
    const auto selection=[&](Id condition,const auto& body){
        const auto active=ctx.OpLabel(),done=ctx.OpLabel();
        ctx.OpSelectionMerge(done,spv::SelectionControlMask::MaskNone);
        ctx.OpBranchConditional(condition,active,done);ctx.AddLabel(active);
        body();ctx.OpBranch(done);ctx.AddLabel(done);
    };
    const auto loop=[&](Id bound,const auto& body,Id first=Id{}){
        ctx.OpStore(cursor,Sirit::ValidId(first)?first:zero);
        const auto header=ctx.OpLabel(),active=ctx.OpLabel(),step=ctx.OpLabel(),done=ctx.OpLabel();
        ctx.OpBranch(header);ctx.AddLabel(header);
        const auto index=ctx.OpLoad(u32_type,cursor);
        const auto condition=ctx.OpULessThan(b1,index,bound);
        ctx.OpLoopMerge(done,step,spv::LoopControlMask::MaskNone);
        ctx.OpBranchConditional(condition,active,done);ctx.AddLabel(active);body(index);
        ctx.OpBranch(step);ctx.AddLabel(step);
        ctx.OpStore(cursor,ctx.OpIAdd(u32_type,index,one));ctx.OpBranch(header);ctx.AddLabel(done);
    };
    ctx.OpStore(count,three);
    const auto provoking=ci(config.last_provoking?2:config.first_provoking);
    const auto misc=Sirit::ValidId(input_misc)?input(uint4,input_misc,provoking):Id{};
    const auto viewport=routed_viewport?ctx.OpCompositeExtract(u32_type,misc,2u):zero;
    const auto viewport_value=[&](u32 member,u32 channel){return ctx.OpLoad(f32,
        ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Uniform,f32),runtime,
            ci(member),ctx.OpUMin(u32_type,viewport,ci(15)),ci(channel)));};
    const auto depth_min=viewport_value(11,0),depth_max=viewport_value(11,1);
    const auto depth_scale=ctx.OpFSub(f32,depth_max,depth_min);
    const auto viewport_flips=ctx.OpLoad(u32_type,ctx.OpAccessChain(
        ctx.TypePointer(spv::StorageClass::Uniform,u32_type),runtime,ci(9)));
    const auto flip_depth=ctx.OpINotEqual(b1,ctx.OpBitwiseAnd(u32_type,
        ctx.OpShiftRightLogical(u32_type,viewport_flips,ctx.OpIAdd(u32_type,ctx.OpUMin(u32_type,viewport,ci(15)),ci(16))),one),zero);
    for(u32 index=0;index<3;++index){
        ctx.OpStore(pointer(data[0],ci(index),0),input(vec4,position,ci(index)));
        for(u32 field=0;field<varyings.size();++field){const auto& varying=varyings[field];
            const auto value=input(varying.type,varying.input,
                varying.qualifier==Q::Flat?provoking:ci(index));
            std::array<Id,4> channels{fzero,fzero,fzero,fzero};
            for(u32 channel=0;channel<varying.width;++channel)
                channels[channel]=varying.width==1?value:component(value,channel);
            ctx.OpStore(pointer(data[0],ci(index),field+1),ctx.OpCompositeConstruct(vec4,channels));
        }
        if(sized){const auto value=input(uint4,input_misc,ci(index));
            const auto size=ctx.OpBitcast(f32,ctx.OpCompositeExtract(u32_type,value,0u));
            ctx.OpStore(pointer(data[0],ci(index),size_field),ctx.OpCompositeConstruct(vec4,size,fzero,fzero,fzero));}
        if(clip_mask)for(u32 word=0;word<2;++word){std::array<Id,4> channels{};
            for(u32 channel=0;channel<4;++channel)channels[channel]=config.emulated_clip&&word?fzero:
                distance_input(clips,index,word*4+channel);
            ctx.OpStore(pointer(data[0],ci(index),clip_field+word),ctx.OpCompositeConstruct(vec4,channels));}
    }
    auto visible=ctx.ConstantTrue(b1);
    for(u32 channel=0;channel<8;++channel)if(cull_mask&(1u<<channel)){
        auto outside=ctx.ConstantTrue(b1);
        for(u32 index=0;index<3;++index)outside=ctx.OpLogicalAnd(b1,outside,
            ctx.OpFOrdLessThan(b1,distance_input(culls,index,channel),fzero));
        visible=ctx.OpLogicalAnd(b1,visible,ctx.OpLogicalNot(b1,outside));
    }
    selection(visible,[&]{
        // Sutherland-Hodgman in homogeneous clip space. A convex triangle
        // gains at most one vertex per plane; no CPU readback or shader replay.
        std::vector<u32> clipping_planes{0,1,2,3};
        if(config.clip_depth){clipping_planes.push_back(4);clipping_planes.push_back(5);}
        for(u32 channel=0;channel<8;++channel)if(clip_mask&(1u<<channel))clipping_planes.push_back(6+channel);
        u32 side{};
        for(const auto plane:clipping_planes){
            const auto from=data[side],to=data[side^1];
            const auto n=ctx.OpLoad(u32_type,count);ctx.OpStore(next_count,zero);
            const auto distance=[&](Id index,Id p){
                if(plane>=6)return component(load(from,index,clip_field+(plane-6)/4),(plane-6)%4);
                const auto w=component(p,3),axis=component(p,plane/2);
                if(plane==4)return axis;
                return plane%2?ctx.OpFSub(f32,w,axis):ctx.OpFAdd(f32,w,axis);
            };
            loop(n,[&](Id current){
                const auto previous=ctx.OpSelect(u32_type,ctx.OpIEqual(b1,current,zero),
                    ctx.OpISub(u32_type,n,one),ctx.OpISub(u32_type,current,one));
                const auto a=load(from,previous,0),b=load(from,current,0);
                const auto da=distance(previous,a),db=distance(current,b);
                const auto inside_a=ctx.OpFOrdGreaterThanEqual(b1,da,fzero),inside_b=ctx.OpFOrdGreaterThanEqual(b1,db,fzero);
                // A vertex exactly on the plane is emitted by the inside
                // branch. Do not emit that endpoint a second time: duplicate
                // vertices would blend/stencil twice in polygon point mode.
                const auto crosses=ctx.OpLogicalAnd(b1,ctx.OpLogicalNotEqual(b1,inside_a,inside_b),
                    ctx.OpLogicalAnd(b1,ctx.OpFOrdNotEqual(b1,da,fzero),ctx.OpFOrdNotEqual(b1,db,fzero)));
                selection(crosses,[&]{
                    const auto t=ctx.OpFDiv(f32,da,ctx.OpFSub(f32,da,db));
                    const auto lerp=[&](Id x,Id y,Id weight){return ctx.OpFAdd(vec4,x,
                        ctx.OpVectorTimesScalar(vec4,ctx.OpFSub(vec4,y,x),weight));};
                    auto p=lerp(a,b,t);
                    // Snap the intersected plane exactly; roundoff must not
                    // reject its point center during the later point expansion.
                    if(plane<6){const auto w=component(p,3);const auto coordinate=plane==4?fzero:
                        plane%2?w:ctx.OpFNegate(f32,w);p=ctx.OpCompositeInsert(vec4,coordinate,p,plane/2);}
                    const auto out=ctx.OpLoad(u32_type,next_count);ctx.OpStore(pointer(to,out,0),p);
                    for(u32 field=1;field<fields;++field){
                        auto weight=t;
                        if(field<=varyings.size()&&varyings[field-1].qualifier==Q::NoPerspective)
                            weight=ctx.OpFDiv(f32,ctx.OpFMul(f32,t,component(b,3)),component(p,3));
                        const auto x=load(from,previous,field),y=load(from,current,field);
                        auto value=field<=varyings.size()&&varyings[field-1].qualifier==Q::Flat?x:lerp(x,y,weight);
                        if(plane>=6&&field==clip_field+(plane-6)/4)
                            value=ctx.OpCompositeInsert(vec4,fzero,value,(plane-6)%4);
                        ctx.OpStore(pointer(to,out,field),value);
                    }
                    ctx.OpStore(next_count,ctx.OpIAdd(u32_type,out,one));
                });
                selection(inside_b,[&]{const auto out=ctx.OpLoad(u32_type,next_count);
                    for(u32 field=0;field<fields;++field)ctx.OpStore(pointer(to,out,field),load(from,current,field));
                    ctx.OpStore(next_count,ctx.OpIAdd(u32_type,out,one));});
            });
            ctx.OpStore(count,ctx.OpLoad(u32_type,next_count));side^=1;
        }
        const auto n=ctx.OpLoad(u32_type,count),clipped=data[side];
        selection(ctx.OpUGreaterThanEqual(b1,n,three),[&]{
            ctx.OpStore(area_sum,fzero);
            if(config.depth_bias_enabled){ctx.OpStore(slope_x,fzero);ctx.OpStore(slope_y,fzero);ctx.OpStore(max_depth,fzero);}
            loop(n,[&](Id index){const auto next=ctx.OpIAdd(u32_type,index,one);
                const auto a=load(clipped,index,0),b=load(clipped,ctx.OpSelect(u32_type,ctx.OpIEqual(b1,next,n),zero,next),0);
                const auto cross=ctx.OpFSub(f32,ctx.OpFMul(f32,component(a,0),component(b,1)),
                    ctx.OpFMul(f32,component(b,0),component(a,1)));
                const auto projected=ctx.OpFDiv(f32,cross,ctx.OpFMul(f32,component(a,3),component(b,3)));
                ctx.OpStore(area_sum,ctx.OpFAdd(f32,ctx.OpLoad(f32,area_sum),projected));
                if(config.depth_bias_enabled){
                    const auto ax=ctx.OpFDiv(f32,component(a,0),component(a,3));
                    const auto ay=ctx.OpFDiv(f32,component(a,1),component(a,3));
                    const auto az=ctx.OpFDiv(f32,component(a,2),component(a,3));
                    const auto bx=ctx.OpFDiv(f32,component(b,0),component(b,3));
                    const auto by=ctx.OpFDiv(f32,component(b,1),component(b,3));
                    const auto bz=ctx.OpFDiv(f32,component(b,2),component(b,3));
                    // Sum the polygon's projected cross products. Unlike a
                    // first-three-corners plane, this also handles collinear
                    // corners introduced by homogeneous clipping.
                    ctx.OpStore(slope_x,ctx.OpFAdd(f32,ctx.OpLoad(f32,slope_x),
                        ctx.OpFSub(f32,ctx.OpFMul(f32,ay,bz),ctx.OpFMul(f32,az,by))));
                    ctx.OpStore(slope_y,ctx.OpFAdd(f32,ctx.OpLoad(f32,slope_y),
                        ctx.OpFSub(f32,ctx.OpFMul(f32,az,bx),ctx.OpFMul(f32,ax,bz))));
                    const auto window_z=ctx.OpFAdd(f32,depth_min,ctx.OpFMul(f32,depth_scale,
                        ctx.OpSelect(f32,flip_depth,ctx.OpFSub(f32,fone,az),az)));
                    ctx.OpStore(max_depth,ctx.OpFMax(f32,ctx.OpLoad(f32,max_depth),ctx.OpFAbs(f32,window_z)));
                }
            });
            const auto flips=ctx.OpLoad(u32_type,ctx.OpAccessChain(ctx.TypePointer(spv::StorageClass::Uniform,u32_type),runtime,ci(9)));
            const auto selected_flips=ctx.OpShiftRightLogical(u32_type,flips,ctx.OpUMin(u32_type,viewport,ci(15)));
            const auto flip=ctx.OpINotEqual(b1,ctx.OpBitwiseAnd(u32_type,selected_flips,one),zero);
            const auto area=ctx.OpLoad(f32,area_sum);
            Id polygon_bias{};
            if(config.depth_bias_enabled){
                const auto scale=ctx.OpFMul(f32,depth_scale,ctx.Constant(f32,2.f));
                const auto projected_slope=[&](Id sum,u32 axis){return ctx.OpFAbs(f32,ctx.OpFDiv(f32,
                    ctx.OpFMul(f32,ctx.OpLoad(f32,sum),scale),ctx.OpFMul(f32,area,viewport_value(10,axis))));};
                const auto slope=ctx.OpSelect(f32,ctx.OpFOrdEqual(b1,area,fzero),fzero,
                    ctx.OpFMax(f32,projected_slope(slope_x,0),projected_slope(slope_y,1)));
                Id unit=ctx.Constant(f32,1.f/65535.f);
                if(config.float_depth){
                    const auto exponent=ctx.OpBitwiseAnd(u32_type,
                        ctx.OpShiftRightLogical(u32_type,ctx.OpBitcast(u32_type,ctx.OpLoad(f32,max_depth)),ci(23)),ci(255));
                    const auto s32_type=ctx.TypeSInt(32);
                    const auto power=ctx.OpISub(s32_type,ctx.OpBitcast(s32_type,ctx.OpUMax(u32_type,exponent,one)),
                        ctx.Constant(s32_type,150));
                    unit=ctx.OpLdexp(f32,fone,power);
                }
                polygon_bias=ctx.OpFAdd(f32,ctx.OpFMul(f32,unit,ctx.Constant(f32,config.depth_bias)),
                    ctx.OpFMul(f32,slope,ctx.Constant(f32,config.slope_depth_bias)));
                if(config.depth_bias_clamp>0)polygon_bias=ctx.OpFMin(f32,polygon_bias,ctx.Constant(f32,config.depth_bias_clamp));
                else if(config.depth_bias_clamp<0)polygon_bias=ctx.OpFMax(f32,polygon_bias,ctx.Constant(f32,config.depth_bias_clamp));
            }
            const auto native_area=ctx.OpSelect(f32,flip,ctx.OpFNegate(f32,area),area);
            const auto front=config.front_ccw?ctx.OpFOrdGreaterThan(b1,native_area,fzero):ctx.OpFOrdLessThan(b1,native_area,fzero);
            auto keep=ctx.ConstantTrue(b1);
            if(config.cull_front)keep=ctx.OpLogicalAnd(b1,keep,ctx.OpLogicalNot(b1,front));
            if(config.cull_back)keep=ctx.OpLogicalAnd(b1,keep,front);
            selection(keep,[&]{
              const auto first=instances>1?ctx.OpIMul(u32_type,ctx.OpLoad(u32_type,invocation),ci(points_per_instance)):zero;
              const auto end=ctx.OpUMin(u32_type,n,ctx.OpIAdd(u32_type,first,ci(points_per_instance)));
              loop(end,[&](Id index){
                std::vector<Shader::Backend::SPIRV::PointOutput> outputs;
                std::array<std::array<Id,4>,32> packed;
                for(auto& channels:packed)channels.fill(ctx.OpUndef(f32));
                for(u32 field=0;field<varyings.size();++field){const auto& varying=varyings[field];
                    const auto value=load(clipped,index,field+1);u32 target=varying.offset;
                    for(u32 channel=0;channel<4;++channel)if(varying.mask&(1u<<channel))
                        packed[varying.location][target++]=channel<varying.width?
                            component(value,channel):ctx.OpUndef(f32);
                }
                for(u32 location=0;location<32;++location){const auto& output=varying_outputs[location];
                    if(!output.width)continue;
                    outputs.push_back({output.type,output.variable,output.width==1?packed[location][0]:
                        ctx.OpCompositeConstruct(output.type,std::span(packed[location].data(),output.width))});
                }
                outputs.push_back({u32_type,output_primitive,ctx.OpLoad(u32_type,primitive)});
                if(config.emulated_clip)outputs.push_back({vec4,output_clips,load(clipped,index,clip_field)});
                if(routed_viewport)outputs.push_back({u32_type,output_viewport,viewport});
                if(routed_layer)outputs.push_back({u32_type,output_layer,ctx.OpCompositeExtract(u32_type,misc,1u)});
                const auto size=sized?component(load(clipped,index,size_field),0):fone;
                const auto p=load(clipped,index,0);
                Shader::Backend::SPIRV::EmitPointStrip(ctx,runtime,p,size,viewport,config.clip_depth,
                    output_position,outputs,{},ctx.OpLogicalNot(b1,front),polygon_bias,
                    config.depth_bias_enabled||!config.clip_depth);
              },first);
            });
        });
    });
    ctx.OpReturn();ctx.OpFunctionEnd();return ctx.Assemble();
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
