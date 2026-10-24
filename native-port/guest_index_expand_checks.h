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
    for(auto type:std::array{P::TriangleFan,P::LineLoop,P::QuadStrip}){
        bool rejected{};try{(void)GuestIndexExpansion::Make(type,UINT32_MAX);}catch(const std::overflow_error&){rejected=true;}
        require(rejected);
    }
}
inline void CheckGuestIndexExpansionGpu(ID3D12Device* device,Rasterizer& renderer){
    using P=AmdGpu::PrimitiveType;
    const std::array<u32,15> fan{0,1,2,0,2,3,0,3,4,0,4,5,0,5,6};
    const std::array<u32,14> loop{0,1,1,2,2,3,3,4,4,5,5,6,6,0};
    const std::array<u32,12> quad{0,1,2,2,1,3,2,3,4,4,3,5};
    const std::array<u32,7> values{0xffffffff,0x80001234,17,0,0x7fc08000,6,9};
    for(auto type:std::array{P::TriangleFan,P::Polygon,P::LineLoop,P::QuadStrip})
    for(u32 element:std::array{0u,2u,4u})for(u32 available:std::array{0u,4u,7u}){
        if(!element&&available!=7)continue;
        const auto plan=GuestIndexExpansion::Make(type,7);
        const u64 bytes=u64(plan.output_count)*4,offset=element;
        auto seed=Buffer(device,64+bytes+16,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto source=renderer.AcquireTransientBuffer(64),output=renderer.AcquireTransientBuffer(bytes+16);
        auto result=Buffer(device,bytes+16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE none{};Check(seed.handle->Map(0,&none,&mapped));
        memset(mapped,0xcd,size_t(64+bytes+16));
        for(u32 i=0;i<7;++i)memcpy(static_cast<std::byte*>(mapped)+offset+i*element,&values[i],element);
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
                pass.Record(renderer,plan,available?&source:nullptr,available?offset:0,available,element,output);
            }else pass.RecordGenerated(renderer,plan,UINT32_MAX-3,output);
        } // The queued shader/root/source/output survive pass destruction.
        if(source.State(0)!=before)throw std::runtime_error("Index expansion lost authoritative source state");
        // A later original producer can overwrite the guest index alias.
        // Subsequent instances must still consume the one expanded snapshot.
        source.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        list->CopyBufferRegion(source.handle.Get(),0,seed.handle.Get(),64,64);
        output.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->CopyBufferRegion(result.handle.Get(),0,output.handle.Get(),0,bytes+16);renderer.Finish();
        const std::span<const u32> ordinals=type==P::LineLoop?std::span<const u32>(loop):
            type==P::QuadStrip?std::span<const u32>(quad):std::span<const u32>(fan);
        std::vector<std::byte> expected(size_t(bytes+16),std::byte{0xcd});
        for(size_t i=0;i<ordinals.size();++i){const u32 value=!element?(UINT32_MAX-3)+ordinals[i]:
            ordinals[i]<available?(element==2?values[ordinals[i]]&65535:values[ordinals[i]]):0;
            memcpy(expected.data()+i*4,&value,4);}
        D3D12_RANGE all{0,SIZE_T(bytes+16)};Check(result.handle->Map(0,&all,&mapped));
        const bool equal=!memcmp(mapped,expected.data(),expected.size());result.handle->Unmap(0,&none);
        if(!equal)throw std::runtime_error("GPU index expansion changed topology/order/bits/zero padding/guards");
    }
}
}
