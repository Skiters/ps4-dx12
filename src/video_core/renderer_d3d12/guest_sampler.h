// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "video_core/amdgpu/resource.h"
#include <algorithm>
namespace NativeDX12 {
inline D3D12_SAMPLER_DESC DescribeGuestSampler(const AmdGpu::Sampler& source,bool depth,
    const float* custom_border=nullptr) {
    if(!source.Valid())throw std::runtime_error("Invalid native guest sampler");
    auto address=[](AmdGpu::ClampMode value) {
        switch(value) {
        case AmdGpu::ClampMode::Wrap:return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case AmdGpu::ClampMode::Mirror:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case AmdGpu::ClampMode::ClampLastTexel:return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case AmdGpu::ClampMode::MirrorOnceLastTexel:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        case AmdGpu::ClampMode::ClampHalfBorder:
        case AmdGpu::ClampMode::ClampBorder:return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case AmdGpu::ClampMode::MirrorOnceHalfBorder:
        case AmdGpu::ClampMode::MirrorOnceBorder:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:throw std::runtime_error("Invalid native guest clamp mode");
        }
    };
    D3D12_SAMPLER_DESC result{};result.AddressU=address(source.clamp_x.Value());
    result.AddressV=address(source.clamp_y.Value());result.AddressW=address(source.clamp_z.Value());
    auto reduction=D3D12_FILTER_REDUCTION_TYPE_STANDARD;
    if(depth) {
        // GCN depth comparison is instruction-selected. The Vulkan reference
        // likewise creates a comparison sampler and does not apply the S#
        // min/max reduction mode to it.
        reduction=D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
    }else if(source.filter_mode.Value()==AmdGpu::FilterMode::Min)reduction=D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
    else if(source.filter_mode.Value()==AmdGpu::FilterMode::Max)reduction=D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
    auto linear=[](AmdGpu::Filter f){return f==AmdGpu::Filter::Bilinear||f==AmdGpu::Filter::AnisoLinear?D3D12_FILTER_TYPE_LINEAR:D3D12_FILTER_TYPE_POINT;};
    const bool aniso=AmdGpu::IsAnisoFilter(source.xy_min_filter.Value())||AmdGpu::IsAnisoFilter(source.xy_mag_filter.Value());
    result.Filter=aniso?D3D12_ENCODE_ANISOTROPIC_FILTER(reduction):D3D12_ENCODE_BASIC_FILTER(
        linear(source.xy_min_filter.Value()),linear(source.xy_mag_filter.Value()),
        source.mip_filter.Value()==AmdGpu::MipFilter::Linear?D3D12_FILTER_TYPE_LINEAR:D3D12_FILTER_TYPE_POINT,reduction);
    result.MaxAnisotropy=1u<<UINT(source.max_aniso.Value());result.ComparisonFunc=D3D12_COMPARISON_FUNC(UINT(source.depth_compare_func.Value())+1);
    result.MipLODBias=std::clamp(source.LodBias(),D3D12_MIP_LOD_BIAS_MIN,D3D12_MIP_LOD_BIAS_MAX);
    result.MinLOD=source.MinLod();result.MaxLOD=source.MaxLod();
    if(source.mip_filter.Value()==AmdGpu::MipFilter::None)result.MinLOD=result.MaxLOD=0;
    if(result.MinLOD>result.MaxLOD)throw std::runtime_error("Native guest sampler inverted LOD bounds");
    switch(source.border_color_type.Value()) {
    case AmdGpu::BorderColor::TransparentBlack:break;
    case AmdGpu::BorderColor::OpaqueBlack:result.BorderColor[3]=1;break;
    case AmdGpu::BorderColor::White:std::fill_n(result.BorderColor,4,1.0f);break;
    case AmdGpu::BorderColor::Custom:
        if(!custom_border)throw std::runtime_error("Native guest custom border table is unmapped");
        std::copy_n(custom_border,4,result.BorderColor);break;
    default:throw std::runtime_error("Invalid native guest border color");
    }
    return result;
}
}
