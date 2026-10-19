// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <span>

#include "common/types.h"

namespace AmdGpu {

enum class Pm4StreamError {
    None,
    UnsupportedPacketType,
    TruncatedType3Packet,
};

struct Pm4StreamValidation {
    Pm4StreamError error{Pm4StreamError::None};
    size_t offset{};
    u32 raw{};
    size_t packet_words{};

    [[nodiscard]] constexpr bool IsValid() const {
        return error == Pm4StreamError::None;
    }
};

[[nodiscard]] constexpr Pm4StreamValidation ValidatePm4Stream(std::span<const u32> stream,
                                                               bool allow_type2 = true) {
    size_t offset{};
    while (offset < stream.size()) {
        const u32 raw = stream[offset];
        const u32 type = raw >> 30;
        if (type == 2 && allow_type2) {
            ++offset;
            continue;
        }
        if (type != 3) {
            return {
                .error = Pm4StreamError::UnsupportedPacketType,
                .offset = offset,
                .raw = raw,
                .packet_words = 1,
            };
        }

        const size_t body_words = ((raw >> 16) & 0x3fffu) + 1u;
        const size_t packet_words = body_words + 1u;
        if (packet_words > stream.size() - offset) {
            return {
                .error = Pm4StreamError::TruncatedType3Packet,
                .offset = offset,
                .raw = raw,
                .packet_words = packet_words,
            };
        }
        offset += packet_words;
    }
    return {};
}

} // namespace AmdGpu
