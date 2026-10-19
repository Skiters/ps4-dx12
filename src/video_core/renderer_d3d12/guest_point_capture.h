// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_graphics_state.h"
#include "rasterizer.h"
#include "shader_recompiler/native_point_capture.h"
#include <functional>
#include <memory>
namespace NativeDX12 {
class Rasterizer;
class ResourceBindings;
class NativePointCapturePass {
    ComPtr<ID3D12RootSignature> clear_root_;
    ComPtr<ID3D12PipelineState> clear_pipeline_;
    std::vector<std::shared_ptr<Resource>> buffers_;
    std::shared_ptr<Resource> Acquire(Rasterizer&,u64);
public:
    using DrawArguments=Rasterizer::DrawArguments;
    explicit NativePointCapturePass(ID3D12Device*);
    static u32 PrimitiveBound(D3D_PRIMITIVE_TOPOLOGY,u32 vertices);
    static GuestGraphicsState ProducerState(const GuestGraphicsState&);
    static void BindProducer(ResourceBindings&,Resource&,u64 offset,u64 bytes);
    static void BindReplay(ResourceBindings&,Resource&,u64 bytes);
    // SRV view starts at a complete primitive boundary; GS PrimitiveId is
    // relative to this slice, while record word zero retains the guest ID.
    static void BindInputStream(ResourceBindings&,Resource&,u64 offset,u64 bytes);
    // All original instances run once with disjoint UAV views, then one replay
    // draw preserves record order and clears attachments only once.
    void Execute(Rasterizer&,const Shader::NativePointCapture::Plan&,const DrawArguments&,D3D_PRIMITIVE_TOPOLOGY,
        const std::function<void(Resource&,const DrawArguments&,u64,u64)>& producer,
        const std::function<void(Resource&,const DrawArguments&)>& replay);
};
}
