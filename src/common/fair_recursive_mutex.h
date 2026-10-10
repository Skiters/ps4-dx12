// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/api_trace.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace Common {
// Recursion is allowed only for the owner. New owners take FIFO tickets, so a
// rendering thread cannot reacquire ahead of a queued file-loading thread.
class FairRecursiveMutex {
    std::mutex gate;
    std::condition_variable changed;
    std::thread::id owner;
    uint64_t next{}, serving{};
    unsigned depth{};
    std::unique_ptr<ApiTrace::Scope> held;
public:
    void lock() {
        ApiTrace::Call("SYNC", "execution_mutex.acquire", [&] {
            std::unique_lock guard(gate);
            const auto caller = std::this_thread::get_id();
            if (depth && owner == caller) { ++depth; return; }
            const auto ticket = next++;
            changed.wait(guard, [&] { return !depth && ticket == serving; });
            owner = caller;
            depth = 1;
            if (ApiTrace::Enabled())
                held = std::make_unique<ApiTrace::Scope>("SYNC", "execution_mutex.hold", ApiTrace::Value(ticket));
        });
    }
    bool try_lock() {
        std::lock_guard guard(gate);
        const auto caller = std::this_thread::get_id();
        if (depth && owner == caller) { ++depth; return true; }
        if (depth || next != serving) return false;
        const auto ticket = next++;
        owner = caller;
        depth = 1;
        if (ApiTrace::Enabled())
            held = std::make_unique<ApiTrace::Scope>("SYNC", "execution_mutex.hold", ApiTrace::Value(ticket));
        return true;
    }
    void unlock() {
        std::lock_guard guard(gate);
        if (!depth || owner != std::this_thread::get_id()) std::terminate();
        if (--depth) return;
        if (held) { held->Return("released"); held.reset(); }
        owner = {};
        ++serving;
        changed.notify_all();
    }
    size_t WaitingCount() {
        std::lock_guard guard(gate);
        return static_cast<size_t>(next - serving - (depth ? 1 : 0));
    }
};
}
