// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_set>
#include <vector>
#include "common/types.h"

namespace Common::GuestFrameTrace {
enum class Completion { Pending, Complete, Failed };
inline bool Enabled(){static const bool enabled=std::getenv("SHADPS4_FRAME_CAPTURE")!=nullptr;return enabled;}
inline double Now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline FILE* OpenLog(const char* destination){
#ifdef _WIN32
    FILE* file{};
    if(destination)fopen_s(&file,destination,"wb");
    else {wchar_t* path{};size_t size{};
        if(_wdupenv_s(&path,&size,L"SHADPS4_FRAME_TRACE_LOG")!=0)return nullptr;
        _wfopen_s(&file,path?path:L"user/log/guest-frames.csv",L"wb");free(path);}
    return file;
#else
    const auto* path=std::getenv("SHADPS4_FRAME_TRACE_LOG");return fopen(destination?destination:path?path:"user/log/guest-frames.csv","wb");
#endif
}
inline std::filesystem::path MarkerPath(){
#ifdef _WIN32
    wchar_t* marker{};size_t size{};
    if(_wdupenv_s(&marker,&size,L"SHADPS4_FRAME_CAPTURE")!=0)throw std::runtime_error("Capture marker environment failed");
    const std::filesystem::path path=marker?marker:L"";free(marker);return path;
#else
    const auto* marker=std::getenv("SHADPS4_FRAME_CAPTURE");return marker?marker:"";
#endif
}
// A bounded, opt-in capture of NEW guest flips. No repeated-frame/blank-frame
// events. Completion is observed asynchronously (2 ms polling), never by adding
// a GPU wait. A marker arms capture only after the measured scene has loaded.
class Capture {
    struct Event {u64 id;const char* phase;double time;u64 fence,observed,completed;size_t pending;bool valid;};
    struct Pending {u64 id,fence;std::function<Completion()> ready;};
    std::mutex mutex;
    std::vector<Event> events;
    std::vector<Pending> pending;
    std::unordered_set<u64> seen;
    std::atomic<bool> armed{};
    std::atomic<u64> sequence{};
    u64 dropped{},written{},observed{},completed{};
    bool valid{true};
    const std::filesystem::path marker;
    FILE* file{};
    std::jthread worker;
    void Add(u64 id,const char* phase,u64 fence=0){
        if(events.size()<4096)events.push_back({id,phase,Now(),fence,observed,completed,pending.size(),valid});
        else {++dropped;valid=false;}
    }
    void Poll(){
        for(auto it=pending.begin();it!=pending.end();){
            Completion status{};
            try{status=it->ready();}catch(...){status=Completion::Failed;}
            if(status==Completion::Pending){++it;continue;}
            const auto id=it->id,fence=it->fence;it=pending.erase(it);
            if(status==Completion::Complete){++completed;if(armed)Add(id,"gpu-complete",fence);}
            else {valid=false;Add(id,"gpu-completion-error",fence);}
        }
    }
    void Write(){
        if(file&&!events.empty()){
            for(const auto& e:events){
                if(written++>=50000){valid=false;continue;}
                fprintf(file,"%llu,%s,%.3f,%llu,%llu,%llu,%zu,%s\n",e.id,e.phase,e.time,e.fence,e.observed,e.completed,e.pending,e.valid?"true":"false");
            }
            if(dropped||written>50000)fprintf(file,"0,capture-overflow,%.3f,%llu,%llu,%llu,%zu,false\n",Now(),dropped,observed,completed,pending.size());
            if(fflush(file)!=0||ferror(file)){valid=false;fprintf(stderr,"Guest frame capture write failed\n");}
        }
        dropped=0;events.clear();
    }
public:
    explicit Capture(const char* destination=nullptr):marker(MarkerPath()),file(OpenLog(destination)){
        events.reserve(4096);pending.reserve(64);
        if(file)fprintf(file,"frame_id,phase,host_ms,fence,observed,completed,pending,valid\n");
        else {valid=false;fprintf(stderr,"Guest frame capture file could not be opened\n");}
        if(marker.empty()){valid=false;Add(0,"invalid-capture-marker");}
        worker=std::jthread([this](std::stop_token stop){
        unsigned tick{};
        while(!stop.stop_requested()){
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            std::lock_guard lock(mutex);
            Poll();
            if(tick++%250==0){
                std::error_code error;const bool exists=!marker.empty()&&std::filesystem::exists(marker,error);
                if(exists&&!std::filesystem::is_regular_file(marker,error)&&!error)
                    error=std::make_error_code(std::errc::invalid_argument);
                if(error&&valid){valid=false;Add(0,"capture-marker-error");}
                const bool active=exists&&valid&&written<50000;
                if(active!=armed.load()){armed=active;Add(0,active?"capture-start":"capture-stop");}
            }
            Write();
        }
        });
    }
    ~Capture(){worker.request_stop();worker.join();std::lock_guard lock(mutex);Poll();
        if(armed){valid=false;armed=false;Add(0,"capture-stopped-by-shutdown");}
        Add(0,"device-stop");Write();if(file)fclose(file);}
    static Capture& Get(){static Capture capture;return capture;}
    u64 New(){const auto id=++sequence;Record(id,"created");return id;}
    void Record(u64 id,const char* phase,u64 fence=0){if(!armed||!id)return;std::lock_guard lock(mutex);Add(id,phase,fence);}
    void Invalid(const char* reason){std::lock_guard lock(mutex);valid=false;Add(0,reason);}
    void Await(u64 id,u64 fence,std::function<Completion()> ready){
        if(!id)return;std::lock_guard lock(mutex);
        if(!valid)return;
        if(!fence||pending.size()>=64||seen.size()>=50000){valid=false;Add(id,"invalid-ready-fence-or-capture-capacity",fence);return;}
        if(!seen.insert(id).second){valid=false;Add(id,"duplicate-new-flip",fence);return;}
        pending.push_back({id,fence,std::move(ready)});++observed;
    }
};
inline u64 New(){return Enabled()?Capture::Get().New():0;}
inline void Record(u64 id,const char* phase,u64 fence=0){if(id&&Enabled())Capture::Get().Record(id,phase,fence);}
inline void Invalid(const char* reason){if(Enabled())Capture::Get().Invalid(reason);}
inline void Await(u64 id,u64 fence,std::function<Completion()> ready){if(id&&Enabled())Capture::Get().Await(id,fence,std::move(ready));}
}
