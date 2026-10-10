// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <chrono>
#include <cstdint>

namespace Libraries::VideoOut {
// Keep PS4 time independent of host Present. Advance late ticks without notification bursts.
class VblankClock {
public:
    using Clock = std::chrono::steady_clock;
    VblankClock(Clock::time_point origin, Clock::duration period)
        : next(origin + period), period(period) {}
    Clock::time_point Deadline() const {
        return next;
    }
    std::uint64_t Advance(Clock::time_point now) {
        if (now < next)
            return 0;
        const auto ticks = 1 + (now - next) / period;
        next += period * ticks;
        return static_cast<std::uint64_t>(ticks);
    }

private:
    Clock::time_point next;
    Clock::duration period;
};
} // namespace Libraries::VideoOut
