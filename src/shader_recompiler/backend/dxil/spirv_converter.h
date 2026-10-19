// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace Shader::Backend::DX12 {
enum class SpirvStage { Vertex, TessellationControl, TessellationEvaluation, Geometry, Pixel, Compute };
enum class SystemValueMode { Zero, Native, RuntimeData };
inline constexpr uint64_t NativeFeatureNvShaderClock=1;
inline constexpr uint64_t NativeFeatureAmdShaderClock=2;
inline constexpr uint64_t NativeFeatureComputeWave64=4;
uint64_t ConverterNativeFeatures(const std::wstring& converter_dll);
struct SpirvSpecialization { uint32_t id{}; uint64_t raw_bits{}; bool defined_on_module{}; };
struct SpirvConvertOptions {
    SpirvStage stage = SpirvStage::Pixel;
    std::string entry_point = "main";
    unsigned shader_model_minor = 2; // SM6.2, supported by the current converter.
    unsigned validator_minor = 6; // DXIL validator1.6 compatibility.
    uint32_t runtime_cbv_space = 31, runtime_cbv_register = 0;
    uint32_t push_cbv_space = 30, push_cbv_register = 0;
    SystemValueMode draw_sysvals = SystemValueMode::RuntimeData;
    SystemValueMode workgroup_id = SystemValueMode::Native;
    bool declared_read_only_images_as_srvs = true;
    bool inferred_read_only_images_as_srvs = false;
    bool force_sample_rate_shading = false;
    bool lower_view_index = false;
    bool lower_view_index_to_rt_layer = false;
    // Native positive-height viewports have the opposite Y direction to the
    // original guest intermediate. Select per-viewport Y/Z flips at draw time.
    bool runtime_viewport_flips = false;
    // Guest stages may alias a read-only buffer with another stage's writable
    // buffer. Use one coherent native UAV access class for storage-buffer
    // views, preserving writes observed through another guest sharp.
    bool native_storage_buffer_uavs = false;
    // Marks one contiguous Vulkan UAV binding range as DXIL rasterizer-ordered
    // resources. Zero count disables the feature. The caller must still bind
    // matching D3D12 UAV descriptors and use this only for pixel shaders.
    uint32_t raster_ordered_uav_space = 0;
    uint32_t raster_ordered_uav_register = 0;
    uint32_t raster_ordered_uav_count = 0;
    uint32_t compute_wave_size = 0; // Zero preserves the original intermediate.
    uint32_t native_shader_clock_backend=0;
    std::vector<SpirvSpecialization> specializations;
};
struct SpirvConvertResult {
    bool success{};
    bool converted{};
    bool validated{};
    bool requires_runtime_data{};
    bool needs_draw_sysvals{};
    uint64_t converter_version{};
    std::vector<uint8_t> bytecode; // Signed/validated DXIL only; empty on failure.
    std::string diagnostics;
};
// DLL directory must also contain compatible dxcompiler.dll and dxil.dll.
// SPIR-V here is an offline compiler intermediate; no Vulkan loader/device is used.
SpirvConvertResult ConvertSpirv(const std::wstring& converter_dll,
                              std::span<const uint32_t> words,
                              const SpirvConvertOptions& options);
struct LinkedSpirvStage {
    SpirvStage stage{};
    std::span<const uint32_t> words;
    std::string entry_point="main";
    std::vector<SpirvSpecialization> specializations;
};
struct LinkedSpirvResult {
    bool success{};
    uint64_t converter_version{};
    std::vector<SpirvConvertResult> stages; // Input order, committed only if all stages validate.
    std::string diagnostics;
};
// shared_options provides runtime/descriptor/SM configuration; its stage,
// entry_point and specializations are ignored in favor of each stage's fields.
// HS/DS are accepted only as a linked pair. Standalone ConvertSpirv guard remains.
LinkedSpirvResult ConvertLinkedStages(const std::wstring& converter_dll,
                                    std::span<const LinkedSpirvStage> stages,
                                    const SpirvConvertOptions& shared_options);
}
