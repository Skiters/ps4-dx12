// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <windows.h>

namespace Common::GpuIdentity {
struct Device {
    const char* backend{};
    uint32_t vendor{},device{},node_mask{},nodes{};
    std::array<uint8_t,8> luid{};
    bool luid_valid{};
    std::array<uint8_t,16> device_uuid{},driver_uuid{};
    bool uuid_valid{};
    std::string_view name;
    uint32_t driver_version{},api_version{};
};
inline void String(FILE* file,std::string_view value){
    fputc('"',file);
    for(const unsigned char ch:value){
        if(ch=='"'||ch=='\\'){fputc('\\',file);fputc(ch,file);}
        else if(ch<32)fprintf(file,"\\u%04x",unsigned(ch));
        else fputc(ch,file);
    }
    fputc('"',file);
}
template<size_t N>inline void Hex(FILE* file,const std::array<uint8_t,N>& bytes){
    fputc('"',file);for(const auto value:bytes)fprintf(file,"%02x",unsigned(value));fputc('"',file);
}
inline bool Write(const Device& device){
    wchar_t path[32768]{};
    const auto length=GetEnvironmentVariableW(L"SHADPS4_GPU_IDENTITY_LOG",path,32768);
    if(!length)return true;
    if(length>=32768)return false;
    // Appending retains every actual device creation; a comparison cannot
    // silently select the last of multiple adapters or overwrite its identity.
    FILE* file{};if(_wfopen_s(&file,path,L"ab")!=0||!file)return false;
    LARGE_INTEGER qpc{},frequency{};QueryPerformanceCounter(&qpc);QueryPerformanceFrequency(&frequency);
    fprintf(file,"{\"backend\":");String(file,device.backend);
    fprintf(file,",\"pid\":%lu,\"qpc\":%lld,\"frequency\":%lld,\"vendorId\":%u,\"deviceId\":%u,\"name\":",
        GetCurrentProcessId(),qpc.QuadPart,frequency.QuadPart,device.vendor,device.device);String(file,device.name);
    fprintf(file,",\"luid\":");Hex(file,device.luid);
    fprintf(file,",\"luidValid\":%s,\"nodeMask\":%u,\"nodes\":%u,\"uuidValid\":%s,\"deviceUuid\":",
        device.luid_valid?"true":"false",device.node_mask,device.nodes,device.uuid_valid?"true":"false");
    Hex(file,device.device_uuid);fprintf(file,",\"driverUuid\":");Hex(file,device.driver_uuid);
    fprintf(file,",\"driverVersion\":%u,\"apiVersion\":%u}\n",device.driver_version,device.api_version);
    const bool written=fflush(file)==0&&!ferror(file);
    return fclose(file)==0&&written;
}
}
