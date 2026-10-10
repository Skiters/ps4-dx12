#include "command_scheduler.h"
#include "diagnostics.h"
#include <algorithm>

namespace NativeDX12 {

CommandScheduler::CommandScheduler(ID3D12Device* device) : device_(device), timeline_(device), copy_timeline_(device) {
    if (!device) throw std::invalid_argument("null D3D12 scheduler device");
    D3D12_COMMAND_QUEUE_DESC description{};
    description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(device_->CreateCommandQueue(&description, IID_PPV_ARGS(&direct_queue_))));
    description.Type=D3D12_COMMAND_LIST_TYPE_COPY;
    Check(DX12_API_CALL(device_->CreateCommandQueue(&description,IID_PPV_ARGS(&copy_queue_))));
    contexts_.reserve(8);
    copy_contexts_.reserve(8);
    active_ = AcquireContext();
    recording_ = true;
    priority_worker_ = std::jthread([this](std::stop_token token) { PriorityWorker(token); });
}

u64 CommandScheduler::SubmitCopy(const std::function<void(ID3D12GraphicsCommandList*)>& record){
    if(!record)throw std::invalid_argument("empty copy submission");
    if(!copy_recording_){copy_active_=AcquireCopyContext();copy_recording_=true;}
    auto& context=copy_contexts_[copy_active_];
    record(context.list.Get());
    Diagnostics::Count("gpu-copy.records");
    // The pending copy list is submitted immediately before the next direct
    // list. Its queue wait preserves the same upload-before-use dependency
    // without one ExecuteCommandLists/Signal pair per buffer slice.
    return 0;
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

size_t CommandScheduler::AcquireCopyContext(){
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
}

CommandScheduler::~CommandScheduler() {
    priority_worker_.request_stop();
    pending_cv_.notify_all();
    try { Finish(); } catch (...) {}
}

size_t CommandScheduler::AcquireContext() {
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
        contexts_.push_back(std::move(context));
        return contexts_.size() - 1;
    }
    const auto oldest = std::min_element(contexts_.begin(), contexts_.end(),
        [](const Context& left, const Context& right) { return left.fence < right.fence; });
    const auto index = size_t(oldest - contexts_.begin());
    timeline_.Wait(oldest->fence);
    Check(DX12_API_CALL(oldest->allocator->Reset()));
    Check(DX12_API_CALL(oldest->list->Reset(oldest->allocator.Get(), nullptr)));
    oldest->closed = false;
    return index;
}

void CommandScheduler::CollectTimestamps(Context& context){
    if(context.timestamps_collected||context.timestamp_samples.empty()||context.fence>timeline_.Completed())return;
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
                             std::span<std::function<void(u64)>> submitted) {
    // A copy batch must always be followed by a direct timeline signal.  Most
    // callers already have a direct list open, but keeping this invariant here
    // also makes a standalone SubmitCopy()/Submit() sequence waitable.
    if (copy_recording_ && !recording_) Begin();
    FlushCopy();
    if (!recording_) {
        Collect();
        return submitted_;
    }
    Diagnostics::Count("gpu-submit.direct");
    auto& context = contexts_[active_];
    Check(DX12_API_CALL(context.list->Close()));
    submitted_ = timeline_.Submit(direct_queue_.Get(), context.list.Get());
    context.fence = submitted_;
    context.closed = true;
    recording_ = false;
    // Publish ownership/fences before collecting older retirement callbacks.
    for(auto& callback:submitted)callback(submitted_);
    for (auto& callback : retirements) Defer(submitted_, std::move(callback));
    Collect();
    return submitted_;
}

void CommandScheduler::Finish() {
    const auto fence = Submit();
    if (fence) timeline_.Wait(fence);
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
    for (auto& operation : ready) operation.callback(operation.fence);
    for(auto& context:contexts_)CollectTimestamps(context);
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
        }
        timeline_.Wait(operation.fence);
        if (!token.stop_requested()) operation.callback(operation.fence);
    }
}

} // namespace NativeDX12
