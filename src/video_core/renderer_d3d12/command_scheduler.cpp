#include "command_scheduler.h"
#include "diagnostics.h"
#include "device_diagnostics.h"
#include <algorithm>
#include <utility>
#include <cstring>

namespace NativeDX12 {

void CommandScheduler::QueueHostCopy(ID3D12Resource* destination,void* mapped,
    std::shared_ptr<const std::vector<std::byte>> source,size_t offset,size_t bytes,bool unmap){
    if(submit_failure_)std::rethrow_exception(submit_failure_);
    if(!destination||!mapped||!source||!bytes||offset>source->size()||bytes>source->size()-offset)
        throw std::invalid_argument("Invalid immutable host upload");
    const auto cost=source->capacity();
    host_copies_.Enqueue([lease=ComPtr<ID3D12Resource>(destination),mapped,source=std::move(source),offset,bytes,unmap]{
        std::memcpy(mapped,source->data()+offset,bytes);
        if(unmap)DX12_API_CALL(lease->Unmap(0,nullptr));
    },cost);
    Diagnostics::Count("buffer-upload.host-deferred.bytes",bytes);
}

CommandScheduler::CommandScheduler(ID3D12Device* device) : device_(device), timeline_(device), copy_timeline_(device) {
    if (!device) throw std::invalid_argument("null D3D12 scheduler device");
    D3D12_COMMAND_QUEUE_DESC description{};
    description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(device_->CreateCommandQueue(&description, IID_PPV_ARGS(&direct_queue_))));
    direct_queue_->SetName(L"shadPS4 DX12 guest DIRECT queue");
    description.Type=D3D12_COMMAND_LIST_TYPE_COPY;
    Check(DX12_API_CALL(device_->CreateCommandQueue(&description,IID_PPV_ARGS(&copy_queue_))));
    copy_queue_->SetName(L"shadPS4 DX12 guest COPY queue");
    contexts_.reserve(8);
    copy_contexts_.reserve(8);
    active_ = AcquireContext();
    recording_ = true;
    priority_worker_ = std::jthread([this](std::stop_token token) { PriorityWorker(token); });
}

void CommandScheduler::SubmitCopy(const std::function<void(ID3D12GraphicsCommandList*)>& record){
    if(submit_failure_)std::rethrow_exception(submit_failure_);
    if(!record)throw std::invalid_argument("empty copy submission");
    if(!copy_recording_){copy_active_=AcquireCopyContext();copy_recording_=true;}
    auto& context=copy_contexts_[copy_active_];
    record(context.list.Get());
    Diagnostics::Count("gpu-copy.records");
    // The pending copy list is submitted immediately before the next direct
    // list. Its queue wait preserves the same upload-before-use dependency
    // without one ExecuteCommandLists/Signal pair per buffer slice.
}

u64 CommandScheduler::FlushCopy(){
    if(!copy_recording_)return copy_submitted_;
    auto& context=copy_contexts_[copy_active_];
    Check(DX12_API_CALL(context.list->Close()));
    copy_submitted_=copy_timeline_.Submit(copy_queue_.Get(),context.list.Get());
    context.fence=copy_submitted_;
    context.closed=true;
    Check(DX12_API_CALL(direct_queue_->Wait(copy_timeline_.Fence(),copy_submitted_)));
    copy_recording_=false;Diagnostics::Count("gpu-submit.copy");
    return copy_submitted_;
}

size_t CommandScheduler::AcquireCopyContext()try{
    const auto completed=copy_timeline_.Completed();
    for(size_t index=0;index<copy_contexts_.size();++index){
        auto& context=copy_contexts_[index];
        if(context.fence>completed)continue;
        Check(DX12_API_CALL(context.allocator->Reset()));
        Check(DX12_API_CALL(context.list->Reset(context.allocator.Get(),nullptr)));
        context.closed=false;
        return index;
    }
    if(copy_contexts_.size()<8){
        Context context;
        Check(DX12_API_CALL(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY,
            IID_PPV_ARGS(&context.allocator))));
        Check(DX12_API_CALL(device_->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COPY,
            context.allocator.Get(),nullptr,IID_PPV_ARGS(&context.list))));
        wchar_t name[96];swprintf_s(name,L"shadPS4 DX12 COPY allocator %zu",copy_contexts_.size());context.allocator->SetName(name);
        swprintf_s(name,L"shadPS4 DX12 COPY list %zu",copy_contexts_.size());context.list->SetName(name);
        copy_contexts_.push_back(std::move(context));
        return copy_contexts_.size()-1;
    }
    const auto oldest=std::min_element(copy_contexts_.begin(),copy_contexts_.end(),
        [](const Context& left,const Context& right){return left.fence<right.fence;});
    const auto index=size_t(oldest-copy_contexts_.begin());
    copy_timeline_.Wait(oldest->fence);
    Check(DX12_API_CALL(oldest->allocator->Reset()));
    Check(DX12_API_CALL(oldest->list->Reset(oldest->allocator.Get(),nullptr)));
    oldest->closed=false;
    return index;
}catch(...){ReportDeviceRemoval(device_.Get());throw;}

