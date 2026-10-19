// SPDX-License-Identifier: GPL-2.0-or-later
#include "video_core/renderer_d3d12/cpu_buffer_snapshot.h"
#include "video_core/renderer_d3d12/host_copy_queue.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>

using namespace NativeDX12;
static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct ReleaseGate {
    std::promise<void>& gate;bool done{};
    void Release(){if(!done){gate.set_value();done=true;}}
    ~ReleaseGate(){Release();}
};
int main(){try{
    {
        CpuBufferSnapshot snapshot;
        const std::byte initial[]{std::byte{0x11},std::byte{0x22},std::byte{0x33},std::byte{0x44}};
        snapshot.Assign(initial);auto old=snapshot.Share();
        snapshot.Assign(std::span(snapshot.data()+1,2));
        Require(snapshot.size()==2&&snapshot.data()[0]==std::byte{0x22}&&snapshot.data()[1]==std::byte{0x33}&&
            old->size()==4&&(*old)[0]==std::byte{0x11}&&(*old)[3]==std::byte{0x44},
            "Snapshot subspan assignment invalidated its own input/immutable version");
        auto shared=snapshot.Share();snapshot.Assign(initial);
        Require(shared->size()==2&&(*shared)[0]==std::byte{0x22}&&snapshot.size()==4,
            "New texture snapshot overwrote an existing immutable lease");
        auto moved=std::move(snapshot);
        Require(moved.size()==4&&moved.data()[3]==std::byte{0x44}&&snapshot.empty(),
            "Publishing a snapshot moved its payload out of immutable shared storage");
        std::puts("PASS: snapshot assign/self-subspan/version and owner publication");
    }
    {
        std::promise<void> gate,started;auto allow=gate.get_future().share();auto running=started.get_future();
        HostCopyQueue queue;ReleaseGate release{gate};
        CpuBufferSnapshot source;source.resize(4096);
        for(size_t i=0;i<source.size();++i)source.WriteData()[i]=std::byte((i*37+19)&255);
        auto output=std::make_shared<std::vector<std::byte>>(4096,std::byte{0});
        std::weak_ptr<std::vector<std::byte>> weak_output=output;
        std::weak_ptr<const std::vector<std::byte>> weak_source=source.Share();
        queue.Enqueue([snapshot=source.Share(),output,allow,&started]{
            started.set_value();allow.wait();std::memcpy(output->data(),snapshot->data(),snapshot->size());
        },source.capacity());
        Require(running.wait_for(std::chrono::seconds(5))==std::future_status::ready,"Copy worker did not start");
        // Real writes and pool-style clear/reuse must detach the worker's bytes.
        source.WriteData()[0]=std::byte{0xff};source.clear();source.resize(4096);
        std::memset(source.WriteData(),0xaa,source.size());
        Require(!weak_source.expired(),"Source version died while copy was gated");
        output.reset();Require(!weak_output.expired(),"Destination lease died while copy was gated");
        auto retained=weak_output.lock();release.Release();queue.Wait();
        for(size_t i=0;i<retained->size();++i)
            Require((*retained)[i]==std::byte((i*37+19)&255),"Copy observed a later snapshot/pooled version");
        Require(weak_source.expired(),"Wait returned before releasing the old source lease");
        retained.reset();Require(weak_output.expired(),"Wait retained its completed destination lease");
        queue.Wait();
        std::puts("PASS: immutable source, detached writes/pool reuse, gated destination and drained lifetime");
    }
    {
        struct CopyFailure final:std::exception {};
        std::promise<void> gate;auto allow=gate.get_future().share();
        std::atomic<unsigned> following{};HostCopyQueue queue;ReleaseGate release{gate};
        queue.Enqueue([allow]{allow.wait();throw CopyFailure{};},1);
        queue.Enqueue([&]{++following;},1);release.Release();
        bool threw{};try{queue.Wait();}catch(const CopyFailure&){threw=true;}
        Require(threw&&following==1,"Copy failure lost its exception or abandoned another issued copy");
        bool rejected{};try{queue.Enqueue([]{},1);}catch(const CopyFailure&){rejected=true;}
        Require(rejected,"Failed copy epoch accepted later work");
        std::puts("PASS: failed epoch drains issued work and reports/rejects without suppression");
    }
    {
        HostCopyQueue queue;unsigned inline_copy{};
        queue.Enqueue([&]{++inline_copy;},64*1024*1024+1);
        Require(inline_copy==1,"Oversize retained source escaped the bounded synchronous route");
        queue.Wait();bool rejected{};try{queue.Enqueue({},1);}catch(const std::invalid_argument&){rejected=true;}
        Require(rejected,"Empty copy operation was accepted");
        std::puts("PASS: bounded oversize route and invalid-operation negative");
    }
    return 0;
}catch(const std::exception& error){std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
