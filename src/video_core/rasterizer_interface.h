// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string_view>
#include <fmt/format.h>
#include "common/types.h"

namespace VideoCore {
// GPU command-processor and memory-manager boundary. No graphics API types.
class RasterizerInterface {
public:
    virtual ~RasterizerInterface() = default;
    virtual void Draw(bool indexed, u32 offset = 0) = 0;
    virtual void DrawIndirect(bool indexed, VAddr address, u32 offset, u32 size,
                              u32 maximum, VAddr count, u16 vertex, u16 instance) = 0;
    virtual void DispatchDirect() = 0;
    virtual void DispatchIndirect(VAddr address, u32 offset, u32 size) = 0;
    virtual void ScopeMarkerBegin(const std::string_view& text, bool guest = false) = 0;
    virtual void ScopeMarkerEnd(bool guest = false) = 0;
    virtual void ScopedMarkerInsertColor(const std::string_view& text, u32 color,
                                         bool guest = false) = 0;
    virtual bool MarkersEnabled() const { return false; }
    template<class Function>
    void ScopeMarker(fmt::string_view format, fmt::format_args args, Function&& action) {
        if (!MarkersEnabled()) { action(); return; }
        ScopeMarkerBegin(fmt::vformat(format, args));
        action();
        ScopeMarkerEnd();
    }
    virtual void FillBuffer(VAddr address, u32 bytes, u32 value, bool gds) = 0;
    virtual void CopyBuffer(VAddr destination, VAddr source, u32 bytes,
                             bool destination_gds, bool source_gds) = 0;
    virtual u32 ReadDataFromGds(u32 offset) = 0;
    virtual bool InvalidateMemory(VAddr address, u64 size, bool assume_locks = false) = 0;
    virtual bool ReadMemory(VAddr address, u64 size, bool assume_locks = false) = 0;
    virtual bool IsMapped(VAddr address, u64 size) = 0;
    virtual void MapMemory(VAddr address, u64 size) = 0;
    virtual void RegisterMemory(VAddr address, u64 size) = 0;
    virtual void UnmapMemory(VAddr address, u64 size) = 0;
    virtual void NotifyMemoryProtection(VAddr address, u64 size) {}
    virtual void PrepareMemoryProtection(VAddr address, u64 size) {}
    // Notification from the owner after a successful committed host mapping,
    // not GPU coverage (which may contain sparse/reserved PRT holes).
    virtual void NotifyHostMapping(VAddr address, u64 size, bool readable, bool writable) {}
    virtual u64 Flush() = 0;
    virtual void Finish() = 0;
    virtual void OnSubmit() = 0;
    virtual void OnFence() = 0;
};
} // namespace VideoCore
