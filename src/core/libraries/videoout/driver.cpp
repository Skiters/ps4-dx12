// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/debug.h"
#include "common/guest_frame_trace.h"
#include "common/thread.h"
#include "core/debug_state.h"
#include "core/emulator_settings.h"
#include "core/libraries/kernel/time.h"
#include "core/libraries/videoout/driver.h"
#include "core/libraries/videoout/native_present_queue.h"
#include "core/libraries/videoout/vblank_clock.h"
#include "core/libraries/videoout/videoout_error.h"
#include "imgui/renderer/imgui_core.h"
#include "video_core/amdgpu/liverpool.h"
#include "video_core/presenter_interface.h"


#include <iostream>

extern std::unique_ptr<VideoCore::PresenterInterface> video_presenter;
extern std::unique_ptr<AmdGpu::Liverpool> liverpool;

namespace Libraries::VideoOut {

constexpr static bool Is32BppPixelFormat(PixelFormat format) {
    switch (format) {
    case PixelFormat::A8R8G8B8Srgb:
    case PixelFormat::A8B8G8R8Srgb:
    case PixelFormat::A2R10G10B10:
    case PixelFormat::A2R10G10B10Srgb:
    case PixelFormat::A2R10G10B10Bt2020Pq:
        return true;
    default:
        return false;
    }
}

constexpr u32 PixelFormatBpp(PixelFormat pixel_format) {
    switch (pixel_format) {
    case PixelFormat::A16R16G16B16Float:
        return 8;
    default:
        return 4;
    }
}

VideoOutDriver::VideoOutDriver(u32 width, u32 height) {
    main_port.resolution.full_width = width;
    main_port.resolution.full_height = height;
    main_port.resolution.pane_width = width;
    main_port.resolution.pane_height = height;
    // Both FG backends may wait for interpolation/presentation. Guest vblank
    // must advance independently, including when an output frame is dropped.
    independent_vblank = video_presenter->IndependentVblank();
    if (independent_vblank)
        vblank_thread = std::jthread([this](std::stop_token token) { VblankThread(token); });
    present_thread = std::jthread([this](std::stop_token token) { PresentThread(token); });
}

VideoOutDriver::~VideoOutDriver() {
    present_thread.request_stop();
    vblank_thread.request_stop();
    request_cv.notify_all();
    main_port.vblank_cv.notify_all();
}

int VideoOutDriver::Open(const ServiceThreadParams* params) {
    if (main_port.is_open) {
        return ORBIS_VIDEO_OUT_ERROR_RESOURCE_BUSY;
    }
    main_port.is_open = true;
    liverpool->SetVoPort(&main_port);
    return 1;
}

void VideoOutDriver::Close(s32 handle) {
    std::scoped_lock lock{mutex, main_port.vo_mutex};

    // Mark as closed
    main_port.is_open = false;
    main_port.flip_rate = 0;
    main_port.prev_index = -1;

    // Clear port information
    std::memset(main_port.buffer_labels.data(), 0, sizeof(main_port.buffer_labels));
    std::memset(main_port.groups.data(), 0, sizeof(main_port.groups));
    std::memset(&main_port.vblank_status, 0, sizeof(main_port.vblank_status));
    main_port.flip_status = FlipStatus{};

    // Re-initialize buffers
    std::memset(main_port.buffer_slots.data(), 0, sizeof(main_port.buffer_slots));
    for (auto& buffer : main_port.buffer_slots) {
        buffer.group_index = -1;
    }

    // Clear events
    for (auto event : main_port.flip_events) {
        auto equeue = Kernel::GetEqueue(event);
        if (equeue != nullptr) {
            equeue->RemoveEvent(static_cast<u64>(OrbisVideoOutInternalEventId::Flip),
                                Kernel::OrbisKernelEvent::Filter::VideoOut);
        }
    }
    main_port.flip_events.clear();
    for (auto event : main_port.vblank_events) {
        auto equeue = Kernel::GetEqueue(event);
        if (equeue != nullptr) {
            equeue->RemoveEvent(static_cast<u64>(OrbisVideoOutInternalEventId::Vblank),
                                Kernel::OrbisKernelEvent::Filter::VideoOut);
        }
    }
    main_port.vblank_events.clear();
}

VideoOutPort* VideoOutDriver::GetPort(int handle) {
    if (handle != 1) [[unlikely]] {
        return nullptr;
    }
    return &main_port;
}

int VideoOutDriver::RegisterBuffers(VideoOutPort* port, s32 startIndex, void* const* addresses,
                                    s32 bufferNum, const BufferAttribute* attribute) {
    const s32 group_index = port->FindFreeGroup();
    if (group_index >= MaxDisplayBufferGroups) {
        return ORBIS_VIDEO_OUT_ERROR_NO_EMPTY_SLOT;
    }

    if (startIndex + bufferNum > MaxDisplayBuffers || startIndex > MaxDisplayBuffers ||
        bufferNum > MaxDisplayBuffers) {
        LOG_ERROR(Lib_VideoOut,
                  "Attempted to register too many buffers startIndex = {}, bufferNum = {}",
                  startIndex, bufferNum);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    const s32 end_index = startIndex + bufferNum;
    if (bufferNum > 0 &&
        std::any_of(port->buffer_slots.begin() + startIndex, port->buffer_slots.begin() + end_index,
                    [](auto& buffer) { return buffer.group_index != -1; })) {
        return ORBIS_VIDEO_OUT_ERROR_SLOT_OCCUPIED;
    }

    if (attribute->reserved0 != 0 || attribute->reserved1 != 0) {
        LOG_ERROR(Lib_VideoOut, "Invalid reserved members");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (attribute->aspect_ratio != 0) {
        LOG_ERROR(Lib_VideoOut, "Invalid aspect ratio = {}", attribute->aspect_ratio);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ASPECT_RATIO;
    }
    if (attribute->width > attribute->pitch_in_pixel) {
        LOG_ERROR(Lib_VideoOut, "Buffer width {} is larger than pitch {}", attribute->width,
                  attribute->pitch_in_pixel);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_PITCH;
    }
    if (attribute->tiling_mode < TilingMode::Tile || attribute->tiling_mode > TilingMode::Linear) {
        LOG_ERROR(Lib_VideoOut, "Invalid tilingMode = {}",
                  static_cast<u32>(attribute->tiling_mode));
        return ORBIS_VIDEO_OUT_ERROR_INVALID_TILING_MODE;
    }

    LOG_INFO(Lib_VideoOut,
             "startIndex = {}, bufferNum = {}, pixelFormat = {}, aspectRatio = {}, "
             "tilingMode = {}, width = {}, height = {}, pitchInPixel = {}, option = {:#x}",
             startIndex, bufferNum, GetPixelFormatString(attribute->pixel_format),
             attribute->aspect_ratio, static_cast<u32>(attribute->tiling_mode), attribute->width,
             attribute->height, attribute->pitch_in_pixel, attribute->option);

    auto& group = port->groups[group_index];
    std::memcpy(&group.attrib, attribute, sizeof(BufferAttribute));
    group.is_occupied = true;

    for (u32 i = 0; i < bufferNum; i++) {
        const uintptr_t address = reinterpret_cast<uintptr_t>(addresses[i]);
        port->buffer_slots[startIndex + i] = VideoOutBuffer{
            .group_index = group_index,
            .address_left = address,
            .address_right = 0,
        };

        // Reset flip label also when registering buffer
        port->buffer_labels[startIndex + i] = 0;
        port->SignalVoLabel();

        video_presenter->RegisterSurface(group, address);
        LOG_INFO(Lib_VideoOut, "buffers[{}] = {:#x}", i + startIndex, address);
    }

    return group_index;
}

int VideoOutDriver::UnregisterBuffers(VideoOutPort* port, s32 attributeIndex) {
    if (attributeIndex >= MaxDisplayBufferGroups || !port->groups[attributeIndex].is_occupied) {
        LOG_ERROR(Lib_VideoOut, "Invalid attribute index {}", attributeIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    auto& group = port->groups[attributeIndex];
    group.is_occupied = false;

    for (auto& buffer : port->buffer_slots) {
        if (buffer.group_index != attributeIndex) {
            continue;
        }
        buffer.group_index = -1;
    }

    return ORBIS_OK;
}

int VideoOutDriver::ChangeBufferAttribute(VideoOutPort* port, s32 attributeIndex,
                                          const BufferAttribute* attribute) {
    if (attributeIndex >= MaxDisplayBufferGroups || !port->groups[attributeIndex].is_occupied) {
        LOG_ERROR(Lib_VideoOut, "Invalid attribute index {}", attributeIndex);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }

    if (attribute->reserved0 != 0 || attribute->reserved1 != 0) {
        LOG_ERROR(Lib_VideoOut, "Invalid reserved members");
        return ORBIS_VIDEO_OUT_ERROR_INVALID_VALUE;
    }
    if (attribute->aspect_ratio != 0) {
        LOG_ERROR(Lib_VideoOut, "Invalid aspect ratio = {}", attribute->aspect_ratio);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_ASPECT_RATIO;
    }
    if (attribute->width > attribute->pitch_in_pixel) {
        LOG_ERROR(Lib_VideoOut, "Buffer width {} is larger than pitch {}", attribute->width,
                  attribute->pitch_in_pixel);
        return ORBIS_VIDEO_OUT_ERROR_INVALID_PITCH;
    }
    if (attribute->tiling_mode < TilingMode::Tile || attribute->tiling_mode > TilingMode::Linear) {
        LOG_ERROR(Lib_VideoOut, "Invalid tilingMode = {}",
                  static_cast<u32>(attribute->tiling_mode));
        return ORBIS_VIDEO_OUT_ERROR_INVALID_TILING_MODE;
    }

    LOG_INFO(Lib_VideoOut,
             "attributeIndex = {}, pixelFormat = {}, aspectRatio = {}, "
             "tilingMode = {}, width = {}, height = {}, pitchInPixel = {}, option = {:#x}",
             attributeIndex, GetPixelFormatString(attribute->pixel_format), attribute->aspect_ratio,
             static_cast<u32>(attribute->tiling_mode), attribute->width, attribute->height,
             attribute->pitch_in_pixel, attribute->option);

    std::unique_lock lock{port->port_mutex};
    std::memcpy(&port->groups[attributeIndex].attrib, attribute, sizeof(BufferAttribute));
    return 0;
}

void VideoOutDriver::Flip(const Request& req) {
    // Update HDR status before presenting.
    video_presenter->SetHDR(req.port->is_hdr);

    // Present the frame.
    const auto trace_id=req.frame->trace_id;
    video_presenter->Present(req.frame);
    Common::GuestFrameTrace::Record(trace_id,"present-queued");

    // Update flip status.
    auto* port = req.port;
    {
        std::unique_lock lock{port->port_mutex};
        auto& flip_status = port->flip_status;
        flip_status.count++;
        flip_status.process_time = Libraries::Kernel::sceKernelGetProcessTime();
        flip_status.tsc = Libraries::Kernel::sceKernelReadTsc();
        flip_status.flip_arg = req.flip_arg;
        flip_status.current_buffer = req.index;
        if (req.eop) {
            --flip_status.gc_queue_num;
        }
        --flip_status.flip_pending_num;
    }

    // Trigger flip events for the port.
    for (auto event : port->flip_events) {
        auto equeue = Kernel::GetEqueue(event);
        if (equeue != nullptr) {
            equeue->TriggerEvent(
                static_cast<u64>(OrbisVideoOutInternalEventId::Flip),
                Kernel::OrbisKernelEvent::Filter::VideoOut,
                reinterpret_cast<void*>(static_cast<u64>(OrbisVideoOutInternalEventId::Flip) |
                                        (req.flip_arg << 16)));
        }
    }

    // Reset prev flip label
    if (port->prev_index != -1) {
        port->buffer_labels[port->prev_index] = 0;
        port->SignalVoLabel();
    }
    // save to prev buf index
    port->prev_index = req.index;
}

void VideoOutDriver::DrawBlankFrame() {
    const auto empty_frame = video_presenter->PrepareBlankFrame(true);
    video_presenter->Present(empty_frame, false, false);
}

void VideoOutDriver::DrawLastFrame() {
    if (video_presenter->SkipRepeatedFrame())
        return;
    const auto frame = video_presenter->PrepareLastFrame();
    if (frame != nullptr) {
        video_presenter->Present(frame, true);
    }
}

bool VideoOutDriver::SubmitFlip(VideoOutPort* port, s32 index, s64 flip_arg,
                                bool is_eop /*= false*/, u64 trace_id) {
    {
        std::unique_lock lock{port->port_mutex};
        if (index != -1 && port->flip_status.flip_pending_num > 16) {
            LOG_ERROR(Lib_VideoOut, "Flip queue is full");
            return false;
        }

        if (is_eop) {
            ++port->flip_status.gc_queue_num;
        }
        ++port->flip_status.flip_pending_num; // integral GPU and CPU pending flips counter
        port->flip_status.submit_tsc = Libraries::Kernel::sceKernelReadTsc();
    }

    if(index!=-1&&!trace_id)trace_id=Common::GuestFrameTrace::New();
    if (!is_eop) {
        // Non EOP flips can arrive from any thread so ask GPU thread to perform them
        liverpool->SendCommand([=, this]() { SubmitFlipInternal(port, index, flip_arg, is_eop, trace_id); });
    } else {
        SubmitFlipInternal(port, index, flip_arg, is_eop, trace_id);
    }

    return true;
}

void VideoOutDriver::SubmitFlipInternal(VideoOutPort* port, s32 index, s64 flip_arg, bool is_eop, u64 trace_id) {
    Common::GuestFrameTrace::Record(trace_id,is_eop?"pm4-flip-ready":"gpu-thread-ready");
    VideoCore::HostFrame* frame;
    if (index == -1) {
        frame = video_presenter->PrepareBlankFrame(false);
    } else {
        const auto& buffer = port->buffer_slots[index];
        ASSERT_MSG(buffer.group_index >= 0, "Trying to flip an unregistered buffer!");
        const auto& group = port->groups[buffer.group_index];
        frame = video_presenter->PrepareFrame(group, buffer.address_left);
    }

    frame->trace_id=trace_id;
    Common::GuestFrameTrace::Record(trace_id,"prepared");
    frame->TraceGpuCompletion();
    {
        std::scoped_lock lock{mutex};
        requests.push({
            .frame = frame,
            .port = port,
            .flip_arg = flip_arg,
            .index = index,
            .eop = is_eop,
        });
    }
    request_cv.notify_one();
}

void VideoOutDriver::PresentThread(std::stop_token token) {
    const std::chrono::nanoseconds vblank_period(1000000000 /
                                                 EmulatorSettings.GetVblankFrequency());

    Common::SetCurrentThreadName("shadPS4:PresentThread");
    Common::SetCurrentThreadRealtime(vblank_period);

    Common::AccurateTimer timer{vblank_period};

    const auto receive_request = [this] -> Request {
        std::scoped_lock lk{mutex};
        if (!requests.empty()) {
            const auto request = requests.front();
            requests.pop();
            return request;
        }
        return {};
    };

    // Output admission belongs to the FSR presenter, not real frame delivery.
    NativePresentCadence native_cadence{
        video_presenter->UngatedDisplayCadence() ? std::chrono::nanoseconds{0} : vblank_period};
    const auto native_request_mode = [this] {
        return independent_vblank && video_presenter &&
               video_presenter->UsesNativePresentation() &&
               main_port.flip_rate.load() == 0;
    };
    bool was_native_request_mode{};
    bool bound_gated_transition{};
    u64 last_vblank{};
    while (!token.stop_requested()) {
        if (native_request_mode()) {
            was_native_request_mode = true;
            Request request{};
            NativePresentWaitResult result;
            {
                std::unique_lock lock{mutex};
                result = WaitNativePresentRequest(
                    request_cv, lock, token, vblank_period, native_cadence,
                    [this] { return !requests.empty(); },
                    [] { return DebugState.IsGuestThreadsPaused(); }, native_request_mode);
                if (result == NativePresentWaitResult::Ready) {
                    // These controls can change while the condition variable releases the lock.
                    if (token.stop_requested()) {
                        result = NativePresentWaitResult::Stopped;
                    } else if (!native_request_mode()) {
                        result = NativePresentWaitResult::ModeChanged;
                    } else if (DebugState.IsGuestThreadsPaused()) {
                        result = NativePresentWaitResult::Paused;
                    } else {
                        request = requests.front();
                        requests.pop();
                    }
                }
            }
            if (result == NativePresentWaitResult::Stopped)
                break;
            if (result == NativePresentWaitResult::ModeChanged)
                continue;
            if (result == NativePresentWaitResult::Paused) {
                DrawLastFrame();
            } else if (request) {
                native_cadence.Dispatched(NativePresentCadence::Clock::now());
                Flip(request);
                FRAME_END;
            } else if (!main_port.is_open) {
                DrawBlankFrame();
            } else if (ImGui::Core::MustKeepDrawing()) {
                DrawLastFrame();
            }
            continue;
        }
        if (was_native_request_mode) {
            // Do not consume an old tick when returning to guest-vblank phase gating.
            std::scoped_lock lock{main_port.vo_mutex};
            last_vblank = main_port.vblank_status.count;
            was_native_request_mode = false;
            bound_gated_transition = true;
        }
        if (independent_vblank) {
            std::unique_lock lock{main_port.vo_mutex};
            main_port.vblank_cv.wait_for(lock, vblank_period, [&] {
                return token.stop_requested() || main_port.vblank_status.count != last_vblank;
            });
            if (token.stop_requested())
                break;
            const auto count = main_port.vblank_status.count;
            if (count == last_vblank)
                continue;
            last_vblank = count;
            // Skip missed host presents instead of replaying a catch-up burst.
            if ((count - 1) % (main_port.flip_rate.load() + 1) != 0)
                continue;
        } else {
            timer.Start();
        }

        if (DebugState.IsGuestThreadsPaused()) {
            DrawLastFrame();
            if (!independent_vblank)
                timer.End();
            continue;
        }

        // A rate change during the vblank wait must route through the native cadence too.
        if (native_request_mode())
            continue;

        // Check if it's time to take a request.
        auto& vblank_status = main_port.vblank_status;
        if (independent_vblank || vblank_status.count % (main_port.flip_rate.load() + 1) == 0) {
            if (bound_gated_transition) {
                if (!native_cadence.Ready(NativePresentCadence::Clock::now()))
                    continue;
                bound_gated_transition = false;
            }
            const auto request = receive_request();
            if (!request) {
                if (independent_vblank || timer.GetTotalWait().count() < 0) {
                    if (!main_port.is_open) {
                        DrawBlankFrame();
                    } else if (ImGui::Core::MustKeepDrawing()) {
                        DrawLastFrame();
                    }
                }
            } else {
                native_cadence.Dispatched(NativePresentCadence::Clock::now());
                Flip(request);
                FRAME_END;
            }
        }

        if (!independent_vblank) {
            SignalVblank();
            timer.End();
        }
    }
}

void VideoOutDriver::SignalVblank(u64 ticks) {
    std::scoped_lock lock{main_port.vo_mutex};
    auto& status = main_port.vblank_status;
    status.count += ticks;
    status.process_time = Kernel::sceKernelGetProcessTime();
    status.tsc = Kernel::sceKernelReadTsc();
    for (auto event : main_port.vblank_events) {
        if (auto equeue = Kernel::GetEqueue(event)) {
            equeue->TriggerEvent(static_cast<u64>(OrbisVideoOutInternalEventId::Vblank),
                Kernel::OrbisKernelEvent::Filter::VideoOut,
                reinterpret_cast<void*>(static_cast<u64>(OrbisVideoOutInternalEventId::Vblank) |
                                        ((status.count - 1) << 16)));
        }
    }
    main_port.vblank_cv.notify_all();
}

void VideoOutDriver::VblankThread(std::stop_token token) {
    Common::SetCurrentThreadName("shadPS4:VblankThread");
    const auto period = std::chrono::nanoseconds(1000000000 / EmulatorSettings.GetVblankFrequency());
    VblankClock clock{VblankClock::Clock::now(), period};
    LOG_INFO(Render_DX12, "[VBLANK] Independent guest clock at {} Hz; FG Present cannot delay vblank",
             EmulatorSettings.GetVblankFrequency());
    while (!token.stop_requested()) {
        std::this_thread::sleep_until(clock.Deadline());
        if (token.stop_requested())
            break;
        if (const auto ticks = clock.Advance(VblankClock::Clock::now()))
            SignalVblank(ticks);
    }
}

int VideoOutDriver::TimingSelfTest() {
    using namespace std::chrono;
    const auto origin = VblankClock::Clock::time_point{};
    const auto period = nanoseconds{16666666};
    VblankClock clock{origin, period};
    if (clock.Advance(origin) || clock.Advance(origin + period) != 1 ||
        clock.Advance(origin + period) || clock.Advance(origin + period * 11) != 10 ||
        clock.Deadline() != origin + period * 12 || clock.Advance(origin + period * 11))
        return 1;
    std::atomic<u64> ticks{};
    std::atomic<bool> started{};
    std::jthread worker([&](std::stop_token stop) {
        VblankClock independent{VblankClock::Clock::now(), milliseconds{5}};
        started = true;
        while (!stop.stop_requested()) {
            std::this_thread::sleep_until(independent.Deadline());
            ticks += independent.Advance(VblankClock::Clock::now());
        }
    });
    while (!started)
        std::this_thread::yield();
    // A simulated blocking host Present must not stop the independent clock.
    std::this_thread::sleep_for(milliseconds{100});
    worker.request_stop();
    worker.join();
    if (ticks < 10)
        return 1;
    std::cout << "Vblank clock OK: missed ticks advanced without bursts; independent clock advanced "
              << ticks << " ticks during blocked host work; no GPU, audio or game\n";
    return 0;
}

} // namespace Libraries::VideoOut

