// SPDX-License-Identifier: GPL-2.0-or-later
#include "nv_shader_clock.h"
#include "common/runtime_module_trace.h"
#include "persistent_cache.h"
#include <cstdio>
#include <windows.h>
#include <stdexcept>
namespace NativeDX12 {
namespace {
using Status=int;
using Initialize=Status(__cdecl*)();
using SetSlot=Status(__cdecl*)(IUnknown*,unsigned,unsigned);
using SupportsOp=Status(__cdecl*)(ID3D12Device*,unsigned,bool*);
struct Api {
    HMODULE module{};
    SetSlot set_slot{};
    SupportsOp supports_op{};
    Api(){
        module=Common::RuntimeModuleTrace::Load(L"nvapi64.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!module)return;
        auto query=reinterpret_cast<void*(__cdecl*)(unsigned)>(GetProcAddress(module,"nvapi_QueryInterface"));
        if(!query)return;
        auto initialize=reinterpret_cast<Initialize>(query(0x0150e828));
        auto slot=reinterpret_cast<SetSlot>(query(0x43d867c0));
        auto supports=reinterpret_cast<SupportsOp>(query(0x3dfacec8));
        if(!initialize||!slot||!supports||initialize()!=0)return;
        set_slot=slot;supports_op=supports;
    }
    ~Api(){if(module)FreeLibrary(module);}
};
Api& Runtime(){static Api api;return api;}
void CheckNv(Status status){
    if(status!=0)throw std::runtime_error("NVAPI shader clock configuration failed: "+std::to_string(status));
}
}
bool NvShaderClockAvailable(ID3D12Device* device){
    auto& api=Runtime();bool supported=false;
    return device&&api.supports_op&&api.supports_op(device,19,&supported)==0&&supported;
}
std::array<std::byte,32> NvShaderClockRuntimeDigest(){
    auto& api=Runtime();if(!api.set_slot)throw std::runtime_error("NVAPI shader extensions unavailable");
    std::wstring path(32768,L'\0');
    const auto size=GetModuleFileNameW(api.module,path.data(),static_cast<DWORD>(path.size()));
    if(!size||size>=path.size())throw std::runtime_error("Cannot identify NVAPI runtime");
    path.resize(size);return CacheDigest::File(path);
}
NvShaderClockScope::NvShaderClockScope(ID3D12Device* device,bool enable){
    if(!enable)return;
    if(!NvShaderClockAvailable(device))throw std::runtime_error("Native GPU does not support the hardware shader clock extension");
    CheckNv(Runtime().set_slot(device,NvClockRegister,NvClockSpace));device_=device;
}
void NvShaderClockScope::Finish(){
    if(!device_)return;
    CheckNv(Runtime().set_slot(device_,UINT_MAX,0));device_=nullptr;
}
NvShaderClockScope::~NvShaderClockScope(){
    if(device_){
        const auto status=Runtime().set_slot(device_,UINT_MAX,0);
        if(status!=0){std::fprintf(stderr,"NVAPI shader extension reset failed: %d\n",status);std::terminate();}
    }
}
}
