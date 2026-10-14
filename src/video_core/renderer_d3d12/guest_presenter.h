// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/presenter_interface.h"
#include "presenter.h"
#include "common/guest_frame_trace.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "video_core/screenshot.h"
#endif
namespace NativeDX12 {
struct GuestSurfaceSnapshot {
    std::shared_ptr<Resource> texture;
    std::function<void()> wait_ready;
    ComPtr<ID3D12Fence> ready_fence;
    u64 ready_value{};
    DXGI_FORMAT view_format=DXGI_FORMAT_UNKNOWN;
    UINT component_mapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    bool srgb_input{};
    uint64_t diagnostic_capture_id{};
};
class GuestSurfaceProvider {
public:
    virtual ~GuestSurfaceProvider() = default;
    virtual void Register(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) = 0;
    virtual GuestSurfaceSnapshot Snapshot(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) = 0;
};
class GuestPresenter final : public VideoCore::PresenterInterface {
    struct Frame final : VideoCore::HostFrame {
        GuestPresenter* owner{};
        GuestSurfaceSnapshot surface;
        bool guest_flip{};
        void BeginGuestFlip() override {guest_flip=true;}
#ifdef SHADPS4_NATIVE_DX12_CORE
        std::vector<std::filesystem::path> game_screenshot_paths;
        VideoCore::ScreenshotFormat game_screenshot_format{};
        bool game_screenshot_hdr{};
#endif
        void TraceGpuCompletion() {
            if(!trace_id)return;
            if(!surface.ready_fence){Common::GuestFrameTrace::Invalid("guest-flip-without-ready-fence");return;}
            Common::GuestFrameTrace::Record(trace_id,"gpu-submitted",surface.ready_value);
            Common::GuestFrameTrace::Await(trace_id,surface.ready_value,
                [fence=surface.ready_fence,value=surface.ready_value]{const auto done=fence->GetCompletedValue();
                    return done==UINT64_MAX?Common::GuestFrameTrace::Completion::Failed:
                        done>=value?Common::GuestFrameTrace::Completion::Complete:Common::GuestFrameTrace::Completion::Pending;});
        }
    };
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    Presenter presenter_;
    GuestSurfaceProvider& surfaces_;
    UINT width_, height_;
    bool independent_vblank_{};
    std::mutex mutex_;
    std::unordered_map<Frame*,std::unique_ptr<Frame>> pending_;
    std::unique_ptr<Frame> last_;
    std::shared_ptr<Resource> blank_;
    Frame* Store(GuestSurfaceSnapshot);
public:
    GuestPresenter(ID3D12Device*, ID3D12CommandQueue*, HWND, UINT, UINT, GuestSurfaceProvider&);
    ~GuestPresenter() override;
    void RegisterSurface(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) override;
    VideoCore::HostFrame* PrepareFrame(const Libraries::VideoOut::BufferAttributeGroup&, VAddr) override;
    VideoCore::HostFrame* PrepareBlankFrame(bool) override;
    VideoCore::HostFrame* PrepareLastFrame() override;
    void Present(VideoCore::HostFrame*,bool=false,bool=true) override;
    void SetHDR(bool) override;
    bool IsHDRSupported() const override {return presenter_.HDRSupported();}
    void SetGamma(float) override;
    bool IndependentVblank() const override {return independent_vblank_;}
    bool UngatedDisplayCadence() const override {return independent_vblank_;}
    bool UsesNativePresentation() const override {return true;}
    void SetHostMarkersEnabled(bool enabled){presenter_.SetHostMarkersEnabled(enabled);}
    bool SkipRepeatedFrame() const override {return false;}
    void SetOverlay(std::function<void(ID3D12GraphicsCommandList*,ID3D12Resource*)> draw) { presenter_.SetOverlay(std::move(draw)); }
    void SetSubmitted(std::function<void(ID3D12Fence*,uint64_t)> callback){presenter_.SetSubmitted(std::move(callback));}
    void SetCompositor(std::function<PresentRegion(UINT,UINT,bool)> layout,
        std::function<void(ID3D12GraphicsCommandList*,ID3D12Resource*,const std::function<void()>&)> draw){
        presenter_.SetCompositor(std::move(layout),std::move(draw));
    }
};
}
