// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <mutex>
#include <string_view>
#include <thread>
#include "common/performance_config.h"

namespace Common {

// A bounded application-local flight recorder. Producers never wait for the disk or a full
// queue: a contended/full queue drops a record and exposes that loss in the counter and CSV.
// Only the writer thread formats/writes CSV. No affinity, priority or system settings change.
class PerformanceRecorder {
public:
    static constexpr bool Enabled = PerformanceDiagnosticsEnabled;
    using Clock = DiagnosticClock;
    enum class Kind { Frame, Warning, Error, GpuSpan, Pipeline, CpuDraw, Temporal, CpuThread };
    struct Event {
        Kind kind{};
        double ms{};
        std::uint64_t source{};
        // interval, source interval, queue age, acquire, submit lock, submit, present,
        // pool wait, fence wait, source preparation, GPU graphics span, CPU op mean/max,
        // pipeline constructor wall time. Non-frame records fill only relevant columns.
        std::array<double, 17> values{};
        std::array<char, 240> text{};
    };
    struct Counters {
        std::uint64_t frames{}, spikes{}, slow{}, warnings{}, errors{}, dropped{};
        bool recording{}, io_failed{};
    };
    class ScopedCompilation {
    public:
        explicit ScopedCompilation(std::string_view label) {
            if constexpr (!Enabled) return;
            event.kind = Kind::Pipeline;
            SetText(event, label);
        }
        ~ScopedCompilation() {
            if constexpr (!Enabled) return;
            event.values[13] = Milliseconds(began);
            Get().Push(event);
        }

    private:
        Event event;
        Clock::time_point began{Clock::now()};
    };

