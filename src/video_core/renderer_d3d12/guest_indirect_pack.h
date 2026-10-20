// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_graphics_runtime.h"
#include "rasterizer.h"
#include "guest_indirect_arguments.h"
namespace NativeDX12 {
struct GuestIndirectPackPlan {
    bool indexed;
    u32 count,source_stride,command_stride,runtime_stride;
    u64 source_bytes,command_bytes,runtime_bytes;
    static GuestIndirectPackPlan Make(bool indexed,u32 stride,u32 count){
        if(!count)throw std::invalid_argument("Indirect packing needs a nonzero maximum count");
        const auto source=GuestIndirectLayout::Make(indexed,0,0,stride,count);
        const auto native=NativeIndirectLayout::Make(indexed,0,0,count,true);
        constexpr u32 runtime_stride=(sizeof(GraphicsRuntime)+255)&~255u;
        const u64 runtime_bytes=u64(runtime_stride)*count;
        if(source.bytes>UINT32_MAX||native.bytes>UINT32_MAX||runtime_bytes>UINT32_MAX)
            throw std::overflow_error("Indirect pack exceeds raw-buffer shader addressing");
        return {indexed,count,stride,native.stride,runtime_stride,source.bytes,native.bytes,runtime_bytes};
    }
    bool operator==(const GuestIndirectPackPlan&)const=default;
};
// Inputs are already prepared/coherent native resources. This snapshots GPU
// arguments before guest draw effects, with no CPU argument/count readback.
// Caller owns private outputs and retains them through the final indirect draw.
class GuestIndirectPackPass {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12PipelineState> dispatch_pipeline_;
    ComPtr<ID3D12CommandSignature> dispatch_signature_;
    Resource dispatch_;
public:
    explicit GuestIndirectPackPass(ID3D12Device*);
    void Record(Rasterizer&,const GuestIndirectPackPlan&,Resource& source,u64 source_offset,
        Resource* count,u64 count_offset,const GraphicsRuntime&,
        Resource& commands,Resource& constants,Resource& output_count);
};
} // namespace NativeDX12
