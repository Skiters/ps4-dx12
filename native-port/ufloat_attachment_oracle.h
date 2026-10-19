// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace NativeDX12::Tests {
struct UfloatBoundary {u32 input,uf11,uf10;};
inline constexpr std::array<UfloatBoundary,28> UfloatBoundaries{{
    UfloatBoundary{0,0,0},{0x80000000,0,0},{0xbf800000,0,0},
    {0x35000000,0,0},{0x35000001,1,0},{0x35800000,1,0},
    {0x35800001,1,1},{0x35c00000,2,1},{0x36000000,2,1},
    {0x36400000,3,2},{0x38780000,62,31},{0x387c0000,63,32},
    {0x387e0000,64,32},{0x38800000,64,32},
    {0x3f800000,0x3c0,0x1e0},{0x3f810000,0x3c0,0x1e0},
    {0x3f810001,0x3c1,0x1e0},{0x3f820000,0x3c1,0x1e0},
    {0x3f820001,0x3c1,0x1e1},{0x3f830000,0x3c2,0x1e1},
    {0x3f860000,0x3c3,0x1e2},{0x477c0000,0x7be,0x3df},
    {0x477dffff,0x7bf,0x3df},{0x477e0000,0x7bf,0x3e0},
    {0x477effff,0x7bf,0x3e0},{0x477f0000,0x7c0,0x3e0},
    {0x7f800000,0x7c0,0x3e0},{0xff800000,0,0}}};

// Independent oracle: enumerate representable finite numbers and choose the
// closest rational value, ties to an even code. No production bit-rounding
// algorithm or intermediate half conversion is used.
inline u32 NearestUfloat(float input,u32 mantissa){
    if(mantissa!=5&&mantissa!=6)throw std::invalid_argument("Unsupported unsigned float oracle width");
    if(std::isnan(input))throw std::invalid_argument("NaN payload is outside this exact-code oracle");
    if(input<=0)return 0;
    const u32 infinity=31u<<mantissa,last=infinity-1;
    const auto value=[&](u32 code){const auto exponent=code>>mantissa,fraction=code&((1u<<mantissa)-1);
        return exponent?std::ldexp(1.+std::ldexp(double(fraction),-int(mantissa)),int(exponent)-15):
            std::ldexp(double(fraction),-14-int(mantissa));};
    if(double(input)>=value(last)+std::ldexp(1.,14-int(mantissa)))return infinity;
    double best=std::numeric_limits<double>::infinity();u32 result{};
    for(u32 code=0;code<=last;++code){const double distance=std::abs(double(input)-value(code));
        if(distance<best||(distance==best&&!(code&1))){best=distance;result=code;}}
    return result;
}
inline void CheckUfloatBoundaryOracle(){
    for(const auto& point:UfloatBoundaries){const float input=std::bit_cast<float>(point.input);
        if(NearestUfloat(input,6)!=point.uf11||NearestUfloat(input,5)!=point.uf10)
            throw std::runtime_error("Independent unsigned float boundary code mismatch: "+std::to_string(point.input));}
    bool rejected{};try{NearestUfloat(std::numeric_limits<float>::quiet_NaN(),6);}
    catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Exact-code oracle accepted an unspecified NaN payload");
}
} // namespace NativeDX12::Tests
