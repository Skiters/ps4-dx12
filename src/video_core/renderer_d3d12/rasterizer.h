#pragma once
#include "resources.h"
#include "buffer_churn.h"
#include "memory_perf.h"
#include "command_scheduler.h"
#include "cpu_buffer_snapshot.h"
#include "memory_allocator.h"
#include "common/fair_recursive_mutex.h"
#include "video_core/rasterizer_interface.h"
#include <map>
#include <tuple>
#include <array>
#include <span>
#include <mutex>
#include <set>
#include <functional>
#include <deque>
#include <optional>
#include <atomic>
namespace NativeDX12 {
namespace Diagnostics {class Scope;}
class ShaderPipeline;
class ResourceBindings;
class DescriptorHeapPool;
class GuestDmaMemory;
struct BufferBudgetObserver;
struct GuestGraphicsState;
// Initial native command backend. Registered ranges must be valid host-backed
// guest memory for their full lifetime. Calls are serialized by the GPU thread.
class Rasterizer : public VideoCore::RasterizerInterface {
protected:
    mutable Common::FairRecursiveMutex execution_mutex_;
    struct IndirectSignature {ComPtr<ID3D12CommandSignature> signature;ComPtr<ID3D12RootSignature> root;};
    std::map<std::tuple<bool,u32,ID3D12RootSignature*>,IndirectSignature> indirect_draw_signatures_;
    virtual std::vector<std::pair<VAddr,u64>> RetainedImageProtectionRanges()const{return {};}
public:
    Common::FairRecursiveMutex& ExecutionMutex()const{return execution_mutex_;}
    struct MemoryReuseStats {u64 snapshots{},staging{},watch_candidates{};};
    MemoryReuseStats MemoryReuse()const {std::lock_guard lock(execution_mutex_);return memory_reuse_;}
    struct TemporaryBuffer {std::vector<u32> words;};
    struct BufferCopy {VAddr destination,source;u32 bytes;};
    // A native allocation generation, distinct from mutable CPU/GPU contents.
    // Only full GPU-preserving envelope copies inherit an older generation.
    struct BufferBackingVersion {const VAddr base;const u64 size;ID3D12Resource* const resource;};
    std::shared_ptr<TemporaryBuffer> AcquireTemporaryBuffer(u64,const void* initial=nullptr);
private:
    std::vector<std::shared_ptr<TemporaryBuffer>> temporary_pool_;
    std::vector<std::shared_ptr<Resource>> index_copy_pool_;
    u64 index_copy_pool_bytes_{};
    std::vector<std::shared_ptr<Resource>> shader_read_copy_pool_;
    u64 shader_read_copy_pool_bytes_{};
    struct IndexStreamPage {std::shared_ptr<UploadArena> arena;u64 used{};};
    std::vector<IndexStreamPage> index_stream_pages_;
    struct ReadbackSpan {u64 begin,end,offset;u64 backing_epoch{};};
    struct FrameReadback {
        Resource source,readback;
        std::vector<ReadbackSpan> spans;
        u64 fence{},serial{},epoch{},allocation_bytes{};
        std::shared_ptr<std::atomic<u64>> accounting;
        ~FrameReadback(){if(accounting)accounting->fetch_sub(allocation_bytes);}
    };
    struct Mapping {VAddr base;u64 size;Resource gpu;bool dirty=false;std::map<u64,u64> dirty_ranges;
        u64 contents_serial{1},last_prefetched_serial{};
        std::shared_ptr<FrameReadback> frame_readback;
        CpuBufferSnapshot cpu_snapshot;Resource readback;bool gpu_owned=false;bool cpu_watched=false;
        // Strong CPU exclusion is page-granular in the pinned RegionManager.
        // One native arena can retain independent GPU-owned pages after a
        // different page has been downloaded for CPU access.
        std::map<u64,u64> gpu_owned_ranges;
        bool cpu_watch_pending=false; // Protected while comparing; not yet a CPU/GPU equality proof.
        bool cpu_partial_proof=false;
        u64 protected_backing_epoch{}; // Non-owning physical-B proof; never a readable VA/ownership promise.
        std::map<u64,u64> cpu_dirty_ranges; // Unprotected CPU pages; the remaining snapshot is proved.
        bool gpu_cpu_diverged=false; // Readonly image/metadata synthesis changed the GPU arena only.
        std::map<u64,u64> cpu_upload_ranges; // Actual CPU stores require upload even for identical bytes.
        u64 cpu_upload_generation{};
        bool initial_upload_failed=false;u64 allocation_id{},last_use{};
        std::shared_ptr<const BufferBackingVersion> backing_version;
        std::vector<std::weak_ptr<const BufferBackingVersion>> preserved_versions;};
    struct RetiredBuffer {Resource gpu;u64 fence;u64 allocation_id;};
    std::vector<RetiredBuffer> buffer_pool_;
    u64 pooled_buffer_bytes_{};
    BufferChurn churn_;u64 next_allocation_id_{},acquired_allocation_id_{},use_clock_{};
    MemoryPerfConfig memory_perf_=MemoryPerfConfig::Environment();
    u64 gpu_pool_budget_=128*MemoryMiB;
    bool memory_pressure_{};
    std::vector<CpuBufferSnapshot> snapshot_pool_;
    u64 snapshot_pool_bytes_{};
    struct LargeUpload {std::unique_ptr<UploadArena> arena;u64 capacity;bool used;};
    std::vector<LargeUpload> large_upload_pool_;
    u64 large_upload_pool_bytes_{};
    MemoryReuseStats memory_reuse_{};
    void TrimPools();
    void ResizeSnapshot(Mapping&);
    void TraceMapping(Mapping&,const char*);
    std::set<VAddr> resolving_buffers_;
    std::map<VAddr,std::map<u64,u64>> resolving_buffer_ranges_;
    static bool OwnsGpuRange(const Mapping&,u64,u64);
    static void MarkGpuOwned(Mapping&,u64,u64);
    static void UnmarkGpuOwned(Mapping&,u64,u64);
    std::function<bool(VAddr,u64)> own_buffer_;
    std::function<bool(VAddr,std::span<const std::byte>)> commit_buffer_;
    std::function<void(VAddr,u64)> release_buffer_;
    std::function<bool(VAddr,u64)> watch_buffer_;
    std::function<bool(VAddr,u64)> rewatch_buffer_;
    std::function<bool(VAddr,u64)> release_cpu_pages_;
    std::function<bool(VAddr,u64,bool)> transfer_buffer_watch_;
    std::function<u64(VAddr,u64)> protected_backing_epoch_;
    Resource AcquireBuffer(u64,u64 maximum_capacity=UINT64_MAX);
    void RetireBuffer(Mapping&);
    void InstallCpuWatch(Mapping&);
    unsigned read_only_batches_{};
    ComPtr<ID3D12Device> device_;
    CommandScheduler scheduler_;
    std::map<std::pair<VAddr,u64>,u32> pending_image_commits_;
    struct PendingImageCommit {VAddr address;u64 bytes;bool backing_valid=true;};
    std::vector<std::weak_ptr<PendingImageCommit>> pending_image_tickets_;
    void CancelImageBackingCommits(VAddr,u64);
    std::shared_ptr<std::atomic<u64>> priority_image_bytes_=std::make_shared<std::atomic<u64>>(0);
    bool HasPendingImageCommit(VAddr address,u64 bytes)const{
        for(const auto& [range,count]:pending_image_commits_)
            if(count&&address<range.first+range.second&&range.first<address+bytes)return true;
        return false;
    }
    bool host_markers_enabled_{},guest_markers_enabled_{};
    std::vector<std::string> marker_scopes_;
    size_t recorded_marker_scopes_{};
    PlacedBufferAllocator buffer_allocator_;
    std::unique_ptr<Descriptors> color_descriptors_,depth_descriptors_;
    std::map<VAddr,Mapping> mappings_;
    Mapping* preparing_buffer_envelope_{};
    std::vector<const Mapping*> preparing_buffer_contents_;
    auto FirstOverlapping(VAddr address){
        auto it=mappings_.lower_bound(address);
        if(it!=mappings_.begin()&&std::prev(it)->first+std::prev(it)->second.size>address)--it;
        return it;
    }
    bool HasGpuOwnedOverlap(VAddr address,u64 size){
        for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<address+size;++it){
            const auto first=std::max(address,it->first),last=std::min(address+size,it->first+it->second.size);
            for(const auto& [begin,end]:it->second.gpu_owned_ranges)
                if(first<it->first+end&&it->first+begin<last)return true;
        }
        return false;
    }
    // Short metadata lock, independent of draw/dispatch submission. A fresh
    // CPU-only file destination must not wait for unrelated GPU work.
    std::mutex memory_index_mutex_;
    std::map<VAddr,VAddr> indexed_ranges_, indexed_buffers_;
    void RemoveBufferIndex(VAddr address){std::lock_guard lock(memory_index_mutex_);indexed_buffers_.erase(address);}
    std::set<VAddr> dirty_mappings_;
    u64 frame_readback_epoch_{};
    std::shared_ptr<std::atomic<u64>> frame_readback_bytes_=std::make_shared<std::atomic<u64>>(0);
    static void InvalidateFrameReadback(Mapping& mapping){
        if(mapping.contents_serial==UINT64_MAX)throw std::overflow_error("Buffer contents serial overflow");
        ++mapping.contents_serial;mapping.frame_readback.reset();
    }
    void CommitBufferReadback(Mapping&,Resource&,std::span<const ReadbackSpan>,bool owned);
    void ConsumeFrameReadback(Mapping&,const std::map<u64,u64>&);
    u64 cached_buffer_bytes_{};
    u64 buffer_cache_budget_=512ull*1024*1024;
    std::unique_ptr<BufferBudgetObserver> buffer_budget_observer_;
    void RefreshBufferBudget();
    // Coverage and cached GPU buffers are distinct. MapMemory may cover a huge
    // reserved PRT interval; it must never touch its host pages or allocate VRAM.
    std::map<VAddr,VAddr> mapped_ranges_;
    std::map<VAddr,VAddr> readable_spans_,writable_spans_;
    // Authoritative committed backing supplied by Core's mapping owner. These
    // are invalidated by remapping/protection notifications, not cache pressure.
    std::map<VAddr,VAddr> committed_readable_,committed_writable_;
    bool cache_host_spans_=false;
    alignas(16) std::array<std::byte,65536> gds_{};
    std::vector<Resource> uploads_;
    bool stream_buffers_enabled_{};
    bool buffer_arenas_enabled_{};
    std::vector<std::shared_ptr<UploadArena>> shader_stream_pages_;
    // Vulkan uses reusable stream/staging pools for small transfers. Keep
    // upload pages mapped and retire them with the same queue submission.
    std::vector<std::unique_ptr<UploadArena>> upload_pages_;
    u64 upload_page_cursor_{};
    std::unique_ptr<UploadArena> constants_;
    std::unique_ptr<DescriptorHeapPool> binding_heap_pool_;
    std::unique_ptr<GuestDmaMemory> dma_memory_;
    std::vector<std::function<void(u64)>> deferred_retirements_;
    std::vector<std::function<void(u64)>> submission_callbacks_;
    void Begin();
    Mapping& Find(VAddr address,u64 size,bool gds,u64 batch_budget=0,bool physical_dma=false);
    bool PreserveBufferBacking(const Mapping&)const;
    u64 AdmitBufferCache(std::span<const std::pair<VAddr,VAddr>>,u64 budget,u64 additional,u64 savings=0);
    void DropCached(VAddr address,u64 size,bool reconcile,const char* reason="overlap");
    bool RematerializeBufferEnvelope(VAddr,u64,bool physical_dma=false,bool defer_owned=true);
    void Upload(Mapping& range,u64 offset,u64 size,const void* data);
    void ClearMappings(){for(auto& [_,mapping]:mappings_)RetireBuffer(mapping);mappings_.clear();dirty_mappings_.clear();cached_buffer_bytes_=0;
        std::lock_guard lock(memory_index_mutex_);indexed_buffers_.clear();}
    bool IsGdsRange(VAddr address,u64 size) const {
        const auto base=reinterpret_cast<VAddr>(gds_.data());
        return address>=base&&address-base<=gds_.size()&&size<=gds_.size()-(address-base);
    }
    void MarkDirty(Mapping& mapping,bool dirty,u64 offset=0,u64 bytes=UINT64_MAX){
        if(!dirty){dirty_mappings_.erase(mapping.base);mapping.dirty_ranges.clear();mapping.dirty=false;
            mapping.frame_readback.reset();return;}
        if(offset>mapping.size)throw std::out_of_range("dirty buffer offset");
        bytes=std::min(bytes,mapping.size-offset);if(!bytes)return;
        InvalidateFrameReadback(mapping);
        // GDS has no direct guest CPU alias. Its internal host array is read
        // only through the CP readback path, so it needs no page protection.
        if(IsGdsRange(mapping.base,mapping.size)&&!mapping.gpu_owned)MarkGpuOwned(mapping,0,mapping.size);
        u64 newly_dirty=bytes;
        u64 begin=offset,end=offset+bytes;auto it=mapping.dirty_ranges.lower_bound(begin);
        if(it!=mapping.dirty_ranges.begin()&&std::prev(it)->second>=begin)it=std::prev(it);
        while(it!=mapping.dirty_ranges.end()&&it->first<=end){
            const auto overlap_first=std::max(offset,it->first),overlap_last=std::min(offset+bytes,it->second);
            if(overlap_last>overlap_first)newly_dirty-=overlap_last-overlap_first;
            begin=std::min(begin,it->first);end=std::max(end,it->second);it=mapping.dirty_ranges.erase(it);}
        Diagnostics::Count("buffer-dirty.new-bytes",newly_dirty);
        mapping.dirty_ranges.emplace(begin,end);dirty_mappings_.insert(mapping.base);mapping.dirty=true;}
    void ForgetBacking(VAddr,u64);
    void CopyBufferBatchImpl(std::span<const BufferCopy>,bool submit_when_deferred);
    static void Unsupported(const char* operation);
protected:
    virtual void CheckBacking(VAddr,u64,bool);
    virtual void CheckGpuBacking(VAddr address,u64 size,bool writing){Rasterizer::CheckBacking(address,size,writing);}
    virtual void PrepareGpuBacking(VAddr,u64,bool) {}
    virtual bool HasGpuImageOverlap(VAddr,u64)const {return false;}
    virtual bool ReadDmaBacking(VAddr,std::span<std::byte>);
public:
    void SetBufferOwnership(std::function<bool(VAddr,u64)> own,
        std::function<bool(VAddr,std::span<const std::byte>)> commit,std::function<void(VAddr,u64)> release){
        own_buffer_=std::move(own);commit_buffer_=std::move(commit);release_buffer_=std::move(release);}
    void ResolveGpuBuffers(VAddr,u64);
    void ReleaseCpuBufferWatches(VAddr,u64);
    void ReleaseUnusedReadOnlyWatches(VAddr,u64,std::span<const std::pair<VAddr,u64>> retained={},
        std::optional<VAddr> retiring_buffer={});
    void InvalidateCpuBufferPages(VAddr,u64);
    void SetBufferWriteWatch(std::function<bool(VAddr,u64)> watch){watch_buffer_=std::move(watch);}
    void SetBufferPageRewatch(std::function<bool(VAddr,u64)> watch){rewatch_buffer_=std::move(watch);}
    void SetCpuPageRelease(std::function<bool(VAddr,u64)> release){release_cpu_pages_=std::move(release);}
    void SetBufferWatchTransfer(std::function<bool(VAddr,u64,bool)> transfer){transfer_buffer_watch_=std::move(transfer);}
    void SetProtectedBackingEpoch(std::function<u64(VAddr,u64)> epoch){protected_backing_epoch_=std::move(epoch);}
    void EnableBufferStreaming(bool enabled){stream_buffers_enabled_=enabled;}
    void EnableBufferArenas(bool enabled){buffer_arenas_enabled_=enabled;}
    struct BufferWrite {VAddr address;u64 size;};
    struct ShaderBufferRequest {
        VAddr address;u64 size;bool written=false;bool unordered_access=false;bool resident=false;
        u8 write_byte_adjust=0;u64 original_write_bytes=0;
        // DEFAULT allocation/reservation is not itself an original EnsureResident.
        bool publish_dma_residency=true;
        BufferWrite WriteRange()const{
            const auto bytes=original_write_bytes?original_write_bytes:size;
            if(write_byte_adjust>size||bytes>size-write_byte_adjust||address>UINT64_MAX-write_byte_adjust)
                throw std::invalid_argument("Original shader write exceeds its native envelope");
            return {address+write_byte_adjust,bytes};
        }
    };
    bool BuffersMayDefer(std::span<const ShaderBufferRequest>);
    virtual void NotifyGpuBufferWrite(VAddr,u64) {}
    void MarkShaderBufferWrites(std::span<const ShaderBufferRequest> requests);
    struct ShaderReadSnapshot;
    struct ShaderBufferView {
        ComPtr<ID3D12Resource> resource;u64 offset,size;
        std::shared_ptr<UploadArena> lease;
        // Keep placed memory alive and prevent pool reuse until the last GPU view retires.
        std::shared_ptr<void> allocation;
        std::shared_ptr<const BufferBackingVersion> backing_version;
        std::shared_ptr<ShaderReadSnapshot> read_snapshot;
    };
    struct ShaderReadSnapshot {
        std::shared_ptr<Resource> gpu;
        VAddr address;u64 size;
        ShaderBufferView source;
    };
private:
    ShaderBufferView SnapshotShaderBufferRead(const ShaderBufferRequest&,const ShaderBufferView&);
    void RefreshShaderReadSnapshot(const ShaderBufferRequest&,const ShaderBufferView&,Mapping&);
    void PrepareBufferContents(Mapping&,bool written,bool uav,bool fresh,
        std::span<const ShaderBufferRequest>,std::optional<std::span<const BufferWrite>>,Diagnostics::Scope&);
    std::vector<ShaderBufferView> PrepareShaderBuffersImpl(std::span<const ShaderBufferRequest>,
        std::optional<std::span<const BufferWrite>>,bool physical_dma);
public:
    // Coalesce and pin guest buffers. Guarded GPU-owned versions remain resident;
    // CPU-visible fallbacks reconcile writes before taking their next snapshot.
    std::vector<ShaderBufferView> PrepareShaderBuffers(std::span<const ShaderBufferRequest>,
        std::optional<std::span<const BufferWrite>> known_writes=std::nullopt);
    // Raw DMA reads B, including under an independently owned image. Only
    // nonpublishing readonly resident UAV ranges are accepted by this path.
    std::vector<ShaderBufferView> PreparePhysicalDmaBuffers(std::span<const ShaderBufferRequest>);
    void SynchronizePreparedDmaBuffers(std::span<const ShaderBufferRequest>,std::span<const ShaderBufferView>);
    // Select only fully materialized backing; never reserve, resize or upload.
    std::optional<ShaderBufferView> PreparedDmaBuffer(VAddr,u64)const;
    // Re-select after attachment preparation without GPU allocation or replaying
    // stage effects. Refresh existing disjoint GPU-read copies at this phase.
    // Reject unrelated replacements, even at the same address.
    std::vector<ShaderBufferView> RefreshPreparedShaderBuffers(
        std::span<const ShaderBufferRequest>,std::span<const ShaderBufferView>);
    Resource CaptureShaderBuffer(VAddr address,u64 bytes,bool publish_dma_residency=true);
    bool IsGpuBufferModified(VAddr address,u64 bytes)const;
    // Actual strong CPU-access guards, distinct from GPU-modified pages or
    // semantic residency. Images must preserve these when demoting ownership.
    std::vector<std::pair<VAddr,u64>> GpuOwnedBufferProtectionRanges(VAddr,u64)const;
    void ValidatePreparedShaderBufferRange(VAddr address,u64 bytes);
    // Capacity/current physical B only, before final native descriptor binding.
    // Late image recreation may grow the envelope; preserve all overlapping
    // prepared generations without publishing formatted I or B residency.
    void PrepareImageBufferSource(VAddr address,u64 bytes);
    Resource CapturePreparedShaderBuffer(VAddr address,u64 bytes);
    struct PreparedBufferBinding {Resource* resource;u64 offset,bytes;std::shared_ptr<const BufferBackingVersion> version;};
    // Borrow only under ExecutionMutex after all allocation/ownership callbacks.
    // Does not allocate, upload, snapshot or resize the prepared B mapping.
    PreparedBufferBinding BorrowPreparedShaderBuffer(VAddr address,u64 bytes);
    void TransitionPreparedShaderBuffer(VAddr address,u64 bytes,D3D12_RESOURCE_STATES);
    void CopyToPreparedShaderBuffer(VAddr address,u64 bytes,Resource& source,D3D12_RESOURCE_STATES final_state,
        bool resident_only=false);
    void CopyResidentImageToPreparedShaderBuffer(VAddr address,u64 bytes,Resource& source,D3D12_RESOURCE_STATES final_state){
        CopyToPreparedShaderBuffer(address,bytes,source,final_state,true);
    }
    void FillPreparedShaderBuffer(VAddr address,u64 bytes,u32 value,D3D12_RESOURCE_STATES final_state);
    void CommitImageBacking(VAddr,std::span<const std::byte>,
        const std::function<bool(VAddr,std::span<std::byte>)>& read,
        const std::function<bool(VAddr,std::span<const std::byte>)>& write);
    void DeferImageBacking(u64 fence,VAddr,u64 bytes,u64 retained_bytes,
        std::function<bool(VAddr,std::span<std::byte>)> read,
        std::function<bool(VAddr,std::span<const std::byte>)> write,
        std::function<void(u64,std::span<std::byte>)> resolve);
    // Must be called outside ExecutionMutex and other callback-needed locks.
    void DrainPriorityOperations(){scheduler_.DrainPriority();}
    u64 PendingImageCommitBytes()const{return priority_image_bytes_->load();}
    bool FormattedBufferNeedsImageSync(VAddr address,u64 bytes){
        std::lock_guard lock(execution_mutex_);
        return bytes>16384||HasGpuOwnedOverlap(address,bytes);
    }
    // Ordered GPU copy packets, bounded like the guest copy-shader HLE batches.
    // Whole guarded pages stay GPU-owned until CPU access; other ranges reconcile.
    void CopyBufferBatch(std::span<const BufferCopy>);
    ID3D12GraphicsCommandList* RecordingList(){std::lock_guard lock(execution_mutex_);Begin();return scheduler_.Begin();}
    void SubmitCopyCommands(const std::function<void(ID3D12GraphicsCommandList*)>& record){
        std::lock_guard lock(execution_mutex_);scheduler_.SubmitCopy(record);}
    void QueueHostTextureCopy(ID3D12Resource* resource,void* mapped,
        std::shared_ptr<const std::vector<std::vector<std::byte>>> slices,
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint,UINT rows,UINT64 row_bytes){
        std::lock_guard lock(execution_mutex_);
        scheduler_.QueueHostTextureCopy(resource,mapped,std::move(slices),footprint,rows,row_bytes);
    }
    void QueueHostBufferCopy(ID3D12Resource* resource,void* mapped,
        std::shared_ptr<const std::vector<std::byte>> source,size_t bytes){
        std::lock_guard lock(execution_mutex_);
        scheduler_.QueueHostCopy(resource,mapped,std::move(source),0,bytes,true);
    }
    VAddr GdsAddress() const {return reinterpret_cast<VAddr>(gds_.data());}
    u64 CompletedFence() const {return scheduler_.Completed();}
    // Host-only scratch obeys the same placed-buffer budget and retirement
    // checks as guest buffers. The caller must retain its allocation to fence.
    Resource AcquireTransientBuffer(u64 bytes){std::lock_guard lock(execution_mutex_);return AcquireBuffer(bytes);}
    // Resource preparation asks its memory owner, not a process-global guest
    // singleton. Standalone renderers retain the requested extent so normal
    // budget/mapping validation rejects oversized descriptors; the actual
    // guest backend overrides this with PS4 address-space boundaries.
    virtual u64 ClampMemoryRange(VAddr address,u64 bytes){
        std::lock_guard lock(execution_mutex_);
        if(bytes>UINT64_MAX-address)throw std::out_of_range("Native guest memory extent overflow");
        if(!IsMapped(address,bytes))throw std::out_of_range("Native guest memory extent outside registered backing");
        return bytes;
    }
    u64 SubmittedFence() const {return scheduler_.Submitted();}
    void MakeQueueWait(ID3D12CommandQueue* queue,u64 fence){
        std::lock_guard lock(execution_mutex_);scheduler_.MakeQueueWait(queue,fence);}
    void WaitExternalFence(ID3D12Fence* fence,u64 value){
        std::lock_guard lock(execution_mutex_);scheduler_.WaitExternal(fence,value);}
    void ValidateHostMemory(VAddr,u64,bool writing=false);
    // Legality only: no CPU reconciliation, ownership change or commands.
    void ValidateGpuBacking(VAddr,u64,bool writing=false);
    // Original clean small shader streams copy physical CPU backing, without
    // publishing an independent native image or formatted buffer into it.
    void ReadShaderStreamBacking(VAddr,std::span<std::byte>);
    virtual bool IsDmaReadable(VAddr,u64);
    bool HasDmaReadOverlap(VAddr,u64)const;
    GuestDmaMemory& DmaMemory();
    // Caller holds ExecutionMutex through the copy and GPU retirement. Never
    // expose a cached image unless its CPU snapshot still matches guest memory.
    virtual Resource* CoherentDisplayTexture(VAddr,UINT,UINT,UINT){return nullptr;}
    virtual void CaptureDisplayCandidates(VAddr,uint64_t,Resource*){}
    // Submit guest rendering and make a consumer queue wait on its GPU fence.
    // Backends with no native display cache retain the conservative CPU path.
    virtual Resource* PrepareDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch,
                                             ID3D12CommandQueue* consumer){
        Finish();return CoherentDisplayTexture(address,width,height,pitch);}
    // Suballocate guest constants; Finish retires the GPU users before reuse.
    UploadArena::Slice UploadConstants(std::span<const std::byte>);
    DescriptorHeapPool& BindingHeapPool();
    void RetainUntilFinish(std::function<void(u64)> retirement);
    void RetainUntilFence(u64 fence,std::function<void(u64)> retirement){
        std::lock_guard lock(execution_mutex_);
        if(!fence||fence>scheduler_.Submitted())throw std::invalid_argument("Unsubmitted retirement fence");
        try{scheduler_.Defer(fence,retirement);}
        catch(...){scheduler_.Wait(fence);retirement(fence);throw;}
    }
    void NotifyOnSubmission(std::function<void(u64)> callback){
        std::lock_guard lock(execution_mutex_);submission_callbacks_.push_back(std::move(callback));}
    void CollectRetirements(){std::lock_guard lock(execution_mutex_);scheduler_.Collect();}
    void EnableHostSpanCache(bool enabled){std::lock_guard lock(execution_mutex_);cache_host_spans_=enabled;readable_spans_.clear();writable_spans_.clear();}
    // Data-only CPU writes do not change page protections or committed backing.
    // Caller retires GPU writes before modifying the guest bytes.
    virtual void NotifyCpuWrite(VAddr address,u64 size){std::lock_guard lock(execution_mutex_);
        ResolveGpuBuffers(address,size);InvalidateCpuBufferPages(address,size);}
    void ExecuteCompute(const ShaderPipeline&,ResourceBindings&,u32 x,u32 y,u32 z,bool wait=true);
    void ValidateDebugMessages();
    struct ColorAttachment {Resource* resource{};D3D12_RENDER_TARGET_VIEW_DESC view{};};
    struct DepthAttachment {
        Resource* resource{};
        D3D12_DEPTH_STENCIL_VIEW_DESC view{};
        bool read_only_depth{},read_only_stencil{};
        bool clear_depth{};
        float clear_value{1.0f};
        bool clear_stencil{};
        UINT8 stencil_value{};
        std::optional<D3D12_RECT> clear_rect;
    };
    struct VertexBinding {Resource* resource{};u64 offset{};UINT bytes{},stride{};};
    struct IndexBinding {Resource* resource{};u64 offset{};UINT bytes{};DXGI_FORMAT format=DXGI_FORMAT_R16_UINT;
        std::shared_ptr<void> allocation;};
    // The index envelope participates in the same prepared batch as shaders.
    // Never materialize or replace another buffer after descriptors are built.
    IndexBinding PreparedIndexBuffer(VAddr address,UINT bytes,DXGI_FORMAT format);
    // Original ObtainBuffer index policy: clean reads <=16KiB take a private
    // physical CPU snapshot; other reads use the already prepared B arena.
    IndexBinding ObtainIndexBuffer(VAddr address,UINT bytes,DXGI_FORMAT format);
    struct DrawArguments {UINT count{},instances=1,first{},first_instance{};INT base_vertex{};};
    // GPU draw arguments. The producing pass owns the
    // GPU count/bounds contract; no CPU draw-count snapshot is substituted.
    struct StreamDrawArguments {
        Resource* resource{};u64 offset{};
        u32 max_count=1,stride=0; // Zero stride selects the DRAW/INDEXED ABI size.
        Resource* count_resource{};u64 count_offset{};
        // Each command starts with a GPU address for b0/space31, then DRAW.
        // Caller proves every address lies in this retained constant buffer.
        Resource* runtime_constants{};
    };
    struct StreamOutputBinding {
        Resource* data{};Resource* filled_size{};
        u64 offset{},bytes{},counter_offset{};
    };
    void ExecuteGraphics(const ShaderPipeline&,ResourceBindings&,const GuestGraphicsState&,
        std::span<const ColorAttachment>,const DepthAttachment*,std::span<const D3D12_VIEWPORT>,
        std::span<const D3D12_RECT>,std::span<const float,4> blend_constants,
        const DrawArguments&,std::span<const VertexBinding> vertices={},const IndexBinding* indices=nullptr,bool wait=true,
        const StreamDrawArguments* stream_draw=nullptr,const StreamOutputBinding* stream_output=nullptr);
    explicit Rasterizer(ID3D12Device* device);
    ~Rasterizer() override;
    void Draw(bool,u32=0) override;
    void DrawIndirect(bool,VAddr,u32,u32,u32,VAddr,u16,u16) override;
    void DispatchDirect() override;
    void DispatchIndirect(VAddr,u32,u32) override;
    void SetMarkersEnabled(bool host,bool guest);
    bool MarkersEnabled()const override{return host_markers_enabled_;}
    void ScopeMarkerBegin(const std::string_view&,bool=false) override;
    void ScopeMarkerEnd(bool=false) override;
    void ScopedMarkerInsert(const std::string_view&,bool=false);
    void ScopedMarkerInsertColor(const std::string_view&,u32,bool=false) override;
    void FillBuffer(VAddr,u32,u32,bool) override;
    void CopyBuffer(VAddr,VAddr,u32,bool,bool) override;
    u32 ReadDataFromGds(u32) override;
    bool InvalidateMemory(VAddr,u64,bool=false) override;
    bool ReadMemory(VAddr,u64,bool=false) override;
    // Internal upload dependency: reconcile only the source, without a CPU
    // download window that would unnecessarily retire neighbouring GPU data.
    virtual bool PrepareCpuUpload(VAddr,u64);
    bool IsMapped(VAddr,u64) override;
    void MapMemory(VAddr,u64) override;
    void RegisterMemory(VAddr,u64) override;
    void UnmapMemory(VAddr,u64) override;
    void NotifyMemoryProtection(VAddr,u64) override;
    void NotifyHostMapping(VAddr,u64,bool,bool) override;
    u64 Flush() override;
    void Finish() override;
    void OnSubmit() override {Flush();}
    void OnFence() override {Finish();}
    void OnFrame() override;
    u64 CachedBufferBytes() const {std::lock_guard lock(execution_mutex_);return cached_buffer_bytes_;}
    u64 BufferCacheBudget() const {std::lock_guard lock(execution_mutex_);return buffer_cache_budget_;}
};
}
