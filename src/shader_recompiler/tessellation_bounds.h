// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <optional>
namespace Shader {
// A source-allocation proof, never a replacement for guest tessellation levels.
// Missing, conditional or dynamic factor writes retain the hardware maximum.
struct TessellationFactorBounds {
    u8 written{},dynamic{};
    bool unconditional{};
    float maximum{};
    void Record(u32 factor,std::optional<float> value){
        if(factor>=6)return;
        written|=u8(1u<<factor);
        if(!value||!std::isfinite(*value)){dynamic|=u8(1u<<factor);return;}
        maximum=std::max(maximum,*value);
    }
    u32 SegmentBound()const{
        if(!unconditional||written!=63||dynamic||!std::isfinite(maximum)||maximum<0||maximum>=32)
            return 64;
        // Covers integer, fractional-even/odd and power-of-two partitioning.
        // The strict next power of two also covers an odd segment above an
        // exactly even guest factor. No rounding mode can exceed this bound.
        return std::bit_ceil(std::max(1u,u32(std::ceil(maximum)))+1);
    }
};
}
