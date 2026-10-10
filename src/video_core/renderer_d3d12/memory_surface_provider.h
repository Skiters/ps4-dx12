// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_presenter.h"
#include "rasterizer.h"
#include "texture_resources.h"
namespace NativeDX12 {
// Called by VideoOut on Liverpool's GPU thread. Each flip owns a completed,
// immutable texture; the display thread never reads mutable guest backing.
class MemorySurfaceProvider final : public GuestSurfaceProvider {
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    Rasterizer& renderer_;
    UINT width_,height_;
    struct Context {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commands;
        std::unique_ptr<Descriptors> swap_views;
        TextureTransfer upload;
        std::shared_ptr<Resource> target;
        u64 fence{};
    };
    std::vector<Context> contexts_;
    size_t active_{};
    Timeline completion_;
    std::vector<std::shared_ptr<Resource>> snapshots_;
    ComPtr<ID3D12RootSignature> swap_root_;
    ComPtr<ID3D12PipelineState> swap_pipeline_;
    Context& AcquireContext();
    void SwapChannels(Context&,Resource&,Resource&);
public:
    MemorySurfaceProvider(ID3D12Device*,ID3D12CommandQueue*,Rasterizer&,UINT width,UINT height);
    ~MemorySurfaceProvider() override;
    void Register(const Libraries::VideoOut::BufferAttributeGroup&,VAddr) override;
    GuestSurfaceSnapshot Snapshot(const Libraries::VideoOut::BufferAttributeGroup&,VAddr) override;
    void Drain();
};
}
