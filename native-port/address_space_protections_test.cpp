// SPDX-License-Identifier: GPL-2.0-or-later
#include <windows.h>
#include "core/address_space.h"
#include "common/alignment.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

static void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static DWORD Protection(VAddr address){MEMORY_BASIC_INFORMATION info{};
    Require(VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof(info))==sizeof(info),"VirtualQuery failed");
    Require(info.State==MEM_COMMIT,"Expected committed neighbouring mapping");return info.Protect;}
int main(int argc,char** argv){try{
    if(argc!=4)throw std::invalid_argument("Expected fixture wrapper arguments");
    Core::AddressSpace space;
    const auto base=Common::AlignUp(space.SystemManagedVirtualBase()+0x100000ULL,0x10000ULL);
    constexpr u64 bytes=0x40000;
    for(const u64 removed:std::array<u64,2>{0x10000,0x4000}){
        Require(space.Map(base,bytes,0)==reinterpret_cast<void*>(base),"Physical parent mapping moved");
        std::array<u8,bytes> expected{};for(u64 i=0;i<bytes;++i)expected[i]=u8((i*17+(i>>12))&255);
        memcpy(reinterpret_cast<void*>(base),expected.data(),bytes);
        const std::array<VAddr,3> watched{base+0x1000,base+0x31000,base+0x22000};
        const std::array<DWORD,3> flags{PAGE_READONLY,PAGE_NOACCESS,PAGE_READWRITE|PAGE_GUARD};
        for(size_t i=0;i<watched.size();++i){DWORD old{};
            Require(VirtualProtect(reinterpret_cast<void*>(watched[i]),0x1000,flags[i],&old)!=0,"Installing neighbouring watch failed");}
        const auto middle=base+0x10000;u64 actual=removed;
        space.Unmap(middle,&actual);Require(actual==removed,"Unmap changed requested extent");
        for(size_t i=0;i<watched.size();++i)Require(Protection(watched[i])==flags[i],"Split/unmap lost neighbouring page protection");
        Require(space.Map(middle,removed,0x10000)==reinterpret_cast<void*>(middle),"Physical middle remapping moved");
        Require(Protection(middle)==PAGE_READWRITE,"Fresh middle retained an unrelated neighbour protection");
        for(size_t i=0;i<watched.size();++i){Require(Protection(watched[i])==flags[i],"Remap lost neighbouring page protection");DWORD old{};
            Require(VirtualProtect(reinterpret_cast<void*>(watched[i]),0x1000,PAGE_READWRITE,&old)!=0,"Removing fixture watch failed");}
        Require(memcmp(reinterpret_cast<const void*>(base),expected.data(),bytes)==0,"Split/remap changed physical backing contents or padding");
        for(u64 offset=0;offset<bytes;offset+=0x4000){
            const auto* entry=space.BackingPages().find((base+offset)>>Core::AddressSpace::Traits::PAGE_BITS);
            Require(entry&&*entry==space.BackingBase()+offset,"Remap broke physical alias lookup");
        }
        u64 release=bytes;space.Unmap(base,&release);Require(release==bytes,"Final unmap changed extent");
    }
    puts("PASS: original neighbouring read-only/no-access/guard protections across 64/16-KiB split/remap, physical bytes and backing aliases");return 0;
}catch(const std::exception& error){fprintf(stderr,"FAIL: %s\n",error.what());return 1;}}
