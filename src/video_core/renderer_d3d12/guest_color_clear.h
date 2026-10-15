// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/amdgpu/regs_color.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>

namespace NativeDX12 {
// A CB clear is a typed value followed by the CB component mapping, not a
// repeated WORD0/WORD1 pattern. Keep physical materialization and native RTV
// clears on the same conversion. The pixel is in the guest encoding of the
// remapped storage format; native transfers retain their format repacking.
struct GuestColorClearValue {
    std::array<std::byte, 16> pixel{};
    std::array<float, 4> rtv{};
    uint32_t data_format{}, number_format{}, pixel_bytes{};
    bool rtv_compatible{true};
};

inline std::optional<GuestColorClearValue> DecodeGuestColorClear(
    const AmdGpu::ColorBuffer& buffer) {
    const uint32_t format = buffer.info.format;
    const uint32_t number = uint32_t(buffer.GetFixedNumberFormat());
    if (format == 6 || format == 7) {
        if (number != 7) return std::nullopt;
        const uint32_t word = buffer.clear_word0;
        // Canonical UF11 bits retain the five-bit exponent. Widening UF10
        // needs only a zero low mantissa bit, including packed subnormals.
        // Original7 uses different fields and maps R/B before storage in6.
        std::array<uint32_t, 4> values = format == 6 ?
            std::array<uint32_t, 4>{word & 0x7ff, (word >> 11) & 0x7ff,
                ((word >> 22) & 0x3ff) << 1, 0x3c0} :
            std::array<uint32_t, 4>{(word & 0x3ff) << 1, (word >> 10) & 0x7ff,
                (word >> 21) & 0x7ff, 0x3c0};
        values = buffer.Swizzle().Apply(values);
        GuestColorClearValue clear;
        // Numeric format conversion preserves infinity and the NaN class, not
        // a particular NaN payload. Choose a quiet packed NaN for every selected
        // NaN lane and require the raw GPU clear to write that same CPU word.
        // Canonicalize before UF11 -> UF10 narrowing: dropping the sole low
        // payload bit of an arbitrary NaN would incorrectly produce infinity.
        for (uint32_t i = 0; i < 3; ++i) {
            if ((values[i] >> 6) == 31 && (values[i] & 63)) {
                values[i] = 0x7e0;
                clear.rtv_compatible = false;
            }
        }
        // Choose deterministic toward-zero UF11 -> UF10 storage conversion
        // (D3D11.3 section 3.2.2, also permitted by Vulkan format conversion).
        // Dropping the one excess mantissa bit handles subnormals and maps
        // the finite UF11 maximum to the finite UF10 maximum, never infinity.
        // Prequantize the RTV input as well, so CPU/native writes agree even
        // when a driver uses a different permitted floating-format tie mode.
        values[2] &= ~1u;
        clear.data_format = 6; clear.number_format = number; clear.pixel_bytes = 4;
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t exponent = values[i] >> 6, mantissa = values[i] & 63;
            // Even the smallest packed subnormal is normal in F32, so the
            // numeric RTV input does not encounter F32 denormal flushing.
            clear.rtv[i] = exponent == 31 ? (mantissa ?
                std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<float>::infinity()) :
                exponent ? std::ldexp(float(64 + mantissa), int(exponent) - 21) :
                std::ldexp(float(mantissa), -20);
        }
        const uint32_t packed = values[0] | (values[1] << 11) | ((values[2] >> 1) << 22);
        std::memcpy(clear.pixel.data(), &packed, 4); return clear;
    }
    if (format >= 16 && format <= 19) {
        // Vulkan R5G6B5 / A1R5G5B5 and DXGI B5G6R5 / B5G5R5A1
        // have identical physical bits. Original format18 remaps to format17,
        // but its clear fields and component mapping remain those of format18.
        // Format19's guest bits match Vulkan B4G4R4A4; NativePacked16 already
        // repacks them to DXGI B4G4R4A4 on transfer. Its RTV stays logical RGBA.
        if (number != 0) return std::nullopt;
        const uint32_t word = buffer.clear_word0;
        std::array<uint32_t, 4> values{}, maxima{};
        if (format == 16) {
            values = {word & 31, (word >> 5) & 63, uint8_t(word >> 11), 1};
            maxima = {31, 63, 31, 1};
        } else if (format == 17) {
            values = {word & 31, (word >> 5) & 31, (word >> 10) & 31, word >> 15 ? 1u : 0u};
            maxima = {31, 31, 31, 1};
        } else if (format == 18) {
            values = {word & 1, (word >> 1) & 31, (word >> 6) & 31, (word >> 11) & 31};
            maxima = {1, 31, 31, 31};
        } else {
            values = {word & 15, (word >> 4) & 15, (word >> 8) & 15, uint8_t(word >> 12)};
            maxima = {15, 15, 15, 15};
        }
        // Format16/19's pinned top U5/U4 helpers receive uint8_t, not already-
        // masked fields; retain those inputs and let UNORM conversion saturate.
        values = buffer.Swizzle().Apply(values);
        maxima = buffer.Swizzle().Apply(maxima);
        GuestColorClearValue clear;
        clear.data_format = uint32_t(buffer.GetDataFmt());
        clear.number_format = number; clear.pixel_bytes = 2;
        const std::array<uint32_t, 4> destination_maxima = format == 16 ?
            std::array<uint32_t, 4>{31, 63, 31, 0} : format == 19 ?
            std::array<uint32_t, 4>{15, 15, 15, 15} : std::array<uint32_t, 4>{31, 31, 31, 1};
        const std::array<uint32_t, 4> offsets = format == 16 ?
            std::array<uint32_t, 4>{11, 5, 0, 0} : format == 19 ?
            std::array<uint32_t, 4>{4, 8, 12, 0} : std::array<uint32_t, 4>{10, 5, 0, 15};
        uint16_t packed{};
        for (uint32_t i = 0; i < (format == 16 ? 3u : 4u); ++i) {
            clear.rtv[i] = float(values[i]) / float(maxima[i]);
            const uint32_t stored = (std::min(values[i], maxima[i]) * destination_maxima[i] +
                maxima[i] / 2) / maxima[i];
            packed |= uint16_t(stored << offsets[i]);
        }
        std::memcpy(clear.pixel.data(), &packed, 2); return clear;
    }
    if (format == 8 || format == 9) {
        // Original 10_10_10_2 and 2_10_10_10 both use native R10G10B10A2.
        // Decode the ORIGINAL lane widths, apply its remapping exactly once,
        // then quantize/narrow to the destination widths (alpha is only 2 bits).
        if (number != 0 && number != 4) return std::nullopt;
        const std::array<uint32_t, 4> widths = format == 8 ?
            std::array<uint32_t, 4>{2, 10, 10, 10} : std::array<uint32_t, 4>{10, 10, 10, 2};
        std::array<uint32_t, 4> values{}, maxima{};
        uint32_t offset{};
        for (uint32_t i = 0; i < 4; ++i) {
            maxima[i] = (1u << widths[i]) - 1;
            values[i] = (buffer.clear_word0 >> offset) & maxima[i]; offset += widths[i];
        }
        values = buffer.Swizzle().Apply(values);
        maxima = buffer.Swizzle().Apply(maxima);
        GuestColorClearValue clear;
        clear.data_format = 9; clear.number_format = number; clear.pixel_bytes = 4;
        uint32_t packed{};
        for (uint32_t i = 0; i < 4; ++i) {
            const uint32_t maximum = i == 3 ? 3 : 1023;
            const uint32_t stored = number == 4 ? values[i] & maximum :
                (values[i] * maximum + maxima[i] / 2) / maxima[i];
            clear.rtv[i] = number == 4 ? float(stored) : float(values[i]) / float(maxima[i]);
            packed |= stored << (i * 10);
        }
        std::memcpy(clear.pixel.data(), &packed, 4); return clear;
    }
    uint32_t bits{}, channels{};
    switch (format) {
    case 1: bits = 8; channels = 1; break;
    case 3: bits = 8; channels = 2; break;
    case 10: bits = 8; channels = 4; break;
    case 2: bits = 16; channels = 1; break;
    case 5: bits = 16; channels = 2; break;
    case 12: bits = 16; channels = 4; break;
    case 4: bits = 32; channels = 1; break;
    case 11: bits = 32; channels = 2; break;
    case 14: bits = 32; channels = 4; break;
    default: return std::nullopt;
    }
    // Original CB types, not descriptor remaps: e.g. 32-bit UNORM storage is
    // remapped to UINT, but the pinned clear helper defines no such conversion.
    if (number != 0 && number != 1 && number != 4 && number != 5 &&
        number != 7 && number != 9) return std::nullopt;
    if ((bits == 32 && number != 4 && number != 5 && number != 7) ||
        (bits == 8 && number == 7) || (number == 9 && format != 10))
        return std::nullopt;

