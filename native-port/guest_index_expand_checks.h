// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_index_expand.h"
#include <array>
#include <cstring>
namespace NativeDX12::Tests {
inline void CheckGuestPatchRestart(){
    using P=AmdGpu::PrimitiveType;
    // The expected patches are independent of the compactor: neither the two
    // leading vertices nor a segment's trailing vertices may form another patch.
    for(u32 marker:std::array{0xffffu,0xffffffffu}){
        const std::array source{marker,10u,11u,marker,20u,21u,22u,23u,24u,marker,marker,30u,31u,32u,marker};
        if(ExpandGuestPrimitive(P::PatchPrimitive,source,true,marker,3)!=std::vector<u32>{20,21,22,30,31,32})
            throw std::runtime_error("Patch restart joined partial control points across a marker");
        if(ExpandGuestPrimitive(P::PatchPrimitive,source,true,marker,4)!=std::vector<u32>{20,21,22,23})
            throw std::runtime_error("Patch restart ignored the original HS input control point count");
        if(ExpandGuestPrimitive(P::PatchPrimitive,source,true,marker,1)!=std::vector<u32>{10,11,20,21,22,23,24,30,31,32})
            throw std::runtime_error("Single-control-point restart lost a complete patch");
        const std::array partial{10u,11u,marker,20u,21u};
        if(!ExpandGuestPrimitive(P::PatchPrimitive,partial,true,marker,3).empty())
            throw std::runtime_error("Incomplete patches were combined into work");
        const std::array strip{10u,11u,12u,13u,14u,marker,20u,21u,22u,23u};
        if(ExpandGuestPrimitive(P::TriangleStrip,strip,true,marker,0,false)!=
           std::vector<u32>{10,11,12,11,13,12,12,13,14,20,21,22,21,23,22}||
           ExpandGuestPrimitive(P::TriangleStrip,strip,true,marker,0,true)!=
           std::vector<u32>{10,11,12,12,11,13,12,13,14,20,21,22,22,21,23})
            throw std::runtime_error("Strip restart changed provoking vertex, winding or segment parity");
    }
    std::vector<u32> source{77,78,UINT32_MAX};
    for(u32 i=0;i<34;++i)source.push_back(100+i);
    source.push_back(UINT32_MAX);
    for(u32 i=0;i<31;++i)source.push_back(200+i);
    std::vector<u32> expected;for(u32 i=0;i<32;++i)expected.push_back(100+i);
    if(ExpandGuestPrimitive(P::PatchPrimitive,source,true,UINT32_MAX,32)!=expected)
        throw std::runtime_error("Maximum-width patch restart changed complete patch contents");
    for(u32 width:std::array{0u,33u}){
        bool rejected{};try{(void)ExpandGuestPrimitive(P::PatchPrimitive,source,true,UINT32_MAX,width);}
        catch(const std::out_of_range&){rejected=true;}
        if(!rejected)throw std::runtime_error("Patch restart accepted an invalid control point count");
    }
}
inline void CheckGuestIndexExpansionPlan(){
    using P=AmdGpu::PrimitiveType;
    const auto require=[](bool value){if(!value)throw std::runtime_error("GPU index expansion plan regression");};
    for(u32 count=0;count<3;++count)require(!GuestIndexExpansion::Make(P::TriangleFan,count).output_count);
    require(GuestIndexExpansion::Make(P::TriangleFan,7).output_count==15);
    require(GuestIndexExpansion::Make(P::Polygon,7).output_count==15);
    require(GuestIndexExpansion::Make(P::LineLoop,7).output_count==14);
    require(!GuestIndexExpansion::Make(P::LineLoop,1).output_count);
    require(GuestIndexExpansion::Make(P::QuadStrip,7).output_count==12);
    require(!GuestIndexExpansion::Make(P::QuadStrip,3).output_count);
    require(GuestIndexExpansion::Make(P::TriangleStrip,7).output_count==15);
    require(GuestIndexExpansion::Make(P::TriangleStrip,7,true).mode==4);
    require(!GuestIndexExpansion::Make(P::TriangleStrip,2).output_count);
    for(auto type:std::array{P::TriangleFan,P::LineLoop,P::QuadStrip,P::TriangleStrip}){
        bool rejected{};try{(void)GuestIndexExpansion::Make(type,UINT32_MAX);}catch(const std::overflow_error&){rejected=true;}
        require(rejected);
    }
}
inline void CheckGuestIndexExpansionGpu(ID3D12Device* device,Rasterizer& renderer){
    using P=AmdGpu::PrimitiveType;
    const std::array<u32,15> fan{0,1,2,0,2,3,0,3,4,0,4,5,0,5,6};
    const std::array<u32,14> loop{0,1,1,2,2,3,3,4,4,5,5,6,6,0};
    const std::array<u32,12> quad{0,1,2,2,1,3,2,3,4,4,3,5};
    const std::array<u32,15> strip_first{0,1,2,1,3,2,2,3,4,3,5,4,4,5,6};
    const std::array<u32,15> strip_last{0,1,2,2,1,3,2,3,4,4,3,5,4,5,6};
    const std::array<u32,7> values{0xffffffff,0x80001234,17,0,0x7fc08000,6,9};
    const std::array types{P::TriangleFan,P::Polygon,P::LineLoop,P::QuadStrip,P::TriangleStrip,P::TriangleStrip,P::PointList};
    const std::array<u32,7> copied{0,1,2,3,4,5,6};
    for(u32 variant=0;variant<types.size();++variant)
    for(u32 element:std::array{0u,2u,4u})for(u32 available:std::array{0u,4u,7u})for(u32 swap=0;swap<4;++swap){
        if(!element&&(available!=7||swap))continue;
        const auto type=types[variant];
        const auto plan=type==P::PointList?GuestIndexExpansion::Copy(7):GuestIndexExpansion::Make(type,7,variant==5);
        const u64 bytes=u64(plan.output_count)*4,offset=element;
        auto seed=Buffer(device,64+bytes+16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto source=renderer.AcquireTransientBuffer(64),output=renderer.AcquireTransientBuffer(bytes+16);
        auto result=Buffer(device,bytes+16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));
        memset(mapped,0xcd,size_t(64+bytes+16));
        std::array<std::byte,28> encoded{};
        for(u32 i=0;i<7;++i)memcpy(encoded.data()+i*element,&values[i],element);
        constexpr UINT lanes[4][4]{{0,1,2,3},{1,0,3,2},{3,2,1,0},{2,3,0,1}};
        for(u32 byte=0;byte<7*element;++byte){
            const u32 start=byte&~3u,selected=start+4<=7*element?start+lanes[swap][byte%4]:swap==1?(byte^1):byte;
            static_cast<std::byte*>(mapped)[offset+byte]=encoded[selected];
        }
        seed.handle->Unmap(0,nullptr);
        auto* list=renderer.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(source.handle.Get(),0,seed.handle.Get(),0,64);
        list->CopyBufferRegion(output.handle.Get(),0,seed.handle.Get(),64,bytes+16);
        if(available==7)source.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        const auto before=source.State(0);
        {
            GuestIndexExpandPass pass(device);
            if(element){
                bool rejected{};try{pass.Record(renderer,plan,&source,offset,available,element,source);}catch(const std::invalid_argument&){rejected=true;}
                if(!rejected)throw std::runtime_error("Index expansion accepted source/output alias");
                pass.Record(renderer,plan,available?&source:nullptr,available?offset:0,available,element,output,swap);
            }else pass.RecordGenerated(renderer,plan,UINT32_MAX-3,output);
        } // The queued shader/root/source/output survive pass destruction.
        if(source.State(0)!=before)throw std::runtime_error("Index expansion lost authoritative source state");
        // A later original producer can overwrite the guest index alias.
        // Subsequent instances must still consume the one expanded snapshot.
        source.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(source.handle.Get(),0,seed.handle.Get(),64,64);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,bytes+16);renderer.Finish();
        const std::span<const u32> ordinals=type==P::PointList?std::span<const u32>(copied):type==P::LineLoop?std::span<const u32>(loop):
            type==P::QuadStrip?std::span<const u32>(quad):type==P::TriangleStrip?
            (variant==5?std::span<const u32>(strip_last):std::span<const u32>(strip_first)):std::span<const u32>(fan);
        std::vector<std::byte> expected(size_t(bytes+16),std::byte{0xcd});
        for(size_t i=0;i<ordinals.size();++i){const u32 value=!element?(UINT32_MAX-3)+ordinals[i]:
            ordinals[i]<available?(element==2?values[ordinals[i]]&65535:values[ordinals[i]]):0;
            memcpy(expected.data()+i*4,&value,4);}
        D3D12_RANGE all{0,SIZE_T(bytes+16)};Check(result.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),expected.size());result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("GPU index expansion changed topology/order/bits/zero padding/guards");
    }
}
inline void CheckGuestIndexRestartGpu(ID3D12Device* device,Rasterizer& renderer){
    using P=AmdGpu::PrimitiveType;
    const std::array types{P::TriangleFan,P::LineLoop,P::QuadStrip,P::TriangleStrip,P::TriangleStrip,
        P::LineStrip,P::PointList,P::TriangleList,P::LineList,P::QuadList,P::AdjTriangleList,P::PatchPrimitive};
    // Literal primitive sequences, independent of both the CPU decoder and
    // GPU segmented scans. Repeated groups cross128-thread boundaries.
    const std::array<std::vector<u32>,12> short_sequences{{
        {10,11,12,10,12,13,30,31,32},
        {10,11,11,12,12,13,13,10,30,31,31,32,32,30},
        {10,11,12,12,11,13},
        {10,11,12,11,13,12,30,31,32},
        {10,11,12,12,11,13,30,31,32},
        {10,11,11,12,12,13,30,31,31,32},
        {10,11,12,13,20,30,31,32},
        {10,11,12,30,31,32},{10,11,12,13,30,31},{10,11,12,13},{},{10,11,12,30,31,32}}};
    const std::array<std::vector<u32>,12> group{{
        {10,11,12,10,12,13},{10,11,11,12,12,13,13,10},
        {10,11,12,12,11,13},{10,11,12,11,13,12},{10,11,12,12,11,13},
        {10,11,11,12,12,13},{10,11,12,13},{10,11,12},{10,11,12,13},{10,11,12,13},{},{10,11,12}}};
    for(u32 variant=0;variant<types.size();++variant)for(u32 element:{2u,4u})
    for(u32 marker:std::array{0u,99u,element==2?65535u:UINT32_MAX})for(u32 shape=0;shape<3;++shape)for(u32 swap=0;swap<4;++swap){
        std::vector<u32> input{marker,10,11,12,13,marker,20,marker,30,31,32,marker,marker};
        std::vector<u32> expected_indices=short_sequences[variant];
        if(shape==1){input.clear();expected_indices.clear();
            for(UINT n=0;n<65;++n){input.insert(input.end(),{marker,10,11,12,13});
                expected_indices.insert(expected_indices.end(),group[variant].begin(),group[variant].end());}}
        const auto plan=GuestIndexExpansion::MakeRestart(types[variant],UINT(input.size()),3,variant==4);
        const u32 available=shape==2?0:UINT(input.size());
        if(shape==2)expected_indices.assign(marker?plan.output_count:0,0);
        const u64 offset=element,source_bytes=(offset+input.size()*element+3)&~u64(3);
        const u64 output_bytes=u64(plan.output_count)*4+16,total=source_bytes+output_bytes+36;
        auto seed=Buffer(device,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto source=renderer.AcquireTransientBuffer(source_bytes),output=renderer.AcquireTransientBuffer(output_bytes);
        auto arguments=renderer.AcquireTransientBuffer(36);
        const bool capture_packet=variant==0&&element==4&&marker==99&&shape==1&&swap==0;
        const u64 result_bytes=output_bytes+36+(capture_packet?20:0);
        auto result=Buffer(device,result_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        Resource instance_arguments;
        void* bytes{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&bytes));memset(bytes,0xcd,size_t(total));
        std::vector<std::byte> encoded(input.size()*element);
        for(size_t i=0;i<input.size();++i)memcpy(encoded.data()+i*element,&input[i],element);
        constexpr UINT lanes[4][4]{{0,1,2,3},{1,0,3,2},{3,2,1,0},{2,3,0,1}};
        for(size_t byte=0;byte<encoded.size();++byte){
            const auto start=byte&~size_t(3);const auto lane=byte%4;
            const auto selected=start+4<=encoded.size()?start+lanes[swap][lane]:
                swap==1?(byte^1):byte;
            static_cast<std::byte*>(bytes)[offset+byte]=encoded[selected];
        }
        seed.handle->Unmap(0,nullptr);
        renderer.RetainUntilFinish([seed,source,output,arguments,result](u64){});
        auto* list=renderer.RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(source.handle.Get(),0,seed.handle.Get(),0,source_bytes);
        list->CopyBufferRegion(output.handle.Get(),0,seed.handle.Get(),source_bytes,output_bytes);
        list->CopyBufferRegion(arguments.handle.Get(),0,seed.handle.Get(),source_bytes+output_bytes,36);
        source.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);const auto previous=source.State(0);
        {
            GuestIndexExpandPass pass(device);const Rasterizer::DrawArguments draw{plan.input_count,3,0,11,-7};
            bool rejected{};try{pass.RecordRestart(renderer,plan,&source,offset,available,element,marker,draw,source,arguments);}
            catch(const std::invalid_argument&){rejected=true;}
            if(!rejected)throw std::runtime_error("Segmented expansion accepted aliased source/output");
            rejected=false;try{pass.RecordRestart(renderer,plan,&source,offset,available,element,marker,draw,output,output);}
            catch(const std::invalid_argument&){rejected=true;}
            if(!rejected)throw std::runtime_error("Segmented expansion accepted aliased output/arguments");
            pass.RecordRestart(renderer,plan,available?&source:nullptr,available?offset:0,available,element,marker,draw,output,arguments,swap);
            if(capture_packet){
                bool rejected{};try{(void)pass.InstanceArguments(renderer,arguments,{plan.input_count,2});}
                catch(const std::invalid_argument&){rejected=true;}
                if(!rejected)throw std::runtime_error("Capture packet accepted multiple original instances");
                instance_arguments=pass.InstanceArguments(renderer,arguments,{plan.input_count,1,0,37,-19});
                if(instance_arguments.State(0)!=D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
                    throw std::runtime_error("Capture packet did not publish its consumer state");
            }
        }
        if(source.State(0)!=previous)throw std::runtime_error("Segmented expansion changed source state");
        if(arguments.State(0)!=D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT||output.State(0)!=D3D12_RESOURCE_STATE_INDEX_BUFFER)
            throw std::runtime_error("Segmented expansion failed to publish native consumer states");
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,output_bytes);
        list->CopyBufferRegion(result.handle.Get(),output_bytes,arguments.handle.Get(),0,36);
        if(capture_packet){instance_arguments.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->CopyBufferRegion(result.handle.Get(),output_bytes+36,instance_arguments.handle.Get(),0,20);}
        renderer.Finish();std::vector<std::byte> expected(size_t(result_bytes),std::byte{0xcd});
        if(!expected_indices.empty())memcpy(expected.data(),expected_indices.data(),expected_indices.size()*4);
        const u32 args[]{UINT(expected_indices.size()),3,0,u32(-7),11};memcpy(expected.data()+output_bytes,args,sizeof(args));
        if(capture_packet){const u32 one[]{UINT(expected_indices.size()),1,0,u32(-19),37};
            memcpy(expected.data()+output_bytes+36,one,sizeof(one));}
        D3D12_RANGE range{0,SIZE_T(result_bytes)};Check(result.handle->Map(0,&range,&bytes));
        const bool equal=memcmp(bytes,expected.data(),expected.size())==0;result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("Segmented expansion changed restart boundaries, order, provoking, count, arguments or guards");
    }
}

}