CommandScheduler::~CommandScheduler() {
    try{WaitHostCopies();}catch(...){}
    try {
        if(submit_failure_){
            // A failed Signal may follow successful ExecuteCommandLists. Drain
            // queues with fresh signals, without executing either list again.
            const auto copy=copy_timeline_.Signal(copy_queue_.Get());
            Check(DX12_API_CALL(direct_queue_->Wait(copy_timeline_.Fence(),copy)));
            const auto fence=timeline_.Signal(direct_queue_.Get());
            {
                std::lock_guard lock(pending_mutex_);
                for(auto& operation:pending_)if(operation.fence==UINT64_MAX)operation.fence=fence;
            }
            timeline_.Wait(fence);
            Collect();
        }else Finish();
    }catch(...){}
    try { DrainPriority(); } catch (...) {}
    priority_worker_.request_stop();
    pending_cv_.notify_all();
    if(priority_worker_.joinable())priority_worker_.join();
}

size_t CommandScheduler::AcquireContext()try {
    const auto completed = timeline_.Completed();
    for (size_t index = 0; index < contexts_.size(); ++index) {
        auto& context = contexts_[index];
        if (context.fence > completed) continue;
        CollectTimestamps(context);
        Check(DX12_API_CALL(context.allocator->Reset()));
        Check(DX12_API_CALL(context.list->Reset(context.allocator.Get(), nullptr)));
        context.closed = false;
        return index;
    }
    if (contexts_.size() < 16) {
        Context context;
        Check(DX12_API_CALL(device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context.allocator))));
        Check(DX12_API_CALL(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            context.allocator.Get(), nullptr, IID_PPV_ARGS(&context.list))));
        wchar_t name[96];swprintf_s(name,L"shadPS4 DX12 DIRECT allocator %zu",contexts_.size());context.allocator->SetName(name);
        swprintf_s(name,L"shadPS4 DX12 DIRECT list %zu",contexts_.size());context.list->SetName(name);
        contexts_.push_back(std::move(context));
        return contexts_.size() - 1;
    }
    const auto oldest = std::min_element(contexts_.begin(), contexts_.end(),
        [](const Context& left, const Context& right) { return left.fence < right.fence; });
    const auto index = size_t(oldest - contexts_.begin());
    Wait(oldest->fence);
    CollectTimestamps(*oldest);
    Check(DX12_API_CALL(oldest->allocator->Reset()));
    Check(DX12_API_CALL(oldest->list->Reset(oldest->allocator.Get(), nullptr)));
    oldest->closed = false;
    return index;
}catch(...){ReportDeviceRemoval(device_.Get());throw;}

void CommandScheduler::CollectTimestamps(Context& context){
    // A reused recording context still carries its previous completed fence.
    // Only a closed, submitted list owns the current timestamp resolve writes.
    if(!context.closed||context.timestamps_collected||context.timestamp_samples.empty()||context.fence>timeline_.Completed())return;
    UINT64 frequency{};if(FAILED(DX12_API_CALL(direct_queue_->GetTimestampFrequency(&frequency)))||!frequency){
        context.timestamp_samples.clear();context.timestamp_cursor=0;context.timestamps_collected=true;return;
    }
    const auto bytes=SIZE_T(context.timestamp_cursor)*sizeof(UINT64);void* mapped{};D3D12_RANGE range{0,bytes};
    Check(DX12_API_CALL(context.timestamp_readback->Map(0,&range,&mapped)));
    const auto* values=static_cast<const UINT64*>(mapped);
    for(const auto& sample:context.timestamp_samples)if(values[sample.begin+1]>=values[sample.begin])
        Diagnostics::GpuTime(sample.name,double(values[sample.begin+1]-values[sample.begin])*1000.0/double(frequency));
    D3D12_RANGE none{};DX12_API_CALL(context.timestamp_readback->Unmap(0,&none));
    context.timestamp_samples.clear();context.timestamp_cursor=0;context.timestamps_collected=true;
}

