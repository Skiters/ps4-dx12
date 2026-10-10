// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <unordered_map>
#include "common/performance_recorder.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#endif

namespace Common {
// Runs only on the recorder's background writer, once per second. CPU time belongs to this
// process's own threads; it is not a sample of their current CPU number, affinity or wait state.
// No scheduling setter or driver/system configuration API is used.
inline void SampleOwnThreadCpu(PerformanceRecorder& recorder, double interval_ms) {
#ifdef _WIN32
    struct Previous {
        std::uint64_t creation{}, cpu{};
    };
    static thread_local std::unordered_map<DWORD, Previous> previous;
    std::unordered_map<DWORD, Previous> current;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return;
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(THREADENTRY32);
    const auto ticks = [](FILETIME time) {
        return (std::uint64_t{time.dwHighDateTime} << 32) | time.dwLowDateTime;
    };
    if (Thread32First(snapshot, &entry))
        do {
            if (entry.th32OwnerProcessID != GetCurrentProcessId())
                continue;
            const HANDLE thread =
                OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
            if (!thread)
                continue;
            FILETIME creation{}, exit{}, kernel{}, user{};
            if (GetThreadTimes(thread, &creation, &exit, &kernel, &user)) {
                const Previous measured{ticks(creation), ticks(kernel) + ticks(user)};
                current.emplace(entry.th32ThreadID, measured);
                const auto old = previous.find(entry.th32ThreadID);
                if (old != previous.end() && old->second.creation == measured.creation &&
                    measured.cpu >= old->second.cpu) {
                    PerformanceRecorder::Event event;
                    event.kind = PerformanceRecorder::Kind::CpuThread;
                    event.source =
                        entry.th32ThreadID; // thread ID for this row kind, not a frame ID
                    event.values[15] = double(measured.cpu - old->second.cpu) / 10000.0;
                    event.values[16] = interval_ms;
                    PWSTR name{};
                    if (SUCCEEDED(GetThreadDescription(thread, &name)) && name) {
                        WideCharToMultiByte(CP_UTF8, 0, name, -1, event.text.data(),
                                            static_cast<int>(event.text.size()), nullptr, nullptr);
                        event.text.back() = 0;
                        LocalFree(name);
                    }
                    recorder.Push(event);
                }
            }
            CloseHandle(thread);
        } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    previous.swap(current); // discard dead threads; creation time prevents ID-reuse deltas
#else
    (void)recorder;
    (void)interval_ms;
#endif
}
} // namespace Common
