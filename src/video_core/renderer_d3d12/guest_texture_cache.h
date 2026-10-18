// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_page_range.h"
#include "resources.h"
#include "memory_allocator.h"
#include "guest_tiler.h"
#include "guest_multisample.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <span>
#include <memory>
#include <string>
#include <optional>
#include <dxgi1_4.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <map>
namespace AmdGpu {struct Image;struct ColorBuffer;struct DepthBuffer;struct DepthView;union CbDbExtent;}
namespace NativeDX12 {
struct GuestTextureDesc {
    uint64_t address=0;
    uint32_t width=1,height=1,pitch=1,levels=1,layers=1;
    uint32_t storage_height=0; // Attachment allocation can exceed its logical view.
    uint64_t storage_slice_bytes=0; // CB/DB registers define the physical slice.
    uint32_t data_format=10,number_format=0,tile_mode=13;
    bool pow2_padding=false;
    bool alt_tile=false;
    uint32_t bank_swizzle=0;
    uint32_t first_mip=0,first_layer=0,view_layers=1;
    uint32_t min_lod=0; // Absolute resource LOD, guest unsigned 4.8 format.
    std::array<uint32_t,4> swizzle{4,5,6,7};
    bool writable=false;
    bool native_packed10_raw=false;
    bool native_raw32=false;
    bool render_target=false;
    bool depth_target=false;
    uint64_t stencil_address=0;
    bool one_dimensional=false;
    uint32_t volume_depth=0;
    uint32_t samples=1;
    bool UsesRawPacked10()const{return data_format==9&&writable&&!render_target&&!depth_target&&
        (native_packed10_raw||number_format==1||number_format==5);}
    bool UsesRaw32()const{return UsesRawPacked10()||((data_format==4||data_format==10)&&
        native_raw32&&writable&&!render_target&&!depth_target);}
    bool operator==(const GuestTextureDesc&)const=default;
};
struct GuestMipLayout {
    uint32_t pitch,height;
    uint64_t offset,slice_bytes;
    bool macro=false;
    uint32_t thickness=1;
};
struct GuestTextureCopyPlans;
class GuestPackedTransfer;
class GuestBcDecoder;
struct GuestTextureLayout {
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;uint32_t bytes_per_pixel=0;
    uint32_t expanded_bytes_per_pixel=0;
    uint32_t HostBytesPerPixel()const{return expanded_bytes_per_pixel?expanded_bytes_per_pixel:bytes_per_pixel==12?16:
        bytes_per_pixel==2&&format==DXGI_FORMAT_R32_UINT?4:bytes_per_pixel;}
    uint32_t block_extent=1;
    uint64_t guest_bytes=0,logical_bytes=0;
    std::vector<GuestMipLayout> mips;
    std::shared_ptr<GuestTextureCopyPlans> copy_plans;
};
inline bool HasExactPackedExpansion(const GuestTextureDesc& d,const GuestTextureLayout& l){
    return l.expanded_bytes_per_pixel==16&&(d.data_format==9||
        (d.data_format>=16&&d.data_format<=19&&
         (d.volume_depth||d.one_dimensional||(d.data_format==19&&d.samples>1&&!d.render_target))&&
         d.number_format==0));
}
GuestTextureDesc DescribeGuestTexture(const AmdGpu::Image& image);
GuestTextureDesc DescribeGuestColorBuffer(const AmdGpu::ColorBuffer&,AmdGpu::CbDbExtent);
uint64_t GuestCmaskBytes(const AmdGpu::ColorBuffer&);
struct GuestCmaskTile {uint32_t layer,x,y;uint64_t byte;uint32_t shift;};
struct GuestColorClearPlan {
    std::vector<uint32_t> uniform_layers;
    std::vector<GuestCmaskTile> tiles;
    bool Pending()const{return !uniform_layers.empty()||!tiles.empty();}
};
// Shared validation/address decoding for physical CPU materialization and
// native attachment clears. Planning does not modify either guest plane.
GuestColorClearPlan PlanGuestColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<const std::byte> cmask);
bool GuestColorClearPending(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<const std::byte> cmask);
bool MaterializeGuestColorClear(const AmdGpu::ColorBuffer&,const GuestTextureDesc&,
    const GuestTextureLayout&,std::span<std::byte> color,std::span<std::byte> cmask);
