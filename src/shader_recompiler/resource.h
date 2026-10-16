// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"
#include "shader_recompiler/ir/type.h"
#include "video_core/amdgpu/resource.h"

#include <boost/container/static_vector.hpp>
#include <cstring>

namespace Shader {

static constexpr u32 NUM_USER_DATA_REGS = 16;
static constexpr u32 NUM_IMAGES = 64;
static constexpr u32 NUM_BUFFERS = 40;
static constexpr u32 NUM_SAMPLERS = 16;
static constexpr u32 NUM_FMASKS = 8;

using SharpLocation = u16;

constexpr SharpLocation UNKNOWN_LOCATION = std::numeric_limits<u16>::max();

template <typename T>
struct SharpFetch {
    static constexpr std::size_t N = sizeof(T) / sizeof(u32);
    static_assert(N <= 8);

    enum class Summary : u8 {
        SingleLoad,
        MultiLoad,
        Invalid,
    };

    Summary summary{Summary::MultiLoad};
    std::array<u32, N> immediates;
    std::array<SharpLocation, N> offsets;
    u8 load_mask;

    bool operator==(const SharpFetch&) const = default;

    template <u32 num_dwords = N>
        requires(num_dwords <= N)
    constexpr bool Fetch(const u32* flatbuf, T* out) const {
        if (summary == Summary::SingleLoad) [[likely]] {
            std::memcpy(out, flatbuf + offsets[0], num_dwords * sizeof(u32));
            return true;
        } else if (summary == Summary::Invalid) [[unlikely]] {
            return false;
        }
        u8 mask = load_mask;
        std::array<u32, num_dwords> out_dw;
        for (u32 i = 0; i < num_dwords; i++) {
            out_dw[i] = (mask & 1) ? flatbuf[offsets[i]] : immediates[i];
            mask >>= 1;
        }
        std::memcpy(out, out_dw.data(), sizeof(out_dw));
        return true;
    }
};

enum class SharpFetchPostOp : u8 {
    None,
    // For buffers
    BitwiseOrDw1WithImm,
    OffsetByProgramBase,
    ClearStrideAndBitshiftDw2WithImm,
    // For images,
    ConvertCubeTo2DArray,
    // For samplers
    DisableAnisoIfSingleLod,
    ForceRepeatXyzClamp,
    ForceLastTexelXyClamp,
    ClearAnisoRatioAndThreshold,
};

enum class BufferType : u8 {
    Guest,
    Flatbuf,
    BdaPagetable,
    FaultBuffer,
    GdsBuffer,
    SharedMemory,
    ClipPlanes,
    BorderColors,
};

struct BufferResource {
    SharpFetch<AmdGpu::Buffer> sharp_fetch{};
    IR::Type used_types{};
    BufferType buffer_type{};
    bool is_written{};
    bool is_formatted{};
    SharpFetchPostOp post_op{};
    u32 post_op_dw1_mask{};
    u32 post_op_dw2_shift{};

    bool IsSpecial() const noexcept {
        return buffer_type != BufferType::Guest;
    }

    constexpr AmdGpu::Buffer GetSharp(const auto& info) const noexcept {
        AmdGpu::Buffer buffer;
        if (!sharp_fetch.Fetch(info.flattened_ud_buf.data(), &buffer)) {
            return AmdGpu::Buffer::Null();
        }
        if (post_op == SharpFetchPostOp::BitwiseOrDw1WithImm) {
            reinterpret_cast<u32*>(&buffer)[1] |= post_op_dw1_mask;
        } else if (post_op == SharpFetchPostOp::ClearStrideAndBitshiftDw2WithImm) {
            buffer.stride = 0;
            buffer.num_records <<= post_op_dw2_shift;
        } else if (post_op == SharpFetchPostOp::OffsetByProgramBase) {
            buffer.base_address += info.pgm_base;
        }
        if (!buffer.Valid()) {
            return AmdGpu::Buffer::Null();
        }
        return buffer;
    }
};
using BufferResourceList = boost::container::static_vector<BufferResource, NUM_BUFFERS>;

enum class MipStorageFallbackMode : u16 {
    None,
    DynamicIndex,
    ConstantIndex,
};

struct ImageResource {
    SharpFetch<AmdGpu::Image> sharp_fetch{};
    bool is_depth{};
    bool is_atomic{};
    bool is_array{};
    bool is_written{};
    bool is_r128{};
    bool native_srgb_read{};
    bool native_msaa_storage{};
    bool native_packed10_raw{};
    bool native_raw32{};
    u8 constant_mip_index{};
    MipStorageFallbackMode mip_fallback_mode{};
    SharpFetchPostOp post_op{};

