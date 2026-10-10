// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/fair_recursive_mutex.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <vector>

int main() {
    Common::FairRecursiveMutex mutex;
    std::vector<unsigned> order;
    std::vector<std::jthread> waiters;
    mutex.lock();
    mutex.lock();
    if (!mutex.try_lock()) return 1;
    mutex.unlock();
    mutex.unlock();
    for (unsigned i = 0; i != 4; ++i) {
        waiters.emplace_back([&, i] {
            std::lock_guard guard(mutex);
            order.push_back(i);
            // Recursive calls must work even with other threads queued.
            std::lock_guard recursive(mutex);
        });
        const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (mutex.WaitingCount() != i + 1)
            if (std::chrono::steady_clock::now() > until) std::terminate();
            else std::this_thread::yield();
    }
    mutex.unlock();
    // Model the render thread immediately reacquiring after every draw.
    mutex.lock();
    order.push_back(4);
    mutex.unlock();
    waiters.clear();
    if (order != std::vector<unsigned>{0, 1, 2, 3, 4}) return 2;
    unsigned counter{};
    std::vector<std::jthread> competing;
    for (unsigned i = 0; i != 6; ++i) competing.emplace_back([&] {
        for (unsigned j = 0; j != 2000; ++j) {
            std::lock_guard outer(mutex);
            std::lock_guard inner(mutex);
            ++counter;
        }
    });
    competing.clear();
    if (counter != 12000 || mutex.WaitingCount()) return 3;
    std::puts("PASS: queued loaders precede immediate render reacquisition; recursive ownership and contended mutual exclusion");
}
