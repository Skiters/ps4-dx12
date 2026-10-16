#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_pipeline.h"
#include "nv_shader_clock.h"
#include "ags_shader_clock.h"
#include "persistent_cache.h"
#include <map>
#include <mutex>
#include <cstring>
#include <algorithm>
namespace NativeDX12 {
namespace {
void AddGraphicsState(CacheKey& k,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    if(d.StreamOutput.NumEntries||d.StreamOutput.NumStrides)
        throw std::invalid_argument("Guest stream output is not connected to native D3D12 yet");
    if(d.InputLayout.NumElements&&!d.InputLayout.pInputElementDescs)
        throw std::invalid_argument("Null native vertex input layout");
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
    k.Add(d.NodeMask);k.Add(d.Flags);return;
}

std::filesystem::path PsoDirectory(){
    return NativeCacheDirectory("pso");
}
CacheKey PsoKey(ID3D12Device* device,const std::wstring& dxc){
    struct Fingerprint {std::filesystem::file_time_type modified;uintmax_t size;std::array<std::byte,32> hash;};
    static std::mutex mutex;static std::map<std::wstring,Fingerprint> fingerprints;
    CacheKey key;key.Text("native-pso-cache-20261009-v1");
    {std::lock_guard lock(mutex);const auto modified=std::filesystem::last_write_time(dxc);const auto size=std::filesystem::file_size(dxc);
        auto found=fingerprints.find(dxc);
        if(found==fingerprints.end()||found->second.modified!=modified||found->second.size!=size)
            found=fingerprints.insert_or_assign(dxc,Fingerprint{modified,size,CacheDigest::File(dxc)}).first;
        key.Blob(found->second.hash);}
    const auto adapter=DX12_API_CALL(device->GetAdapterLuid());key.Add(adapter.LowPart);key.Add(adapter.HighPart);return key;
}
template<class Desc,class Create>
void CreateCachedPso(const CacheKey& key,Desc& desc,ComPtr<ID3D12PipelineState>& state,Create create){
    PersistentCache cache(PsoDirectory());std::vector<std::byte> saved;
    const bool cached=cache.Load(key.bytes,saved)&&!saved.empty();
    if(cached)desc.CachedPSO={saved.data(),saved.size()};
    try{create();}catch(const std::exception&){
        if(!cached)throw;
        // Driver upgrades/rejections must fall back to the exact original PSO,
        // rather than accepting stale driver code or failing the guest draw.
        desc.CachedPSO={};state.Reset();create();
    }
    ComPtr<ID3DBlob> blob;
    if(SUCCEEDED(DX12_API_CALL(state->GetCachedBlob(&blob))))cache.Store(key.bytes,
        {static_cast<const std::byte*>(DX12_API_CALL(blob->GetBufferPointer())),DX12_API_CALL(blob->GetBufferSize())});
}

template<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type,class T>
struct alignas(void*) StreamPart {D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type=Type;T data{};};
void CreateExtendedPipeline(ID3D12Device* device,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d,
    bool depth_bounds,ShaderPipeline::StencilFaces masks,ID3D12PipelineState** output) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS2 options{};
    Check(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2,&options,sizeof(options))));
    if(depth_bounds&&!options.DepthBoundsTestSupported)throw std::runtime_error("Native GPU does not support D3D12 depth bounds");
    struct Stream {
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE,ID3D12RootSignature*> root;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS,D3D12_SHADER_BYTECODE> vs;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS,D3D12_SHADER_BYTECODE> ps;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS,D3D12_SHADER_BYTECODE> hs;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS,D3D12_SHADER_BYTECODE> ds;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS,D3D12_SHADER_BYTECODE> gs;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT,D3D12_STREAM_OUTPUT_DESC> so;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND,D3D12_BLEND_DESC> blend;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK,UINT> mask;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER,D3D12_RASTERIZER_DESC> raster;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL2,D3D12_DEPTH_STENCIL_DESC2> depth;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT,D3D12_INPUT_LAYOUT_DESC> input;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE,D3D12_INDEX_BUFFER_STRIP_CUT_VALUE> strip;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY,D3D12_PRIMITIVE_TOPOLOGY_TYPE> topology;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS,D3D12_RT_FORMAT_ARRAY> formats;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT,DXGI_FORMAT> dsv;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC,DXGI_SAMPLE_DESC> samples;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK,UINT> node;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO,D3D12_CACHED_PIPELINE_STATE> cache;
        StreamPart<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS,D3D12_PIPELINE_STATE_FLAGS> flags;
    } s;
    s.root.data=d.pRootSignature;s.vs.data=d.VS;s.ps.data=d.PS;s.hs.data=d.HS;s.ds.data=d.DS;s.gs.data=d.GS;
    s.so.data=d.StreamOutput;s.blend.data=d.BlendState;s.mask.data=d.SampleMask;s.raster.data=d.RasterizerState;
    const auto& z=d.DepthStencilState;
    const auto face=[](const D3D12_DEPTH_STENCILOP_DESC& source,UINT8 read,UINT8 write){
        return D3D12_DEPTH_STENCILOP_DESC1{source.StencilFailOp,source.StencilDepthFailOp,
            source.StencilPassOp,source.StencilFunc,read,write};};
    s.depth.data={z.DepthEnable,z.DepthWriteMask,z.DepthFunc,z.StencilEnable,
        face(z.FrontFace,masks.separate?masks.front_read:z.StencilReadMask,masks.separate?masks.front_write:z.StencilWriteMask),
        face(z.BackFace,masks.separate?masks.back_read:z.StencilReadMask,masks.separate?masks.back_write:z.StencilWriteMask),
        depth_bounds};
    s.input.data=d.InputLayout;s.strip.data=d.IBStripCutValue;s.topology.data=d.PrimitiveTopologyType;
    s.formats.data.NumRenderTargets=d.NumRenderTargets;std::copy_n(d.RTVFormats,8,s.formats.data.RTFormats);
    s.dsv.data=d.DSVFormat;s.samples.data=d.SampleDesc;s.node.data=d.NodeMask;s.cache.data=d.CachedPSO;s.flags.data=d.Flags;
    D3D12_PIPELINE_STATE_STREAM_DESC stream{sizeof(s),&s};
    ComPtr<ID3D12Device2> extended;Check(DX12_API_CALL(device->QueryInterface(IID_PPV_ARGS(&extended))));
    Check(DX12_API_CALL(extended->CreatePipelineState(&stream,IID_PPV_ARGS(output))));
}
void Validate(std::span<const uint8_t> code,bool required) {
    if(code.empty()&&!required)return;
    if(code.size()<32||std::memcmp(code.data(),"DXBC",4))
        throw std::invalid_argument("Native pipeline requires compiled DXIL container");
}
}
ShaderPipeline::ShaderPipeline(ID3D12Device* device,const std::wstring& dxc,
    std::span<const uint8_t> compute)
    :layout_(device,dxc,std::span(&compute,1)),compute_(true) {
    Validate(compute,true);
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature=layout_.Root();desc.CS={compute.data(),compute.size()};
    auto key=PsoKey(device,dxc);key.Text("compute");key.Blob(std::as_bytes(compute));
    const bool nv_clock=std::ranges::any_of(layout_.Bindings(),[](const auto& b){return b.nv_clock;});
    key.Add(nv_clock);if(nv_clock){key.Text("nvapi-global-timer-v1");key.Blob(NvShaderClockRuntimeDigest());}
    const bool amd_clock=std::ranges::any_of(layout_.Bindings(),[](const auto& b){return b.amd_clock;});
    key.Add(amd_clock);if(amd_clock){key.Text("ags-shader-clock-v1");key.Blob(AmdShaderClockRuntimeDigest(device));}
    NvShaderClockScope clock_scope(device,nv_clock);
    CreateCachedPso(key,desc,state_,[&]{Check(DX12_API_CALL(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&state_))));});
    clock_scope.Finish();
}
ShaderPipeline::ShaderPipeline(ID3D12Device* device,const std::wstring& dxc,
    const GraphicsShaders& shaders,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& fixed_state,bool depth_bounds)
    :ShaderPipeline(device,dxc,shaders,fixed_state,depth_bounds,StencilFaces{}) {}
