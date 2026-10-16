// SPDX-License-Identifier: GPL-2.0-or-later
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define VK_NO_PROTOTYPES
#include <windows.h>
#include <vulkan/vk_layer.h>
#include "common/gpu_identity.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <compare>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

extern "C" {
__declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance,const char*);
__declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice,const char*);
}
namespace {
template<class T>void* Key(T handle){return *reinterpret_cast<void**>(handle);}
struct Instance{
    VkInstance handle{};
    PFN_vkGetInstanceProcAddr next{};
    PFN_GetPhysicalDeviceProcAddr physical{};
};
struct Ticket{
    VkSemaphore semaphore{};uint64_t identity{},value{};
    auto operator<=>(const Ticket&)const=default;
};
std::atomic<bool> capture_claimed{};
struct Device{
    VkDevice handle{};PFN_vkGetDeviceProcAddr next{};
    PFN_vkGetSemaphoreCounterValue counter{};
    VkPhysicalDeviceProperties2 properties{};
    VkPhysicalDeviceIDProperties identifiers{};
    std::mutex mutex;
    std::map<VkSemaphore,uint64_t> timelines;
    std::map<VkSemaphore,std::vector<Ticket>> binary_signals;
    std::set<std::vector<Ticket>> seen;
    std::vector<std::vector<Ticket>> pending;
    uint64_t semaphore_sequence{},completed{},observed{};
    bool valid{true};FILE* log{};
    uintptr_t flip_start{},flip_end{};
    uintptr_t flip_present_return{};uint32_t request_index_offset{};
    std::wstring marker;
    bool armed{};
    std::chrono::steady_clock::time_point next_marker_check{};
    std::jthread poller;
    template<class T>T Function(const char* name){return reinterpret_cast<T>(next(handle,name));}
    void Print(const char* kind,VkResult status=VK_SUCCESS){
        if(!log)return;LARGE_INTEGER time{},frequency{};QueryPerformanceCounter(&time);QueryPerformanceFrequency(&frequency);
        fprintf(log,"{\"type\":\"%s\",\"qpc\":%lld,\"frequency\":%lld,\"observed\":%llu,\"completed\":%llu,\"pending\":%zu,\"valid\":%s,\"status\":%d}\n",
            kind,time.QuadPart,frequency.QuadPart,observed,completed,pending.size(),valid?"true":"false",int(status));
        if(fflush(log)!=0||ferror(log)){valid=false;fprintf(stderr,"Reference GPU completion capture write failed\n");}
    }
    int GuestFlip()const{
        if(!log||!flip_present_return)return -2;
        __try {
            CONTEXT context{};RtlCaptureContext(&context);ULONG_PTR low{},high{};GetCurrentThreadStackLimits(&low,&high);
            for(unsigned frame=0;frame<128;++frame){
                if(context.Rip==flip_present_return){
                    if(context.Rbx>UINTPTR_MAX-request_index_offset)return -3;
                    int index{};SIZE_T read{};
                    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(context.Rbx+request_index_offset),&index,sizeof(index),&read)||read!=sizeof(index))return -3;
                    return index>=-1&&index<16?index:-3;
                }
                if(!context.Rip||context.Rsp<low||context.Rsp>high-sizeof(uint64_t))return -2;
                DWORD64 image{};const auto entry=RtlLookupFunctionEntry(context.Rip,&image,nullptr);
                if(entry){PVOID data{};DWORD64 establisher{};
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER,image,context.Rip,entry,&context,&data,&establisher,nullptr);
                }else {SIZE_T read{};uint64_t next{};
                    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(context.Rsp),&next,sizeof(next),&read)||read!=sizeof(next))return -2;
                    context.Rip=next;context.Rsp+=sizeof(uint64_t);
                }
            }
            return -2;
        }__except(EXCEPTION_EXECUTE_HANDLER){return -3;}
    }
    void Start(){
        wchar_t destination[32768]{};const auto length=GetEnvironmentVariableW(L"SHADPS4_REFERENCE_COMPLETION_LOG",destination,32768);
        if(!length||length>=32768)return;
        char start[32]{},size[32]{},callsite[32]{},index[32]{},reg[16]{};
        const auto start_length=GetEnvironmentVariableA("SHADPS4_REFERENCE_FLIP_RVA",start,32);
        const auto size_length=GetEnvironmentVariableA("SHADPS4_REFERENCE_FLIP_SIZE",size,32);
        const auto call_length=GetEnvironmentVariableA("SHADPS4_REFERENCE_FLIP_RETURN_RVA",callsite,32);
        const auto index_length=GetEnvironmentVariableA("SHADPS4_REFERENCE_REQUEST_INDEX_OFFSET",index,32);
        const auto register_length=GetEnvironmentVariableA("SHADPS4_REFERENCE_REQUEST_REGISTER",reg,16);
        char* start_end{};char* size_end{};char* call_end{};char* index_end{};
        const auto offset=strtoull(start,&start_end,16),extent=strtoull(size,&size_end,16);
        const auto return_rva=strtoull(callsite,&call_end,16),index_offset=strtoull(index,&index_end,16);
        const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(start_length&&start_length<32&&size_length&&size_length<32&&*start_end==0&&*size_end==0&&
           offset&&extent&&offset<=UINTPTR_MAX-image&&extent<=UINTPTR_MAX-image-offset){
            flip_start=image+offset;flip_end=flip_start+extent;}
        if(call_length&&call_length<32&&index_length&&index_length<32&&register_length==3&&strcmp(reg,"rbx")==0&&
           *call_end==0&&*index_end==0&&return_rva>offset&&return_rva<offset+extent&&return_rva<=UINTPTR_MAX-image&&index_offset==24){
            flip_present_return=image+return_rva;request_index_offset=uint32_t(index_offset);}
        _wfopen_s(&log,destination,L"ab");
        if(!log){valid=false;return;}
        if(capture_claimed.exchange(true)){valid=false;Print("multiple-capture-devices");return;}
        Common::GpuIdentity::Device identity{};identity.backend="vulkan";
        identity.vendor=properties.properties.vendorID;identity.device=properties.properties.deviceID;
        identity.name=properties.properties.deviceName;identity.node_mask=identifiers.deviceNodeMask;
        identity.luid_valid=identifiers.deviceLUIDValid==VK_TRUE;
        memcpy(identity.luid.data(),identifiers.deviceLUID,VK_LUID_SIZE);
        memcpy(identity.device_uuid.data(),identifiers.deviceUUID,VK_UUID_SIZE);
        memcpy(identity.driver_uuid.data(),identifiers.driverUUID,VK_UUID_SIZE);
        identity.uuid_valid=properties.properties.apiVersion!=0;
        identity.driver_version=properties.properties.driverVersion;identity.api_version=properties.properties.apiVersion;
        if(!Common::GpuIdentity::Write(identity)||!identity.luid_valid){valid=false;Print("gpu-identity-record-error");return;}
        if(!flip_start||!flip_present_return||!counter){valid=false;Print("invalid-symbol-or-timeline-api");return;}
        wchar_t marker_path[32768]{};const auto marker_length=GetEnvironmentVariableW(L"SHADPS4_FRAME_CAPTURE",marker_path,32768);
        if(!marker_length||marker_length>=32768){valid=false;Print("invalid-capture-marker");return;}
        marker=marker_path;
        Print("device-start");
        poller=std::jthread([this](std::stop_token stop){
            while(!stop.stop_requested()){
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                std::lock_guard lock(mutex);Poll();UpdateMarker();
            }
        });
    }
    void UpdateMarker(){
        const auto now=std::chrono::steady_clock::now();if(now<next_marker_check)return;
        next_marker_check=now+std::chrono::milliseconds(500);
        const auto attributes=GetFileAttributesW(marker.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES){const auto error=GetLastError();
            if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND){valid=false;Print("capture-marker-error");}}
        else if(attributes&FILE_ATTRIBUTE_DIRECTORY){valid=false;Print("capture-marker-not-file");}
        const bool active=valid&&attributes!=INVALID_FILE_ATTRIBUTES;
        if(active!=armed){armed=active;Print(active?"capture-start":"capture-stop");}
    }
    void Poll(){
        if(!valid||pending.empty())return;
        std::map<VkSemaphore,uint64_t> values;
        for(const auto& frame:pending)for(const auto& ticket:frame){
            if(values.contains(ticket.semaphore))continue;
            if(!timelines.contains(ticket.semaphore)||timelines.at(ticket.semaphore)!=ticket.identity){
                valid=false;Print("timeline-destroyed-before-completion");return;}
            uint64_t value{};const auto result=counter(handle,ticket.semaphore,&value);
            if(result!=VK_SUCCESS){valid=false;Print("counter-error",result);return;}
            values.emplace(ticket.semaphore,value);
        }
        const auto previous=completed;
        std::erase_if(pending,[&](const auto& frame){
            const bool ready=std::ranges::all_of(frame,[&](const auto& ticket){return values.at(ticket.semaphore)>=ticket.value;});
            if(ready)++completed;return ready;
        });
        if(previous!=completed)Print("gpu-completed");
    }
    void Stop(){
        if(poller.joinable()){poller.request_stop();poller.join();}
        std::lock_guard lock(mutex);Poll();
        if(armed){valid=false;armed=false;Print("capture-stopped-by-shutdown");}
        Print("device-stop");if(log){fclose(log);log=nullptr;}
    }
};
std::mutex states_mutex;
std::map<void*,Instance> instances;
std::map<void*,std::shared_ptr<Device>> devices;
Instance FindInstance(VkInstance handle){std::lock_guard lock(states_mutex);return instances.at(Key(handle));}
template<class T>std::shared_ptr<Device> FindDevice(T handle){std::lock_guard lock(states_mutex);return devices.at(Key(handle));}
template<class T>const T* Chain(const void* next,VkStructureType type){
    for(auto* item=static_cast<const VkBaseInStructure*>(next);item;item=item->pNext)
        if(item->sType==type)return reinterpret_cast<const T*>(item);
    return nullptr;
}
void StoreSignals(Device& device,uint32_t waits,const VkSemaphore* wait_handles,const uint64_t* wait_values,
                  uint32_t signals,const VkSemaphore* signal_handles){
    std::vector<Ticket> tickets;
    for(uint32_t i=0;i<waits;++i){auto found=device.timelines.find(wait_handles[i]);
        if(found!=device.timelines.end()&&wait_values&&wait_values[i])tickets.push_back({found->first,found->second,wait_values[i]});}
    std::ranges::sort(tickets);
    for(uint32_t i=0;i<signals;++i)
        if(!device.timelines.contains(signal_handles[i]))device.binary_signals[signal_handles[i]]=tickets;
}
}
extern "C" {
VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* info,const VkAllocationCallbacks* allocator,VkInstance* output){
    auto* link=const_cast<VkLayerInstanceCreateInfo*>(Chain<VkLayerInstanceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO));
    while(link&&link->function!=VK_LAYER_LINK_INFO)
        link=const_cast<VkLayerInstanceCreateInfo*>(Chain<VkLayerInstanceCreateInfo>(link->pNext,VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO));
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    const auto next=link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    const auto physical=link->u.pLayerInfo->pfnNextGetPhysicalDeviceProcAddr;
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto create=reinterpret_cast<PFN_vkCreateInstance>(next(nullptr,"vkCreateInstance"));
    const auto result=create(info,allocator,output);
    if(result==VK_SUCCESS){std::lock_guard lock(states_mutex);instances.emplace(Key(*output),Instance{*output,next,physical});}
    return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance handle,const VkAllocationCallbacks* allocator){
    const auto state=FindInstance(handle);const auto key=Key(handle);
    reinterpret_cast<PFN_vkDestroyInstance>(state.next(handle,"vkDestroyInstance"))(handle,allocator);
    std::lock_guard lock(states_mutex);instances.erase(key);
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical,const VkDeviceCreateInfo* info,const VkAllocationCallbacks* allocator,VkDevice* output){
    Instance instance;{std::lock_guard lock(states_mutex);instance=instances.at(Key(physical));}
    auto* link=const_cast<VkLayerDeviceCreateInfo*>(Chain<VkLayerDeviceCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO));
    while(link&&link->function!=VK_LAYER_LINK_INFO)
        link=const_cast<VkLayerDeviceCreateInfo*>(Chain<VkLayerDeviceCreateInfo>(link->pNext,VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO));
    if(!link||!link->u.pLayerInfo)return VK_ERROR_INITIALIZATION_FAILED;
    const auto next=link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    const auto create=reinterpret_cast<PFN_vkCreateDevice>(link->u.pLayerInfo->pfnNextGetInstanceProcAddr(instance.handle,"vkCreateDevice"));
    link->u.pLayerInfo=link->u.pLayerInfo->pNext;
    const auto result=create(physical,info,allocator,output);
    if(result==VK_SUCCESS){auto device=std::make_shared<Device>();device->handle=*output;device->next=next;
        auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance.next(instance.handle,"vkGetPhysicalDeviceProperties2"));
        if(!properties)properties=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(instance.next(instance.handle,"vkGetPhysicalDeviceProperties2KHR"));
        if(properties){device->properties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            device->identifiers.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
            device->properties.pNext=&device->identifiers;properties(physical,&device->properties);}
        device->counter=device->Function<PFN_vkGetSemaphoreCounterValue>("vkGetSemaphoreCounterValue");
        if(!device->counter)device->counter=device->Function<PFN_vkGetSemaphoreCounterValue>("vkGetSemaphoreCounterValueKHR");
        {std::lock_guard lock(states_mutex);devices.emplace(Key(*output),device);}device->Start();}
    return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice handle,const VkAllocationCallbacks* allocator){
    auto state=FindDevice(handle);state->Stop();const auto key=Key(handle);
    state->Function<PFN_vkDestroyDevice>("vkDestroyDevice")(handle,allocator);
    std::lock_guard lock(states_mutex);devices.erase(key);
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice handle,const VkSemaphoreCreateInfo* info,const VkAllocationCallbacks* allocator,VkSemaphore* output){
    auto state=FindDevice(handle);const auto result=state->Function<PFN_vkCreateSemaphore>("vkCreateSemaphore")(handle,info,allocator,output);
    if(result==VK_SUCCESS){const auto* type=Chain<VkSemaphoreTypeCreateInfo>(info->pNext,VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO);
        if(type&&type->semaphoreType==VK_SEMAPHORE_TYPE_TIMELINE){std::lock_guard lock(state->mutex);state->timelines[*output]=++state->semaphore_sequence;}}
    return result;
}
VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice handle,VkSemaphore semaphore,const VkAllocationCallbacks* allocator){
    auto state=FindDevice(handle);std::lock_guard lock(state->mutex);state->Poll();
    state->Function<PFN_vkDestroySemaphore>("vkDestroySemaphore")(handle,semaphore,allocator);
    state->timelines.erase(semaphore);state->binary_signals.erase(semaphore);
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,uint32_t count,const VkSubmitInfo* infos,VkFence fence){
    auto state=FindDevice(queue);std::lock_guard lock(state->mutex);
    const auto result=state->Function<PFN_vkQueueSubmit>("vkQueueSubmit")(queue,count,infos,fence);
    if(result==VK_SUCCESS)for(uint32_t i=0;i<count;++i){const auto& info=infos[i];
        const auto* timeline=Chain<VkTimelineSemaphoreSubmitInfo>(info.pNext,VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO);
        StoreSignals(*state,info.waitSemaphoreCount,info.pWaitSemaphores,
            timeline&&timeline->waitSemaphoreValueCount==info.waitSemaphoreCount?timeline->pWaitSemaphoreValues:nullptr,
            info.signalSemaphoreCount,info.pSignalSemaphores);}
    else{state->valid=false;state->Print("submit-error",result);}
    return result;
}
static VkResult Submit2(VkQueue queue,uint32_t count,const VkSubmitInfo2* infos,VkFence fence,const char* name){
    auto state=FindDevice(queue);std::lock_guard lock(state->mutex);
    const auto function=state->Function<PFN_vkQueueSubmit2>(name);if(!function)return VK_ERROR_EXTENSION_NOT_PRESENT;
    const auto result=function(queue,count,infos,fence);
    if(result==VK_SUCCESS)for(uint32_t i=0;i<count;++i){const auto& info=infos[i];std::vector<VkSemaphore> waits,signals;std::vector<uint64_t> values;
        for(uint32_t w=0;w<info.waitSemaphoreInfoCount;++w){waits.push_back(info.pWaitSemaphoreInfos[w].semaphore);values.push_back(info.pWaitSemaphoreInfos[w].value);}
        for(uint32_t s=0;s<info.signalSemaphoreInfoCount;++s)signals.push_back(info.pSignalSemaphoreInfos[s].semaphore);
        StoreSignals(*state,uint32_t(waits.size()),waits.data(),values.data(),uint32_t(signals.size()),signals.data());}
    else{state->valid=false;state->Print("submit-error",result);}
    return result;
}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,uint32_t count,const VkSubmitInfo2* infos,VkFence fence){return Submit2(queue,count,infos,fence,"vkQueueSubmit2");}
VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2KHR(VkQueue queue,uint32_t count,const VkSubmitInfo2* infos,VkFence fence){return Submit2(queue,count,infos,fence,"vkQueueSubmit2KHR");}
VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,const VkPresentInfoKHR* info){
    auto state=FindDevice(queue);const int guest=state->GuestFlip();
    {std::lock_guard lock(state->mutex);
        if(guest==-3){state->valid=false;state->Print("invalid-guest-flip-context");}
        if(guest>=0){std::vector<Ticket> tickets;
            for(uint32_t i=0;i<info->waitSemaphoreCount;++i){auto found=state->binary_signals.find(info->pWaitSemaphores[i]);
                if(found!=state->binary_signals.end())tickets.insert(tickets.end(),found->second.begin(),found->second.end());}
            std::ranges::sort(tickets);tickets.erase(std::unique(tickets.begin(),tickets.end()),tickets.end());
            if(tickets.empty()){state->valid=false;state->Print("guest-present-without-ready-timeline");}
            else if(state->seen.insert(tickets).second){state->pending.push_back(std::move(tickets));++state->observed;}
            else{state->valid=false;state->Print("duplicate-ready-token-for-new-guest-flip");}
        }
        for(uint32_t i=0;i<info->waitSemaphoreCount;++i)state->binary_signals.erase(info->pWaitSemaphores[i]);
    }
    const auto result=state->Function<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(queue,info);
    if(result!=VK_SUCCESS&&result!=VK_SUBOPTIMAL_KHR){std::lock_guard lock(state->mutex);state->valid=false;state->Print("present-error",result);}
    return result;
}
static PFN_vkVoidFunction Intercept(const char* name){
#define ENTRY(symbol) if(!strcmp(name,#symbol))return reinterpret_cast<PFN_vkVoidFunction>(symbol)
    ENTRY(vkGetInstanceProcAddr);ENTRY(vkGetDeviceProcAddr);ENTRY(vkCreateInstance);ENTRY(vkDestroyInstance);
    ENTRY(vkCreateDevice);ENTRY(vkDestroyDevice);ENTRY(vkCreateSemaphore);ENTRY(vkDestroySemaphore);
    ENTRY(vkQueueSubmit);ENTRY(vkQueueSubmit2);ENTRY(vkQueueSubmit2KHR);ENTRY(vkQueuePresentKHR);
#undef ENTRY
    return nullptr;
}
__declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,const char* name){
    if(!strcmp(name,"vkCreateInstance")||!strcmp(name,"vkGetInstanceProcAddr")||!strcmp(name,"vkGetDeviceProcAddr"))return Intercept(name);
    if(!instance)return nullptr;const auto state=FindInstance(instance);const auto next=state.next(instance,name);
    if(next)if(auto function=Intercept(name))return function;return next;
}
__declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,const char* name){
    if(!device)return nullptr;auto state=FindDevice(device);const auto next=state->next(device,name);
    if(next)if(auto function=Intercept(name))return function;return next;
}
__declspec(dllexport) VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_layerGetPhysicalDeviceProcAddr(VkInstance instance,const char* name){
    if(!instance)return nullptr;const auto state=FindInstance(instance);return state.physical?state.physical(instance,name):state.next(instance,name);
}
VKAPI_ATTR VkResult VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface* version){
    if(!version||version->sType!=LAYER_NEGOTIATE_INTERFACE_STRUCT||version->loaderLayerInterfaceVersion<2)return VK_ERROR_INITIALIZATION_FAILED;
    version->loaderLayerInterfaceVersion=2;version->pfnGetInstanceProcAddr=vkGetInstanceProcAddr;
    version->pfnGetDeviceProcAddr=vkGetDeviceProcAddr;version->pfnGetPhysicalDeviceProcAddr=vk_layerGetPhysicalDeviceProcAddr;return VK_SUCCESS;
}
}
