// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/presenter_interface.h"
#include "presenter.h"
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
namespace NativeDX12 {
struct GuestSurfaceSnapshot {
    std::shared_ptr<Resource> texture;
    std::function<void()> wait_ready;
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
    };
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    Presenter presenter_;
    GuestSurfaceProvider& surfaces_;
    UINT width_, height_;
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
    bool IndependentVblank() const override {return true;}
    bool UngatedDisplayCadence() const override {return true;}
    bool UsesNativePresentation() const override {return true;}
    bool SkipRepeatedFrame() const override {return false;}
    void SetOverlay(std::function<void(ID3D12GraphicsCommandList*,ID3D12Resource*)> draw) { presenter_.SetOverlay(std::move(draw)); }
};
}