ShaderPipeline::ShaderPipeline(ID3D12Device* device,const std::wstring& dxc,
    const GraphicsShaders& shaders,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& fixed_state,bool depth_bounds,
    StencilFaces stencil)
    :layout_(device,dxc,shaders.All()),compute_(false) {
    Validate(shaders.vertex,true);
    for(auto code:{shaders.pixel,shaders.hull,shaders.domain,shaders.geometry})Validate(code,false);
    if(shaders.hull.empty()!=shaders.domain.empty())
        throw std::invalid_argument("Native hull/domain shaders must be provided together");
    if((!shaders.hull.empty())!=(fixed_state.PrimitiveTopologyType==D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH))
        throw std::invalid_argument("Native tessellation shader/topology mismatch");
    auto desc=fixed_state;
    if(desc.Flags&D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS){
        D3D12_FEATURE_DATA_D3D12_OPTIONS16 options{};
        if(FAILED(DX12_API_CALL(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS16,&options,sizeof(options))))||!options.DynamicDepthBiasSupported)
            throw std::runtime_error("Native GPU/runtime does not support fractional D3D12 depth bias");
    }
    if(desc.NumRenderTargets>targets_.size())throw std::invalid_argument("Native pipeline attachment count");
    target_count_=desc.NumRenderTargets;sample_count_=desc.SampleDesc.Count;depth_=desc.DSVFormat;
    std::copy(std::begin(desc.RTVFormats),std::end(desc.RTVFormats),targets_.begin());
    desc.pRootSignature=layout_.Root();
    desc.VS={shaders.vertex.data(),shaders.vertex.size()};
    desc.PS=fixed_state.StreamOutput.RasterizedStream==D3D12_SO_NO_RASTERIZED_STREAM?
        D3D12_SHADER_BYTECODE{}:D3D12_SHADER_BYTECODE{shaders.pixel.data(),shaders.pixel.size()};
    desc.HS={shaders.hull.data(),shaders.hull.size()};
    desc.DS={shaders.domain.data(),shaders.domain.size()};
    desc.GS={shaders.geometry.data(),shaders.geometry.size()};
    auto key=PsoKey(device,dxc);key.Text("graphics");key.Add(depth_bounds);key.Add(stencil.separate);
    key.Add(stencil.front_read);key.Add(stencil.front_write);key.Add(stencil.back_read);key.Add(stencil.back_write);AddGraphicsState(key,desc);
    for(const auto code:shaders.All())key.Blob(std::as_bytes(code));
    const bool nv_clock=std::ranges::any_of(layout_.Bindings(),[](const auto& b){return b.nv_clock;});
    key.Add(nv_clock);if(nv_clock){key.Text("nvapi-global-timer-v1");key.Blob(NvShaderClockRuntimeDigest());}
    const bool amd_clock=std::ranges::any_of(layout_.Bindings(),[](const auto& b){return b.amd_clock;});
    key.Add(amd_clock);if(amd_clock){key.Text("ags-shader-clock-v1");key.Blob(AmdShaderClockRuntimeDigest(device));}
    NvShaderClockScope clock_scope(device,nv_clock);
    CreateCachedPso(key,desc,state_,[&]{
        if(depth_bounds||stencil.separate)CreateExtendedPipeline(device,desc,depth_bounds,stencil,state_.GetAddressOf());
        else Check(DX12_API_CALL(device->CreateGraphicsPipelineState(&desc,IID_PPV_ARGS(&state_))));
    });
    clock_scope.Finish();
}
void ShaderPipeline::Bind(ID3D12GraphicsCommandList* list,ResourceBindings& resources) const {
    if(&resources.Layout()!=&layout_)
        throw std::invalid_argument("Resource descriptors belong to a different native pipeline");
    resources.Bind(list,compute_);DX12_API_CALL(list->SetPipelineState(state_.Get()));
}
}
