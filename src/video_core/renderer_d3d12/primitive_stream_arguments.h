// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include "resources.h"
#include "stream_output_layout.h"
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
class PrimitiveStreamArguments {
    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
public:
    explicit PrimitiveStreamArguments(ID3D12Device*);
    void Record(Rasterizer&,Resource& filled_size,Resource& arguments,const PrimitiveStreamRange&);
};
struct PrimitiveStreamLease {
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
};
} // namespace NativeDX12
