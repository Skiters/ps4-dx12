// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <cstdint>
#include <array>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>
// Avoid leaking Windows macros into every HLE declaration through tls.h.
extern "C" __declspec(dllimport) unsigned long __stdcall GetCurrentThreadId();

namespace ApiTrace {
inline std::atomic<unsigned long long> next_id{};
inline std::atomic<unsigned long long> next_file{};
struct Stream {
    FILE* file{};
    std::mutex mutex;
    std::array<char,65536> buffer{};
    std::string record;
    explicit Stream(const std::string& path) {
        file=std::fopen(path.c_str(),"wb");
        if(!file)return;
        std::setvbuf(file,buffer.data(),_IOFBF,buffer.size());
        record.reserve(2048);
    }
    ~Stream(){if(file)std::fclose(file);}
};
class Session {
    std::mutex registry_mutex,wait_mutex;
    std::condition_variable wake;
    std::vector<std::weak_ptr<Stream>> streams;
    std::jthread flusher;
    std::atomic<bool> stopped{};
    std::atomic<unsigned long long> written_bytes{};
    unsigned long long byte_limit{512ULL*1024*1024};
public:
    std::string path;
    std::shared_ptr<Stream> root;
    bool immediate{};
    Session(){
        const auto* configured=std::getenv("SHADPS4_API_TRACE");
        if(!configured||!*configured)return;
        if(const auto* limit=std::getenv("SHADPS4_API_TRACE_MAX_BYTES")){
            char* end{};const auto requested=std::strtoull(limit,&end,10);
            if(end!=limit && *end=='\0' && requested>0 && requested<byte_limit)byte_limit=requested;
        }
        path=configured;root=std::make_shared<Stream>(path);
        if(!root->file){Stop("cannot open output");return;}
        streams.push_back(root);
        const auto* sync=std::getenv("SHADPS4_API_TRACE_SYNC");immediate=sync&&sync[0]=='1';
        if(!immediate)flusher=std::jthread([this](std::stop_token stop){
            std::unique_lock lock(wait_mutex);
            while(!stop.stop_requested()){
                wake.wait_for(lock,std::chrono::milliseconds(20),[&]{return stop.stop_requested();});
                lock.unlock();Flush();lock.lock();
            }
        });
    }
    ~Session(){if(flusher.joinable()){flusher.request_stop();wake.notify_all();flusher.join();}Flush();}
    bool Enabled() const {return root && root->file && !stopped.load(std::memory_order_relaxed);}
    void Stop(const char* reason){
        if(!stopped.exchange(true,std::memory_order_relaxed))
            std::fprintf(stderr,"API trace stopped: %s; game continues (trace may be incomplete)\n",reason);
    }
    bool Reserve(size_t bytes){
        if(!Enabled())return false;
        auto used=written_bytes.load(std::memory_order_relaxed);
        do{
            if(bytes>byte_limit-used){Stop("session size limit reached");return false;}
        }while(!written_bytes.compare_exchange_weak(used,used+bytes,std::memory_order_relaxed));
        return !stopped.load(std::memory_order_relaxed);
    }
    void Register(const std::shared_ptr<Stream>& stream){std::lock_guard lock(registry_mutex);streams.push_back(stream);}
    void Flush(){
        std::vector<std::shared_ptr<Stream>> snapshot;
        {std::lock_guard lock(registry_mutex);for(auto it=streams.begin();it!=streams.end();){
            if(auto stream=it->lock()){snapshot.push_back(std::move(stream));++it;}else it=streams.erase(it);
        }}
        for(auto& stream:snapshot){std::lock_guard lock(stream->mutex);
            if(stream->file && std::fflush(stream->file))Stop("flush failed");}
    }
};
inline Session& GetSession(){static Session session;return session;}
inline bool Enabled(){return GetSession().Enabled();}
inline void FlushAll(){GetSession().Flush();}
inline Stream& ThreadSink(){
    // No sampling: every record enters a bounded 64KiB stream buffer. Writes
    // are batched and periodically flushed for live readers. Each guest thread
    // owns its file; there is no cross-thread API write lock.
    static thread_local auto stream=[] {
        auto& session=GetSession();const auto index=next_file.fetch_add(1,std::memory_order_relaxed);
        if(!index)return session.root;
        auto stream=std::make_shared<Stream>(session.path+".thread-"+
            std::to_string(GetCurrentThreadId())+"-"+std::to_string(index)+".jsonl");
        if(!stream->file)session.Stop("cannot open thread output");
        session.Register(stream);return stream;
    }();
    return *stream;
}
inline void AppendEscaped(std::string& out,const char* text) {
    for (const unsigned char* p=reinterpret_cast<const unsigned char*>(text?text:"unknown");*p;++p) {
        const unsigned char c=*p;
        if (c == '\\' || c == '"') out += '\\';
        if (c < 32) { char escaped[8]; std::snprintf(escaped,sizeof(escaped),"\\u%04x",c);out += escaped; }
        else out += char(c);
    }
}
template<class T> std::string Value(const T& value) {
    if constexpr (std::is_pointer_v<T>) {
        char text[32]; std::snprintf(text,sizeof(text),"0x%llx",static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(value)));return text;
    } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
        char text[32];std::snprintf(text,sizeof(text),"0x%llx",static_cast<unsigned long long>(value));return text;
    } else if constexpr (std::is_floating_point_v<T>) {
        char text[64];std::snprintf(text,sizeof(text),"%.17g",static_cast<double>(value));return text;
    } else if constexpr (std::is_trivially_copyable_v<T>) {
        std::string text="bytes:";const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof(T);++i){char hex[3];std::snprintf(hex,sizeof(hex),"%02x",bytes[i]);text+=hex;}
        return text;
    } else return "object";
}
template<class... Args> std::string Arguments(const Args&... args) {
    std::string text;((text += (text.empty()?"":",") + Value(args)),...);return text;
}
class Scope {
    unsigned long long id{};
    const char* domain;
    const char* name;
    std::chrono::steady_clock::time_point started;
    bool returned{};
    void Write(const char* phase,const std::string& value) {
        auto& session=GetSession();
        if(!session.Enabled())return;
        if(!id)id=next_id.fetch_add(1,std::memory_order_relaxed)+1;
        const auto now=std::chrono::steady_clock::now();
        const auto stamp=std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        const auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(now-started).count();
        auto& stream=ThreadSink();std::lock_guard lock(stream.mutex);
        if(!stream.file || !session.Enabled())return;
        // Reuse the thread's record storage. Full tracing must not allocate
        // several temporary strings for every hot GPU/PS4 API call.
        char header[256];
        const int length=std::snprintf(header,sizeof(header),
            "{\"id\":%llu,\"thread\":%lu,\"time_us\":%lld,\"elapsed_us\":%lld,\"domain\":\"%s\",\"phase\":\"%s\",\"name\":\"",
            id,GetCurrentThreadId(),stamp,elapsed,domain,phase);
        if(length<0||size_t(length)>=sizeof(header)){session.Stop("header overflow");return;}
        auto& record=stream.record;record.assign(header,size_t(length));
        AppendEscaped(record,name);record+="\",\"value\":\"";
        AppendEscaped(record,value.c_str());record+="\"}\n";
        if(!session.Reserve(record.size()))return;
        if(std::fwrite(record.data(),1,record.size(),stream.file)!=record.size() ||
            (session.immediate && std::fflush(stream.file)!=0))session.Stop("write failed");
    }
public:
    Scope(const char* domain_,const char* name_,const std::string& args):domain(domain_),name(name_),started(std::chrono::steady_clock::now()){Write("enter",args);}
    void Return(const std::string& value){Write("return",value);returned=true;}
    ~Scope(){if(!returned)Write("exception","C++ exception unwind");}
};
template<class F,class... Args> auto Call(const char* domain,const char* name,F&& fn,const Args&... args)->std::invoke_result_t<F> {
    if(!Enabled())return std::forward<F>(fn)();
    Scope trace(domain,name,Arguments(args...));
    if constexpr(std::is_void_v<std::invoke_result_t<F>>){std::forward<F>(fn)();trace.Return("void");}
    else {decltype(auto) result=std::forward<F>(fn)();trace.Return(Value(result));return result;}
}
}
#define DX12_API_CALL(expression) ApiTrace::Call("DX12",__FILE__ ":" #expression,[&]()->decltype(auto){return (expression);})
