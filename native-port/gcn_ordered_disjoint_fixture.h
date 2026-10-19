// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_rasterizer.h"
#include "video_core/amdgpu/liverpool.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {

// Authored real-GCN regression coverage; execution is a separate gated step.
// Four guest sharps exercise two independent image domains in one command.
inline void RunOrderedDisjointGcnFixture(GuestRasterizer& rasterizer, AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile, std::byte* memory, std::byte* backing, u64 base) {
    constexpr u64 AllocationBytes = 65536, ObservationOffset = 57344;
    constexpr std::array<u64, 2> ImageOffsets{16384, 32768};
    constexpr std::array<u32, 2> BufferValues{0x13579bdf, 0x9b7d531f};
    constexpr std::array<u32, 2> ImageValues{0x2468ace0, 0xeac86420};
    constexpr std::array<u32, 2> ImageNext{0x6a5b4c3d, 0x3c4d5e6f};
    constexpr std::array<u32, 2> ImageFar{0x3142a6b7, 0x7b6a2413};
    constexpr std::array<u32, 2> FinalValues{0x4567a9cd, 0xcda97654};
    struct alignas(256) Code { std::array<u32, 128> words{}; };
    std::array<Code, 2> seed{}, before_probe{}, after_probe{};
    Code ordered{};
    GuestShaderFrontend reflection(profile);
    // Declared after all code storage, so error unwinding drains GPU work before
    // the guest program arrays disappear, including failed precondition checks.
    struct Drain {
        GuestRasterizer& rasterizer;
        bool armed = true;
        ~Drain() { if (armed) try { rasterizer.Finish(); } catch (...) {} }
    } drain{rasterizer};
    const auto initialize = [](Code& code, const std::vector<u32>& words, u64 hash) {
        constexpr u32 MetadataWord = 96;
        if (words.size() > MetadataWord ||
            MetadataWord * sizeof(u32) + sizeof(AmdGpu::BinaryInfo) > sizeof(code.words))
            throw std::runtime_error("Disjoint GCN code overlaps binary metadata");
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
        // Exactly one invocation; initial and repeated loads have no wave race.
        program.num_thread_x.full = program.num_thread_y.full = program.num_thread_z.full = 1;
        program.dim_x = program.dim_y = program.dim_z = 1;
        program.settings.num_user_regs = user_registers;
        program.settings.num_vgprs = 3;
        program.settings.num_sgprs = 3;
        return program;
    };
    const auto initial = [](u32 domain, u32 x, u32 y) {
        return (domain ? 0x708090a0u : 0x10203040u) ^ (y << 12) ^ x;
    };
    for (const bool writer_first_b : {false, true}) {
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.InvalidateMemory(base, AllocationBytes);
        std::memset(memory, writer_first_b ? 0xb6 : 0xa7, AllocationBytes);
        std::array<AmdGpu::Image, 2> images{};
        std::array<AmdGpu::Buffer, 2> targets{};
        std::array<u64, 2> far_offsets{};
        for (u32 domain = 0; domain < 2; ++domain) {
            auto& image = images[domain];
            image.base_address = (base + ImageOffsets[domain]) >> 8;
            image.type = u64(AmdGpu::ImageType::Color2D);
            image.data_format = 4;
            image.num_format = 4; // R32_UINT, exact bits in both resource domains.
            image.tiling_index = 5;
            image.width = 23; image.height = 31; image.pitch = 31;
            image.dst_sel_x = 4; image.dst_sel_y = 5; image.dst_sel_z = 6; image.dst_sel_w = 7;
            const auto desc = DescribeGuestTexture(image);
            const auto layout = LayoutGuestTexture(desc);
            if (base % 4096 || layout.guest_bytes != 4096 ||
                layout.guest_bytes <= u64(desc.width) * desc.height * sizeof(u32) ||
                ImageOffsets[0] + layout.guest_bytes > ImageOffsets[1] ||
                ImageOffsets[1] + layout.guest_bytes > ObservationOffset)
                throw std::runtime_error("Disjoint GCN fixture needs two disjoint aligned padded 4-KiB images");
            constexpr std::array<std::array<u32, 2>, 4> ObservationCoordinates{{
                {0, 0}, {1, 0}, {0, 1}, {1, 1}}};
            for (u32 index = 0; index < ObservationCoordinates.size(); ++index) {
                const auto [x, y] = ObservationCoordinates[index];
                if (GuestPixelOffset(desc, layout, 0, 0, x, y) != index * 4)
                    throw std::runtime_error("Disjoint GCN fixture's fixed observation texels changed");
            }
            far_offsets[domain] = GuestPixelOffset(desc, layout, 0, 0, 0, 8);
            if (far_offsets[domain] < 16 || far_offsets[domain] + 4 > layout.guest_bytes)
                throw std::runtime_error("Disjoint GCN fixture lost its untouched image-tail texel");
            for (u32 y = 0; y < desc.height; ++y) for (u32 x = 0; x < desc.width; ++x) {
                const auto value = initial(domain, x, y);
                std::memcpy(memory + ImageOffsets[domain] + GuestPixelOffset(desc, layout, 0, 0, x, y),
                            &value, sizeof(value));
            }
            auto& target = targets[domain];
            target.base_address = base + ImageOffsets[domain];
            target.stride = 4; target.num_records = u32(layout.guest_bytes / 4);
            target.data_format = 4; target.num_format = 4;
            target.dst_sel_x = 4; target.dst_sel_y = 5; target.dst_sel_z = 6; target.dst_sel_w = 7;
        }
        auto observation = targets[0];
        observation.base_address = base + ObservationOffset;
        std::vector<std::byte> expected(backing, backing + AllocationBytes);
        const auto expect_word = [&](u64 offset, u32 value) {
            std::memcpy(expected.data() + offset, &value, sizeof(value));
        };
        const u64 identity = u64(writer_first_b) << 8;
        for (u32 domain = 0; domain < 2; ++domain) {
            // A full-page raw writer first establishes resident, GPU-modified B.
            // Subsequent image stores preserve an independent I without clearing
            // B's dirty precondition. Small clean readers would take a private
            // CPU snapshot and would not exercise two RetileToBuffer captures.
            initialize(seed[domain], {
                0x7e000280, 0x7e0202ff, BufferValues[domain],
                0xe0700000, 0x80000100,
                0x7e040280, 0x7e060280, 0x7e1002ff, ImageValues[domain],
                0xf0200100, 0x00010802,
                0x7e040281, 0x7e1002ff, ImageNext[domain],
                0xf0200100, 0x00010802,
                0x7e040280, 0x7e060288, 0x7e1002ff, ImageFar[domain],
                0xf0200100, 0x00010802, 0xbf810000
            }, 0x4449535345450000ULL + identity + domain);
            auto& program = bind(seed[domain], 12);
            std::memcpy(program.user_data.data(), &targets[domain], sizeof(AmdGpu::Buffer));
            std::memcpy(program.user_data.data() + 4, &images[domain], sizeof(AmdGpu::Image));
            rasterizer.DispatchDirect();
        }
        const auto require_target_ownership = [&](const char* phase) {
            for (u32 domain = 0; domain < 2; ++domain) {
                const Rasterizer::ShaderBufferRequest request{
                    targets[domain].base_address, targets[domain].GetSize(), true, true};
                const bool dirty = rasterizer.IsGpuBufferModified(request.address, request.size);
                const bool owned = rasterizer.BuffersMayDefer(std::span(&request, 1));
                if (!dirty || !owned) {
                    std::fprintf(stderr, "Disjoint %s domain=%u dirty=%u owned=%u\n", phase,
                                 domain, dirty, owned);
                    throw std::runtime_error("Disjoint GCN target lost dirty ownership");
                }
            }
        };
        require_target_ownership("after-seed");
        for (u32 domain = 0; domain < 2; ++domain) {
            // Read B raw and I through actual image instructions, publishing only
            // to the separate observation page. No target CPU readback occurs.
            const u32 output = domain * 64;
            initialize(before_probe[domain], {
                0x7e000280, 0x7e020280, 0xe0300000, 0x80000400,
                0xf0000100, 0x00010500,
                0x7e000281, 0xf0000100, 0x00010600,
                0x7e000280, 0x7e020288, 0xf0000100, 0x00010700,
                0xbf8c0000,
                0xe0700000 | output, 0x80030400,
                0xe0700000 | (output + 4), 0x80030500,
                0xe0700000 | (output + 8), 0x80030600,
                0xe0700000 | (output + 12), 0x80030700, 0xbf810000
            }, 0x4449534245460000ULL + identity + domain);
            auto& program = bind(before_probe[domain], 16);
            std::memcpy(program.user_data.data(), &targets[domain], sizeof(AmdGpu::Buffer));
            std::memcpy(program.user_data.data() + 4, &images[domain], sizeof(AmdGpu::Image));
            std::memcpy(program.user_data.data() + 12, &observation, sizeof(observation));
            rasterizer.DispatchDirect();
            require_target_ownership(domain ? "after-probe-1" : "after-probe-0");
            expect_word(ObservationOffset + output, BufferValues[domain]);
            expect_word(ObservationOffset + output + 4, ImageValues[domain]);
            expect_word(ObservationOffset + output + 8, ImageNext[domain]);
            expect_word(ObservationOffset + output + 12, ImageFar[domain]);
        }
        rasterizer.ReadMemory(observation.base_address, observation.GetSize());
        rasterizer.Finish();
        const bool physical_match = std::memcmp(backing, expected.data(), AllocationBytes) == 0;
        const bool first_dirty =
            rasterizer.IsGpuBufferModified(targets[0].base_address, targets[0].GetSize());
        const bool second_dirty =
            rasterizer.IsGpuBufferModified(targets[1].base_address, targets[1].GetSize());
        if (!physical_match || !first_dirty || !second_dirty) {
            size_t mismatch = 0;
            while (mismatch < AllocationBytes && backing[mismatch] == expected[mismatch]) ++mismatch;
            std::fprintf(stderr,
                         "Disjoint seed physical=%u first_dirty=%u second_dirty=%u mismatch=%zu "
                         "actual=%02x expected=%02x\n",
                         physical_match, first_dirty, second_dirty, mismatch,
                         mismatch < AllocationBytes ? std::to_integer<unsigned>(backing[mismatch]) : 0,
                         mismatch < AllocationBytes
                             ? std::to_integer<unsigned>(expected[mismatch])
                             : 0);
            throw std::runtime_error("Disjoint GCN seed lost independent dirty B/I or physical/padding preconditions");
        }

        // SGPR groups deliberately differ from first-use order: RA=s[4:7],
        // WA=s[0:3], RB=s[12:15], WB=s[8:11]. Only four guest sharps (16 SGPRs).
        // Observations are stored through their own W, keeping all initial loads
        // live. The same R sharp is loaded again after W's actual final store.
        std::vector<u32> words{
            0xe0000000, 0x80010400, // RA formatted -> v4
            0xe0300000, 0x80000500  // WA raw -> v5
        };
        const std::array<u32, 2> rb{0xe0000000, 0x80030600};
        const std::array<u32, 2> wb{0xe0300000, 0x80020700};
        const auto append = [&](const auto& pair) { words.insert(words.end(), pair.begin(), pair.end()); };
        append(writer_first_b ? wb : rb);
        append(writer_first_b ? rb : wb);
        words.insert(words.end(), {
            0xbf8c0000,
            0x7e1002ff, FinalValues[0], 0xe0700000, 0x80000800,
            0x7e1202ff, FinalValues[1], 0xe0700000, 0x80020900,
            0xbf8c0000,
            0xe0000000, 0x80010a00, // same RA -> v10
            0xe0000000, 0x80030b00, // same RB -> v11
            0xbf8c0000,
            0xe0700004, 0x80000400, 0xe0700008, 0x80000500,
            0xe070000c, 0x80000a00,
            0xe0700004, 0x80020600, 0xe0700008, 0x80020700,
            0xe070000c, 0x80020b00, 0xbf810000
        });
        initialize(ordered, words, 0x4449534f52440000ULL + identity);
        auto& command = bind(ordered, 16);
        std::memcpy(command.user_data.data(), &targets[0], sizeof(AmdGpu::Buffer));
        std::memcpy(command.user_data.data() + 4, &targets[0], sizeof(AmdGpu::Buffer));
        std::memcpy(command.user_data.data() + 8, &targets[1], sizeof(AmdGpu::Buffer));
        std::memcpy(command.user_data.data() + 12, &targets[1], sizeof(AmdGpu::Buffer));
        const auto& stage = reflection.BuildCompute(gpu);
        if (!stage.info.images.empty() || stage.info.uses_dma)
            throw std::runtime_error("Disjoint GCN command unexpectedly has image or DMA operands");
        std::vector<const Shader::BufferResource*> buffers;
        for (const auto& buffer : stage.info.buffers)
            if (!buffer.IsSpecial()) buffers.push_back(&buffer);
        if (buffers.size() != 4)
            throw std::runtime_error("Disjoint GCN frontend merged or removed a guest sharp");
        for (u32 index = 0; index < buffers.size(); ++index) {
            const auto& buffer = *buffers[index];
            const auto sharp = buffer.GetSharp(stage.info);
            const u32 domain = index / 2;
            const bool reader = index < 2 ? index == 0 : (index == 2) != writer_first_b;
            if (sharp.base_address != targets[domain].base_address ||
                sharp.GetSize() != targets[domain].GetSize() ||
                buffer.is_written == reader || buffer.is_formatted != reader)
                throw std::runtime_error("Actual GCN reflection lost requested disjoint first-use R/W order");
        }
        rasterizer.DispatchDirect();
        if (!rasterizer.IsGpuBufferModified(targets[0].base_address, targets[0].GetSize()) ||
            !rasterizer.IsGpuBufferModified(targets[1].base_address, targets[1].GetSize()) ||
            std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Disjoint GCN command retired before its GPU-only postwrite image observations");
        for (u32 domain = 0; domain < 2; ++domain) {
            const bool retiled = domain == 0 || !writer_first_b;
            const u32 loaded = retiled ? ImageValues[domain] : BufferValues[domain];
            expect_word(ImageOffsets[domain], FinalValues[domain]);
            expect_word(ImageOffsets[domain] + 4, loaded);
            expect_word(ImageOffsets[domain] + 8, loaded);
            expect_word(ImageOffsets[domain] + 12, FinalValues[domain]);
            if (retiled) expect_word(ImageOffsets[domain] + far_offsets[domain], ImageFar[domain]);
            // The post-command image fetch must see W's new first four texels
            // and retained/overwritten tail before any CPU target readback.
            const u32 output = domain * 64 + 16;
            initialize(after_probe[domain], {
                0x7e000280, 0x7e020280, 0xf0000100, 0x00000400,
                0x7e000281, 0xf0000100, 0x00000500,
                0x7e000280, 0x7e020281, 0xf0000100, 0x00000600,
                0x7e000281, 0xf0000100, 0x00000700,
                0x7e000280, 0x7e020288, 0xf0000100, 0x00000800,
                0xbf8c0000,
                0xe0700000 | output, 0x80020400,
                0xe0700000 | (output + 4), 0x80020500,
                0xe0700000 | (output + 8), 0x80020600,
                0xe0700000 | (output + 12), 0x80020700,
                0xe0700000 | (output + 16), 0x80020800, 0xbf810000
            }, 0x4449534146540000ULL + identity + domain);
            auto& program = bind(after_probe[domain], 12);
            std::memcpy(program.user_data.data(), &images[domain], sizeof(AmdGpu::Image));
            std::memcpy(program.user_data.data() + 8, &observation, sizeof(observation));
            rasterizer.DispatchDirect();
            expect_word(ObservationOffset + output, FinalValues[domain]);
            expect_word(ObservationOffset + output + 4, loaded);
            expect_word(ObservationOffset + output + 8, loaded);
            expect_word(ObservationOffset + output + 12, FinalValues[domain]);
            expect_word(ObservationOffset + output + 16,
                        retiled ? ImageFar[domain] : initial(domain, 0, 8));
        }
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.Finish();
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Disjoint GCN initial/repeated loads, final images or full physical/padding oracle mismatch");
        std::printf("PASS: ordered-disjoint RA/WA/%s; two dirty 4-KiB domains, repeated same-R aliases, full 64-KiB oracle\n",
                    writer_first_b ? "WB/RB" : "RB/WB");
    }
    rasterizer.Finish();
    drain.armed = false;
}
} // namespace NativeDX12