GuestTextureDesc DescribeGuestDepthBuffer(const AmdGpu::DepthBuffer&,const AmdGpu::DepthView&,AmdGpu::CbDbExtent);
GuestTextureDesc DescribeGuestStencil(const GuestTextureDesc& depth);
GuestTextureLayout LayoutGuestTexture(const GuestTextureDesc& desc);
bool CompatibleGuestSampledStorage(GuestTextureDesc source,GuestTextureDesc sampled);
// Logical readonly BC view compatibility, including numeric variants/prefixes.
// Actual native resource casting and ownership must be proved by the cache.
bool CompatibleGuestBlockSampledViews(const GuestTextureDesc&,const GuestTextureLayout&,
    const GuestTextureDesc&,const GuestTextureLayout&);
// Writable descriptors may differ in mip/layer ranges and compatible numeric
// interpretations while sharing one typeless allocation.
bool CompatibleGuestWritableStorage(GuestTextureDesc source,GuestTextureDesc view);
bool CompatibleGuestShaderStorage(GuestTextureDesc source,GuestTextureDesc view);
// Original 32-bit mutable color class; actual sharing also needs resource casts.
bool CompatibleGuestReadonlyWordStorage(GuestTextureDesc source,GuestTextureDesc view);
// Uncompressed mip-major storage can share a complete physical mip prefix.
bool CompatibleGuestShaderMipPrefixes(GuestTextureDesc,const GuestTextureLayout&,
    GuestTextureDesc,const GuestTextureLayout&);
// Physical I transfer/growth can reinterpret packed storage; this does not
// authorize binding the same native texture under incompatible typed views.
bool CompatibleGuestImageMipGeometry(GuestTextureDesc,const GuestTextureLayout&,
    GuestTextureDesc,const GuestTextureLayout&);
bool MatchesGuestMipLayout(const GuestTextureLayout&,const GuestTextureLayout&);
bool CompatibleGuestAttachmentStorage(GuestTextureDesc source,GuestTextureDesc view);
// A smaller readonly/DSV binding can retain a grown native depth allocation.
// Its logical mip/layer selectors do not redefine the owner's guest layout.
bool CompatibleGuestDepthSubview(const GuestTextureDesc&,const GuestTextureLayout&,
    const GuestTextureDesc&,const GuestTextureLayout&);
