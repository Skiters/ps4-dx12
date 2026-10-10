// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace Libraries::VideoOut {

// Bound real source-frame dispatches without aligning them to the guest vblank phase.
// XeSS remains responsible for pacing its generated output frames.
class NativePresentCadence {
public:
    using Clock = std::chrono::steady_clock;

    explicit NativePresentCadence(Clock::duration period) : period{period} {}

    bool Ready(Clock::time_point now) const {
        return !dispatched || now >= next_dispatch;
    }

    Clock::time_point Deadline() const {
        return next_dispatch;
    }

    void Dispatched(Clock::time_point now) {
        // Advance from actual work, never replay missed deadlines after a slow Present.
        next_dispatch = now + period;
        dispatched = true;
    }

private:
    Clock::duration period;
    Clock::time_point next_dispatch{};
    bool dispatched{};
};

enum class NativePresentWaitResult { Ready, Idle, Paused, ModeChanged, Stopped };

// The caller owns the queue lock. Predicates inspect state without acquiring vo_mutex.
// Idle/paused redraws remain bounded even when pause/rate changes have no explicit notifier.
template <typename Mutex, typename Ready, typename Paused, typename NativeMode>
NativePresentWaitResult WaitNativePresentRequest(
    std::condition_variable_any& cv, std::unique_lock<Mutex>& lock, std::stop_token stop,
    NativePresentCadence::Clock::duration idle_period, const NativePresentCadence& cadence,
    Ready&& ready, Paused&& paused, NativeMode&& native_mode) {
    using Clock = NativePresentCadence::Clock;
    const auto idle_deadline = Clock::now() + idle_period;
    for (;;) {
        if (stop.stop_requested())
            return NativePresentWaitResult::Stopped;
        if (!native_mode())
            return NativePresentWaitResult::ModeChanged;
        const auto now = Clock::now();
        if (paused()) {
            if (now >= idle_deadline)
                return NativePresentWaitResult::Paused;
            cv.wait_until(lock, stop, idle_deadline,
                          [&] { return !paused() || !native_mode(); });
        } else if (ready()) {
            if (cadence.Ready(now))
                return NativePresentWaitResult::Ready;
            // A queued request must not make the deadline wait return immediately.
            cv.wait_until(lock, stop, cadence.Deadline(),
                          [&] { return paused() || !native_mode(); });
        } else {
            if (now >= idle_deadline)
                return NativePresentWaitResult::Idle;
            cv.wait_until(lock, stop, idle_deadline,
                          [&] { return ready() || paused() || !native_mode(); });
        }
    }
}

} // namespace Libraries::VideoOut
