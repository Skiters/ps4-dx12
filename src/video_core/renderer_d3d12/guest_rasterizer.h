// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_pipeline_cache.h"
#include "guest_compute.h"
#include "guest_texture_cache.h"
#include "page_watch.h"
#include "guest_buffer_access.h"
#include "guest_depth_metadata.h"
namespace NativeDX12 {
// Actual Liverpool dispatch entry points, distinct from the isolated probe's
// buffer-only rasterizer. Unsupported guest states fail before GPU submission.
class GuestRasterizer : public GuestBufferAccess {
    ComPtr<ID3D12Device> device_;
    AmdGpu::Liverpool& gpu_;
    GuestShaderFrontend frontend_;
    GuestShaderCache shaders_;
    GuestPipelineCache pipelines_;
    GuestTextureCache textures_;
    GuestDepthMetadata depth_metadata_;
    bool native_alpha_factor_{};
    std::function<bool(VAddr,std::span<std::byte>)> read_backing_;
    std::function<bool(VAddr,std::span<const std::byte>)> write_backing_;
    bool OwnImage(GuestTextureCache::Entry&);
    void ResolveImage(GuestTextureCache::Entry&);
    void ResolveMemory(VAddr,u64);
    void ReleaseImmutableWatch(VAddr,u64);
    void ResolveImages(VAddr address,u64 size) override {ResolveMemory(address,size);}
    void ReleaseImageWatches(VAddr,u64) override;
    void DiscardImages(VAddr,u64);
    void MaterializeColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,const GuestTextureLayout&);
    void ResolveColorTargets();
    void Dispatch(const u32 (&groups)[3]);
    bool ComputeImageOperation(const Shader::Info&,const u32 (&groups)[3]);
    void DrawInternal(bool,u32,const DrawArguments*,u16,u16);
public:
    GuestRasterizer(ID3D12Device*,AmdGpu::Liverpool&,const Shader::Profile&,
        std::wstring converter,std::wstring dxc);
    ~GuestRasterizer() override;
    void SetBackingAccess(std::function<bool(VAddr,u64)>,
        std::function<bool(VAddr,std::span<std::byte>)>,std::function<bool(VAddr,std::span<const std::byte>)>);
    bool ReadMemory(VAddr,u64,bool=false) override;
    bool InvalidateMemory(VAddr,u64,bool=false) override;
    void RegisterMemory(VAddr,u64) override;
    void UnmapMemory(VAddr,u64) override;
    void PrepareMemoryProtection(VAddr,u64) override;
    Resource* CoherentDisplayTexture(VAddr,UINT,UINT,UINT) override;
    Resource* PrepareDisplayTexture(VAddr,UINT,UINT,UINT,ID3D12CommandQueue*) override;
    void Draw(bool indexed,u32 index_offset=0) override;
    void FillBuffer(VAddr,u32,u32,bool) override;
    void DrawIndirect(bool,VAddr,u32,u32,u32,VAddr,u16,u16) override;
    void DispatchDirect() override;
    void DispatchIndirect(VAddr,u32 offset,u32 size) override;
};
}