ID3D12GraphicsCommandList* CommandScheduler::Begin() {
    if(submit_failure_)std::rethrow_exception(submit_failure_);
    if (!recording_) {
        active_ = AcquireContext();
        recording_ = true;
    }
    return contexts_[active_].list.Get();
}

CommandScheduler::GpuTimestamp CommandScheduler::BeginTimestamp(const char* name){
    if(!Diagnostics::Enabled()||!name)return {};
    auto* list=Begin();auto& context=contexts_[active_];constexpr UINT capacity=2048;
    if(!context.timestamps){
        D3D12_QUERY_HEAP_DESC desc{D3D12_QUERY_HEAP_TYPE_TIMESTAMP,capacity,0};
        Check(DX12_API_CALL(device_->CreateQueryHeap(&desc,IID_PPV_ARGS(&context.timestamps))));
        context.timestamp_readback=Buffer(device_.Get(),u64(capacity)*sizeof(UINT64),
            D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST).handle;
    }
    if(context.timestamp_cursor>capacity-2)return {};
    const UINT begin=context.timestamp_cursor;context.timestamp_cursor+=2;
    DX12_API_CALL(list->EndQuery(context.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,begin));
    return {active_,begin};
}

void CommandScheduler::EndTimestamp(GpuTimestamp token,const char* name){
    if(!token)return;
    if(!recording_||token.context!=active_)throw std::logic_error("GPU timestamp crossed command-list submission");
    auto& context=contexts_[active_];auto* list=context.list.Get();
    DX12_API_CALL(list->EndQuery(context.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,token.begin+1));
    DX12_API_CALL(list->ResolveQueryData(context.timestamps.Get(),D3D12_QUERY_TYPE_TIMESTAMP,
        token.begin,2,context.timestamp_readback.Get(),u64(token.begin)*sizeof(UINT64)));
    context.timestamp_samples.push_back({token.begin,name});context.timestamps_collected=false;
}

u64 CommandScheduler::Submit(std::span<std::function<void(u64)>> retirements,
                             std::span<std::function<void(u64)>> submitted) try {
    if(submit_failure_)std::rethrow_exception(submit_failure_);
    // Both COPY and DIRECT queues may read upload heaps. CPU copies must
    // finish before either executes and before publication callbacks/labels.
    WaitHostCopies();
    if (copy_recording_ && !recording_) Begin();
    // Allocate and COPY every resource lease before either queue executes.
    // deque references survive append/pop of other nodes; UINT64_MAX cannot
    // be collected until this batch receives a successful direct fence.
    std::vector<PendingOperation*> batch;
    if(!submitted.empty()&&retirements.size()==SIZE_MAX)
        throw std::overflow_error("Submission callback count overflow");
    batch.reserve(retirements.size()+!submitted.empty());
    {
        std::lock_guard lock(pending_mutex_);
        const auto old_size=pending_.size();
        try {
            if(!submitted.empty()){
                // Publication callbacks may themselves capture GPU resources.
                // Keep even uncalled captures through the batch fence if an
                // earlier publication callback throws and the caller unwinds.
                pending_.push_back({UINT64_MAX,
                    [callbacks=std::vector<std::function<void(u64)>>(submitted.begin(),submitted.end())](u64){
                        (void)callbacks;
                    }});
                batch.push_back(&pending_.back());
            }
            for(const auto& callback:retirements)if(callback){
                pending_.push_back({UINT64_MAX,callback});
                batch.push_back(&pending_.back());
            }
        }catch(...){
            while(pending_.size()>old_size)pending_.pop_back();
            throw;
        }
    }
    FlushCopy();
    if(recording_){
        Diagnostics::Count("gpu-submit.direct");
        auto& context=contexts_[active_];
        Check(DX12_API_CALL(context.list->Close()));
        // Once ExecuteCommandLists may have run, no failure can replay or reset
        // this allocator. A failed Signal leaves the leases parked indefinitely.
        context.closed=true;context.fence=UINT64_MAX;recording_=false;
        submitted_=timeline_.Submit(direct_queue_.Get(),context.list.Get());
        context.fence=submitted_;
    }
    if(!submitted_&&(!batch.empty()||!submitted.empty()))
        throw std::logic_error("Submission callbacks have no GPU fence");
    {
        std::lock_guard lock(pending_mutex_);
        for(auto* operation:batch)operation->fence=submitted_;
    }
    // All leases are already scheduler-owned if publication throws. The outer
    // failure latch stops recording, so partially executed callbacks are not retried.
    for(auto& callback:submitted)if(callback)callback(submitted_);
    Collect();
    return submitted_;
}catch(...){submit_failure_=std::current_exception();ReportDeviceRemoval(device_.Get());throw;}

