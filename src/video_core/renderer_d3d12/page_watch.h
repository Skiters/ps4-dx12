// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <vector>
#include <span>
#include <set>
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
    std::function<void(uint64_t,bool)> resolve;
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
            try{self->resolve(address,write);}catch(...){return EXCEPTION_CONTINUE_SEARCH;}
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
    explicit PageWatch(std::function<void(uint64_t,bool)> callback):resolve(std::move(callback)){
        PageWatch* expected{};if(!active.compare_exchange_strong(expected,this))throw std::runtime_error("Multiple native page watcher owners");
        handler=AddVectoredExceptionHandler(1,Fault);
        if(!handler){active.store(nullptr);throw std::runtime_error("Cannot register native page watcher");}
    }
    explicit PageWatch(std::function<void(uint64_t)> callback)
        :PageWatch([callback=std::move(callback)](uint64_t address,bool){callback(address);}){}
    ~PageWatch(){RemoveVectoredExceptionHandler(handler);active.store(nullptr);std::lock_guard lock(mutex);for(const auto& [_,range]:ranges)Restore(range);}
    bool Contains(uint64_t address){std::lock_guard lock(mutex);const auto it=ranges.upper_bound(address);return it!=ranges.begin()&&std::prev(it)->second.end>address;}
    bool WasTracked(uint64_t address){std::lock_guard lock(mutex);const auto it=retired.upper_bound(address);return it!=retired.begin()&&std::prev(it)->second>address;}
    // Preserve protected neighbours when a CPU store invalidates one page.
    // GPU ownership and image retirement still use the whole-range Untrack.
    bool UntrackReadOnly(uint64_t address,uint64_t size,bool preserve_gpu=false){
        if(!size||address%4096||size%4096||address>UINT64_MAX-size)return false;
        std::lock_guard lock(mutex);const auto end=address+size;
        auto first=ranges.lower_bound(address);
        if(first!=ranges.begin()&&std::prev(first)->second.end>address)--first;
        for(auto it=first;it!=ranges.end()&&it->first<end;++it){
            if(!it->second.read_only){if(preserve_gpu)continue;return false;}
            for(const auto& p:it->second.protection){
                const auto begin=std::max(p.begin,address),stop=std::min(p.begin+p.size,end);
                if(begin>=stop)continue;
                for(auto current=begin;current<stop;){MEMORY_BASIC_INFORMATION region{};
                    if(!VirtualQuery(reinterpret_cast<void*>(current),&region,sizeof(region))||
                       region.State!=MEM_COMMIT||region.Protect!=p.watched)return false;
                    current=std::min(stop,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
                }
            }
        }
        const auto slice=[](const Range& range,uint64_t begin,uint64_t stop,uint64_t generation){
            Range result{stop,{},true,generation};
            for(const auto& p:range.protection){const auto a=std::max(begin,p.begin),b=std::min(stop,p.begin+p.size);
                if(a<b)result.protection.push_back({a,b-a,p.original,p.watched});}
            return result;
        };
        for(auto it=first;it!=ranges.end()&&it->first<end;){
            if(!it->second.read_only){++it;continue;}
            const auto begin=it->first,stop=it->second.end;
            const auto a=std::max(begin,address),b=std::min(stop,end);
            auto removed=slice(it->second,a,b,++next_generation);
            for(const auto& p:removed.protection){DWORD previous{};
                if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,p.original,&previous))
                    throw std::runtime_error("Cannot release dirty CPU page protection");}
            RememberRetired(a,b);
            auto left=begin<a?slice(it->second,begin,a,++next_generation):Range{};
            auto right=b<stop?slice(it->second,b,stop,++next_generation):Range{};
            it=ranges.erase(it);
            if(begin<a)ranges.emplace(begin,std::move(left));
            if(b<stop)ranges.emplace(b,std::move(right));
        }
        return true;
    }
    // Rearm only the writable holes. Existing read-only pages never lose their
    // proof. Readable witnesses reject NOACCESS; independent image write guards
    // can share a stronger buffer owner without weakening its protection.
    bool TrackReadOnly(uint64_t address,uint64_t size,bool allow_gpu=false){
        if(!address||!size||address%4096||size%4096||address>UINT64_MAX-size)return false;
        std::vector<std::pair<uint64_t,uint64_t>> gaps;
        {std::lock_guard lock(mutex);const auto end=address+size;auto cursor=address;
            auto it=ranges.upper_bound(address);
            if(it!=ranges.begin()&&std::prev(it)->second.end>address)--it;
            for(;it!=ranges.end()&&it->first<end;++it){
                // Independent image versions need a CPU-write guard, not a
                // promise of readable physical backing. A buffer's stronger
                // NOACCESS guard remains owned by that buffer; its read fault
                // resolver will rearm this image's READONLY guard on release.
                if(!it->second.read_only&&!allow_gpu)return false;
                // These are the same retained pages accepted by an unchanged
                // watch hit. Core remap/protection notifications retire that
                // proof. Only gaps acquire new proof and need Track's complete
                // OS validation; rescanning clean 16KiB mappings here scales
                // with the whole PS4 allocation on every small CPU store.
                if(cursor<it->first)gaps.emplace_back(cursor,it->first-cursor);
                cursor=std::max(cursor,it->second.end);
            }
            if(cursor<end)gaps.emplace_back(cursor,end-cursor);
        }
        size_t added{};
        try{for(const auto& [begin,bytes]:gaps){if(!Track(begin,bytes,true)){
                for(size_t i=0;i<added;++i)Untrack(gaps[i].first,gaps[i].second);return false;}++added;}}
        catch(...){for(size_t i=0;i<added;++i)Untrack(gaps[i].first,gaps[i].second);throw;}
        return true;
    }
    // Acquire all physical image planes as one transaction. Existing stronger
    // B owners stay NOACCESS. Only an explicitly transferred exclusive I may
    // demote its own guards, and then every requested page must already exist.
    // All planning, allocation and OS validation precede protection changes.
    bool TrackImageReadOnly(std::span<const std::pair<uint64_t,uint64_t>> planes,
        bool demote_exclusive=false,
        std::span<const std::pair<uint64_t,uint64_t>> preserve_strong={}){
        if(planes.empty())return false;
        const auto valid=[](const auto& range){return range.first&&range.second&&
            range.first%4096==0&&range.second%4096==0&&range.first<=UINT64_MAX-range.second;};
        std::vector<std::pair<uint64_t,uint64_t>> requested(planes.begin(),planes.end());
        for(const auto& range:requested)if(!valid(range))return false;
        for(const auto& range:preserve_strong)if(!valid(range))return false;
        std::sort(requested.begin(),requested.end());
        size_t count{};
        for(const auto& [begin,bytes]:requested){
            if(count&&requested[count-1].first+requested[count-1].second>=begin){
                auto& prior=requested[count-1];prior.second=std::max(prior.first+prior.second,begin+bytes)-prior.first;
            }else requested[count++]={begin,bytes};
        }
        requested.resize(count);
        const auto contains=[](const auto& spans,uint64_t address){return std::any_of(spans.begin(),spans.end(),
            [address](const auto& range){return address>=range.first&&address-range.first<range.second;});};
        std::lock_guard lock(mutex);
        std::set<uint64_t> old_keys;
        for(const auto& [begin,bytes]:requested){
            auto it=ranges.upper_bound(begin);
            if(it!=ranges.begin()&&std::prev(it)->second.end>begin)--it;
            for(;it!=ranges.end()&&it->first<begin+bytes;++it)old_keys.insert(it->first);
        }
        std::map<uint64_t,Range> replacement;
        struct Change {Protection* protection;DWORD desired;};
        std::vector<Change> changes;
        auto generation=next_generation;
        for(const auto key:old_keys){
            const auto& original=ranges.at(key);
            std::vector<uint64_t> cuts{key,original.end};
            const auto add_cuts=[&](const auto& spans){for(const auto& [begin,bytes]:spans){
                if(begin>key&&begin<original.end)cuts.push_back(begin);
                if(begin+bytes>key&&begin+bytes<original.end)cuts.push_back(begin+bytes);
            }};
            add_cuts(requested);add_cuts(preserve_strong);
            std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
            for(size_t i=1;i<cuts.size();++i){
                const auto begin=cuts[i-1],end=cuts[i];
                const bool selected=contains(requested,begin),strong=contains(preserve_strong,begin);
                if(selected&&strong&&original.read_only)return false;
                const bool read_only=original.read_only||(selected&&demote_exclusive&&!strong);
                Range part{end,{},read_only,selected?++generation:original.generation};
                for(const auto& p:original.protection){
                    const auto a=std::max(begin,p.begin),b=std::min(end,p.begin+p.size);
                    if(a<b)part.protection.push_back({a,b-a,p.original,p.watched});
                }
                auto [entry,inserted]=replacement.emplace(begin,std::move(part));
                if(!inserted)throw std::logic_error("Overlapping image protection plan");
                if(selected)for(auto& p:entry->second.protection)
                    changes.push_back({&p,DWORD(read_only?PAGE_READONLY:PAGE_NOACCESS)});
            }
        }
        const auto add_gap=[&](uint64_t begin,uint64_t end){
            if(demote_exclusive||std::any_of(preserve_strong.begin(),preserve_strong.end(),
                [begin,end](const auto& range){return begin<range.first+range.second&&range.first<end;}))return false;
            Range part{end,{},true,++generation};
            for(auto cursor=begin;cursor<end;){
                MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=PAGE_READWRITE)return false;
                const auto next=std::min(end,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
                if(next<=cursor)return false;
                part.protection.push_back({cursor,next-cursor,PAGE_READWRITE,PAGE_READWRITE});cursor=next;
            }
            auto [entry,inserted]=replacement.emplace(begin,std::move(part));
            if(!inserted)throw std::logic_error("Overlapping image protection gap");
            for(auto& p:entry->second.protection)changes.push_back({&p,PAGE_READONLY});
            return true;
        };
        for(const auto& [begin,bytes]:requested){
            auto cursor=begin;const auto end=begin+bytes;
            auto it=ranges.upper_bound(begin);
            if(it!=ranges.begin()&&std::prev(it)->second.end>begin)--it;
            for(;it!=ranges.end()&&it->first<end;++it){
                if(cursor<it->first&&!add_gap(cursor,it->first))return false;
                cursor=std::max(cursor,std::min(end,it->second.end));
            }
            if(cursor<end&&!add_gap(cursor,end))return false;
        }
        for(const auto& change:changes){const auto& p=*change.protection;
            for(auto cursor=p.begin;cursor<p.begin+p.size;){
                MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=p.watched)return false;
                const auto next=std::min(p.begin+p.size,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
                if(next<=cursor)return false;cursor=next;
            }
        }
        std::vector<const Change*> applied;applied.reserve(changes.size());
        for(const auto& change:changes){const auto& p=*change.protection;DWORD previous{};
            if(p.watched==change.desired)continue;
            if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,change.desired,&previous)){
                for(auto it=applied.rbegin();it!=applied.rend();++it){const auto& old=*(*it)->protection;DWORD ignored{};
                    if(!VirtualProtect(reinterpret_cast<void*>(old.begin),old.size,old.watched,&ignored))
                        throw std::runtime_error("Cannot roll back image plane protection transaction");
                }
                return false;
            }
            applied.push_back(&change);
        }
        for(const auto& change:changes)change.protection->watched=change.desired;
        for(const auto key:old_keys)ranges.erase(key);
        ranges.merge(replacement);next_generation=generation;
        return true;
    }
    // Transfer an existing exact watch without ever allowing a CPU store.
    // The renderer holds its execution lock; pending faults resolve the new
    // generation after acquiring that lock. Retired history is untouched.
    bool ChangeMode(uint64_t address,uint64_t size,bool read_only){
        if(!size||address>UINT64_MAX-size)return false;
        std::lock_guard lock(mutex);
        auto first=ranges.find(address);if(first==ranges.end())return false;
        auto last=first;auto cursor=address;
        while(last!=ranges.end()&&last->first==cursor&&last->second.end<=address+size){
            cursor=last->second.end;++last;if(cursor==address+size)break;}
        if(cursor!=address+size)return false;
        std::vector<Protection*> protections;
        for(auto it=first;it!=last;++it)for(auto& p:it->second.protection){
            for(auto current=p.begin;current<p.begin+p.size;){
                MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(current),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=p.watched)return false;
                current=std::min(p.begin+p.size,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
            }
            protections.push_back(&p);
        }
        const DWORD desired=read_only?PAGE_READONLY:PAGE_NOACCESS;
        for(size_t i=0;i<protections.size();++i){auto& p=*protections[i];DWORD previous{};
            if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,desired,&previous)){
                // Roll back to the old protected mode, never PAGE_READWRITE.
                for(size_t j=0;j<i;++j){auto& old=*protections[j];DWORD ignored{};
                    if(!VirtualProtect(reinterpret_cast<void*>(old.begin),old.size,old.watched,&ignored))
                        throw std::runtime_error("Cannot restore native ownership protection");}
                return false;
            }
        }
        for(auto* p:protections)p->watched=desired;
        for(auto it=first;it!=last;++it){it->second.read_only=read_only;it->second.generation=++next_generation;}
        return true;
    }
    // Acquire exact buffer pages over existing image read-only witnesses and
    // writable gaps. Preserve protection outside this owner's page envelope.
    bool TrackOwned(uint64_t address,uint64_t size){
        if(!address||!size||address%4096||size%4096||address>UINT64_MAX-size)return false;
        if(Track(address,size)||ChangeMode(address,size,false))return true;
        std::lock_guard lock(mutex);const auto end=address+size;
        auto first=ranges.lower_bound(address);
        if(first!=ranges.begin()&&std::prev(first)->second.end>address)--first;
        auto last=first;while(last!=ranges.end()&&last->first<end)++last;
        std::map<uint64_t,Range> replacement;
        const auto slice=[](const Range& source,uint64_t begin,uint64_t stop){
            Range result{stop,{},source.read_only,source.generation};
            for(const auto& p:source.protection){const auto a=std::max(begin,p.begin),b=std::min(stop,p.begin+p.size);
                if(a<b)result.protection.push_back({a,b-a,p.original,p.watched});}
            return result;
        };
        const auto add_gap=[&](uint64_t begin,uint64_t stop){
            Range result{stop,{},false,++next_generation};
            for(auto cursor=begin;cursor<stop;){MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(cursor),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=PAGE_READWRITE)return false;
                const auto next=std::min(stop,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
                if(next<=cursor)return false;
                result.protection.push_back({cursor,next-cursor,PAGE_READWRITE,PAGE_READWRITE});cursor=next;}
            replacement.emplace(begin,std::move(result));return true;
        };
        auto cursor=address;
        for(auto it=first;it!=last;++it){
            if(cursor<it->first&&!add_gap(cursor,std::min(end,it->first)))return false;
            const auto begin=std::max(address,it->first),stop=std::min(end,it->second.end);
            if(it->first<address)replacement.emplace(it->first,slice(it->second,it->first,address));
            auto owned=slice(it->second,begin,stop);owned.read_only=false;owned.generation=++next_generation;
            replacement.emplace(begin,std::move(owned));
            if(it->second.end>end)replacement.emplace(end,slice(it->second,end,it->second.end));
            cursor=stop;
        }
        if(cursor<end&&!add_gap(cursor,end))return false;
        std::vector<Protection*> protection;
        for(auto& [begin,range]:replacement)if(begin>=address&&begin<end)for(auto& p:range.protection){
            for(auto current=p.begin;current<p.begin+p.size;){MEMORY_BASIC_INFORMATION region{};
                if(!VirtualQuery(reinterpret_cast<void*>(current),&region,sizeof(region))||
                   region.State!=MEM_COMMIT||region.Protect!=p.watched)return false;
                const auto stop=std::min(p.begin+p.size,uint64_t(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
                if(stop<=current)return false;current=stop;}
            protection.push_back(&p);
        }
        // All vectors/map nodes exist before changing OS protection. A rejected
        // transition rolls back to the preceding watched modes, retaining proof.
        for(size_t i=0;i<protection.size();++i){auto& p=*protection[i];DWORD previous{};
            if(!VirtualProtect(reinterpret_cast<void*>(p.begin),p.size,PAGE_NOACCESS,&previous)){
                for(size_t j=0;j<i;++j){const auto& old=*protection[j];DWORD ignored{};
                    if(!VirtualProtect(reinterpret_cast<void*>(old.begin),old.size,old.watched,&ignored))
                        throw std::runtime_error("Cannot restore mixed native ownership protection");}
                return false;
            }
        }
        for(auto* p:protection)p->watched=PAGE_NOACCESS;
        ranges.erase(first,last);ranges.merge(replacement);
        return true;
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
