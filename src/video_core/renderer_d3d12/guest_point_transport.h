// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shader_recompiler/info.h"
#include "shader_recompiler/runtime_info.h"
#include <stdexcept>
namespace NativeDX12 {
struct NativePointTransport {
    std::array<Shader::NativePointOutput,32> outputs{};
    std::array<u8,32> locations{},masks{},offsets{},widths{};
};
template<class GetInput>
NativePointTransport BuildNativePointTransport(const Shader::Info& fragment,u32 count,GetInput get_input){
    if(count>32)throw std::runtime_error("Invalid native point fragment input count");
    NativePointTransport result;
    std::array<Shader::Qualifier,32> primary{},auxiliary{};
    std::array<u8,32> groups{};
    u32 group_count{};
    for(u32 input=0;input<count;++input){
        const auto control=get_input(input);
        const auto attribute=Shader::IR::Attribute::Param0+input;
        if(control.IsDefault()||!fragment.loads.GetAny(attribute))continue;
        if(control.param_index>=32)throw std::runtime_error("Invalid native point parameter source");
        const auto interpolation=fragment.fs_interpolation[input];
        const u8 mask=interpolation.primary==Shader::Qualifier::PerVertex?15:
            fragment.loads.flags[Shader::Info::AttributeFlags::Index(attribute)];
        u32 location=32;
        for(u32 candidate=0;candidate<32;++candidate){
            const auto& output=result.outputs[candidate];
            if(output.mask&&output.source==control.param_index&&primary[candidate]==interpolation.primary&&
               auxiliary[candidate]==interpolation.auxiliary){location=candidate;break;}
        }
        if(location==32){
            location=group_count++;
            result.outputs[location].source=control.param_index;
            primary[location]=interpolation.primary;auxiliary[location]=interpolation.auxiliary;
        }
        result.outputs[location].mask|=mask;
        groups[input]=u8(location);
    }
    // Inputs with identical interpolation may share one host float4 even
    // when they address different guest parameters. Keep every source group
    // contiguous so original component reads map to an offset plus rank.
    std::array<u8,32> occupied{};
    std::array<Shader::Qualifier,32> host_primary{},host_auxiliary{};
    for(u32 group=0;group<group_count;++group){
        auto& output=result.outputs[group];
        const auto components=u32(std::popcount(u32(output.mask)));
        u32 location=32;
        for(u32 candidate=0;candidate<32;++candidate)
            if(occupied[candidate]&&occupied[candidate]+components<=4&&
               host_primary[candidate]==primary[group]&&host_auxiliary[candidate]==auxiliary[group]){
                location=candidate;break;
            }
        if(location==32)for(u32 candidate=0;candidate<32;++candidate)
            if(!occupied[candidate]){location=candidate;break;}
        if(location==32)throw std::runtime_error("Native point interpolation transport has no varying slot");
        output.location=u8(location);output.component=occupied[location];
        occupied[location]+=u8(components);
        host_primary[location]=primary[group];host_auxiliary[location]=auxiliary[group];
    }
    for(u32 input=0;input<count;++input){
        const auto control=get_input(input);
        if(!control.IsDefault()&&fragment.loads.GetAny(Shader::IR::Attribute::Param0+input)){
            const auto& output=result.outputs[groups[input]];
            result.locations[input]=output.location;result.masks[input]=output.mask;
            result.offsets[input]=output.component;result.widths[input]=occupied[output.location];
        }
    }
    return result;
}
}
