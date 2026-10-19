// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"

namespace Shader::NativeOrderedColor {
inline constexpr u32 DescriptorSet = 35;
inline constexpr u32 Binding = 0;
inline constexpr u32 Targets = 8;
inline constexpr u32 MaxSamples = 8;
inline constexpr u32 Count = Targets * MaxSamples;
inline constexpr u32 Register(u32 target, u32 sample = 0) {
    return Binding + target * MaxSamples + sample;
}
}
