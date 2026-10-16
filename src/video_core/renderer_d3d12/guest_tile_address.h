// SPDX-FileCopyrightText: Copyright 2025 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <bit>
#include <stdexcept>
#include "video_core/amdgpu/tiling.h"

namespace NativeDX12 {

// CPU counterpart of the pinned upstream host_shaders/tiling.comp. Both the
// native transfer path and its semantic fixtures use these guest coordinates;
// host resource footprints never determine PS4 storage addressing.
struct GuestTileAddress {
    AmdGpu::ArrayMode array;
    AmdGpu::MicroTileMode micro;
    AmdGpu::PipeConfig pipe;
    u32 bytes, samples, thickness;
    u32 pipes=1, bank_width=1, bank_height=1, banks=1, aspect=1, split=1;

    GuestTileAddress(AmdGpu::TileMode mode, u32 bytes_per_pixel, u32 num_samples)
        : array(AmdGpu::GetArrayMode(mode)), micro(AmdGpu::GetMicroTileMode(mode)),
          pipe(AmdGpu::GetPipeConfig(mode)), bytes(bytes_per_pixel), samples(num_samples),
          thickness(AmdGpu::GetMicroTileThickness(array)) {
        if (AmdGpu::IsMacroTiled(array)) {
            const auto macro=AmdGpu::CalculateMacrotileMode(mode, bytes*8, samples);
            // Upstream TileManager specializes the base pipe/bank parameters
            // even for alt_tile images. alt_tile changes the allocation
            // extents; the descriptor's bank_swizzle remains an address input.
            pipes=AmdGpu::GetPipeCount(pipe);
            bank_width=AmdGpu::GetBankWidth(macro);
            bank_height=AmdGpu::GetBankHeight(macro);
            banks=AmdGpu::GetNumBanks(macro);
            aspect=AmdGpu::GetMacrotileAspect(macro);
            split=AmdGpu::CalculateTileSplit(mode,array,micro,bytes*8);
        }
    }

    u32 Pixel(u32 x,u32 y,u32 z,u32 tile_thickness) const {
        const auto b=[](u32 v,u32 bit){return (v>>bit)&1u;};
        if (micro==AmdGpu::MicroTileMode::Display) {
            switch(bytes) {
            case 1:return b(x,0)|(b(x,1)<<1)|(b(x,2)<<2)|(b(y,1)<<3)|(b(y,0)<<4)|(b(y,2)<<5);
            case 2:return (x&7)|((y&7)<<3);
            case 4:return b(x,0)|(b(x,1)<<1)|(b(y,0)<<2)|(b(x,2)<<3)|(b(y,1)<<4)|(b(y,2)<<5);
            case 8:return b(x,0)|(b(y,0)<<1)|(b(x,1)<<2)|(b(x,2)<<3)|(b(y,1)<<4)|(b(y,2)<<5);
            case 12:return 0;
            case 16:return b(x,0)|(b(y,0)<<1)|(b(x,1)<<2)|(b(y,1)<<3)|(b(x,2)<<4)|(b(y,2)<<5);
            default:throw std::invalid_argument("Guest display tile has unsupported element size");
            }
        }
        if(micro==AmdGpu::MicroTileMode::Thin||micro==AmdGpu::MicroTileMode::Depth)
            return b(x,0)|(b(y,0)<<1)|(b(x,1)<<2)|(b(y,1)<<3)|(b(x,2)<<4)|(b(y,2)<<5);
        u32 pixel=(b(x,2)<<6)|(b(y,2)<<7);
        if(bytes!=12)pixel|=b(x,0)|(b(y,0)<<1);
        switch(bytes) {
        case 1:case 2:pixel|=(b(x,1)<<2)|(b(y,1)<<3)|(b(z,0)<<4)|(b(z,1)<<5);break;
        case 4:pixel|=(b(x,1)<<2)|(b(z,0)<<3)|(b(y,1)<<4)|(b(z,1)<<5);break;
        case 8:case 16:pixel|=(b(z,0)<<2)|(b(x,1)<<3)|(b(y,1)<<4)|(b(z,1)<<5);break;
        case 12:break;
        default:throw std::invalid_argument("Guest thick tile has unsupported element size");
        }
        if(tile_thickness==8)pixel|=b(z,2)<<8;
        return pixel;
    }

    u64 Element(u32 x,u32 y,u32 slice,u32 sample,u32 tile_thickness) const {
        const auto pixel=Pixel(x,y,slice,tile_thickness);
        return u64(micro==AmdGpu::MicroTileMode::Depth ? pixel*samples+sample :
                   sample*64*tile_thickness+pixel)*bytes;
    }

    u64 Micro(u32 x,u32 y,u32 slice,u32 pitch,u32 height,u32 sample,u32 tile_thickness) const {
        const u64 micro_bytes=64ull*tile_thickness*bytes*samples;
        const u64 slice_bytes=u64(pitch)*height*tile_thickness*bytes*samples;
        return (slice/tile_thickness)*slice_bytes+
            (u64(y/8)*(pitch/8)+x/8)*micro_bytes+Element(x,y,slice,sample,tile_thickness);
    }

