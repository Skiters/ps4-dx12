// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_graphics_runtime.h"
#include "rasterizer.h"
#include "guest_indirect_arguments.h"
#include "video_core/amdgpu/regs_vertex.h"
#include <algorithm>
namespace NativeDX12 {
struct GuestIndirectPackPlan {
    bool indexed,with_runtime,index_views;
    u32 count,source_stride,command_stride,runtime_stride;
    u64 source_bytes,command_bytes,runtime_bytes;
    static GuestIndirectPackPlan Make(bool indexed,u32 stride,u32 count,bool with_runtime=true,bool index_views=false){
        if(!count)throw std::invalid_argument("Indirect packing needs a nonzero maximum count");
        const auto source=GuestIndirectLayout::Make(indexed,0,0,stride,count);
        const auto native=NativeIndirectLayout::Make(indexed,0,0,count,with_runtime,index_views);
        const u32 runtime_stride=with_runtime?(sizeof(GraphicsRuntime)+255)&~255u:0;
        const u64 runtime_bytes=u64(runtime_stride)*count;
        if(source.bytes>UINT32_MAX||native.bytes>UINT32_MAX||runtime_bytes>UINT32_MAX)
            throw std::overflow_error("Indirect pack exceeds raw-buffer shader addressing");
        return {indexed,with_runtime,index_views,count,stride,native.stride,runtime_stride,source.bytes,native.bytes,runtime_bytes};
    }
    bool operator==(const GuestIndirectPackPlan&)const=default;
};
// Inputs are already prepared/coherent native resources. This snapshots GPU
// arguments before guest draw effects, with no CPU argument/count readback.
// Caller owns private outputs and retains them through the final indirect draw.
struct GuestIndirectIndexDecode {
    Resource* source{};u64 offset{};u32 available{},swap_mode{};
    Resource* output{};
    static u64 OutputBytes(u32 available,u32 count){
        const u64 stride=u64(std::max(1u,available))*4;
        if(count&&stride>UINT64_MAX/count)throw std::overflow_error("Indirect index arena size overflow");
        return stride*count;
    }
    static bool Admit(AmdGpu::IndexType type,AmdGpu::IndexSwapMode swap,bool restart,u32 available,u32 count){
        // Bound speculative per-command scratch. Larger workloads retain the
        // complete per-command path instead of truncating guest operations.
        return !restart&&type==AmdGpu::IndexType::Index16&&
            (swap==AmdGpu::IndexSwapMode::Swap32||swap==AmdGpu::IndexSwapMode::SwapWord)&&
            count&&u64(std::max(1u,available))*count<=16ull*1024*1024;
    }
};
class GuestIndirectPackPass {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12PipelineState> dispatch_pipeline_;
    ComPtr<ID3D12CommandSignature> dispatch_signature_;
    ComPtr<ID3D12PipelineState> index_pipeline_;
    Resource dispatch_;
    Resource null_template_;
public:
    explicit GuestIndirectPackPass(ID3D12Device*);
    void Record(Rasterizer&,const GuestIndirectPackPlan&,Resource& source,u64 source_offset,
        Resource* count,u64 count_offset,const GraphicsRuntime&,
        Resource& commands,Resource* constants,Resource& output_count,const GuestIndirectIndexDecode* decode=nullptr);
    void Record(Rasterizer& renderer,const GuestIndirectPackPlan& plan,Resource& source,u64 source_offset,
        Resource* count,u64 count_offset,const GraphicsRuntime& runtime,
        Resource& commands,Resource& constants,Resource& output_count){
        Record(renderer,plan,source,source_offset,count,count_offset,runtime,commands,&constants,output_count);
    }
};
} // namespace NativeDX12
