#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"

namespace NativeDX12 {
struct Capabilities {
    D3D_SHADER_MODEL shader_model = D3D_SHADER_MODEL_5_1;
    D3D12_RESOURCE_BINDING_TIER resource_binding_tier{};
    bool barycentrics{};
    bool wave_operations{};
    bool native_16bit{};
    bool native_float64{};
    bool pixel_shader_stencil_ref{};
    bool buffer_int64_atomics{};
    UINT min_wave_lanes{},max_wave_lanes{};
    static Capabilities Query(ID3D12Device* device) {
        if (!device) throw std::invalid_argument("null capability device");
        Capabilities result;
        // Newer shader-model queries can be rejected by older runtimes.
        for (auto model : {D3D_SHADER_MODEL_6_6, D3D_SHADER_MODEL_6_2,
                           D3D_SHADER_MODEL_6_1, D3D_SHADER_MODEL_6_0}) {
            D3D12_FEATURE_DATA_SHADER_MODEL data{model};
            if (SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &data, sizeof(data))))) {
                result.shader_model = data.HighestShaderModel;
                break;
            }
        }
        D3D12_FEATURE_DATA_D3D12_OPTIONS base{};
        Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &base, sizeof(base))));
        result.resource_binding_tier = base.ResourceBindingTier;
        result.native_float64 = base.DoublePrecisionFloatShaderOps;
        result.pixel_shader_stencil_ref = base.PSSpecifiedStencilRefSupported;
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 waves{};
        if (SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &waves, sizeof(waves))))) {
            result.wave_operations = waves.WaveOps;
            result.min_wave_lanes=waves.WaveLaneCountMin;result.max_wave_lanes=waves.WaveLaneCountMax;
        }
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 interpolation{};
        D3D12_FEATURE_DATA_D3D12_OPTIONS11 atomics{};
        if (SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS11, &atomics, sizeof(atomics)))))
            result.buffer_int64_atomics = waves.Int64ShaderOps && result.shader_model >= D3D_SHADER_MODEL_6_6 &&
                atomics.AtomicInt64OnDescriptorHeapResourceSupported;
        if (SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &interpolation, sizeof(interpolation)))))
            result.barycentrics = interpolation.BarycentricsSupported && result.shader_model >= D3D_SHADER_MODEL_6_1;
        D3D12_FEATURE_DATA_D3D12_OPTIONS4 precision{};
        if (SUCCEEDED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &precision, sizeof(precision)))))
            result.native_16bit = precision.Native16BitShaderOpsSupported;
        return result;
    }
};
}
