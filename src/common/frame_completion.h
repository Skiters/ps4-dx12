// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <functional>
#include <utility>

namespace Common {
// Stack-owned by VideoOut::Flip and used synchronously on the presentation thread.
// The GPU-ready path and the legacy/error fallback must acknowledge a frame only once.
class FrameCompletion {
  public:
    explicit FrameCompletion(std::function<void()> callback) : callback{std::move(callback)} {}
    FrameCompletion(const FrameCompletion &) = delete;
    FrameCompletion &operator=(const FrameCompletion &) = delete;

    void Complete() {
        if (!std::exchange(completed, true))
            callback();
    }

  private:
    std::function<void()> callback;
    bool completed{};
};
} // namespace Common
