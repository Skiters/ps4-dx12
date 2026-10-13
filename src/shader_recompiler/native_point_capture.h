// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <array>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace Shader::NativePointCapture {
inline constexpr u32 DescriptorSet=33,Binding=0;
inline constexpr u32 ValidWord=0,PositionWord=1,SizeWord=5,LayerWord=6,ViewportWord=7,
    ClipWord=8,CullWord=16,ParameterWord=24,RecordWords=152;
struct Record {
    // Zero is an unfilled slot; otherwise this is original PrimitiveId + 1.
    // The tag preserves primitive IDs across replay and instanced segments.
    u32 valid{};
    std::array<u32,4> position;
    u32 size,layer,viewport;
    std::array<u32,8> clip,cull;
    std::array<std::array<u32,4>,32> parameters;
};
inline constexpr u64 RecordBytes=RecordWords*sizeof(u32);
class Required : public std::runtime_error {public:using std::runtime_error::runtime_error;};
static_assert(std::is_standard_layout_v<Record>&&std::is_trivially_copyable_v<Record>);
static_assert(sizeof(Record)==RecordBytes&&RecordBytes%16==0);
static_assert(offsetof(Record,position)==PositionWord*4&&offsetof(Record,size)==SizeWord*4);
static_assert(offsetof(Record,layer)==LayerWord*4&&offsetof(Record,viewport)==ViewportWord*4);
static_assert(offsetof(Record,clip)==ClipWord*4&&offsetof(Record,cull)==CullWord*4);
static_assert(offsetof(Record,parameters)==ParameterWord*4);
// Slots are fixed by original primitive/invocation/emission order. Missing
// emissions leave invalid slots; no unordered global atomic append is used.
struct Plan {
    u32 primitive_bound,invocations,emissions,instances;
    u64 records,bytes;
    static constexpr Plan Make(u32 primitives,u32 calls,u32 vertices,u32 draws){
        if(!calls||!vertices)throw std::invalid_argument("Invalid native point capture dimensions");
        // DXIL RawBufferStore takes a 32-bit byte offset after SPIR-V's word
        // index is multiplied by four; bounding only words permits wraparound.
        constexpr u64 maximum=std::numeric_limits<u32>::max()/RecordBytes;
        u64 records=draws;
        for(const u32 factor:std::array{primitives,calls,vertices}){
            if(factor&&records>maximum/factor)
                throw std::overflow_error("Native point capture exceeds 32-bit shader byte addressing");
            records*=factor;
        }
        return {primitives,calls,vertices,draws,records,records*RecordBytes};
    }
    constexpr u32 Slot(u32 instance,u32 primitive,u32 invocation,u32 emission)const{
        if(instance>=instances||primitive>=primitive_bound||invocation>=invocations||emission>=emissions)
            throw std::out_of_range("Native point capture ordinal exceeds its reserved range");
        return u32(((u64(instance)*primitive_bound+primitive)*invocations+invocation)*emissions+emission);
    }
};
}
