// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/stream_output_layout.h"
#include "video_core/renderer_d3d12/persistent_cache.h"
#include "video_core/renderer_d3d12/primitive_stream_arguments.h"
#include <array>

namespace NativeDX12::Tests {
inline void CheckStreamOutputLayout(){
    for(const u32 patch:std::array<u32,2>{3,4}){
        const auto complete=AuxiliaryPointStreamRange(patch,patch*2,2,4);
        const auto incomplete=AuxiliaryPointStreamRange(patch,patch-1,1,4);
        if(complete.capacity_vertices!=24||complete.stride!=32||incomplete.capacity_vertices!=3)
            throw std::runtime_error("Fixed auxiliary tessellation bound lost complete patches/instances or incomplete producer effects");
    }
    bool overflow{};try{(void)AuxiliaryPointStreamRange(3,UINT32_MAX,UINT32_MAX,4);}
    catch(const std::overflow_error&){overflow=true;}
    if(!overflow)throw std::runtime_error("Auxiliary tessellation count multiplication wrapped");
    const PrimitiveStreamPartitions parts(PrimitiveStreamRange::Make(32,15,3,0,15),2,256,622592);
    if(parts.Count()!=3)throw std::runtime_error("Capture budget did not partition all five input primitives");
    for(u32 index=0;index<3;++index){const auto part=parts.At(index);
        if(part.input.first!=index*6||part.input.limit!=(index==2?3u:6u)||
           part.first!=(index==0)||part.last!=(index==2)||part.capture.bytes!=(index==2?311296u:622592u))
            throw std::runtime_error("Primitive stream partition lost exact coverage/order/capture extent");}
    for(const u32 adjacency:std::array<u32,2>{4,6}){
        const PrimitiveStreamPartitions adjacent(PrimitiveStreamRange::Make(16,adjacency*5,adjacency,0,adjacency*5),1,1,1216);
        u32 covered{};for(u32 index=0;index<adjacent.Count();++index){const auto part=adjacent.At(index);
            if(part.input.first!=covered||part.input.limit%adjacency)throw std::runtime_error("Adjacency primitive split across capture arenas");
            covered+=part.input.limit;}
        if(covered!=adjacency*5)throw std::runtime_error("Adjacency capture partition truncated the original stream");
    }
    for(unsigned invalid=0;invalid<4;++invalid){bool rejected{};
        try{if(invalid==3)(void)parts.At(3);
            else (void)PrimitiveStreamPartitions(PrimitiveStreamRange::Make(32,15,3,0,15),
                invalid==0?0u:invalid==2?UINT32_MAX:2u,invalid==2?UINT32_MAX:256u,invalid==1?311295u:UINT64_MAX);}
        catch(const std::invalid_argument&){if(invalid!=0)throw;rejected=true;}
        catch(const std::overflow_error&){if(invalid!=1&&invalid!=2)throw;rejected=true;}
        catch(const std::out_of_range&){if(invalid!=3)throw;rejected=true;}
        if(!rejected)throw std::runtime_error("Malformed/undersized/overflowing primitive partition accepted");
    }
    const auto full=PrimitiveStreamRange::Make(16,24,3,0,24);
    const auto tail=PrimitiveStreamRange::Make(16,24,3,18,6);
    (void)PrimitiveStreamRange::Make(16,24,4,0,24);
    (void)PrimitiveStreamRange::Make(16,24,6,6,18);
    if(full.limit!=24||tail.first!=18||tail.limit!=6)
        throw std::runtime_error("Primitive stream partition changed its exact extent");
    for(const auto invalid:{std::array<u32,5>{0,24,3,0,24},{16,24,0,0,24},{16,24,3,1,6},
        {16,24,3,18,9},{16,24,3,0,0},{16,UINT32_MAX,1,0,1},{16,30,5,0,30}}){
        bool rejected{};
        try{(void)PrimitiveStreamRange::Make(invalid[0],invalid[1],invalid[2],invalid[3],invalid[4]);}
        catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("Malformed primitive stream range accepted");
    }
    const auto key=[](const D3D12_STREAM_OUTPUT_DESC& desc){
        CacheKey result;StreamOutputLayout::AppendKey(result,desc);return result.bytes;
    };
    char position[]="POSITION";
    const std::array<D3D12_SO_DECLARATION_ENTRY,3> original{{
        {0,position,0,0,4,0},{0,nullptr,0,0,2,0},{0,"TEXCOORD",1,1,2,1}}};
    auto entries=original;std::array<UINT,2> strides{24,8};
    D3D12_STREAM_OUTPUT_DESC desc{entries.data(),UINT(entries.size()),strides.data(),UINT(strides.size()),D3D12_SO_NO_RASTERIZED_STREAM};
    const auto expected=key(desc);StreamOutputLayout owned(desc);
    position[0]='X';entries[0].SemanticIndex=3;strides[0]=32;
    if(key(owned.Desc())!=expected||key(desc)==expected)
        throw std::runtime_error("Primitive stream layout retained temporary declaration/name/stride pointers");
    position[0]='P';entries=original;strides={24,8};
    StreamOutputLayout moved(std::move(owned));
    if(key(moved.Desc())!=expected||moved.Desc().pSODeclaration[1].SemanticName)
        throw std::runtime_error("Moved primitive stream layout lost semantic/gap ownership");
    Serialization::Archive archive;Serialization::Writer save(archive);moved.Save(save);
    Serialization::Archive input(archive.TakeOff());Serialization::Reader load(input);
    auto restored=StreamOutputLayout::Load(load);
    if(!input.IsEoS()||key(restored.Desc())!=expected||restored.Desc().pSODeclaration[1].SemanticName)
        throw std::runtime_error("Primitive stream layout cache roundtrip changed declarations or gaps");
    for(unsigned field=0;field<9;++field){
        entries=original;strides={24,8};auto changed=desc;
        switch(field){
        case 0:entries[2].SemanticName="COLOR";break;
        case 1:entries[2].SemanticIndex=2;break;
        case 2:entries[2].StartComponent=0;break;
        case 3:entries[2].ComponentCount=1;break;
        case 4:entries[2].OutputSlot=0;break;
        case 5:entries[2].Stream=1;break;
        case 6:strides[1]=12;break;
        case 7:changed.RasterizedStream=0;break;
        case 8:entries[1].SemanticName="GAP";break;
        }
        if(key(changed)==expected)throw std::runtime_error("Distinct primitive stream layout collided in the pipeline key");
    }
    for(unsigned invalid=0;invalid<8;++invalid){
        entries=original;strides={24,8};auto bad=desc;
        switch(invalid){
        case 0:bad.pSODeclaration=nullptr;break;case 1:bad.pBufferStrides=nullptr;break;
        case 2:entries[0].Stream=D3D12_SO_STREAM_COUNT;break;
        case 3:entries[0].ComponentCount=0;break;case 4:entries[0].StartComponent=1;break;
        case 5:entries[0].OutputSlot=D3D12_SO_BUFFER_SLOT_COUNT;break;
        case 6:strides[0]=3;break;case 7:bad.RasterizedStream=D3D12_SO_STREAM_COUNT;break;
        }
        bool rejected{};try{StreamOutputLayout value(bad);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("Malformed primitive stream layout was accepted");
    }
    Serialization::Archive bad_archive;Serialization::Writer bad_save(bad_archive);
    bad_save.Write(UINT32_MAX);bad_save.Write(UINT(0));bad_save.Write(UINT(0));
    Serialization::Archive bad_input(bad_archive.TakeOff());Serialization::Reader bad_load(bad_input);
    bool rejected{};try{auto value=StreamOutputLayout::Load(bad_load);}catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("Malformed primitive stream cache extent was accepted");
}
} // namespace NativeDX12::Tests
