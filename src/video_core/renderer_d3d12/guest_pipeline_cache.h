// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_shader_cache.h"
#include "shader_pipeline.h"
#include "stream_output_layout.h"
#include <memory>
#include <cstdint>
namespace NativeDX12 {
// GPU-command-thread owner. The frontend/cache must outlive this object. The
// caller translates fixed guest raster/depth/blend/vertex state into `state`.
class GuestPipelineCache {
    ComPtr<ID3D12Device> device_;
    std::wstring dxc_;
    GuestShaderFrontend& frontend_;
    GuestShaderCache& shaders_;
    std::map<u64,std::unique_ptr<ShaderPipeline>> compute_;
    struct GraphicsRecord {
        GuestGraphicsStages guest;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
        std::vector<std::string> semantics;
        StreamOutputLayout stream_output;
        bool depth_bounds{};
        bool root_graphics_runtime{};
        ShaderPipeline::StencilFaces stencil{};
        std::unique_ptr<ShaderPipeline> pipeline;
        GraphicsRecord(const GuestGraphicsStages&,const D3D12_GRAPHICS_PIPELINE_STATE_DESC&,bool,ShaderPipeline::StencilFaces,bool);
    };
    std::map<std::string,std::unique_ptr<GraphicsRecord>> graphics_;
    struct LogicalLayoutRecord {
        std::vector<ComPtr<ID3D12RootSignature>> roots;
        std::shared_ptr<ResourceLayout> layout;
    };
    std::map<std::vector<std::uintptr_t>,LogicalLayoutRecord> logical_layouts_;
    std::unique_ptr<ShaderPipeline> BuildGraphics(const GuestGraphicsStages&,const D3D12_GRAPHICS_PIPELINE_STATE_DESC&,bool,ShaderPipeline::StencilFaces,bool);
#ifdef SHADPS4_NATIVE_DX12_CORE
    bool warming_{};
    void RegisterPipelineData(const GuestShaderStage&);
    void RegisterPipelineData(const GuestGraphicsStages&,const D3D12_GRAPHICS_PIPELINE_STATE_DESC&,
        bool,ShaderPipeline::StencilFaces,bool);
#endif
public:
    std::optional<u64> ReplaceShader(u64,std::span<const u32>,bool);
    GuestPipelineCache(ID3D12Device* device,std::wstring dxc,
                       GuestShaderFrontend& frontend,GuestShaderCache& shaders)
        :device_(device),dxc_(std::move(dxc)),frontend_(frontend),shaders_(shaders) {
#ifdef SHADPS4_NATIVE_DX12_CORE
            WarmUp();
#endif
        }
#ifdef SHADPS4_NATIVE_DX12_CORE
    ~GuestPipelineCache();
    void WarmUp();
    void Sync();
#endif
    const ShaderPipeline& Compute(AmdGpu::Liverpool&);
    const ShaderPipeline& Compute(const GuestShaderStage&);
    StreamOutputLayout PrimitiveStreamLayout(const GuestGraphicsStages&);
    std::shared_ptr<ResourceLayout> LogicalLayout(std::span<const ShaderPipeline* const> domains);
    size_t GraphicsCount()const{return graphics_.size();}
    const ShaderPipeline& Graphics(AmdGpu::Liverpool&,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,
        u16 vertex_sgpr_offset=0,u32 instance_sgpr_offset=0);
    const ShaderPipeline& Graphics(const GuestGraphicsStages&,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds=false,
        ShaderPipeline::StencilFaces stencil={},bool root_graphics_runtime=false);
};
}
