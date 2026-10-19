#include "common/api_trace.h"
#pragma once
#include "resources.h"
#include <cstring>
namespace NativeDX12 {
inline Resource Texture2D(ID3D12Device* device,UINT width,UINT height,DXGI_FORMAT format,
                          D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES initial,
                          const D3D12_CLEAR_VALUE* clear=nullptr) {
    if(!width||!height)throw std::invalid_argument("zero texture dimension");
    D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width=width;d.Height=height;d.DepthOrArraySize=1;d.MipLevels=1;d.Format=format;
    d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;d.Flags=flags;
    D3D12_HEAP_PROPERTIES p{};p.Type=D3D12_HEAP_TYPE_DEFAULT;
    Resource r;r.state=initial;
    Check(DX12_API_CALL(device->CreateCommittedResource(&p,D3D12_HEAP_FLAG_NONE,&d,initial,clear,IID_PPV_ARGS(&r.handle))));
    return r;
}
// Owns the staging resource through GPU completion. Caller retains this object
// until the command-list submission fence completes, just as for upload arenas.
struct TextureTransfer {
    Resource staging;
    UINT subresource=0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0;
    UINT64 row_bytes=0,total_bytes=0;
    static TextureTransfer Create(ID3D12Device* device,const Resource& texture,bool readback) {
        return Create(device,DX12_API_CALL(texture.handle->GetDesc()),readback);
    }
    static TextureTransfer Create(ID3D12Device* device,const D3D12_RESOURCE_DESC& desc,bool readback) {
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.MipLevels!=1||desc.DepthOrArraySize!=1||desc.SampleDesc.Count!=1)
            throw std::invalid_argument("transfer supports single-subresource non-MSAA 2D textures");
        return CreateSubresource(device,desc,0,readback);
    }
    static TextureTransfer CreateSubresource(ID3D12Device* device,const D3D12_RESOURCE_DESC& desc,UINT subresource,bool readback) {
        if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.SampleDesc.Count!=1||!desc.MipLevels||!desc.DepthOrArraySize)
            throw std::invalid_argument("transfer requires non-MSAA 2D texture storage");
        uint64_t count=uint64_t(desc.MipLevels)*desc.DepthOrArraySize;
        if(subresource>=count){
            D3D12_FEATURE_DATA_FORMAT_INFO format{desc.Format,0};
            Check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,&format,sizeof(format)));
            count*=format.PlaneCount;
        }
        if(subresource>=count)
            throw std::out_of_range("texture transfer subresource exceeds storage");
        TextureTransfer t;t.subresource=subresource;
        DX12_API_CALL(device->GetCopyableFootprints(&desc,subresource,1,0,&t.footprint,&t.rows,&t.row_bytes,&t.total_bytes));
        if(!t.total_bytes||t.total_bytes==UINT64_MAX)throw std::runtime_error("invalid texture footprint");
        t.staging=Buffer(device,t.total_bytes,readback?D3D12_HEAP_TYPE_READBACK:D3D12_HEAP_TYPE_UPLOAD,
                         readback?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_GENERIC_READ);
        return t;
    }
    void WriteRows(const void* source,size_t source_size,UINT64 source_pitch) {
        if(staging.state!=D3D12_RESOURCE_STATE_GENERIC_READ)throw std::logic_error("not upload staging");
        if(!source||source_pitch<row_bytes||source_pitch>SIZE_MAX||rows==0||
           source_pitch>UINT64_MAX/rows||source_size<source_pitch*(rows-1)+row_bytes)
            throw std::invalid_argument("short texture source");
        void* mapped=nullptr;D3D12_RANGE empty{0,0};Check(DX12_API_CALL(staging.handle->Map(0,&empty,&mapped)));
        auto dest=static_cast<std::byte*>(mapped)+footprint.Offset;
        for(UINT row=0;row<rows;++row)memcpy(dest+UINT64(row)*footprint.Footprint.RowPitch,
                                            static_cast<const std::byte*>(source)+UINT64(row)*source_pitch,size_t(row_bytes));
        DX12_API_CALL(staging.handle->Unmap(0,nullptr));
    }
    void Upload(ID3D12GraphicsCommandList* list,Resource& texture) {
        if(staging.state!=D3D12_RESOURCE_STATE_GENERIC_READ)throw std::logic_error("not upload staging");
        texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST,
            texture.SubresourceCount()==1?D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES:subresource);
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=staging.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprint;
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=texture.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=subresource;
        DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,nullptr));
    }
    void Readback(ID3D12GraphicsCommandList* list,Resource& texture,const D3D12_BOX* region=nullptr) {
        if(staging.state!=D3D12_RESOURCE_STATE_COPY_DEST)throw std::logic_error("not readback staging");
        if(region){
            const auto source=texture.handle->GetDesc();
            const auto mip=subresource%source.MipLevels;
            if(region->right<=region->left||region->bottom<=region->top||region->front||region->back!=1||
               region->right>std::max<UINT64>(source.Width>>mip,1)||region->bottom>std::max(source.Height>>mip,1u)||
               region->right-region->left!=footprint.Footprint.Width||
               region->bottom-region->top!=footprint.Footprint.Height)
                throw std::invalid_argument("Texture readback region/footprint mismatch");
        }
        texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE,
            texture.SubresourceCount()==1?D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES:subresource);
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=texture.handle.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=subresource;
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=staging.handle.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprint;
        DX12_API_CALL(list->CopyTextureRegion(&dst,0,0,0,&src,region));
    }
    void ReadRows(void* destination,size_t destination_size,UINT64 destination_pitch) {
        if(staging.state!=D3D12_RESOURCE_STATE_COPY_DEST)throw std::logic_error("not readback staging");
        if(!destination||destination_pitch<row_bytes||destination_pitch>SIZE_MAX||rows==0||
           destination_pitch>UINT64_MAX/rows||destination_size<destination_pitch*(rows-1)+row_bytes)
            throw std::invalid_argument("short texture destination");
        void* mapped=nullptr;D3D12_RANGE range{SIZE_T(footprint.Offset),SIZE_T(total_bytes)};
        Check(DX12_API_CALL(staging.handle->Map(0,&range,&mapped)));auto src=static_cast<const std::byte*>(mapped)+footprint.Offset;
        for(UINT row=0;row<rows;++row)memcpy(static_cast<std::byte*>(destination)+UINT64(row)*destination_pitch,
                                          src+UINT64(row)*footprint.Footprint.RowPitch,size_t(row_bytes));
        D3D12_RANGE empty{0,0};DX12_API_CALL(staging.handle->Unmap(0,&empty));
    }
};
inline Resource Depth32(ID3D12Device* device,UINT width,UINT height) {
    D3D12_CLEAR_VALUE clear{};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1.f;
    return Texture2D(device,width,height,DXGI_FORMAT_D32_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
                     D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear);
}
}
