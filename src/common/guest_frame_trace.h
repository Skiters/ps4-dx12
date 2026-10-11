// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "common/types.h"

namespace Common::GuestFrameTrace {
inline bool Enabled(){static const bool enabled=std::getenv("SHADPS4_FRAME_CAPTURE")!=nullptr;return enabled;}
inline double Now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
// A bounded, opt-in capture of NEW guest flips. No repeated-frame/blank-frame
// events. Completion is observed asynchronously (2 ms polling), never by adding
// a GPU wait. A marker arms capture only after the measured scene has loaded.
class Capture {
    struct Event {u64 id;const char* phase;double time;u64 fence;};
    struct Pending {u64 id,fence;std::function<bool()> ready;};
    std::mutex mutex;
    std::vector<Event> events;
    std::vector<Pending> pending;
    std::atomic<bool> armed{};
    std::atomic<u64> sequence{};
    u64 dropped{},written{};
    FILE* file{};
    std::jthread worker;
    void Add(u64 id,const char* phase,u64 fence=0){
        if(events.size()<4096)events.push_back({id,phase,Now(),fence});else ++dropped;
    }
public:
    Capture():file(fopen("user/log/guest-frames.csv","wb")){
        events.reserve(4096);pending.reserve(64);if(file)fprintf(file,"frame_id,phase,host_ms,fence\n");
        worker=std::jthread([this](std::stop_token stop){
        unsigned tick{};
        while(!stop.stop_requested()){
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            std::lock_guard lock(mutex);
            if(tick++%250==0){
                std::error_code error;const auto* marker=std::getenv("SHADPS4_FRAME_CAPTURE");
                const bool active=marker&&std::filesystem::exists(marker,error)&&written<50000;
                if(active!=armed.load()){armed=active;Add(0,active?"capture-start":"capture-stop");}
            }
            for(auto it=pending.begin();it!=pending.end();){
                if(it->ready()){if(armed)Add(it->id,"gpu-complete",it->fence);it=pending.erase(it);}else ++it;
            }
            if(file&&!events.empty()){
                for(const auto& e:events)if(written++<50000)fprintf(file,"%llu,%s,%.3f,%llu\n",e.id,e.phase,e.time,e.fence);
                if(dropped){fprintf(file,"0,dropped,%.3f,%llu\n",Now(),dropped);dropped=0;}
                fflush(file);
            }
            events.clear();
        }
        });
    }
    ~Capture(){worker.request_stop();worker.join();if(file)fclose(file);}
    static Capture& Get(){static Capture capture;return capture;}
    u64 New(){const auto id=++sequence;Record(id,"created");return id;}
    void Record(u64 id,const char* phase,u64 fence=0){if(!armed||!id)return;std::lock_guard lock(mutex);Add(id,phase,fence);}
    void Await(u64 id,u64 fence,std::function<bool()> ready){
        if(!armed||!id)return;std::lock_guard lock(mutex);
        if(pending.size()<64)pending.push_back({id,fence,std::move(ready)});else ++dropped;
    }
};
inline u64 New(){return Enabled()?Capture::Get().New():0;}
inline void Record(u64 id,const char* phase,u64 fence=0){if(id&&Enabled())Capture::Get().Record(id,phase,fence);}
inline void Await(u64 id,u64 fence,std::function<bool()> ready){if(id&&Enabled())Capture::Get().Await(id,fence,std::move(ready));}
}
