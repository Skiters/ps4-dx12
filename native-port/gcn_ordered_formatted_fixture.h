// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace NativeDX12 {

// Authored, unrun real-GCN coverage. The tested dispatch has no image operands;
// separate seed/probe dispatches establish and inspect the two resource domains.
inline void RunOrderedFormattedGcnFixture(GuestRasterizer& rasterizer, AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile, std::byte* memory, std::byte* backing, u64 base) {
    constexpr u64 AllocationBytes = 65536, ImageOffset = 16384, ObservationOffset = 57344;
    constexpr u32 BufferValue = 0x13579bdf, ImageValue = 0x2468ace0;
    constexpr u32 ImageNext = 0x6a5b4c3d, FinalValue = 0x4567a9cd;
    alignas(256) std::array<u32, 128> seed{}, before_probe{}, ordered{}, after_probe{};
    GuestShaderFrontend reflection(profile);
    struct Drain {
        GuestRasterizer& rasterizer;
        bool armed = true;
        ~Drain() { if (armed) try { rasterizer.Finish(); } catch (...) {} }
    } drain{rasterizer};
    const auto initialize = [](auto& program, const std::vector<u32>& words, u64 hash) {
        if (words.size() > 32) throw std::runtime_error("Ordered GCN code overlaps binary metadata");
        program.fill(0);
        std::copy(words.begin(), words.end(), program.begin());
        AmdGpu::BinaryInfo binary{};
        binary.signature = binary.signature_ref;
        binary.length = u32(words.size() * sizeof(u32));
        binary.shader_hash = hash;
        std::memcpy(program.data() + 32, &binary, sizeof(binary));
    };
    const auto bind = [&](const auto& code, u32 user_registers) -> AmdGpu::ComputeProgram& {
        auto& program = gpu.GetCsRegs();
        program = {};
        program.address = reinterpret_cast<u64>(code.data()) >> 8;
        // Exactly one invocation: both alias loads finish before the final store.
        program.num_thread_x.full = program.num_thread_y.full = program.num_thread_z.full = 1;
        program.dim_x = program.dim_y = program.dim_z = 1;
        program.settings.num_user_regs = user_registers;
        program.settings.num_vgprs = 3;
        program.settings.num_sgprs = 3;
        return program;
    };
    const auto initial = [](u32 x, u32 y) { return 0x10203040u ^ (y << 12) ^ x; };
    for (const bool small : {false, true}) for (const bool read_first : {true, false})
    for (const bool clean : {false, true}) {
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.InvalidateMemory(base, AllocationBytes);
        std::memset(memory, small ? 0xb6 : 0xa7, AllocationBytes);
        AmdGpu::Image image{};
        image.base_address = (base + ImageOffset) >> 8;
        image.type = u64(AmdGpu::ImageType::Color2D);
        image.data_format = 4;
        image.num_format = 4; // R32_UINT, exact bits through image and formatted buffer loads.
        image.tiling_index = 5;
        image.width = small ? 23 : 119;
        image.height = small ? 31 : 47;
        image.pitch = small ? 31 : 127;
        image.dst_sel_x = 4; image.dst_sel_y = 5; image.dst_sel_z = 6; image.dst_sel_w = 7;
        const auto desc = DescribeGuestTexture(image);
        const auto layout = LayoutGuestTexture(desc);
        if (!layout.guest_bytes || layout.guest_bytes % 4096 ||
            ImageOffset + layout.guest_bytes > ObservationOffset ||
            (small ? layout.guest_bytes > 16384 : layout.guest_bytes <= 16384) ||
            layout.guest_bytes <= u64(desc.width) * desc.height * sizeof(u32))
            throw std::runtime_error("Ordered GCN fixture requires full page-aligned range and real tile padding");
        const auto first = GuestPixelOffset(desc, layout, 0, 0, 0, 0);
        const auto second = GuestPixelOffset(desc, layout, 0, 0, 1, 0);
        if (first != 0 || second != 4)
            throw std::runtime_error("Ordered GCN fixture's fixed first two texels changed");
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
        target.stride = 4;
        target.num_records = u32(layout.guest_bytes / 4);
        target.data_format = 4; target.num_format = 4;
        target.dst_sel_x = 4; target.dst_sel_y = 5; target.dst_sel_z = 6; target.dst_sel_w = 7;
        auto observation = target;
        observation.base_address = base + ObservationOffset;
        // Full protected page permits deferred GPU ownership, even though only
        // seven isolated DWORDs are written over the whole scenario.
        observation.num_records = 4096 / 4;
        const u64 identity = (u64(small) << 8) | (u64(read_first) << 1) | u64(clean);
        initialize(seed, {
            0x7e000280, 0x7e0202ff, BufferValue,
            0xe0700000, 0x80000100, // raw store v1, s[0:3]
            0x7e040280, 0x7e060280, 0x7e1002ff, ImageValue,
            0xf0200100, 0x00010802, // image_store v8, coordinates v[2:3], s[4:11]
            0x7e040281, 0x7e1002ff, ImageNext,
            0xf0200100, 0x00010802, 0xbf810000
        }, 0x4f52445345450000ULL + identity);
        auto& seed_program = bind(seed, 12);
        std::memcpy(seed_program.user_data.data(), &target, sizeof(target));
        std::memcpy(seed_program.user_data.data() + 4, &image, sizeof(image));
        rasterizer.DispatchDirect();
        if (!rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes))
            throw std::runtime_error("Ordered GCN seed did not retain the modified buffer domain");

        // Prove B != I through guest reads before the command under test. These
        // are ordinary raw/image reads, not the formatted synchronization path.
        initialize(before_probe, {
            0x7e000280, 0x7e020280, 0xe0300000, 0x80000400,
            0xf0000100, 0x00010500, 0x7e000281,
            0xf0000100, 0x00010600, 0xbf8c0000,
            0xe0700000, 0x80030400, 0xe0700004, 0x80030500,
            0xe0700008, 0x80030600, 0xbf810000
        }, 0x4f52444245460000ULL + identity);
        auto& probe_program = bind(before_probe, 16);
        std::memcpy(probe_program.user_data.data(), &target, sizeof(target));
        std::memcpy(probe_program.user_data.data() + 4, &image, sizeof(image));
        std::memcpy(probe_program.user_data.data() + 12, &observation, sizeof(observation));
        rasterizer.DispatchDirect();
        rasterizer.ReadMemory(observation.base_address, 4096);
        rasterizer.Finish();
        expect_word(ObservationOffset, BufferValue);
        expect_word(ObservationOffset + 4, ImageValue);
        expect_word(ObservationOffset + 8, ImageNext);
        if (std::memcmp(backing, expected.data(), AllocationBytes) ||
            !rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes))
            throw std::runtime_error("Ordered GCN precondition lost independent image, buffer or physical padding");
        if (clean) {
            // A real CPU read downloads B, while ResolveMemory deliberately
            // leaves the independent image version I intact. No CPU write or
            // invalidation is used to manufacture the clean-buffer condition.
            rasterizer.ReadMemory(target.base_address, layout.guest_bytes);
            rasterizer.Finish();
            expect_word(ImageOffset + first, BufferValue);
            if (rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes) ||
                std::memcmp(backing, expected.data(), AllocationBytes))
                throw std::runtime_error("Ordered GCN CPU read did not establish the clean B domain");
        }

        // Resource discovery follows first live instruction use, not SGPR index.
        // Put the FIRST used descriptor in s[4:7] in BOTH cases. The writer's raw
        // load is observed, so it cannot disappear through dead-code removal.
        const u32 read_group = read_first ? 1 : 0, write_group = read_first ? 0 : 1;
        const std::array<u32, 2> formatted_load{0xe0000000, 0x80000400 | (read_group << 16)};
        const std::array<u32, 2> raw_load{0xe0300000, 0x80000500 | (write_group << 16)};
        std::vector<u32> words;
        const auto append = [&](const auto& pair) { words.insert(words.end(), pair.begin(), pair.end()); };
        append(read_first ? formatted_load : raw_load);
        append(read_first ? raw_load : formatted_load);
        words.insert(words.end(), {
            0xbf8c0000, // s_waitcnt: both values are captured before any store
            0xe0700010, 0x80020400, 0xe0700014, 0x80020500,
            0x7e0c02ff, FinalValue, 0xe0700000, 0x80000600 | (write_group << 16),
            0xbf810000
        });
        initialize(ordered, words, 0x4f52444f52440000ULL + identity);
        auto& ordered_program = bind(ordered, 12);
        std::memcpy(ordered_program.user_data.data(), &target, sizeof(target));
        std::memcpy(ordered_program.user_data.data() + 4, &target, sizeof(target));
        std::memcpy(ordered_program.user_data.data() + 8, &observation, sizeof(observation));
        const auto& stage = reflection.BuildCompute(gpu);
        if (!stage.info.images.empty() || stage.info.uses_dma)
            throw std::runtime_error("Ordered GCN command unexpectedly has image or DMA operands");
        std::vector<const Shader::BufferResource*> guest_buffers;
        for (const auto& buffer : stage.info.buffers)
            if (!buffer.IsSpecial()) guest_buffers.push_back(&buffer);
        if (guest_buffers.size() != 3)
            throw std::runtime_error("Ordered GCN descriptor collection unexpectedly merged or removed resources");
        for (size_t index = 0; index < 3; ++index) {
            const auto& buffer = *guest_buffers[index];
            const auto sharp = buffer.GetSharp(stage.info);
            const bool reader = index == (read_first ? 0u : 1u);
            const auto address = index == 2 ? observation.base_address : target.base_address;
            const auto bytes = index == 2 ? u64(4096) : layout.guest_bytes;
            if (sharp.base_address != address || sharp.GetSize() != bytes ||
                buffer.is_written == reader || buffer.is_formatted != reader)
                throw std::runtime_error("Actual translated buffer collection does not have the requested R/W order");
        }
        const bool must_reject = small && clean && read_first;
        bool rejected = false;
        try {
            rasterizer.DispatchDirect();
        } catch (const std::runtime_error& error) {
            if (!must_reject || std::string_view(error.what()) !=
                "Ordered formatted buffer/image writers require a unique full-range resident compute alias")
                throw;
            rejected = true;
        }
        if (rejected != must_reject)
            throw std::runtime_error("Small clean R/W command did not preserve its explicit streaming boundary");

        // Read the image only AFTER the buffer writer invalidated its old entry.
        // The first pixel detects a stale image; the second detects lost retile.
        initialize(after_probe, {
            0x7e000280, 0x7e020280, 0xf0000100, 0x00000400,
            0x7e000281, 0xf0000100, 0x00000500, 0xbf8c0000,
            0xe0700020, 0x80020400, 0xe0700024, 0x80020500, 0xbf810000
        }, 0x4f52444146540000ULL + identity);
        auto& after_program = bind(after_probe, 12);
        std::memcpy(after_program.user_data.data(), &image, sizeof(image));
        std::memcpy(after_program.user_data.data() + 8, &observation, sizeof(observation));
        rasterizer.DispatchDirect();
        const auto loaded = read_first ? ImageValue : BufferValue;
        const auto next = read_first ? ImageNext : initial(1, 0);
        if (!must_reject) {
            expect_word(ObservationOffset + 16, loaded);
            expect_word(ObservationOffset + 20, loaded); // aliases share one resident arena
            expect_word(ImageOffset + first, FinalValue);
            if (read_first) expect_word(ImageOffset + second, ImageNext);
        }
        // A rejected command must leave I alive, B unchanged, and the tested
        // observation DWORDs untouched. The later probe itself still executes.
        expect_word(ObservationOffset + 32, must_reject ? ImageValue : FinalValue);
        expect_word(ObservationOffset + 36, next);
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.Finish();
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Ordered GCN observation, final buffer, image reload or physical/padding oracle mismatch");
        std::printf("PASS: ordered-formatted %s %s %s%s bytes=%llu; real GCN, full physical oracle\n",
            small ? "small" : "large", clean ? "CPU-clean" : "GPU-modified",
            read_first ? "R/W" : "W/R", must_reject ? " rejected before mutation" : "",
            static_cast<unsigned long long>(layout.guest_bytes));
    }
    rasterizer.Finish();
    drain.armed = false;
}
} // namespace NativeDX12
