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
        // Exactly one invocation: no inter-thread alias race enters the oracle.
        program.num_thread_x.full = program.num_thread_y.full = program.num_thread_z.full = 1;
        program.dim_x = program.dim_y = program.dim_z = 1;
        program.settings.num_user_regs = user_registers;
        program.settings.num_vgprs = 3;
        program.settings.num_sgprs = 3;
        return program;
    };
    const auto initial = [](u32 x, u32 y) { return 0x10203040u ^ (y << 12) ^ x; };
    enum class Sequence { ReadWrite, WriteRead, RawWriteRead, ReadWriteRead };
    for (const auto sequence : {Sequence::ReadWrite, Sequence::WriteRead,
                               Sequence::RawWriteRead, Sequence::ReadWriteRead})
    for (const bool small : {false, true})
    for (const bool clean : {false, true}) {
        const bool read_first = sequence != Sequence::WriteRead;
        const bool repeated = sequence == Sequence::RawWriteRead || sequence == Sequence::ReadWriteRead;
        // The original eight cases already cover the size threshold. Keep the
        // new store/reread cases focused on small private versus resident views.
        if (repeated && !small) continue;
        const bool early_formatted = sequence != Sequence::RawWriteRead;
        const bool private_early = small && clean && read_first;
        const bool image_tiled = read_first && early_formatted && !private_early;
        const bool partial_output = small && clean && sequence == Sequence::ReadWriteRead;
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
        // Seed/probe commands always use a full protected page. The last case
        // gives the tested command a separate fresh, partial-page output below.
        observation.num_records = 4096 / 4;
        const u64 identity = (u64(sequence) << 12) | (u64(small) << 8) | u64(clean);
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
        // Put the FIRST used descriptor in s[4:7] in every case. The writer's raw
        // load is observed, so it cannot disappear through dead-code removal.
        const u32 read_group = read_first ? 1 : 0, write_group = read_first ? 0 : 1;
        const u32 output_group = repeated ? 3 : 2;
        const u32 early_opcode = early_formatted ? 0xe0000000 : 0xe0300000;
        auto command_output = observation;
        if (partial_output) {
            // Do not reuse the full-page observation's resident allocation:
            // doing so could acquire ownership for a small sharp and hide the
            // synchronous fallback. This page has no earlier GPU binding.
            command_output.base_address = base + ObservationOffset + 4096;
            command_output.num_records = 64 / 4;
        }
        const u64 command_output_offset = command_output.base_address - base;
        const std::array<u32, 2> early_load{early_opcode, 0x80000400 | (read_group << 16)};
        const std::array<u32, 2> raw_load{0xe0300000, 0x80000500 | (write_group << 16)};
        std::vector<u32> words;
        const auto append = [&](const auto& pair) { words.insert(words.end(), pair.begin(), pair.end()); };
        append(read_first ? early_load : raw_load);
        append(read_first ? raw_load : early_load);
        words.push_back(0xbf8c0000); // s_waitcnt before the actual store
        if (repeated) {
            words.insert(words.end(), {
                0x7e1002ff, FinalValue, 0xe0700000, 0x80000800 | (write_group << 16),
                0xbf8c0000,
                // Reload the SAME early descriptor after W changed the arena.
                // A private CPU snapshot stays B; a resident alias sees Final.
                early_opcode, 0x80000600 | (read_group << 16),
                0xe0000000, 0x80020700, // distinct late formatted R, s[8:11]
                0xbf8c0000
            });
        }
        words.insert(words.end(), {
            0xe0700010, 0x80000400 | (output_group << 16),
            0xe0700014, 0x80000500 | (output_group << 16)
        });
        if (repeated) {
            words.insert(words.end(), {
                0xe0700018, 0x80000600 | (output_group << 16),
                0xe070001c, 0x80000700 | (output_group << 16)
            });
        } else {
            words.insert(words.end(), {
                0x7e1002ff, FinalValue, 0xe0700000, 0x80000800 | (write_group << 16)
            });
        }
        words.push_back(0xbf810000);
        initialize(ordered, words, 0x4f52444f52440000ULL + identity);
        auto& ordered_program = bind(ordered, repeated ? 16 : 12);
        std::memcpy(ordered_program.user_data.data(), &target, sizeof(target));
        std::memcpy(ordered_program.user_data.data() + 4, &target, sizeof(target));
        if (repeated) std::memcpy(ordered_program.user_data.data() + 8, &target, sizeof(target));
        std::memcpy(ordered_program.user_data.data() + output_group * 4, &command_output, sizeof(command_output));
        const auto& stage = reflection.BuildCompute(gpu);
        if (!stage.info.images.empty() || stage.info.uses_dma)
            throw std::runtime_error("Ordered GCN command unexpectedly has image or DMA operands");
        std::vector<const Shader::BufferResource*> guest_buffers;
        for (const auto& buffer : stage.info.buffers)
            if (!buffer.IsSpecial()) guest_buffers.push_back(&buffer);
        if (guest_buffers.size() != (repeated ? 4u : 3u))
            throw std::runtime_error("Ordered GCN descriptor collection unexpectedly merged or removed resources");
        for (size_t index = 0; index < guest_buffers.size(); ++index) {
            const auto& buffer = *guest_buffers[index];
            const auto sharp = buffer.GetSharp(stage.info);
            const bool early_reader = index == (read_first ? 0u : 1u);
            const bool late_reader = repeated && index == 2;
            const bool reader = early_reader || late_reader;
            const bool formatted = late_reader || (early_reader && early_formatted);
            const bool output = index == guest_buffers.size() - 1;
            const auto address = output ? command_output.base_address : target.base_address;
            const auto bytes = output ? command_output.GetSize() : layout.guest_bytes;
            if (sharp.base_address != address || sharp.GetSize() != bytes ||
                buffer.is_written == reader || buffer.is_formatted != formatted)
                throw std::runtime_error("Actual translated buffer collection does not have the requested R/W order");
        }
        rasterizer.DispatchDirect();
        const auto loaded = image_tiled ? ImageValue : BufferValue;
        const auto next = image_tiled ? ImageNext : initial(1, 0);
        expect_word(command_output_offset + 16, loaded);
        expect_word(command_output_offset + 20, loaded);
        if (repeated) {
            expect_word(command_output_offset + 24, private_early ? BufferValue : FinalValue);
            expect_word(command_output_offset + 28, FinalValue);
        }
        if (partial_output) {
            // Without asking for a readback, the unowned output must already be
            // CPU-visible when the synchronous dispatch fallback returns.
            if (rasterizer.IsGpuBufferModified(command_output.base_address, command_output.GetSize()) ||
                std::memcmp(backing + command_output_offset, expected.data() + command_output_offset,
                            size_t(command_output.GetSize())))
                throw std::runtime_error("Partial ordered-stream observation did not retire synchronously");
        }

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
        expect_word(ImageOffset + first, FinalValue);
        if (image_tiled) expect_word(ImageOffset + second, ImageNext);
        expect_word(ObservationOffset + 32, FinalValue);
        expect_word(ObservationOffset + 36, next);
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.Finish();
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Ordered GCN observation, final buffer, image reload or physical/padding oracle mismatch");
        const char* sequence_name = sequence == Sequence::ReadWrite ? "R/W" :
            sequence == Sequence::WriteRead ? "W/R" :
            sequence == Sequence::RawWriteRead ? "rawR/W/formattedR" : "formattedR/W/formattedR";
        std::printf("PASS: ordered-formatted %s %s %s%s bytes=%llu; real GCN, full physical oracle\n",
            small ? "small" : "large", clean ? "CPU-clean" : "GPU-modified",
            sequence_name, partial_output ? " partial-output sync fallback" : "",
            static_cast<unsigned long long>(layout.guest_bytes));
    }
    rasterizer.Finish();
    drain.armed = false;
}
} // namespace NativeDX12
