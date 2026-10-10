// SPDX-License-Identifier: GPL-2.0-or-later
#include <winsock2.h>
#include "core/libraries/network/net_epoll_wait.h"
#include <chrono>
#include <cstdio>
#include <thread>
#include <atomic>

int main(){
    WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data))return 1;
    auto epoll=epoll_create1(0);if(!epoll)return 2;
    epoll_event output[4]{};
    auto timed=[&](int us,bool pending){
        auto start=std::chrono::steady_clock::now();
        int result=Libraries::Net::WaitNativeEpoll(epoll,output,4,us,pending);
        auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        std::printf("wait=%d pending=%d result=%d elapsed_us=%lld\n",us,pending,result,elapsed);
        return std::pair{result,elapsed};
    };
    auto empty=timed(33000,false);if(empty.first!=0||empty.second<30000)return 3;
    auto tiny=timed(999,false);if(tiny.first!=0||tiny.second<500)return 4;
    if(timed(0,false).first!=0)return 5;
    auto resolver=timed(5000000,true);if(resolver.first!=0||resolver.second>1000000)return 6;
    std::atomic<bool> pending{};
    std::jthread resolver_thread([&]{std::this_thread::sleep_for(std::chrono::milliseconds(30));pending=true;});
    auto resolver_start=std::chrono::steady_clock::now();
    if(Libraries::Net::WaitEpollWithResolvers(epoll,output,4,-1,[&]{return pending.load();})!=0)return 12;
    if(!pending || std::chrono::steady_clock::now()-resolver_start>std::chrono::seconds(1))return 13;
    resolver_thread.join();
    SOCKET socket=::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);if(socket==INVALID_SOCKET)return 7;
    sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(socket,reinterpret_cast<sockaddr*>(&address),sizeof(address)))return 8;
    int length=sizeof(address);if(getsockname(socket,reinterpret_cast<sockaddr*>(&address),&length))return 9;
    epoll_event registration{};registration.events=EPOLLIN;registration.data.u64=123;
    if(epoll_ctl(epoll,EPOLL_CTL_ADD,socket,&registration))return 10;
    std::jthread sender([&]{std::this_thread::sleep_for(std::chrono::milliseconds(30));
        sendto(socket,"x",1,0,reinterpret_cast<sockaddr*>(&address),sizeof(address));});
    auto ready=timed(-1,false);
    if(ready.first!=1||output[0].data.u64!=123||!(output[0].events&EPOLLIN))return 11;
    sender.join();epoll_close(epoll);closesocket(socket);WSACleanup();
    std::puts("PASS: empty epoll timeout, sub-ms timeout, resolver readiness and socket wakeup");
}
