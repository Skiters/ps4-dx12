// SPDX-License-Identifier: GPL-2.0-or-later
#include "shader_recompiler/backend/dxil/spirv_buffer_access.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
using Words=std::vector<uint32_t>;
namespace {
Words Module(bool grouped){
    Words words{0x07230203,0x00010500,0,64,0};
    const auto emit=[&](uint32_t opcode,std::initializer_list<uint32_t> args){
        words.push_back((uint32_t(args.size()+1)<<16)|opcode);words.insert(words.end(),args);
    };
    if(grouped){emit(71,{8,19});emit(71,{8,24});emit(71,{8,23});emit(73,{8});emit(74,{8,5,20});}
    else{emit(71,{5,19});emit(71,{5,24});emit(71,{20,19});emit(71,{20,24});emit(71,{20,23});}
    emit(71,{3,2});emit(72,{3,0,35,0});emit(71,{21,2});emit(72,{21,0,35,0});
    emit(21,{1,32,0});emit(30,{3,1});emit(30,{21,1});emit(32,{4,12,3});emit(32,{18,2,21});
    emit(59,{4,5,12});emit(59,{4,6,12});emit(59,{18,20,2});
    emit(61,{3,50,5});emit(62,{6,50});
    return words;
}
std::map<uint32_t,std::multiset<uint32_t>> Access(const Words& words){
    std::map<uint32_t,std::multiset<uint32_t>> result;
    for(size_t i=5;i<words.size();i+=words[i]>>16)
        if((words[i]&65535)==71&&(words[i]>>16)==3)result[words[i+1]].insert(words[i+2]);
    for(size_t i=5;i<words.size();i+=words[i]>>16)
        if((words[i]&65535)==74)for(size_t j=i+2;j<i+(words[i]>>16);++j){
            const auto inherited=result[words[i+1]];result[words[j]].insert(inherited.begin(),inherited.end());
        }
    return result;
}
Words MemoryInstructions(const Words& words){
    Words result;
    for(size_t i=5;i<words.size();i+=words[i]>>16)if((words[i]&65535)==61||(words[i]&65535)==62)
        result.insert(result.end(),words.begin()+i,words.begin()+i+(words[i]>>16));
    return result;
}
}
int main(){try{
    for(bool grouped:{false,true}){
        const auto input=Module(grouped),output=Shader::Backend::DX12::NativeStorageBufferUavs(input);
        const auto before=Access(input),after=Access(output);
        for(uint32_t id:{5u,6u}){
            const auto& access=after.at(id);
            if(access.count(19)||access.count(24)||access.count(20)!=1||access.count(23)!=1)
                throw std::runtime_error("Native SSBO alias is not uniquely coherent and read/write");
        }
        if(before.at(20)!=after.at(20)||MemoryInstructions(input)!=MemoryInstructions(output))
            throw std::runtime_error("Native buffer rewrite changed UBO access or actual memory operations");
        if(output!=Shader::Backend::DX12::NativeStorageBufferUavs(output))
            throw std::runtime_error("Native SSBO access rewrite is not idempotent");
    }
    puts("PASS: direct/grouped SSBO aliases are coherent; UBO qualifiers and memory instructions survive; rewrite is idempotent");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
