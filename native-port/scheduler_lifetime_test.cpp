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
static void TextureHostUploads(ID3D12Device* device){
    ComPtr<ID3D12InfoQueue> info;Check(device->QueryInterface(IID_PPV_ARGS(&info)));
    const auto first_message=info->GetNumStoredMessages();
    const auto discarded=info->GetNumMessagesDiscardedByMessageCountLimit();
    for(UINT width:{3u,333u})for(UINT depth:{1u,3u})for(bool copy_queue:{false,true}){
        CommandScheduler scheduler(device);constexpr UINT height=7;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=depth==1?D3D12_RESOURCE_DIMENSION_TEXTURE2D:D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        desc.Width=width;desc.Height=height;desc.DepthOrArraySize=UINT16(depth);desc.MipLevels=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UINT;desc.SampleDesc.Count=1;
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        Resource texture;texture.state=copy_queue?D3D12_RESOURCE_STATE_COMMON:D3D12_RESOURCE_STATE_COPY_DEST;
        Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,texture.state,nullptr,IID_PPV_ARGS(&texture.handle)));
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows{};UINT64 row_bytes{},total{};
        device->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&row_bytes,&total);
        auto staging=Buffer(device,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto raw=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        auto pixels=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE empty{};Check(staging.handle->Map(0,&empty,&mapped));
        std::memset(mapped,0xa5,size_t(total));
        auto slices=std::make_shared<std::vector<std::vector<std::byte>>>(depth);
        for(UINT z=0;z<depth;++z){(*slices)[z].resize(size_t(rows*row_bytes));
            for(UINT row=0;row<rows;++row)for(UINT64 x=0;x<row_bytes;++x)
                (*slices)[z][size_t(row*row_bytes+x)]=std::byte((z*71+row*13+x*7)&255);}
        auto short_source=std::make_shared<std::vector<std::vector<std::byte>>>(*slices);
        short_source->front().resize(size_t(rows*row_bytes-1));bool rejected{};
        try{scheduler.QueueHostTextureCopy(staging.handle.Get(),mapped,short_source,footprint,rows,row_bytes);}
        catch(const std::out_of_range&){rejected=true;}
        Require(rejected,"Short immutable texture source was accepted");
        auto bad=footprint;bad.Offset=total;rejected=false;
        try{scheduler.QueueHostTextureCopy(staging.handle.Get(),mapped,slices,bad,rows,row_bytes);}
        catch(const std::out_of_range&){rejected=true;}
        Require(rejected,"Out-of-bounds texture destination was accepted");
        scheduler.QueueHostTextureCopy(staging.handle.Get(),mapped,std::move(slices),footprint,rows,row_bytes);
        const auto record=[&](auto* list){
            list->CopyBufferRegion(raw.handle.Get(),0,staging.handle.Get(),0,total);
            D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=staging.handle.Get();
            from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=footprint;
            D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=texture.handle.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
        };
        if(copy_queue)scheduler.SubmitCopy(record);else record(scheduler.Begin());
        texture.Transition(scheduler.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION from{};from.pResource=texture.handle.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION to{};to.pResource=pixels.handle.Get();
        to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint;
        scheduler.Begin()->CopyTextureRegion(&to,0,0,0,&from,nullptr);scheduler.Finish();
        D3D12_RANGE all{0,SIZE_T(total)};Check(raw.handle->Map(0,&all,&mapped));
        bool equal=true;for(UINT64 offset=0;offset<total;++offset){
            const auto relative=offset-footprint.Offset,flat_row=relative/footprint.Footprint.RowPitch;
            const auto x=relative%footprint.Footprint.RowPitch;
            const bool logical=offset>=footprint.Offset&&flat_row<uint64_t(rows)*depth&&x<row_bytes;
            const auto expected=logical?unsigned(((flat_row/rows)*71+(flat_row%rows)*13+x*7)&255):0xa5;
            equal&=static_cast<const std::byte*>(mapped)[offset]==std::byte(expected);
        }
        raw.handle->Unmap(0,&empty);Require(equal,"Deferred row upload changed source bytes or native padding");
        Check(pixels.handle->Map(0,&all,&mapped));equal=true;
        for(UINT z=0;z<depth;++z)for(UINT row=0;row<rows;++row)for(UINT64 x=0;x<row_bytes;++x){
            const auto offset=footprint.Offset+(uint64_t(z)*rows+row)*footprint.Footprint.RowPitch+x;
            equal&=static_cast<const std::byte*>(mapped)[offset]==std::byte((z*71+row*13+x*7)&255);}
        pixels.handle->Unmap(0,&empty);Require(equal,"Actual native texture lost deferred rows/volume slices");
    }
    Require(info->GetNumMessagesDiscardedByMessageCountLimit()==discarded,"Texture upload lost GPU validation messages");
    for(UINT64 i=first_message;i<info->GetNumStoredMessages();++i){SIZE_T bytes{};Check(info->GetMessage(i,nullptr,&bytes));
        std::vector<std::byte> storage(bytes);auto* message=reinterpret_cast<D3D12_MESSAGE*>(storage.data());
        Check(info->GetMessage(i,message,&bytes));Require(message->Severity>D3D12_MESSAGE_SEVERITY_ERROR,message->pDescription);}
    std::puts("PASS: eight deferred 2D/3D row uploads, COPY/DIRECT, exact pixels/padding and invalid footprint/source negatives");
}
int main() { try {
    ComPtr<ID3D12Debug> debug;
    Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    debug->EnableDebugLayer();
    ComPtr<ID3D12Device> device;
    Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
    RunSchedulerSubmissionFixture(device.Get());
    TextureHostUploads(device.Get());
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
