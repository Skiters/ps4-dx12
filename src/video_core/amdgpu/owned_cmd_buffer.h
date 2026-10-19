// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <span>
#include <vector>

#include "common/types.h"

namespace AmdGpu {

struct OwnedCmdBufferStorage {
    std::vector<u32> dcb;
    std::vector<u32> ccb;
    uintptr_t dcb_source_addr{};
    uintptr_t ccb_source_addr{};
};

struct OwnedCmdBuffers {
    std::span<const u32> dcb;
    std::span<const u32> ccb;
    std::shared_ptr<OwnedCmdBufferStorage> storage;
};

inline OwnedCmdBuffers OwnCmdBuffers(std::span<const u32> dcb, std::span<const u32> ccb) {
    auto storage = std::make_shared<OwnedCmdBufferStorage>();
    storage->dcb_source_addr = reinterpret_cast<uintptr_t>(dcb.data());
    storage->ccb_source_addr = reinterpret_cast<uintptr_t>(ccb.data());
    storage->dcb.assign(dcb.begin(), dcb.end());
    storage->ccb.assign(ccb.begin(), ccb.end());
    return {storage->dcb, storage->ccb, std::move(storage)};
}

} // namespace AmdGpu
