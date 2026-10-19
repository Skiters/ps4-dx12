// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/guest_page_range.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include <thread>
#include <condition_variable>
#include <cstdio>
using namespace NativeDX12;
static void Require(bool value){if(!value)throw std::runtime_error("Native ownership/cache assertion failed");}
static_assert(GuestProtectionPages(0x2100,0x1200)->address==0x2000);
static_assert(GuestProtectionPages(0x2100,0x1200)->size==0x2000);
static_assert(!GuestProtectionPages(UINT64_MAX-7,8));
static_assert(!GuestProtectionPages(0x1000,0));
static_assert(ShareGuestProtectionPages(0,UINT64_MAX,0x2100,1));
static_assert(!ShareGuestProtectionPages(0x2000,4096,0x3000,4096));
// Explicit exception injection makes rejected faults observable without an
// invalid store or dependence on the compiler's asynchronous SEH support.
// The GPU ownership test below this suite uses genuine CPU page faults.
__declspec(noinline) static bool DispatchWriteFault(volatile unsigned char* address){
    const ULONG_PTR parameters[]{1,reinterpret_cast<ULONG_PTR>(address)};
    __try{RaiseException(EXCEPTION_ACCESS_VIOLATION,0,2,parameters);return true;}
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
int main(){try{
    constexpr size_t bytes=1024*1024;
    HANDLE section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,bytes,nullptr);Require(section!=nullptr);
    auto* guest=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));
    auto* alias=static_cast<std::byte*>(MapViewOfFile(section,FILE_MAP_ALL_ACCESS,0,0,bytes));Require(guest&&alias);
    {
        unsigned callbacks{};PageWatch* unresolved{};
        PageWatch watch([&](uint64_t address,bool){++callbacks;unresolved->InvalidateBackingEpoch(address,1);});
        unresolved=&watch;const auto base=reinterpret_cast<uint64_t>(guest);
        Require(watch.Track(base,4096));
        Require(!DispatchWriteFault(reinterpret_cast<volatile unsigned char*>(guest))&&callbacks==1);
        watch.Untrack(base,4096); // A physical epoch bump cannot suppress an unresolved access violation.
    }
    std::recursive_mutex execution;std::mutex gate;std::condition_variable condition;bool entered=false,release=false;
    std::atomic<unsigned> resolutions{},finished{};PageWatch* watcher{};
    {
        PageWatch watch([&](uint64_t address,bool writing){std::lock_guard rendering(execution);Require(!writing);
            if(!watcher->Contains(address))return;
            {std::unique_lock waiting(gate);entered=true;condition.notify_all();condition.wait(waiting,[&]{return release;});}
            // Model committing a retired GPU result through physical backing:
            // this alias stays accessible while the guest view is guarded.
            memset(alias,0x7a,bytes);++resolutions;watcher->Untrack(reinterpret_cast<uint64_t>(guest),bytes);
        });watcher=&watch;
        Require(watch.Track(reinterpret_cast<uint64_t>(guest),bytes));
        const auto base=reinterpret_cast<uint64_t>(guest);
        const auto epoch=watch.ProtectedBackingEpoch(base,bytes);
        Require(epoch&&watch.ProtectedBackingEpoch(base+17,4096)==epoch);
        Require(!watch.ProtectedBackingEpoch(base,bytes+4096));
        Require(!watch.ProtectedBackingEpoch(UINT64_MAX-1,4));
        watch.InvalidateBackingEpoch(base+17,1);
        Require(watch.ProtectedBackingEpoch(base,bytes)>epoch);
        Require(watch.ChangeMode(base+4096,4096,true));
        Require(watch.ProtectedBackingEpoch(base,4096)>epoch); // Splits retain physical-write history.
        Require(watch.ChangeMode(base+4096,4096,false));
        MEMORY_BASIC_INFORMATION protected_state{};VirtualQuery(guest,&protected_state,sizeof(protected_state));
        Require(protected_state.Protect==PAGE_NOACCESS); // Epoch invalidation never demotes I.
        Require(!watch.Track(reinterpret_cast<uint64_t>(guest)+4096,bytes-4096));
        std::vector<std::jthread> readers;
        for(unsigned i=0;i<4;++i)readers.emplace_back([&,i]{auto* pointer=reinterpret_cast<volatile unsigned char*>(guest+i*4096);
            if(*pointer!=0x7a)std::terminate();++finished;});
        {std::unique_lock waiting(gate);Require(condition.wait_for(waiting,std::chrono::seconds(2),[&]{return entered;}));Require(finished==0);release=true;condition.notify_all();}
        readers.clear();Require(finished==4&&resolutions==1&&!watch.Contains(reinterpret_cast<uint64_t>(guest)));
        guest[17]=std::byte{0x42};Require(alias[17]==std::byte{0x42});
        Require(watch.Track(reinterpret_cast<uint64_t>(guest),bytes));
        watch.Untrack(reinterpret_cast<uint64_t>(guest)+4096,4096);Require(!watch.Contains(reinterpret_cast<uint64_t>(guest)));
    }
    {
        unsigned callbacks{};
        PageWatch watch([&](uint64_t,bool writing){Require(writing);++callbacks;});
        Require(watch.Track(reinterpret_cast<uint64_t>(guest),bytes));
        Require(!DispatchWriteFault(reinterpret_cast<volatile unsigned char*>(guest))&&callbacks==1);
        MEMORY_BASIC_INFORMATION region{};
        Require(VirtualQuery(guest,&region,sizeof(region))&&region.Protect==PAGE_NOACCESS);
        Require(watch.Contains(reinterpret_cast<uint64_t>(guest)));
        watch.Untrack(reinterpret_cast<uint64_t>(guest),bytes);
        DWORD previous{};Require(VirtualProtect(guest,bytes,PAGE_READONLY,&previous));
        Require(!DispatchWriteFault(reinterpret_cast<volatile unsigned char*>(guest))&&callbacks==1);
        Require(VirtualProtect(guest,bytes,PAGE_READWRITE,&previous));
        Require(!DispatchWriteFault(nullptr)&&callbacks==1);
    }
    {
        unsigned callbacks{};PageWatch* active{};
        PageWatch watch([&](uint64_t){
            ++callbacks;active->Untrack(reinterpret_cast<uint64_t>(guest),bytes);
            Require(active->Track(reinterpret_cast<uint64_t>(guest),bytes));
        });active=&watch;
        Require(watch.Track(reinterpret_cast<uint64_t>(guest),bytes));
        Require(!DispatchWriteFault(reinterpret_cast<volatile unsigned char*>(guest))&&callbacks==64);
        Require(watch.Contains(reinterpret_cast<uint64_t>(guest)));
    }
    {
        // Real shared physical aliases, two independent I ranges, a stronger B
        // owner on the tail page, and stores outside I followed by unguarded
        // stores inside I. Exercise the production full-byte witness helpers.
        memset(alias,0x17,8192);
        const auto base=reinterpret_cast<uint64_t>(guest);
        GuestImageBoundaryPages a,b;
        std::vector<std::byte> logical_i(4608,std::byte{0xa5});
        unsigned callbacks{};PageWatch* active{};
        const auto read=[&](uint64_t address,std::span<std::byte> out){
            if(address<base||address-base>bytes||out.size()>bytes-(address-base))return false;
            memcpy(out.data(),alias+size_t(address-base),out.size());return true;
        };
        PageWatch watch([&](uint64_t address,bool writing){
            Require(writing);++callbacks;
            RememberGuestImageBoundary(a,base+256,4608,address,1,read);
            RememberGuestImageBoundary(b,base+512,128,address,1,read);
            Require(active->UntrackReadOnly(address&~uint64_t(4095),4096,true));
        });active=&watch;
        const auto protect=[&](uint64_t address,uint64_t size){return watch.TrackReadOnly(address,size,true);};
        Require(watch.TrackReadOnly(base,8192));
        Require(watch.TrackOwned(base+4096,4096));
        *reinterpret_cast<volatile unsigned char*>(guest+10)=0x31;
        Require(callbacks==1&&a.size()==1&&b.size()==1&&a.begin()->second.size()==3840&&b.begin()->second.size()==128);
        Require(!watch.Contains(base)&&watch.Contains(base+4096));
        MEMORY_BASIC_INFORMATION region{};
        Require(VirtualQuery(guest+4096,&region,sizeof(region))&&region.Protect==PAGE_NOACCESS);
        Require(ValidateGuestImageBoundary(a,base+256,read,protect).empty());
        Require(ValidateGuestImageBoundary(b,base+512,read,protect).empty());
        Require(watch.Contains(base)&&logical_i.front()==std::byte{0xa5}&&alias[256]==std::byte{0x17});
        *reinterpret_cast<volatile unsigned char*>(guest+11)=0x32;
        Require(callbacks==2&&a.size()==1&&b.size()==1);
        *reinterpret_cast<volatile unsigned char*>(guest+600)=0x83;
        // Repeated outside stores must not replace the original B snapshot.
        Require(!RememberGuestImageBoundary(a,base+256,4608,base+12,1,read));
        const auto changed_a=ValidateGuestImageBoundary(a,base+256,read,protect);
        const auto changed_b=ValidateGuestImageBoundary(b,base+512,read,protect);
        Require(callbacks==2&&changed_a==std::vector<uint64_t>{base}&&changed_b==changed_a);
        Require(a.empty()&&b.empty()&&watch.Contains(base));
        // Releasing the stronger B owner restores the independent I tail guard.
        watch.Untrack(base+4096,4096);Require(watch.TrackReadOnly(base,8192,true));
        *reinterpret_cast<volatile unsigned char*>(guest+5000)=0x42;
        Require(callbacks==3&&a.size()==1&&b.empty()&&a.begin()->second.size()==768);
        *reinterpret_cast<volatile unsigned char*>(guest+4700)=0x94;
        Require(watch.Contains(base)&&!watch.Contains(base+4096));
        bool rejected=false;
        try{ValidateGuestImageBoundary(a,base+256,read,[](uint64_t,uint64_t){return false;});}
        catch(const std::runtime_error&){rejected=true;}
        Require(rejected&&a.size()==1); // failed reprotection cannot certify I
        Require(ValidateGuestImageBoundary(a,base+256,read,protect)==std::vector<uint64_t>{base+4096});
        Require(callbacks==3&&a.empty()&&watch.Contains(base)&&watch.Contains(base+4096));
        Require(alias[600]==std::byte{0x83}&&alias[4700]==std::byte{0x94}&&logical_i.back()==std::byte{0xa5});
        watch.Untrack(base,8192);
    }
    {
        // Split D/S witnesses must remain distinct even on one host page.
        // Use genuine write faults and the physical alias, not injected VEH
        // exceptions or a second copy of the production comparison algorithm.
        const auto base=reinterpret_cast<uint64_t>(guest);
        for(const size_t stencil_offset:{size_t{1024},size_t{4608}}){
            memset(alias,0x27,12288);
            GuestImageBoundaryPages depth,stencil;
            const std::array planes{
                GuestImageBoundaryPlane{base+256,uint64_t{512},&depth},
                GuestImageBoundaryPlane{base+stencil_offset,uint64_t{128},&stencil}};
            const auto stencil_page=(base+stencil_offset)&~uint64_t(4095);
            unsigned callbacks{};PageWatch* active{};
            const auto read=[&](uint64_t address,std::span<std::byte> out){
                if(address<base||address-base>bytes||out.size()>bytes-(address-base))return false;
                memcpy(out.data(),alias+size_t(address-base),out.size());return true;
            };
            PageWatch watch([&](uint64_t address,bool writing){
                Require(writing);++callbacks;
                for(const auto& plane:planes)
                    RememberGuestImageBoundary(*plane.saved,plane.address,plane.bytes,address,1,read);
                Require(active->UntrackReadOnly(address&~uint64_t(4095),4096,true));
            });active=&watch;
            Require(watch.TrackReadOnly(base,8192));
            Require(watch.TrackOwned(base+8192,4096));
            const auto outside_stores=[&]{
                *reinterpret_cast<volatile unsigned char*>(guest+10)=0x61;
                if(stencil_page!=base)*reinterpret_cast<volatile unsigned char*>(guest+4106)=0x62;
            };
            outside_stores();
            Require(depth.size()==1&&stencil.size()==1&&depth.begin()->second.size()==512&&
                stencil.begin()->second.size()==128);
            std::vector<std::pair<uint64_t,uint64_t>> protected_ranges;
            for(const auto& plane:planes)VisitGuestImageProtectedRanges(plane,[&](uint64_t a,uint64_t n){
                protected_ranges.emplace_back(a,n);});
            Require(protected_ranges.empty()); // both released boundary pages are holes
            // A same-value store must not fabricate a content change in I.
            *reinterpret_cast<volatile unsigned char*>(guest+stencil_offset)=0x27;
            const auto all_guards_before_read=[&](uint64_t a,std::span<std::byte> out){
                MEMORY_BASIC_INFORMATION region{};
                for(const auto page:{base,stencil_page})
                    Require(VirtualQuery(reinterpret_cast<void*>(page),&region,sizeof(region))&&region.Protect==PAGE_READONLY);
                return read(a,out);
            };
            const auto protect=[&](uint64_t a,uint64_t n){return watch.TrackReadOnly(a,n,true);};
            Require(ValidateGuestImageBoundaries(planes,all_guards_before_read,protect).empty());
            Require(depth.empty()&&stencil.empty());
            outside_stores();
            // Failed reprotection must compare nothing and retain BOTH proofs.
            unsigned reads{};bool rejected=false;
            try{ValidateGuestImageBoundaries(planes,[&](uint64_t,std::span<std::byte>){++reads;return true;},
                [](uint64_t,uint64_t){return false;});}catch(const std::runtime_error&){rejected=true;}
            Require(rejected&&!reads&&depth.size()==1&&stencil.size()==1);
            // A late S read error must not clear the already compared D proof.
            rejected=false;
            try{ValidateGuestImageBoundaries(planes,[&](uint64_t a,std::span<std::byte> out){
                return a==planes[1].address?false:read(a,out);},protect);
            }catch(const std::runtime_error&){rejected=true;}
            Require(rejected&&depth.size()==1&&stencil.size()==1);
            // Those failed reads restored protection, so make a real outside
            // fault before the unguarded inside-S write; retain earliest bytes.
            outside_stores();
            const auto count=callbacks;
            *reinterpret_cast<volatile unsigned char*>(guest+stencil_offset+7)=0x93;
            Require(callbacks==count);
            const auto changed=ValidateGuestImageBoundaries(planes,all_guards_before_read,protect);
            Require(changed==std::vector<uint64_t>{stencil_page}&&depth.empty()&&stencil.empty());
            Require(alias[256]==std::byte{0x27}&&alias[stencil_offset+7]==std::byte{0x93});
            for(const auto& plane:planes)VisitGuestImageProtectedRanges(plane,[&](uint64_t a,uint64_t n){
                protected_ranges.emplace_back(a,n);});
            Require(protected_ranges.size()==2&&protected_ranges[0]==std::pair<uint64_t,uint64_t>{base,4096}&&
                protected_ranges[1]==std::pair<uint64_t,uint64_t>{stencil_page,4096});
            Require(watch.UntrackReadOnly(base,12288,true));
            MEMORY_BASIC_INFORMATION region{};
            Require(VirtualQuery(guest+8192,&region,sizeof(region))&&region.Protect==PAGE_NOACCESS);
            watch.Untrack(base+8192,4096);
        }
        puts("PASS: two-plane shared/separate-page boundary proofs, all guards before reads, same-value stores, read/protection failure retention and stronger B guard");
    }
    {
        const auto base=reinterpret_cast<uint64_t>(guest);
        PageWatch* active{};unsigned writes{};
        PageWatch watch([&](uint64_t address,bool writing){
            Require(writing);++writes;Require(active->UntrackReadOnly(address&~uint64_t(4095),4096,true));
        });active=&watch;
        const std::array separate{std::pair{base,uint64_t{4096}},std::pair{base+8192,uint64_t{4096}}};
        const auto protection=[&](uint64_t address){MEMORY_BASIC_INFORMATION info{};
            Require(VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info)));return info.Protect;};
        Require(watch.Track(base,4096)&&watch.Track(base+8192,4096));
        // A broken second exclusive-plane proof must not demote the first.
        DWORD previous{};Require(VirtualProtect(guest+8192,4096,PAGE_READONLY,&previous));
        Require(!watch.TrackImageReadOnly(separate,true));
        Require(protection(base)==PAGE_NOACCESS&&protection(base+8192)==PAGE_READONLY&&
            watch.Contains(base)&&watch.Contains(base+8192));
        Require(VirtualProtect(guest+8192,4096,PAGE_NOACCESS,&previous));
        Require(watch.TrackImageReadOnly(separate,true));
        Require(protection(base)==PAGE_READONLY&&protection(base+4096)==PAGE_READWRITE&&
            protection(base+8192)==PAGE_READONLY);
        *reinterpret_cast<volatile unsigned char*>(guest+7)=0x91;
        Require(writes==1&&alias[7]==std::byte{0x91}&&!watch.Contains(base)&&watch.Contains(base+8192));
        Require(watch.TrackImageReadOnly(separate));
        Require(protection(base)==PAGE_READONLY&&protection(base+8192)==PAGE_READONLY);
        watch.Untrack(base,12288);
        // Acquisition over new writable pages also validates S before any
        // changes. Untracked readonly pages are not a legitimate witness.
        Require(VirtualProtect(guest+8192,4096,PAGE_READONLY,&previous));
        Require(!watch.TrackImageReadOnly(separate));
        Require(protection(base)==PAGE_READWRITE&&!watch.Contains(base));
        Require(VirtualProtect(guest+8192,4096,PAGE_READWRITE,&previous));
        Require(watch.TrackOwned(base,16384));
        // D and S share a page, the union crosses a stronger B subrange, and
        // the tail beyond both planes belongs to another exclusive owner.
        const std::array shared{std::pair{base,uint64_t{12288}},std::pair{base+8192,uint64_t{4096}}};
        const std::array strong{std::pair{base+4096,uint64_t{4096}}};
        Require(watch.TrackImageReadOnly(shared,true,strong));
        Require(protection(base)==PAGE_READONLY&&protection(base+4096)==PAGE_NOACCESS&&
            protection(base+8192)==PAGE_READONLY&&protection(base+12288)==PAGE_NOACCESS);
        Require(watch.UntrackReadOnly(base,16384,true));
        Require(protection(base)==PAGE_READWRITE&&protection(base+4096)==PAGE_NOACCESS&&
            protection(base+8192)==PAGE_READWRITE&&protection(base+12288)==PAGE_NOACCESS);
        watch.Untrack(base,16384);
        // A claimed stronger B guard in a gap is a broken ownership contract,
        // including when that gap starts BEFORE B, not just at its first page.
        Require(!watch.TrackImageReadOnly(shared,false,strong));
        Require(protection(base)==PAGE_READWRITE&&!watch.Contains(base));
        Require(watch.Track(base+4096,4096));
        Require(watch.TrackImageReadOnly(shared,false,strong));
        Require(protection(base)==PAGE_READONLY&&protection(base+4096)==PAGE_NOACCESS&&
            protection(base+8192)==PAGE_READONLY&&protection(base+12288)==PAGE_READWRITE);
        const auto stable=protection(base);
        const std::array invalid{std::pair{base,uint64_t{4096}},std::pair{base+8192,uint64_t{1}}};
        Require(!watch.TrackImageReadOnly(invalid)&&protection(base)==stable);
        watch.Untrack(base,16384);
        // A NEW copied image can inherit only its explicit old image's strong
        // guards. Strong B and an unrelated exclusive neighbour stay intact.
        Require(watch.TrackOwned(base,16384));
        const std::array replacement{std::pair{base,uint64_t{12288}}};
        const std::array transferred{std::pair{base,uint64_t{8192}}};
        Require(watch.TrackImageReadOnly(replacement,false,strong,transferred));
        Require(protection(base)==PAGE_READONLY&&protection(base+4096)==PAGE_NOACCESS&&
            protection(base+8192)==PAGE_NOACCESS&&protection(base+12288)==PAGE_NOACCESS);
        watch.Untrack(base,16384);
        Require(watch.Track(base,4096));
        // A claimed old guard cannot be manufactured out of writable gaps;
        // rejection must leave the first, valid old guard unchanged.
        const std::array missing_source{std::pair{base,uint64_t{12288}}};
        Require(!watch.TrackImageReadOnly(replacement,false,{},missing_source));
        Require(protection(base)==PAGE_NOACCESS&&protection(base+8192)==PAGE_READWRITE);
        watch.Untrack(base,16384);
        puts("PASS: transactional image-plane acquisition/demotion, second-plane rejection, shared union, stronger B and outside-owner preservation");
    }
    UnmapViewOfFile(alias);UnmapViewOfFile(guest);CloseHandle(section);
    {
        auto* pages=static_cast<unsigned char*>(VirtualAlloc(nullptr,16384,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Require(pages);
        const auto base=reinterpret_cast<uint64_t>(pages);PageWatch* active{};unsigned callbacks{};
        {
            PageWatch watch([&](uint64_t address){++callbacks;
                Require(active->UntrackReadOnly(address&~uint64_t(4095),4096));});active=&watch;
            Require(watch.TrackReadOnly(base,16384));
            *reinterpret_cast<volatile unsigned char*>(pages+4096+7)=0x35;
            Require(callbacks==1&&!watch.Contains(base+4096)&&watch.Contains(base)&&watch.Contains(base+8192));
            MEMORY_BASIC_INFORMATION region{};Require(VirtualQuery(pages,&region,sizeof(region))&&region.Protect==PAGE_READONLY);
            Require(!watch.ChangeMode(base,16384,false)); // a hole is not an atomic proof
            Require(watch.TrackReadOnly(base,16384));
            Require(watch.ChangeMode(base,16384,false));
            Require(VirtualQuery(pages+4096,&region,sizeof(region))&&region.Protect==PAGE_NOACCESS);
            Require(!watch.UntrackReadOnly(base+4096,4096)); // cannot release GPU ownership
            Require(watch.ChangeMode(base,16384,true));
            *reinterpret_cast<volatile unsigned char*>(pages+8192+3)=0x81;
            Require(callbacks==2&&pages[4096+7]==0x35&&pages[8192+3]==0x81);
            watch.Untrack(base,16384);
            Require(VirtualQuery(pages,&region,sizeof(region))&&region.Protect==PAGE_READWRITE);
        }
        Require(VirtualFree(pages,0,MEM_RELEASE));
    }
    {
        auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,16384,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        Require(memory);const auto base=reinterpret_cast<uint64_t>(memory);
        {
            PageWatch watch([](uint64_t){throw std::runtime_error("mixed ownership probe unexpectedly faulted");});
            Require(watch.TrackReadOnly(base,16384));
            Require(watch.TrackOwned(base+4096,8192));
            MEMORY_BASIC_INFORMATION region{};
            const auto mode=[&](uint64_t offset,DWORD expected){
                Require(VirtualQuery(memory+offset,&region,sizeof(region))&&region.Protect==expected);
            };
            mode(0,PAGE_READONLY);mode(4096,PAGE_NOACCESS);mode(8192,PAGE_NOACCESS);mode(12288,PAGE_READONLY);
            Require(watch.Contains(base)&&watch.Contains(base+4096)&&watch.Contains(base+12288));
            Require(watch.ChangeMode(base+4096,8192,true));
            Require(watch.UntrackReadOnly(base+4096,4096));
            mode(4096,PAGE_READWRITE);mode(0,PAGE_READONLY);mode(8192,PAGE_READONLY);
            Require(watch.TrackOwned(base,16384));
            for(uint64_t offset=0;offset<16384;offset+=4096)mode(offset,PAGE_NOACCESS);
            watch.Untrack(base,16384);
            for(uint64_t offset=0;offset<16384;offset+=4096)mode(offset,PAGE_READWRITE);
            DWORD previous{};Require(VirtualProtect(memory+12288,4096,PAGE_READONLY,&previous));
            Require(!watch.TrackOwned(base,16384));
            mode(0,PAGE_READWRITE);mode(4096,PAGE_READWRITE);mode(8192,PAGE_READWRITE);mode(12288,PAGE_READONLY);
            Require(!watch.Contains(base)&&!watch.Contains(base+12288));
            Require(VirtualProtect(memory+12288,4096,PAGE_READWRITE,&previous));
            Require(watch.TrackReadOnly(base,16384));
            Require(VirtualProtect(memory+8192,4096,PAGE_READWRITE,&previous));
            Require(!watch.TrackOwned(base+4096,8192)); // changed OS permissions cannot prove ownership
            mode(0,PAGE_READONLY);mode(4096,PAGE_READONLY);mode(8192,PAGE_READWRITE);mode(12288,PAGE_READONLY);
            Require(VirtualProtect(memory+8192,4096,PAGE_READONLY,&previous));
            watch.Untrack(base,16384);
            for(uint64_t offset=0;offset<16384;offset+=4096)mode(offset,PAGE_READWRITE);
        }
        Require(VirtualFree(memory,0,MEM_RELEASE));
    }
    {
        constexpr uint64_t pages=4097,stride=8192;
        auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,pages*stride,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        Require(memory!=nullptr);
        {
            PageWatch watch([](uint64_t){throw std::runtime_error("retired page called resolver");});
            const auto base=reinterpret_cast<uint64_t>(memory);
            for(uint64_t i=0;i<pages;++i){Require(watch.Track(base+i*stride,4096,true));watch.Untrack(base+i*stride,4096);}
            Require(watch.WasTracked(base)&&watch.WasTracked(base+(pages-1)*stride));
            Require(!watch.WasTracked(base+4096)); // do not fill unwatched holes
            Require(DispatchWriteFault(memory)); // delayed fault on a restored valid page
        }
        Require(VirtualFree(memory,0,MEM_RELEASE));
    }
    {
        auto* memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,16384,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Require(memory);
        const auto base=reinterpret_cast<uint64_t>(memory);memory[4096]=37;
        PageWatch* active{};unsigned reads{},writes{};
        {
            PageWatch watch([&](uint64_t address,bool writing){
                const auto page=address&~uint64_t(4095);
                if(writing){++writes;Require(active->UntrackReadOnly(page,4096));}
                else{++reads;active->Untrack(page,4096);Require(active->TrackReadOnly(base,16384,true));}
            });active=&watch;
            Require(watch.Track(base+4096,4096));
            Require(!watch.TrackReadOnly(base,16384)); // readable witness cannot borrow GPU ownership
            Require(watch.TrackReadOnly(base,16384,true)); // independent image needs only a write guard
            MEMORY_BASIC_INFORMATION info{};
            Require(VirtualQuery(memory+4096,&info,sizeof(info))&&info.Protect==PAGE_NOACCESS);
            Require(watch.UntrackReadOnly(base,16384,true)); // release image, preserve buffer owner
            Require(VirtualQuery(memory,&info,sizeof(info))&&info.Protect==PAGE_READWRITE);
            Require(VirtualQuery(memory+4096,&info,sizeof(info))&&info.Protect==PAGE_NOACCESS);
            Require(watch.TrackReadOnly(base,16384,true));
            const auto physical=*reinterpret_cast<volatile unsigned char*>(memory+4096);
            Require(reads==1&&physical==37);
            Require(VirtualQuery(memory+4096,&info,sizeof(info))&&info.Protect==PAGE_READONLY);
            *reinterpret_cast<volatile unsigned char*>(memory+8192)=53;
            Require(writes==1&&memory[8192]==53);
            Require(VirtualQuery(memory,&info,sizeof(info))&&info.Protect==PAGE_READONLY);
            Require(VirtualQuery(memory+12288,&info,sizeof(info))&&info.Protect==PAGE_READONLY);
            watch.Untrack(base,16384);
        }
        Require(VirtualFree(memory,0,MEM_RELEASE));
    }
    auto directory=std::filesystem::temp_directory_path()/("native-cache-test-"+std::to_string(GetCurrentProcessId()));
    PersistentCache cache(directory);CacheKey key;key.Text("shader inputs");const std::byte payload[]{std::byte{1},std::byte{2},std::byte{3}};
    cache.Store(key.bytes,payload);std::vector<std::byte> result;Require(cache.Load(key.bytes,result)&&result==std::vector<std::byte>(std::begin(payload),std::end(payload)));
    auto other=key;other.Add(17u);Require(!cache.Load(other.bytes,result));
    std::filesystem::path cached;
    for(const auto& entry:std::filesystem::directory_iterator(directory))if(entry.path().extension()==".bin")cached=entry.path();
    {std::ofstream corrupt(cached,std::ios::binary|std::ios::trunc);corrupt<<"partial cache file";}
    Require(!cache.Load(key.bytes,result));cache.Store(key.bytes,payload);Require(cache.Load(key.bytes,result));
    std::filesystem::remove(cached);std::filesystem::remove(directory);
    puts("PASS: four CPU faults wait for backing commit, one resolution, permission restoration, partial untrack; atomic cache writes, exact keys and corruption fallback");return 0;
}catch(const std::exception& e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
