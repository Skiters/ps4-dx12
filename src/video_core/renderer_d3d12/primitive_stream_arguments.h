// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include "resources.h"
#include "stream_output_layout.h"
#include <span>
#include <string>
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
} // namespace NativeDX12
