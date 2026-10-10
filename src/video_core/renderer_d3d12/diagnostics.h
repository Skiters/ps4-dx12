// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <mutex>
#include <thread>
#include <windows.h>

namespace NativeDX12::Diagnostics {
enum class Kind { Draw, Dispatch, Reconcile, Readback, Upload, PrepareFrame, Present, FenceWait, Resources, BufferViews, Deferred, Count };
inline constexpr const char* Names[]{"draw", "dispatch", "buffer-reconcile", "image-readback",
    "image-upload", "frame-prepare", "present", "fence-wait", "resource-bindings", "buffer-views", "deferred-submit"};
using Clock = std::chrono::steady_clock;
inline bool Enabled() {
    static const bool enabled = [] { const auto* value = std::getenv("SHADPS4_NATIVE_DIAGNOSTICS");
        if(value)return value[0]=='1';
        std::error_code error;
        return std::filesystem::exists("native-diagnostics.enabled",error); }();
    return enabled;
}
struct Active {
    DWORD thread{}; Kind kind{}; const char* stage{};
    Clock::time_point start{}, stage_start{}; char detail[160]{};
};
struct Totals { unsigned long long calls{}, failed{}; double ms{}, max_ms{}; };
struct StageTotals {
    Kind kind{}; const char* name{}; unsigned long long calls{}; double ms{}, max_ms{};
    std::array<double, 512> samples{}; size_t sample_count{}, sample_cursor{};
};
struct Counter { const char* name{}; unsigned long long value{}; };
// Opt-in, bounded, aggregate diagnostics. No images, command history or persistent
// profiler cache. The normal renderer never constructs the worker or takes its lock.
class Hub {
public:
    std::mutex mutex;
    std::condition_variable wake;
    std::array<Active, 16> active{};
    std::array<Totals, size_t(Kind::Count)> totals{};
    std::array<StageTotals, 96> stages{};
    std::array<Counter, 64> counters{};
    unsigned lines{};
    const Clock::time_point started = Clock::now();
    FILE* output = [] {
        char path[128];snprintf(path,sizeof(path),"user/log/native-render-%lu.txt",GetCurrentProcessId());
        auto* file=fopen(path,"wb");return file?file:stderr;
    }();
    FILE* performance = [] {
        char path[128];snprintf(path,sizeof(path),"user/log/native-performance-%lu.csv",GetCurrentProcessId());
        return fopen(path,"wb");
    }();
    unsigned performance_rows{};
    std::jthread worker;
    Hub() : worker([this](std::stop_token stop) {
        std::unique_lock lock(mutex);
        while (!stop.stop_requested()) {
            wake.wait_for(lock, std::chrono::seconds(2), [&] { return stop.stop_requested(); });
            if (stop.stop_requested()) break;
            const auto now = Clock::now();
            if(performance&&performance_rows++<2048){
                const auto& frames=totals[size_t(Kind::PrepareFrame)];
                const auto& draws=totals[size_t(Kind::Draw)];const auto& uploads=totals[size_t(Kind::Upload)];
                const auto& compute=totals[size_t(Kind::Dispatch)];const auto& waits=totals[size_t(Kind::FenceWait)];
                fprintf(performance,"%.3f,%llu,%.3f,%llu,%.3f,%llu,%.3f,%llu,%.3f,%llu,%.3f\n",
                    Seconds(now),frames.calls,frames.ms,draws.calls,draws.ms,uploads.calls,uploads.ms,
                    compute.calls,compute.ms,waits.calls,waits.ms);fflush(performance);
            }
            for (size_t i = 0; i < totals.size(); ++i) {
                const auto t = totals[i]; totals[i] = {};
                if (t.calls && lines++ < 8192)
                    fprintf(output, "DX12 diag t=%.3f summary=%s calls=%llu failed=%llu total_ms=%.3f avg_ms=%.3f max_ms=%.3f (inclusive)\n",
                        Seconds(now), Names[i], t.calls, t.failed, t.ms, t.ms/t.calls, t.max_ms);
            }
            for (auto& s : stages) {
                if (!s.calls) continue;
                auto samples=s.samples;const auto count=std::min(samples.size(),s.sample_count);
                std::sort(samples.begin(),samples.begin()+count);
                const auto percentile=[&](double p){return samples[std::min(count-1,size_t((count-1)*p))];};
                if(lines++<8192)fprintf(output,
                    "DX12 diag t=%.3f stage-summary op=%s stage=%s calls=%llu total_ms=%.3f avg_ms=%.3f max_ms=%.3f samples=%zu p50_ms=%.3f p95_ms=%.3f p99_ms=%.3f (cpu-wall)\n",
                    Seconds(now),Names[size_t(s.kind)],s.name,s.calls,s.ms,s.ms/s.calls,s.max_ms,count,
                    percentile(.50),percentile(.95),percentile(.99));
                s.calls=0;s.ms=0;s.max_ms=0;s.sample_count=0;s.sample_cursor=0;
            }
            for(auto& c:counters)if(c.value){
                if(lines++<8192)fprintf(output,"DX12 diag t=%.3f counter=%s value=%llu\n",
                    Seconds(now),c.name,c.value);c.value=0;
            }
            for (const auto& a : active) if (a.thread && a.stage && lines++ < 8192)
                fprintf(output, "DX12 diag t=%.3f active tid=%lu op=%s stage=%s age_ms=%.3f stage_ms=%.3f %s\n",
                    Seconds(now), a.thread, Names[size_t(a.kind)], a.stage,
                    Milliseconds(now-a.start), Milliseconds(now-a.stage_start), a.detail);
            fflush(output);
        }
    }) { fprintf(output, "DX12 diagnostics enabled: 2s summaries, 100ms slow operations, 8192-record limit; no frame capture\n");
        if(performance)fprintf(performance,"time,frames,frame_prepare_ms,draws,draw_ms,uploads,upload_ms,dispatches,dispatch_ms,waits,wait_ms\n"); }
    ~Hub() { worker.request_stop(); wake.notify_all(); if(worker.joinable())worker.join();if(output!=stderr)fclose(output);if(performance)fclose(performance); }
    static Hub& Get() { static Hub hub; return hub; }
    static double Milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
    double Seconds(Clock::time_point t) const { return std::chrono::duration<double>(t-started).count(); }
    void RecordStage(Kind kind,const char* name,double elapsed){
        StageTotals* target{};
        for(auto& stage:stages)if(stage.name&&stage.kind==kind&&!std::strcmp(stage.name,name)){target=&stage;break;}
        if(!target)for(auto& stage:stages)if(!stage.name){stage.kind=kind;stage.name=name;target=&stage;break;}
        if(!target)return;++target->calls;target->ms+=elapsed;target->max_ms=std::max(target->max_ms,elapsed);
        target->samples[target->sample_cursor++%target->samples.size()]=elapsed;
        target->sample_count=std::min(target->sample_count+1,target->samples.size());
    }
};
inline void Count(const char* name,unsigned long long value=1){
    if(!Enabled()||!value)return;auto& hub=Hub::Get();std::lock_guard lock(hub.mutex);
    Counter* target{};for(auto& counter:hub.counters)if(counter.name&&!std::strcmp(counter.name,name)){target=&counter;break;}
    if(!target)for(auto& counter:hub.counters)if(!counter.name){counter.name=name;target=&counter;break;}
    if(target)target->value+=value;
}
class Scope {
    Hub* hub_{}; size_t slot_{}; Active previous_{}, current_{}; int exceptions_{};
public:
    explicit Scope(Kind kind, const char* stage) {
        if (!Enabled()) return;
        hub_ = &Hub::Get(); std::lock_guard lock(hub_->mutex);
        const auto tid = GetCurrentThreadId(); slot_ = hub_->active.size();
        for (size_t i=0; i<hub_->active.size(); ++i) if(hub_->active[i].thread==tid) {slot_=i;break;}
        if(slot_==hub_->active.size()) for(size_t i=0;i<hub_->active.size();++i)
            if(!hub_->active[i].thread){slot_=i;break;}
        if(slot_==hub_->active.size()){hub_=nullptr;return;}
        previous_=hub_->active[slot_]; current_.thread=tid;current_.kind=kind;current_.stage=stage;
        current_.start=current_.stage_start=Clock::now();exceptions_=std::uncaught_exceptions();
        hub_->active[slot_]=current_;
    }
    void Stage(const char* stage) {
        if(!hub_)return; std::lock_guard lock(hub_->mutex);
        const auto now=Clock::now();const auto elapsed=Hub::Milliseconds(now-current_.stage_start);
        hub_->RecordStage(current_.kind,current_.stage,elapsed);
        if(elapsed>=100&&hub_->lines++<8192)fprintf(hub_->output,
            "DX12 diag t=%.3f slow-stage tid=%lu op=%s stage=%s ms=%.3f %s\n",
            hub_->Seconds(now),current_.thread,Names[size_t(current_.kind)],current_.stage,elapsed,current_.detail);
        current_.stage=stage;current_.stage_start=now;hub_->active[slot_]=current_;
    }
    template<typename... Args> void Detail(const char* format, Args... args) {
        if(!hub_)return;std::lock_guard lock(hub_->mutex);
        snprintf(current_.detail,sizeof(current_.detail),format,args...);hub_->active[slot_]=current_;
    }
    ~Scope() {
        if(!hub_)return; std::lock_guard lock(hub_->mutex);
        const auto now=Clock::now();const auto elapsed=Hub::Milliseconds(now-current_.start);
        hub_->RecordStage(current_.kind,current_.stage,Hub::Milliseconds(now-current_.stage_start));
        const bool failed=std::uncaught_exceptions()>exceptions_;
        auto& t=hub_->totals[size_t(current_.kind)];++t.calls;t.failed+=failed;t.ms+=elapsed;
        if(elapsed>t.max_ms)t.max_ms=elapsed;
        if((failed||elapsed>=100)&&hub_->lines++<8192) {
            fprintf(hub_->output,"DX12 diag t=%.3f completed tid=%lu op=%s stage=%s ms=%.3f failed=%u %s\n",
                hub_->Seconds(Clock::now()),current_.thread,Names[size_t(current_.kind)],current_.stage,
                elapsed,unsigned(failed),current_.detail);fflush(hub_->output);
        }
        hub_->active[slot_]=previous_;
    }
    Scope(const Scope&)=delete;
};
}
