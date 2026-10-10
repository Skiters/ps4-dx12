// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <span>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <cstdint>
namespace Shader::Backend::DX12 {
// Offline intermediate rewrite only. Keep image/UBO access decorations and
// actual load/store instructions unchanged; unify SSBO native descriptor class.
inline std::vector<uint32_t> NativeStorageBufferUavs(std::span<const uint32_t> words) {
    if(words.size()<5||words[0]!=0x07230203u)throw std::runtime_error("Invalid buffer-unification intermediate header");
    std::unordered_map<uint32_t,std::pair<uint32_t,uint32_t>> pointers;
    std::unordered_set<uint32_t> buffer_blocks,variables,structures;
    for(size_t i=5;i<words.size();){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(!count||count>words.size()-i)throw std::runtime_error("Truncated buffer-unification intermediate");
        if(op==32&&count==4)pointers.emplace(words[i+1],std::pair{words[i+2],words[i+3]}); // OpTypePointer
        if(op==71&&count==3&&words[i+2]==3)buffer_blocks.insert(words[i+1]); // BufferBlock
        i+=count;
    }
    for(size_t i=5;i<words.size();i+=words[i]>>16){
        const auto count=words[i]>>16,op=words[i]&65535;
        if(op!=59||count<4)continue; // OpVariable
        const auto pointer=pointers.find(words[i+1]);if(pointer==pointers.end())continue;
        const auto [storage,type]=pointer->second;
        if(storage==12||(storage==2&&buffer_blocks.contains(type))){
            variables.insert(words[i+2]);structures.insert(type);
        }
    }
    std::vector<uint32_t> result(words.begin(),words.begin()+5);result.reserve(words.size());
    for(size_t i=5;i<words.size();){
        const auto count=words[i]>>16,op=words[i]&65535;
        const bool variable_readonly=op==71&&count==3&&words[i+2]==24&&variables.contains(words[i+1]);
        const bool member_readonly=op==72&&count==4&&words[i+3]==24&&structures.contains(words[i+1]);
        if(!variable_readonly&&!member_readonly)result.insert(result.end(),words.begin()+i,words.begin()+i+count);
        i+=count;
    }
    return result;
}
}
