// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_indirect_pack.h"
namespace NativeDX12::Tests {
inline void CheckGuestIndirectPackGpu(ID3D12Device* device,Rasterizer& renderer){
    for(const bool indexed:std::array{false,true})for(const bool padded:std::array{false,true})
    for(const u32 active:std::array<u32,5>{0,1,2,99,UINT32_MAX}){
        const auto plan=GuestIndirectPackPlan::Make(indexed,padded?32:indexed?20:16,3);
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
            auto input_count=Buffer(device,4,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            renderer.RecordingList()->CopyBufferRegion(input_count.handle.Get(),0,upload.handle.Get(),4,4);
            Resource* count_input=active==UINT32_MAX?nullptr:padded?&input_count:&source;
            if(!active){
                const auto reject=[&](auto action){bool failed{};try{action();}catch(const std::invalid_argument&){failed=true;}
                    if(!failed)throw std::runtime_error("GPU indirect pack accepted invalid plan/extent/alias");};
                auto malformed=plan;malformed.runtime_stride+=256;
                reject([&]{pack.Record(renderer,malformed,source,16,&source,4,runtime,commands,constants,count);});
                reject([&]{pack.Record(renderer,plan,source,16,&source,2,runtime,commands,constants,count);});
                reject([&]{pack.Record(renderer,plan,source,16,&source,4,runtime,constants,constants,count);});
                auto duplicate=source;
                reject([&]{pack.Record(renderer,plan,source,16,&duplicate,4,runtime,commands,constants,count);});
                reject([&]{pack.Record(renderer,plan,source,UINT64_MAX,nullptr,0,runtime,commands,constants,count);});
            }
            pack.Record(renderer,plan,source,16,count_input,count_input==&source?4:0,
                runtime,commands,constants,count);
            if(source.State(0)!=input_state)throw std::runtime_error("GPU pack leaked its input SRV state into the guest draw");
            if(input_count.State(0)!=D3D12_RESOURCE_STATE_COPY_DEST)throw std::runtime_error("GPU pack leaked its count SRV state");
        } // PSO/root and immutable template must outlive the owner.
        const u32 actual=std::min(active,3u);memcpy(expected.data()+total-20,&actual,4);
        for(u32 command=0;command<actual;++command){
            const u64 address=constants.handle->GetGPUVirtualAddress()+u64(command)*1024;
            auto* args=expected.data()+input_bytes+command*plan.command_stride;
            memcpy(args,&address,8);memcpy(args+8,inputs[command].data(),indexed?20:16);
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
} // namespace NativeDX12::Tests
