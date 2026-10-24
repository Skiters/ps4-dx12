// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_pipeline_cache.h"
#include "guest_shader_debug.h"
#include "common/serdes.h"
#include "common/logging/log.h"
#include "core/emulator_settings.h"
#include "video_core/cache_storage.h"
#include <cstdlib>
#include <sstream>

namespace NativeDX12 {
namespace {
constexpr u32 StageVersion=40,PipelineVersion=6;
std::string DigestName(std::span<const std::byte> data){
    const auto digest=CacheDigest::Of(data);std::string name;
    constexpr char hex[]="0123456789abcdef";
    for(const auto value:digest){const auto b=std::to_integer<u8>(value);name+=hex[b>>4];name+=hex[b&15];}
    return name;
}
std::array<std::byte,32> ExecutableIdentity(){
    static const auto digest=[]{
        std::vector<wchar_t> path(512);
        for(;;){
            const auto size=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
            if(!size)throw std::runtime_error("Native executable identity path unavailable");
            if(size<path.size())return CacheDigest::File(std::filesystem::path(std::wstring(path.data(),size)));
            if(path.size()>UINT32_MAX/2)throw std::runtime_error("Native executable identity path overflow");
            path.resize(path.size()*2);
        }
    }();
    return digest;
}
std::vector<u8> Identity(const GuestShaderCache& shaders){
    CacheKey key;key.Text("native-pipeline-metadata-v36");key.Blob(ExecutableIdentity());
    key.Blob(shaders.ConfigurationIdentity());
    return {reinterpret_cast<const u8*>(key.bytes.data()),reinterpret_cast<const u8*>(key.bytes.data()+key.bytes.size())};
}
struct FixedGraphicsState{
    D3D12_BLEND_DESC blend{};
    UINT sample_mask{};
    UINT rasterized_stream{};
    D3D12_RASTERIZER_DESC raster{};
    D3D12_DEPTH_STENCIL_DESC depth{};
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE strip{};
    D3D12_PRIMITIVE_TOPOLOGY_TYPE topology{};
    UINT target_count{};
    std::array<DXGI_FORMAT,8> targets{};
    DXGI_FORMAT depth_format{};
    DXGI_SAMPLE_DESC samples{};
    UINT nodes{};
    D3D12_PIPELINE_STATE_FLAGS flags{};
};
FixedGraphicsState State(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d){
    StreamOutputLayout::Validate(d.StreamOutput);
    FixedGraphicsState result{};
    result.blend=d.BlendState;result.sample_mask=d.SampleMask;result.raster=d.RasterizerState;
    result.rasterized_stream=d.StreamOutput.RasterizedStream;
    result.depth=d.DepthStencilState;result.strip=d.IBStripCutValue;result.topology=d.PrimitiveTopologyType;
    result.target_count=d.NumRenderTargets;std::copy(std::begin(d.RTVFormats),std::end(d.RTVFormats),result.targets.begin());
    result.depth_format=d.DSVFormat;result.samples=d.SampleDesc;result.nodes=d.NodeMask;result.flags=d.Flags;
    return result;
}
void SaveGraphicsState(Serialization::Archive& ar,const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d){
    Serialization::Writer save(ar);const auto state=State(d);save.Write(&state,sizeof(state));
    save.Write(d.InputLayout.NumElements);
    for(UINT i=0;i<d.InputLayout.NumElements;++i){
        const auto& input=d.InputLayout.pInputElementDescs[i];
        if(!input.SemanticName)throw std::invalid_argument("Null cached vertex semantic");
        save.Write(std::string(input.SemanticName));save.Write(input.SemanticIndex);save.Write(input.Format);
        save.Write(input.InputSlot);save.Write(input.AlignedByteOffset);save.Write(input.InputSlotClass);
        save.Write(input.InstanceDataStepRate);
    }
    StreamOutputLayout(d.StreamOutput).Save(save);
}
struct GraphicsState{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
    std::vector<std::string> semantics;
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputs;
    StreamOutputLayout stream_output;
    explicit GraphicsState(Serialization::Archive& ar){
        Serialization::Reader load(ar);FixedGraphicsState fixed{};load.Read(&fixed,sizeof(fixed));
        if(fixed.target_count>8||!fixed.samples.Count)throw std::runtime_error("Invalid cached graphics target state");
        state.BlendState=fixed.blend;state.SampleMask=fixed.sample_mask;state.RasterizerState=fixed.raster;
        if(fixed.rasterized_stream>=D3D12_SO_STREAM_COUNT&&fixed.rasterized_stream!=D3D12_SO_NO_RASTERIZED_STREAM)
            throw std::runtime_error("Invalid cached rasterized stream");
        state.StreamOutput.RasterizedStream=fixed.rasterized_stream;
        state.DepthStencilState=fixed.depth;state.IBStripCutValue=fixed.strip;state.PrimitiveTopologyType=fixed.topology;
        state.NumRenderTargets=fixed.target_count;std::copy(fixed.targets.begin(),fixed.targets.end(),std::begin(state.RTVFormats));
        state.DSVFormat=fixed.depth_format;state.SampleDesc=fixed.samples;state.NodeMask=fixed.nodes;state.Flags=fixed.flags;
        UINT count{};load.Read(count);
        if(count>D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)throw std::runtime_error("Invalid cached vertex input count");
        semantics.resize(count);inputs.resize(count);
        for(UINT i=0;i<count;++i){
            auto& input=inputs[i];load.Read(semantics[i]);input.SemanticName=semantics[i].c_str();
            load.Read(input.SemanticIndex);load.Read(input.Format);load.Read(input.InputSlot);
            load.Read(input.AlignedByteOffset);load.Read(input.InputSlotClass);load.Read(input.InstanceDataStepRate);
        }
        state.InputLayout={inputs.data(),count};
        stream_output=StreamOutputLayout::Load(load);state.StreamOutput=stream_output.Desc();
        if(state.StreamOutput.RasterizedStream!=fixed.rasterized_stream)
            throw std::runtime_error("Cached primitive stream state disagrees with its fixed graphics state");
    }
};
}
void GuestShaderFrontend::SerializeStage(Serialization::Archive& ar,const GuestShaderStage& stage)const{
    Serialization::Writer save(ar);save.Write(StageVersion);save.Write(stage.auxiliary);
    if(stage.auxiliary){
        save.Write(stage.info.sw_stage);
    }else{
        const auto& variants=programs_.at(Key{stage.info.pgm_hash,stage.info.hw_stage,stage.info.sw_stage});
        const auto found=std::ranges::find_if(variants,[&](const auto& value){return value.second.get()==&stage;});
        if(found==variants.end())throw std::logic_error("Cached shader is not owned by the frontend");
        save.Write(found->first);
        auto specialization=stage.specialization;auto runtime=stage.runtime;
        // Translation consumes this borrowed process-local span. Cached SPIR-V
        // and the owned copy-output map are sufficient for restoration; retain
        // the stable copy hash, never a pointer into a previous guest mapping.
        if(stage.info.hw_stage==Shader::HwStage::Geometry){
            specialization.runtime_info.hw.gs.vs_copy={};runtime.hw.gs.vs_copy={};
        }
        stage.info.Serialize(ar);specialization.Serialize(ar);
        save.Write(&runtime,sizeof(runtime));save.Write(stage.bindings_start);save.Write(stage.bindings_end);
    }
    const auto binary_name=DigestName(std::as_bytes(std::span(stage.spirv)));
    save.Write(binary_name);
    if(Storage::DataBase::Instance().IsOpened())
        Storage::DataBase::Instance().Save(Storage::BlobType::ShaderBinary,binary_name,std::vector<u32>(stage.spirv));
}
GuestShaderStage& GuestShaderFrontend::RestoreStage(Serialization::Archive& ar){
    Serialization::Reader load(ar);u32 version{};load.Read(version);
    if(version!=StageVersion)throw std::runtime_error("Incompatible native shader metadata");
    auto stage=std::make_unique<GuestShaderStage>();load.Read(stage->auxiliary);size_t index{};
    if(stage->auxiliary){
        load.Read(stage->info.sw_stage);
        if(stage->info.sw_stage!=Shader::SwStage::TessellationControl&&stage->info.sw_stage!=Shader::SwStage::TessellationEval&&
           stage->info.sw_stage!=Shader::SwStage::Geometry)
            throw std::runtime_error("Invalid native auxiliary stage");
        stage->info.hw_stage=stage->info.sw_stage==Shader::SwStage::TessellationControl?Shader::HwStage::Hull:
            stage->info.sw_stage==Shader::SwStage::Geometry?Shader::HwStage::Geometry:Shader::HwStage::Vertex;
    }else{
        load.Read(index);stage->info.Deserialize(ar);stage->specialization.Deserialize(ar);
        stage->specialization.info=&stage->info;
        load.Read(&stage->runtime,sizeof(stage->runtime));load.Read(stage->bindings_start);load.Read(stage->bindings_end);
        const bool borrowed_copy=stage->info.hw_stage==Shader::HwStage::Geometry&&
            (stage->runtime.hw.gs.vs_copy.data()||stage->runtime.hw.gs.vs_copy.size()||
             stage->specialization.runtime_info.hw.gs.vs_copy.data()||stage->specialization.runtime_info.hw.gs.vs_copy.size());
        if(u32(stage->info.hw_stage)>u32(Shader::HwStage::Compute)||u32(stage->info.sw_stage)>=Shader::MaxStageTypes||
           borrowed_copy||
           stage->info.flattened_ud_buf.size()<stage->info.srt_info.flattened_bufsize_dw||
           stage->bindings_start!=stage->specialization.start||stage->runtime!=stage->specialization.runtime_info||
           stage->specialization.buffers.size()!=stage->info.buffers.size()||
           stage->specialization.images.size()!=stage->info.images.size()||
           stage->specialization.fmasks.size()!=stage->info.fmasks.size()||
           stage->specialization.samplers.size()!=stage->info.samplers.size()||
           stage->info.buffers.size()+stage->info.images.size()+stage->info.fmasks.size()>Shader::StageSpecialization::MaxStageResources||
           stage->bindings_end.unified<stage->bindings_start.unified||
           stage->bindings_end.buffer<stage->bindings_start.buffer)
            throw std::runtime_error("Invalid native cached shader contract");
    }
    std::string binary_name;load.Read(binary_name);
    if(binary_name.size()!=64)throw std::runtime_error("Invalid native shader binary identity");
    Storage::DataBase::Instance().Load(Storage::BlobType::ShaderBinary,binary_name,stage->spirv);
    if(stage->spirv.size()<5||stage->spirv[0]!=0x07230203||
       binary_name!=DigestName(std::as_bytes(std::span(stage->spirv))))
        throw std::runtime_error("Missing or stale native shader binary");
    if(stage->auxiliary){
        const auto found=auxiliary_.find(stage->spirv);if(found!=auxiliary_.end())return *found->second;
        stage->id=ReserveStageId();const auto words=stage->spirv;
        return *auxiliary_.emplace(words,std::move(stage)).first->second;
    }
    auto& variants=programs_[Key{stage->info.pgm_hash,stage->info.hw_stage,stage->info.sw_stage}];
    if(const auto found=variants.find(index);found!=variants.end()){
        auto& existing=*found->second;
        if(existing.spirv!=stage->spirv||existing.bindings_end!=stage->bindings_end||
           existing.bindings_start!=stage->bindings_start||existing.specialization!=stage->specialization)
            throw std::runtime_error("Conflicting native cached permutation index");
        return existing;
    }
    stage->id=ReserveStageId();LoadShaderPatch(*stage,index);
    return *variants.emplace(index,std::move(stage)).first->second;
}
void GuestPipelineCache::RegisterPipelineData(const GuestShaderStage& stage){
    if(warming_||!Storage::DataBase::Instance().IsOpened())return;
    Serialization::Archive ar;Serialization::Writer save(ar);save.Write(PipelineVersion);save.Write(u32{1});
    Serialization::Archive meta;frontend_.SerializeStage(meta,stage);
    const auto blob=meta.TakeOff();const auto meta_name=DigestName(std::as_bytes(std::span(blob)));save.Write(meta_name);
    Storage::DataBase::Instance().Save(Storage::BlobType::ShaderMeta,meta_name,std::vector<u8>(blob));
    auto data=ar.TakeOff();const auto name="c_"+DigestName(std::as_bytes(std::span(data)));
    Storage::DataBase::Instance().Save(Storage::BlobType::PipelineKey,name,std::move(data));
}
void GuestPipelineCache::RegisterPipelineData(const GuestGraphicsStages& guest,
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC& state,bool depth_bounds,ShaderPipeline::StencilFaces stencil,bool root_runtime){
    if(warming_||!Storage::DataBase::Instance().IsOpened())return;
    Serialization::Archive ar;Serialization::Writer save(ar);save.Write(PipelineVersion);save.Write(u32{0});
    for(const auto* stage:guest.stages){
        save.Write(stage!=nullptr);if(!stage)continue;
        Serialization::Archive meta;frontend_.SerializeStage(meta,*stage);const auto blob=meta.TakeOff();
        const auto meta_name=DigestName(std::as_bytes(std::span(blob)));save.Write(meta_name);
        Storage::DataBase::Instance().Save(Storage::BlobType::ShaderMeta,meta_name,std::vector<u8>(blob));
    }
    SaveGraphicsState(ar,state);save.Write(depth_bounds);save.Write(&stencil,sizeof(stencil));
    save.Write(root_runtime);
    auto data=ar.TakeOff();const auto name="g_"+DigestName(std::as_bytes(std::span(data)));
    Storage::DataBase::Instance().Save(Storage::BlobType::PipelineKey,name,std::move(data));
}
void GuestPipelineCache::WarmUp(){
    if(!EmulatorSettings.IsPipelineCacheEnabled())return;
    auto& database=Storage::DataBase::Instance();database.Open();if(!database.IsOpened())return;
    const auto identity=Identity(shaders_);std::vector<u8> cached;
    database.Load(Storage::BlobType::ShaderProfile,"profile",cached);
    Serialization::Archive profile_ar;Serialization::Writer profile_save(profile_ar);
    profile_save.Write(identity);const auto& profile=frontend_.Profile();profile_save.Write(&profile,sizeof(profile));
    bool compatible=true;
    if(!cached.empty()){
        try{
            Serialization::Archive ar(std::move(cached));Serialization::Reader load(ar);
            std::vector<u8> cached_identity;Shader::Profile cached_profile{};load.Read(cached_identity);load.Read(&cached_profile,sizeof(cached_profile));
            compatible=ar.IsEoS()&&cached_identity==identity&&cached_profile==profile;
        }catch(const std::exception&){compatible=false;}
    }
    if(!compatible){LOG_WARNING(Render_DX12,"Ignoring native pipeline cache with incompatible executable/compiler/profile");database.Close();return;}
    warming_=true;u32 loaded{},total{};
    try{
        database.ForEachBlob(Storage::BlobType::PipelineKey,[&](std::vector<u8>&& blob){
            ++total;
            try{
                Serialization::Archive ar(std::move(blob));Serialization::Reader load(ar);u32 version{},compute{};
                load.Read(version);load.Read(compute);if(version!=PipelineVersion)throw std::runtime_error("Incompatible native pipeline record");
                const auto stage=[&]()->GuestShaderStage&{
                    std::string name;load.Read(name);std::vector<u8> meta;
                    database.Load(Storage::BlobType::ShaderMeta,name,meta);
                    if(meta.empty())throw std::runtime_error("Missing native shader metadata");
                    Serialization::Archive meta_ar(std::move(meta));
                    auto& result=frontend_.RestoreStage(meta_ar);
                    if(!meta_ar.IsEoS())throw std::runtime_error("Trailing native shader metadata");return result;
                };
                if(compute==1){
                    auto& shader=stage();if(!ar.IsEoS())throw std::runtime_error("Trailing native compute record");
                    Compute(shader);
                }else if(compute==0){
                    GuestGraphicsStages guest;
                    for(u32 i=0;i<guest.stages.size();++i){
                        bool present{};load.Read(present);if(present){guest.stages[i]=&stage();
                            if(u32(guest.stages[i]->info.sw_stage)!=i)throw std::runtime_error("Native cached graphics stage slot mismatch");}
                    }
                    if(const auto* fragment=guest.stages[u32(Shader::SwStage::Fragment)])
                        for(u32 i=0;i<fragment->runtime.hw.fs.ordered_color_targets.size();++i)
                            if(fragment->runtime.hw.fs.ordered_color_targets[i].enabled)
                                guest.ordered_color_mask|=1u<<i;
                    GraphicsState state(ar);bool bounds{};ShaderPipeline::StencilFaces masks{};
                    load.Read(bounds);load.Read(&masks,sizeof(masks));bool root_runtime{};load.Read(root_runtime);
                    if(!ar.IsEoS())throw std::runtime_error("Trailing native graphics record");
                    Graphics(guest,state.state,bounds,masks,root_runtime);
                }else throw std::runtime_error("Invalid native pipeline kind");
                ++loaded;
            }catch(const std::exception& error){LOG_WARNING(Render_DX12,"Native pipeline preload: {}",error.what());}
        });
        database.FinishPreload();database.Save(Storage::BlobType::ShaderProfile,"profile",profile_ar.TakeOff());
    }catch(...){warming_=false;database.Close();throw;}
    warming_=false;
    static const bool exit_handler=[](){std::at_quick_exit([]{Storage::DataBase::Instance().Close();});return true;}();
    (void)exit_handler;
    LOG_INFO(Render_DX12,"Preloaded {} native pipelines ({} stale records)",loaded,total-loaded);
}
void GuestPipelineCache::Sync(){Storage::DataBase::Instance().Close();}
GuestPipelineCache::~GuestPipelineCache(){Sync();}
} // namespace NativeDX12

