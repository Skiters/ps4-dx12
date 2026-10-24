// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "video_core/amdgpu/regs_vertex.h"
namespace NativeDX12 {
struct GuestIndexExpansion {
    u32 mode,input_count,output_count;
    static GuestIndexExpansion Make(AmdGpu::PrimitiveType type,u32 count,bool last_provoking=false){
        using P=AmdGpu::PrimitiveType;
        u32 mode{};u64 output{};
        switch(type){
        case P::TriangleFan:case P::Polygon:mode=0;output=count>2?u64(count-2)*3:0;break;
        case P::LineLoop:mode=1;output=count>1?u64(count)*2:0;break;
        case P::QuadStrip:mode=2;output=count>3?u64((count-2)/2)*6:0;break;
        case P::TriangleStrip:mode=last_provoking?4:3;output=count>2?u64(count-2)*3:0;break;
        default:throw std::invalid_argument("Unsupported GPU index expansion topology");
        }
        if(output>UINT32_MAX/4)throw std::overflow_error("Expanded GPU index view exceeds native addressing");
        return {mode,count,u32(output)};
    }
};
class GuestIndexExpandPass {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    Resource zero_;
    void RecordInternal(Rasterizer&,const GuestIndexExpansion&,Resource*,u64,u32,u32,Resource&,bool,u32);
public:
    explicit GuestIndexExpandPass(ID3D12Device*);
    // Source is the authoritative prepared B wrapper; missing source elements
    // are zero, matching the complete CPU decoder's padded index vector.
    void Record(Rasterizer&,const GuestIndexExpansion&,Resource* source,u64 offset,
        u32 available,u32 element,Resource& output);
    void RecordGenerated(Rasterizer&,const GuestIndexExpansion&,u32 first,Resource& output);
};
}
