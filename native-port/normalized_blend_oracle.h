// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace NativeDX12::Tests {

struct NormalizedBlendBoundary {
    std::array<float, 4> source;
    std::array<float, 4> constants;
    std::array<int, 4> unorm_before;
    std::array<int, 4> snorm_before;
    std::array<std::array<int, 4>, 3> unorm_after;
    std::array<std::array<int, 4>, 3> snorm_after;
    bool color_factors;
};

// Literal code-space results, independent of the shader emitter/converter.
// All written finite results avoid half-integer rounding boundaries. Case2's
// SNORM source2 and ONE_MINUS_SRC_COLOR=-1 also distinguish selecting factors
// from the original export from selecting them from an already clamped export.
inline constexpr std::array<NormalizedBlendBoundary, 3> NormalizedBlendBoundaries{{
    {{2.f, -2.f, .5f, 2.f}, {.25f, .5f, .75f, .5f},
     {65, 96, 160, 128}, {-63, -31, 34, 64},
     {{{96, 48, 176, 255}, {176, 48, 104, 255}, {160, 48, 144, 255}}},
     {{{0, -79, 65, 127}, {16, -79, 40, 127}, {32, -79, 49, 127}}}, false},
    {{-2.f, 2.f, .25f, -2.f}, {2.f, -2.f, .5f, 2.f},
     {65, 96, 160, 128}, {-63, -31, 34, 64},
     {{{65, 96, 192, 0}, {0, 96, 80, 0}, {0, 255, 64, 0}}},
     {{{-127, -127, 50, -127}, {127, -127, -15, -127}, {-64, 127, -2, -127}}}, false},
    {{2.f, -2.f, .25f, 2.f}, {.25f, .5f, .75f, .5f},
     {64, 96, 160, 128}, {-64, -31, 34, 64},
     {{{96, 48, 128, 255}, {32, 48, 128, 255}, {255, 0, 64, 255}}},
     {{{0, -79, 41, 127}, {-127, -127, 41, 127}, {127, -96, -2, 127}}}, true},
}};

inline void CheckNormalizedBlendOracle() {
    unsigned source_negative{}, factor_negative{}, selection_negative{};
    for (const auto& point : NormalizedBlendBoundaries) {
        for (const bool snorm : {false, true}) {
            const double scale = snorm ? 127. : 255.;
            const auto clamp = [=](double value) { return std::clamp(value, snorm ? -1. : 0., 1.); };
            const auto& before = snorm ? point.snorm_before : point.unorm_before;
            const auto& after = snorm ? point.snorm_after : point.unorm_after;
            for (unsigned target = 0; target < 3; ++target) {
                for (unsigned channel = 0; channel < 4; ++channel) {
                    const auto evaluate = [&](bool clamp_source, bool clamp_factor, bool wrong_selection) {
                        const double raw = point.source[channel];
                        const double selected = wrong_selection ? clamp(raw) : raw;
                        const double alpha = wrong_selection ? clamp(point.source[3]) : point.source[3];
                        double src_factor = channel == 3 ? 1. : target == 0 ? point.constants[channel] :
                            target == 1 ? (point.color_factors ? 1. - selected : 1. - point.constants[3]) :
                            point.color_factors ? alpha : point.constants[3];
                        double dst_factor = channel == 3 ? 0. : target == 0 ? point.constants[3] :
                            target == 1 ? (point.color_factors ? point.constants[3] : 1. - point.constants[channel]) :
                            point.color_factors ? 1. - alpha : 1. - point.constants[3];
                        if (clamp_factor) { src_factor = clamp(src_factor); dst_factor = clamp(dst_factor); }
                        const double result = (clamp_source ? clamp(raw) : raw) * src_factor +
                            (double(before[channel]) / scale) * dst_factor;
                        // No ties in the literal cases: independent nearest integer suffices.
                        return int(std::round(clamp(result) * scale));
                    };
                    if (evaluate(true, true, false) != after[target][channel])
                        throw std::runtime_error("Normalized blend literal oracle disagrees with the API equation");
                    source_negative += evaluate(false, true, false) != after[target][channel];
                    factor_negative += evaluate(true, false, false) != after[target][channel];
                    selection_negative += evaluate(true, true, true) != after[target][channel];
                }
            }
        }
    }
    if (!source_negative || !factor_negative || !selection_negative)
        throw std::runtime_error("Normalized blend oracle lost a source/factor/selection negative control");
}

} // namespace NativeDX12::Tests
