// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include <thread>
#include <condition_variable>
#include <cstdio>
using namespace NativeDX12;
static void Require(bool value){if(!value)throw std::runtime_error("Native ownership/cache assertion failed");}
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
    std::recursive_mutex execution;std::mutex gate;std::condition_variable condition;bool entered=false,release=false;
    std::atomic<unsigned> resolutions{},finished{};PageWatch* watcher{};
    {
        PageWatch watch([&](uint64_t address){std::lock_guard rendering(execution);
            if(!watcher->Contains(address))return;
            {std::unique_lock waiting(gate);entered=true;condition.notify_all();condition.wait(waiting,[&]{return release;});}
            // Model committing a retired GPU result through physical backing:
            // this alias stays accessible while the guest view is guarded.
            memset(alias,0x7a,bytes);++resolutions;watcher->Untrack(reinterpret_cast<uint64_t>(guest),bytes);
        });watcher=&watch;
        Require(watch.Track(reinterpret_cast<uint64_t>(guest),bytes));
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
        PageWatch watch([&](uint64_t){++callbacks;});
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
