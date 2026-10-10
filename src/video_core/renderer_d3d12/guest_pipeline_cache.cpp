// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_pipeline_cache.h"
#include "diagnostics.h"
namespace NativeDX12 {
namespace {
// Serialize semantic fields, never host pointer values or struct padding.
// This preserves pipeline reuse across draws with different temporary layouts.
struct Key {
    std::string data;
    template<class T>void Add(T value) {data.append(reinterpret_cast<const char*>(&value),sizeof(value));}
    void Text(const char* value) {
        if(!value)throw std::invalid_argument("Null native vertex semantic");
        auto size=std::char_traits<char>::length(value);Add(size);data.append(value,size);
    }
};
std::string GraphicsKey(const GuestGraphicsStages& guest,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    if(d.StreamOutput.NumEntries||d.StreamOutput.NumStrides)
        throw std::invalid_argument("Guest stream output is not connected to native D3D12 yet");
    if(d.InputLayout.NumElements&&!d.InputLayout.pInputElementDescs)
        throw std::invalid_argument("Null native vertex input layout");
    Key k;for(auto* stage:guest.stages)k.Add(stage?stage->id:u64(0));
    k.Add(d.BlendState.AlphaToCoverageEnable);k.Add(d.BlendState.IndependentBlendEnable);
    for(const auto& r:d.BlendState.RenderTarget) {
        k.Add(r.BlendEnable);k.Add(r.LogicOpEnable);k.Add(r.SrcBlend);k.Add(r.DestBlend);k.Add(r.BlendOp);
        k.Add(r.SrcBlendAlpha);k.Add(r.DestBlendAlpha);k.Add(r.BlendOpAlpha);k.Add(r.LogicOp);k.Add(r.RenderTargetWriteMask);
    }
    k.Add(d.SampleMask);const auto& r=d.RasterizerState;
    k.Add(r.FillMode);k.Add(r.CullMode);k.Add(r.FrontCounterClockwise);k.Add(r.DepthBias);
    k.Add(r.DepthBiasClamp);k.Add(r.SlopeScaledDepthBias);k.Add(r.DepthClipEnable);k.Add(r.MultisampleEnable);
    k.Add(r.AntialiasedLineEnable);k.Add(r.ForcedSampleCount);k.Add(r.ConservativeRaster);
    const auto& z=d.DepthStencilState;k.Add(z.DepthEnable);k.Add(z.DepthWriteMask);k.Add(z.DepthFunc);
    k.Add(z.StencilEnable);k.Add(z.StencilReadMask);k.Add(z.StencilWriteMask);
    for(auto face:{z.FrontFace,z.BackFace}) {
        k.Add(face.StencilFailOp);k.Add(face.StencilDepthFailOp);k.Add(face.StencilPassOp);k.Add(face.StencilFunc);
    }
    k.Add(d.InputLayout.NumElements);
    for(UINT i=0;i<d.InputLayout.NumElements;++i) {
        const auto& e=d.InputLayout.pInputElementDescs[i];k.Text(e.SemanticName);k.Add(e.SemanticIndex);
        k.Add(e.Format);k.Add(e.InputSlot);k.Add(e.AlignedByteOffset);k.Add(e.InputSlotClass);k.Add(e.InstanceDataStepRate);
    }
    k.Add(d.IBStripCutValue);k.Add(d.PrimitiveTopologyType);k.Add(d.NumRenderTargets);
    for(auto f:d.RTVFormats)k.Add(f);k.Add(d.DSVFormat);k.Add(d.SampleDesc.Count);k.Add(d.SampleDesc.Quality);
    k.Add(d.NodeMask);k.Add(d.Flags);return std::move(k.data);
}
}
const ShaderPipeline& GuestPipelineCache::Compute(AmdGpu::Liverpool& gpu) {
    auto& guest=frontend_.BuildCompute(gpu);
    return Compute(guest);
}
const ShaderPipeline& GuestPipelineCache::Compute(const GuestShaderStage& guest) {
    if(auto found=compute_.find(guest.id);found!=compute_.end()){
        Diagnostics::Count("compute-pso.hit");return *found->second;
    }
    Diagnostics::Count("compute-pso.miss");
    const auto& dxil=shaders_.Compute(guest);
    auto pipeline=std::make_unique<ShaderPipeline>(device_.Get(),dxc_,dxil.bytecode);
    return *compute_.emplace(guest.id,std::move(pipeline)).first->second;
}
const ShaderPipeline& GuestPipelineCache::Graphics(AmdGpu::Liverpool& gpu,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,u16 vertex_sgpr_offset,u32 instance_sgpr_offset) {
    auto guest=frontend_.BuildGraphics(gpu,vertex_sgpr_offset,instance_sgpr_offset);
    return Graphics(guest,state);
}
const ShaderPipeline& GuestPipelineCache::Graphics(const GuestGraphicsStages& guest,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds,ShaderPipeline::StencilFaces stencil) {
    auto key=GraphicsKey(guest,state);
    key.push_back(depth_bounds?'\1':'\0');
    key.push_back(stencil.separate?'\1':'\0');key.push_back(char(stencil.front_read));
    key.push_back(char(stencil.front_write));key.push_back(char(stencil.back_read));key.push_back(char(stencil.back_write));
    if(auto found=graphics_.find(key);found!=graphics_.end())return *found->second;
    const auto& dxil=shaders_.Graphics(guest);
    auto code=[&](Shader::SwStage stage)->std::span<const uint8_t>{return dxil.stages[u32(stage)].bytecode;};
    ShaderPipeline::GraphicsShaders stages{code(Shader::SwStage::Vertex),code(Shader::SwStage::Fragment),
        code(Shader::SwStage::TessellationControl),code(Shader::SwStage::TessellationEval),code(Shader::SwStage::Geometry)};
    auto pipeline=std::make_unique<ShaderPipeline>(device_.Get(),dxc_,stages,state,depth_bounds,stencil);
    return *graphics_.emplace(std::move(key),std::move(pipeline)).first->second;
}
}