void MergeGuestColorAttachmentLayers(std::span<GuestTextureDesc>);
bool CompatibleGuestPackedStorage(GuestTextureDesc source,GuestTextureDesc view);
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
    enum class UploadPurpose { Binding, Preparation };
    using BufferResidency=std::vector<std::pair<uint64_t,uint64_t>>;
    struct BlockExpansion {
        Resource snapshot;
        BufferResidency residency;
    };
    struct IndependentImageVersion {Resource depth,stencil;BufferResidency seed_residency;};
    struct BufferImageSource {Resource snapshot;bool resident{};};
    class CachedImageBindingPlan {
        friend class GuestTextureCache;
        const GuestTextureCache* cache_{};
        GuestTextureDesc storage_;
        std::shared_ptr<void> source_pin_;
        CachedImageBindingPlan()=default;
    public:
        const GuestTextureDesc& Storage()const{return storage_;}
    };

    struct Entry {
        GuestTextureDesc desc;GuestTextureLayout layout;Resource texture;
        GuestTextureCache* cache{};
        uint64_t image_uid{};
        // Allocation identity survives CPU/B dirtying; it never proves current
        // I contents or ownership. Private prewrite snapshots do not replace it.
        bool cached_native_current=false;
        // Physical prewarm can seed new I from B before its original image bind.
        // Retain the deferred EnsureResident effect with that exact I version.
        BufferResidency pending_buffer_residency;
        uint64_t diagnostic_writer{},diagnostic_vs{},diagnostic_ps{};
        // Tokens can outlive cache destruction after a device-loss error.
        // Their counter must not point back into the destroyed Entry.
        std::shared_ptr<uint32_t> binding_pins;
        bool BindingPinned()const{return binding_pins&&*binding_pins;}
        std::array<uint64_t,3> metadata_addresses{};
        uint32_t htile_initial_mask=UINT32_MAX;
        std::map<UINT,Resource> sample_backings;
        std::vector<std::shared_ptr<GuestMultisample::CopyLease>> sample_copy_leases;
        std::vector<Resource> storage_samples;
        Resource storage_linear;
        // Completed UAV sample planes are reconstructible from the native MSAA
        // image. Pressure can retire these copies without retiring its owner.
        bool storage_retired=false;
        std::vector<std::shared_ptr<GuestMultisample::StorageLease>> storage_leases;
        std::vector<std::shared_ptr<void>> storage_clear_leases;
        std::vector<Resource> uploads;uint64_t fence=0;bool pending=false;
        std::vector<Resource> buffer_sync_resources;
        uint64_t buffer_sync_allocation_bytes{};
        bool gpu_owned=false;
        // Original GpuModified survives invalidation and readonly refresh.
        // Logical validity does not imply current physical/page ownership.
        bool logical_gpu_modified=false,logical_dirty=true;
        void NoteLogicalImageWrite(){logical_gpu_modified=true;logical_dirty=false;}
        void MarkLogicalDirty(){logical_dirty=true;}
        bool HasValidLogicalImage()const{return logical_gpu_modified&&!logical_dirty;}
        // Separate shader buffer/image writers retain the original cache
        // domains. Their image version does not replace physical buffer bytes.
        bool buffer_alias_version=false,buffer_alias_watch=false;
        // Only unprotected boundary pages need a physical-B equality witness.
        // I remains logically distinct; compare these bytes before reusing I.
        GuestImageBoundaryPages unprotected_cpu_pages,stencil_unprotected_cpu_pages;
        auto ProtectionPlanes(){return std::array{
            GuestImageBoundaryPlane{desc.address,layout.guest_bytes,&unprotected_cpu_pages},
            GuestImageBoundaryPlane{desc.stencil_address,desc.stencil_address?stencil_layout.guest_bytes:0,
                &stencil_unprotected_cpu_pages}};}
        auto ProtectionPlanes()const{return std::array{
            GuestImageBoundaryPlane<const GuestImageBoundaryPages>{desc.address,layout.guest_bytes,&unprotected_cpu_pages},
            GuestImageBoundaryPlane<const GuestImageBoundaryPages>{desc.stencil_address,
                desc.stencil_address?stencil_layout.guest_bytes:0,&stencil_unprotected_cpu_pages}};}
        bool HasUnprotectedCpuPages()const{
            return !unprotected_cpu_pages.empty()||!stencil_unprotected_cpu_pages.empty();}
        bool linear_download_pending=false;
        // Ownership changes also invalidate the CP's authoritative-image index.
        void SetGpuOwned(bool);
        bool cpu_watched=false;
        // Refilled only after the entry's GPU fence has retired.
        std::vector<Resource> upload_pool;
        std::vector<Resource> tiler_input_pool,tiler_output_pool;
        std::vector<std::shared_ptr<GuestTiler::Lease>> tiler_leases;
        std::shared_ptr<GuestTiler> tiler;
        std::shared_ptr<GuestMultisample> multisample;
        std::vector<std::shared_ptr<GuestMultisample::Lease>> multisample_leases;
        std::vector<std::shared_ptr<GuestMultisample::ColorDepthLease>> color_depth_leases;
        struct NativePlaneSource {Resource resource;std::shared_ptr<void> binding_pin;};
        std::vector<NativePlaneSource> native_plane_sources;
        std::vector<std::byte> sampled_snapshot;
        std::vector<std::byte> stencil_snapshot;
        GuestTextureLayout stencil_layout;
        std::shared_ptr<std::vector<Resource>> readback_pool;
        uint64_t last_use=0;
        uint64_t allocation_bytes=0,readback_reserve_bytes=0;
        uint64_t format_retirement_bytes{};
        Resource expanded_guest_backing,expanded_written_mask;
        uint64_t expanded_backing_bytes{};
        std::vector<std::shared_ptr<GuestPackedTransfer>> format_transfers;
        uint64_t ResidentBytes()const;
        void RememberSnapshot(std::span<const std::byte>,std::span<const std::byte> stencil={});
        D3D12_SHADER_RESOURCE_VIEW_DESC Srv()const;
        D3D12_SHADER_RESOURCE_VIEW_DESC Srv(const GuestTextureDesc& sampled_view)const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC Uav(uint32_t mip)const;
        D3D12_UNORDERED_ACCESS_VIEW_DESC Uav(const GuestTextureDesc& view,uint32_t mip)const;
        D3D12_RENDER_TARGET_VIEW_DESC Rtv(uint32_t mip=0)const;
        D3D12_RENDER_TARGET_VIEW_DESC Rtv(const GuestTextureDesc&,uint32_t mip=0)const;
        D3D12_DEPTH_STENCIL_VIEW_DESC Dsv(bool read_only_depth=false,bool read_only_stencil=false)const;
        D3D12_DEPTH_STENCIL_VIEW_DESC Dsv(const GuestTextureDesc&,bool read_only_depth=false,bool read_only_stencil=false)const;
    };
    enum class MetaType { CMask,FMask,HTile };
    struct MetaDataInfo {
        MetaType type;
        uint64_t owner_uid{};
        uint32_t num_samples=1;
        uint32_t clear_mask=UINT32_MAX;
    };
    struct WritableBlockAlias {
        Entry* parent{};Entry* image{};uint64_t parent_uid{};
        std::shared_ptr<void> parent_pin,image_pin;
    };
