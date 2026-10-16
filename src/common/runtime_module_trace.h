// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "gpu_identity.h"
#include "guest_frame_trace.h"
#include "sha256.h"
#include <map>
#include <mutex>

namespace Common::RuntimeModuleTrace {
inline const std::wstring& Destination(){
    static const auto path=[] {wchar_t value[32768]{};
        const auto size=GetEnvironmentVariableW(L"SHADPS4_RUNTIME_MODULE_LOG",value,32768);
        if(size>=32768)throw std::runtime_error("Module identity log path exceeds the Windows limit");
        return std::wstring(value,size);}();return path;
}
inline void Record(HMODULE module)noexcept{
    try{
        const auto& destination=Destination();if(destination.empty()||!module)return;
        wchar_t name[32768]{};const auto length=GetModuleFileNameW(module,name,32768);
        if(!length||length>=32768)throw std::runtime_error("Cannot identify loaded module path");
        const std::filesystem::path path(name);
        const auto modified=std::filesystem::last_write_time(path);const auto size=std::filesystem::file_size(path);
        struct Fingerprint {std::filesystem::file_time_type modified;uintmax_t size;};
        static std::mutex mutex;static std::map<std::filesystem::path,Fingerprint> recorded;
        std::lock_guard lock(mutex);
        const auto found=recorded.find(path);
        if(found!=recorded.end()&&found->second.modified==modified&&found->second.size==size)return;
        if(recorded.size()>=128)throw std::runtime_error("Module identity capacity exceeded");
        const auto digest=Sha256::File(path);
        if(std::filesystem::last_write_time(path)!=modified||std::filesystem::file_size(path)!=size)
            throw std::runtime_error("Loaded module file changed during hashing");
        const auto utf8=path.u8string();std::array<uint8_t,32> hash{};
        for(size_t i=0;i<hash.size();++i)hash[i]=std::to_integer<uint8_t>(digest[i]);
        FILE* file{};if(_wfopen_s(&file,destination.c_str(),L"ab")!=0||!file)throw std::runtime_error("Cannot open loaded module identity log");
        LARGE_INTEGER time{},frequency{};QueryPerformanceCounter(&time);QueryPerformanceFrequency(&frequency);
        fprintf(file,"{\"type\":\"module-loaded\",\"source\":\"native-load\",\"pid\":%lu,\"qpc\":%lld,\"frequency\":%lld,\"baseAddress\":%llu,\"path\":",
            GetCurrentProcessId(),time.QuadPart,frequency.QuadPart,static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(module)));
        GpuIdentity::String(file,{reinterpret_cast<const char*>(utf8.data()),utf8.size()});
        fprintf(file,",\"sha256\":");GpuIdentity::Hex(file,hash);fprintf(file,"}\n");
        const bool flushed=fflush(file)==0&&!ferror(file);const bool closed=fclose(file)==0;
        if(!flushed||!closed)throw std::runtime_error("Loaded module identity write failed");
        recorded.insert_or_assign(path,Fingerprint{modified,size});
    }catch(const std::exception& error){
        fprintf(stderr,"Native loaded module identity failed: %s\n",error.what());
        try{GuestFrameTrace::Invalid("native-module-identity-error");}catch(...){}
    }
}
inline HMODULE Load(LPCWSTR path,HANDLE file,DWORD flags){
    const auto module=LoadLibraryExW(path,file,flags);
    const auto error=GetLastError();if(module)Record(module);SetLastError(error);return module;
}
}
