// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "resources.h"
#include "memory_allocator.h"
#include <array>
#include <span>
#include <memory>
#include <dxgi1_4.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
namespace AmdGpu {struct Image;struct ColorBuffer;struct DepthBuffer;struct DepthView;union CbDbExtent;}
namespace NativeDX12 {
struct GuestTextureDesc {
    uint64_t address=0;
    uint32_t width=1,height=1,pitch=1,levels=1,layers=1;
    uint32_t storage_height=0; // Attachment allocation can exceed its logical view.
    uint32_t data_format=10,number_format=0,tile_mode=13;
    bool pow2_padding=false;
    uint32_t first_mip=0,first_layer=0,view_layers=1;
    uint32_t min_lod=0; // Absolute resource LOD, guest unsigned 4.8 format.
    std::array<uint32_t,4> swizzle{4,5,6,7};
    bool writable=false;
    bool render_target=false;
    bool depth_target=false;
    uint64_t stencil_address=0;
    bool one_dimensional=false;
    uint32_t volume_depth=0;
    uint32_t samples=1;
    bool operator==(const GuestTextureDesc&)const=default;
};
struct GuestMipLayout {uint32_t pitch,height;uint64_t offset,slice_bytes;bool macro=false;};
struct GuestTextureCopyPlans;
struct GuestTextureLayout {
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;uint32_t bytes_per_pixel=0;
    uint32_t block_extent=1;
    uint64_t guest_bytes=0,logical_bytes=0;
    std::vector<GuestMipLayout> mips;
    std::shared_ptr<GuestTextureCopyPlans> copy_plans;
};
GuestTextureDesc DescribeGuestTexture(const AmdGpu::Image& image);
GuestTextureDesc DescribeGuestColorBuffer(const AmdGpu::ColorBuffer&,AmdGpu::CbDbExtent);
uint64_t GuestCmaskBytes(const AmdGpu::ColorBuffer&);
bool GuestColorClearPending(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<const std::byte> cmask);
bool MaterializeGuestColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<std::byte> color,std::span<std::byte> cmask);
GuestTextureDesc DescribeGuestDepthBuffer(const AmdGpu::DepthBuffer&,const AmdGpu::DepthView&,AmdGpu::CbDbExtent);
GuestTextureDesc DescribeGuestStencil(const GuestTextureDesc& depth);
GuestTextureLayout LayoutGuestTexture(const GuestTextureDesc& desc);
bool CompatibleGuestSampledStorage(GuestTextureDesc source,GuestTextureDesc sampled);
// Writable descriptors may differ in mip/layer ranges and compatible numeric
// interpretations while sharing one typeless allocation.
bool CompatibleGuestWritableStorage(GuestTextureDesc source,GuestTextureDesc view);
bool CompatibleGuestShaderStorage(GuestTextureDesc source,GuestTextureDesc view);
// Reuses PS4 thin/display 8x8 microtile addressing from host_shaders/tiling.comp.
uint64_t GuestPixelOffset(const GuestTextureDesc&,const GuestTextureLayout&,
                          uint32_t mip,uint32_t layer,uint32_t x,uint32_t y,uint32_t sample=0);
// Multisample linear data is sample-major within each layer, with no resolve.
std::vector<std::byte> DecodeGuestSubresource(const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<const std::byte>,uint32_t mip,uint32_t layer);
// Copy only logical texels. Preserve padding and untouched mip/layer storage.
void EncodeGuestSubresource(const GuestTextureDesc&,const GuestTextureLayout&,
    std::span<std::byte> guest,std::span<const std::byte> linear,uint32_t mip,uint32_t layer);
