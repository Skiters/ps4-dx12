// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include <dxgi1_6.h>
#include <array>
#include <functional>
#include <memory>
namespace NativeDX12 {
struct TextureTransfer;
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
    UINT width_, height_;
    float gamma_=1.f;
    DXGI_FORMAT output_format_=DXGI_FORMAT_R8G8B8A8_UNORM;
    bool allow_tearing_{};
    bool hdr_supported_{},hdr_enabled_{};
    ComPtr<ID3D12RootSignature> scale_root_;
    ComPtr<ID3D12PipelineState> scale_pipeline_;
    std::unique_ptr<Descriptors> scale_srvs_,scale_rtvs_;
    void EnsureScaler();
    std::function<void(ID3D12GraphicsCommandList*, ID3D12Resource*)> overlay_;
public:
    Presenter(ID3D12Device* device, ID3D12CommandQueue* queue, HWND window,
              UINT width, UINT height);
    ~Presenter();
    // Same-size copy or native GPU contrast-adaptive spatial upscale.
    void Present(Resource& rgba8, TextureTransfer* capture = nullptr,
                 std::shared_ptr<void> source_lifetime = {});
    void Drain();
    void SetGamma(float gamma){gamma_=gamma;}
    void SetHDR(bool enabled);
    bool HDRSupported()const{return hdr_supported_;}
    void SetOverlay(std::function<void(ID3D12GraphicsCommandList*, ID3D12Resource*)> draw) { overlay_=std::move(draw); }
};
}
