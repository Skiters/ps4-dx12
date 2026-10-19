#include "video_core/renderer_d3d12/command_scheduler.h"
#include "scheduler_submission_fixture.h"
#include "video_core/renderer_d3d12/cpu_buffer_snapshot.h"
#include <dxgi1_6.h>
#include <cstdio>
#include <atomic>
using namespace NativeDX12;
static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static u64 Samples() {
    auto& hub = Diagnostics::Hub::Get();
    std::lock_guard lock(hub.mutex);
    u64 samples = 0;
    for (const auto& entry : hub.gpu) samples += entry.calls;
    return samples;
}
int main() { try {
    ComPtr<ID3D12Debug> debug;
    Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    RunSchedulerSubmissionFixture(device.Get());
    // Deferred host writes must precede both queue types. Expected bytes are
    // independent of the live snapshot, which changes before GPU submission.
    for(unsigned mode=0;mode<4;++mode){
        const bool copy_queue=(mode&1)!=0,unmap=(mode&2)!=0;
        CommandScheduler uploads(device.Get());
        constexpr size_t bytes=1024*1024;
        auto source=CpuBufferSnapshot{};source.resize(bytes);
        auto* initial=source.WriteData();
        for(size_t i=0;i<bytes;++i)initial[i]=std::byte((i*29+71)&255);
        auto staging=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto result=Buffer(device.Get(),bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE empty{};Check(staging.handle->Map(0,&empty,&mapped));
        uploads.QueueHostCopy(staging.handle.Get(),mapped,source.Share(),0,bytes,unmap);
        std::memset(source.WriteData(),0xee,bytes);
        const auto record=[&](auto* list){list->CopyBufferRegion(result.handle.Get(),0,staging.handle.Get(),0,bytes);};
        if(copy_queue)uploads.SubmitCopy(record);else record(uploads.Begin());
        uploads.Finish();if(!unmap)staging.handle->Unmap(0,nullptr);
        D3D12_RANGE all{0,bytes};Check(result.handle->Map(0,&all,&mapped));
        bool equal=true;for(size_t i=0;i<bytes;++i)
            equal&=static_cast<const std::byte*>(mapped)[i]==std::byte((i*29+71)&255);
        result.handle->Unmap(0,&empty);
        Require(equal,"GPU queue read before host upload or consumed a later snapshot version");
        std::printf("PASS: immutable host upload before %s queue, unmap=%u, exact 1MiB readback\n",copy_queue?"COPY":"DIRECT",unsigned(unmap));
    }
    CommandScheduler scheduler(device.Get());
    // A recording list has no completion fence, including after context reuse.
    for (unsigned pass = 0; pass != 2; ++pass) {
        auto stamp = scheduler.BeginTimestamp("lifetime-test");
        Require(bool(stamp), "timestamp unavailable");
        scheduler.EndTimestamp(stamp, "lifetime-test");
        auto before = Samples();
        scheduler.Collect();
        Require(Samples() == before, "collected unsubmitted GPU timestamps");
        scheduler.Finish();
    }
    // Copy-only work must be covered by a newly signalled direct fence.
    auto upload = Buffer(device.Get(), 8, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    auto output = Buffer(device.Get(), 8, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST);
    void* data{};
    D3D12_RANGE none{};
    Check(upload.handle->Map(0, &none, &data));
    const u32 expected[]{0x12345678, 0xabcdef01};
    memcpy(data, expected, sizeof(expected));
    upload.handle->Unmap(0, nullptr);
    auto previous = scheduler.Submitted();
    for (u64 offset : {0ull, 4ull}) scheduler.SubmitCopy([&](auto* list) {
        list->CopyBufferRegion(output.handle.Get(), offset, upload.handle.Get(), offset, 4);
    });
    scheduler.Finish();
    Require(scheduler.Submitted() > previous, "copy-only batch lacks completion fence");
    D3D12_RANGE range{0, 8};
    Check(output.handle->Map(0, &range, &data));
    Require(memcmp(data, expected, sizeof(expected)) == 0, "batched copy visibility mismatch");
    output.handle->Unmap(0, &none);
    // A fence alone does not mean a priority physical-write callback has run.
    // Retain its readback while the caller drops its resource and release the
    // simulated memory lock before draining the worker.
    std::mutex backing_mutex;std::atomic<unsigned> priority_calls{};
    std::unique_lock backing_lock(backing_mutex);
    scheduler.Defer(scheduler.Submitted(),[&,retained=output](u64){
        std::lock_guard lock(backing_mutex);void* mapped{};D3D12_RANGE all{0,8};
        Check(retained.handle->Map(0,&all,&mapped));
        const bool equal=memcmp(mapped,expected,sizeof(expected))==0;
        D3D12_RANGE empty{};retained.handle->Unmap(0,&empty);
        Require(equal,"priority readback source was released or changed");++priority_calls;
    },true);
    output={};backing_lock.unlock();scheduler.DrainPriority();
    Require(priority_calls==1,"priority drain ended before physical write");
    scheduler.Defer(scheduler.Submitted(),[](u64){throw std::runtime_error("injected priority write failure");},true);
    scheduler.Defer(scheduler.Submitted(),[&](u64){++priority_calls;},true);
    bool priority_failed{};try{scheduler.DrainPriority();}catch(const std::runtime_error&){priority_failed=true;}
    Require(priority_failed&&priority_calls==2,"priority failure was lost or prevented following retirement");
    {
        CommandScheduler retiring(device.Get());const auto fence=retiring.Submit();
        retiring.Defer(fence,[&](u64){++priority_calls;},true);
    }
    Require(priority_calls==3,"scheduler destruction abandoned a priority write");
    puts("PASS: unsubmitted timestamp isolation and copy-only fence visibility");
    return 0;
} catch (const std::exception& e) { fprintf(stderr, "FAIL: %s\n", e.what()); return 1; } }
