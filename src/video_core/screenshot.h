// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <filesystem>
#include <span>
#include <vector>

namespace VideoCore {
enum class ScreenshotKind : u8 { GameOnly,WithOverlays };
enum class ScreenshotFormat : u8 {
    R8G8B8A8Unorm,R8G8B8A8Srgb,B8G8R8A8Unorm,B8G8R8A8Srgb,
    A2R10G10B10UnormPack32,A2B10G10R10UnormPack32,
};
struct ScreenshotReadback {
    ScreenshotKind kind{};
    std::vector<std::filesystem::path> paths;
    struct {std::span<const u8> mapped_data;} buffer;
    u32 width{},height{};
    ScreenshotFormat format{};
    bool hdr_encoded{};
};
std::vector<std::filesystem::path> BuildScreenshotPaths(ScreenshotKind,u32 count);
void SavePendingScreenshot(const ScreenshotReadback&);
}
