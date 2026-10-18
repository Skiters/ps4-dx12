// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bitset>

#include "common/types.h"
#include "shader_recompiler/backend/bindings.h"
#include "shader_recompiler/frontend/fetch_shader.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/native_sampler.h"
#include "shader_recompiler/profile.h"

namespace Shader {

struct VsAttribSpecialization {
    u32 divisor{};
    AmdGpu::NumberClass num_class{};
    AmdGpu::CompMapping dst_select{};

    bool operator==(const VsAttribSpecialization&) const = default;
};

struct BufferSpecialization {
    u32 stride : 14;
    u32 is_formatted : 1;
    u32 swizzle_enable : 1;
    u32 data_format : 6;
    u32 num_format : 4;
    u32 index_stride : 2;
    u32 element_size : 2;
    AmdGpu::CompMapping dst_select{};
    AmdGpu::NumberConversion num_conversion{};

    bool operator==(const BufferSpecialization& other) const {
        return stride == other.stride && is_formatted == other.is_formatted &&
               swizzle_enable == other.swizzle_enable &&
               (!is_formatted ||
                (data_format == other.data_format && num_format == other.num_format &&
                 dst_select == other.dst_select && num_conversion == other.num_conversion)) &&
               (!swizzle_enable ||
                (index_stride == other.index_stride && element_size == other.element_size));
    }
};

struct ImageSpecialization {
    AmdGpu::ImageType type = AmdGpu::ImageType::Color2D;
    bool is_integer = false;
    bool is_storage = false;
    bool is_srgb = false;
    AmdGpu::CompMapping dst_select{};
    AmdGpu::NumberConversion num_conversion{};
    // FIXME any pipeline cache changes needed?
    u32 num_bindings = 0;
    u32 native_storage_mip_levels = 0;
    u32 native_resource_min_lod = 0;
    u32 native_number_format = 0;
    u32 native_packed_format = 0;
    bool native_srgb_read = false;
    bool native_slice_view = false;

    bool operator==(const ImageSpecialization&) const = default;
};

struct FMaskSpecialization {
    u32 width;
    u32 height;

    bool operator==(const FMaskSpecialization&) const = default;
};

struct SamplerSpecialization {
    u8 force_unnormalized : 1;
    u8 force_degamma : 1;
    u64 native_raw0 = 0;
    u64 native_raw1 = 0;

    bool operator==(const SamplerSpecialization&) const = default;
};

/**
 * Alongside runtime information, this structure also checks bound resources
 * for compatibility. Can be used as a key for storing shader permutations.
 * Is separate from runtime information, because resource layout can only be deduced
 * after the first compilation of a module.
 */
struct StageSpecialization {
    static constexpr size_t MaxStageResources = 128;

    const Info* info{};
    RuntimeInfo runtime_info{};
    std::bitset<MaxStageResources> bitset{};
    Gcn::FetchShaderData fetch_shader_data{};
    SmallVector<VsAttribSpecialization, 32> vs_attribs;
    SmallVector<BufferSpecialization, 16> buffers;
    SmallVector<ImageSpecialization, 16> images;
    SmallVector<u32, 16> native_image_aliases;
    SmallVector<u8, 16> native_external_image_aliases;
    SmallVector<FMaskSpecialization, 8> fmasks;
    SmallVector<SamplerSpecialization, 16> samplers;
    Backend::Bindings start{};

