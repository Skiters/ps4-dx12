// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

namespace Common::GuestFrameTrace {
using u64=std::uint64_t;
inline bool Enabled(){static const bool enabled=std::getenv("SHADPS4_FRAME_CAPTURE")!=nullptr;return enabled;}
inline double Now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// A bounded, opt-in capture of NEW guest flips. No repeated-frame/blank-frame
// events. The scheduler reports an existing timeline completion asynchronously;
// its observation time can lag GPU completion until the next scheduler poll.
// The 2 ms worker flushes records; it does not provide GPU completion timestamps.
// A marker arms capture only after the measured scene has loaded.
class Capture {
    struct Event {u64 id;const char* phase;double time;u64 fence;};
    std::mutex mutex;
    std::vector<Event> events;
    std::atomic<bool> armed{};
    std::atomic<u64> sequence{};
    u64 dropped{},written{};
    bool storage_limited{};
    FILE* file{};
    std::jthread worker;
    void Add(u64 id,const char* phase,u64 fence=0){
        if(events.size()<4096)events.push_back({id,phase,Now(),fence});else ++dropped;
    }
    // Called under mutex, including the final drain at orderly destruction.
    void Flush(){
        if(file){
            for(const auto& e:events){
                if(written<50000){fprintf(file,"%llu,%s,%.3f,%llu\n",e.id,e.phase,e.time,e.fence);++written;}
                else ++dropped;
            }
            if(dropped){fprintf(file,"0,dropped,%.3f,%llu\n",Now(),dropped);dropped=0;}
            if(written>=50000&&!storage_limited){
                storage_limited=true;armed=false;
                fprintf(file,"0,storage-cap,%.3f,%llu\n",Now(),written);
            }
            fflush(file);
        }
        events.clear();
    }
public:
    Capture():file(fopen("user/log/guest-frames.csv","wb")){
        events.reserve(4096);if(file)fprintf(file,"frame_id,phase,host_ms,fence\n");
        else fprintf(stderr,"Guest frame capture could not open user/log/guest-frames.csv\n");
        worker=std::jthread([this](std::stop_token stop){
        double next_marker_check{};
        while(!stop.stop_requested()){
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            std::lock_guard lock(mutex);
            const auto now=Now();
            if(now>=next_marker_check){
                next_marker_check=now+500;
                std::error_code error;const auto* marker=std::getenv("SHADPS4_FRAME_CAPTURE");
                const bool active=file&&marker&&std::filesystem::exists(marker,error)&&!storage_limited;
                if(active!=armed.load()){armed=active;Add(0,active?"capture-start":"capture-stop");}
            }
            if(!events.empty()||dropped)Flush();
        }
        std::lock_guard lock(mutex);Flush();
        });
    }
    ~Capture(){worker.request_stop();worker.join();if(file)fclose(file);}
    static Capture& Get(){static Capture capture;return capture;}
    u64 New(){const auto id=++sequence;Record(id,"created");return id;}
    void Record(u64 id,const char* phase,u64 fence=0){if(!armed||!id)return;std::lock_guard lock(mutex);Add(id,phase,fence);}
};
inline u64 New(){return Enabled()?Capture::Get().New():0;}
inline void Record(u64 id,const char* phase,u64 fence=0){if(id&&Enabled())Capture::Get().Record(id,phase,fence);}
}
