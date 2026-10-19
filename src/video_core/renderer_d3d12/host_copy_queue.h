// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace NativeDX12 {
// Deferred host work for already-resolved, immutable upload sources. Like the
// native Windows port's pre-submit copy epoch, Wait drains all issued copies
// before GPU execution. Tasks must not access guest VAs, renderer locks or GPU
// queues. The waiting submit thread helps execute work instead of spinning.
class HostCopyQueue {
    struct Task {std::function<void()> copy;size_t bytes;};
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Task> tasks_;
    std::vector<std::thread> workers_;
    size_t pending_{},bytes_{};
    bool stopping_{};
    std::exception_ptr failure_;
    static constexpr size_t budget_=64*1024*1024;
    void Run(Task task)noexcept{
        std::exception_ptr failure;
        try{task.copy();}catch(...){failure=std::current_exception();}
        // Release source/destination leases BEFORE reporting completion.
        task.copy={};
        {
            std::lock_guard lock(mutex_);
            if(failure&&!failure_)failure_=failure;
            --pending_;bytes_-=task.bytes;
        }
        ready_.notify_all();
    }
    void Worker(){
        for(;;){Task task;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock,[&]{return stopping_||!tasks_.empty();});
                if(tasks_.empty()){if(stopping_)return;continue;}
                task=std::move(tasks_.front());tasks_.pop_front();
            }
            Run(std::move(task));
        }
    }
    void Start(){
        if(!workers_.empty())return;
        const unsigned count=std::clamp(std::thread::hardware_concurrency()/4,1u,4u);
        try{for(unsigned i=0;i<count;++i)workers_.emplace_back([this]{Worker();});}
        catch(...){
            {std::lock_guard lock(mutex_);stopping_=true;}
            ready_.notify_all();
            for(auto& worker:workers_)worker.join();
            workers_.clear();stopping_=false;throw;
        }
    }
public:
    HostCopyQueue()=default;
    HostCopyQueue(const HostCopyQueue&)=delete;
    HostCopyQueue& operator=(const HostCopyQueue&)=delete;
    ~HostCopyQueue(){
        try{Wait();}catch(...){}
        {std::lock_guard lock(mutex_);stopping_=true;}
        ready_.notify_all();for(auto& worker:workers_)worker.join();
    }
    // Enqueue/Wait have one producer (the renderer execution lock). Workers
    // only consume. Cost includes retained snapshot storage, not just the slice.
    void Enqueue(std::function<void()> copy,size_t bytes){
        if(!copy||!bytes)throw std::invalid_argument("Invalid deferred host copy");
        {
            std::lock_guard lock(mutex_);
            if(failure_)std::rethrow_exception(failure_);
        }
        if(bytes>budget_){Wait();copy();return;}
        bool drain;
        {std::lock_guard lock(mutex_);drain=bytes_>budget_-bytes;}
        if(drain)Wait();
        Start();
        {
            std::lock_guard lock(mutex_);
            if(failure_)std::rethrow_exception(failure_);
            tasks_.push_back({std::move(copy),bytes});++pending_;bytes_+=bytes;
        }
        ready_.notify_one();
    }
    void Wait(){
        for(;;){Task task;
            {
                std::unique_lock lock(mutex_);
                ready_.wait(lock,[&]{return !pending_||!tasks_.empty();});
                if(!pending_){if(failure_)std::rethrow_exception(failure_);return;}
                task=std::move(tasks_.front());tasks_.pop_front();
            }
            Run(std::move(task));
        }
    }
};
} // namespace NativeDX12
