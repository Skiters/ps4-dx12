// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/rasterizer.h"
#include "common/scope_exit.h"
#include <cstring>

namespace NativeDX12 {
inline void RunLargeImageTransferFixture(ID3D12Device* device){
    GuestTextureDesc desc;desc.address=0x10000;desc.width=desc.height=desc.pitch=2048;
    desc.layers=5;desc.view_layers=5;desc.data_format=4;desc.number_format=4;
    const auto layout=LayoutGuestTexture(desc);
    if(layout.guest_bytes<=64*1024*1024||!GuestTextureCache::CanUploadGpuSnapshot(desc,layout))
        throw std::runtime_error("Large image is excluded from the addressable GPU snapshot domain");
    // Cross both the old 64 MiB cutoff and a partial final dispatch row. Guard
    // DWORDs are initialized separately and must never be merged.
    constexpr uint64_t bytes=64ull*1024*1024+65536+4,guard=1024,storage=bytes+guard;
    const auto mask_bytes=GuestTiler::WrittenBytes(storage);
    Rasterizer renderer(device);GuestTiler tiler(device);
    auto input=GuestTiler::Scratch(device,storage),output=GuestTiler::Scratch(device,storage);
    auto mask=GuestTiler::Scratch(device,mask_bytes);
    auto upload=Buffer(device,storage*2+mask_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    auto readback=Buffer(device,storage,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    const auto input_word=[](uint32_t i){return 0x71727374u^(i*0x10204081u);};
    const auto output_word=[](uint32_t i){return 0x31323334u^(i*0x01010101u);};
    const auto mask_word=[](uint32_t i){return 0x96a53cf0u^(i*0x00102041u);};
    void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
    auto* words=static_cast<uint32_t*>(mapped);
    for(uint32_t i=0;i<storage/4;++i){words[i]=input_word(i);words[storage/4+i]=output_word(i);}
    for(uint32_t i=0;i<mask_bytes/4;++i)words[storage/2+i]=mask_word(i);
    upload.handle->Unmap(0,nullptr);
    // All resources and leases survive exceptional exits until submitted work
    // retires; a failed assertion must not itself create a deleted-resource TDR.
    std::shared_ptr<GuestTiler::Lease> lease;
    SCOPE_EXIT {renderer.Finish();};
    uint64_t offset{};
    for(auto* target:{&input,&output,&mask}){
        target->Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        renderer.RecordingList()->CopyBufferRegion(target->handle.Get(),0,upload.handle.Get(),offset,
            target->handle->GetDesc().Width);
        offset+=target->handle->GetDesc().Width;
    }
    bool rejected=false;
    try{tiler.PrepareMaskedMerge(input,input,mask,bytes);}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Large masked merge accepted aliased input/output");
    rejected=false;
    try{tiler.PrepareMaskedMerge(input,output,mask,GuestTiler::MaxGuestBytes+4);}
    catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Masked merge accepted an unaddressable allocation envelope");
    lease=tiler.PrepareMaskedMerge(input,output,mask,bytes);
    lease->Record(renderer.RecordingList(),input,output,mask);
    output.Transition(renderer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    renderer.RecordingList()->CopyBufferRegion(readback.handle.Get(),0,output.handle.Get(),0,storage);
    renderer.Finish();D3D12_RANGE range{0,SIZE_T(storage)};Check(readback.handle->Map(0,&range,&mapped));
    SCOPE_EXIT {readback.handle->Unmap(0,&empty);};
    const auto* actual=static_cast<const uint32_t*>(mapped);
    for(uint32_t i=0;i<storage/4;++i){
        uint32_t expected=output_word(i);
        if(uint64_t(i)*4<bytes){
            // Independent byte-by-byte oracle, not the shader's word-mask math.
            for(uint32_t byte=0;byte<4;++byte){const auto address=i*4+byte;
                if((mask_word(address/32)>>(address%32))&1u){
                    expected&=~(255u<<(byte*8));expected|=input_word(i)&(255u<<(byte*8));
                }
            }
        }
        if(actual[i]!=expected)throw std::runtime_error("Large masked merge contents/guard mismatch at DWORD "+std::to_string(i));
    }
    renderer.ValidateDebugMessages();
}
}
