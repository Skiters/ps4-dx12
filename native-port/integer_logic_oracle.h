// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

namespace NativeDX12::Tests {
struct IntegerLogicCase {
    const char* name;
    unsigned format, bytes, components, width;
    std::array<std::uint32_t,4> unsigned_source, signed_source;
};

// Literal guest integer encodings for exports UINT{5,10,3,1} and
// SINT{-5,-10,3,-1}. Every value is representable in its original format.
// No renderer integer clamp or float packing function constructs the oracle.
inline constexpr std::array<IntegerLogicCase,10> IntegerLogicCases{{
    {"r8",1,1,1,8,{5,0,0,0},{0xfb,0,0,0}},
    {"r16",2,2,1,16,{5,0,0,0},{0xfffb,0,0,0}},
    {"rg8",3,2,2,8,{0x0a05,0,0,0},{0xf6fb,0,0,0}},
    {"r32",4,4,1,32,{5,0,0,0},{0xfffffffb,0,0,0}},
    {"rg16",5,4,2,16,{0x000a0005,0,0,0},{0xfff6fffb,0,0,0}},
    {"rgb10a2",9,4,4,10,{0x40302805,0,0,0},{0xc03fdbfb,0,0,0}},
    {"rgba8",10,4,4,8,{0x01030a05,0,0,0},{0xff03f6fb,0,0,0}},
    {"rg32",11,8,2,32,{5,10,0,0},{0xfffffffb,0xfffffff6,0,0}},
    {"rgba16",12,8,4,16,{0x000a0005,0x00010003,0,0},{0xfff6fffb,0xffff0003,0,0}},
    {"rgba32",14,16,4,32,{5,10,3,1},{0xfffffffb,0xfffffff6,3,0xffffffff}},
}};
inline constexpr std::array<std::uint32_t,4> IntegerLogicBefore{
    0x92abcdef,0x87654321,0x7fc12345,0x80000000};

inline std::uint32_t IntegerLogicMask(const IntegerLogicCase& shape,unsigned channels,unsigned word) {
    std::uint64_t mask{};
    for(unsigned channel=0;channel<shape.components;++channel){
        const unsigned bit=channel*shape.width;
        if(!(channels&(1u<<channel))||bit/32!=word)continue;
        const unsigned width=shape.format==9&&channel==3?2:shape.width;
        mask|=((std::uint64_t{1}<<width)-1)<<(bit%32);
    }
    return std::uint32_t(mask);
}
inline std::uint32_t IntegerLogicExpected(const IntegerLogicCase& shape,bool sint,unsigned channels,
                                         unsigned operation,unsigned word,std::uint32_t before) {
    const auto source=(sint?shape.signed_source:shape.unsigned_source)[word];
    const auto mask=IntegerLogicMask(shape,channels,word);
    const auto value=operation==0?source^before:operation==1?source&~before:
        operation==2?~before:before;
    return (before&~mask)|(value&mask);
}
inline void CheckIntegerLogicOracle() {
    for(const auto& shape:IntegerLogicCases)for(bool sint:{false,true})
    for(unsigned channels=0;channels<16;++channels)for(unsigned word=0;word<(shape.bytes+3)/4;++word){
        const auto before=IntegerLogicBefore[word];
        const auto first=IntegerLogicExpected(shape,sint,channels,0,word,before);
        if(IntegerLogicExpected(shape,sint,channels,0,word,first)!=before||
           IntegerLogicExpected(shape,sint,channels,3,word,before)!=before||
           IntegerLogicExpected(shape,sint,0,2,word,before)!=before||
           (IntegerLogicExpected(shape,sint,channels,2,word,before)^before)!=IntegerLogicMask(shape,channels,word))
            throw std::runtime_error("Integer logical oracle lost involution/noop/mask negative controls");
    }
}
} // namespace NativeDX12::Tests
