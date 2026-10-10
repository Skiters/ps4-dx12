#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "capabilities.h"
#include <dxgi1_6.h>
#include <cstdio>
using namespace NativeDX12;
int main() {
    try {
        ComPtr<IDXGIFactory6> factory;
        Check(DX12_API_CALL(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))));
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            if (DX12_API_CALL(factory->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&adapter))) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC1 description{};
            Check(DX12_API_CALL(adapter->GetDesc1(&description)));
            if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            ComPtr<ID3D12Device> device;
            if (FAILED(DX12_API_CALL(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device))))) continue;
            const auto caps = Capabilities::Query(device.Get());
            wprintf(L"Hardware: %ls\n", description.Description);
            printf("ShaderModel=0x%x BindingTier=%u Barycentrics=%d WaveOps=%d Native16=%d\n",
                unsigned(caps.shader_model), unsigned(caps.resource_binding_tier), caps.barycentrics,
                caps.wave_operations, caps.native_16bit);
            if (GetModuleHandleW(L"vulkan-1.dll")) throw std::runtime_error("Unexpected Vulkan runtime");
            return caps.shader_model >= D3D_SHADER_MODEL_6_2 ? 0 : 1;
        }
        throw std::runtime_error("No hardware D3D12 adapter");
    } catch (const std::exception& error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
