// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "video_core/renderer_d3d12/guest_subresource_rebase.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace NativeDX12 {
// Authored, UNRUN CPU address controls. No GPU allocation or owner is created.
inline void RunGuestSubresourceRebaseChecks() {
    struct Case {
        GuestTextureDesc parent,local;
        GuestTextureLayout parent_layout,local_layout;
        u32 mip,first;
    };
    const auto require=[](bool valid,const char* label){
        if(!valid)throw std::runtime_error(std::string("Guest slice addressing: ")+label);
    };
    // Parent dimensions are actual BC pixels; local dimensions are UINT words.
    // All examples select mip1 with nonzero physical mip offset.
    const auto make=[](u32 tile,u32 bytes,u32 word_width,u32 word_height,u32 layers,
                       u32 first,u32 local_layers,u32 local_bank=0){
        Case c{};c.mip=1;c.first=first;
        c.parent.address=0x100000000ULL;c.parent.width=c.parent.pitch=word_width*4;
        c.parent.height=word_height*4;c.parent.levels=2;c.parent.layers=c.parent.view_layers=layers;
        c.parent.tile_mode=tile;c.parent.data_format=bytes==16?37:35;c.parent.number_format=0;
        c.parent_layout=LayoutGuestTexture(c.parent);
        const auto& physical=c.parent_layout.mips.at(c.mip);
        c.local.address=c.parent.address+physical.offset+u64(first)*physical.slice_bytes;
        c.local.width=c.local.pitch=word_width/2;c.local.height=word_height/2;
        c.local.levels=1;c.local.layers=c.local.view_layers=local_layers;c.local.tile_mode=tile;
        c.local.data_format=bytes==16?14:11;c.local.number_format=4;c.local.writable=true;
        c.local.bank_swizzle=local_bank;c.local_layout=LayoutGuestTexture(c.local);return c;
    };
    // This oracle never calls the new admission helper. It checks the existing
    // address function at EVERY logical block and local layer, including every
    // low z bit and tile-split position. No guest backing memory is accessed.
    const auto same_addresses=[&](const Case& c){
        const auto& a=c.parent_layout.mips.at(c.mip);
        const u32 width=(std::max(c.parent.width>>c.mip,1u)+c.parent_layout.block_extent-1)/c.parent_layout.block_extent;
        const u32 height=(std::max(c.parent.height>>c.mip,1u)+c.parent_layout.block_extent-1)/c.parent_layout.block_extent;
        require(c.local.width==width&&c.local.height==height,"oracle logical extent mismatch");
        require(c.local.address==c.parent.address+a.offset+u64(c.first)*a.slice_bytes,"oracle absolute origin mismatch");
        u64 checked=0;
        for(u32 layer=0;layer<c.local.layers;++layer)for(u32 y=0;y<height;++y)for(u32 x=0;x<width;++x){
            const auto parent=GuestPixelOffset(c.parent,c.parent_layout,c.mip,c.first+layer,x,y);
            const auto local=GuestPixelOffset(c.local,c.local_layout,0,layer,x,y);
            require(parent+c.parent_layout.bytes_per_pixel<=c.parent_layout.guest_bytes&&
                local+c.local_layout.bytes_per_pixel<=c.local_layout.guest_bytes,"oracle address escaped physical allocation");
            ++checked;
            if(c.parent.address+parent!=c.local.address+local)return false;
        }
        require(checked!=0,"empty oracle enumeration");return true;
    };
    const auto accept=[&](const Case& c,bool macro,u32 thickness,const char* label){
        const auto& a=c.parent_layout.mips.at(c.mip);const auto& b=c.local_layout.mips.at(0);
        require(a.offset&&a.macro==macro&&b.macro==macro&&a.thickness==thickness&&b.thickness==thickness,label);
        require(MatchesGuestSliceAddressing(c.parent,c.parent_layout,c.mip,c.local,c.local_layout,c.first),label);
        require(same_addresses(c),label);
    };
    const auto reject=[&](const Case& c,const char* label,bool address_counterexample=false){
        require(!MatchesGuestSliceAddressing(c.parent,c.parent_layout,c.mip,c.local,c.local_layout,c.first),label);
        if(address_counterexample)require(!same_addresses(c),label);
    };

    // tile14 / 16B: eight pipes, eight banks, bank rotation3 per thin layer.
    // Pitch128/height64 covers two complete64x64 macrotiles.
    const auto thin=make(14,16,256,128,12,0,3);
    accept(thin,true,1,"thin macro mip boundary K0");
    const auto period=make(14,16,256,128,12,8,3);
    accept(period,true,1,"thin macro K8 rotation period");
    const auto compensated=make(14,16,256,128,12,1,3,3);
    accept(compensated,true,1,"thin macro K1 compensated local bank across three groups");
    auto wrong_bank=compensated;wrong_bank.local.bank_swizzle=0;
    reject(wrong_bank,"uncompensated thin bank rotation",true);
    auto wrong_zero_bank=thin;wrong_zero_bank.local.bank_swizzle=1;
    reject(wrong_zero_bank,"wrong bank at zero origin",true);

    // tile14 / 8B: sixteen banks, rotation7; K16 preserves the correction.
    const auto bc1=make(14,8,256,128,20,16,2);
    accept(bc1,true,1,"BC1 word-size macro rotation period");
    // tile0 / 16B: depth microtiles split1024 bytes into sixteen64B pieces;
    // selected physical256x128 contains every split position.
    const auto split=make(0,16,512,256,4,0,2);
    accept(split,true,1,"depth macro64B split positions");

    // tile15 is 3D thin tiling: eight pipes rotate by3*slice. A bank change
    // cannot repair the K1 pipe mismatch, even though dimensions/stride match.
    const auto pipe=make(15,16,256,128,12,1,3);
    reject(pipe,"3D thin pipe rotation cannot be rebased",true);

    // tile19 is micro thick4. The parent has nine logical layers but twelve
    // physical layers; a three-layer local allocation occupies four slices.
    const auto thick=make(19,16,32,16,9,4,3);
    accept(thick,false,4,"thick micro aligned origin");
    const auto terminal=make(19,16,32,16,9,8,1);
    accept(terminal,false,4,"rounded terminal thick group fits parent padding");
    require(thick.local_layout.guest_bytes==thick.local_layout.mips[0].slice_bytes*4,
        "thick local rounded slice padding precondition");
    const auto interleaved=make(19,16,32,16,9,1,3);
    reject(interleaved,"interleaved thick origin changes pixel z bits",true);
    auto short_local=thick;short_local.local_layout.guest_bytes-=short_local.local_layout.mips[0].slice_bytes;
    reject(short_local,"local rounded group padding missing");
    auto short_parent=terminal;short_parent.parent_layout.guest_bytes-=short_parent.parent_layout.mips[1].slice_bytes;
    reject(short_parent,"parent rounded terminal padding missing");
    const auto out_of_range=make(19,16,32,16,9,8,2);
    reject(out_of_range,"local logical layers exceed parent");

    // tile20 uses macro thick4. K4 needs bank3 and the five local layers
    // exercise both a whole low-z group and the start of the following group.
    const auto thick_macro=make(20,16,256,128,16,4,5,3);
    accept(thick_macro,true,4,"thick macro compensated origin across groups");
    auto wrong_thick_bank=thick_macro;wrong_thick_bank.local.bank_swizzle=0;
    reject(wrong_thick_bank,"thick macro wrong bank compensation",true);
    const auto xthick=make(25,16,256,128,24,8,9,3);
    accept(xthick,true,8,"xthick macro aligned origin across two groups");
    const auto xinterleaved=make(25,16,256,128,24,4,9);
    reject(xinterleaved,"xthick interleaved origin changes high pixel z bit",true);

    // Deliberately malformed proof inputs must fail before any address oracle.
    auto macro_mismatch=thin;macro_mismatch.local_layout.mips[0].macro=false;
    reject(macro_mismatch,"mismatched macro flag");
    auto thickness_mismatch=thick;thickness_mismatch.local_layout.mips[0].thickness=1;
    reject(thickness_mismatch,"mismatched physical thickness");
    auto pitch_mismatch=thin;pitch_mismatch.local_layout.mips[0].pitch+=8;
    reject(pitch_mismatch,"mismatched physical pitch");
    auto offset_mismatch=thin;offset_mismatch.local_layout.mips[0].offset=256;
    reject(offset_mismatch,"nonzero standalone mip offset");
    auto stride_mismatch=thin;stride_mismatch.local_layout.mips[0].slice_bytes+=256;
    reject(stride_mismatch,"mismatched physical slice stride");
    auto sampled=thin;sampled.local.samples=2;sampled.local_layout=LayoutGuestTexture(sampled.local);
    reject(sampled,"multisample local storage");
    auto invalid_tile=thin;invalid_tile.parent.tile_mode=invalid_tile.local.tile_mode=27;
    reject(invalid_tile,"invalid tile enum");
    auto one_dimensional=thin;one_dimensional.local.one_dimensional=true;
    reject(one_dimensional,"unsupported one-dimensional interpretation");
    auto volume=thin;volume.local.volume_depth=2;
    reject(volume,"unsupported volume interpretation");
    // No valid GetPipeConfig tile exposes P16 to GuestTileAddress today.
    // alt_tile changes allocation extents, not that base address pipe enum.
}
} // namespace NativeDX12
