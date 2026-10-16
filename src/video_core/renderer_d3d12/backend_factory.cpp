#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "backend_factory.h"
#include "common/gpu_identity.h"
#include "ags_shader_clock.h"
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
ComPtr<ID3D12Device> CreateHardwareDevice(std::shared_ptr<void>* driver_lifetime,const DeviceOptions& options) {
    const auto* validation_override=std::getenv("SHADPS4_NATIVE_D3D12_VALIDATION");
    const bool validation=validation_override?validation_override[0]&&validation_override[0]!='0':
        options.validation_enabled&&(options.validation_core||options.validation_sync||options.validation_gpu);
    if(validation){
        ComPtr<ID3D12Debug> debug;
        Check(DX12_API_CALL(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))));DX12_API_CALL(debug->EnableDebugLayer());
        ComPtr<ID3D12Debug1> extended;Check(debug.As(&extended));
        extended->SetEnableSynchronizedCommandQueueValidation(options.validation_sync);
        extended->SetEnableGPUBasedValidation(options.validation_gpu);
        if(!options.validation_core&&(options.validation_sync||options.validation_gpu))
            fprintf(stderr,"DX12 synchronization/GPU validation also requires the core debug layer\n");
    }
    if(options.crash_diagnostics){
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> settings;
        Check(D3D12GetDebugInterface(IID_PPV_ARGS(&settings)));
        settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> context;Check(settings.As(&context));
        context->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }
    ComPtr<ID3D12Device> device;
    ComPtr<IDXGIFactory6> factory;Check(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))));
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> adapter;
        const auto status=DX12_API_CALL(factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter)));
        if(status==DXGI_ERROR_NOT_FOUND)break;Check(status);
        if(options.adapter_index>=0&&index!=UINT(options.adapter_index))continue;
        DXGI_ADAPTER_DESC1 description{};Check(DX12_API_CALL(adapter->GetDesc1(&description)));
        if(description.Flags&DXGI_ADAPTER_FLAG_SOFTWARE){
            if(options.adapter_index>=0)throw std::runtime_error("Selected DX12 adapter is a software renderer");
            continue;
        }
        if(description.VendorId==0x1002&&driver_lifetime){device=CreateAmdHardwareDevice(adapter.Get(),*driver_lifetime);break;}
        const auto created=DX12_API_CALL(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)));
        if(SUCCEEDED(created))break;
        if(options.adapter_index>=0)Check(created);
    }
    if(!device)throw std::runtime_error(options.adapter_index>=0?"Selected hardware DX12 adapter is unavailable":"No hardware DirectX 12 adapter");
    if(GetEnvironmentVariableW(L"SHADPS4_GPU_IDENTITY_LOG",nullptr,0)){
        const auto luid=device->GetAdapterLuid();ComPtr<IDXGIAdapter1> adapter;
        Check(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(&adapter)));
        DXGI_ADAPTER_DESC1 desc{};Check(adapter->GetDesc1(&desc));
        char name[1024]{};const auto converted=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,desc.Description,-1,name,1024,nullptr,nullptr);
        Common::GpuIdentity::Device identity{};identity.backend="dx12";
        identity.vendor=desc.VendorId;identity.device=desc.DeviceId;
        identity.nodes=device->GetNodeCount();identity.node_mask=1;
        memcpy(identity.luid.data(),&luid,sizeof(luid));identity.luid_valid=true;
        identity.name=converted?name:"";
        if(!Common::GpuIdentity::Write(identity)||!converted)
            Common::GuestFrameTrace::Invalid("gpu-identity-record-error");
    }
    if(validation){
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
    auto result=std::make_unique<Backend>();result->device=CreateHardwareDevice(&result->driver_lifetime,configuration.device);
    result->capabilities=Capabilities::Query(result->device.Get());
    D3D12_COMMAND_QUEUE_DESC desc{};desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(result->device->CreateCommandQueue(&desc,IID_PPV_ARGS(&result->presentation_queue))));
    result->rasterizer=std::make_unique<Rasterizer>(result->device.Get());
    result->rasterizer->SetMarkersEnabled(configuration.device.host_markers,configuration.device.guest_markers);
    result->presenter=std::make_unique<GuestPresenter>(result->device.Get(),result->presentation_queue.Get(),configuration.window,configuration.width,configuration.height,surfaces);
    static_cast<GuestPresenter*>(result->presenter.get())->SetHostMarkersEnabled(configuration.device.host_markers);
    return result;
}
}
