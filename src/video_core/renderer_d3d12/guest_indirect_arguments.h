// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "common/types.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

namespace NativeDX12 {
// ExecuteIndirect reserves ByteStride for every maximum command, including
// trailing padding. This is stricter than the CPU decoder's last payload.
struct NativeIndirectLayout {
    u64 offset,bytes;
    u32 stride,count;
    static NativeIndirectLayout Make(bool indexed,u64 offset,u32 stride,u32 count){
        const u32 size=indexed?20:16;
        if(!stride)stride=size;
        const u64 bytes=u64(stride)*count;
        if(stride<size||stride%4||offset%4||offset>UINT64_MAX-bytes)
            throw std::out_of_range("Invalid native indirect batch extent/stride");
        return {offset,bytes,stride,count};
    }
};
struct GuestIndirectDraw {
    u32 count,instances,first_vertex,first_instance;
    s32 base_vertex;
    u32 first_index;
};
// Vulkan's indirect commands have the same word ABI. Keep command bytes
// distinct from stride padding: no padding is retained in the CPU snapshot.
struct GuestIndirectLayout {
    u64 address,bytes;
    u32 stride,count,command_bytes;
    bool indexed;
    static GuestIndirectLayout Make(bool indexed,u64 address,u32 offset,u32 stride,u32 count){
        const u32 command_bytes=indexed?20:16;
        if(stride<command_bytes||stride%4||address>UINT64_MAX-offset)
            throw std::out_of_range("Invalid native guest indirect draw layout");
        const u64 start=address+offset;
        const u64 bytes=count?u64(count-1)*stride+command_bytes:0;
        if(start>UINT64_MAX-bytes)
            throw std::out_of_range("Native indirect draw argument overflow");
        return {start,bytes,stride,count,command_bytes,indexed};
    }
    std::vector<GuestIndirectDraw> Snapshot(std::span<const std::byte> source)const{
        if(source.size()<bytes)throw std::out_of_range("Truncated native indirect argument snapshot");
        std::vector<GuestIndirectDraw> result;result.reserve(count);
        for(u32 i=0;i<count;++i){
            std::array<u32,5> raw{};
            memcpy(raw.data(),source.data()+u64(i)*stride,command_bytes);
            result.push_back({raw[0],raw[1],indexed?0:raw[2],indexed?raw[4]:raw[3],
                indexed?std::bit_cast<s32>(raw[3]):0,indexed?raw[2]:0});
        }
        return result;
    }
};
} // namespace NativeDX12
