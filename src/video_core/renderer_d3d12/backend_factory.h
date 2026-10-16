// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_presenter.h"
#include "rasterizer.h"
#include "capabilities.h"
#include <string_view>
namespace NativeDX12 {
struct DeviceOptions {
    int adapter_index=-1;
    bool validation_enabled=false,validation_core=true,validation_sync=false,validation_gpu=false;
    bool crash_diagnostics=false,host_markers=false,guest_markers=false;
};
struct BackendConfiguration {
    std::string_view backend = "dx12";
    HWND window{};
    UINT width{},height{};
    DeviceOptions device;
};
struct Backend {
    std::shared_ptr<void> driver_lifetime;
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
ComPtr<ID3D12Device> CreateHardwareDevice(std::shared_ptr<void>* driver_lifetime=nullptr,const DeviceOptions& options={});
}
