// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
namespace NativeDX12 {
// Native counterpart of Vulkan's read watchers. Guest access is withheld until
// the resolver commits through an unprotected physical backing alias. Restoring
// permissions happens only AFTER the GPU result has been committed.
class PageWatch {
    struct Protection{uint64_t begin,size;DWORD original,watched;};
    struct Range{uint64_t end;std::vector<Protection> protection;bool read_only;};
    std::map<uint64_t,Range> ranges;
    std::map<uint64_t,uint64_t> retired;
    std::mutex mutex;
    std::function<void(uint64_t)> resolve;
    void* handler{};
    inline static std::atomic<PageWatch*> active{};
    static LONG CALLBACK Fault(EXCEPTION_POINTERS* context){
        if(context->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||context->ExceptionRecord->NumberParameters<2||
           context->ExceptionRecord->ExceptionInformation[0]==8)return EXCEPTION_CONTINUE_SEARCH;
        auto* self=active.load(std::memory_order_acquire);if(!self)return EXCEPTION_CONTINUE_SEARCH;
        const auto address=uint64_t(context->ExceptionRecord->ExceptionInformation[1]);
        const bool owned=self->Contains(address);
        // A second CPU fault can enter this handler after the first resolver
        // already restored permissions. Retry only previously watched pages.
        if(!owned&&!self->WasTracked(address))return EXCEPTION_CONTINUE_SEARCH;
        if(owned)try{self->resolve(address);}catch(...){return EXCEPTION_CONTINUE_SEARCH;}
        MEMORY_BASIC_INFORMATION region{};
        if(!VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))||region.State!=MEM_COMMIT||
           (region.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return EXCEPTION_CONTINUE_SEARCH;
        const bool write=context->ExceptionRecord->ExceptionInformation[0]==1;
        const auto protection=region.Protect&0xff;
        const bool writable=protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
        const bool readable=writable||protection==PAGE_READONLY||protection==PAGE_EXECUTE_READ;
        return (write?writable:readable)?EXCEPTION_CONTINUE_EXECUTION:EXCEPTION_CONTINUE_SEARCH;
    }
    static void Restore(const Range& range){
        for(const auto& p:range.protection){MEMORY_BASIC_INFORMATION region{};DWORD previous{};
            // Never overwrite permissions of pages remapped by Core meanwhile.
            if(VirtualQuery(reinterpret_cast<void*>(p.begin),&region,sizeof(region))&&region.State==MEM_COMMIT&&region.Protect==p.watched)
                VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,p.original,&previous);}
    }
public:
    explicit PageWatch(std::function<void(uint64_t)> callback):resolve(std::move(callback)){
        PageWatch* expected{};if(!active.compare_exchange_strong(expected,this))throw std::runtime_error("Multiple native page watcher owners");
        handler=AddVectoredExceptionHandler(1,Fault);
        if(!handler){active.store(nullptr);throw std::runtime_error("Cannot register native page watcher");}
    }
    ~PageWatch(){RemoveVectoredExceptionHandler(handler);active.store(nullptr);std::lock_guard lock(mutex);for(const auto& [_,range]:ranges)Restore(range);}
    bool Contains(uint64_t address){std::lock_guard lock(mutex);const auto it=ranges.upper_bound(address);return it!=ranges.begin()&&std::prev(it)->second.end>address;}
    bool WasTracked(uint64_t address){std::lock_guard lock(mutex);const auto it=retired.upper_bound(address);return it!=retired.begin()&&std::prev(it)->second>address;}
    bool Track(uint64_t address,uint64_t size,bool read_only=false){
        if(!address||!size||address%4096||size%4096||address>UINT64_MAX-size)return false;
        std::lock_guard lock(mutex);const auto end=address+size;
        auto next=ranges.lower_bound(address);
        if(next!=ranges.end()&&next->first==address&&next->second.end==end)return next->second.read_only==read_only;
        if((next!=ranges.end()&&next->first<end)||(next!=ranges.begin()&&std::prev(next)->second.end>address))return false;
        Range range{end,{},read_only};
        for(auto current=address;current<end;){MEMORY_BASIC_INFORMATION region{};
            if(!VirtualQuery(reinterpret_cast<void*>(current),&region,sizeof(region))||region.State!=MEM_COMMIT||region.Protect!=PAGE_READWRITE)return false;
            const auto stop=std::min(end,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
            if(stop<=current)return false;range.protection.push_back({current,stop-current,region.Protect,DWORD(read_only?PAGE_READONLY:PAGE_NOACCESS)});current=stop;
        }
        auto [entry,_]=ranges.emplace(address,std::move(range));
        for(const auto& p:entry->second.protection){DWORD previous{};
            if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,p.watched,&previous)){
                Restore(entry->second);ranges.erase(entry);throw std::runtime_error("Cannot protect native GPU-owned guest memory");}}
        return true;
    }
    void Untrack(uint64_t address,uint64_t size){
        if(!size||address>UINT64_MAX-size)return;std::lock_guard lock(mutex);
        auto it=ranges.lower_bound(address);if(it!=ranges.begin()&&std::prev(it)->second.end>address)--it;
        while(it!=ranges.end()&&it->first<address+size){
            if(retired.size()>=4096)retired.clear();retired[it->first]=it->second.end;
            Restore(it->second);it=ranges.erase(it);}
    }
};
}
