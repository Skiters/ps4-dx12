// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_presenter.h"
#include "rasterizer.h"
#include "capabilities.h"
#include <string_view>
namespace NativeDX12 {
struct BackendConfiguration {
    std::string_view backend = "dx12";
    HWND window{};
    UINT width{},height{};
};
struct Backend {
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> presentation_queue;
    Capabilities capabilities;
    std::unique_ptr<Rasterizer> rasterizer;
    std::unique_ptr<GuestSurfaceProvider> owned_surfaces;
    std::unique_ptr<GuestPresenter> presenter;
};
// Provider snapshots must own immutable textures until Present returns. Any
// producer work on another queue must be completed by snapshot.wait_ready.
std::unique_ptr<Backend> CreateBackend(const BackendConfiguration&,GuestSurfaceProvider&);
ComPtr<ID3D12Device> CreateHardwareDevice();
}
