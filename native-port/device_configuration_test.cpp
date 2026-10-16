// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/backend_factory.h"
#include <d3d12sdklayers.h>
#include <climits>
#include <cstdio>
#include <cstring>
using namespace NativeDX12;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::invalid_argument("Native device fixture requires one mode");
    _putenv_s("SHADPS4_NATIVE_D3D12_VALIDATION","");
    const std::string_view mode=argv[1];DeviceOptions options;
    if(mode=="invalid-adapter"){
        options.adapter_index=INT_MAX;bool rejected{};
        try{CreateHardwareDevice(nullptr,options);}catch(const std::runtime_error& error){
            rejected=std::string_view(error.what()).find("Selected hardware DX12 adapter is unavailable")!=std::string_view::npos;}
        if(!rejected)throw std::runtime_error("Native invalid explicit adapter silently selected a fallback");
        puts("PASS: explicit unavailable hardware adapter fails without fallback");return 0;
    }
    if(mode=="validation"){
        options.validation_enabled=true;options.validation_sync=true;options.validation_gpu=true;options.crash_diagnostics=true;
    }else if(mode!="default")throw std::invalid_argument("Unknown native device fixture mode");
    options.host_markers=options.guest_markers=true;
    auto device=CreateHardwareDevice(nullptr,options);const auto expected_luid=device->GetAdapterLuid();
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)));
    bool found{};for(UINT index=0;;++index){ComPtr<IDXGIAdapter1> adapter;
        const auto result=factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter));
        if(result==DXGI_ERROR_NOT_FOUND)break;Check(result);DXGI_ADAPTER_DESC1 description{};Check(adapter->GetDesc1(&description));
        if(memcmp(&description.AdapterLuid,&expected_luid,sizeof(expected_luid)))continue;
        options.adapter_index=int(index);auto selected=CreateHardwareDevice(nullptr,options);const auto luid=selected->GetAdapterLuid();
        if(memcmp(&luid,&expected_luid,sizeof(luid)))throw std::runtime_error("Native explicit adapter changed device identity");
        found=true;break;
    }
    if(!found)throw std::runtime_error("Native created adapter is absent from explicit selection inventory");
    if(mode=="validation"){ComPtr<ID3D12InfoQueue> messages;Check(device.As(&messages));
        ComPtr<ID3D12DeviceRemovedExtendedData1> dred;Check(device.As(&dred));}
    Rasterizer raster(device.Get());raster.SetMarkersEnabled(options.host_markers,options.guest_markers);
    auto source=Buffer(device.Get(),256,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    auto target=Buffer(device.Get(),256,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    const UINT value=0x89abcdef;void* mapped{};const D3D12_RANGE empty{};Check(source.handle->Map(0,&empty,&mapped));
    memcpy(mapped,&value,4);source.handle->Unmap(0,nullptr);
    raster.ScopeMarkerBegin("native-marker-submission");
    raster.ScopeMarkerBegin(std::string(1400,'x')+" percent:% marker",true);
    raster.ScopedMarkerInsertColor("colored guest marker",0x7fa012de,true);
    raster.RecordingList()->CopyBufferRegion(target.handle.Get(),0,source.handle.Get(),0,4);
    const auto fence=raster.Flush();if(!fence)throw std::runtime_error("Native marker/copy submission has no fence");
    raster.Finish();raster.ScopeMarkerEnd(true);raster.ScopeMarkerEnd();raster.Finish();
    const D3D12_RANGE read{0,4};Check(target.handle->Map(0,&read,&mapped));UINT actual{};memcpy(&actual,mapped,4);target.handle->Unmap(0,&empty);
    if(actual!=value)throw std::runtime_error("Native markers changed actual GPU copy contents");
    raster.ValidateDebugMessages();if(GetModuleHandleW(L"vulkan-1.dll"))throw std::runtime_error("Unexpected Vulkan native device execution");
    puts("PASS: configured native device, explicit LUID selection, GPU copy, long/color markers and scopes across submissions");return 0;
}catch(const std::exception& error){fprintf(stderr,"%s\n",error.what());return 1;}}
