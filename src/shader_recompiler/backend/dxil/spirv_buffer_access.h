// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <span>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>
#include <limits>
namespace Shader::Backend::DX12 {
// Offline intermediate rewrite only. Keep other resource declarations and
// actual load/store instructions unchanged; unify SSBO native descriptor class.
// Distinct guest sharps can address the same UAV arena. GLSL450 otherwise lets
// the converter assume that separate SSBO declarations do not alias.
inline std::vector<uint32_t> NativeStorageBufferUavs(std::span<const uint32_t> words) {
    if(words.size()<5||words[0]!=0x07230203u)throw std::runtime_error("Invalid buffer-unification intermediate header");
    std::unordered_map<uint32_t,std::pair<uint32_t,uint32_t>> pointers;
    std::unordered_set<uint32_t> buffer_blocks,variables,structures;
    std::unordered_map<uint32_t,uint32_t> access_decorations;
    std::unordered_set<uint32_t> decoration_groups;
    std::vector<uint32_t> variable_order,group_order;
    constexpr uint32_t restrict_access=1,aliased_access=2,readonly_access=4;
    size_t annotation_begin=words.size();
    for(size_t i=5;i<words.size();){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(!count||count>words.size()-i)throw std::runtime_error("Truncated buffer-unification intermediate");
        if(op==73){ // OpDecorationGroup
            if(count!=2)throw std::runtime_error("Invalid native decoration group");
            if(decoration_groups.insert(words[i+1]).second)group_order.push_back(words[i+1]);
        }
        if((op==74||op==75)&&(count<2||(op==75&&(count-2)%2)))
            throw std::runtime_error("Invalid native decoration group targets");
        if(op==32&&count==4)pointers.emplace(words[i+1],std::pair{words[i+2],words[i+3]}); // OpTypePointer
        if(op==71&&count==3&&words[i+2]==3)buffer_blocks.insert(words[i+1]); // BufferBlock
        if(op==71&&count==3){ // OpDecorate, including decoration group properties
            const auto decoration=words[i+2];
            const auto flag=decoration==19?restrict_access:decoration==20?aliased_access:
                            decoration==24?readonly_access:0;
            if(flag)access_decorations[words[i+1]]|=flag;
        }
        // All annotation forms, including OpDecorateId and string decorations.
        if(annotation_begin==words.size()&&
           ((op>=71&&op<=75)||op==332||op==5632||op==5633))annotation_begin=i;
        i+=count;
    }
    // Legacy Uniform SSBOs can obtain BufferBlock through a decoration group.
    for(size_t i=5;i<words.size();i+=words[i]>>16){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(op==74&&count>=3&&buffer_blocks.contains(words[i+1])) // OpGroupDecorate
            for(size_t j=i+2;j<i+count;++j)buffer_blocks.insert(words[j]);
    }
    for(size_t i=5;i<words.size();i+=words[i]>>16){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(op!=59||count<4)continue; // OpVariable
        const auto pointer=pointers.find(words[i+1]);if(pointer==pointers.end())continue;
        const auto [storage,type]=pointer->second;
        if(storage==12||(storage==2&&buffer_blocks.contains(type))){
            if(variables.insert(words[i+2]).second)variable_order.push_back(words[i+2]);
            structures.insert(type);
        }
    }
    if(variables.empty())return {words.begin(),words.end()};
    if(annotation_begin==words.size())
        throw std::runtime_error("Native SSBO intermediate has no annotation section");
    std::unordered_set<uint32_t> already_aliased;
    for(const auto variable:variable_order)
        if(access_decorations[variable]&aliased_access)already_aliased.insert(variable);
    // Only OpDecorate may decorate a group. Keep arbitrary literal payloads,
    // including strings, but do not clone invalid OpDecorateId/String targets.
    std::unordered_map<uint32_t,std::vector<size_t>> group_annotations;
    for(size_t i=5;i<words.size();i+=words[i]>>16){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(count<2||!decoration_groups.contains(words[i+1]))continue;
        if(op==332||op==5632)
            throw std::runtime_error("OpDecorateId/String cannot target a native decoration group");
        if(op==71&&count>=3)group_annotations[words[i+1]].push_back(i);
    }
    // Split applications per target: IDs outside SSBO variables/pointee types
    // retain every original decoration. Reuse one sanitized clone per group.
    std::unordered_set<uint32_t> split_groups;
    for(size_t i=5;i<words.size();i+=words[i]>>16){
        const auto count=words[i]>>16,op=words[i]&65535;
        if((op!=74&&op!=75)||count<3)continue;
        const auto found=access_decorations.find(words[i+1]);
        if(found==access_decorations.end())continue;
        const auto flags=found->second;
        if(op==74){
            for(size_t j=i+2;j<i+count;++j){
                if(!variables.contains(words[j]))continue;
                if(flags&(restrict_access|readonly_access))
                    split_groups.insert(words[i+1]);
                if(flags&aliased_access)already_aliased.insert(words[j]);
            }
        }else{
            for(size_t j=i+2;j+1<i+count;j+=2)
                if(structures.contains(words[j])&&(flags&(restrict_access|readonly_access)))
                    split_groups.insert(words[i+1]);
        }
    }
    std::unordered_map<uint32_t,uint32_t> cloned_groups;
    std::vector<uint32_t> clone_annotations;
    auto bound=words[3];
    for(const auto group:group_order)if(split_groups.contains(group)){
        const auto& annotations=group_annotations[group];
        bool nonempty=false;
        for(const auto i:annotations)if(words[i+2]!=19&&words[i+2]!=24){nonempty=true;break;}
        // A group containing only removed qualifiers needs no replacement ID.
        if(!nonempty){cloned_groups.emplace(group,0);continue;}
        if(!bound||bound==std::numeric_limits<uint32_t>::max())
            throw std::runtime_error("Native decoration group ID bound overflows");
        const auto clone=bound++;
        cloned_groups.emplace(group,clone);
        for(const auto i:annotations){
            if(words[i+2]==19||words[i+2]==24)continue;
            const auto count=words[i]>>16;
            clone_annotations.push_back(words[i]);
            clone_annotations.push_back(clone);
            clone_annotations.insert(clone_annotations.end(),words.begin()+i+2,words.begin()+i+count);
        }
        // Group decorations MUST precede the OpDecorationGroup declaration.
        clone_annotations.insert(clone_annotations.end(),{(2u<<16)|73u,clone});
    }
    for(const auto group:split_groups)if(!cloned_groups.contains(group))
        throw std::runtime_error("Native decoration group was not declared");
    std::vector<uint32_t> result(words.begin(),words.begin()+5);
    result[3]=bound;
    result.reserve(words.size()+3*variable_order.size());
    for(size_t i=5;i<words.size();){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(i==annotation_begin){
            for(const auto variable:variable_order)
                if(!already_aliased.contains(variable))
                    result.insert(result.end(),{(3u<<16)|71u,variable,20u}); // Aliased
            result.insert(result.end(),clone_annotations.begin(),clone_annotations.end());
        }
        const bool variable_restriction=op==71&&count==3&&
            (words[i+2]==19||words[i+2]==24)&&variables.contains(words[i+1]);
        const bool member_readonly=op==72&&count==4&&words[i+3]==24&&structures.contains(words[i+1]);
        if((op==74||op==75)&&count>=3&&cloned_groups.contains(words[i+1])){
            const auto clone=cloned_groups.at(words[i+1]);
            std::vector<uint32_t> original_targets{0,words[i+1]},affected_targets{0,clone};
            const size_t stride=op==75?2:1;
            for(size_t j=i+2;j<i+count;j+=stride){
                const bool affected=op==74?variables.contains(words[j]):structures.contains(words[j]);
                auto& targets=affected?affected_targets:original_targets;
                targets.insert(targets.end(),words.begin()+j,words.begin()+j+stride);
            }
            if(original_targets.size()>2){
                original_targets[0]=(uint32_t(original_targets.size())<<16)|op;
                result.insert(result.end(),original_targets.begin(),original_targets.end());
            }
            if(clone&&affected_targets.size()>2){
                affected_targets[0]=(uint32_t(affected_targets.size())<<16)|op;
                result.insert(result.end(),affected_targets.begin(),affected_targets.end());
            }
        }else if(!variable_restriction&&!member_readonly)
            result.insert(result.end(),words.begin()+i,words.begin()+i+count);
        i+=count;
    }
    return result;
}
}