private:
    void RememberNativeImageStorage(Entry&);
    void ForgetRecreatedSourceMeta(Entry&);
    Entry& RecreateCachedDepthAtBind(Entry&,const GuestTextureDesc&,const GuestTextureLayout&,ID3D12GraphicsCommandList*,uint64_t);
    bool IsCachedDepthRecreationSource(const Entry&,const GuestTextureDesc&,const GuestTextureLayout&)const;
    enum class NativeReadPurpose { Independent, CachedDepthRecreation };
    bool CanTransferDependentBlockView(const Entry&,const GuestTextureDesc&,const GuestTextureLayout&)const;
    bool IsLiveBlockParent(const WritableBlockAlias&,const GuestTextureDesc&,const GuestTextureLayout&)const;
    bool IsLiveWritableBlockAlias(const WritableBlockAlias&)const;
    Entry& UploadVersion(const GuestTextureDesc&,std::span<const std::byte>,ID3D12GraphicsCommandList*,
        uint64_t,std::span<const std::byte>,bool,const Resource*,bool,bool,const WritableBlockAlias*,UploadPurpose,
        const Resource* stencil_gpu_snapshot=nullptr,bool fresh_depth_recreation=false);
    Resource RetileImageVersion(Entry&,Resource&,ID3D12GraphicsCommandList*,uint64_t,
        const WritableBlockAlias*,UINT plane=0,bool private_depth_snapshot=false,
        const GuestTextureDesc* mapped_desc=nullptr,const GuestTextureLayout* mapped_layout=nullptr,
        NativeReadPurpose=NativeReadPurpose::Independent);
    ComPtr<ID3D12Device> device_;std::vector<std::unique_ptr<Entry>> entries_;
    std::shared_ptr<GuestTiler> tiler_;
    std::shared_ptr<GuestMultisample> multisample_;
    PlacedTextureAllocator texture_allocator_;
    std::unordered_multimap<uint64_t,Entry*> address_index_;
    mutable std::map<uint64_t,uint64_t> guest_overlap_ranges_;
    mutable std::map<uint64_t,uint64_t> guest_owned_overlap_ranges_;
    mutable bool guest_overlap_ranges_dirty_=true;
    mutable bool guest_owned_overlap_ranges_dirty_=true;
    std::unordered_map<uint64_t,Entry*> image_uids_;
    std::unordered_map<uint64_t,MetaDataInfo> surface_metas_;
    uint64_t next_image_uid_=1;
    std::unordered_set<DXGI_FORMAT> writable_formats_;
    uint64_t collected_fence_{};
    uint64_t use_sequence_=0;
    ComPtr<IDXGIAdapter3> budget_adapter_;
    HANDLE budget_event_{};
    DWORD budget_cookie_{};
    uint64_t resident_budget_=512*1024*1024,budget_checked_at_=0;
    std::shared_ptr<std::atomic<uint64_t>> pressure_retirement_bytes_=std::make_shared<std::atomic<uint64_t>>(0);
    std::wstring converter_;
    std::unique_ptr<GuestBcDecoder> bc_decoder_;
    void RefreshMemoryBudget();
    void ReserveMemory(uint64_t bytes,uint64_t completed,Entry* preserve=nullptr);
    std::shared_ptr<void> PinPressureEntry(Entry*);
    uint64_t ReleaseRetiredStorage(Entry&,uint64_t completed);
    bool EvictPressureVictim(uint64_t completed,Entry* preserve);
    Resource AllocateTexture(Entry* preserve,const D3D12_RESOURCE_DESC&,D3D12_RESOURCE_STATES,
        const D3D12_CLEAR_VALUE* clear=nullptr,std::span<const DXGI_FORMAT> formats={});
    bool ConvertPackedBacking(Entry&,const GuestTextureDesc&,const GuestTextureLayout&,
        ID3D12GraphicsCommandList*);
    void Index(Entry& entry){guest_overlap_ranges_dirty_=guest_owned_overlap_ranges_dirty_=true;address_index_.emplace(entry.desc.address,&entry);image_uids_.emplace(entry.image_uid,&entry);}
    void RebuildIndex(){
        guest_overlap_ranges_dirty_=guest_owned_overlap_ranges_dirty_=true;
        address_index_.clear();image_uids_.clear();for(auto& entry:entries_)Index(*entry);
        std::erase_if(surface_metas_,[&](const auto& meta){return !image_uids_.contains(meta.second.owner_uid);});
    }
    std::function<void(Entry&)> resolve_owned_;
    std::function<Resource(Entry&,uint64_t,uint64_t)> read_independent_;
    std::function<IndependentImageVersion(Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> read_independent_image_;
    std::function<IndependentImageVersion(Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> read_cached_depth_;
    std::function<void(uint64_t,uint64_t)> prepare_image_buffer_;
    std::function<BufferImageSource(uint64_t,uint64_t)> current_image_buffer_;
    bool planned_image_bind_{};
    std::function<void()> transfer_submission_;
    std::function<BlockExpansion(Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> expand_block_;
    std::function<void(std::span<const std::pair<uint64_t,uint64_t>>)> publish_buffer_residency_;
    std::function<bool(Entry&)> acquire_independent_;
    std::function<bool(Entry&,Entry&)> acquire_independent_replacement_;
    std::function<void(Entry&)> retire_independent_;
    std::function<void(Entry&)> retire_pressure_;
    std::function<uint64_t()> wait_pressure_;
    std::function<bool(Entry&)> acquire_owned_;
    std::function<void(Entry&)> notify_write_;
    std::function<bool(const GuestTextureDesc&,const GuestTextureLayout&)> retain_buffer_alias_;
    std::function<bool(const GuestTextureDesc&,const GuestTextureLayout&)> retain_independent_image_;
    std::function<void()> retire_pending_;
    std::function<void(const std::function<void(ID3D12GraphicsCommandList*)>&)> submit_copy_;
    std::function<ID3D12GraphicsCommandList*()> recording_list_;
    std::function<void(Entry&)> watch_immutable_,release_immutable_;
public:
    explicit GuestTextureCache(ID3D12Device* device,std::wstring converter={});
    ~GuestTextureCache();
    void ValidateWritableFormat(DXGI_FORMAT);
    void RegisterMeta(Entry&,MetaType,uint64_t address,uint32_t initial_mask=UINT32_MAX);
    std::optional<MetaDataInfo> GetMetaInfo(uint64_t address)const;
    Resource RetileBufferVersion(Entry&,Resource& initial,ID3D12GraphicsCommandList*,uint64_t completed);
    // Immutable prewrite copy of an owned native I plane, without publishing
    // pixels to guest B or granting independent multi-plane ownership.
    Resource CaptureDepthPlane(Entry&,UINT plane,Resource& initial,ID3D12GraphicsCommandList*,uint64_t completed);
    void ClearRawStorageImage(Entry&,const float (&color)[4],ID3D12GraphicsCommandList*,uint64_t completed);
    bool ClearMeta(uint64_t address);
    bool ConsumeMeta(uint64_t address,uint32_t slice);
    void RemoveMeta(uint64_t address,uint64_t size);
    void SetBackingSamples(Entry&,UINT samples,ID3D12GraphicsCommandList*,bool copy=true);
    void EnsureMultisampleStorage(Entry&);
    bool RetireMultisampleStorage(Entry&,uint64_t completed_fence);
    std::shared_ptr<GuestMultisample::StorageLease> PrepareMultisampleStorage(Entry&);
    // current_buffer_snapshot is restricted to a proven current-B capture after
    // logical invalidation. Private feedback snapshots cannot validate history.
    Entry& Upload(const GuestTextureDesc&,std::span<const std::byte> guest_bytes,
                  ID3D12GraphicsCommandList*,uint64_t completed_fence,
                  std::span<const std::byte> stencil_bytes={},bool guest_snapshot=false,
                  const Resource* gpu_snapshot=nullptr,bool writable_snapshot=false,
                  bool current_buffer_snapshot=false,UploadPurpose purpose=UploadPurpose::Binding,
                  const Resource* stencil_gpu_snapshot=nullptr);
    // Only an actual image use consumes a prepared B-source residency effect.
    void PublishBufferResidency(Entry&);
    // Uses the owned Upload reuse predicate; ambiguous clean sources fail closed.
    Entry* FindValidIndependentImage(const GuestTextureDesc&,const GuestTextureLayout&)const;
    Entry* FindDepthViewParent(const GuestTextureDesc&,const GuestTextureLayout&)const;
    // Native allocation contents and original GpuModified history are distinct.
    void SetImageBufferSources(std::function<void(uint64_t,uint64_t)> prepare,
        std::function<BufferImageSource(uint64_t,uint64_t)> current){
        prepare_image_buffer_=std::move(prepare);current_image_buffer_=std::move(current);
    }
    bool IsPlannedImageBind()const{return planned_image_bind_;}
    Entry* FindImageRecreationBindingSource(const GuestTextureDesc&,const GuestTextureLayout&)const;
    std::shared_ptr<const CachedImageBindingPlan> PlanCachedImageBinding(const GuestTextureDesc&,const GuestTextureLayout&);
    Entry& BindPlannedImage(const CachedImageBindingPlan&,ID3D12GraphicsCommandList*,uint64_t);
    bool HasCurrentNativeContents(const Entry&)const;
    bool HasCurrentDepthContents(const Entry&)const;
    // A cache recreation reads old initialized native I even after CPU dirtying.
    // This does NOT admit the old entry to ordinary independent/readonly reuse.
    Entry* FindCachedDepthRecreationSource(const GuestTextureDesc&,const GuestTextureLayout&)const;
    // Resolve full parent/recreated depth storage BEFORE caller source/prewrite
    // captures. Requested selectors stay on the binding view.
    GuestTextureDesc ResolveDepthViewStorage(const GuestTextureDesc&)const;
    // Also admits existing packed conversion and pinned-compatible uncompressed
    // color image interpretations. These are sources, never direct view reuse.
    Entry* FindIndependentUploadSource(const GuestTextureDesc&,const GuestTextureLayout&)const;
    static bool CanUploadGpuSnapshot(const GuestTextureDesc&,const GuestTextureLayout&,bool writable_snapshot=false);
    // Structural raw 8/16/32-bit component contract; typed views and MSAA quality still
    // require the existing device checks during allocation/transfer preparation.
    static bool CanTransferIndependentMultisample(const GuestTextureDesc&,const GuestTextureLayout&);
    // Transient UINT views never own physical guest memory. Before dispatch,
    // the caller proves parent identity and disjoint SRV/UAV subresources.
    Entry* FindBlockViewParent(const GuestTextureDesc&,const GuestTextureLayout&)const;
    Entry* FindReadonlyBlockParent(const GuestTextureDesc&,const GuestTextureLayout&)const;
    Entry* FindWritableBlockParent(const GuestTextureDesc&,const GuestTextureLayout&)const;
    // Rebase a standalone physical mip/slice onto its BC or word parent chain.
    // The returned description is a binding view, never another guest owner.
    std::optional<GuestTextureDesc> RebaseBlockSubresource(const GuestTextureDesc&,
        const GuestTextureLayout&,bool array)const;
    std::optional<WritableBlockAlias> BeginWritableBlockAlias(const GuestTextureDesc&,
        ID3D12GraphicsCommandList*,uint64_t completed);
    Entry& CommitWritableBlockAlias(WritableBlockAlias&,ID3D12GraphicsCommandList*,uint64_t completed);
    std::shared_ptr<void> PinBinding(Entry&);
    struct PressureSourceLease {std::shared_ptr<void> lease;uint64_t bytes;};
    PressureSourceLease RetainPressureSource(Entry&);
    uint64_t PressureRetirementBytes()const{return pressure_retirement_bytes_->load();}
    uint64_t ResidentBytes()const;
    // Reclaim only retired, unpinned entries; false means live/preserved or
    // reference-ineligible versions still keep residency above this limit.
    bool ReclaimResidentMemory(uint64_t target,uint64_t completed,Entry* preserve=nullptr);
    void Submitted(uint64_t fence);
    void Used(Entry& entry,uint64_t fence);
    void Collect(uint64_t completed_fence);
    void SetOwnershipResolver(std::function<void(Entry&)> resolver){resolve_owned_=std::move(resolver);}
    void SetIndependentVersionTransfer(
        std::function<Resource(Entry&,uint64_t,uint64_t)> read,
        std::function<bool(Entry&)> acquire,std::function<void(Entry&)> retire){
        read_independent_=std::move(read);acquire_independent_=std::move(acquire);retire_independent_=std::move(retire);
    }
    void SetIndependentBlockExpansion(
        std::function<BlockExpansion(Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> expand){
        expand_block_=std::move(expand);
    }
    void SetIndependentImageTransfer(std::function<IndependentImageVersion(
        Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> read){read_independent_image_=std::move(read);}
    void SetIndependentOwnershipTransfer(std::function<bool(Entry&,Entry&)> acquire){
        acquire_independent_replacement_=std::move(acquire);
    }
    void SetCachedDepthRecreationTransfer(std::function<IndependentImageVersion(
        Entry&,const GuestTextureDesc&,const GuestTextureLayout&)> read){read_cached_depth_=std::move(read);}
    IndependentImageVersion CaptureCachedDepthRecreationVersion(Entry&,const GuestTextureDesc&,
        const GuestTextureLayout&,ID3D12GraphicsCommandList*,uint64_t,
        const std::function<Resource(uint64_t,uint64_t)>& capture_buffer);
    // Original overlap copies use private depth/color inputs. Both CopyImage
    // and CopyImageWithBuffer exclude S; explicit CopyDepthStencil is separate.
    // Color->MS depth returns initialization seeds here; UploadVersion records
    // the pinned channel-to-sample draw directly into the initialized target.
    IndependentImageVersion CaptureIndependentImageVersion(Entry&,const GuestTextureDesc&,
        const GuestTextureLayout&,ID3D12GraphicsCommandList*,uint64_t completed,
        const std::function<Resource(uint64_t,uint64_t)>& capture_buffer);
    void SetBufferResidencyPublisher(std::function<void(std::span<const std::pair<uint64_t,uint64_t>>)> publish){
        publish_buffer_residency_=std::move(publish);
    }
    void SetTransferSubmissionPublisher(std::function<void()> publish){transfer_submission_=std::move(publish);}
    void SetPressureRetirement(std::function<void(Entry&)> retirement){retire_pressure_=std::move(retirement);}
    void SetPressureWait(std::function<uint64_t()> wait){wait_pressure_=std::move(wait);}
    void SetOwnershipAcquirer(std::function<bool(Entry&)> acquire){acquire_owned_=std::move(acquire);}
    void SetImageWriteNotifier(std::function<void(Entry&)> notify){notify_write_=std::move(notify);}
    void SetBufferAliasRetention(std::function<bool(const GuestTextureDesc&,const GuestTextureLayout&)> retain){retain_buffer_alias_=std::move(retain);}
    bool CanRetainBufferAlias(const GuestTextureDesc& desc,const GuestTextureLayout& layout)const {
        return retain_buffer_alias_&&retain_buffer_alias_(desc,layout);
    }
    // Physical image ownership is separate from permission to initialize a
    // writable image version alongside an independently writable buffer.
    void SetIndependentImageRetention(std::function<bool(const GuestTextureDesc&,const GuestTextureLayout&)> retain){retain_independent_image_=std::move(retain);}
    bool CanRetainIndependentImage(const GuestTextureDesc& desc,const GuestTextureLayout& layout)const {
        return retain_independent_image_&&retain_independent_image_(desc,layout);
    }
    bool Own(Entry& entry){
        entry.NoteLogicalImageWrite();
        if(entry.gpu_owned){if(notify_write_)notify_write_(entry);RememberNativeImageStorage(entry);return true;}
        if(!acquire_owned_||!acquire_owned_(entry))return false;
        entry.SetGpuOwned(true);if(notify_write_)notify_write_(entry);RememberNativeImageStorage(entry);return true;}
    bool RevalidateReadonly(Entry& entry){
        if(!entry.HasValidLogicalImage()||!acquire_independent_||!acquire_independent_(entry))return false;
        RememberNativeImageStorage(entry);return true;
    }
    bool CanShareReadonlyWordStorage(const GuestTextureDesc& a,const GuestTextureDesc& b)const{
        return texture_allocator_.SupportsFormatCasting()&&CompatibleGuestReadonlyWordStorage(a,b);
    }
    bool HasOwnedShaderStorage(const GuestTextureDesc& desc)const{
        const auto [first,last]=address_index_.equal_range(desc.address);
        for(auto it=first;it!=last;++it)if(it->second->gpu_owned&&
            (CompatibleGuestShaderStorage(it->second->desc,desc)||
             (it->second->desc.depth_target&&CompatibleGuestDepthSubview(
                 it->second->desc,it->second->layout,desc,LayoutGuestTexture(desc)))||
             CompatibleGuestPackedStorage(it->second->desc,desc)))return true;
        return false;}
    void SetPendingRetirement(std::function<void()> retirement){retire_pending_=std::move(retirement);}
    // CPU backing reads/watch callbacks can submit and retire the caller's list.
    // Resolve the current context again at each subsequent upload recording step.
    void SetRecordingListProvider(std::function<ID3D12GraphicsCommandList*()> current){recording_list_=std::move(current);}
    void SetCopySubmission(std::function<void(const std::function<void(ID3D12GraphicsCommandList*)>&)> submit){
        submit_copy_=std::move(submit);}
    void SetImmutableWatch(std::function<void(Entry&)> watch,std::function<void(Entry&)> release){
        watch_immutable_=std::move(watch);release_immutable_=std::move(release);}
    const auto& Entries(){return entries_;}
    const auto& Entries()const{return entries_;}
    bool HasGuestRangeOverlap(uint64_t address,uint64_t size,bool gpu_owned_only=false)const;
    Resource* CoherentDisplayTexture(uint64_t address,UINT width,UINT height,UINT pitch,uint64_t completed_fence);
    // Invalidation is safe only once all pending/submitted users have finished.
    void Invalidate(uint64_t address,uint64_t size,uint64_t completed_fence);
};
}