    constexpr AmdGpu::Image GetSharp(const auto& info) const noexcept {
        AmdGpu::Image image{};
        if (!Fetch(info.flattened_ud_buf.data(), &image)) {
            return AmdGpu::Image::Null(is_depth);
        }
        if (post_op == SharpFetchPostOp::ConvertCubeTo2DArray) {
            image.type = u64(AmdGpu::ImageType::Color2DArray);
            image.depth = (image.depth + 1) * 6 - 1;
        }
        if (!image.Valid()) {
            image = AmdGpu::Image::Null(is_depth);
        } else if (is_depth) {
            const auto data_fmt = image.GetDataFmt();
            if (data_fmt != AmdGpu::DataFormat::Format16 &&
                data_fmt != AmdGpu::DataFormat::Format32) {
                image = AmdGpu::Image::Null(true);
            }
        }
        return image;
    }

    constexpr bool Fetch(const u32* flatbuf, AmdGpu::Image* out) const {
        if (!is_r128) {
            // Fetch full 8 byte T#
            return sharp_fetch.Fetch(flatbuf, out);
        }
        // Fetch r128 T# and fix pitch
        if (!sharp_fetch.Fetch<4>(flatbuf, out)) {
            return false;
        }
        out->pitch = out->width;
        return true;
    }

    bool NeedsNativeSliceView(const auto& info) const {
        const auto type = GetSharp(info).GetViewType(is_array);
        // A logical non-array image always binds one physical array slice.
        // Its backing can then be a standalone texture or an existing array
        // without changing the compiled resource dimension at binding time.
        return !is_array && (type == AmdGpu::ImageType::Color1D ||
            type == AmdGpu::ImageType::Color2D || type == AmdGpu::ImageType::Color2DMsaa);
    }

    AmdGpu::ImageType NativeViewType(const auto& info) const {
        const auto type = GetSharp(info).GetViewType(is_array);
        if (!NeedsNativeSliceView(info)) return type;
        switch (type) {
        case AmdGpu::ImageType::Color1D: return AmdGpu::ImageType::Color1DArray;
        case AmdGpu::ImageType::Color2D: return AmdGpu::ImageType::Color2DArray;
        case AmdGpu::ImageType::Color2DMsaa: return AmdGpu::ImageType::Color2DMsaaArray;
        default: return type;
        }
    }

    u32 NumBindings(const auto& info) const {
        const AmdGpu::Image tsharp = GetSharp(info);
        if (native_msaa_storage) return tsharp.NumSamples();
        return (mip_fallback_mode == MipStorageFallbackMode::DynamicIndex)
                   ? (tsharp.last_level - tsharp.base_level + 1)
                   : 1;
    }
};
using ImageResourceList = boost::container::static_vector<ImageResource, NUM_IMAGES>;

// View origins/ranges do not change physical image storage. Shader coordinates
// and UAV descriptors retain these fields independently for each access.
inline bool NativeImageStorageAliases(AmdGpu::Image left, AmdGpu::Image right) {

    left.base_level = right.base_level = 0;
    left.min_lod = right.min_lod = 0;
    left.base_array = right.base_array = 0;
    left.last_array = right.last_array = 0;
    left.dst_sel_x = right.dst_sel_x = 4;
    left.dst_sel_y = right.dst_sel_y = 5;
    left.dst_sel_z = right.dst_sel_z = 6;
    left.dst_sel_w = right.dst_sel_w = 7;
    // These families have typeless native storage with fully typed UAV views.
    // Packed/compressed families without that representation stay distinct.
    if ((left.data_format == 4 || left.data_format == 10) &&
        (right.data_format == 4 || right.data_format == 10)) {
        left.data_format = right.data_format = 4;
        left.num_format = right.num_format = 0;
    } else if (left.data_format == right.data_format) {
        switch (left.data_format) {
        case 1: case 2: case 3: case 4: case 5:
        case 9: case 10: case 11: case 12: case 14:
            left.num_format = right.num_format = 0;
            break;
        default: break;
        }
    }
    return left.base_address != 0 && right.base_address != 0 &&
           std::memcmp(&left, &right, sizeof(left)) == 0;
}

// Resolved descriptions are independent of another stage's user-data registers.
struct NativeImageWrite {
    AmdGpu::Image sharp;
    bool is_array{};
    bool is_depth{};
};

// A native single-slice view and an array view retain distinct coordinates and
// descriptors but can share the same physical 1D/2D array backing.
inline bool NativeImageArrayViewsCompatible(AmdGpu::ImageType type, bool a, bool b) {
    return a == b || type == AmdGpu::ImageType::Color1DArray ||
           type == AmdGpu::ImageType::Color2DArray || type == AmdGpu::ImageType::Color2DMsaaArray;
}
inline bool NativeImageViewAliases(const ImageResource& a, const ImageResource& b,
                                   const auto& info) {
    // Comparison is an access operation, not a different physical allocation.
    // GetSharp validates depth formats before matching the actual storage.
    if ((a.is_depth || b.is_depth) && a.GetSharp(info).GetDataFmt() != b.GetSharp(info).GetDataFmt()) return false;
    return NativeImageArrayViewsCompatible(a.GetSharp(info).GetType(), a.is_array, b.is_array) &&
           NativeImageStorageAliases(a.GetSharp(info), b.GetSharp(info));
}

inline bool NativeImageViewAliases(const ImageResource& source, const NativeImageWrite& write,
                                   const auto& info) {
    if ((source.is_depth || write.is_depth) && source.GetSharp(info).GetDataFmt() != write.sharp.GetDataFmt()) return false;
    return NativeImageArrayViewsCompatible(source.GetSharp(info).GetType(), source.is_array, write.is_array) &&
           NativeImageStorageAliases(source.GetSharp(info), write.sharp);
}

struct SamplerResource {
    SharpFetch<AmdGpu::Sampler> sharp_fetch{};
    SharpFetchPostOp post_op{};
    SharpLocation post_op_tsharp_dw3_off{};
    bool is_depth{};
    bool native_manual_sampling{};
    bool native_custom_border{};

