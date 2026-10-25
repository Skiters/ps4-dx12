// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <optional>
namespace Shader {
// A source-allocation proof, never a replacement for guest tessellation levels.
// Missing or dynamic factor writes retain the hardware maximum. A conditional
// writer is usable only when every terminating path writes the required factors.
struct TessellationFactorBounds {
    u8 written{},dynamic{};
    u8 guaranteed{};
    float maximum{};
    void Record(u32 factor,std::optional<float> value){
        if(factor>=6)return;
        written|=u8(1u<<factor);
        if(!value||!std::isfinite(*value)){dynamic|=u8(1u<<factor);return;}
        maximum=std::max(maximum,*value);
    }
    u32 SegmentBound(u8 required=63)const{
        // Isolines consume outer0/1; triangles consume outer0/1/2 and inner0.
        // Unused domain factors need not be written by the original HS.
        if(!required||(required&~63u)||(guaranteed&required)!=required||(written&required)!=required||(dynamic&required)||
           !std::isfinite(maximum)||maximum<0||maximum>=32)
            return 64;
        // Covers integer, fractional-even/odd and power-of-two partitioning.
        // The strict next power of two also covers an odd segment above an
        // exactly even guest factor. No rounding mode can exceed this bound.
        return std::bit_ceil(std::max(1u,u32(std::ceil(maximum)))+1);
    }
};
}
