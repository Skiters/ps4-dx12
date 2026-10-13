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
std::vector<u32> EmitGuestPointGeometry(const Shader::Info&,bool clip_depth,bool emulated_clip=false,u32 live_parameters=~0u,
    std::span<const Shader::NativePointOutput> transport={});
// Replay host records, never guest shader code. Only the flat record ID crosses
// the hardware varying interface; original PS attributes are fetched from SRV.
std::vector<u32> EmitNativePointReplayVertex();
std::vector<u32> EmitNativePointReplayGeometry(const Shader::Info&,
    const Shader::HwGeometryRuntimeInfo&,const Shader::Profile&);
}
