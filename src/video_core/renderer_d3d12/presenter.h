// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "host_fsr.h"
#include <dxgi1_6.h>
#include <array>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace NativeDX12 {
struct TextureTransfer;
using PresentCaptureCallback=std::function<void(UINT,UINT,DXGI_FORMAT,bool,std::span<const std::byte>)>;
struct PresentRegion {
    LONG x{},y{};UINT width{},height{};D3D12_RECT clip{};bool visible=true;
};
// Native presentation only. Caller owns the source texture and serializes its
// command recording with this context; source writes must precede Present on queue.
class Presenter {
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_;
    std::array<ComPtr<ID3D12Resource>,3> buffers_;
    std::array<ComPtr<ID3D12CommandAllocator>,3> allocators_;
    std::array<uint64_t,3> completed_{};
    std::array<std::shared_ptr<void>,3> source_lifetimes_;
    ComPtr<ID3D12GraphicsCommandList> commands_;
    Timeline timeline_;
    HWND window_{};
    UINT width_, height_;
    float gamma_=1.f;
    DXGI_FORMAT output_format_=DXGI_FORMAT_R8G8B8A8_UNORM;
    bool allow_tearing_{};
    bool host_markers_{};
    std::string present_mode_;
    UINT sync_interval_{},present_flags_{};
    bool hdr_supported_{},hdr_enabled_{};
    ComPtr<ID3D12RootSignature> scale_root_;
    ComPtr<ID3D12PipelineState> scale_pipeline_;
    std::unique_ptr<Descriptors> scale_srvs_,scale_rtvs_;
    FsrSettings fsr_settings_;
    std::unique_ptr<FsrPass> fsr_;
    std::array<std::shared_ptr<FsrPass::Lease>,3> fsr_leases_;
    bool using_fsr_{};
    UINT game_width_{},game_height_{};
    struct CaptureRequest {bool overlays;bool source;PresentCaptureCallback callback;};
    struct CaptureFrame;
    std::vector<CaptureRequest> capture_requests_;
    std::vector<std::shared_ptr<CaptureFrame>> pending_captures_;
    void CollectCaptures();
    void EnsureScaler();
    std::function<void(ID3D12GraphicsCommandList*, ID3D12Resource*)> overlay_;
    std::function<void(ID3D12Fence*,uint64_t)> submitted_;
    std::function<PresentRegion(UINT,UINT,bool)> layout_;
    std::function<void(ID3D12GraphicsCommandList*,ID3D12Resource*,const std::function<void()>&)> compositor_;
public:
    Presenter(ID3D12Device* device, ID3D12CommandQueue* queue, HWND window,
              UINT width, UINT height);
    ~Presenter();
    void Present(Resource& rgba8, TextureTransfer* capture = nullptr,
                 std::shared_ptr<void> source_lifetime = {},DXGI_FORMAT view_format=DXGI_FORMAT_UNKNOWN,
                 UINT mapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,bool srgb_input=false,bool reusing=false,
                 const std::function<void()>& before_present={});
    void Drain();
    void Capture(bool with_overlays,PresentCaptureCallback);
    void CaptureSource(PresentCaptureCallback);
    void Resize(UINT width,UINT height);
    UINT Width()const{return width_;}
    UINT Height()const{return height_;}
    UINT GameWidth()const{return game_width_;}
    UINT GameHeight()const{return game_height_;}
    void SetGamma(float gamma){gamma_=gamma;}
    void SetFsr(FsrSettings settings){fsr_settings_=settings;}
    void SetPresentMode(std::string_view mode);
    void SetHostMarkersEnabled(bool enabled){host_markers_=enabled;}
    bool UsingFsr()const{return using_fsr_;}
    void SetHDR(bool enabled);
    bool HDRSupported()const{return hdr_supported_;}
    DXGI_FORMAT OutputFormat()const{return output_format_;}
    void SetSubmitted(std::function<void(ID3D12Fence*,uint64_t)> callback){submitted_=std::move(callback);}
    void SetCompositor(std::function<PresentRegion(UINT,UINT,bool)> layout,
        std::function<void(ID3D12GraphicsCommandList*,ID3D12Resource*,const std::function<void()>&)> draw){
        if(!layout||!draw)throw std::invalid_argument("Missing native UI composition callback");
        layout_=std::move(layout);compositor_=std::move(draw);
    }
    void SetOverlay(std::function<void(ID3D12GraphicsCommandList*, ID3D12Resource*)> draw) { overlay_=std::move(draw); }
};
}
