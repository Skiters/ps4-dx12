// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/libraries/ajm/ajm_batch.h"
#include "common/bounded_threadsafe_queue.h"

#include <chrono>
#include <cstdio>
#include <latch>
#include <stdexcept>
#include <thread>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}
} // namespace

int main() {
    using namespace std::chrono_literals;
    using Libraries::Ajm::AjmBatch;
    try {
        // A canceled queued batch still owns a completion that BatchWait must
        // consume. It must not execute codec work or pretend it was processed.
        AjmBatch canceled;
        canceled.jobs.emplace_back();
        canceled.canceled = true;
        unsigned executed{};
        Common::MPSCQueue<AjmBatch*> queue;
        queue.EmplaceWait(&canceled);
        std::jthread cancel_worker([&] {
            auto* batch = queue.PopWait();
            batch->Execute([&](auto, auto) { ++executed; });
        });
        Require(canceled.finished.try_acquire_for(100ms),
                "Canceled queued AJM batch never completes");
        Require(executed == 0 && !canceled.processed && canceled.canceled,
                "Canceled AJM batch executed work or lost cancellation status");
        Require(!canceled.finished.try_acquire(), "AJM batch completed more than once");

        AjmBatch normal;
        normal.id = 17;
        normal.jobs.emplace_back();
        std::latch entered{1}, release{1};
        unsigned output{};
        std::jthread worker([&] {
            normal.Execute([&](auto id, auto jobs) {
                entered.count_down();
                release.wait();
                output = id == 17 && jobs.size() == 1 ? 0xcafebabeu : 0;
            });
        });
        entered.wait();
        const bool premature = normal.finished.try_acquire();
        release.count_down();
        Require(!premature, "AJM completion was visible before codec work finished");
        Require(normal.finished.try_acquire_for(1s), "Normal AJM batch never completes");
        Require(normal.processed && output == 0xcafebabeu,
                "AJM completion did not publish codec results");

        AjmBatch failed;
        bool propagated{};
        try {
            failed.Execute([](auto, auto) { throw std::runtime_error("codec failure"); });
        } catch (const std::runtime_error&) {
            propagated = true;
        }
        Require(propagated && !failed.finished.try_acquire(),
                "Codec exception was suppressed or published as completed work");
        puts("PASS: canceled AJM completion, real work visibility, no false success on exception");
        return 0;
    } catch (const std::exception& error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
