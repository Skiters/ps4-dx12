// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_pipeline_cache.h"
#include "guest_compute.h"
#include "guest_texture_cache.h"
#include "page_watch.h"
#include "guest_buffer_access.h"
namespace NativeDX12 {
class NativePointCapturePass;
class GuestRawColorClear;
// Actual Liverpool dispatch entry points, distinct from the isolated probe's
// buffer-only rasterizer. Unsupported guest states fail before GPU submission.
class GuestRasterizer : public GuestBufferAccess {
    ComPtr<ID3D12Device> device_;
    AmdGpu::Liverpool& gpu_;
    GuestShaderFrontend frontend_;
    GuestShaderCache shaders_;
    GuestPipelineCache pipelines_;
    GuestTextureCache textures_;
    std::unique_ptr<NativePointCapturePass> point_capture_;
    std::unique_ptr<GuestRawColorClear> raw_color_clear_;
    bool native_alpha_factor_{};
    bool image_watch_pending_{},validating_image_watches_{};
    std::function<bool(VAddr,std::span<std::byte>)> read_backing_;
    std::function<bool(VAddr,std::span<const std::byte>)> write_backing_;
    bool OwnImage(GuestTextureCache::Entry&);
    bool OwnIndependentImage(GuestTextureCache::Entry&);
    bool OwnImageClear(GuestTextureCache::Entry&);
    void QueueLinearImageDownload(GuestTextureCache::Entry&);
    void DownloadIndependentImage(GuestTextureCache::Entry&);
    void RetireImageForPressure(GuestTextureCache::Entry&);
    void ReleaseIndependentImageWatches(GuestTextureCache::Entry&);
    void ReleaseImageProtection(GuestTextureCache::Entry&);
    std::vector<std::pair<VAddr,u64>> ImageProtectionRanges(const GuestTextureCache::Entry* exclude=nullptr)const;
    void ValidateIndependentImageWatches();
    void RememberImageBoundary(GuestTextureCache::Entry&,VAddr,u64);
    void ProcessDownloadImages();
    void ResolveImage(GuestTextureCache::Entry&);
    void ResolveMemory(VAddr,u64);
    void ReleaseImmutableWatch(VAddr,u64);
    void DiscardBufferImageVersions(VAddr,u64,bool gpu_write);
    void WatchBufferImageVersions(VAddr,u64);
    void ResolveCpuFault(VAddr,bool) override;
    void ReleaseBufferPageWatch(VAddr,u64) override;
    void CheckBacking(VAddr,u64,bool) override;
    void PrepareGpuBacking(VAddr,u64,bool) override;
    void ResolveImages(VAddr address,u64 size) override {ResolveMemory(address,size);}
    bool HasGpuImageOverlap(VAddr,u64)const override;
    void ReleaseImageWatches(VAddr,u64) override;
    void DiscardImages(VAddr,u64);
    bool MaterializeOwnedColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
        const GuestTextureLayout&,const GuestColorClearPlan&,std::span<std::byte> metadata);
    void MaterializeColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,const GuestTextureLayout&);
    void ResolveColorTargets();
    void Dispatch(const u32 (&groups)[3]);
    bool ComputeImageOperation(const Shader::Info&,const u32 (&groups)[3]);
    void DrawInternal(bool,u32,const DrawArguments*,u16,u16);
public:
    GuestRasterizer(ID3D12Device*,AmdGpu::Liverpool&,const Shader::Profile&,
        std::wstring converter,std::wstring dxc);
    ~GuestRasterizer() override;
    std::optional<u64> ReplaceShader(u64,std::span<const u32>,bool);
    void SetBackingAccess(std::function<bool(VAddr,u64)>,
        std::function<bool(VAddr,std::span<std::byte>)>,std::function<bool(VAddr,std::span<const std::byte>)>);
    bool ReadMemory(VAddr,u64,bool=false) override;
    u64 ClampMemoryRange(VAddr,u64) override;
    bool InvalidateMemory(VAddr,u64,bool=false) override;
    void NotifyCpuWrite(VAddr,u64) override;
    void NotifyGpuBufferWrite(VAddr,u64) override;
    void RegisterMemory(VAddr,u64) override;
    void UnmapMemory(VAddr,u64) override;
    void PrepareMemoryProtection(VAddr,u64) override;
    Resource* CoherentDisplayTexture(VAddr,UINT,UINT,UINT) override;
    void CaptureDisplayCandidates(VAddr,uint64_t,Resource*) override;
    Resource* PrepareDisplayTexture(VAddr,UINT,UINT,UINT,ID3D12CommandQueue*) override;
    void Draw(bool indexed,u32 index_offset=0) override;
    void FillBuffer(VAddr,u32,u32,bool) override;
    void DrawIndirect(bool,VAddr,u32,u32,u32,VAddr,u16,u16) override;
    void DispatchDirect() override;
    void DispatchIndirect(VAddr,u32 offset,u32 size) override;
    void OnSubmit() override;
    void OnFence() override;
};
}
