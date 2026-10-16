#include "video_core/renderer_d3d12/command_scheduler.h"
#include "scheduler_submission_fixture.h"
#include <dxgi1_6.h>
#include <cstdio>
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
    puts("PASS: unsubmitted timestamp isolation and copy-only fence visibility");
    return 0;
} catch (const std::exception& e) { fprintf(stderr, "FAIL: %s\n", e.what()); return 1; } }
