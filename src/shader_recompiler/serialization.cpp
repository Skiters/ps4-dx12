// SPDX-FileCopyrightText: Copyright 2025-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/serdes.h"
#include "shader_recompiler/info.h"
#include "shader_recompiler/specialization.h"
#include <type_traits>

namespace Shader {
namespace {
struct SaveFields {
    Serialization::Writer data;
    explicit SaveFields(Serialization::Archive& ar):data(ar){}
    template<class T> void Field(const T& value){
        if constexpr(std::is_trivially_copyable_v<T>) data.Write(&value,sizeof(value));
        else data.Write(value);
    }
    template<class... T> void Fields(const T&... value){(Field(value),...);}
};
struct LoadFields {
    Serialization::Reader data;
    explicit LoadFields(Serialization::Archive& ar):data(ar){}
    template<class T> void Field(T& value){
        if constexpr(std::is_trivially_copyable_v<T>) data.Read(&value,sizeof(value));
        else data.Read(value);
    }
    template<class... T> void Fields(T&... value){(Field(value),...);}
};
template<class IO,class InfoType> void InfoFields(IO& io,InfoType& info){
    io.Field(info.buffers);
    io.Field(info.images);
    io.Field(info.native_pipeline_writes);
    io.Field(info.native_pipeline_signed_packed_views);
    io.Field(info.native_pipeline_raw32_views);
    io.Field(info.samplers);
    io.Field(info.fmasks);
    io.Field(info.fetch_shader_sgpr_base);
    io.Field(info.shared_memory_scratch_size);
    io.Field(info.pgm_hash);
    io.Field(info.tess_consts_dword_offset);
    io.Field(info.tess_consts_ptr_base);
    io.Field(info.hw_stage);
    io.Field(info.sw_stage);
    io.Field(info.mrt_mask);
    io.Field(info.has_fetch_shader);
    io.Field(info.has_bitwise_xor);
    io.Field(info.uses_dma);
    io.Field(info.flattened_ud_buf);
    io.Field(info.loads);
    io.Field(info.stores);
    io.Field(info.readconst_types);
    io.Field(info.uses_patches);
    io.Field(info.has_storage_images);
    io.Field(info.has_discard);
    io.Field(info.has_image_gather);
    io.Field(info.has_image_query);
    io.Field(info.has_flatbuf);
    io.Field(info.uses_buffer_atomic_float_min_max);
    io.Field(info.uses_image_atomic_float_min_max);
    io.Field(info.uses_lane_id);
    io.Field(info.uses_shader_clock);
    io.Field(info.uses_group_quad);
    io.Field(info.uses_group_shuffle);
    io.Field(info.uses_group_ballot);
    io.Field(info.shared_types);
    io.Field(info.uses_fp16);
    io.Field(info.uses_fp64);
    io.Field(info.uses_pack_10_11_11);
    io.Field(info.uses_unpack_10_11_11);
    io.Field(info.uses_buffer_int64_atomics);
    io.Field(info.uses_shared_int64_atomics);
    io.Field(info.stores_tess_level_outer);
    io.Field(info.stores_tess_level_inner);
    io.Field(info.translation_failed);
    io.Field(info.fs_interpolation);
    io.Fields(info.gs_copy_data.num_attrs,info.gs_copy_data.output_vertices,info.gs_copy_data.num_comps);
}
template<class IO,class SpecType> void SpecFields(IO& io,SpecType& spec){
    io.Fields(spec.start,spec.runtime_info,spec.vs_attribs,spec.buffers,spec.images,
        spec.native_image_aliases,spec.native_external_image_aliases,spec.fmasks,spec.samplers);
}
}
void Info::Serialize(Serialization::Archive& ar)const{
    SaveFields save(ar);InfoFields(save,*this);
    save.Field(gs_copy_data.attr_map.size());
    for(const auto& [key,value]:gs_copy_data.attr_map)save.Fields(key,value.first,value.second);
    srt_info.Serialize(ar);
}
bool Info::Deserialize(Serialization::Archive& ar){
    LoadFields load(ar);InfoFields(load,*this);
    size_t count{};load.Field(count);
    if(count>ar.RemainingBytes()/(sizeof(u32)*3))throw std::runtime_error("Invalid cached geometry attribute map");
    gs_copy_data.attr_map.clear();
    for(size_t i=0;i<count;++i){
        u32 key{},component{};IR::Attribute attribute{};load.Fields(key,attribute,component);
        if(!gs_copy_data.attr_map.emplace(key,std::pair{attribute,component}).second)
            throw std::runtime_error("Duplicate cached geometry attribute");
    }
    user_data={};pgm_base=0;
    return srt_info.Deserialize(ar);
}
void Gcn::FetchShaderData::Serialize(Serialization::Archive& ar)const{
    SaveFields save(ar);save.Fields(size,vertex_offset_sgpr,instance_offset_sgpr,attributes);
}
bool Gcn::FetchShaderData::Deserialize(Serialization::Archive& ar){
    LoadFields load(ar);load.Fields(size,vertex_offset_sgpr,instance_offset_sgpr,attributes);return true;
}
void PersistentSrtInfo::Serialize(Serialization::Archive& ar)const{
    SaveFields save(ar);save.Fields(walker_func_size,flattened_bufsize_dw);
    if(walker_func_size){
        if(!walker_func)throw std::logic_error("Missing cached SRT walker");
        save.data.Write(reinterpret_cast<const u8*>(walker_func),walker_func_size);
    }
}
bool PersistentSrtInfo::Deserialize(Serialization::Archive& ar){
    LoadFields load(ar);load.Fields(walker_func_size,flattened_bufsize_dw);
    if(walker_func_size>ar.RemainingBytes())throw std::runtime_error("Short cached SRT walker");
    walker_func=walker_func_size?RegisterWalkerCode(ar.CurrPtr(),walker_func_size):nullptr;
    ar.Advance(walker_func_size);return true;
}
void StageSpecialization::Serialize(Serialization::Archive& ar)const{
    SaveFields save(ar);SpecFields(save,*this);
    save.Field(bitset.to_string());fetch_shader_data.Serialize(ar);
}
bool StageSpecialization::Deserialize(Serialization::Archive& ar){
    LoadFields load(ar);SpecFields(load,*this);
    std::string bits;load.Field(bits);
    if(bits.size()!=MaxStageResources)throw std::runtime_error("Invalid cached specialization bitset");
    bitset=std::bitset<MaxStageResources>(bits);return fetch_shader_data.Deserialize(ar);
}
} // namespace Shader

