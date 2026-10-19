// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdlib>

#include "core/libraries/gnmdriver/command_writer_trace.h"
#include "video_core/amdgpu/pm4_stream_validation.h"

namespace {

constexpr u32 Type3(u32 opcode, u32 body_words) {
    return 0xc0000000u | ((body_words - 1u) << 16) | (opcode << 8);
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

} // namespace

int main() {
    using AmdGpu::Pm4StreamError;
    using AmdGpu::ValidatePm4Stream;

    constexpr std::array valid{0x80000000u, Type3(0x10, 1), 0u, Type3(0x12, 2), 1u, 2u};
    Require(ValidatePm4Stream(valid).IsValid());

    constexpr std::array type0{0x00000000u};
    const auto type0_result = ValidatePm4Stream(type0);
    Require(type0_result.error == Pm4StreamError::UnsupportedPacketType);
    Require(type0_result.offset == 0);

    constexpr std::array type1{0x40000000u};
    Require(ValidatePm4Stream(type1).error == Pm4StreamError::UnsupportedPacketType);

    constexpr std::array truncated{Type3(0x10, 3), 0u};
    const auto truncated_result = ValidatePm4Stream(truncated);
    Require(truncated_result.error == Pm4StreamError::TruncatedType3Packet);
    Require(truncated_result.offset == 0);
    Require(truncated_result.packet_words == 4);

    constexpr std::array malformed_after_prefix{Type3(0x10, 1), 0u, 0x40000000u};
    const auto prefix_result = ValidatePm4Stream(malformed_after_prefix);
    Require(prefix_result.error == Pm4StreamError::UnsupportedPacketType);
    Require(prefix_result.offset == 2);

    Require(ValidatePm4Stream(valid, false).error == Pm4StreamError::UnsupportedPacketType);

    std::array<u32, 8> writer_buffer{};
    Libraries::GnmDriver::CommandWriterTrace::Record("wide-writer", writer_buffer.data(), 4);
    Libraries::GnmDriver::CommandWriterTrace::Record("narrow-writer", writer_buffer.data() + 2,
                                                     1);
    const auto writers = Libraries::GnmDriver::CommandWriterTrace::FindRecentCovering(
        reinterpret_cast<uintptr_t>(writer_buffer.data() + 2));
    Require(writers.size() == 2);
    Require(writers[0].function == "narrow-writer");
    Require(writers[1].function == "wide-writer");
    Require(writers[0].fingerprint == 0);
    {
        Libraries::GnmDriver::CommandWriterTrace::Scope changed_scope{
            "changed-writer", writer_buffer.data() + 4, 1};
        writer_buffer[4] = 0x12345678u;
        changed_scope.MarkWritten(1);
    }
    const auto changed = Libraries::GnmDriver::CommandWriterTrace::FindRecentCovering(
        reinterpret_cast<uintptr_t>(writer_buffer.data() + 4));
    Require(changed.size() == 1);
    Require(changed[0].function == "changed-writer");
    Require(changed[0].snapshot[0] == 0x12345678u);
    {
        Libraries::GnmDriver::CommandWriterTrace::Scope bounded_scope{
            "bounded-writer", writer_buffer.data() + 5, 3};
        bounded_scope.MarkWritten(1);
    }
    Require(Libraries::GnmDriver::CommandWriterTrace::FindRecentCovering(
                reinterpret_cast<uintptr_t>(writer_buffer.data() + 6)).empty());
    const auto bounded = Libraries::GnmDriver::CommandWriterTrace::FindRecentCovering(
        reinterpret_cast<uintptr_t>(writer_buffer.data() + 5));
    Require(bounded.size() == 1 && bounded[0].size_dw == 1);
    return 0;
}