    GuestColorClearValue clear;
    clear.data_format = uint32_t(buffer.GetDataFmt());
    clear.number_format = uint32_t(buffer.GetNumberFmt());
    clear.pixel_bytes = channels * bits / 8;
    const uint32_t mask = bits == 32 ? UINT32_MAX : (1u << bits) - 1;
    std::array<uint32_t, 4> raw{};
    // Defaults are part of the reference's two-channel Alternate/Reverse
    // mappings. CB mappings select lanes or zero (never a constant one).
    raw[3] = number == 4 || number == 5 ? 1u :
        number == 7 ? (bits == 16 ? 0x3c00u : 0x3f800000u) :
        number == 1 ? mask >> 1 : mask;
    const uint32_t words[]{buffer.clear_word0, buffer.clear_word1};
    for (uint32_t i = 0; i < channels; ++i) {
        if (bits == 32) raw[i] = i == 3 || (channels == 2 && i == 1) ? words[1] : words[0];
        else raw[i] = (words[(i * bits) / 32] >> ((i * bits) % 32)) & mask;
    }
    // Unlike packed narrow components, the reference takes the entire WORD0
    // for single-channel integer formats (including sign-extended SINT).
    if (channels == 1 && (number == 4 || number == 5)) raw[0] = words[0];
    raw = buffer.Swizzle().Apply(raw);
    for (uint32_t i = 0; i < channels; ++i) {
        uint32_t stored = raw[i];
        switch (number) {
        case 0: clear.rtv[i] = float(raw[i]) / float(mask); break;
        case 9: {
            clear.rtv[i] = float(raw[i]) / 255.f;
            // Pinned Vulkan interprets CB bytes/255 as LINEAR clear input to
            // an sRGB view. Alpha is ordinary UNORM. This is numeric format
            // conversion; hardware quantization still needs runtime coverage.
            if (i < 3) {
                const double linear = double(clear.rtv[i]);
                const double srgb = linear <= 0.0031308 ? linear * 12.92 :
                    1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
                stored = uint32_t(std::lround(std::clamp(srgb, 0.0, 1.0) * 255.0));
            }
            break;
        }
        case 1: {
            const int32_t signed_value = int32_t(raw[i]) -
                ((raw[i] & (1u << (bits - 1))) ? int32_t(1u << bits) : 0);
            const int32_t maximum = int32_t(mask >> 1);
            const int32_t value = std::max(signed_value, -maximum);
            clear.rtv[i] = float(value) / float(maximum);
            stored = uint32_t(value) & mask;
            break;
        }
        case 4:
            // Vulkan's unsigned narrow clear truncates, whereas D3D's float
            // conversion can saturate. Narrow first, then pass the exact value.
            stored &= mask;
            clear.rtv[i] = float(stored);
            clear.rtv_compatible &= double(clear.rtv[i]) == double(stored);
            break;
        case 5: {
            const int32_t value = std::bit_cast<int32_t>(raw[i]);
            // Packed negative narrow SINT values are zero-extended by the
            // pinned helper: its out-of-range signed clears are undefined.
            // Keep those cases on the historical raw fallback, not a guessed
            // saturation/sign-extension rule claimed as reference parity.
            if (bits < 32 && (value < -int32_t(1u << (bits - 1)) ||
                             value > int32_t(mask >> 1))) return std::nullopt;
            stored &= mask;
            clear.rtv[i] = float(value);
            // The D3D clear conversion specifies low-one fill for large
            // negative SINT. A representable float alone is not sufficient.
            clear.rtv_compatible &= double(clear.rtv[i]) == double(value) &&
                value >= -0x1000000;
            break;
        }
        case 7:
            if (bits == 32) {
                clear.rtv[i] = std::bit_cast<float>(raw[i]);
                // ClearRenderTargetView flushes F32 denormals; numeric NaN
                // conversion does not establish payload preservation either.
                const auto exponent = raw[i] & 0x7f800000u;
                clear.rtv_compatible &= exponent != 0x7f800000u &&
                    (exponent != 0 || (raw[i] & 0x7fffffu) == 0);
            } else {
                const uint32_t exponent = (raw[i] >> 10) & 31;
                const uint32_t mantissa = raw[i] & 1023;
                clear.rtv_compatible &= exponent != 31;
                const float value = exponent ? std::ldexp(float(1024 + mantissa), int(exponent) - 25) :
                    std::ldexp(float(mantissa), -24);
                clear.rtv[i] = raw[i] & 0x8000 ? -value : value;
            }
            break;
        }
        // Exact raw integer/F32 lanes never round-trip through float here.
        std::memcpy(clear.pixel.data() + i * bits / 8, &stored, bits / 8);
    }
    return clear;
}
} // namespace NativeDX12
