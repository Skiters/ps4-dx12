// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shader_recompiler/info.h"
#include "shader_recompiler/runtime_info.h"
#include "shader_recompiler/profile.h"
#include "shader_recompiler/native_point_capture.h"
#include <span>
#include <stdexcept>
namespace NativeDX12 {
inline u32 NativePointMiscLocation(const Shader::Info& vertex,bool emulated_clip,u32 live_parameters=~0u,
    std::span<const Shader::NativePointOutput> transport={}){
    // Mesa uses a 64-bit varying mask with generic slots starting at 32.
    // Keep this private uint4 inside the 32 generic locations.
    if(!transport.empty()){
        if(transport.size()!=32)throw std::runtime_error("Invalid native point transport count");
        u32 occupied=u32(emulated_clip);
        for(const auto& output:transport)if(output.mask){
            const u32 location=output.location+u32(emulated_clip);
            if(location>=32)throw std::runtime_error("Native emulated clip interface exceeds generic varying slots");
            occupied|=1u<<location;
        }
        for(u32 location=0;location<32;++location)if(!(occupied&(1u<<location)))return location;
        throw Shader::NativePointCapture::Required("Native point interface needs a free generic varying slot");
    }
    if(emulated_clip&&(live_parameters&(1u<<31))&&vertex.stores.GetAny(Shader::IR::Attribute::Param31))
        throw std::runtime_error("Native emulated clip interface exceeds generic varying slots");
    for(u32 location=0;location<32;++location){
        if(emulated_clip&&location==0)continue;
        const u32 parameter=location-u32(emulated_clip);
        if(!(live_parameters&(1u<<parameter))||!vertex.stores.GetAny(Shader::IR::Attribute::Param0+parameter))return location;
    }
    throw Shader::NativePointCapture::Required("Native point interface needs a free generic varying slot");
}
inline std::array<Shader::NativePointOutput,32> NativePolygonPointOutputs(
    const Shader::Info& vertex,u32 live_parameters){
    std::array<Shader::NativePointOutput,32> outputs{};
    std::array<u32,32> widths{};
    for(u32 parameter=0;parameter<32;++parameter){
        if(!(live_parameters&(1u<<parameter)))continue;
        const u32 mask=vertex.stores.flags[Shader::Info::AttributeFlags::Index(
            Shader::IR::Attribute::Param0+parameter)]&15u;
        const u32 width=std::popcount(mask);
        if(!width)continue;
        // Keep each parameter in one row, packing only its exported lanes.
        // Consumer reconstruction restores logical xyzw before interpolation.
        for(u32 row=0;row<32;++row)if(widths[row]+width<=4){
            outputs[parameter]={u8(parameter),u8(mask),u8(row),u8(widths[row])};
            widths[row]+=width;
            break;
        }
    }
    return outputs;
}
inline u32 NativePolygonPointParameterLocation(const Shader::Info& vertex,u32 parameter,u32 live_parameters){
    if(parameter>=32)throw std::out_of_range("Invalid polygon parameter location");
    return NativePolygonPointOutputs(vertex,live_parameters)[parameter].location;
}
inline u32 NativePolygonPointMiscLocation(const Shader::Info& vertex,bool emulated_clip,u32 live_parameters=~0u,
    bool compact_parameters=false){
    if(compact_parameters){
        u32 location=u32(emulated_clip);
        for(const auto& output:NativePolygonPointOutputs(vertex,live_parameters))if(output.mask)
            location=std::max(location,u32(output.location)+1+u32(emulated_clip));
        if(location<32)return location;
        throw std::runtime_error("Polygon point payload needs a buffered varying transport");
    }
    try{return NativePointMiscLocation(vertex,emulated_clip,live_parameters);}
    catch(const Shader::NativePointCapture::Required&){
        // Ordinary point capture consumes an original point-output GS. It
        // cannot be substituted for this polygon producer when slots run out.
        throw std::runtime_error("Polygon point payload needs a buffered varying transport");
    }
}
inline u32 NativePolygonPointLiveParameters(const Shader::Info* fragment,const Shader::HwFragmentRuntimeInfo& fs){
    if(!fragment)return 0;
    if(fs.num_inputs<u32(fs.clip_distance_emulation)||fs.num_inputs-u32(fs.clip_distance_emulation)>fs.inputs.size())
        throw std::invalid_argument("Invalid polygon fragment input count");
    u32 mask{};
    for(u32 i=0;i<fs.num_inputs-u32(fs.clip_distance_emulation);++i){
        const auto& input=fs.inputs[i];
        if(!fragment->loads.GetAny(Shader::IR::Attribute::Param0+i)||input.IsDefault())continue;
        if(input.param_index>=32)throw std::invalid_argument("Invalid polygon fragment parameter");
        mask|=1u<<input.param_index;
    }
    return mask;
}
std::vector<u32> EmitGuestPointGeometry(const Shader::Info&,bool clip_depth,bool emulated_clip=false,u32 live_parameters=~0u,
    std::span<const Shader::NativePointOutput> transport={});
struct GuestPolygonPointConfig {
    bool cull_front{},cull_back{},front_ccw{},last_provoking{},emulated_clip{};
    bool clip_depth{true};
    u32 first_provoking{};
    bool depth_bias_enabled{},float_depth{true};
    float depth_bias{},slope_depth_bias{},depth_bias_clamp{};
    bool compact_parameters{};
};
// Clip the completed triangle, then expand its surviving polygon vertices.
// No guest shader is evaluated again for the generated clipping vertices.
std::vector<u32> EmitGuestPolygonPoints(const Shader::Info&,const Shader::Info*,
    const Shader::HwFragmentRuntimeInfo&,const GuestPolygonPointConfig&);
// Replay host records, never guest shader code. Only the flat record ID crosses
// the hardware varying interface; original PS attributes are fetched from SRV.
std::vector<u32> EmitNativePointReplayVertex();
// Ordered uint4 SO chunks of the original DS/ES vertex payload plus patch ID.
// This auxiliary GS replaces the original GS only in the first capture pass.
std::vector<u32> EmitNativePrimitiveStream(const Shader::HwGeometryRuntimeInfo&,const Shader::Info* input_info=nullptr);
std::vector<u32> EmitNativePointReplayGeometry(const Shader::Info&,
    const Shader::HwGeometryRuntimeInfo&,const Shader::Profile&);
}
