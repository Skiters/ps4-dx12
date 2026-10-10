// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <chrono>

namespace Common {
// Production policy: no profiling, sampling, CSV writer or diagnostic GPU queries.
inline constexpr bool PerformanceDiagnosticsEnabled = false;

// Only diagnostic timestamps use this clock. Simulation, audio and XeSS pacing
// must keep using their real clocks, independently of the profiling policy.
struct DiagnosticClock : std::chrono::steady_clock {
    static time_point now() noexcept {
        if constexpr (PerformanceDiagnosticsEnabled) {
            return std::chrono::steady_clock::now();
        }
        return {};
    }
};
} // namespace Common
