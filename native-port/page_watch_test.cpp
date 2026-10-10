// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/page_watch.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include <thread>
#include <condition_variable>
#include <cstdio>
using namespace NativeDX12;
static void Require(bool value){if(!value)throw std::runtime_error("Native ownership/cache assertion failed");}
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
    UnmapViewOfFile(alias);UnmapViewOfFile(guest);CloseHandle(section);
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
