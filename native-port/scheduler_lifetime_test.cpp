#include "video_core/renderer_d3d12/command_scheduler.h"
#include "scheduler_submission_fixture.h"
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