    StageSpecialization() = default;
    StageSpecialization(const Info& info_, RuntimeInfo runtime_info_, const Profile& profile_,
                        Backend::Bindings start_)
        : info{&info_}, runtime_info{runtime_info_}, start{start_} {
        if (info_.sw_stage == SwStage::Vertex && Gcn::ParseFetchShader(info_, fetch_shader_data)) {
            // Specialize shader on VS input number types to follow spec.
            ForEachSharp(vs_attribs, fetch_shader_data.attributes,
                         [this](auto& spec, const auto& desc, AmdGpu::Buffer sharp) {
                             using InstanceIdType = Shader::Gcn::VertexAttribute::InstanceIdType;
                             if (const auto step_rate = desc.GetStepRate();
                                 step_rate != InstanceIdType::None) {
                                 spec.divisor = step_rate == InstanceIdType::OverStepRate0
                                                    ? runtime_info.sw.vs.step_rate_0
                                                    : (step_rate == InstanceIdType::OverStepRate1
                                                           ? runtime_info.sw.vs.step_rate_1
                                                           : 1);
                             }
                             spec.num_class = AmdGpu::GetNumberClass(sharp.GetNumberFmt());
                             spec.dst_select = sharp.DstSelect();
                         });
        }
        u32 binding{};
        ForEachSharp(binding, buffers, info->buffers,
                     [](auto& spec, const auto& desc, AmdGpu::Buffer sharp) {
                         spec.stride = sharp.GetStride();
                         spec.is_formatted = desc.is_formatted;
                         spec.swizzle_enable = sharp.swizzle_enable;
                         if (spec.is_formatted) {
                             spec.data_format = static_cast<u32>(sharp.GetDataFmt());
                             spec.num_format = static_cast<u32>(sharp.GetNumberFmt());
                             spec.dst_select = sharp.DstSelect();
                             spec.num_conversion = sharp.GetNumberConversion();
                         }
                         if (spec.swizzle_enable) {
                             spec.index_stride = sharp.index_stride;
                             spec.element_size = sharp.element_size;
                         }
                     });
        ForEachSharp(binding, images, info->images,
                     [&](auto& spec, const auto& desc, AmdGpu::Image sharp) {
                         spec.type = sharp.GetViewType(desc.is_array);
                         spec.is_integer = AmdGpu::IsInteger(sharp.GetNumberFmt());
                         if (profile_.native_coherent_images)
                             spec.native_number_format = u32(sharp.GetNumberFmt());
                         spec.is_storage = desc.is_written;
                         const u32 data_format = u32(sharp.GetDataFmt());
                         if (profile_.native_coherent_images && (desc.native_packed10_raw || desc.native_raw32 || (desc.is_written &&
                             ((data_format == 9 && (
                               sharp.GetNumberFmt() == AmdGpu::NumberFormat::Snorm ||
                               sharp.GetNumberFmt() == AmdGpu::NumberFormat::Sint)) ||
                              (data_format >= 16 && data_format <= 19)))))
                             spec.native_packed_format = data_format;
                         spec.native_srgb_read = desc.native_srgb_read;
                         spec.native_slice_view = profile_.native_coherent_images && desc.NeedsNativeSliceView(*info);
                         if (spec.is_storage || desc.native_packed10_raw || desc.native_raw32) {
                             spec.dst_select = sharp.DstSelect();
                         } else {
                             spec.is_srgb = sharp.GetNumberFmt() == AmdGpu::NumberFormat::Srgb;
                         }
                         spec.num_conversion = sharp.GetNumberConversion();
                         spec.num_bindings = desc.NumBindings(*info);
                         // Read-only explicit-LOD Gather also lowers to typed loads.
                         // Its selected mip depends on these view constants.
                         if (profile_.native_coherent_images) {
                             spec.native_storage_mip_levels = sharp.last_level - sharp.base_level + 1;
                             const u32 minimum = std::min(u32(sharp.min_lod), u32(sharp.last_level) * 256);
                             spec.native_resource_min_lod = minimum > u32(sharp.base_level) * 256 ?
                                 minimum - u32(sharp.base_level) * 256 : 0;
                         }
                     });
        if (profile_.native_coherent_images) {
            for (const auto& image : info->images)
                native_external_image_aliases.push_back(u8(std::ranges::any_of(
                    info->native_pipeline_writes, [&](const auto& write) {
                        return NativeImageViewAliases(image, write, *info);
                    })) | u8(std::ranges::any_of(info->native_pipeline_signed_packed_views,
                        [&](const auto& view){return NativeImageViewAliases(image,view,*info);}) ? 2 : 0) |
                    u8(std::ranges::any_of(info->native_pipeline_raw32_views,
                        [&](const auto& view){return view.sharp.GetDataFmt()!=image.GetSharp(*info).GetDataFmt()&&
                            NativeImageViewAliases(image,view,*info);}) ? 4 : 0));
        }
        if (profile_.native_coherent_images && std::ranges::any_of(info->images,
                [](const auto& image) { return image.is_written; })) {
            for (u32 i = 0; i < info->images.size(); ++i) {
                u32 group = i;
                for (u32 j = 0; j < i; ++j)
                    if (NativeImageViewAliases(info->images[i], info->images[j], *info)) {
                        group = native_image_aliases[j];
                        break;
                    }
                native_image_aliases.push_back(group);
            }
        }
        ForEachSharp(binding, fmasks, info->fmasks,
                     [](auto& spec, const auto& desc, AmdGpu::Image sharp) {
                         spec.width = sharp.width;
                         spec.height = sharp.height;
                     });
        ForEachSharp(samplers, info->samplers,
                     [&](auto& spec, const auto& desc, AmdGpu::Sampler sharp) {
                         spec.force_unnormalized = sharp.force_unnormalized;
                         spec.force_degamma = sharp.force_degamma;
                         if (profile_.native_coherent_images && (desc.native_manual_sampling ||
                             NativeSampler::RequiresManualAnisotropy(sharp) ||
                             NativeSampler::RequiresManualAddressing(sharp, AmdGpu::ImageType::Color3D))) {
                             spec.native_raw0 = sharp.raw0;
                             spec.native_raw1 = sharp.raw1;
                         }
                     });

        // Initialize runtime_info fields that rely on analysis in tessellation passes
        if (info->sw_stage == SwStage::TessellationControl ||
            info->sw_stage == SwStage::TessellationEval) {
            TessellationDataConstantBuffer tess_constants{};
            info->ReadTessConstantBuffer(tess_constants);
            runtime_info.InitFromTessConstants(tess_constants);
        }
    }

