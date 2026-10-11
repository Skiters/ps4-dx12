// Bounded, opt-in instrumentation for the isolated Vulkan reference build.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <vector>
#include "reference_frames.h"
#ifdef _WIN32
#include <windows.h>
#endif
namespace ReferenceTrace {
using U64 = unsigned long long;
inline bool Enabled() {
    static const char* path = std::getenv("SHADPS4_REFERENCE_TRACE");
    if (!path || !*path) return false;
    static const char* marker = std::getenv("SHADPS4_REFERENCE_MARKER");
    if (!marker || !*marker) return true;
    static std::atomic<U64> checked{};
    static std::atomic<bool> armed{};
    const auto ms = U64(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    auto previous = checked.load(std::memory_order_relaxed);
    if (ms - previous >= 250 && checked.compare_exchange_strong(previous, ms)) {
        std::error_code error;
        armed = std::filesystem::exists(marker, error);
    }
    return armed.load(std::memory_order_relaxed);
}
inline U64 Now() { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline U64 Cpu() {
#ifdef _WIN32
    FILETIME a{},b{},k{},u{}; GetThreadTimes(GetCurrentThread(),&a,&b,&k,&u);
    return (((U64(k.dwHighDateTime)<<32)|k.dwLowDateTime)+((U64(u.dwHighDateTime)<<32)|u.dwLowDateTime))*100;
#else
    return 0;
#endif
}
inline U64 Tid() {
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}
inline std::atomic<U64> frame_hint{},next_frame{},next_id{};
inline thread_local U64 active_id{};
struct Event { const char* name; U64 id,parent,frame,tid,start,end,cpu,exclusive_wall,exclusive_cpu,a,b,c,d; };
struct Aggregate { std::atomic<const char*> name{};std::atomic<U64> calls{},wall{},cpu{},exclusive_wall{},exclusive_cpu{},sum_a{}; };
inline std::array<Aggregate,4096> totals{};
inline std::atomic<U64> aggregate_overflow{};
inline std::array<std::atomic<U64>,768> packet_counts{};
inline std::atomic<U64> invalid_packet_categories{};
inline void AggregateEvent(const Event& e){
    U64 hash=1469598103934665603ull;for(const char* p=e.name;*p;++p)hash=(hash^static_cast<unsigned char>(*p))*1099511628211ull;
    for(unsigned probe=0;probe<totals.size();++probe){auto& a=totals[(hash+probe)%totals.size()];const char* name=a.name.load();
        if(!name){const char* empty=nullptr;a.name.compare_exchange_strong(empty,e.name);name=a.name.load();}
        if(std::strcmp(name,e.name)==0){++a.calls;a.wall+=e.end-e.start;a.cpu+=e.cpu;a.exclusive_wall+=e.exclusive_wall;a.exclusive_cpu+=e.exclusive_cpu;a.sum_a+=e.a;return;}
    }++aggregate_overflow;
}
class Sink {
    std::mutex lock; std::vector<Event> events; std::atomic<U64> dropped{},filtered{};
    std::ofstream output; std::jthread worker; U64 written{}; const U64 cap=512ull<<20;
public:
    Sink() {
        events.reserve(32768);
        const char* path=std::getenv("SHADPS4_REFERENCE_TRACE");
        std::filesystem::create_directories(path);
        output.open(std::filesystem::path(path)/"internal-events.tsv");
        const auto monotonic = Now();
        const auto unix_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::ofstream manifest(std::filesystem::path(path)/"trace-clock.json");
        manifest<<"{\"cpu_monotonic_ns\":"<<monotonic<<",\"utc_unix_ns\":"<<unix_ns<<",\"gpu_cpu_calibrated\":false}";
        output<<"name\tid\tparent\tframe_hint\ttid\tstart_ns\tend_ns\tcpu_ns\texclusive_wall_ns\texclusive_cpu_ns\ta\tb\tc\td\n";
        worker=std::jthread([this](std::stop_token stop){std::vector<Event> batch;batch.reserve(32768);
            while(!stop.stop_requested()){std::this_thread::sleep_for(std::chrono::milliseconds(250));Drain(batch);}Drain(batch);});
    }
    ~Sink(){worker.request_stop();worker.join();output.flush();}
    void Drain(std::vector<Event>& batch){
        {std::lock_guard guard(lock);events.swap(batch);}
        for(const auto& e:batch){
            if(written>=cap){++dropped;continue;}
            output<<e.name<<'\t'<<e.id<<'\t'<<e.parent<<'\t'<<e.frame<<'\t'<<e.tid<<'\t'<<e.start<<'\t'<<e.end<<'\t'<<e.cpu<<'\t'<<e.exclusive_wall<<'\t'<<e.exclusive_cpu<<'\t'<<e.a<<'\t'<<e.b<<'\t'<<e.c<<'\t'<<e.d<<'\n';
            written=U64(output.tellp());
        }batch.clear();output.flush();
        const char* path=std::getenv("SHADPS4_REFERENCE_TRACE");
        std::ofstream status(std::filesystem::path(path)/"trace-status.json");
        status<<"{\"mode\":\"B\",\"reference_revision\":\"c7e065d1b415be16c23e260a21f1dd8bbfc4cb57\",\"dropped_events\":"<<dropped<<",\"filtered_short_events\":"<<filtered<<",\"aggregate_overflow\":"<<aggregate_overflow<<",\"written_bytes\":"<<written<<",\"clock\":\"steady_clock nanoseconds; GPU durations are separate\",\"frame_hint_is_not_causal_attribution\":true}";
        std::ofstream counters(std::filesystem::path(path)/"aggregate-counters.csv");
        counters<<"name,calls,wall_ns,cpu_ns,exclusive_wall_ns,exclusive_cpu_ns,sum_a\n";
        for(const auto& a:totals)if(const char* name=a.name.load())counters<<name<<','<<a.calls<<','<<a.wall<<','<<a.cpu<<','<<a.exclusive_wall<<','<<a.exclusive_cpu<<','<<a.sum_a<<'\n';
        std::ofstream packets(std::filesystem::path(path)/"pm4-opcode-counts.csv");
        packets<<"queue_kind,opcode,calls\n";
        for(unsigned i=0;i<packet_counts.size();++i)if(auto count=packet_counts[i].load())packets<<i/256<<','<<i%256<<','<<count<<'\n';
    }
    void Add(Event e){
        AggregateEvent(e);
        // Aggregates include every measured operation. Detailed short calls
        // are explicitly filtered by default; they used to exhaust the file
        // budget before gameplay and bury real frame completions.
        static const bool detail = std::getenv("SHADPS4_REFERENCE_DETAIL") != nullptr;
        if(!detail && e.end-e.start<100000 &&
           std::strncmp(e.name,"frame.",6) && std::strncmp(e.name,"gpu.",4) &&
           std::strncmp(e.name,"queue.",6)){++filtered;return;}
        if(!lock.try_lock()){++dropped;return;}
        if(events.size()<32768)events.push_back(e);else++dropped;
        lock.unlock();
    }
};
inline Sink& Output(){static Sink sink;return sink;}
inline void Emit(const char* name,U64 a=0,U64 b=0,U64 c=0,U64 d=0,U64 frame=0,U64 parent_override=0){
    if(!Enabled())return;const auto t=Now();Output().Add({name,++next_id,parent_override?parent_override:active_id,frame?frame:frame_hint.load(),Tid(),t,t,0,0,0,a,b,c,d});
}
inline void Packet(U64 opcode,unsigned queue_kind){
    if(!Enabled())return;
    if(opcode<256 && queue_kind<3)++packet_counts[queue_kind*256+opcode];else++invalid_packet_categories;
    // Keep opcode totals without a mutex, event ID, or disk record per packet.
}
struct Scope;
inline thread_local Scope* current{};
struct Scope {
    const char* name{};U64 id{},start{},cpu{},child_wall{},child_cpu{},a{},b{},c{},d{},frame{};Scope* parent{};
    explicit Scope(const char* label,U64 x=0,U64 y=0,U64 z=0,U64 w=0){
        if(!Enabled())return;Output();name=label;parent=current;id=++next_id;frame=frame_hint.load();a=x;b=y;c=z;d=w;start=Now();cpu=Cpu();current=this;active_id=id;
    }
    ~Scope(){if(!name)return;const auto end=Now(),used=Cpu()-cpu,wall=end-start;
        current=parent;active_id=parent?parent->id:0;if(parent){parent->child_wall+=wall;parent->child_cpu+=used;}
        Output().Add({name,id,parent?parent->id:0,frame,Tid(),start,end,used,wall>child_wall?wall-child_wall:0,used>child_cpu?used-child_cpu:0,a,b,c,d});
    }
};
inline U64 NewFrame(){
    const auto id=Common::GuestFrameTrace::Enabled()?Common::GuestFrameTrace::New():++next_frame;
    frame_hint=id;Emit("guest.frame.prepared",0,0,0,0,id);return id;
}
}
