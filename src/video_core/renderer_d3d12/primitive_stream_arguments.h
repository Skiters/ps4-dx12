// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include "resources.h"
#include "stream_output_layout.h"
#include "shader_recompiler/native_point_capture.h"
#include <span>
#include <string>
#include <functional>
#include <memory>
namespace NativeDX12 {
class Rasterizer;
StreamOutputLayout ReflectPrimitiveStreamLayout(const std::wstring& dxc,std::span<const u8> bytecode);
// A bounded slice of a host-internal, fully captured primitive stream. Caller
// must cover the complete stream with ordered slices and prevent SO overflow.
struct PrimitiveStreamRange {
    u32 stride{},capacity_vertices{},vertices_per_primitive{},first{},limit{};
    static PrimitiveStreamRange Make(u32 stride,u32 capacity,u32 vertices_per_primitive,u32 first,u32 limit);
};
PrimitiveStreamRange AuxiliaryPointStreamRange(u32 patch_vertices,u32 input_vertices,u32 instances,u32 vertex_words);
// Conservative complete source-SO bound for arbitrary hardware tessellation,
// distinct from the auxiliary two-triangles-per-patch construction.
PrimitiveStreamRange TessellatedPointStreamRange(u32 patch_vertices,u32 input_vertices,u32 instances,
    u32 vertex_words,u32 vertices_per_primitive);
struct PrimitiveStreamPartition {
    PrimitiveStreamRange input;
    Shader::NativePointCapture::Plan capture;
    bool first{},last{};
};
// Lazy complete coverage: the budget limits one capture arena, never the
// number of original primitives replayed. All boundaries are primitive aligned.
class PrimitiveStreamPartitions {
    PrimitiveStreamRange input_;
    u32 calls_,emissions_,vertices_,count_;
public:
    PrimitiveStreamPartitions(const PrimitiveStreamRange&,u32 calls,u32 emissions,u64 capture_byte_budget);
    u32 Count()const{return count_;}
    PrimitiveStreamPartition At(u32)const;
};
class PrimitiveStreamArguments {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12PipelineState> partition_pipeline_;
    void RecordCount(Rasterizer&,Resource&,Resource&,u32 divisor,u32 first,u32 limit,u32 multiplier,u32 primitive_width=0);
public:
    explicit PrimitiveStreamArguments(ID3D12Device*);
    void Record(Rasterizer&,Resource& filled_size,Resource& arguments,const PrimitiveStreamRange&);
    void RecordPartition(Rasterizer&,Resource& filled_size,Resource& arguments,const PrimitiveStreamPartition&);
};
struct PrimitiveStreamLease {
    // DRAW packets at0/16 and clear DISPATCH at32. One wrapper owns the
    // shared state/lifetime of all indirect consumers.
    Resource data,filled_size,arguments;
    PrimitiveStreamRange range;
};
// Captures a producer once, then builds an authoritative GPU draw count.
// Caller proves the complete amplification bound before Capture; overflow is
// never a workload truncation policy. The guest frontend remains guarded
// until that proof and the complete replay scheduling are connected.
class PrimitiveStreamPass {
    PrimitiveStreamArguments generator_;
    Resource zero_;
    std::vector<std::shared_ptr<PrimitiveStreamLease>> arenas_;
public:
    explicit PrimitiveStreamPass(ID3D12Device*);
    std::shared_ptr<PrimitiveStreamLease> Capture(Rasterizer&,u32 vertex_stride,u32 capacity_vertices,
        u32 vertices_per_primitive,const std::function<void(PrimitiveStreamLease&)>& producer);
    // Consumer binds input SRV at input.first*stride and uses generated GPU
    // arguments. Its capture addresses are local to that slice. Only first
    // may perform attachment beginning actions; every slice must be consumed.
    void Replay(Rasterizer&,const std::shared_ptr<PrimitiveStreamLease>&,u32 calls,u32 emissions,u64 capture_byte_budget,
        const std::function<void(PrimitiveStreamLease&,const PrimitiveStreamPartition&)>& consumer);
};
} // namespace NativeDX12
