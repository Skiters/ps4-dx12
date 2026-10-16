// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "gcn_ordered_disjoint_fixture.h"
#include "gcn_readonly_revalidation_fixture.h"
#include "gcn_bc_block_expansion_fixture.h"
#include "gcn_bc_mip_expansion_fixture.h"
#include "gcn_bc_partial_expansion_fixture.h"
#include "gcn_bc_writable_alias_fixture.h"
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
    constexpr u32 ImageFar = 0x3142a6b7, TailWrite = 0x5c6d7e8f;
    constexpr u64 ShortBytes = 64, LongBytes = 20480;
    alignas(256) std::array<u32, 128> seed{}, before_probe{}, ordered{}, after_probe{};
    GuestShaderFrontend reflection(profile);
    struct Drain {
        GuestRasterizer& rasterizer;
        bool armed = true;
        ~Drain() { if (armed) try { rasterizer.Finish(); } catch (...) {} }
    } drain{rasterizer};
    const auto initialize = [](auto& program, const std::vector<u32>& words, u64 hash, u32 metadata_word = 32) {
        if (words.size() > metadata_word || metadata_word * sizeof(u32) + sizeof(AmdGpu::BinaryInfo) > sizeof(program))
            throw std::runtime_error("Ordered GCN code overlaps binary metadata");
        program.fill(0);
        std::copy(words.begin(), words.end(), program.begin());
        AmdGpu::BinaryInfo binary{};
        binary.signature = binary.signature_ref;
        binary.length = u32(words.size() * sizeof(u32));
        binary.shader_hash = hash;
        std::memcpy(program.data() + metadata_word, &binary, sizeof(binary));
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
    enum class Sequence { ReadWrite, WriteRead, RawWriteRead, ReadWriteRead,
        CleanShortDirtyTail, DirtyShort, CleanLong, MixedShortLong,
        ShortWriterFullRead, ShortWriterDirtyRead, ShortWriterPrivateRead,
        ShortWriterFirst, LongWriter, BindingOrderResident, OffsetWriterReadFirst, OffsetWriterFirst };
    for (const auto sequence : {Sequence::ReadWrite, Sequence::WriteRead,
                               Sequence::RawWriteRead, Sequence::ReadWriteRead,
                               Sequence::CleanShortDirtyTail, Sequence::DirtyShort,
                               Sequence::CleanLong, Sequence::MixedShortLong,
                               Sequence::ShortWriterFullRead, Sequence::ShortWriterDirtyRead,
                               Sequence::ShortWriterPrivateRead, Sequence::ShortWriterFirst,
                               Sequence::LongWriter, Sequence::BindingOrderResident,
                               Sequence::OffsetWriterReadFirst, Sequence::OffsetWriterFirst})
    for (const bool small_image : {false, true})
    for (const bool clean : {false, true})
    for (const u32 write_bytes : {4U, 1U, 2U}) {
        const bool short_views = sequence >= Sequence::CleanShortDirtyTail && sequence <= Sequence::MixedShortLong;
        const bool writer_cases = sequence >= Sequence::ShortWriterFullRead;
        const bool offset_writer = sequence == Sequence::OffsetWriterReadFirst || sequence == Sequence::OffsetWriterFirst;
        if (!offset_writer && write_bytes != 4) continue;
        const bool narrow_writer = offset_writer && write_bytes < 4;
        const u32 write_shift = narrow_writer ? write_bytes * 8 : 0;
        const u32 write_mask = write_bytes == 1 ? 0xffU : write_bytes == 2 ? 0xffffU : 0xffffffffU;
        const u32 physical_mask = write_mask << write_shift;
        const u32 gpu_far_word = (ImageFar & ~physical_mask) | ((FinalValue & write_mask) << write_shift);
        const u32 writer_load_opcode = write_bytes == 1 ? 0xe0200000 : write_bytes == 2 ? 0xe0280000 : 0xe0300000;
        const u32 writer_store_opcode = write_bytes == 1 ? 0xe0600000 : write_bytes == 2 ? 0xe0680000 : 0xe0700000;
        const bool binding_order = sequence == Sequence::BindingOrderResident;
        const bool long_writer = sequence == Sequence::LongWriter;
        const bool short_writer = writer_cases && !long_writer;
        const bool redirty_prefix = sequence == Sequence::ShortWriterDirtyRead;
        const bool dirty_tail = sequence == Sequence::CleanShortDirtyTail;
        const bool long_first = sequence == Sequence::CleanLong;
        const bool mixed = sequence == Sequence::MixedShortLong;
        if (short_views && (dirty_tail ? (small_image || !clean) :
            sequence == Sequence::DirtyShort ? (!small_image || clean) : (!small_image || !clean))) continue;
        if (!offset_writer && writer_cases && (sequence == Sequence::ShortWriterFullRead ? (small_image || !clean) :
            redirty_prefix ? (!small_image || clean) : (!small_image || !clean))) continue;
        const bool read_first = sequence != Sequence::WriteRead && sequence != Sequence::ShortWriterFirst &&
            sequence != Sequence::OffsetWriterFirst;
        const bool repeated = writer_cases || short_views || sequence == Sequence::RawWriteRead || sequence == Sequence::ReadWriteRead;
        // The original eight cases already cover the size threshold. Keep the
        // new store/reread cases focused on small private versus resident views.
        if (!short_views && !writer_cases && repeated && !small_image) continue;
        const bool early_formatted = sequence != Sequence::RawWriteRead;
        const bool private_early = offset_writer ? (small_image && clean && read_first) : writer_cases ? sequence == Sequence::ShortWriterPrivateRead :
            short_views ? (dirty_tail || mixed) : small_image && clean && read_first;
        const bool image_tiled = offset_writer || mixed || (read_first && early_formatted && !private_early);
        const bool has_long_tail = long_first || mixed || long_writer;
        const bool partial_output = small_image && clean && sequence == Sequence::ReadWriteRead;
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.InvalidateMemory(base, AllocationBytes);
        std::memset(memory, small_image ? 0xb6 : 0xa7, AllocationBytes);
        AmdGpu::Image image{};
        image.base_address = (base + ImageOffset) >> 8;
        image.type = u64(AmdGpu::ImageType::Color2D);
        image.data_format = 4;
        image.num_format = 4; // R32_UINT, exact bits through image and formatted buffer loads.
        image.tiling_index = 5;
        image.width = small_image ? 23 : 119;
        image.height = small_image ? 31 : 47;
        image.pitch = small_image ? 31 : 127;
        image.dst_sel_x = 4; image.dst_sel_y = 5; image.dst_sel_z = 6; image.dst_sel_w = 7;
        const auto desc = DescribeGuestTexture(image);
        const auto layout = LayoutGuestTexture(desc);
        if (!layout.guest_bytes || layout.guest_bytes % 4096 ||
            ImageOffset + layout.guest_bytes > ObservationOffset ||
            (small_image ? layout.guest_bytes > 16384 : layout.guest_bytes <= 16384) ||
            layout.guest_bytes <= u64(desc.width) * desc.height * sizeof(u32))
            throw std::runtime_error("Ordered GCN fixture requires full page-aligned range and real tile padding");
        const auto first = GuestPixelOffset(desc, layout, 0, 0, 0, 0);
        const auto second = GuestPixelOffset(desc, layout, 0, 0, 1, 0);
        const auto far_offset = GuestPixelOffset(desc, layout, 0, 0, 0, 8);
        if (first != 0 || second != 4)
            throw std::runtime_error("Ordered GCN fixture's fixed first two texels changed");
        for (u32 y = 0; y < desc.height; ++y) for (u32 x = 0; x < desc.width; ++x) {
            const auto value = initial(x, y);
            std::memcpy(memory + ImageOffset + GuestPixelOffset(desc, layout, 0, 0, x, y), &value, 4);
        }
        if ((short_views || writer_cases) && (far_offset < ShortBytes || far_offset + 4 > layout.guest_bytes))
            throw std::runtime_error("Short-view fixture needs an image texel outside its readonly prefix");
        if (has_long_tail) {
            if (layout.guest_bytes >= LongBytes || ImageOffset + LongBytes > ObservationOffset)
                throw std::runtime_error("Long readonly fixture lost its non-image tail");
            for (u64 offset = layout.guest_bytes; offset < LongBytes; offset += 4) {
                const u32 value = 0x8d732100u ^ u32(offset);
                std::memcpy(memory + ImageOffset + offset, &value, 4);
            }
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
        const u64 identity = (u64(sequence) << 12) | (u64(small_image) << 8) | (u64(write_bytes) << 1) | u64(clean);
        std::vector<u32> seed_words{
            0x7e000280, 0x7e0202ff, BufferValue,
            0xe0700000, 0x80000100, // raw store v1, s[0:3]
            0x7e040280, 0x7e060280, 0x7e1002ff, ImageValue,
            0xf0200100, 0x00010802, // image_store v8, coordinates v[2:3], s[4:11]
            0x7e040281, 0x7e1002ff, ImageNext,
            0xf0200100, 0x00010802
        };
        if (short_views || writer_cases) seed_words.insert(seed_words.end(), {
            0x7e040280, 0x7e060288, 0x7e1002ff, ImageFar,
            0xf0200100, 0x00010802
        });
        seed_words.push_back(0xbf810000);
        initialize(seed, seed_words, 0x4f52445345450000ULL + identity);
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
        if (clean || redirty_prefix) {
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
        if (binding_order) {
            // A real full-allocation raw read establishes a page-aligned cached
            // envelope. Later narrow sharps keep their original extents while
            // acquiring ownership of this envelope, so dispatch cannot retire
            // W_A synchronously before the GPU-only I-tail observation.
            auto warm = target;
            warm.base_address = base;
            warm.num_records = AllocationBytes / 4;
            initialize(seed, {
                0xe0300000, 0x80000400, 0xbf8c0000,
                0xe070000c, 0x80010400, 0xbf810000
            }, 0x4f52445741520000ULL + identity);
            auto& warm_program = bind(seed, 8);
            std::memcpy(warm_program.user_data.data(), &warm, sizeof(warm));
            std::memcpy(warm_program.user_data.data() + 4, &observation, sizeof(observation));
            const auto& warm_stage = reflection.BuildCompute(gpu);
            size_t warm_index{};
            if (!warm_stage.info.images.empty() || warm_stage.info.uses_dma)
                throw std::runtime_error("Binding-order warmup unexpectedly has image or DMA operands");
            for (const auto& buffer : warm_stage.info.buffers) if (!buffer.IsSpecial()) {
                const auto sharp = buffer.GetSharp(warm_stage.info);
                const auto& original = warm_index == 0 ? warm : observation;
                if (warm_index >= 2 || sharp.base_address != original.base_address ||
                    sharp.GetSize() != original.GetSize() || buffer.is_written != (warm_index == 1) ||
                    buffer.is_formatted)
                    throw std::runtime_error("Binding-order warmup lost its original raw 64-KiB envelope");
                ++warm_index;
            }
            if (warm_index != 2)
                throw std::runtime_error("Binding-order warmup lost a real guest resource");
            rasterizer.DispatchDirect();
            rasterizer.ReadMemory(observation.base_address, 4096);
            rasterizer.Finish();
            u32 warm_value{};
            std::memcpy(&warm_value, expected.data(), sizeof(warm_value));
            expect_word(ObservationOffset + 12, warm_value);
            if (base % 4096 || rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes + 4) ||
                std::memcmp(backing, expected.data(), AllocationBytes))
                throw std::runtime_error("Binding-order warmup changed I/B preconditions or left old dirty bytes");
        }
        if (redirty_prefix) {
            // Reconcile the original full seed writer FIRST. A fresh shifted
            // DWORD writer in a different 256-byte image-address bucket then
            // dirties query page 0, without making the entire image CPU-dirty.
            auto marker = target;
            marker.base_address += 256;
            marker.num_records = 1;
            initialize(seed, {0x7e0202ff, TailWrite, 0xe0700000, 0x80000100, 0xbf810000},
                0x4f52444d41520000ULL + identity);
            auto& marker_program = bind(seed, 4);
            std::memcpy(marker_program.user_data.data(), &marker, sizeof(marker));
            rasterizer.DispatchDirect();
            if (!rasterizer.IsGpuBufferModified(target.base_address, ShortBytes))
                throw std::runtime_error("Short-writer fixture did not establish the tiny prior dirty interval");
            auto& check_program = bind(before_probe, 16);
            std::memcpy(check_program.user_data.data(), &target, sizeof(target));
            std::memcpy(check_program.user_data.data() + 4, &image, sizeof(image));
            std::memcpy(check_program.user_data.data() + 12, &observation, sizeof(observation));
            rasterizer.DispatchDirect();
            rasterizer.ReadMemory(observation.base_address, 4096);
            rasterizer.Finish();
            if (std::memcmp(backing, expected.data(), AllocationBytes) ||
                !rasterizer.IsGpuBufferModified(target.base_address, ShortBytes))
                throw std::runtime_error("Tiny prior writer lost independent I or published unexpected physical bytes");
        }
        if (dirty_tail) {
            // A shifted, page-sized guest writer dirties only the second page.
            // It cannot invalidate the independent image at a different base.
            auto tail = target;
            tail.base_address += 4096;
            tail.num_records = 4096 / 4;
            initialize(seed, {0x7e0202ff, TailWrite, 0xe0700000, 0x80000100, 0xbf810000},
                0x4f52445441490000ULL + identity);
            auto& tail_program = bind(seed, 4);
            std::memcpy(tail_program.user_data.data(), &tail, sizeof(tail));
            rasterizer.DispatchDirect();
            if (rasterizer.IsGpuBufferModified(target.base_address, ShortBytes) ||
                !rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes))
                throw std::runtime_error("Short-prefix fixture did not isolate GPU modification to the tail page");
            // The prior independent image must survive the shifted writer.
            auto& check_program = bind(before_probe, 16);
            std::memcpy(check_program.user_data.data(), &target, sizeof(target));
            std::memcpy(check_program.user_data.data() + 4, &image, sizeof(image));
            std::memcpy(check_program.user_data.data() + 12, &observation, sizeof(observation));
            rasterizer.DispatchDirect();
            rasterizer.ReadMemory(observation.base_address, 4096);
            rasterizer.Finish();
            if (std::memcmp(backing, expected.data(), AllocationBytes) ||
                rasterizer.IsGpuBufferModified(target.base_address, ShortBytes) ||
                !rasterizer.IsGpuBufferModified(target.base_address, layout.guest_bytes))
                throw std::runtime_error("Tail-only GPU write lost the image or polluted the clean prefix");
        }
        if (short_views) {
            const u64 early_bytes = long_first ? LongBytes : ShortBytes;
            if (rasterizer.IsGpuBufferModified(target.base_address, early_bytes) !=
                (sequence == Sequence::DirtyShort))
                throw std::runtime_error("Readonly original-size dirty precondition changed");
        }

        // Resource discovery follows first live instruction use, not SGPR index.
        // Put the FIRST used descriptor in s[4:7] in every case. The writer's raw
        // load is observed, so it cannot disappear through dead-code removal.
        const u32 read_group = binding_order ? 0 : read_first ? 1 : 0;
        const u32 write_group = binding_order ? 2 : read_first ? 0 : 1;
        const u32 output_group = binding_order ? 1 : repeated ? 3 : 2;
        const u32 late_group = binding_order ? 3 : 2;
        const u32 early_opcode = early_formatted ? 0xe0000000 : 0xe0300000;
        auto early_sharp = target, late_sharp = target, writer_sharp = target;
        if (short_views) {
            early_sharp.num_records = u32((long_first ? LongBytes : ShortBytes) / 4);
            late_sharp.num_records = u32((mixed ? LongBytes : ShortBytes) / 4);
        }
        if (writer_cases) {
            writer_sharp.num_records = u32((long_writer ? LongBytes : 4) / 4);
            early_sharp.num_records = u32((long_writer ? LongBytes :
                sequence == Sequence::ShortWriterFullRead ? layout.guest_bytes : ShortBytes) / 4);
            late_sharp.num_records = u32(ShortBytes / 4);
        }
        if (offset_writer) {
            // A full formatted reader stays at image base. The DWORD writer
            // targets the separately seeded I texel at (0,8); both R/W and W/R
            // binding orders must tile I without invalidating its logical owner.
            writer_sharp.base_address += far_offset + (narrow_writer ? write_bytes : 0);
            if (narrow_writer) {
                writer_sharp.stride = 0;
                writer_sharp.num_records = write_bytes;
            }
            early_sharp.num_records = late_sharp.num_records = u32(layout.guest_bytes / 4);
            if (far_offset < 256 || far_offset % 4 || far_offset + 8 > layout.guest_bytes)
                throw std::runtime_error("Offset writer lost its contained DWORD range and distinct image base");
        }
        if (binding_order) early_sharp.num_records = u32((layout.guest_bytes + 4) / 4);
        auto command_output = observation;
        if (binding_order) {
            // This prior logical writer shares R's last 4-KiB query page but
            // overlaps neither the image nor any original R byte. Its binding
            // must make the initially clean small R resident before W_A binds.
            command_output.base_address = target.base_address + layout.guest_bytes + 8;
            command_output.num_records = ShortBytes / 4;
            if (layout.guest_bytes != 4096 || early_sharp.GetSize() != 4100 ||
                command_output.base_address < early_sharp.base_address + early_sharp.GetSize() ||
                (command_output.base_address >> 12) !=
                    ((early_sharp.base_address + early_sharp.GetSize() - 1) >> 12) ||
                (command_output.base_address >> 12) !=
                    ((command_output.base_address + command_output.GetSize() - 1) >> 12) ||
                command_output.base_address + command_output.GetSize() > base + ObservationOffset ||
                rasterizer.IsGpuBufferModified(early_sharp.base_address, early_sharp.GetSize()))
                throw std::runtime_error("Binding-order fixture lost its clean disjoint same-page precondition");
        }
        if (partial_output) {
            // Do not reuse the full-page observation's resident allocation:
            // doing so could acquire ownership for a small sharp and hide the
            // synchronous fallback. This page has no earlier GPU binding.
            command_output.base_address = base + ObservationOffset + 4096;
            command_output.num_records = 64 / 4;
        }
        const u64 command_output_offset = command_output.base_address - base;
        const std::array<u32, 2> early_load{early_opcode, 0x80000400 | (read_group << 16)};
        const std::array<u32, 2> raw_load{writer_load_opcode, 0x80000500 | (write_group << 16)};
        std::vector<u32> words;
        const auto append = [&](const auto& pair) { words.insert(words.end(), pair.begin(), pair.end()); };
        if (binding_order) words.insert(words.end(), {
            // First live resource is W_out in s[4:7], before either R or W_A.
            0x7e1802ff, TailWrite, 0xe0700000, 0x80000c00 | (output_group << 16)
        });
        append(read_first ? early_load : raw_load);
        if (mixed) words.insert(words.end(), {0xe0000000, 0x80020900}); // long resident R before W
        append(read_first ? raw_load : early_load);
        words.push_back(0xbf8c0000); // s_waitcnt before the actual store
        if (repeated) {
            words.insert(words.end(), {
                0x7e1002ff, FinalValue, writer_store_opcode, 0x80000800 | (write_group << 16)
            });
            if (short_writer) words.insert(words.end(), {
                // Exact W is one scalar. The next same-width store is outside
                // its sharp, including within a padded raw DWORD descriptor.
                writer_store_opcode | write_bytes, 0x80000800 | (write_group << 16)
            });
            if (long_writer) words.insert(words.end(), {
                // The original long W really owns this non-image DWORD. Keep
                // the rest of its patterned tail unchanged in the full oracle.
                0x7e0002ff, u32(layout.guest_bytes), 0x7e1802ff, TailWrite,
                0xe0701000, 0x80000c00 | (write_group << 16)
            });
            words.insert(words.end(), {
                0xbf8c0000,
                // Reload the SAME early descriptor after W changed the arena.
                // A private CPU snapshot stays B; a resident alias sees Final.
                early_opcode, 0x80000600 | (read_group << 16),
                0xe0000000, 0x80000700 | (late_group << 16), // distinct late formatted R
                0xbf8c0000
            });
        }
        const bool check_short_bound = (short_views || writer_cases) && early_sharp.GetSize() == ShortBytes;
        if (short_views || writer_cases) {
            if (check_short_bound) words.insert(words.end(), {
                // Offset64 is outside the unchanged short sharp, even though
                // the common arena and retained image are larger. It reads0.
                0xe0000040, 0x80000b00 | (read_group << 16)
            });
            if (has_long_tail) {
                const u32 long_group = mixed ? 2 : read_group;
                words.insert(words.end(), {
                    0x7e0002ff, u32(layout.guest_bytes),
                    0xe0001000, 0x80000a00 | (long_group << 16)
                });
            }
            words.push_back(0xbf8c0000);
        }
        if (narrow_writer) words.insert(words.end(), {
            // Query the live full-buffer alias before any CPU readback: exact
            // dirty masks alone could conceal an illegal GPU-only padding write.
            0x7e0002ff, u32(far_offset), 0xe0301000, 0x80000d00 | (late_group << 16),
            writer_load_opcode | write_bytes, 0x80000e00 | (write_group << 16),
            0xbf8c0000, 0xe0700038, 0x80000d00 | (output_group << 16),
            0xe070003c, 0x80000e00 | (output_group << 16)
        });
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
        if (short_views || writer_cases) {
            if (mixed) words.insert(words.end(), {0xe0700028, 0x80000900 | (output_group << 16)});
            if (has_long_tail)
                words.insert(words.end(), {0xe070002c, 0x80000a00 | (output_group << 16)});
            if (check_short_bound)
                words.insert(words.end(), {0xe0700030, 0x80000b00 | (output_group << 16)});
        }
        words.push_back(0xbf810000);
        // Extended straight-line probes need more instruction words. The real
        // BinaryInfo scanner supports this later metadata placement unchanged.
        initialize(ordered, words, 0x4f52444f52440000ULL + identity, short_views || writer_cases ? 64 : 32);
        auto& ordered_program = bind(ordered, repeated ? 16 : 12);
        std::memcpy(ordered_program.user_data.data() + write_group * 4, &writer_sharp, sizeof(writer_sharp));
        std::memcpy(ordered_program.user_data.data() + read_group * 4, &early_sharp, sizeof(early_sharp));
        if (repeated) std::memcpy(ordered_program.user_data.data() + late_group * 4, &late_sharp, sizeof(late_sharp));
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
            const bool early_reader = index == (binding_order ? 1u : read_first ? 0u : 1u);
            const bool late_reader = repeated && index == (binding_order ? 3u : mixed ? 1u : 2u);
            const bool reader = early_reader || late_reader;
            const bool formatted = late_reader || (early_reader && early_formatted);
            const bool output = index == (binding_order ? 0u : guest_buffers.size() - 1);
            const auto address = output ? command_output.base_address : reader ? target.base_address : writer_sharp.base_address;
            const auto bytes = output ? command_output.GetSize() : early_reader ? early_sharp.GetSize() :
                late_reader ? late_sharp.GetSize() : writer_sharp.GetSize();
            if (sharp.base_address != address || sharp.GetSize() != bytes ||
                buffer.is_written == reader || buffer.is_formatted != formatted)
                throw std::runtime_error("Actual translated buffer collection does not have the requested R/W order");
            if (narrow_writer && !reader && !output &&
                buffer.used_types != (write_bytes == 1 ? Shader::IR::Type::U8 : Shader::IR::Type::U16))
                throw std::runtime_error("Narrow writer unexpectedly acquired a wider native storage alias");
        }
        rasterizer.DispatchDirect();
        if (binding_order && (!rasterizer.IsGpuBufferModified(target.base_address, 4) ||
            !rasterizer.IsGpuBufferModified(command_output.base_address, command_output.GetSize())))
            throw std::runtime_error("Binding-order dispatch retired before the GPU-only image-tail observation");
        const auto loaded = image_tiled ? ImageValue : BufferValue;
        const auto early_loaded = private_early ? BufferValue : loaded;
        const auto next = image_tiled ? ImageNext : initial(1, 0);
        if (binding_order) expect_word(command_output_offset, TailWrite);
        expect_word(command_output_offset + 16, early_loaded);
        expect_word(command_output_offset + 20, offset_writer ? (ImageFar >> write_shift) & write_mask : loaded);
        if (narrow_writer) {
            expect_word(command_output_offset + 56, gpu_far_word);
            expect_word(command_output_offset + 60, 0);
        }
        if (repeated) {
            expect_word(command_output_offset + 24, private_early ? BufferValue : offset_writer ? ImageValue : FinalValue);
            expect_word(command_output_offset + 28, offset_writer ? ImageValue : FinalValue);
        }
        if (short_views || writer_cases) {
            if (mixed) expect_word(command_output_offset + 40, ImageValue);
            if (has_long_tail) {
                expect_word(command_output_offset + 44,
                    long_writer ? TailWrite : 0x8d732100u ^ u32(layout.guest_bytes));
                if (rasterizer.IsGpuBufferModified(target.base_address + layout.guest_bytes,
                                                  LongBytes - layout.guest_bytes) != long_writer)
                    throw std::runtime_error("Original writer extent disagrees with the long tail modification state");
            }
            if (check_short_bound) expect_word(command_output_offset + 48, 0);
        }
        if (partial_output) {
            // Without asking for a readback, the unowned output must already be
            // CPU-visible when the synchronous dispatch fallback returns.
            if (rasterizer.IsGpuBufferModified(command_output.base_address, command_output.GetSize()) ||
                std::memcmp(backing + command_output_offset, expected.data() + command_output_offset,
                            size_t(command_output.GetSize())))
                throw std::runtime_error("Partial ordered-stream observation did not retire synchronously");
        }

        // Exact-base writers invalidate I; offset writers must retain it. Probe
        // both untouched first texels and the offset writer's independently
        // seeded I texel, which must not acquire the new physical buffer value.
        std::vector<u32> after_words{
            0x7e000280, 0x7e020280, 0xf0000100, 0x00000400,
            0x7e000281, 0xf0000100, 0x00000500, 0xbf8c0000,
            0xe0700020, 0x80020400, 0xe0700024, 0x80020500
        };
        if (writer_cases) after_words.insert(after_words.end(), {
            0x7e000280, 0x7e020288, 0xf0000100, 0x00000600, 0xbf8c0000,
            0xe0700034, 0x80020600
        });
        after_words.push_back(0xbf810000);
        initialize(after_probe, after_words, 0x4f52444146540000ULL + identity);
        auto& after_program = bind(after_probe, 12);
        const auto& after_output = binding_order ? command_output : observation;
        const u64 after_output_offset = after_output.base_address - base;
        std::memcpy(after_program.user_data.data(), &image, sizeof(image));
        std::memcpy(after_program.user_data.data() + 8, &after_output, sizeof(after_output));
        rasterizer.DispatchDirect();
        if (offset_writer) {
            // A prior full dirty B interval publishes the retiled I bytes too.
            // With clean B, only the current DWORD writer becomes CPU-visible;
            // synchronization itself must not mark all image bytes as written.
            if (!clean) {
                expect_word(ImageOffset + first, ImageValue);
                expect_word(ImageOffset + second, ImageNext);
            }
            const u32 cpu_far_word = (clean ? initial(0, 8) : ImageFar) & ~physical_mask;
            expect_word(ImageOffset + far_offset, cpu_far_word | ((FinalValue & write_mask) << write_shift));
        } else expect_word(ImageOffset + first, FinalValue);
        // A readonly retile is not a CPU write. Only the original W extent
        // publishes I tail to physical memory; a four-byte W leaves that tail B.
        if (image_tiled && !short_writer) expect_word(ImageOffset + second, ImageNext);
        if ((short_views || writer_cases) && image_tiled && !short_writer) expect_word(ImageOffset + far_offset, ImageFar);
        // The tiny prior dirty interval at+256 is restored from I by retile.
        // Its seeded I texel equals original B there, so expected stays intact.
        if (dirty_tail) expect_word(ImageOffset + 4096, TailWrite);
        if (long_writer) expect_word(ImageOffset + layout.guest_bytes, TailWrite);
        expect_word(after_output_offset + 32, offset_writer ? ImageValue : FinalValue);
        expect_word(after_output_offset + 36, next);
        if (writer_cases) expect_word(after_output_offset + 52, image_tiled ? ImageFar : initial(0, 8));
        rasterizer.ReadMemory(base, AllocationBytes);
        rasterizer.Finish();
        if (std::memcmp(backing, expected.data(), AllocationBytes))
            throw std::runtime_error("Ordered GCN observation, final buffer, image reload or physical/padding oracle mismatch");
        const char* sequence_name = sequence == Sequence::ReadWrite ? "R/W" :
            sequence == Sequence::WriteRead ? "W/R" :
            sequence == Sequence::RawWriteRead ? "rawR/W/formattedR" :
            sequence == Sequence::ReadWriteRead ? "formattedR/W/formattedR" :
            dirty_tail ? "clean64-byteR/dirty-tail/fullW" :
            sequence == Sequence::DirtyShort ? "dirty64-byteR/fullW" :
            long_first ? "clean20KiBR/full-imageW" : mixed ? "private64-byteR/resident20KiBR/fullW" :
            sequence == Sequence::ShortWriterFullRead ? "clean-fullR/4-byteW" :
            redirty_prefix ? "dirty64-byteR/4-byteW" :
            sequence == Sequence::ShortWriterPrivateRead ? "private64-byteR/4-byteW" :
            sequence == Sequence::ShortWriterFirst ? "4-byteW/64-byteR" :
            long_writer ? "clean20KiBR/20KiBW" :
            sequence == Sequence::OffsetWriterReadFirst ? "fullR/interior4-byteW/retainedI" :
            sequence == Sequence::OffsetWriterFirst ? "interior4-byteW/fullR/retainedI" : "same-page-disjointW/clean4100-byteR/4-byteW";
        std::printf("PASS: ordered-formatted %s %s %s%s writer-bytes=%u bytes=%llu; real GCN, full physical oracle\n",
            small_image ? "small" : "large", clean ? "CPU-clean" : "GPU-modified",
            sequence_name, partial_output ? " partial-output sync fallback" : "", write_bytes,
            static_cast<unsigned long long>(layout.guest_bytes));
    }
    rasterizer.Finish();
    drain.armed = false;
    RunOrderedDisjointGcnFixture(rasterizer, gpu, profile, memory, backing, base);
    RunReadonlyRevalidationGcnFixture(rasterizer, gpu, profile, memory, backing, base);
    RunBcBlockExpansionGcnFixture(rasterizer, gpu, profile, memory, backing, base);
    RunBcMipExpansionGcnFixture(rasterizer, gpu, profile, memory, backing, base);
    RunBcPartialExpansionGcnFixture(rasterizer, gpu, profile, memory, backing, base);
    RunBcWritableAliasGcnFixture(rasterizer, gpu, profile, memory, backing, base);
}
} // namespace NativeDX12
