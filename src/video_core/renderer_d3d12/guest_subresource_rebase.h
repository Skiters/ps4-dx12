// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "guest_texture_cache.h"
#include "guest_tile_address.h"

namespace NativeDX12 {

// Physical addressing only: callers separately prove the format, logical
// dimensions, full parent ownership and canonical binding/prefix lifetime.
inline bool MatchesGuestSliceAddressing(const GuestTextureDesc& parent,
    const GuestTextureLayout& parent_layout,uint32_t mip,const GuestTextureDesc& local,
    const GuestTextureLayout& local_layout,uint32_t first_layer){
    if(mip>=parent_layout.mips.size()||local.levels!=1||local_layout.mips.size()!=1||
       !parent.layers||!local.layers||parent.layers>2048||local.layers>2048||
       first_layer>=parent.layers||local.layers>parent.layers-first_layer||
       parent.volume_depth||local.volume_depth||parent.one_dimensional||local.one_dimensional||
       parent.tile_mode!=local.tile_mode||parent.alt_tile!=local.alt_tile||
       parent.tile_mode>26||parent.tile_mode==8||parent.samples!=1||local.samples!=1||
       parent_layout.bytes_per_pixel!=local_layout.bytes_per_pixel||
       (parent_layout.bytes_per_pixel!=8&&parent_layout.bytes_per_pixel!=16))return false;
    const auto& a=parent_layout.mips[mip];const auto& b=local_layout.mips[0];
    const auto thickness=a.thickness;
    if((thickness!=1&&thickness!=4&&thickness!=8)||b.thickness!=thickness||
       a.macro!=b.macro||b.offset||!a.pitch||!a.height||a.pitch>32768||a.height>32768||
       a.pitch!=b.pitch||a.height!=b.height||a.slice_bytes!=b.slice_bytes||
       a.slice_bytes!=u64(a.pitch)*a.height*parent_layout.bytes_per_pixel||
       first_layer%thickness)return false;
    const auto rounded=[thickness](uint32_t layers){return (layers+thickness-1)&~(thickness-1);};
    const auto parent_layers=rounded(parent.layers),local_layers=rounded(local.layers);
    if(first_layer>parent_layers||local_layers>parent_layers-first_layer||
       a.offset>parent_layout.guest_bytes||
       a.slice_bytes*parent_layers>parent_layout.guest_bytes-a.offset||
       local_layout.guest_bytes!=b.slice_bytes*local_layers)return false;
    // A thickness-aligned origin retains every low slice bit used by Pixel.
    // Micro's only remaining slice term advances by whole physical groups.
    if(!a.macro)return true;
    const auto mode=AmdGpu::TileMode(parent.tile_mode);
    const auto pipe=AmdGpu::GetPipeConfig(mode);
    if(pipe!=AmdGpu::PipeConfig::P2&&pipe!=AmdGpu::PipeConfig::P8_32x32_8x16&&
       pipe!=AmdGpu::PipeConfig::P8_32x32_16x16)return false;
    const GuestTileAddress address(mode,parent_layout.bytes_per_pixel,1);
    if(!AmdGpu::IsMacroTiled(address.array)||address.thickness!=thickness||!address.split||
       !std::has_single_bit(address.pipes)||!std::has_single_bit(address.banks)||
       (address.banks!=2&&address.banks!=4&&address.banks!=8&&address.banks!=16)||
       !address.bank_width||!address.bank_height||!address.aspect)return false;
    const u32 mw=8*address.bank_width*address.pipes*address.aspect,
        mh=8*address.bank_height*address.banks/address.aspect;
    if(!mw||!mh||a.pitch%mw||a.height%mh)return false;
    u64 micro_bytes=64ull*thickness*address.bytes;
    u32 splits=1;
    if(micro_bytes>address.split&&thickness==1){
        if(micro_bytes%address.split)return false;
        splits=u32(micro_bytes/address.split);micro_bytes=address.split;
    }
    const auto macro_bytes=micro_bytes*(mw/8)*(mh/8)/(address.pipes*address.banks);
    const auto group=u64(a.pitch/mw)*(a.height/mh)*macro_bytes*splits;
    // Inserting pipe/bank bits must turn one group into exactly its physical
    // slice stride. A 256-byte boundary prevents carries into those XOR bits.
    if(group%256||group*address.pipes*address.banks!=a.slice_bytes*thickness)return false;
    for(u32 layer=0;layer<local.layers;layer+=thickness){
        // Pipe/Bank separate coordinate XORs from these slice corrections.
        // Equal corrections at (0,0), with equal low slice bits, therefore
        // prove all x/y and tile-split positions, not just one sampled texel.
        if(address.Pipe(0,0,first_layer+layer)!=address.Pipe(0,0,layer)||
           address.Bank(0,0,first_layer+layer,0,parent.bank_swizzle)!=
           address.Bank(0,0,layer,0,local.bank_swizzle))return false;
    }
    return true;
}

} // namespace NativeDX12
