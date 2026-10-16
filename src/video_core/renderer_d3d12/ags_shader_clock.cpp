// SPDX-License-Identifier: GPL-2.0-or-later
#include "ags_shader_clock.h"
#include "common/runtime_module_trace.h"
#include "persistent_cache.h"
#include <windows.h>
#include <dxgi.h>
#define AGS_GCC
#define AGS_EXCLUDE_DIRECTX_11
#include <amd_ags.h>
#undef AGS_EXCLUDE_DIRECTX_11
#undef AGS_GCC
#include <cstdio>
namespace NativeDX12 {
namespace {
constexpr GUID ClockCapabilitiesGuid{0x2cd92748,0x7dce,0x4a50,{0xa1,0xaf,0x53,0x9e,0xbd,0xcc,0x20,0x2d}};
struct ClockCapabilities {HMODULE module{};UINT supported{};};
std::wstring ModulePath(HMODULE module){
    std::wstring path(32768,L'\0');const auto size=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(!size||size>=path.size())throw std::runtime_error("Cannot identify AGS runtime path");
    path.resize(size);return path;
}
void CheckAgs(AGSReturnCode status){
    if(status!=AGS_SUCCESS)throw std::runtime_error("AGS native device operation failed: "+std::to_string(int(status)));
}
struct DriverLease {
    HMODULE module{};AGSContext* context{};ID3D12Device* device{};
    AGS_DEINITIALIZE deinitialize{};AGS_DRIVEREXTENSIONSDX12_DESTROYDEVICE destroy{};
    ~DriverLease(){
        if(device){
            const auto cleared=device->SetPrivateData(ClockCapabilitiesGuid,0,nullptr);
            unsigned references{};const auto status=destroy(context,device,&references);
            if(FAILED(cleared)||status!=AGS_SUCCESS||references){
                std::fprintf(stderr,"AGS device retirement failed: HRESULT=%08lx status=%d references=%u\n",
                    static_cast<unsigned long>(cleared),int(status),references);std::terminate();
            }
        }
        if(context){const auto status=deinitialize(context);
            if(status!=AGS_SUCCESS){std::fprintf(stderr,"AGS context retirement failed: %d\n",int(status));std::terminate();}}
        if(module)FreeLibrary(module);
    }
    template<class T>T Symbol(const char* name){
        const auto address=GetProcAddress(module,name);
        if(!address)throw std::runtime_error(std::string("Missing AGS runtime export: ")+name);
        return reinterpret_cast<T>(address);
    }
};
ClockCapabilities Capabilities(ID3D12Device* device){
    ClockCapabilities result{};UINT size=sizeof(result);
    if(!device||FAILED(device->GetPrivateData(ClockCapabilitiesGuid,&size,&result))||size!=sizeof(result))return {};
    return result;
}
}
ComPtr<ID3D12Device> CreateAmdHardwareDevice(IDXGIAdapter* adapter,std::shared_ptr<void>& lifetime){
    if(lifetime)throw std::invalid_argument("Native AMD driver lifetime already initialized");
    auto state=std::make_shared<DriverLease>();
    const auto path=std::filesystem::path(ModulePath(nullptr)).parent_path()/L"native-compiler"/L"amd_ags_x64.dll";
    state->module=Common::RuntimeModuleTrace::Load(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!state->module)throw std::runtime_error("Cannot load packaged AGS runtime, Win32 "+std::to_string(GetLastError()));
    const auto initialize=state->Symbol<AGS_INITIALIZE>("agsInitialize");
    state->deinitialize=state->Symbol<AGS_DEINITIALIZE>("agsDeInitialize");
    const auto create=state->Symbol<AGS_DRIVEREXTENSIONSDX12_CREATEDEVICE>("agsDriverExtensionsDX12_CreateDevice");
    state->destroy=state->Symbol<AGS_DRIVEREXTENSIONSDX12_DESTROYDEVICE>("agsDriverExtensionsDX12_DestroyDevice");
    AGSGPUInfo info{};CheckAgs(initialize(AGS_CURRENT_VERSION,nullptr,&state->context,&info));
    AGSDX12DeviceCreationParams creation{adapter,__uuidof(ID3D12Device),D3D_FEATURE_LEVEL_12_0};
    AGSDX12ExtensionParams extensions{L"shadPS4 DX12",L"shadPS4",AGS_UNSPECIFIED_VERSION,AGS_UNSPECIFIED_VERSION,0};
    AGSDX12ReturnedParams returned{};CheckAgs(create(state->context,&creation,&extensions,&returned));
    if(!returned.pDevice)throw std::runtime_error("AGS returned no native device");
    state->device=returned.pDevice;
    const ClockCapabilities capabilities{state->module,returned.extensionsSupported.shaderClock};
    Check(state->device->SetPrivateData(ClockCapabilitiesGuid,sizeof(capabilities),&capabilities));
    ComPtr<ID3D12Device> result=state->device;lifetime=state;return result;
}
bool AmdShaderClockAvailable(ID3D12Device* device){return Capabilities(device).supported!=0;}
std::array<std::byte,32> AmdShaderClockRuntimeDigest(ID3D12Device* device){
    const auto capabilities=Capabilities(device);
    if(!capabilities.supported||!capabilities.module)throw std::runtime_error("Native AMD shader clock unavailable");
    return CacheDigest::File(ModulePath(capabilities.module));
}
}
