#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend_factory.h"
#include <d3d12sdklayers.h>
#include <cstdlib>
#include <atomic>
#include <cstdio>
namespace NativeDX12 {
namespace {
void CALLBACK DebugMessage(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
    D3D12_MESSAGE_ID id, LPCSTR description, void*) {
    static std::atomic<unsigned> warnings{},errors{};
    if(severity>D3D12_MESSAGE_SEVERITY_WARNING)return;
    // Repeated warnings must never consume the error-reporting budget.
    if(severity==D3D12_MESSAGE_SEVERITY_WARNING){if(warnings.fetch_add(1)>=64)return;}
    else if(errors.fetch_add(1)>=512)return;
    fprintf(stderr,"DX12 validation severity=%u id=%u: %s\n",unsigned(severity),unsigned(id),description);
    fflush(stderr);
}
}
ComPtr<ID3D12Device> CreateHardwareDevice() {
    if(std::getenv("SHADPS4_NATIVE_D3D12_VALIDATION")){
        ComPtr<ID3D12Debug> debug;
        if(SUCCEEDED(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))))DX12_API_CALL(debug->EnableDebugLayer());
    }
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const auto status=DX12_API_CALL(factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
        if(status==DXGI_ERROR_NOT_FOUND)break;Check(status);
        DXGI_ADAPTER_DESC1 description{};Check(DX12_API_CALL(adapter->GetDesc1(&description)));
        if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;
        if(SUCCEEDED(DX12_API_CALL(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))))break;
    }
    if(!device)throw std::runtime_error("No hardware DirectX 12 adapter");
    if(std::getenv("SHADPS4_NATIVE_D3D12_VALIDATION")){
        ComPtr<ID3D12InfoQueue1> info;
        DWORD cookie{};
        const auto status=DX12_API_CALL(device.As(&info));
        if(SUCCEEDED(status))Check(DX12_API_CALL(info->RegisterMessageCallback(DebugMessage,D3D12_MESSAGE_CALLBACK_FLAG_NONE,nullptr,&cookie)));
        else fprintf(stderr,"DX12 validation message callback unavailable: HRESULT=%08lx\n",(unsigned long)status);
    }
    return device;
}
std::unique_ptr<Backend> CreateBackend(const BackendConfiguration& configuration,GuestSurfaceProvider& surfaces) {
    if(configuration.backend!="dx12")throw std::invalid_argument("Native application requires backend=dx12");
    if(!configuration.window||!configuration.width||!configuration.height)throw std::invalid_argument("Invalid native window configuration");
    auto result=std::make_unique<Backend>();result->device=CreateHardwareDevice();
    result->capabilities=Capabilities::Query(result->device.Get());
    D3D12_COMMAND_QUEUE_DESC desc{};desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(result->device->CreateCommandQueue(&desc,IID_PPV_ARGS(&result->presentation_queue))));
    result->rasterizer=std::make_unique<Rasterizer>(result->device.Get());
    result->presenter=std::make_unique<GuestPresenter>(result->device.Get(),result->presentation_queue.Get(),configuration.window,configuration.width,configuration.height,surfaces);
    return result;
}
}
