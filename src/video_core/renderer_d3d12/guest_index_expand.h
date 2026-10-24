// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "video_core/amdgpu/regs_vertex.h"
namespace NativeDX12 {
struct GuestIndexExpansion {
    u32 mode,input_count,output_count;
    u32 group_width{};
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
    static bool SupportsRestart(AmdGpu::PrimitiveType type){
        using P=AmdGpu::PrimitiveType;
        switch(type){
        case P::TriangleFan:case P::Polygon:case P::LineLoop:case P::QuadStrip:case P::TriangleStrip:
        case P::LineStrip:case P::PointList:case P::TriangleList:case P::RectList:case P::LineList:
        case P::QuadList:case P::AdjLineList:case P::AdjTriangleList:case P::PatchPrimitive:return true;
        default:return false;
        }
    }
    static GuestIndexExpansion MakeRestart(AmdGpu::PrimitiveType type,u32 count,u32 patch_points=0,bool last_provoking=false){
        using P=AmdGpu::PrimitiveType;u32 width{};
        switch(type){
        case P::LineStrip:
            if(count>UINT32_MAX/8)throw std::overflow_error("Restart line indices exceed native addressing");
            return {5,count,count>1?(count-1)*2:0,0};
        case P::PointList:width=1;break;
        case P::TriangleList:case P::RectList:width=3;break;
        case P::LineList:width=2;break;
        case P::QuadList:case P::AdjLineList:width=4;break;
        case P::AdjTriangleList:width=6;break;
        case P::PatchPrimitive:
            if(!patch_points||patch_points>32)throw std::out_of_range("Restart patch width must be1..32");
            width=patch_points;break;
        default:return Make(type,count,last_provoking);
        }
        if(count>UINT32_MAX/8)throw std::overflow_error("Restart scan exceeds native addressing");
        return {6,count,count/width*width,width};
    }
};
class GuestIndexExpandPass {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    Resource zero_;
    ComPtr<ID3D12RootSignature> restart_root_;
    ComPtr<ID3D12PipelineState> restart_pipeline_;
    void RecordInternal(Rasterizer&,const GuestIndexExpansion&,Resource*,u64,u32,u32,Resource&,bool,u32);
public:
    explicit GuestIndexExpandPass(ID3D12Device*);
    // Source is the authoritative prepared B wrapper; missing source elements
    // are zero, matching the complete CPU decoder's padded index vector.
    void Record(Rasterizer&,const GuestIndexExpansion&,Resource* source,u64 offset,
        u32 available,u32 element,Resource& output);
    void RecordGenerated(Rasterizer&,const GuestIndexExpansion&,u32 first,Resource& output);
    // Stable segmented GPU expansion. Output count is written as DRAW_INDEXED
    // arguments; no host readback or dummy primitives are used.
    void RecordRestart(Rasterizer&,const GuestIndexExpansion&,Resource* source,u64 offset,
        u32 available,u32 element,u32 marker,const Rasterizer::DrawArguments&,
        Resource& output,Resource& arguments);
};
}
