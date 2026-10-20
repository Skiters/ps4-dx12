// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_indirect_arguments.h"
#include "video_core/renderer_d3d12/guest_indirect_pack.h"
#include <limits>
namespace NativeDX12::Tests {
inline void CheckGuestIndirectArguments(){
    const auto require=[](bool value){if(!value)throw std::runtime_error("Guest indirect argument ABI/bounds regression");};
    std::array<std::byte,52> source;source.fill(std::byte{0xcd});
    const std::array<u32,4> first{3,2,7,9},second{8,4,11,13};
    memcpy(source.data(),first.data(),16);memcpy(source.data()+32,second.data(),16);
    const auto plain=GuestIndirectLayout::Make(false,0x1000,4,32,2);
    require(plain.address==0x1004&&plain.bytes==48);
    const auto commands=plain.Snapshot(source);
    source.fill(std::byte{0}); // All commands must already be immutable.
    require(commands.size()==2&&commands[0].count==3&&commands[0].instances==2&&
        commands[0].first_vertex==7&&commands[0].first_instance==9&&commands[0].base_vertex==0&&commands[0].first_index==0);
    require(commands[1].count==8&&commands[1].instances==4&&commands[1].first_vertex==11&&commands[1].first_instance==13);
    const std::array<u32,5> indexed{12,5,17,0x80000000u,19};
    memcpy(source.data(),indexed.data(),20);
    const auto index_layout=GuestIndirectLayout::Make(true,0,0,20,1);
    const auto index=index_layout.Snapshot(source);
    require(index.size()==1&&index[0].count==12&&index[0].instances==5&&index[0].first_vertex==0&&
        index[0].first_index==17&&index[0].base_vertex==std::numeric_limits<s32>::min()&&index[0].first_instance==19);
    require(GuestIndirectLayout::Make(false,0,0,16,0).Snapshot({}).empty());
    require(GuestIndirectLayout::Make(true,UINT64_MAX-20,0,20,1).bytes==20);
    // Count-buffer reduction must be applied before validating the argument
    // extent: a maximum possible batch can overflow while one actual draw fits.
    require(GuestIndirectLayout::Make(false,UINT64_MAX-64,0,16,1).bytes==16);
    const auto reject=[&](auto action){bool failed{};try{action();}catch(const std::out_of_range&){failed=true;}require(failed);};
    reject([&]{(void)plain.Snapshot(std::span(source).first(47));});
    reject([]{(void)GuestIndirectLayout::Make(false,0,0,12,1);});
    reject([]{(void)GuestIndirectLayout::Make(true,0,0,16,1);});
    reject([]{(void)GuestIndirectLayout::Make(false,0,0,18,1);});
    reject([]{(void)GuestIndirectLayout::Make(false,UINT64_MAX,4,16,1);});
    reject([]{(void)GuestIndirectLayout::Make(true,UINT64_MAX-19,0,20,1);});
    reject([]{(void)GuestIndirectLayout::Make(false,UINT64_MAX-64,0,16,UINT32_MAX);});
    const auto native=NativeIndirectLayout::Make(true,4,32,3);
    require(native.offset==4&&native.bytes==96&&native.stride==32&&native.count==3);
    require(NativeIndirectLayout::Make(false,0,0,2).bytes==32);
    require(NativeIndirectLayout::Make(true,0,0,2).bytes==40);
    require(NativeIndirectLayout::Make(false,0,0,0).bytes==0);
    require(NativeIndirectLayout::Make(false,8,0,3,true).bytes==72);
    require(NativeIndirectLayout::Make(true,8,0,3,true).bytes==96);
    reject([]{(void)NativeIndirectLayout::Make(true,0,16,1);});
    reject([]{(void)NativeIndirectLayout::Make(false,2,16,1);});
    reject([]{(void)NativeIndirectLayout::Make(false,0,18,1);});
    reject([]{(void)NativeIndirectLayout::Make(false,UINT64_MAX-15,16,1);});
    reject([]{(void)NativeIndirectLayout::Make(true,0,24,1,true);});
    reject([]{(void)NativeIndirectLayout::Make(false,4,24,1,true);});
    const auto pack=GuestIndirectPackPlan::Make(false,32,3);
    require(pack.source_bytes==80&&pack.command_stride==24&&pack.command_bytes==72&&
        pack.runtime_stride==1024&&pack.runtime_bytes==3072);
    const auto indexed_pack=GuestIndirectPackPlan::Make(true,32,3);
    require(indexed_pack.source_bytes==84&&indexed_pack.command_stride==32&&indexed_pack.command_bytes==96);
    constexpr u32 maximum=UINT32_MAX/1024;
    require(GuestIndirectPackPlan::Make(false,16,maximum).runtime_bytes==u64(maximum)*1024);
    bool overflow{};try{(void)GuestIndirectPackPlan::Make(false,16,maximum+1);}catch(const std::overflow_error&){overflow=true;}
    require(overflow);overflow=false;
    try{(void)GuestIndirectPackPlan::Make(false,UINT32_MAX-3,2);}catch(const std::overflow_error&){overflow=true;}
    require(overflow);
    bool empty{};try{(void)GuestIndirectPackPlan::Make(false,16,0);}catch(const std::invalid_argument&){empty=true;}
    require(empty);
}
} // namespace NativeDX12::Tests
