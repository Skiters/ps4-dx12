// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_shader_cache.h"
#include "diagnostics.h"
#include <stdexcept>

namespace NativeDX12 {
namespace DXIL = Shader::Backend::DX12;
namespace {
std::filesystem::path CacheDirectory(){
    return NativeCacheDirectory("dxil");
}
void OptionsKey(CacheKey& key,const DXIL::SpirvConvertOptions& o){
    key.Add(o.stage);key.Text(o.entry_point);key.Add(o.shader_model_minor);key.Add(o.validator_minor);
    key.Add(o.runtime_cbv_space);key.Add(o.runtime_cbv_register);key.Add(o.push_cbv_space);key.Add(o.push_cbv_register);
    key.Add(o.draw_sysvals);key.Add(o.workgroup_id);key.Add(o.declared_read_only_images_as_srvs);key.Add(o.inferred_read_only_images_as_srvs);
    key.Add(o.force_sample_rate_shading);key.Add(o.lower_view_index);key.Add(o.lower_view_index_to_rt_layer);
    key.Add(o.runtime_viewport_flips);key.Add(o.native_storage_buffer_uavs);key.Add(o.compute_wave_size);
    key.Add(o.native_shader_clock_backend);
    key.Add(uint64_t(o.specializations.size()));for(const auto& s:o.specializations){key.Add(s.id);key.Add(s.raw_bits);key.Add(s.defined_on_module);}
}
void SaveStage(CacheKey& data,const DXIL::SpirvConvertResult& s){
    data.Add(s.converter_version);data.Add(s.requires_runtime_data);data.Add(s.needs_draw_sysvals);
    data.Blob(std::as_bytes(std::span(s.bytecode)));
}
struct CacheReader {
    std::span<const std::byte> data;
    template<class T>T Read(){if(data.size()<sizeof(T))throw std::runtime_error("Short DXIL cache");T value{};memcpy(&value,data.data(),sizeof(value));data=data.subspan(sizeof(value));return value;}
    DXIL::SpirvConvertResult Stage(){
        DXIL::SpirvConvertResult stage;stage.converter_version=Read<uint64_t>();
        stage.requires_runtime_data=Read<uint8_t>()!=0;stage.needs_draw_sysvals=Read<uint8_t>()!=0;
        const auto size=Read<uint64_t>();if(size<32||size>data.size()||memcmp(data.data(),"DXBC",4))throw std::runtime_error("Malformed cached DXIL");
        stage.bytecode.resize(size);memcpy(stage.bytecode.data(),data.data(),size);data=data.subspan(size);
        stage.success=stage.converted=stage.validated=true;stage.diagnostics="Native persistent DXIL cache hit";return stage;
    }
};
DXIL::SpirvStage HostStage(Shader::SwStage stage) {
    switch (stage) {
    case Shader::SwStage::Vertex: return DXIL::SpirvStage::Vertex;
    case Shader::SwStage::Fragment: return DXIL::SpirvStage::Pixel;
    case Shader::SwStage::TessellationControl: return DXIL::SpirvStage::TessellationControl;
    case Shader::SwStage::TessellationEval: return DXIL::SpirvStage::TessellationEvaluation;
    case Shader::SwStage::Geometry: return DXIL::SpirvStage::Geometry;
    case Shader::SwStage::Compute: return DXIL::SpirvStage::Compute;
    default: throw std::invalid_argument("Invalid native guest shader stage");
    }
}
}
GuestShaderCache::GuestShaderCache(std::wstring converter,DXIL::SpirvConvertOptions options)
    :converter_(std::move(converter)),options_(std::move(options)),disk_(CacheDirectory()) {
    configuration_.Text("native-dxil-cache-20261018-mutable-word-uav-v4");OptionsKey(configuration_,options_);
    const auto directory=std::filesystem::path(converter_).parent_path();
    for(const auto& path:{std::filesystem::path(converter_),directory/L"dxcompiler.dll",directory/L"dxil.dll"})
        configuration_.Blob(CacheDigest::File(path));
}
void GuestShaderCache::ValidateStage(const GuestShaderStage& stage){
    auto options=options_;options.stage=HostStage(stage.info.sw_stage);
    const auto compiled=DXIL::ConvertSpirv(converter_,stage.EffectiveSpirv(),options);
    if(!compiled.success||compiled.bytecode.empty())throw std::runtime_error("Native shader replacement compilation failed: "+compiled.diagnostics);
}
const GuestDxilStages& GuestShaderCache::Graphics(const GuestGraphicsStages& guest) {
    std::array<u64, Shader::MaxStageTypes> key{};
    for(u32 index=0;index<guest.stages.size();++index)if(guest.stages[index])key[index]=guest.stages[index]->id;
    if (const auto existing = graphics_.find(key); existing != graphics_.end()) return existing->second;
    std::vector<DXIL::LinkedSpirvStage> inputs;
    std::vector<u32> indices;
    auto disk_key=configuration_;disk_key.Text("graphics");
    for (u32 index = 0; index < guest.stages.size(); ++index) {
        const auto* stage = guest.stages[index];
        if (!stage) continue;
        if (stage->info.sw_stage == Shader::SwStage::Compute)
            throw std::invalid_argument("Compute shader in guest graphics set");
        key[index] = stage->id;
        DXIL::LinkedSpirvStage input;
        input.stage = HostStage(stage->info.sw_stage);
        input.words = stage->EffectiveSpirv();
        disk_key.Add(index);disk_key.Add(input.stage);disk_key.Blob(std::as_bytes(input.words));
        inputs.push_back(std::move(input));
        indices.push_back(index);
    }
    std::vector<std::byte> saved;
    if(disk_.Load(disk_key.bytes,saved))try{
        CacheReader reader{saved};if(reader.Read<uint32_t>()!=indices.size())throw std::runtime_error("Cached graphics stage count mismatch");
        GuestDxilStages result;for(const auto index:indices)result.stages[index]=reader.Stage();
        if(!reader.data.empty())throw std::runtime_error("Trailing DXIL cache bytes");
        ++disk_hits_;return graphics_.emplace(key,std::move(result)).first->second;
    }catch(const std::exception&){}
    auto compiled = DXIL::ConvertLinkedStages(converter_, inputs, options_);
    if (!compiled.success || compiled.stages.size() != indices.size())
        throw std::runtime_error("Guest linked DXIL compilation failed: " + compiled.diagnostics);
    GuestDxilStages result;
    for (u32 index = 0; index < indices.size(); ++index) {
        if (!compiled.stages[index].success || compiled.stages[index].bytecode.empty())
            throw std::runtime_error("Guest compiler returned incomplete linked bytecode");
        result.stages[indices[index]] = std::move(compiled.stages[index]);
    }
    CacheKey data;data.Add(uint32_t(indices.size()));for(const auto index:indices)SaveStage(data,result.stages[index]);
    disk_.Store(disk_key.bytes,data.bytes);
    return graphics_.emplace(key, std::move(result)).first->second;
}
const DXIL::SpirvConvertResult& GuestShaderCache::Compute(const GuestShaderStage& guest) {
    if (guest.info.sw_stage != Shader::SwStage::Compute)
        throw std::invalid_argument("Non-compute guest shader in compute pipeline");
    if (const auto existing = compute_.find(guest.id); existing != compute_.end()) {
        Diagnostics::Count("compute-dxil.memory-hit");return existing->second;
    }
    auto options = options_;
    options.stage = DXIL::SpirvStage::Compute;
    auto disk_key=configuration_;disk_key.Text("compute");OptionsKey(disk_key,options);disk_key.Blob(std::as_bytes(guest.EffectiveSpirv()));
    std::vector<std::byte> saved;
    if(disk_.Load(disk_key.bytes,saved))try{
        CacheReader reader{saved};auto compiled=reader.Stage();if(!reader.data.empty())throw std::runtime_error("Trailing DXIL cache bytes");
        ++disk_hits_;Diagnostics::Count("compute-dxil.disk-hit");
        return compute_.emplace(guest.id,std::move(compiled)).first->second;
    }catch(const std::exception&){}
    Diagnostics::Count("compute-dxil.compile");
    auto compiled = DXIL::ConvertSpirv(converter_, guest.EffectiveSpirv(), options);
    if (!compiled.success) throw std::runtime_error("Guest compute DXIL compilation failed: " + compiled.diagnostics);
    CacheKey data;SaveStage(data,compiled);disk_.Store(disk_key.bytes,data.bytes);
    return compute_.emplace(guest.id, std::move(compiled)).first->second;
}
}