class GuestTextureCache {
public:
    struct Entry {
        GuestTextureDesc desc;GuestTextureLayout layout;Resource texture;
        std::vector<Resource> uploads;uint64_t fence=0;bool pending=false;
        bool gpu_owned=false;
        bool cpu_watched=false;
        // Refilled only after the entry's GPU fence has retired.
        std::vector<Resource> upload_pool;
        std::vector<std::byte> sampled_snapshot;
        std::vector<std::byte> stencil_snapshot;
        GuestTextureLayout stencil_layout;
        std::shared_ptr<std::vector<Resource>> readback_pool;
        uint64_t last_use=0;
        void RememberSnapshot(std::span<const std::byte>,std::span<const std::byte> stencil={});
        D3D12_SHADER_RESOURCE_VIEW_DESC Srv()const;
        D3D12_SHADER_RESOURCE_VIEW_DESC Srv(const GuestTextureDesc& sampled_view)const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC Uav(uint32_t mip)const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC Uav(const GuestTextureDesc& view,uint32_t mip)const;
        D3D12_RENDER_TARGET_VIEW_DESC Rtv(uint32_t mip=0)const;
        D3D12_DEPTH_STENCIL_VIEW_DESC Dsv(bool read_only_depth=false,bool read_only_stencil=false)const;
    };
private:
    ComPtr<ID3D12Device> device_;std::vector<std::unique_ptr<Entry>> entries_;
    PlacedTextureAllocator texture_allocator_;
    std::unordered_multimap<uint64_t,Entry*> address_index_;
    std::unordered_set<DXGI_FORMAT> writable_formats_;
    uint64_t collected_fence_{};
    uint64_t use_sequence_=0;
    ComPtr<IDXGIAdapter3> budget_adapter_;
    HANDLE budget_event_{};
    DWORD budget_cookie_{};
    uint64_t resident_budget_=512*1024*1024,budget_checked_at_=0;
    void RefreshMemoryBudget();
    void Index(Entry& entry){address_index_.emplace(entry.desc.address,&entry);}
    void RebuildIndex(){address_index_.clear();for(auto& entry:entries_)Index(*entry);}
    std::function<void(Entry&)> resolve_owned_;
    std::function<bool(Entry&)> acquire_owned_;
    std::function<void()> retire_pending_;
    std::function<uint64_t(const std::function<void(ID3D12GraphicsCommandList*)>&)> submit_copy_;
    std::function<void(Entry&)> watch_immutable_,release_immutable_;
public:
    explicit GuestTextureCache(ID3D12Device* device);
    ~GuestTextureCache();
    void ValidateWritableFormat(DXGI_FORMAT);
    Entry& Upload(const GuestTextureDesc&,std::span<const std::byte> guest_bytes,
                  ID3D12GraphicsCommandList*,uint64_t completed_fence,
                  std::span<const std::byte> stencil_bytes={});
    void Submitted(uint64_t fence);
    void Used(Entry& entry,uint64_t fence);
    void Collect(uint64_t completed_fence);
    void SetOwnershipResolver(std::function<void(Entry&)> resolver){resolve_owned_=std::move(resolver);}
    void SetOwnershipAcquirer(std::function<bool(Entry&)> acquire){acquire_owned_=std::move(acquire);}
    bool Own(Entry& entry){
        if(entry.gpu_owned)return true;
        if(!acquire_owned_||!acquire_owned_(entry))return false;
        entry.gpu_owned=true;return true;}
    bool HasOwnedShaderStorage(const GuestTextureDesc& desc)const{
        const auto [first,last]=address_index_.equal_range(desc.address);
        for(auto it=first;it!=last;++it)if(it->second->gpu_owned&&
            CompatibleGuestShaderStorage(it->second->desc,desc))return true;
        return false;}
    void SetPendingRetirement(std::function<void()> retirement){retire_pending_=std::move(retirement);}
    void SetCopySubmission(std::function<uint64_t(const std::function<void(ID3D12GraphicsCommandList*)>&)> submit){
        submit_copy_=std::move(submit);}
    void SetImmutableWatch(std::function<void(Entry&)> watch,std::function<void(Entry&)> release){
        watch_immutable_=std::move(watch);release_immutable_=std::move(release);}
    auto& Entries(){return entries_;}
    Resource* CoherentDisplayTexture(uint64_t address,UINT width,UINT height,UINT pitch,uint64_t completed_fence);
    // Invalidation is safe only once all pending/submitted users have finished.
    void Invalidate(uint64_t address,uint64_t size,uint64_t completed_fence);
};
}
