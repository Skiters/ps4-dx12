// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_dma.h"
#include "video_core/renderer_d3d12/compute_pipeline.h"
#include "common/scope_exit.h"
#include "image_source_residency_fixture.h"
#include <cstring>

namespace NativeDX12 {
// Prepared for the final hardware suite. Compilation does not execute this.
template<class Adapter>
void RunSemanticBufferResidencyFixture(ID3D12Device* device){
    namespace Dma=Shader::NativeDma;
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    constexpr u64 bytes=2*u64(Dma::PageSize);
    auto* physical=static_cast<std::byte*>(VirtualAlloc(nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(physical,"Cannot allocate semantic buffer residency fixture");
    SCOPE_EXIT {VirtualFree(physical,0,MEM_RELEASE);};
    memset(physical,0x31,size_t(bytes));const auto base=reinterpret_cast<VAddr>(physical);
    Adapter renderer(device);renderer.MapMemory(base,bytes);renderer.NotifyHostMapping(base,bytes,true,true);
    renderer.SetBufferBackingAccess([=](VAddr a,u64 n){return a>=base&&a-base<=bytes&&n<=bytes-(a-base);},
        [=](VAddr a,std::span<const std::byte> data){memcpy(physical+a-base,data.data(),data.size());return true;});
    renderer.PhysicalReader([=](VAddr a,std::span<std::byte> out){
        if(a<base||a-base>bytes||out.size()>bytes-(a-base))return false;
        memcpy(out.data(),physical+a-base,out.size());return true;
    });
    auto& dma=renderer.DmaMemory();
    Rasterizer::ShaderBufferRequest reserve{base,bytes,false,true,true};reserve.publish_dma_residency=false;
    renderer.PrepareShaderBuffers(std::span(&reserve,1));
    (void)renderer.CaptureShaderBuffer(base,bytes,false);
    require(dma.ResidentIntersections(base,bytes).empty(),"Private reservation/capture published BDA pages");
    auto upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* data{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&data));memset(data,0x41,size_t(bytes));
    upload.handle->Unmap(0,nullptr);
    auto tiled=UavBuffer(device,bytes);tiled.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
    renderer.RetainUntilFinish([upload,tiled](u64){});
    renderer.RecordingList()->CopyBufferRegion(tiled.handle.Get(),0,upload.handle.Get(),0,bytes);
    const auto copy_and_read=[&]{
        renderer.CopyResidentImageToPreparedShaderBuffer(base,bytes,tiled,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        auto snapshot=renderer.CapturePreparedShaderBuffer(base,bytes);
        auto rb=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        snapshot.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        renderer.RecordingList()->CopyBufferRegion(rb.handle.Get(),0,snapshot.handle.Get(),0,bytes);renderer.Finish();
        void* mapped{};D3D12_RANGE range{0,size_t(bytes)};Check(rb.handle->Map(0,&range,&mapped));
        std::vector<std::byte> result(static_cast<size_t>(bytes));memcpy(result.data(),mapped,size_t(bytes));
        rb.handle->Unmap(0,&empty);return result;
    };
    const Rasterizer::ShaderBufferRequest first{base+4,4,false,true,true};
    dma.ObserveResidentSharps(std::span(&first,1));
    auto result=copy_and_read();
    for(u64 i=0;i<bytes;++i)require(result[i]==(i<Dma::PageSize?std::byte{0x41}:std::byte{0x31}),
        "Earlier retile populated a later descriptor's unresident B page");
    dma.ObserveResidentSharps(std::span(&reserve,1));
    const std::vector<std::pair<VAddr,u64>> first_page{{base,Dma::PageSize}};
    require(dma.ResidentIntersections(base,bytes)==first_page,"Private reservation erased existing residency");
    const Rasterizer::ShaderBufferRequest second{base+Dma::PageSize,4,false,true,true};
    dma.ObserveResidentSharps(std::span(&second,1));
    const std::vector<std::pair<VAddr,u64>> merged{{base+Dma::PageSize-4,8}};
    require(dma.ResidentIntersections(base+Dma::PageSize-4,8)==merged,"Resident clipping lost original byte bounds");
    result=copy_and_read();
    for(const auto value:result)require(value==std::byte{0x41},"Later resident retile omitted a newly bound page");
    // 64-bit native fixture addresses remain usable by I/B without entering
    // the 40-bit PS4 table. The highest page must not wrap at its upper end.
    GuestDmaMemory high(nullptr);
    const Rasterizer::ShaderBufferRequest top{UINT64_MAX-7,4,false,true,true};
    high.ObserveResidentSharps(std::span(&top,1));
    const std::vector<std::pair<VAddr,u64>> clipped{{UINT64_MAX-7,4}};
    require(high.ResidentIntersections(UINT64_MAX-7,4)==clipped,"High native residency wrapped or was truncated to PS4 VA");
    bool rejected=false;try{(void)high.ResidentIntersections(UINT64_MAX-3,8);}catch(const std::out_of_range&){rejected=true;}
    require(rejected,"Resident intersection accepted an overflowing native range");
    RunImageSourceResidencyFixture(device,renderer,base);
    renderer.ValidateDebugMessages();
}
}
