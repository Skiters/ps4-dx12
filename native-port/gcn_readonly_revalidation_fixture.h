// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {

// Authored, unrun real-GCN coverage of readonly image history and refresh.
inline void RunReadonlyRevalidationGcnFixture(GuestRasterizer& rasterizer, AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile, std::byte* memory, std::byte* backing, u64 base) {
    constexpr u64 AllocationBytes = 65536, ImageOffset = 16384, ObservationOffset = 57344;
    constexpr u32 BufferValue = 0x13579bdf, ImageValue = 0x2468ace0;
    constexpr u32 ImageNext = 0x6a5b4c3d, ImageFar = 0x3142a6b7;
    constexpr u32 FinalValue = 0x4567a9cd, MarkerValue = 0x5c6d7e8f;
    struct alignas(256) Code { std::array<u32, 128> words{}; };
    Code warm{}, seed{}, before_probe{}, marker{}, refresh{}, raw_probe{}, formatted{}, after_probe{};
    GuestShaderFrontend reflection(profile);
    struct Drain {
        GuestRasterizer& rasterizer;
        bool armed = true;
        ~Drain() { if (armed) try { rasterizer.Finish(); } catch (...) {} }
    } drain{rasterizer};
    const auto initialize = [](Code& code, const std::vector<u32>& words, u64 hash) {
        constexpr u32 MetadataWord = 96;
        if (words.size() > MetadataWord ||
            MetadataWord * sizeof(u32) + sizeof(AmdGpu::BinaryInfo) > sizeof(code.words))
            throw std::runtime_error("Readonly revalidation GCN code overlaps binary metadata");
        code.words.fill(0);
        std::copy(words.begin(), words.end(), code.words.begin());
        AmdGpu::BinaryInfo binary{};
        binary.signature = binary.signature_ref;
        binary.length = u32(words.size() * sizeof(u32));
        binary.shader_hash = hash;
        std::memcpy(code.words.data() + MetadataWord, &binary, sizeof(binary));
    };
    const auto bind = [&](const Code& code, u32 user_registers) -> AmdGpu::ComputeProgram& {
        auto& program = gpu.GetCsRegs();
        program = {};
        program.address = reinterpret_cast<u64>(code.words.data()) >> 8;
        program.num_thread_x.full = program.num_thread_y.full = program.num_thread_z.full = 1;
        program.dim_x = program.dim_y = program.dim_z = 1;
        program.settings.num_user_regs = user_registers;
        program.settings.num_vgprs = 3;
        program.settings.num_sgprs = 3;
        return program;
    };
    struct BufferExpectation { AmdGpu::Buffer sharp; bool written, formatted; };
    const auto verify = [](const auto& stage, std::initializer_list<BufferExpectation> expected,
                           const AmdGpu::Image* image = nullptr, bool image_written = false) {
        if (stage.info.uses_dma || stage.info.images.size() != (image ? 1u : 0u))
            throw std::runtime_error("Readonly revalidation GCN lost its expected image/DMA operands");
        auto next = expected.begin();
        for (const auto& buffer : stage.info.buffers) if (!buffer.IsSpecial()) {
            if (next == expected.end())
                throw std::runtime_error("Readonly revalidation GCN gained a guest buffer");
            const auto sharp = buffer.GetSharp(stage.info);
            if (sharp.base_address != next->sharp.base_address || sharp.GetSize() != next->sharp.GetSize() ||
                buffer.is_written != next->written || buffer.is_formatted != next->formatted)
                throw std::runtime_error("Readonly revalidation GCN lost original sharp bounds or first-use order");
            ++next;
        }
        if (next != expected.end())
            throw std::runtime_error("Readonly revalidation GCN removed a guest buffer");
        if (image) {
            const auto& resource = stage.info.images.front();
            const auto sharp = resource.GetSharp(stage.info);
            // Each dispatch has one image format and no mixed pipeline views;
            // image writing alone does not select native raw32 storage.
            if (resource.is_written != image_written || resource.native_raw32 ||
                std::memcmp(&sharp, image, sizeof(sharp)))
                throw std::runtime_error("Readonly revalidation GCN changed the original image sharp/access");
        }
    };
    const auto initial = [](u32 x, u32 y) { return 0x10203040u ^ (y << 12) ^ x; };
    for (const u32 variant : {0u, 1u, 2u, 3u}) {
        const bool interior_writer = variant != 0, packed_sample = variant == 2, pair_uint = variant == 3;
        const u32 buffer_value = packed_sample ? 0x13579b10u : BufferValue;
        const u64 identity = u64(variant) << 8;
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.InvalidateMemory(base, AllocationBytes);
        std::memset(memory, pair_uint ? 0xf0 : packed_sample ? 0xe1 : interior_writer ? 0xd2 : 0xc3, AllocationBytes);
        AmdGpu::Image image{};
        image.base_address = (base + ImageOffset) >> 8;
        image.type = u64(AmdGpu::ImageType::Color2D);
        image.data_format = 4; image.num_format = 4;
        image.tiling_index = 5;
        image.width = 23; image.height = 31; image.pitch = 31;
        image.dst_sel_x = 4; image.dst_sel_y = 5; image.dst_sel_z = 6; image.dst_sel_w = 7;
        const auto desc = DescribeGuestTexture(image);
        const auto layout = LayoutGuestTexture(desc);
        auto sampled_image = image;
        if (packed_sample || pair_uint) {
            // Preserve one physical DWORD per texel while changing readonly components.
            sampled_image.data_format = pair_uint ? 5 : 10;
            sampled_image.num_format = pair_uint ? 4 : 0; // R16G16_UINT or RGBA8_UNORM over R32_UINT I.
            const auto sampled_desc = DescribeGuestTexture(sampled_image);
            const auto sampled_layout = LayoutGuestTexture(sampled_desc);
            bool same_layout = sampled_layout.guest_bytes == layout.guest_bytes &&
                sampled_layout.logical_bytes == layout.logical_bytes &&
                sampled_layout.bytes_per_pixel == layout.bytes_per_pixel &&
                sampled_layout.block_extent == layout.block_extent &&
                sampled_layout.mips.size() == layout.mips.size();
            if (same_layout) for (size_t mip = 0; mip < layout.mips.size(); ++mip) {
                const auto& left = layout.mips[mip];
                const auto& right = sampled_layout.mips[mip];
                same_layout &= left.pitch == right.pitch && left.height == right.height &&
                    left.offset == right.offset && left.slice_bytes == right.slice_bytes &&
                    left.macro == right.macro && left.thickness == right.thickness;
            }
            if (!same_layout)
                throw std::runtime_error("Readonly packed fixture changed the image's physical layout");
        }
        const auto far_offset = GuestPixelOffset(desc, layout, 0, 0, 0, 8);
        if (base % 4096 || layout.guest_bytes != 4096 ||
            layout.guest_bytes <= u64(desc.width) * desc.height * sizeof(u32) ||
            GuestPixelOffset(desc, layout, 0, 0, 0, 0) != 0 ||
            GuestPixelOffset(desc, layout, 0, 0, 1, 0) != 4 ||
            GuestPixelOffset(desc, layout, 0, 0, 8, 0) != 256 || far_offset < 260 || far_offset + 4 > layout.guest_bytes ||
            ImageOffset + layout.guest_bytes > ObservationOffset || far_offset > 4095)
            throw std::runtime_error("Readonly revalidation needs an aligned padded 4-KiB image and distinct probes");
        for (u32 y = 0; y < desc.height; ++y) for (u32 x = 0; x < desc.width; ++x) {
            const auto value = initial(x, y);
            std::memcpy(memory + ImageOffset + GuestPixelOffset(desc, layout, 0, 0, x, y), &value, 4);
        }
        std::vector<std::byte> expected(backing, backing + AllocationBytes);
        const auto expect_word = [&](u64 offset, u32 value) {
            std::memcpy(expected.data() + offset, &value, sizeof(value));
        };
        AmdGpu::Buffer target{};
        target.base_address = base + ImageOffset;
        target.stride = 4; target.num_records = u32(layout.guest_bytes / 4);
        target.data_format = 4; target.num_format = 4;
        target.dst_sel_x = 4; target.dst_sel_y = 5; target.dst_sel_z = 6; target.dst_sel_w = 7;
        auto output = target;
        output.base_address = base + ObservationOffset;
        auto full = target;
        full.base_address = base; full.num_records = AllocationBytes / 4;

        // A real raw full-allocation read caches a page-aligned owned envelope.
        // Narrow later writers retain exact logical dirty ranges on this arena.
        initialize(warm, {0xe0300000, 0x80000400, 0xbf8c0000,
            0xe0700040, 0x80010400, 0xbf810000}, 0x524556414c000001ULL + identity);
        auto& warm_program = bind(warm, 8);
        std::memcpy(warm_program.user_data.data(), &full, sizeof(full));
        std::memcpy(warm_program.user_data.data() + 4, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{full, false, false}, {output, true, false}});
        rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address, output.GetSize());
        rasterizer.Finish();
        u32 warm_value{};
        std::memcpy(&warm_value, expected.data(), sizeof(warm_value));
        expect_word(ObservationOffset + 64, warm_value);
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Readonly revalidation warm arena changed physical bytes");

        initialize(seed, {
            0x7e0202ff, buffer_value, 0xe0700000, 0x80000100,
            0x7e040280, 0x7e060280, 0x7e1002ff, ImageValue, 0xf0200100, 0x00010802,
            0x7e040281, 0x7e1002ff, ImageNext, 0xf0200100, 0x00010802,
            0x7e040280, 0x7e060288, 0x7e1002ff, ImageFar, 0xf0200100, 0x00010802,
            0xbf810000
        }, 0x524556414c000002ULL + identity);
        auto& seed_program = bind(seed, 12);
        std::memcpy(seed_program.user_data.data(), &target, sizeof(target));
        std::memcpy(seed_program.user_data.data() + 4, &image, sizeof(image));
        verify(reflection.BuildCompute(gpu), {{target, true, false}}, &image, true);
        rasterizer.DispatchDirect();
        if (!rasterizer.IsGpuBufferModified(target.base_address, target.GetSize()))
            throw std::runtime_error("Readonly revalidation seed lost its buffer domain");

        initialize(before_probe, {
            0xe0300000, 0x80000400, 0x7e000280, 0x7e020280, 0xf0000100, 0x00010500,
            0x7e000281, 0xf0000100, 0x00010600,
            0x7e000280, 0x7e020288, 0xf0000100, 0x00010700, 0xbf8c0000,
            0xe0700000, 0x80030400, 0xe0700004, 0x80030500,
            0xe0700008, 0x80030600, 0xe070000c, 0x80030700, 0xbf810000
        }, 0x524556414c000003ULL + identity);
        auto& before_program = bind(before_probe, 16);
        std::memcpy(before_program.user_data.data(), &target, sizeof(target));
        std::memcpy(before_program.user_data.data() + 4, &image, sizeof(image));
        std::memcpy(before_program.user_data.data() + 12, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{target, false, false}, {output, true, false}}, &image);
        rasterizer.DispatchDirect();
        rasterizer.ReadMemory(output.base_address, output.GetSize());
        rasterizer.ReadMemory(target.base_address, target.GetSize());
        rasterizer.Finish();
        expect_word(ImageOffset, buffer_value);
        expect_word(ObservationOffset, buffer_value);
        expect_word(ObservationOffset + 4, ImageValue);
        expect_word(ObservationOffset + 8, ImageNext);
        expect_word(ObservationOffset + 12, ImageFar);
        if (rasterizer.IsGpuBufferModified(target.base_address, target.GetSize()) ||
            std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Readonly revalidation lost clean B and independently GPU-written I");
        const auto physical_before = expected;

        // This actual shifted writer keeps I valid while B gains a GPU-only
        // marker. Reconcile the seed first so the marker's dirty span is exact.
        auto marker_sharp = target;
        marker_sharp.base_address += 256; marker_sharp.num_records = 1;
        initialize(marker, {0x7e0202ff, MarkerValue, 0xe0700000, 0x80000100, 0xbf810000},
            0x524556414c000004ULL + identity);
        auto& marker_program = bind(marker, 4);
        std::memcpy(marker_program.user_data.data(), &marker_sharp, sizeof(marker_sharp));
        verify(reflection.BuildCompute(gpu), {{marker_sharp, true, false}});
        rasterizer.DispatchDirect();

        auto writer = target;
        writer.num_records = 1;
        if (interior_writer) writer.base_address += 4;
        // First actual store creates B1, while the readonly image descriptor
        // must already retain old I or capture the pinned prewrite B0 version.
        const u32 image_load = pair_uint ? 0xf0000300u : 0xf0000100u; // image_load dmaskXY/X
        const u32 second_reg = pair_uint ? 6 : 5, far_reg = pair_uint ? 8 : 6, marker_reg = pair_uint ? 10 : 7;
        std::vector<u32> refresh_words{
            0x7e1802ff, FinalValue, 0xe0700000, 0x80000c00,
            0x7e000280, 0x7e020280, image_load, 0x00010400,
            0x7e000281, image_load, 0x00010000 | (second_reg << 8),
            0x7e000280, 0x7e020288, image_load, 0x00010000 | (far_reg << 8),
            0x7e000288, 0x7e020280, image_load, 0x00010000 | (marker_reg << 8), 0xbf8c0000,
            0xe0700010, 0x80030400, 0xe0700014, 0x80030000 | (second_reg << 8),
            0xe0700018, 0x80030000 | (far_reg << 8), 0xe070001c, 0x80030000 | (marker_reg << 8)
        };
        if (pair_uint) refresh_words.insert(refresh_words.end(), {
            0xe0700050, 0x80030500, 0xe0700054, 0x80030700,
            0xe0700058, 0x80030900, 0xe070005c, 0x80030b00
        });
        refresh_words.push_back(0xbf810000);
        initialize(refresh, refresh_words, 0x524556414c000005ULL + identity);
        auto& refresh_program = bind(refresh, 16);
        std::memcpy(refresh_program.user_data.data(), &writer, sizeof(writer));
        std::memcpy(refresh_program.user_data.data() + 4, &sampled_image, sizeof(sampled_image));
        std::memcpy(refresh_program.user_data.data() + 12, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{writer, true, false}, {output, true, false}}, &sampled_image);
        rasterizer.DispatchDirect();
        const u32 kept_first = interior_writer ? ImageValue : buffer_value;
        const u32 kept_second = interior_writer ? ImageNext : initial(1, 0);
        const u32 kept_far = interior_writer ? ImageFar : initial(0, 8);
        const u32 kept_marker = interior_writer ? initial(8, 0) : MarkerValue;
        const auto image_component = [pair_uint](u32 word) { return pair_uint ? word & 65535u : word; };
        if (!packed_sample) {
            expect_word(ObservationOffset + 16, image_component(kept_first));
            expect_word(ObservationOffset + 20, image_component(kept_second));
            expect_word(ObservationOffset + 24, image_component(kept_far));
            expect_word(ObservationOffset + 28, image_component(kept_marker));
        }
        if (pair_uint) {
            expect_word(ObservationOffset + 80, kept_first >> 16);
            expect_word(ObservationOffset + 84, kept_second >> 16);
            expect_word(ObservationOffset + 88, kept_far >> 16);
            expect_word(ObservationOffset + 92, kept_marker >> 16);
        }

        // Prove W really changed current B before the formatted reader restores I.
        initialize(raw_probe, {
            0xe0300000, 0x80000400, 0xe0300004, 0x80000500,
            0xe0300100, 0x80000700, 0xbf8c0000,
            0xe0700044, 0x80010400, 0xe0700048, 0x80010500,
            0xe070004c, 0x80010700, 0xbf810000
        }, 0x524556414c000006ULL + identity);
        auto& raw_program = bind(raw_probe, 8);
        std::memcpy(raw_program.user_data.data(), &target, sizeof(target));
        std::memcpy(raw_program.user_data.data() + 4, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{target, false, false}, {output, true, false}});
        rasterizer.DispatchDirect();
        expect_word(ObservationOffset + 68, interior_writer ? buffer_value : FinalValue);
        expect_word(ObservationOffset + 72, interior_writer ? FinalValue : initial(1, 0));
        expect_word(ObservationOffset + 76, MarkerValue);
        if (!rasterizer.IsGpuBufferModified(target.base_address, target.GetSize()) ||
            std::memcmp(backing, physical_before.data(), AllocationBytes))
            throw std::runtime_error("Readonly revalidation retired B before its formatted/image observations");

        initialize(formatted, {
            0xe0000000, 0x80000400, 0xe0000004, 0x80000500,
            0xe0000000 | u32(far_offset), 0x80000600, 0xe0000100, 0x80000700, 0xbf8c0000,
            0xe0700020, 0x80010400, 0xe0700024, 0x80010500,
            0xe0700028, 0x80010600, 0xe070002c, 0x80010700, 0xbf810000
        }, 0x524556414c000007ULL + identity);
        auto& formatted_program = bind(formatted, 8);
        std::memcpy(formatted_program.user_data.data(), &target, sizeof(target));
        std::memcpy(formatted_program.user_data.data() + 4, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{target, false, true}, {output, true, false}});
        rasterizer.DispatchDirect();
        expect_word(ObservationOffset + 32, kept_first);
        expect_word(ObservationOffset + 36, kept_second);
        expect_word(ObservationOffset + 40, kept_far);
        expect_word(ObservationOffset + 44, kept_marker);
        if (!rasterizer.IsGpuBufferModified(target.base_address, target.GetSize()) ||
            std::memcmp(backing, physical_before.data(), AllocationBytes))
            throw std::runtime_error("Readonly revalidation formatted retile published readonly bytes early");

        std::vector<u32> after_words{
            0x7e000280, 0x7e020280, image_load, 0x00000400,
            0x7e000281, image_load, second_reg << 8,
            0x7e000280, 0x7e020288, image_load, far_reg << 8,
            0x7e000288, 0x7e020280, image_load, marker_reg << 8, 0xbf8c0000,
            0xe0700030, 0x80020400, 0xe0700034, 0x80020000 | (second_reg << 8),
            0xe0700038, 0x80020000 | (far_reg << 8), 0xe070003c, 0x80020000 | (marker_reg << 8)
        };
        if (pair_uint) after_words.insert(after_words.end(), {
            0xe0700060, 0x80020500, 0xe0700064, 0x80020700,
            0xe0700068, 0x80020900, 0xe070006c, 0x80020b00
        });
        after_words.push_back(0xbf810000);
        initialize(after_probe, after_words, 0x524556414c000008ULL + identity);
        auto& after_program = bind(after_probe, 12);
        std::memcpy(after_program.user_data.data(), &sampled_image, sizeof(sampled_image));
        std::memcpy(after_program.user_data.data() + 8, &output, sizeof(output));
        verify(reflection.BuildCompute(gpu), {{output, true, false}}, &sampled_image);
        rasterizer.DispatchDirect();
        if (!packed_sample) {
            expect_word(ObservationOffset + 48, image_component(kept_first));
            expect_word(ObservationOffset + 52, image_component(kept_second));
            expect_word(ObservationOffset + 56, image_component(kept_far));
            expect_word(ObservationOffset + 60, image_component(kept_marker));
        }
        if (pair_uint) {
            expect_word(ObservationOffset + 96, kept_first >> 16);
            expect_word(ObservationOffset + 100, kept_second >> 16);
            expect_word(ObservationOffset + 104, kept_far >> 16);
            expect_word(ObservationOffset + 108, kept_marker >> 16);
        }
        // Retile is readonly: publish only the exact W and marker dirty union.
        if (interior_writer) expect_word(ImageOffset + 4, ImageNext);
        expect_word(ImageOffset + 256, kept_marker);
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.Finish();
        if (packed_sample) {
            // UNORM loads return floats. Validate a narrow numerical tolerance
            // before accepting only these eight observed output DWORDs; every
            // formatted packed DWORD and every physical/padding byte stays exact.
            const std::array<u32, 4> packed{kept_first, kept_second, kept_far, kept_marker};
            for (const u64 output_offset : {16u, 48u}) for (u32 index = 0; index < packed.size(); ++index) {
                const u64 offset = ObservationOffset + output_offset + index * 4;
                u32 observed{}; float value{};
                std::memcpy(&observed, backing + offset, sizeof(observed));
                std::memcpy(&value, &observed, sizeof(value));
                const float wanted = float(packed[index] & 255u) / 255.0f;
                if (!std::isfinite(value) || std::abs(value - wanted) > 0.000001f)
                    throw std::runtime_error("Readonly packed conversion did not preserve old I within UNORM tolerance");
                expect_word(offset, observed);
            }
        }
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Readonly revalidation image history, prewrite snapshot or full physical oracle mismatch");
        std::printf("PASS: readonly revalidation %s; real GCN, live B1 probe, full 64-KiB physical/padding oracle\n",
            pair_uint ? "interior W preserves R32 I through exact R16G16_UINT components" :
            packed_sample ? "interior W preserves packed I through RGBA8_UNORM sampling" :
            interior_writer ? "interior W preserves old I" : "exact-base W refreshes prewrite B0 into I");
    }
    rasterizer.Finish();
    drain.armed = false;
}
} // namespace NativeDX12