    void ForEachSharp(auto& spec_list, auto& desc_list, auto&& func) {
        for (const auto& desc : desc_list) {
            auto& spec = spec_list.emplace_back();
            const auto sharp = desc.GetSharp(*info);
            if (!sharp) {
                continue;
            }
            func(spec, desc, sharp);
        }
    }

    void ForEachSharp(u32& binding, auto& spec_list, auto& desc_list, auto&& func) {
        for (const auto& desc : desc_list) {
            auto& spec = spec_list.emplace_back();
            const auto sharp = desc.GetSharp(*info);
            if (!sharp) {
                binding++;
                continue;
            }
            bitset[binding++] = true;
            func(spec, desc, sharp);
        }
    }

    [[nodiscard]] bool Valid() const {
        return info != nullptr;
    }

    bool operator==(const StageSpecialization& other) const {
        if (!Valid()) {
            return false;
        }

        if (buffers.size() != other.buffers.size() || images.size() != other.images.size() ||
            samplers.size() != other.samplers.size() || fmasks.size() != other.fmasks.size() ||
            buffers.size() + images.size() + fmasks.size() > MaxStageResources) {
            return false;
        }

        if (vs_attribs != other.vs_attribs) {
            return false;
        }

        if (runtime_info != other.runtime_info) {
            return false;
        }

        if (fetch_shader_data != other.fetch_shader_data) {
            return false;
        }

        if (fmasks != other.fmasks) {
            return false;
        }
        if (native_image_aliases != other.native_image_aliases) return false;
        if (native_external_image_aliases != other.native_external_image_aliases) return false;

        // For VS which only generates geometry and doesn't have any inputs, its start
        // bindings still may change as they depend on previously processed FS. The check below
        // handles this case and prevents generation of redundant permutations. This is also safe
        // for other types of shaders with no bindings.
        if (buffers.empty() && images.empty() && samplers.empty()) {
            return true;
        }

        if (start != other.start) {
            return false;
        }

        u32 binding{};
        for (u32 i = 0; i < buffers.size(); i++) {
            if (other.bitset[binding++] && buffers[i] != other.buffers[i]) {
                return false;
            }
        }
        for (u32 i = 0; i < images.size(); i++) {
            if (other.bitset[binding++] && images[i] != other.images[i]) {
                return false;
            }
        }

        for (u32 i = 0; i < samplers.size(); i++) {
            if (samplers[i] != other.samplers[i]) {
                return false;
            }
        }
        return true;
    }

    void Serialize(Serialization::Archive& ar) const;
    bool Deserialize(Serialization::Archive& ar);
};

} // namespace Shader