void CommandScheduler::Wait(u64 fence){
    try{timeline_.Wait(fence);}catch(...){ReportDeviceRemoval(device_.Get());throw;}
}

void CommandScheduler::Finish() {
    const auto fence = Submit();
    if (fence) Wait(fence);
    Collect();
}

void CommandScheduler::Defer(u64 fence, std::function<void(u64)> callback, bool priority) {
    if (!fence || !callback) throw std::invalid_argument("invalid scheduler retirement");
    {
        std::lock_guard lock(pending_mutex_);
        (priority ? priority_pending_ : pending_).push_back({fence, std::move(callback)});
    }
    pending_cv_.notify_one();
}

void CommandScheduler::Collect() {
    const auto completed = timeline_.Completed();
    std::deque<PendingOperation> ready;
    {
        std::lock_guard lock(pending_mutex_);
        while (!pending_.empty() && pending_.front().fence <= completed) {
            ready.push_back(std::move(pending_.front()));
            pending_.pop_front();
        }
    }
    // A completed callback can report an error, but later completed callbacks
    // still own required releases/publications. Drain all of them once before
    // reporting the first failure instead of destroying the uncalled tail.
    std::exception_ptr callback_failure;
    for (auto& operation : ready) {
        try { operation.callback(operation.fence); }
        catch (...) { if(!callback_failure)callback_failure=std::current_exception(); }
    }
    for(auto& context:contexts_)CollectTimestamps(context);
    if(callback_failure)std::rethrow_exception(callback_failure);
    std::exception_ptr failure;
    {std::lock_guard lock(pending_mutex_);failure=std::exchange(priority_failure_,{});}
    if(failure)std::rethrow_exception(failure);
}

void CommandScheduler::DrainPriority(){
    if(std::this_thread::get_id()==priority_worker_.get_id())
        throw std::logic_error("Priority callback cannot drain its own scheduler");
    std::unique_lock lock(pending_mutex_);
    pending_cv_.wait(lock,[&]{return priority_pending_.empty()&&!priority_in_flight_;});
    auto failure=std::exchange(priority_failure_,{});lock.unlock();
    if(failure)std::rethrow_exception(failure);
}

void CommandScheduler::PriorityWorker(std::stop_token token) {
    SetThreadDescription(GetCurrentThread(),L"shadPS4:DX12PriorityRetire");
    while (!token.stop_requested()) {
        PendingOperation operation;
        {
            std::unique_lock lock(pending_mutex_);
            pending_cv_.wait(lock, [&] { return token.stop_requested() || !priority_pending_.empty(); });
            if (token.stop_requested()) break;
            operation = std::move(priority_pending_.front());
            priority_pending_.pop_front();
            priority_in_flight_=true;
        }
        std::exception_ptr failure;
        try{Wait(operation.fence);operation.callback(operation.fence);}
        catch(...){failure=std::current_exception();Diagnostics::Count("gpu-priority.failure");}
        // Release retained source/readback allocations before publishing drain
        // completion; a derived owner can now safely destroy its fields.
        operation={};
        {std::lock_guard lock(pending_mutex_);
            if(failure&&!priority_failure_)priority_failure_=std::move(failure);
            priority_in_flight_=false;
        }
        pending_cv_.notify_all();
    }
}

} // namespace NativeDX12
