// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/backend_factory.h"
#include <dxgi1_6.h>
#include <cstdio>
namespace NativeDX12 {
// Headless tests select a device by the mechanism's required capability.
// This does not change the application's configured/default adapter.
inline ComPtr<ID3D12Device> CreateFormatTestDevice(DXGI_FORMAT format){
    if(format==DXGI_FORMAT_UNKNOWN)return CreateHardwareDevice();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> adapter;
        const auto hr=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(hr==DXGI_ERROR_NOT_FOUND)break;Check(hr);DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        DeviceOptions options;options.adapter_index=int(i);auto device=CreateHardwareDevice(nullptr,options);
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{format};
        const auto required=D3D12_FORMAT_SUPPORT1_RENDER_TARGET|D3D12_FORMAT_SUPPORT1_BLENDABLE;
        if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)))||
           (support.Support1&required)!=required)continue;
        printf("FIXTURE adapter=%u vendor=%04x device=%04x required_rt=%u name=%ls\n",i,desc.VendorId,desc.DeviceId,unsigned(format),desc.Description);
        return device;
    }
    throw std::runtime_error("No hardware adapter supports the required test render format");
}
} // namespace NativeDX12
