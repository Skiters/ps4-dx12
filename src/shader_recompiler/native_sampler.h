// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include "video_core/amdgpu/resource.h"

namespace Shader::NativeSampler {
inline constexpr float MinLodBias = -16.f;
inline constexpr float MaxLodBias = 15.99f;

inline float ClampLodBias(float value) {
    return std::clamp(value, MinLodBias, MaxLodBias);
}

inline bool RequiresManualAnisotropy(const AmdGpu::Sampler& sampler) {
    const auto linear = [](AmdGpu::Filter filter) {
        return filter == AmdGpu::Filter::Bilinear || filter == AmdGpu::Filter::AnisoLinear;
    };
    const bool anisotropic = AmdGpu::IsAnisoFilter(sampler.xy_mag_filter.Value()) ||
                             AmdGpu::IsAnisoFilter(sampler.xy_min_filter.Value());
    return anisotropic && (!linear(sampler.xy_mag_filter.Value()) ||
        !linear(sampler.xy_min_filter.Value()) || sampler.mip_filter != AmdGpu::MipFilter::Linear);
}

inline bool RequiresManualAddressing(const AmdGpu::Sampler& sampler, AmdGpu::ImageType view) {
    const u32 axes = view == AmdGpu::ImageType::Color1D ||
        view == AmdGpu::ImageType::Color1DArray ? 1 : view == AmdGpu::ImageType::Color3D ? 3 : 2;
    const std::array clamps{sampler.clamp_x.Value(), sampler.clamp_y.Value(), sampler.clamp_z.Value()};
    for (u32 axis = 0; axis < axes; ++axis)
        if (clamps[axis] == AmdGpu::ClampMode::ClampHalfBorder ||
            clamps[axis] == AmdGpu::ClampMode::MirrorOnceHalfBorder ||
            clamps[axis] == AmdGpu::ClampMode::MirrorOnceBorder)
            return true;
    return false;
}
}
