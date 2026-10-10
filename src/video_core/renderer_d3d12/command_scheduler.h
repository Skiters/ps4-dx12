#pragma once

#include "resources.h"
#include "common/types.h"
#include <deque>
#include <functional>
#include <span>
#include <thread>
#include <condition_variable>

namespace NativeDX12 {

// D3D12 counterpart of Vulkan's scheduler/session/semaphore/resource-pool
// layer.  Command allocators and lists are leased until their fence retires;
// queue submission and callback retirement are ordered by one timeline.
class CommandScheduler {
    struct Context {
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        u64 fence{};
        bool closed{};
        ComPtr<ID3D12QueryHeap> timestamps;
        ComPtr<ID3D12Resource> timestamp_readback;
        struct Timestamp {UINT begin{};const char* name{};};
        std::vector<Timestamp> timestamp_samples;
        UINT timestamp_cursor{};
        bool timestamps_collected=true;
    };
    struct PendingOperation {
        u64 fence{};
        std::function<void(u64)> callback;
    };

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> direct_queue_;
    ComPtr<ID3D12CommandQueue> copy_queue_;
    Timeline timeline_;
    Timeline copy_timeline_;
    std::vector<Context> copy_contexts_;
    size_t copy_active_{};
    bool copy_recording_{};
    u64 copy_submitted_{};
    std::vector<Context> contexts_;
    size_t active_{};
    bool recording_{};
    u64 submitted_{};
    std::deque<PendingOperation> pending_;
    std::deque<PendingOperation> priority_pending_;
    mutable std::mutex pending_mutex_;
    std::condition_variable pending_cv_;
    std::jthread priority_worker_;

    size_t AcquireContext();
    size_t AcquireCopyContext();
    u64 FlushCopy();
    void PriorityWorker(std::stop_token);
    void CollectTimestamps(Context&);

public:
    struct GpuTimestamp {size_t context=SIZE_MAX;UINT begin{};explicit operator bool()const{return context!=SIZE_MAX;}};
    explicit CommandScheduler(ID3D12Device*);
    ~CommandScheduler();
    CommandScheduler(const CommandScheduler&) = delete;
    CommandScheduler& operator=(const CommandScheduler&) = delete;

    ID3D12GraphicsCommandList* Begin();
    GpuTimestamp BeginTimestamp(const char* name);
    void EndTimestamp(GpuTimestamp,const char* name);
    u64 Submit(std::span<std::function<void(u64)>> retirements = {},
               std::span<std::function<void(u64)>> submitted = {});
    void Wait(u64 fence) { timeline_.Wait(fence); }
    void Finish();
    void Collect();
    void Defer(u64 fence, std::function<void(u64)> callback, bool priority = false);
    // Records into the pending upload list. Completion belongs to the next
    // direct Submit() fence; no independent completed fence exists yet.
    void SubmitCopy(const std::function<void(ID3D12GraphicsCommandList*)>& record);
    void MakeQueueWait(ID3D12CommandQueue* queue, u64 fence) {
        if(!queue||!fence)throw std::invalid_argument("invalid scheduler consumer wait");
        Check(DX12_API_CALL(queue->Wait(timeline_.Fence(),fence)));
    }
    void WaitExternal(ID3D12Fence* fence, u64 value) {
        if(!fence||!value)throw std::invalid_argument("invalid external queue wait");
        Check(DX12_API_CALL(direct_queue_->Wait(fence,value)));
    }

    ID3D12CommandQueue* Queue() const { return direct_queue_.Get(); }
    u64 Completed() const { return timeline_.Completed(); }
    u64 Submitted() const { return submitted_; }
    bool Recording() const { return recording_; }
};

} // namespace NativeDX12