    constexpr AmdGpu::Sampler GetSharp(const auto& info) const noexcept {
        AmdGpu::Sampler sampler{};
        sharp_fetch.Fetch(info.flattened_ud_buf.data(), &sampler);
        if (post_op == SharpFetchPostOp::DisableAnisoIfSingleLod) {
            const u32 tsharp_dw3 = info.flattened_ud_buf[post_op_tsharp_dw3_off];
            if (((tsharp_dw3 >> 12) & 0xff) == 0) {
                sampler.max_aniso.Assign(AmdGpu::AnisoRatio::One);
            }
        } else if (post_op == SharpFetchPostOp::ForceRepeatXyzClamp) {
            sampler.clamp_x.Assign(AmdGpu::ClampMode::Wrap);
            sampler.clamp_y.Assign(AmdGpu::ClampMode::Wrap);
            sampler.clamp_z.Assign(AmdGpu::ClampMode::Wrap);
        } else if (post_op == SharpFetchPostOp::ForceLastTexelXyClamp) {
            sampler.clamp_x.Assign(AmdGpu::ClampMode::ClampLastTexel);
            sampler.clamp_y.Assign(AmdGpu::ClampMode::ClampLastTexel);
            sampler.clamp_z.Assign(AmdGpu::ClampMode::Wrap);
        } else if (post_op == SharpFetchPostOp::ClearAnisoRatioAndThreshold) {
            sampler.max_aniso.Assign(AmdGpu::AnisoRatio::One);
            sampler.aniso_threshold.Assign(0);
        }
        return sampler;
    }
};
using SamplerResourceList = boost::container::static_vector<SamplerResource, NUM_SAMPLERS>;

struct FMaskResource {
    SharpLocation sharp_idx;

    constexpr AmdGpu::Image GetSharp(const auto& info) const noexcept {
        return info.template ReadUdSharp<AmdGpu::Image>(sharp_idx);
    }
};
using FMaskResourceList = boost::container::static_vector<FMaskResource, NUM_FMASKS>;

struct PushData {
    static constexpr u32 XOffsetIndex = 0;
    static constexpr u32 YOffsetIndex = 1;
    static constexpr u32 XScaleIndex = 2;
    static constexpr u32 YScaleIndex = 3;
    static constexpr u32 BufOffsetIndex = 4;

    float xoffset;
    float yoffset;
    float xscale;
    float yscale;
    std::array<u8, NUM_BUFFERS> buf_offsets;

    void AddOffset(u32 binding, u32 offset) {
        ASSERT(offset < 256 && binding < buf_offsets.size());
        buf_offsets[binding] = offset;
    }
};
static_assert(sizeof(PushData) <= 128,
              "PushData size is greater than minimum size guaranteed by Vulkan spec");

} // namespace Shader
