// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_shader_cache.h"
#include "shader_pipeline.h"
#include <memory>
namespace NativeDX12 {
// GPU-command-thread owner. The frontend/cache must outlive this object. The
// caller translates fixed guest raster/depth/blend/vertex state into `state`.
class GuestPipelineCache {
    ComPtr<ID3D12Device> device_;
    std::wstring dxc_;
    GuestShaderFrontend& frontend_;
    GuestShaderCache& shaders_;
    std::map<u64,std::unique_ptr<ShaderPipeline>> compute_;
    std::map<std::string,std::unique_ptr<ShaderPipeline>> graphics_;
public:
    GuestPipelineCache(ID3D12Device* device,std::wstring dxc,
                       GuestShaderFrontend& frontend,GuestShaderCache& shaders)
        :device_(device),dxc_(std::move(dxc)),frontend_(frontend),shaders_(shaders) {}
    const ShaderPipeline& Compute(AmdGpu::Liverpool&);
    const ShaderPipeline& Compute(const GuestShaderStage&);
    const ShaderPipeline& Graphics(AmdGpu::Liverpool&,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,
        u16 vertex_sgpr_offset=0,u32 instance_sgpr_offset=0);
    const ShaderPipeline& Graphics(const GuestGraphicsStages&,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds=false,
        ShaderPipeline::StencilFaces stencil={});
};
}
