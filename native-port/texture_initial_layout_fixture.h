// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/rasterizer.h"
#include <array>
#include <cstring>
namespace NativeDX12 {
inline bool RunTextureInitialLayoutFixture(ID3D12Device* device){
    PlacedTextureAllocator allocator(device,65536,65536);
    if(!allocator.SupportsFormatCasting())return false;
    const std::array formats{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_UINT};
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=desc.Height=8;desc.DepthOrArraySize=desc.MipLevels=1;
    desc.Format=formats[0];desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    for(const auto initial:{D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COMMON}){
        Rasterizer renderer(device);auto image=allocator.Allocate(desc,initial,nullptr,formats);
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT rows{};UINT64 row_bytes{},bytes{};
        device->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&row_bytes,&bytes);
        if(rows!=8||row_bytes!=32)throw std::runtime_error("Initial layout fixture footprint changed");
        auto upload=Buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        auto readback=Buffer(device,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));memset(mapped,0xa7,size_t(bytes));
        const auto word=[](u32 x,u32 y){return 0xabcde000u+y*19+x*37;};
        for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){const auto value=word(x,y);
            memcpy(static_cast<std::byte*>(mapped)+footprint.Offset+y*footprint.Footprint.RowPitch+x*4,&value,4);}
        upload.handle->Unmap(0,nullptr);auto* list=renderer.RecordingList();
        renderer.RetainUntilFinish([image,upload,readback](u64){(void)image;(void)upload;(void)readback;});
        image.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION native{};native.pResource=image.handle.Get();native.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION linear{};linear.pResource=upload.handle.Get();linear.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;linear.PlacedFootprint=footprint;
        list->CopyTextureRegion(&native,0,0,0,&linear,nullptr);image.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        linear.pResource=readback.handle.Get();list->CopyTextureRegion(&linear,0,0,0,&native,nullptr);
        renderer.Finish();renderer.ValidateDebugMessages();
        if(!image.enhanced_barriers)throw std::runtime_error("Cast-list texture lost its enhanced initial barrier model");
        D3D12_RANGE range{0,SIZE_T(bytes)};Check(readback.handle->Map(0,&range,&mapped));bool valid=true;
        for(u32 y=0;y<8;++y)for(u32 x=0;x<8;++x){u32 actual{};
            memcpy(&actual,static_cast<const std::byte*>(mapped)+footprint.Offset+y*footprint.Footprint.RowPitch+x*4,4);
            valid&=actual==word(x,y);}
        readback.handle->Unmap(0,&empty);
        if(!valid)throw std::runtime_error("Cast-list enhanced COPY_DEST/COMMON transfer lost native words");
    }
    return true;
}
} // namespace NativeDX12
