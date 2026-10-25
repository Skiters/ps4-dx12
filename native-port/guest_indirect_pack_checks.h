// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_indirect_pack.h"
namespace NativeDX12::Tests {
// Explicit hardware mode: crosses the65535-group X boundary without game
// assets or a multi-gigabyte runtime arena. Only selected records read back.
inline void CheckGuestIndirectLargeGridGpu(ID3D12Device* device,Rasterizer& renderer){
    constexpr u32 capacity=65535*128+1;
    const auto plan=GuestIndirectPackPlan::Make(false,16,capacity,false);
    auto upload=Buffer(device,plan.source_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));
    memset(mapped,0x5a,size_t(plan.source_bytes));const std::array<u32,4> last{1,2,3,4};
    memcpy(static_cast<std::byte*>(mapped)+plan.source_bytes-16,last.data(),16);upload.handle->Unmap(0,nullptr);
    auto poison=Buffer(device,32,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    Check(poison.handle->Map(0,&none,&mapped));memset(mapped,0xcd,32);poison.handle->Unmap(0,nullptr);
    auto source=Buffer(device,plan.source_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto commands=renderer.AcquireTransientBuffer(plan.command_bytes+16);
    auto count=renderer.AcquireTransientBuffer(4);
    auto result=Buffer(device,52,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    auto* list=renderer.RecordingList();commands.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(source.handle.Get(),0,upload.handle.Get(),0,plan.source_bytes);
    list->CopyBufferRegion(commands.handle.Get(),0,poison.handle.Get(),0,16);
    list->CopyBufferRegion(commands.handle.Get(),plan.command_bytes-16,poison.handle.Get(),0,32);
    {
        GuestIndirectPackPass pack(device);GraphicsRuntime unused{};
        pack.Record(renderer,plan,source,0,nullptr,0,unused,commands,nullptr,count);
    }
    list=renderer.RecordingList();commands.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    count.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    list->CopyBufferRegion(result.handle.Get(),0,commands.handle.Get(),0,16);
    list->CopyBufferRegion(result.handle.Get(),16,commands.handle.Get(),plan.command_bytes-16,32);
    list->CopyBufferRegion(result.handle.Get(),48,count.handle.Get(),0,4);renderer.Finish();
    std::array<std::byte,52> expected;expected.fill(std::byte{0xcd});memset(expected.data(),0x5a,16);
    memcpy(expected.data()+16,last.data(),16);memcpy(expected.data()+48,&capacity,4);
    D3D12_RANGE all{0,52};Check(result.handle->Map(0,&all,&mapped));
    const bool equal=!memcmp(mapped,expected.data(),expected.size());result.handle->Unmap(0,&none);
    if(!equal)throw std::runtime_error("GPU indirect pack lost its second dispatch row, first record, count or tail guard");
}
inline void CheckGuestIndirectPackGpu(ID3D12Device* device,Rasterizer& renderer){
    for(const bool with_runtime:std::array{false,true})
    for(const bool indexed:std::array{false,true})for(const bool padded:std::array{false,true})
    for(const u32 active:std::array<u32,5>{0,1,2,99,UINT32_MAX}){
        const auto plan=GuestIndirectPackPlan::Make(indexed,padded?32:indexed?20:16,3,with_runtime);
        const u32 input_bytes=u32(plan.source_bytes)+32,command_bytes=u32(plan.command_bytes)+16;
        const u32 runtime_bytes=u32(plan.runtime_bytes)+16,total=input_bytes+command_bytes+runtime_bytes+20;
        auto upload=Buffer(device,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));memset(mapped,0x6d,total);
        memcpy(static_cast<std::byte*>(mapped)+4,&active,4);
        std::array<std::array<u32,5>,3> inputs{};
        for(u32 command=0;command<3;++command){
            inputs[command]={3,2+command,11+command,indexed?std::bit_cast<u32>(-7+s32(command)):13+command,13+command};
            memcpy(static_cast<std::byte*>(mapped)+16+command*plan.source_stride,inputs[command].data(),indexed?20:16);
        }
        std::vector<std::byte> expected(total,std::byte{0x6d});memcpy(expected.data(),mapped,input_bytes);
        upload.handle->Unmap(0,nullptr);
        auto source=Buffer(device,input_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto commands=renderer.AcquireTransientBuffer(command_bytes);
        auto constants=renderer.AcquireTransientBuffer(runtime_bytes);
        auto count=renderer.AcquireTransientBuffer(20);
        auto result=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        auto* list=renderer.RecordingList();commands.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        constants.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);count.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(source.handle.Get(),0,upload.handle.Get(),0,input_bytes);
        list->CopyBufferRegion(commands.handle.Get(),0,upload.handle.Get(),input_bytes,command_bytes);
        list->CopyBufferRegion(constants.handle.Get(),0,upload.handle.Get(),input_bytes+command_bytes,runtime_bytes);
        list->CopyBufferRegion(count.handle.Get(),0,upload.handle.Get(),total-20,20);
        std::array<std::byte,sizeof(GraphicsRuntime)> template_bytes;
        for(u32 byte=0;byte<template_bytes.size();++byte)template_bytes[byte]=std::byte(byte*19+7);
        GraphicsRuntime runtime;memcpy(&runtime,template_bytes.data(),sizeof(runtime));runtime.draw_id=UINT32_MAX-1;
        runtime.viewport_extents[0][0]=std::bit_cast<float>(0x7fc12345u);
        runtime.viewport_depths[15][3]=std::bit_cast<float>(0x80000000u);
        {
            GuestIndirectPackPass pack(device);
            const auto input_state=source.State(0);
            Resource input_count;
            // Only enqueue the separate count upload when Record will retain
            // that resource. An unused copy otherwise outlives this scope and
            // dereferences a destroyed resource when the command list executes.
            if(padded&&active!=UINT32_MAX){
                input_count=Buffer(device,4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
                Check(input_count.handle->SetName(L"indirect fixture count retained by pack"));
                renderer.RecordingList()->CopyBufferRegion(input_count.handle.Get(),0,upload.handle.Get(),4,4);
            }
            Resource* count_input=active==UINT32_MAX?nullptr:padded?&input_count:&source;
            if(!active){
                const auto reject=[&](auto action){bool failed{};try{action();}catch(const std::invalid_argument&){failed=true;}
                    if(!failed)throw std::runtime_error("GPU indirect pack accepted invalid plan/extent/alias");};
                auto malformed=plan;malformed.runtime_stride+=256;
                reject([&]{pack.Record(renderer,malformed,source,16,&source,4,runtime,commands,with_runtime?&constants:nullptr,count);});
                reject([&]{pack.Record(renderer,plan,source,16,&source,2,runtime,commands,with_runtime?&constants:nullptr,count);});
                if(with_runtime)reject([&]{pack.Record(renderer,plan,source,16,&source,4,runtime,constants,constants,count);});
                else reject([&]{pack.Record(renderer,plan,source,16,&source,4,runtime,commands,nullptr,commands);});
                auto duplicate=source;
                reject([&]{pack.Record(renderer,plan,source,16,&duplicate,4,runtime,commands,with_runtime?&constants:nullptr,count);});
                reject([&]{pack.Record(renderer,plan,source,UINT64_MAX,nullptr,0,runtime,commands,with_runtime?&constants:nullptr,count);});
            }
            pack.Record(renderer,plan,source,16,count_input,count_input==&source?4:0,
                runtime,commands,with_runtime?&constants:nullptr,count);
            if(source.State(0)!=input_state)throw std::runtime_error("GPU pack leaked its input SRV state into the guest draw");
            if(input_count.handle&&input_count.State(0)!=D3D12_RESOURCE_STATE_COPY_DEST)throw std::runtime_error("GPU pack leaked its count SRV state");
        } // PSO/root and immutable template must outlive the owner.
        const u32 actual=std::min(active,3u);memcpy(expected.data()+total-20,&actual,4);
        for(u32 command=0;command<actual;++command){
            const u64 address=constants.handle->GetGPUVirtualAddress()+u64(command)*1024;
            auto* args=expected.data()+input_bytes+command*plan.command_stride;
            if(with_runtime)memcpy(args,&address,8);
            memcpy(args+(with_runtime?8:0),inputs[command].data(),indexed?20:16);
            if(!with_runtime)continue;
            auto* target=expected.data()+input_bytes+command_bytes+command*1024;
            memset(target,0,1024);memcpy(target,&runtime,sizeof(runtime));
            const u32 first=indexed?std::bit_cast<u32>(-7+s32(command)):11+command;
            const u32 instance=13+command,kind=indexed?1:0,draw_id=(UINT32_MAX-1)+command;
            memcpy(target,&first,4);memcpy(target+4,&instance,4);memcpy(target+8,&kind,4);memcpy(target+16,&draw_id,4);
        }
        list=renderer.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        commands.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);constants.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        count.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(result.handle.Get(),0,source.handle.Get(),0,input_bytes);
        list->CopyBufferRegion(result.handle.Get(),input_bytes,commands.handle.Get(),0,command_bytes);
        list->CopyBufferRegion(result.handle.Get(),input_bytes+command_bytes,constants.handle.Get(),0,runtime_bytes);
        list->CopyBufferRegion(result.handle.Get(),total-20,count.handle.Get(),0,20);
        renderer.Finish();D3D12_RANGE all{0,total};Check(result.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),total);result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("GPU guest indirect pack lost command/runtime/count/padding/guard contents");
    }
}
// Expected values use a byte-vector decoder, independent of the HLSL address
// calculation. Full private arenas (including untouched tails) are compared.
inline void CheckGuestIndirectIndexGpu(ID3D12Device* device,Rasterizer& renderer){
    for(const bool runtime_enabled:std::array{false,true})for(const u32 swap:std::array<u32,4>{0,1,2,3})
    for(const u32 marker:std::array<u32,4>{UINT32_MAX,0,0x1717,0xffff})
    for(const u32 available:std::array<u32,3>{0,10,259})for(const u32 active:std::array<u32,4>{0,2,3,UINT32_MAX}){
        const auto plan=GuestIndirectPackPlan::Make(true,32,3,runtime_enabled,true);
        const u32 source_bytes=16+u32(plan.source_bytes),index_bytes=(2+available*2+3)&~3u;
        const u32 command_bytes=u32(plan.command_bytes)+16,runtime_bytes=u32(plan.runtime_bytes)+16;
        const u32 decoded_bytes=u32(GuestIndirectIndexDecode::OutputBytes(available,3))+16;
        const u32 total=source_bytes+index_bytes+command_bytes+runtime_bytes+decoded_bytes+20;
        auto upload=Buffer(device,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(upload.handle->Map(0,&none,&mapped));memset(mapped,0x6d,total);
        memcpy(mapped,&active,4);
        const std::array<std::array<u32,5>,3> inputs{{{5,2,1,u32(-3),7},{4,1,2,2,9},
            {6,1,available>3?available-3:UINT32_MAX,u32(-1),11}}};
        for(u32 i=0;i<3;++i)memcpy(static_cast<std::byte*>(mapped)+16+i*32,inputs[i].data(),20);
        for(u32 i=0;i<available*2;++i)static_cast<std::byte*>(mapped)[source_bytes+2+i]=std::byte(i*37+13);
        if(available>2)memset(static_cast<std::byte*>(mapped)+source_bytes+2+4,0x17,2);
        if(available>3)memset(static_cast<std::byte*>(mapped)+source_bytes+2+6,0,2);
        if(available>4)memset(static_cast<std::byte*>(mapped)+source_bytes+2+8,0xff,2);
        std::vector<std::byte> expected(total);memcpy(expected.data(),mapped,total);upload.handle->Unmap(0,nullptr);
        auto source=Buffer(device,source_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto indices=Buffer(device,index_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        auto commands=renderer.AcquireTransientBuffer(command_bytes),constants=renderer.AcquireTransientBuffer(runtime_bytes);
        auto decoded=renderer.AcquireTransientBuffer(decoded_bytes),count=renderer.AcquireTransientBuffer(20);
        auto result=Buffer(device,total,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        const std::array<std::pair<Resource*,u32>,6> regions{{{&source,source_bytes},{&indices,index_bytes},
            {&commands,command_bytes},{&constants,runtime_bytes},{&decoded,decoded_bytes},{&count,20}}};
        auto* list=renderer.RecordingList();u32 cursor{};
        for(auto [resource,bytes]:regions){resource->Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            list->CopyBufferRegion(resource->handle.Get(),0,upload.handle.Get(),cursor,bytes);cursor+=bytes;}
        GraphicsRuntime runtime{};runtime.draw_id=UINT32_MAX;
        GuestIndirectIndexDecode decode{&indices,2,available,swap,&decoded,marker!=UINT32_MAX,marker};
        {
            GuestIndirectPackPass pack(device);
            if(!active){
                const auto reject=[&](GuestIndirectIndexDecode bad){bool caught=false;
                    try{pack.Record(renderer,plan,source,16,&source,0,runtime,commands,
                        runtime_enabled?&constants:nullptr,count,&bad);}catch(const std::invalid_argument&){caught=true;}
                    if(!caught)throw std::runtime_error("Indirect index decoder accepted malformed alias/offset/mode");};
                auto bad=decode;bad.output=&commands;reject(bad);
                bad=decode;bad.offset=1;reject(bad);
                bad=decode;bad.swap_mode=4;reject(bad);
                bad=decode;bad.restart=true;bad.marker=0x10000;reject(bad);
                bad=decode;bad.source=&source;bad.offset=0;bad.output=&source;reject(bad);
            }
            pack.Record(renderer,plan,source,16,active==UINT32_MAX?nullptr:&source,0,runtime,
                commands,runtime_enabled?&constants:nullptr,count,&decode);
            if(source.State(0)!=D3D12_RESOURCE_STATE_COPY_DEST||indices.State(0)!=D3D12_RESOURCE_STATE_COPY_DEST)
                throw std::runtime_error("Indirect index packing leaked source states");
        }
        const u32 actual=std::min(active,3u),command_at=source_bytes+index_bytes;
        const u32 runtime_at=command_at+command_bytes,decoded_at=runtime_at+runtime_bytes;
        memcpy(expected.data()+total-20,&actual,4);
        for(u32 command=0;command<actual;++command){
            auto* args=expected.data()+command_at+command*plan.command_stride;
            if(runtime_enabled){const u64 address=constants.handle->GetGPUVirtualAddress()+u64(command)*plan.runtime_stride;
                memcpy(args,&address,8);args+=8;
                auto copy=runtime;copy.first_vertex=inputs[command][3];copy.base_instance=inputs[command][4];
                copy.indexed=1;copy.draw_id+=command;
                auto* bytes=expected.data()+runtime_at+command*plan.runtime_stride;
                memset(bytes,0,plan.runtime_stride);memcpy(bytes,&copy,sizeof(copy));}
            const u32 length=std::min(inputs[command][0],available-std::min(inputs[command][2],available));
            const u64 address=decoded.handle->GetGPUVirtualAddress()+u64(command)*std::max(1u,available)*4;
            const D3D12_INDEX_BUFFER_VIEW view{address,length*4,DXGI_FORMAT_R32_UINT};memcpy(args,&view,16);
            auto words=inputs[command];words[2]=0;
            if(marker==0)words[0]=length; // omitted robust suffix consists only of restart tokens
            memcpy(args+16,words.data(),20);
            std::vector<std::byte> bytes(length*2);
            if(length)memcpy(bytes.data(),expected.data()+source_bytes+2+inputs[command][2]*2,bytes.size());
            if(swap==1)for(size_t i=0;i+1<bytes.size();i+=2)std::swap(bytes[i],bytes[i+1]);
            if(swap>=2)for(size_t i=0;i+3<bytes.size();i+=4){
                if(swap==2){std::swap(bytes[i],bytes[i+3]);std::swap(bytes[i+1],bytes[i+2]);}
                else{std::swap(bytes[i],bytes[i+2]);std::swap(bytes[i+1],bytes[i+3]);}}
            for(u32 i=0;i<length;++i){u16 value{};memcpy(&value,bytes.data()+i*2,2);const u32 word=marker!=UINT32_MAX&&value==marker?UINT32_MAX:value;
                memcpy(expected.data()+decoded_at+command*std::max(1u,available)*4+i*4,&word,4);}
        }
        list=renderer.RecordingList();cursor=0;
        for(auto [resource,bytes]:regions){resource->Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(result.handle.Get(),cursor,resource->handle.Get(),0,bytes);cursor+=bytes;}
        renderer.Finish();D3D12_RANGE all{0,total};Check(result.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),total);result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("Per-command indirect index decode/IBV/runtime/count/guards mismatch");
    }
}
} // namespace NativeDX12::Tests
