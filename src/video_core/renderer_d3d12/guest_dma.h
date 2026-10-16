// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "shader_recompiler/native_dma.h"
#include "dma_fault_compactor.h"
namespace NativeDX12 {
// DMA uses views of the ordinary buffer cache, never an independent copy of
// guest data. Callers hold ExecutionMutex through preparation and submission.
class GuestDmaMemory {
    ComPtr<ID3D12Device> device_;
    Resource table_,faults_,zero_faults_,exported_;
    std::unique_ptr<DmaFaultCompactor> compactor_;
    struct Feedback {Resource readback;bool pending{},ready{};};
    std::array<std::shared_ptr<Feedback>,4> feedback_;
    std::set<u32> pages_;
    std::map<u32,u64> entries_;
    std::vector<Rasterizer::ShaderBufferRequest> ranges_;
    struct Translation {u32 entry,bytes;size_t range;u64 offset;};
    std::vector<Translation> translations_;
    std::map<u32,u64> indirect_entries_;
    u32 required_entries_=Shader::NativeDma::TableEntries;
    void Initialize(Rasterizer&);
public:
    explicit GuestDmaMemory(ID3D12Device* device):device_(device){}
    u64 PageTableBytes()const{return table_.handle?table_.handle->GetDesc().Width:0;}
    void AppendRequests(Rasterizer&,std::vector<Rasterizer::ShaderBufferRequest>&);
    void Bind(Rasterizer&,ResourceBindings&,std::span<const Rasterizer::ShaderBufferView>);
    void BindMetadata(ResourceBindings&,UINT reg,bool fault);
    void RecordFaults(Rasterizer&);
    void ConsumeFaults(Rasterizer&);
};
}
