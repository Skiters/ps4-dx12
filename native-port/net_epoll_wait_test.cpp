// SPDX-License-Identifier: GPL-2.0-or-later
#include <winsock2.h>
#include "core/net/host_net.h"
#include <chrono>
#include <cstdio>
#include <thread>
#include <atomic>

using namespace Core::Net::Host;

static int Run(){
    HostEpoll epoll;if(!epoll.Valid())return 2;
    EpollEvent output[4]{};
    auto timed=[&](int us){
        auto start=std::chrono::steady_clock::now();
        const Deadline deadline=us<0?Deadline{}:Deadline{Clock::now()+std::chrono::microseconds(us)};
        const auto result=epoll.Wait(output,deadline);
        auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("wait=%d result=%d wake=%d error=%d elapsed_us=%lld\n",us,result.count,result.woken,int(result.error),elapsed);
        return std::pair{result,elapsed};
    };
    auto empty=timed(33000);if(empty.first.count||empty.first.woken||empty.first.error!=Error::Ok||empty.second<30000)return 3;
    auto tiny=timed(999);if(tiny.first.count||tiny.first.error!=Error::Ok||tiny.second<500)return 4;
    if(timed(0).first.count)return 5;
    epoll.Wake();
    auto resolver=timed(5000000);if(resolver.first.count||!resolver.first.woken||resolver.first.error!=Error::Ok||resolver.second>1000000)return 6;
    if(!timed(0).first.woken)return 14;
    epoll.DrainWake();if(timed(0).first.woken)return 15;
    std::atomic<bool> pending{};
    std::jthread resolver_thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(30));pending=true;epoll.Wake();});
    auto resolver_start=std::chrono::steady_clock::now();
    auto wake=timed(-1).first;if(wake.count||!wake.woken||wake.error!=Error::Ok)return 12;
    if(!pending || std::chrono::steady_clock::now()-resolver_start>std::chrono::seconds(1))return 13;
    resolver_thread.join();epoll.DrainWake();
    struct SocketLease{SOCKET socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);~SocketLease(){if(socket!=INVALID_SOCKET)closesocket(socket);}} socket_lease;
    const auto socket=socket_lease.socket;if(socket==INVALID_SOCKET)return 7;
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(socket,reinterpret_cast<sockaddr*>(&address),sizeof(address)))return 8;
    int length=sizeof(address);if(getsockname(socket,reinterpret_cast<sockaddr*>(&address),&length))return 9;
    if(epoll.Add(socket,123,EvIn,0)!=Error::Ok)return 10;
    std::jthread sender([&]{std::this_thread::sleep_for(std::chrono::milliseconds(30));
        sendto(socket,"x",1,0,reinterpret_cast<sockaddr*>(&address),sizeof(address));});
    auto ready=timed(-1);
    if(ready.first.count!=1||ready.first.woken||ready.first.error!=Error::Ok||output[0].tag!=123||!(output[0].events&EvIn))return 11;
    sender.join();
    std::puts("PASS: empty epoll timeout, sub-ms timeout, resolver readiness and socket wakeup");
    return 0;
}
int main(){
    if(!Initialize())return 1;
    const int result=Run();Shutdown();return result;
}
