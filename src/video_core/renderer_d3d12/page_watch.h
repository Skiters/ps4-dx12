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
#include <cstdio>
#include <cstdlib>
namespace NativeDX12 {
// Native counterpart of Vulkan's read watchers. Guest access is withheld until
// the resolver commits through an unprotected physical backing alias. Restoring
// permissions happens only AFTER the GPU result has been committed.
class PageWatch {
    struct Protection{uint64_t begin,size;DWORD original,watched;};
    struct Range{uint64_t end;std::vector<Protection> protection;bool read_only;uint64_t generation;};
    std::map<uint64_t,Range> ranges;
    std::map<uint64_t,uint64_t> retired;
    std::mutex mutex;
    uint64_t next_generation{};
    std::function<void(uint64_t)> resolve;
    void* handler{};
    inline static std::atomic<PageWatch*> active{};
    static LONG CALLBACK Fault(EXCEPTION_POINTERS* context){
        if(context->ExceptionRecord->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION||context->ExceptionRecord->NumberParameters<2||
           context->ExceptionRecord->ExceptionInformation[0]==8)return EXCEPTION_CONTINUE_SEARCH;
        auto* self=active.load(std::memory_order_acquire);if(!self)return EXCEPTION_CONTINUE_SEARCH;
        const auto address=uint64_t(context->ExceptionRecord->ExceptionInformation[1]);
        const bool write=context->ExceptionRecord->ExceptionInformation[0]==1;
        uint64_t resolved_generation{};
        // A renderer can acquire its execution lock and rearm the page as soon
        // as resolve() releases that lock. Host protection is therefore not a
        // stable result of the previous resolution. Resolve the NEW owner too;
        // never mistake a new watch for a failed write to an invalid address.
        // Keep lookup + VirtualQuery atomic with respect to Track/Untrack.
        for(unsigned attempt=0;attempt<64;++attempt){
            uint64_t generation{};
            {
                std::lock_guard lock(self->mutex);
                const auto next=self->ranges.upper_bound(address);
                const auto current=next==self->ranges.begin()?self->ranges.end():std::prev(next);
                const bool owned=current!=self->ranges.end()&&current->second.end>address;
                MEMORY_BASIC_INFORMATION region{};
                const bool committed=VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))&&region.State==MEM_COMMIT;
                const auto protection=region.Protect&0xff;
                const bool writable=protection==PAGE_READWRITE||protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
                const bool readable=writable||protection==PAGE_READONLY||protection==PAGE_EXECUTE_READ;
                const bool accessible=committed&&!(region.Protect&PAGE_GUARD)&&(write?writable:readable);
                if(!owned){
                    const auto retired=self->retired.upper_bound(address);
                    const bool tracked=retired!=self->retired.begin()&&std::prev(retired)->second>address;
                    return (resolved_generation||tracked)&&accessible?EXCEPTION_CONTINUE_EXECUTION:EXCEPTION_CONTINUE_SEARCH;
                }
                generation=current->second.generation;
                // Reads need not invalidate a newly installed CPU write watch.
                if(!write&&current->second.read_only&&accessible)return EXCEPTION_CONTINUE_EXECUTION;
                // A callback that leaves the same ownership unresolved is a
                // real failure, not a reason to retry forever or unprotect it.
                if(generation==resolved_generation)return EXCEPTION_CONTINUE_SEARCH;
                if(resolved_generation&&std::getenv("SHADPS4_PAGE_FAULT_TRACE")){
                    static std::atomic<unsigned> count{};
                    if(count.fetch_add(1)<64){
                        fprintf(stderr,"PAGEWATCH REARM tid=%lu rip=%llx address=%llx generation=%llu->%llu protect=%lx\n",
                            GetCurrentThreadId(),context->ContextRecord->Rip,address,resolved_generation,generation,region.Protect);fflush(stderr);
                    }
                }
            }
            try{self->resolve(address);}catch(...){return EXCEPTION_CONTINUE_SEARCH;}
            resolved_generation=generation;
        }
        // Bound pathological callbacks without permitting a protected write.
        return EXCEPTION_CONTINUE_SEARCH;
    }

    static void Restore(const Range& range){
        for(const auto& p:range.protection){MEMORY_BASIC_INFORMATION region{};DWORD previous{};
            // Never overwrite permissions of pages remapped by Core meanwhile.
            if(VirtualQuery(reinterpret_cast<void*>(p.begin),&region,sizeof(region))&&region.State==MEM_COMMIT&&region.Protect==p.watched)
                VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,p.original,&previous);}
    }
    void RememberRetired(uint64_t begin,uint64_t end){
        // Faults can already be pending on other CPU threads when protection
        // is restored. Keep the union of retired pages: a shorter watch must
        // not truncate an earlier range or hide it behind a later map key.
        // There is no grace period proving queued Windows faults have drained,
        // so clearing this history at an arbitrary entry count is not safe.
        auto next=retired.upper_bound(begin);
        auto current=next!=retired.begin()&&std::prev(next)->second>=begin?
            std::prev(next):retired.emplace(begin,end).first;
        current->second=std::max(current->second,end);
        next=std::next(current);
        while(next!=retired.end()&&next->first<=current->second){
            current->second=std::max(current->second,next->second);
            next=retired.erase(next);
        }
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
    // Transfer an existing exact watch without ever allowing a CPU store.
    // The renderer holds its execution lock; pending faults resolve the new
    // generation after acquiring that lock. Retired history is untouched.
    bool ChangeMode(uint64_t address,uint64_t size,bool read_only){
        if(!size||address>UINT64_MAX-size)return false;
        std::lock_guard lock(mutex);
        auto it=ranges.find(address);
        if(it==ranges.end()||it->second.end!=address+size)return false;
        auto& range=it->second;
        if(range.read_only==read_only)return true;
        for(const auto& p:range.protection){
            for(auto current=p.begin;current<p.begin+p.size;){
                MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(current),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=p.watched)return false;
                current=std::min(p.begin+p.size,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
            }
        }
        const DWORD desired=read_only?PAGE_READONLY:PAGE_NOACCESS;
        for(size_t i=0;i<range.protection.size();++i){auto& p=range.protection[i];DWORD previous{};
            if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,desired,&previous)){
                // Roll back to the old protected mode, never PAGE_READWRITE.
                for(size_t j=0;j<i;++j){auto& old=range.protection[j];DWORD ignored{};
                    if(!VirtualProtect(reinterpret_cast<void*>(old.begin),old.size,old.watched,&ignored))
                        throw std::runtime_error("Cannot restore native ownership protection");}
                return false;
            }
        }
        for(auto& p:range.protection)p.watched=desired;
        range.read_only=read_only;range.generation=++next_generation;return true;
    }
    bool Track(uint64_t address,uint64_t size,bool read_only=false){
        if(!address||!size||address%4096||size%4096||address>UINT64_MAX-size)return false;
        std::lock_guard lock(mutex);const auto end=address+size;
        auto next=ranges.lower_bound(address);
        if(next!=ranges.end()&&next->first==address&&next->second.end==end)return next->second.read_only==read_only;
        if((next!=ranges.end()&&next->first<end)||(next!=ranges.begin()&&std::prev(next)->second.end>address))return false;
        Range range{end,{},read_only,++next_generation};
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
            RememberRetired(it->first,it->second.end);
            Restore(it->second);it=ranges.erase(it);}
    }
};
}