    static PerformanceRecorder& Get() {
        static PerformanceRecorder recorder;
        return recorder;
    }
    static bool IsSpike(double interval_ms, double baseline_ms) {
        return baseline_ms > 0 && interval_ms > std::max(50.0, baseline_ms * 1.5);
    }
    static double Milliseconds(Clock::time_point start) {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
    static void SetText(Event& event, std::string_view text) {
        const auto count = std::min(text.size(), event.text.size() - 1);
        std::memcpy(event.text.data(), text.data(), count);
        event.text[count] = 0;
    }

    bool Start(const std::filesystem::path& path,
               void (*sampler)(PerformanceRecorder&, double) = nullptr) {
        if constexpr (!Enabled) return false;
        Stop();
        file.open(path, std::ios::out | std::ios::trunc);
        file.imbue(std::locale::classic());
        if (!file) {
            io_failed = true;
            return false;
        }
        file << "kind,elapsed_ms,source_frame,interval_ms,source_interval_ms,queue_age_ms,"
                "acquire_ms,submit_lock_ms,submit_ms,present_ms,pool_wait_ms,fence_wait_ms,"
                "prepare_ms,gpu_graphics_span_ms,cpu_operation_mean_ms,cpu_operation_max_ms,"
                "pipeline_constructor_ms,present_cpu_before_display_ms,thread_cpu_ms,thread_"
                "interval_ms,warnings,errors,spikes,"
                "slow33,dropped,text\n";
        file.flush();
        start = Clock::now();
        head = tail = size = 0;
        baseline = 0;
        frames = spikes = slow = warnings = errors = dropped = 0;
        io_failed = false;
        stopping = false;
        thread_sampler = sampler;
        running.store(true, std::memory_order_release);
        try {
            writer = std::thread([this] {
                try {
                    WriteLoop();
                } catch (...) {
                    io_failed = true;
                    running = false;
                }
            });
        } catch (...) {
            running = false;
            file.close();
            return false;
        }
        return true;
    }

    void Stop() {
        if constexpr (!Enabled) return;
        running.store(false, std::memory_order_release);
        stopping = true;
        ready.notify_one();
        if (writer.joinable()) {
            writer.join();
        }
        if (file.is_open()) {
            file.close();
        }
    }

    void Push(Event event) { if constexpr (!Enabled) return;
        if (!running.load(std::memory_order_acquire)) {
            return;
        }
        event.ms = Milliseconds(start);
        std::unique_lock lock{mutex, std::try_to_lock};
        if (!lock.owns_lock() || size == queue.size()) {
            ++dropped;
            return;
        }
        queue[tail] = event;
        tail = (tail + 1) % queue.size();
        ++size;
        if (size % 32 == 0) {
            ready.notify_one();
        }
    }

    // Single presenter thread only. This counts display gaps, not inferred software errors.
    void Frame(Event event) { if constexpr (!Enabled) return;
        if (!running.load(std::memory_order_acquire)) {
            return;
        }
        event.kind = Kind::Frame;
        const double interval = event.values[0];
        ++frames;
        if (interval > 33.334) {
            ++slow;
        }
        if (IsSpike(interval, baseline)) {
            ++spikes;
            SetText(event, "display-gap-spike; attribution requires other timing rows");
        }
        if (interval > 0) {
            baseline = baseline == 0 ? interval
                                     : baseline * 0.95 + std::min(interval, baseline * 1.5) * 0.05;
        }
        Push(event);
    }

    // Called by the existing logging frontend only for warning/error messages already enabled
    // by its filter. It never calls the logger, avoiding recursion or sink-lock inversion.
    void Message(bool error, std::string_view message) { if constexpr (!Enabled) return;
        if (!running.load(std::memory_order_acquire)) {
            return;
        }
        if (error) {
            ++errors;
        } else {
            ++warnings;
        }
        Event event;
        event.kind = error ? Kind::Error : Kind::Warning;
        SetText(event, message);
        Push(event);
    }

    Counters Read() const {
        if constexpr (!Enabled) return {};
        return {frames.load(), spikes.load(),  slow.load(),    warnings.load(),
                errors.load(), dropped.load(), running.load(), io_failed.load()};
    }

    ~PerformanceRecorder() {
        if constexpr (!Enabled) return;
        Stop();
    }

private:
    void WriteLoop() {
        auto last_flush = Clock::now();
        std::uint64_t bytes{};
        for (;;) {
            std::array<Event, 64> batch;
            std::size_t count{};
            {
                std::unique_lock lock{mutex};
                ready.wait_for(lock, std::chrono::milliseconds{250},
                               [this] { return size >= 32 || stopping.load(); });
                while (size && count < batch.size()) {
                    batch[count++] = queue[head];
                    head = (head + 1) % queue.size();
                    --size;
                }
                if (!count && stopping) {
                    break;
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                const auto& event = batch[i];
                static constexpr std::array names{"frame",    "warning",   "error",
                                                  "gpu_span", "pipeline",  "cpu_draw_sample",
                                                  "temporal", "cpu_thread"};
                file << names[static_cast<unsigned>(event.kind)] << ',' << std::fixed
                     << std::setprecision(3) << event.ms << ',' << event.source;
                for (const auto value : event.values) {
                    file << ',' << value;
                }
                const auto counters = Read();
                file << ',' << counters.warnings << ',' << counters.errors << ',' << counters.spikes
                     << ',' << counters.slow << ',' << counters.dropped << ",\"";
                for (const char c : event.text) {
                    if (!c)
                        break;
                    if (c == '"')
                        file << '"';
                    file << (c == '\n' || c == '\r' ? ' ' : c);
                }
                file << "\"\n";
            }
            const auto position = file.tellp();
            if (position >= 0)
                bytes = static_cast<std::uint64_t>(position);
            if (Clock::now() - last_flush >= std::chrono::seconds{1}) {
                const auto now = Clock::now();
                const double interval_ms =
                    std::chrono::duration<double, std::milli>(now - last_flush).count();
                file.flush();
                last_flush = now;
                if (thread_sampler)
                    thread_sampler(*this, interval_ms);
            }
            if (!file || bytes >= 128ull * 1024 * 1024) {
                io_failed = !file;
                running = false;
                file.flush();
                return; // bounded disk use; overlay distinguishes stopped recording
            }
        }
        file.flush();
    }

    std::array<Event, Enabled ? 2048 : 1> queue{};
    std::size_t head{}, tail{}, size{};
    std::mutex mutex;
    std::condition_variable ready;
    std::thread writer;
    std::ofstream file;
    Clock::time_point start{};
    void (*thread_sampler)(PerformanceRecorder&, double){};
    double baseline{};
    std::atomic<bool> running{}, stopping{}, io_failed{};
    std::atomic<std::uint64_t> frames{}, spikes{}, slow{}, warnings{}, errors{}, dropped{};
};
} // namespace Common