    u32 Pipe(u32 x,u32 y,u32 slice) const {
        const auto b=[](u32 v,u32 bit){return (v>>bit)&1u;};
        const u32 tx=x/8,ty=y/8;
        u32 result;
        switch(pipe) {
        case AmdGpu::PipeConfig::P2:result=b(tx,0)^b(ty,0);break;
        case AmdGpu::PipeConfig::P8_32x32_8x16:
            result=(b(tx,1)^b(ty,0)^b(tx,2))|((b(tx,0)^b(ty,1))<<1)|((b(tx,2)^b(ty,2))<<2);break;
        case AmdGpu::PipeConfig::P8_32x32_16x16:
            result=(b(tx,0)^b(ty,0)^b(tx,1))|((b(tx,1)^b(ty,1))<<1)|((b(tx,2)^b(ty,2))<<2);break;
        default:throw std::invalid_argument("Guest tiling pipe configuration has no upstream address implementation");
        }
        u32 rotation=0;
        if(array==AmdGpu::ArrayMode::Array3DTiledThin1||array==AmdGpu::ArrayMode::Array3DTiledThick||
           array==AmdGpu::ArrayMode::Array3DTiledXThick)
            rotation=std::max(1u,pipes/2-1)*(slice/thickness);
        return result^(rotation&(pipes-1));
    }

    u32 Bank(u32 x,u32 y,u32 slice,u32 split_slice,u32 bank_swizzle) const {
        const auto b=[](u32 v,u32 bit){return (v>>bit)&1u;};
        const u32 tx=x/8/(bank_width*pipes),ty=y/8/bank_height;
        u32 result;
        switch(banks) {
        case 16:result=(b(tx,0)^b(ty,3))|((b(tx,1)^b(ty,2)^b(ty,3))<<1)|
            ((b(tx,2)^b(ty,1))<<2)|((b(tx,3)^b(ty,0))<<3);break;
        case 8:result=(b(tx,0)^b(ty,2))|((b(tx,1)^b(ty,1)^b(ty,2))<<1)|((b(tx,2)^b(ty,0))<<2);break;
        case 4:result=(b(tx,0)^b(ty,1))|((b(tx,1)^b(ty,0))<<1);break;
        case 2:result=b(tx,0)^b(ty,0);break;
        default:throw std::invalid_argument("Invalid guest tile bank count");
        }
        u32 rotation=0,split_rotation=0;
        if(array==AmdGpu::ArrayMode::Array2DTiledThin1||array==AmdGpu::ArrayMode::Array2DTiledThick||
           array==AmdGpu::ArrayMode::Array2DTiledXThick)
            rotation=(banks/2-1)*(slice/thickness);
        else if(array==AmdGpu::ArrayMode::Array3DTiledThin1||array==AmdGpu::ArrayMode::Array3DTiledThick||
                array==AmdGpu::ArrayMode::Array3DTiledXThick)
            rotation=std::max(1u,pipes/2-1)*(slice/thickness)/pipes;
        if(array==AmdGpu::ArrayMode::Array2DTiledThin1||array==AmdGpu::ArrayMode::Array3DTiledThin1||
           array==AmdGpu::ArrayMode::ArrayPrt2DTiledThin1||array==AmdGpu::ArrayMode::ArrayPrt3DTiledThin1)
            split_rotation=(banks/2+1)*split_slice;
        return (result^(bank_swizzle+rotation)^split_rotation)&(banks-1);
    }

    u64 Macro(u32 x,u32 y,u32 slice,u32 pitch,u32 height,u32 sample,u32 bank_swizzle) const {
        u64 element=Element(x,y,slice,sample,thickness);
        u32 micro_bytes=64*thickness*bytes*samples,splits=1,split_slice=0;
        if(micro_bytes>split&&thickness==1) {
            splits=micro_bytes/split;split_slice=u32(element/split);
            element%=split;micro_bytes=split;
        }
        const u32 mw=8*bank_width*pipes*aspect,mh=8*bank_height*banks/aspect;
        const u64 macro_bytes=u64(micro_bytes)*(mw/8)*(mh/8)/(pipes*banks);
        const u64 per_row=pitch/mw,per_slice=per_row*(height/mh);
        const u64 slice_offset=per_slice*macro_bytes*(split_slice+u64(splits)*(slice/thickness));
        const u64 macro_offset=(u64(y/mh)*per_row+x/mw)*macro_bytes;
        const u32 tile=(y/8%bank_height)*bank_width+(x/8/pipes)%bank_width;
        const u64 total=slice_offset+macro_offset+u64(tile)*micro_bytes+element;
        if(AmdGpu::IsPrt(array)){x%=mw;y%=mh;}
        const u32 p=Pipe(x,y,slice),bank=Bank(x,y,slice,split_slice,bank_swizzle);
        const u32 pipe_bits=std::countr_zero(pipes),bank_bits=std::countr_zero(banks);
        return (total&255)|(u64(p)<<8)|(u64(bank)<<(8+pipe_bits))|
            ((total>>8)<<(8+pipe_bits+bank_bits));
    }
};
} // namespace NativeDX12
