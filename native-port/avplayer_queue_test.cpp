// SPDX-License-Identifier: GPL-2.0-or-later
#include "core/libraries/avplayer/avplayer_common.h"
#include <atomic>
#include <array>
#include <barrier>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using Libraries::AvPlayer::AvPlayerQueue;
struct Frame {
    std::unique_ptr<unsigned> value;
    explicit Frame(unsigned id):value(std::make_unique<unsigned>(id)){}
    Frame(Frame&&)=default;
    Frame& operator=(Frame&&)=default;
};
int main(){
    constexpr unsigned count=30000;
    AvPlayerQueue<Frame> queue;
#ifndef AVPLAYER_BASELINE_QUEUE
    queue.Push(Frame(7));
    if(queue.PopIf([](const Frame& f){return *f.value<=6;})||queue.Size()!=1)return 1;
    const auto ready=queue.PopIf([](const Frame& f){return *f.value<=7;});
    if(!ready||!ready->value||*ready->value!=7||queue.Pop())return 2;
#endif
    std::vector<std::atomic_uint> seen(count+1);
    std::atomic_bool done{},bad{};
    std::atomic_uint consumed{};
    std::barrier start(5);
    std::jthread producer([&]{start.arrive_and_wait();for(unsigned i=1;i<=count;++i){queue.Push(Frame(i));if(i%16==0)std::this_thread::yield();}done=true;});
    std::array<std::jthread,3> readers;
    for(auto& reader:readers)reader=std::jthread([&]{
        start.arrive_and_wait();
        do{if(auto item=queue.Pop()){
            if(!item->value||*item->value==0||*item->value>count){bad=true;break;}
            if(seen[*item->value].fetch_add(1))bad=true;
            ++consumed;
        }else std::this_thread::yield();}while(!done||queue.Size());
    });
    std::jthread clearer([&]{start.arrive_and_wait();while(!done){queue.Clear();std::this_thread::yield();}});
    producer.join();clearer.join();for(auto& reader:readers)reader.join();
    if(bad||queue.Pop())return 3;
    // Exercise an uninterrupted FIFO too; Clear legitimately discards frames.
    for(unsigned i=1;i<=128;++i)queue.Push(Frame(i));
    for(unsigned i=1;i<=128;++i){const auto item=queue.Pop();if(!item||!item->value||*item->value!=i)return 4;}
    if(queue.Pop()||queue.Size())return 5;
    std::printf("PASS: future frame retained; FIFO and concurrent consumers/Clear preserved ownership (%u consumed)\n",consumed.load());
}
