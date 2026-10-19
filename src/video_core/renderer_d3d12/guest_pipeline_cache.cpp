// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_pipeline_cache.h"
#include "diagnostics.h"
#include "primitive_stream_arguments.h"
#include <algorithm>
#include <d3d12sdklayers.h>
namespace NativeDX12 {
std::shared_ptr<ResourceLayout> GuestPipelineCache::LogicalLayout(std::span<const ShaderPipeline* const> domains){
    if(domains.empty())throw std::invalid_argument("Logical draw needs graphics pipeline domains");
    std::vector<std::uintptr_t> key;
    key.reserve(domains.size());
    for(const auto* domain:domains){
        if(!domain||domain->IsCompute())throw std::invalid_argument("Invalid logical graphics pipeline domain");
        key.push_back(reinterpret_cast<std::uintptr_t>(domain->Layout().Root()));
    }
    std::sort(key.begin(),key.end());key.erase(std::unique(key.begin(),key.end()),key.end());
    if(const auto found=logical_layouts_.find(key);found!=logical_layouts_.end())return found->second.layout;
    std::vector<const ResourceLayout*> layouts;layouts.reserve(domains.size());
    LogicalLayoutRecord record;
    for(const auto* domain:domains){
        layouts.push_back(&domain->Layout());
        // Retain root identities so a deleted pipeline's pointer cannot be
        // recycled into a different cache key before invalidation.
        record.roots.emplace_back(domain->Layout().Root());
    }
    record.layout=std::make_shared<ResourceLayout>(device_.Get(),layouts);
    return logical_layouts_.emplace(std::move(key),std::move(record)).first->second.layout;
}
StreamOutputLayout GuestPipelineCache::PrimitiveStreamLayout(const GuestGraphicsStages& stages){
    const auto& geometry=shaders_.Graphics(stages).stages[u32(Shader::SwStage::Geometry)];
    return ReflectPrimitiveStreamLayout(dxc_,geometry.bytecode);
}
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
std::string GraphicsKey(const GuestGraphicsStages& guest,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d,
    bool depth_bounds,ShaderPipeline::StencilFaces stencil) {
    if(d.InputLayout.NumElements&&!d.InputLayout.pInputElementDescs)
        throw std::invalid_argument("Null native vertex input layout");
    Key k;for(auto* stage:guest.stages)k.Add(stage?stage->id:u64(0));
    StreamOutputLayout::AppendKey(k,d.StreamOutput);
    k.Add(d.BlendState.AlphaToCoverageEnable);k.Add(d.BlendState.IndependentBlendEnable);
    for(const auto& r:d.BlendState.RenderTarget) {
        k.Add(r.BlendEnable);k.Add(r.LogicOpEnable);k.Add(r.SrcBlend);k.Add(r.DestBlend);k.Add(r.BlendOp);
        k.Add(r.SrcBlendAlpha);k.Add(r.DestBlendAlpha);k.Add(r.BlendOpAlpha);k.Add(r.LogicOp);k.Add(r.RenderTargetWriteMask);
    }
    k.Add(d.StreamOutput.RasterizedStream);k.Add(d.SampleMask);const auto& r=d.RasterizerState;
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
    k.Add(d.NodeMask);k.Add(d.Flags);
    k.data.push_back(depth_bounds?'\1':'\0');k.data.push_back(stencil.separate?'\1':'\0');
    k.data.push_back(char(stencil.front_read));k.data.push_back(char(stencil.front_write));
    k.data.push_back(char(stencil.back_read));k.data.push_back(char(stencil.back_write));return std::move(k.data);
}
}
GuestPipelineCache::GraphicsRecord::GraphicsRecord(const GuestGraphicsStages& g,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d,bool bounds,ShaderPipeline::StencilFaces faces)
    :guest(g),state(d),stream_output(d.StreamOutput),depth_bounds(bounds),stencil(faces){
    inputs.reserve(d.InputLayout.NumElements);semantics.reserve(d.InputLayout.NumElements);
    for(UINT i=0;i<d.InputLayout.NumElements;++i){inputs.push_back(d.InputLayout.pInputElementDescs[i]);
        semantics.emplace_back(inputs.back().SemanticName);}
    for(size_t i=0;i<inputs.size();++i)inputs[i].SemanticName=semantics[i].c_str();
    state.InputLayout={inputs.data(),UINT(inputs.size())};state.pRootSignature=nullptr;
    state.StreamOutput=stream_output.Desc();
    state.VS=state.PS=state.DS=state.HS=state.GS={};state.CachedPSO={};
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
#ifdef SHADPS4_NATIVE_DX12_CORE
    RegisterPipelineData(guest);
#endif
    return *compute_.emplace(guest.id,std::move(pipeline)).first->second;
}
const ShaderPipeline& GuestPipelineCache::Graphics(AmdGpu::Liverpool& gpu,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,u16 vertex_sgpr_offset,u32 instance_sgpr_offset) {
    auto guest=frontend_.BuildGraphics(gpu,vertex_sgpr_offset,instance_sgpr_offset);
    return Graphics(guest,state);
}
const ShaderPipeline& GuestPipelineCache::Graphics(const GuestGraphicsStages& guest,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds,ShaderPipeline::StencilFaces stencil) {
    auto key=GraphicsKey(guest,state,depth_bounds,stencil);
    if(auto found=graphics_.find(key);found!=graphics_.end())return *found->second->pipeline;
    auto record=std::make_unique<GraphicsRecord>(guest,state,depth_bounds,stencil);
    record->pipeline=BuildGraphics(guest,record->state,depth_bounds,stencil);
#ifdef SHADPS4_NATIVE_DX12_CORE
    RegisterPipelineData(guest,state,depth_bounds,stencil);
#endif
    return *graphics_.emplace(std::move(key),std::move(record)).first->second->pipeline;
}
std::unique_ptr<ShaderPipeline> GuestPipelineCache::BuildGraphics(const GuestGraphicsStages& guest,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds,ShaderPipeline::StencilFaces stencil){
    const auto& dxil=shaders_.Graphics(guest);
    auto code=[&](Shader::SwStage stage)->std::span<const uint8_t>{return dxil.stages[u32(stage)].bytecode;};
    ShaderPipeline::GraphicsShaders stages{code(Shader::SwStage::Vertex),code(Shader::SwStage::Fragment),
        code(Shader::SwStage::TessellationControl),code(Shader::SwStage::TessellationEval),code(Shader::SwStage::Geometry)};
    try{return std::make_unique<ShaderPipeline>(device_.Get(),dxc_,stages,state,depth_bounds,stencil);}
    catch(const std::exception& error){
        fprintf(stderr,"Native graphics PSO: %s\n",error.what());
        ComPtr<ID3D12InfoQueue> queue;
        if(SUCCEEDED(device_.As(&queue)))for(UINT64 i=0;i<queue->GetNumStoredMessages();++i){
            SIZE_T bytes{};queue->GetMessage(i,nullptr,&bytes);std::vector<std::byte> text(bytes);
            auto* message=reinterpret_cast<D3D12_MESSAGE*>(text.data());
            if(SUCCEEDED(queue->GetMessage(i,message,&bytes)))fprintf(stderr,"Native graphics validation: %s\n",message->pDescription);
        }
        fflush(stderr);throw;
    }
}
std::optional<u64> GuestPipelineCache::ReplaceShader(u64 id,std::span<const u32> words,bool patched){
    auto* original=frontend_.FindStage(id);if(!original)return {};
    if(words.size()<5||words[0]!=0x07230203U)throw std::invalid_argument("Invalid shader replacement SPIR-V");
    for(size_t offset=5;offset<words.size();){const auto count=words[offset]>>16;
        if(!count||count>words.size()-offset)throw std::invalid_argument("Truncated shader replacement SPIR-V");offset+=count;}
    GuestShaderStage candidate=*original;candidate.id=frontend_.ReserveStageId();candidate.is_patched=patched;
    if(patched)candidate.patch_spirv.assign(words.begin(),words.end());
    else candidate.spirv.assign(words.begin(),words.end());
    std::map<u64,std::unique_ptr<ShaderPipeline>> new_compute;
    std::map<std::string,std::unique_ptr<GraphicsRecord>> new_graphics;
    std::vector<std::string> retired_keys;
    if(compute_.contains(id)||original->info.sw_stage==Shader::SwStage::Compute){
        const auto& dxil=shaders_.Compute(candidate);
        new_compute.emplace(candidate.id,std::make_unique<ShaderPipeline>(device_.Get(),dxc_,dxil.bytecode));
    }
    for(const auto& [key,record]:graphics_){
        auto guest=record->guest;bool affected=false;
        for(auto*& stage:guest.stages)if(stage==original){stage=&candidate;affected=true;}
        if(!affected)continue;
        auto replacement=std::make_unique<GraphicsRecord>(record->guest,record->state,record->depth_bounds,record->stencil);
        replacement->pipeline=BuildGraphics(guest,replacement->state,record->depth_bounds,record->stencil);
        new_graphics.emplace(GraphicsKey(guest,replacement->state,record->depth_bounds,record->stencil),std::move(replacement));
        retired_keys.push_back(key);
    }
    if(new_compute.empty()&&new_graphics.empty())shaders_.ValidateStage(candidate);
    original->spirv.swap(candidate.spirv);original->patch_spirv.swap(candidate.patch_spirv);
    original->is_patched=candidate.is_patched;original->id=candidate.id;
    compute_.merge(new_compute);graphics_.merge(new_graphics);
    logical_layouts_.clear();
    compute_.erase(id);for(const auto& key:retired_keys)graphics_.erase(key);
    return original->id;
}
}
