// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include "core/libraries/videoout/buffer.h"

namespace VideoCore {
// Backend-owned frame. Ownership remains with the presenter until presentation
// completes. Guest flip acknowledgement remains the VideoOut driver's duty.
struct HostFrame {
    u64 trace_id{};
    virtual void BeginGuestFlip() {}
    virtual ~HostFrame() = default;
};
class PresenterInterface {
public:
    virtual ~PresenterInterface() = default;
    virtual void RegisterSurface(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) = 0;
    virtual HostFrame* PrepareFrame(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) = 0;
    virtual HostFrame* PrepareBlankFrame(bool) = 0;
    virtual HostFrame* PrepareLastFrame() = 0;
    virtual void Present(HostFrame*, bool reusing = false, bool game_frame = true) = 0;
    virtual void SetHDR(bool) = 0;
    virtual bool IsHDRSupported() const = 0;
    virtual void SetGamma(float) = 0;
    virtual bool IndependentVblank() const = 0;
    virtual bool UngatedDisplayCadence() const = 0;
    virtual bool UsesNativePresentation() const = 0;
    virtual bool SkipRepeatedFrame() const = 0;
};
}
