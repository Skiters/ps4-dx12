#include "common/api_trace.h"
#include "rasterizer.h"
#include <dxgi1_4.h>
#include <cstring>
#include "shader_pipeline.h"
#include "compute_pipeline.h"
#include "guest_dma.h"
#include "guest_page_range.h"
#include "common/scope_exit.h"
#include <algorithm>
#include <execution>
#include <numeric>
#include <thread>
#include <utility>
#include "gpu_markers.h"
namespace NativeDX12 {
namespace {
void RememberSpan(std::map<VAddr,VAddr>& spans,VAddr begin,VAddr end){
    auto it=spans.lower_bound(begin);
    if(it!=spans.begin()&&std::prev(it)->second>=begin)it=std::prev(it);
    while(it!=spans.end()&&it->first<=end){begin=std::min(begin,it->first);end=std::max(end,it->second);it=spans.erase(it);}
    spans.emplace(begin,end);
}
bool CoversSpan(const std::map<VAddr,VAddr>& spans,VAddr begin,VAddr end){
    auto it=spans.upper_bound(begin);return it!=spans.begin()&&std::prev(it)->second>=end;
}
void ForgetSpan(std::map<VAddr,VAddr>& spans,VAddr begin,VAddr end){
    auto it=spans.lower_bound(begin);
    if(it!=spans.begin()&&std::prev(it)->second>begin)it=std::prev(it);
    while(it!=spans.end()&&it->first<end){
        const auto first=it->first,last=it->second;it=spans.erase(it);
        if(first<begin)spans.emplace(first,begin);
        if(last>end){spans.emplace(end,last);break;}
    }
}
// Mapping coverage is not proof of committed CPU backing (notably PS4 PRT).
// Until sparse-copy callbacks are wired, fail explicitly instead of dereferencing holes.
void CheckHostSpan(VAddr address, u64 size, bool writing) {
    if (!address || !size || address > UINT64_MAX - size)
        throw std::out_of_range("invalid host memory span");
    const auto end = address + size;
    while (address < end) {
        MEMORY_BASIC_INFORMATION region{};
        if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region)) ||
            region.State != MEM_COMMIT || (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
            throw std::runtime_error("native guest access needs committed backing or sparse-copy callback");
        const auto protection = region.Protect & 0xff;
        const bool writable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
            protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
        const bool readable = writable || protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ;
        if (!(writing ? writable : readable))
            throw std::runtime_error("native guest backing has incompatible CPU protection");
        const auto base = reinterpret_cast<VAddr>(region.BaseAddress);
        if (base > UINT64_MAX - region.RegionSize || base + region.RegionSize <= address)
            throw std::out_of_range("invalid host region extent");
        address = std::min(end, base + region.RegionSize);
    }
}
}
struct BufferBudgetObserver {
    ComPtr<IDXGIAdapter3> adapter;HANDLE event{};DWORD cookie{};bool registered{};u64 checked{};
    explicit BufferBudgetObserver(ID3D12Device* device){
        ComPtr<IDXGIFactory4> factory;
        if(SUCCEEDED(DX12_API_CALL(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))))
            DX12_API_CALL(factory->EnumAdapterByLuid(DX12_API_CALL(device->GetAdapterLuid()),IID_PPV_ARGS(&adapter)));
        if(adapter){
            event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
            if(event)registered=SUCCEEDED(DX12_API_CALL(adapter->RegisterVideoMemoryBudgetChangeNotificationEvent(event,&cookie)));
            if(!registered&&event){CloseHandle(event);event=nullptr;}
        }
    }
    ~BufferBudgetObserver(){
        if(registered)DX12_API_CALL(adapter->UnregisterVideoMemoryBudgetChangeNotification(cookie));
        if(event)CloseHandle(event);
    }
    bool Query(DXGI_QUERY_VIDEO_MEMORY_INFO& memory,bool force=false){
        const auto now=GetTickCount64();
        if(!force&&checked&&now-checked<1000&&(!event||WaitForSingleObject(event,0)!=WAIT_OBJECT_0))return false;
        checked=now;
        return adapter&&SUCCEEDED(DX12_API_CALL(adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory)))&&memory.Budget;
    }
};
void Rasterizer::RefreshBufferBudget(){
    std::lock_guard lock(execution_mutex_);
    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
    if(!buffer_budget_observer_->Query(memory))return;
    const auto limits=memory_perf_.Budget(memory.Budget,memory.CurrentUsage,buffer_allocator_.CommittedBytes());
    memory_pressure_=memory_perf_.enabled?limits.cache<memory_perf_.requested:memory.CurrentUsage>memory.Budget;
    if(Diagnostics::Enabled()&&(limits.cache!=buffer_cache_budget_||Diagnostics::Hub::Get().capturing))fprintf(stderr,
        "DX12 memory mode=%u requested=%llu cache=%llu arena=%llu local_usage=%llu local_budget=%llu buffer_heaps=%llu resident=%llu gpu_pool=%llu snapshot_pool=%llu staging_pool=%llu pressure=%u\n",
        unsigned(memory_perf_.enabled),memory_perf_.requested,limits.cache,limits.arena,memory.CurrentUsage,memory.Budget,
        buffer_allocator_.CommittedBytes(),cached_buffer_bytes_,pooled_buffer_bytes_,snapshot_pool_bytes_,large_upload_pool_bytes_,unsigned(memory_pressure_));
    buffer_allocator_.SetBudget(limits.arena);
    buffer_cache_budget_=limits.cache;
    gpu_pool_budget_=memory_perf_.enabled?std::min<u64>(512*MemoryMiB,limits.cache/8):128*MemoryMiB;
    MEMORYSTATUSEX ram{sizeof(ram)};
    if(memory_perf_.enabled&&GlobalMemoryStatusEx(&ram)&&ram.ullAvailPhys<4ull*1024*MemoryMiB)memory_pressure_=true;
    TrimPools();
}
Rasterizer::Rasterizer(ID3D12Device* device):device_(device),scheduler_(device),buffer_allocator_(device) {
    buffer_allocator_.SetCompactLargePages(memory_perf_.enabled);
    buffer_budget_observer_=std::make_unique<BufferBudgetObserver>(device);
    RefreshBufferBudget();
    MapMemory(reinterpret_cast<VAddr>(gds_.data()),gds_.size());
}
bool Rasterizer::OwnsGpuRange(const Mapping& mapping,u64 offset,u64 bytes){
    return bytes&&offset<=mapping.size&&bytes<=mapping.size-offset&&
        CoversSpan(mapping.gpu_owned_ranges,offset,offset+bytes);
}
void Rasterizer::MarkGpuOwned(Mapping& mapping,u64 offset,u64 bytes){
    if(!bytes||offset>mapping.size||bytes>mapping.size-offset)
        throw std::out_of_range("GPU ownership range exceeds its buffer envelope");
    RememberSpan(mapping.gpu_owned_ranges,offset,offset+bytes);
    ForgetSpan(mapping.cpu_dirty_ranges,offset,offset+bytes);mapping.gpu_owned=true;
}
void Rasterizer::UnmarkGpuOwned(Mapping& mapping,u64 offset,u64 bytes){
    if(!bytes||offset>mapping.size||bytes>mapping.size-offset)
        throw std::out_of_range("GPU ownership release exceeds its buffer envelope");
    ForgetSpan(mapping.gpu_owned_ranges,offset,offset+bytes);
    mapping.gpu_owned=!mapping.gpu_owned_ranges.empty();
}
Rasterizer::~Rasterizer(){try{DrainPriorityOperations();}catch(...) {}
    // Upload arenas are members destroyed before the scheduler. Drain CPU
    // writers even when Finish later throws due to device/submission failure.
    try{scheduler_.WaitHostCopies();}catch(...){}
    try{Finish();}catch(...) {/* device loss: release resources */}}
GuestDmaMemory& Rasterizer::DmaMemory(){
    if(!dma_memory_)dma_memory_=std::make_unique<GuestDmaMemory>(device_.Get());
    return *dma_memory_;
}
bool Rasterizer::IsDmaReadable(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size||!IsMapped(address,size))return false;
    if(CoversSpan(committed_readable_,address,address+size))return true;
    auto found=mappings_.upper_bound(address);
    if(found!=mappings_.begin()){--found;if(OwnsGpuRange(found->second,address-found->first,size)&&
        size<=found->second.size-(address-found->first))return true;}
    try{CheckHostSpan(address,size,false);return true;}
    catch(const std::runtime_error&){return false;}
    catch(const std::out_of_range&){return false;}
}
bool Rasterizer::HasDmaReadOverlap(VAddr address,u64 size)const{
    std::lock_guard lock(execution_mutex_);
    return dma_memory_&&dma_memory_->HasReadOverlap(address,size);
}
bool Rasterizer::ReadDmaBacking(VAddr address,std::span<std::byte> bytes){
    // Standalone/non-guest callers can use accessible host backing. The core
    // overrides this with its physical alias reader for protected guest pages.
    try{CheckHostSpan(address,bytes.size(),false);}
    catch(const std::runtime_error&){return false;}
    memcpy(bytes.data(),reinterpret_cast<const void*>(address),bytes.size());return true;
}
void Rasterizer::ReadShaderStreamBacking(VAddr address,std::span<std::byte> bytes){
    std::lock_guard lock(execution_mutex_);
    ValidateGpuBacking(address,bytes.size());
    if(!ReadDmaBacking(address,bytes))throw std::runtime_error("Shader CPU stream lost physical backing");
    Diagnostics::Count("buffer-stream.physical-bytes",bytes.size());
}
DescriptorHeapPool& Rasterizer::BindingHeapPool(){
    if(!binding_heap_pool_)binding_heap_pool_=std::make_unique<DescriptorHeapPool>(device_.Get());
    return *binding_heap_pool_;
}
void Rasterizer::RetainUntilFinish(std::function<void(u64)> retirement){
    std::lock_guard lock(execution_mutex_);
    deferred_retirements_.push_back(std::move(retirement));
}
std::shared_ptr<Rasterizer::TemporaryBuffer> Rasterizer::AcquireTemporaryBuffer(u64 bytes,const void* initial){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||bytes>65536)return {};
    const auto words=size_t((bytes+3)/4);
    std::shared_ptr<TemporaryBuffer> block;
    for(const auto& candidate:temporary_pool_)if(candidate.use_count()==1&&candidate->words.size()==words){block=candidate;break;}
    if(!block){
        if(temporary_pool_.size()>=256)return {};
        block=std::make_shared<TemporaryBuffer>();block->words.resize(words);
        MapMemory(reinterpret_cast<VAddr>(block->words.data()),u64(words)*4);
        temporary_pool_.push_back(block);
    }
    std::fill(block->words.begin(),block->words.end(),0u);
    if(initial)memcpy(block->words.data(),initial,size_t(bytes));
    return block;
}
void Rasterizer::CheckBacking(VAddr address,u64 size,bool writing){
    if(!address||!size||address>UINT64_MAX-size)throw std::out_of_range("Invalid cached host span");
    const auto end=address+size;
    if(CoversSpan(writing?committed_writable_:committed_readable_,address,end))return;
    if(cache_host_spans_){
        const auto& spans=writing?writable_spans_:readable_spans_;
        if(CoversSpan(spans,address,end))return;
    }
    CheckHostSpan(address,size,writing);
    if(cache_host_spans_){
        const auto remember=[&](auto& spans){
            if(spans.size()>=4096)spans.clear();
            RememberSpan(spans,address,end);
        };
        remember(readable_spans_);if(writing)remember(writable_spans_);
    }
}
void Rasterizer::ForgetBacking(VAddr address,u64 size){
    for(auto* spans:{&readable_spans_,&writable_spans_,&committed_readable_,&committed_writable_}){
        auto it=spans->lower_bound(address);
        if(it!=spans->begin()&&std::prev(it)->second>address)it=std::prev(it);
        while(it!=spans->end()&&it->first<address+size){
            const auto begin=it->first,end=it->second;
            it=spans->erase(it);
            if(begin<address)spans->emplace(begin,address);
            if(end>address+size){spans->emplace(address+size,end);break;}
        }
    }
}
void Rasterizer::CancelImageBackingCommits(VAddr address,u64 bytes){
    std::erase_if(pending_image_tickets_,[&](const auto& weak){
        auto ticket=weak.lock();if(!ticket)return true;
        if(address<ticket->address+ticket->bytes&&ticket->address<address+bytes)
            ticket->backing_valid=false;
        return false;
    });
}
void Rasterizer::NotifyMemoryProtection(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!size||address>UINT64_MAX-size)throw std::out_of_range("Invalid protection notification");
    ForgetBacking(address,size);
}
void Rasterizer::NotifyHostMapping(VAddr address,u64 size,bool readable,bool writable){
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size||(!readable&&writable))
        throw std::invalid_argument("Invalid committed host mapping notification");
    CancelImageBackingCommits(address,size);ForgetBacking(address,size);
    if(readable)RememberSpan(committed_readable_,address,address+size);
    if(writable)RememberSpan(committed_writable_,address,address+size);
}
void Rasterizer::ValidateHostMemory(VAddr address,u64 size,bool writing) {
    std::lock_guard lock(execution_mutex_);
    if(!IsMapped(address,size))throw std::out_of_range("Native shader memory is unmapped: address="+std::to_string(address)+" bytes="+std::to_string(size)+" writing="+std::to_string(writing));
    CheckBacking(address,size,writing);
}
void Rasterizer::ValidateGpuBacking(VAddr address,u64 size,bool writing) {
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size||!IsMapped(address,size))
        throw std::out_of_range("Invalid native GPU backing range");
    CheckGpuBacking(address,size,writing);
    Diagnostics::Count("buffer-access.gpu-validation");
}
UploadArena::Slice Rasterizer::UploadConstants(std::span<const std::byte> data) {
    std::lock_guard lock(execution_mutex_);
    if(data.empty()||data.size()>65536)throw std::invalid_argument("Native constant data size");
    if(!constants_)constants_=std::make_unique<UploadArena>(device_.Get(),128*1024);
    const auto bytes=(data.size()+255)&~size_t(255);
    auto slice=constants_->Allocate(bytes);
    memset(slice.cpu,0,bytes);memcpy(slice.cpu,data.data(),data.size());
    return slice;
}
void Rasterizer::Unsupported(const char* op){throw std::runtime_error(std::string("Native D3D12 backend does not implement PS4 ")+op);}
void Rasterizer::Draw(bool,u32){Unsupported("Draw/shader translation");}
void Rasterizer::DrawIndirect(bool,VAddr,u32,u32,u32,VAddr,u16,u16){Unsupported("DrawIndirect/shader translation");}
void Rasterizer::DispatchDirect(){Unsupported("DispatchDirect/shader translation");}
void Rasterizer::DispatchIndirect(VAddr,u32,u32){Unsupported("DispatchIndirect/shader translation");}
void Rasterizer::Begin(){
    auto* list=scheduler_.Begin();
    while(recorded_marker_scopes_<marker_scopes_.size()){
        GpuMarkers::Begin(list,marker_scopes_[recorded_marker_scopes_]);++recorded_marker_scopes_;
    }
}
void Rasterizer::SetMarkersEnabled(bool host,bool guest){
    std::lock_guard lock(execution_mutex_);
    if(!marker_scopes_.empty())throw std::logic_error("Cannot change native markers inside an active scope");
    host_markers_enabled_=host;guest_markers_enabled_=guest;
}
void Rasterizer::ScopeMarkerBegin(const std::string_view& text,bool guest){
    std::lock_guard lock(execution_mutex_);
    if(guest?!guest_markers_enabled_:!host_markers_enabled_)return;
    marker_scopes_.emplace_back(text);
    try{Begin();}catch(...){marker_scopes_.pop_back();throw;}
}
void Rasterizer::ScopeMarkerEnd(bool guest){
    std::lock_guard lock(execution_mutex_);
    if(guest?!guest_markers_enabled_:!host_markers_enabled_)return;
    if(marker_scopes_.empty()){fprintf(stderr,"DX12 marker pop without a matching scope\n");return;}
    Begin();GpuMarkers::End(scheduler_.Begin());--recorded_marker_scopes_;marker_scopes_.pop_back();
}
void Rasterizer::ScopedMarkerInsertColor(const std::string_view& text,u32 color,bool guest){
    std::lock_guard lock(execution_mutex_);
    if(guest?!guest_markers_enabled_:!host_markers_enabled_)return;
    GpuMarkers::Insert(RecordingList(),text,color);
}
void Rasterizer::ScopedMarkerInsert(const std::string_view& text,bool guest){
    ScopedMarkerInsertColor(text,0,guest);
}
bool Rasterizer::PreserveBufferBacking(const Mapping& mapping)const{
    return mapping.gpu_cpu_diverged||resolving_buffers_.contains(mapping.base)||
        std::ranges::find(preparing_buffer_contents_,&mapping)!=preparing_buffer_contents_.end()||
        (mapping.backing_version&&mapping.backing_version.use_count()>1)||
        std::ranges::any_of(mapping.preserved_versions,[](const auto& weak){return !weak.expired();});
}
u64 Rasterizer::AdmitBufferCache(std::span<const std::pair<VAddr,VAddr>> active,u64 budget,u64 additional,u64 savings){
    const auto fits=[&]{const auto used=CachedBufferBytes()>savings?CachedBufferBytes()-savings:0;
        return additional<=budget&&used<=budget-additional;};
    if(fits())return budget;
    // Pinned Vulkan resident_ranges keep B backing through arena migration.
    // A cache target may reclaim reconstructible B, not an independent B1 or
    // a generation selected by a still-live binding/preparation.
    Finish();
    const auto overlaps=[&](const Mapping& mapping){return std::ranges::any_of(active,[&](const auto& range){
        return mapping.base<range.second&&range.first<mapping.base+mapping.size;});};
    while(!fits()){
        u64 required{};
        for(const auto& [begin,end]:active){if(end-begin>UINT64_MAX-required)throw std::overflow_error("Buffer admission footprint overflow");required+=end-begin;}
        for(const auto& [_,mapping]:mappings_)if(PreserveBufferBacking(mapping)){
            auto outside=mapping.size;
            for(const auto& [begin,end]:active){const auto a=std::max(begin,mapping.base),b=std::min(end,mapping.base+mapping.size);
                if(a<b)outside-=b-a;}
            if(outside>UINT64_MAX-required)throw std::overflow_error("Preserved buffer footprint overflow");required+=outside;
        }
        if(required>budget){Diagnostics::Count("buffer-cache.mandatory-admitted.bytes",required-budget);budget=required;}
        if(fits())break;
        std::vector<VAddr> candidates;
        for(const auto& [base,mapping]:mappings_)if(!PreserveBufferBacking(mapping)&&!overlaps(mapping))candidates.push_back(base);
        if(candidates.empty())throw std::logic_error("Active buffer admission footprint changed during preparation");
        if(memory_perf_.enabled)std::sort(candidates.begin(),candidates.end(),[&](VAddr a,VAddr b){
            return mappings_.at(a).last_use<mappings_.at(b).last_use;});
        // Elect once per pressure pass instead of rescanning the entire cache
        // for each victim. Callbacks may acquire a binding, so recheck both
        // before and after reconciliation; changed graphs get another pass.
        for(const auto base:candidates){
            if(fits())break;
            auto chosen=mappings_.find(base);
            if(chosen==mappings_.end()||PreserveBufferBacking(chosen->second)||overlaps(chosen->second))continue;
            ResolveGpuBuffers(base,chosen->second.size);Finish();chosen=mappings_.find(base);
            if(chosen==mappings_.end()||PreserveBufferBacking(chosen->second)||overlaps(chosen->second))continue;
            TraceMapping(chosen->second,"budget");cached_buffer_bytes_-=chosen->second.size;
            dirty_mappings_.erase(base);RemoveBufferIndex(base);RetireBuffer(chosen->second);mappings_.erase(chosen);
        }
    }
    return budget;
}
Rasterizer::Mapping& Rasterizer::Find(VAddr address,u64 size,bool gds,u64 batch_budget,bool physical_dma){
    if(preparing_buffer_envelope_)
        throw std::logic_error("Cannot materialize buffers from a growth witness callback");
    if(gds){if(address>gds_.size()||size>gds_.size()-address)throw std::out_of_range("GDS range");address+=reinterpret_cast<VAddr>(gds_.data());}
    if(!size||address>UINT64_MAX-size)throw std::out_of_range("invalid guest range");
    if(!IsMapped(address,size))throw std::out_of_range("unmapped guest memory");
    auto it=mappings_.upper_bound(address);
    if(it!=mappings_.begin()){
        --it;
        if(address-it->first<=it->second.size&&size<=it->second.size-(address-it->first)){
            if(!it->second.initial_upload_failed){Diagnostics::Count("buffer-cache.hit");it->second.last_use=++use_clock_;
                churn_.Record(address,size,it->first,it->second.size,it->second.allocation_id,false,it->second.dirty,it->second.gpu_owned,scheduler_.Completed());return it->second;}
            // An upload failure can happen after copy submission. Keep its
            // allocation alive until the ordinary fence retirement path runs,
            // but never return partially initialized contents on a retry.
            DropCached(it->first,it->second.size,true,"failed-reuse");
        }
    }
    Diagnostics::Count("buffer-cache.miss");Diagnostics::Count("buffer-materialized.bytes",size);
    // Correctness-first bounded cache: operation-sized materialization only.
    // A new overlapping slice reconciles previous GPU writes before rebuilding.
    // Raw descriptor element counts bound each view; aggregate resident bytes
    // are governed separately by the adapter's current memory budget.
    constexpr u64 max_slice=Shader::NativeDma::MaxBufferBytes;
    auto cache_budget=std::max(buffer_cache_budget_,batch_budget);
    if(size>max_slice||size>cache_budget)throw std::runtime_error("Native buffer slice exceeds current slice/residency budget");
    for(auto overlap=FirstOverlapping(address);overlap!=mappings_.end()&&overlap->first<address+size;++overlap)
        if(overlap->second.gpu_cpu_diverged)
            throw std::runtime_error("GPU-only B overlap requires preserving batch materialization");
    DropCached(address,size,true);
    const std::pair<VAddr,VAddr> active{address,address+size};
    cache_budget=AdmitBufferCache(std::span(&active,1),cache_budget,size);
    if(physical_dma)ValidateGpuBacking(address,size,false);
    else CheckBacking(address,size,false);
    // Publish before GPU allocation/upload; concurrent CPU invalidation must
    // take the serialized path as soon as a GPU view is being materialized.
    {std::lock_guard lock(memory_index_mutex_);indexed_buffers_[address]=address+size;}
    Mapping* stored{};
    try{
        Mapping range{address,size,AcquireBuffer((size+3)&~u64(3))};
        range.backing_version=std::make_shared<const BufferBackingVersion>(address,size,range.gpu.handle.Get());
        range.allocation_id=acquired_allocation_id_;range.last_use=++use_clock_;
        stored=&mappings_.emplace(address,std::move(range)).first->second;
        churn_.Record(address,size,address,size,stored->allocation_id,true,false,false,scheduler_.Completed());
    }catch(...){
        // No GPU command references this allocation yet. Do not leave the CPU
        // invalidation index advertising a mapping that was never created.
        RemoveBufferIndex(address);throw;
    }
    cached_buffer_bytes_+=size;
    try{
        if(physical_dma){
            std::vector<std::byte> physical(static_cast<size_t>(size));
            if(!ReadDmaBacking(address,physical))throw std::runtime_error("DMA materialization lost physical backing");
            Upload(*stored,0,size,physical.data());
        }else Upload(*stored,0,size,reinterpret_cast<void*>(address));
    }
    catch(...){stored->initial_upload_failed=true;throw;}
    return *stored;
}
Resource Rasterizer::AcquireBuffer(u64 size){
    const auto completed=scheduler_.Completed();
    auto best=buffer_pool_.end();
    for(auto it=buffer_pool_.begin();it!=buffer_pool_.end();++it){
        if(it->gpu.allocation&&it->gpu.allocation.use_count()>1)continue;
        const auto capacity=it->gpu.handle->GetDesc().Width;
        if(it->fence<=completed&&capacity>=size&&capacity-size<=std::max<u64>(65536,size/4)&&
           (best==buffer_pool_.end()||capacity<best->gpu.handle->GetDesc().Width))best=it;
    }
    if(best==buffer_pool_.end()){
        Diagnostics::Count("buffer-allocation.new");
        Diagnostics::Count(buffer_pool_.empty()?"buffer-pool.empty":"buffer-pool.no-compatible-retired-allocation");
        acquired_allocation_id_=++next_allocation_id_;
        try{return buffer_allocator_.Allocate(size);}
        catch(const BufferBudgetExhausted&){
            // Idle pool resources can occupy otherwise reusable arena pages.
            // Never reclaim a lease held by a view or an unfinished submission.
            for(auto it=buffer_pool_.begin();it!=buffer_pool_.end();){
                if(it->fence>completed||(it->gpu.allocation&&it->gpu.allocation.use_count()>1)){++it;continue;}
                pooled_buffer_bytes_-=it->gpu.handle->GetDesc().Width;it=buffer_pool_.erase(it);
            }
            buffer_allocator_.Trim();Diagnostics::Count("buffer-allocation.pressure-retry");
            try{return buffer_allocator_.Allocate(size);}
            catch(const BufferBudgetExhausted&){
                // The configured cache/arena target is not permission to drop
                // pinned guest B or in-flight copy sources. Admit mandatory
                // backing only within a fresh real adapter budget/reserve.
                DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
                if(!buffer_budget_observer_->Query(memory,true))throw;
                const auto available=memory_perf_.AvailableBacking(memory.Budget,memory.CurrentUsage,buffer_allocator_.CommittedBytes());
                constexpr u64 alignment=D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
                if(size>UINT64_MAX-(alignment-1))throw;
                const auto minimum=(size+alignment-1)&~(alignment-1),committed=buffer_allocator_.CommittedBytes();
                if(committed>available||minimum>available-committed)throw;
                buffer_allocator_.SetBudget(committed+minimum);
                Diagnostics::Count("buffer-allocation.mandatory-admission");
                return buffer_allocator_.Allocate(size);
            }
        }
    }
    Diagnostics::Count("buffer-allocation.reuse");
    acquired_allocation_id_=best->allocation_id;
    pooled_buffer_bytes_-=best->gpu.handle->GetDesc().Width;
    auto result=std::move(best->gpu);buffer_pool_.erase(best);return result;
}
void Rasterizer::RetireBuffer(Mapping& mapping){
    if(mapping.gpu_owned)throw std::logic_error("Retiring GPU-owned buffer before CPU reconciliation");
    if(mapping.cpu_watched||mapping.cpu_watch_pending||mapping.cpu_partial_proof){release_buffer_(mapping.base,mapping.size);
        mapping.cpu_watched=false;mapping.cpu_watch_pending=false;mapping.cpu_partial_proof=false;mapping.cpu_dirty_ranges.clear();}
    const auto capacity=mapping.gpu.handle->GetDesc().Width;
    // Retired resources remain alive through their last fence. Keep a bounded
    // best-fit pool so changing PS4 slices do not allocate committed heaps per draw.
    // Every caller submits and waits before retirement; never label open-list
    // users with the previous submission's fence.
    if(scheduler_.Recording()||scheduler_.Completed()<scheduler_.Submitted())
        throw std::logic_error("Retiring buffer before its recorded GPU users complete");
    if(memory_perf_.enabled&&!memory_pressure_&&mapping.cpu_snapshot.capacity()&&
       mapping.cpu_snapshot.capacity()<=memory_perf_.cpu_pool/2-snapshot_pool_bytes_&&snapshot_pool_.size()<4096){
        mapping.cpu_snapshot.clear();snapshot_pool_bytes_+=mapping.cpu_snapshot.capacity();
        snapshot_pool_.push_back(std::move(mapping.cpu_snapshot));
    }
    if(pooled_buffer_bytes_+capacity<=gpu_pool_budget_&&buffer_pool_.size()<4096){
        pooled_buffer_bytes_+=capacity;buffer_pool_.push_back({std::move(mapping.gpu),scheduler_.Submitted(),mapping.allocation_id});
    }
}
void Rasterizer::TrimPools(){
    if(memory_pressure_)for(auto it=shader_read_copy_pool_.begin();it!=shader_read_copy_pool_.end();){
        if(it->use_count()!=1){++it;continue;}
        shader_read_copy_pool_bytes_-=(*it)->handle->GetDesc().Width;it=shader_read_copy_pool_.erase(it);
    }
    if(!memory_perf_.enabled)return;
    const auto limit=memory_pressure_?0:gpu_pool_budget_;
    for(auto it=buffer_pool_.begin();it!=buffer_pool_.end()&&pooled_buffer_bytes_>limit;){
        if(it->fence>scheduler_.Completed()||(it->gpu.allocation&&it->gpu.allocation.use_count()>1)){++it;continue;}
        pooled_buffer_bytes_-=it->gpu.handle->GetDesc().Width;it=buffer_pool_.erase(it);
    }
    if(memory_pressure_){snapshot_pool_.clear();snapshot_pool_bytes_=0;
        for(auto it=large_upload_pool_.begin();it!=large_upload_pool_.end();){
            if(it->used){++it;continue;}large_upload_pool_bytes_-=it->capacity;it=large_upload_pool_.erase(it);
        }
        buffer_allocator_.Trim();
    }
}
void Rasterizer::ResizeSnapshot(Mapping& mapping){
    mapping.protected_backing_epoch=0;
    if(memory_perf_.enabled&&mapping.cpu_snapshot.capacity()<mapping.size){
        auto best=snapshot_pool_.end();
        for(auto it=snapshot_pool_.begin();it!=snapshot_pool_.end();++it)
            if(it->capacity()>=mapping.size&&it->capacity()-mapping.size<=std::max<u64>(65536,mapping.size/4)&&
               (best==snapshot_pool_.end()||it->capacity()<best->capacity()))best=it;
        if(best!=snapshot_pool_.end()){
            snapshot_pool_bytes_-=best->capacity();mapping.cpu_snapshot=std::move(*best);snapshot_pool_.erase(best);
            Diagnostics::Count("buffer-snapshot.allocation-reuse");
            ++memory_reuse_.snapshots;
        }
    }
    if(mapping.cpu_snapshot.capacity()<mapping.size)Diagnostics::Count("buffer-snapshot.allocation-new");
    mapping.cpu_snapshot.resize(size_t(mapping.size));
}
void Rasterizer::ReleaseCpuBufferWatches(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!size)return;
    if(address>UINT64_MAX-size)throw std::out_of_range("CPU buffer watch range overflow");
    // Logical mappings are disjoint, but unaligned mappings share OS pages.
    // Retire every affected page witness, including a neighbouring sharp.
    const auto pages=GuestProtectionPages(address,size);
    const auto watch_begin=pages?pages->address:address;
    const auto watch_end=pages?pages->address+pages->size:address+size;
    for(auto it=FirstOverlapping(watch_begin);it!=mappings_.end()&&it->first<watch_end;++it){
        ++memory_reuse_.watch_candidates;auto& mapping=it->second;
        if(!(mapping.cpu_watched||mapping.cpu_watch_pending||mapping.cpu_partial_proof))continue;
        if(mapping.gpu_owned&&release_cpu_pages_){
            // Partial GPU ownership shares one arena with read-only CPU proof.
            // Retire only proof gaps; strong NOACCESS pages keep their owner.
            auto retained=RetainedImageProtectionRanges();
            for(const auto& [begin,end]:mapping.gpu_owned_ranges){
                retained.emplace_back(mapping.base+begin,end-begin);
            }
            // Logical arena boundaries need not be page aligned after growth.
            // Retire the OS-page envelope, retaining every strong GPU/image
            // page and every other buffer's proof without a restore window.
            ReleaseUnusedReadOnlyWatches(mapping.base,mapping.size,retained,mapping.base);
        }else release_buffer_(it->first,mapping.size);
        mapping.cpu_watched=false;mapping.cpu_watch_pending=false;mapping.cpu_partial_proof=false;
        mapping.cpu_dirty_ranges.clear();
    }
}
void Rasterizer::ReleaseUnusedReadOnlyWatches(VAddr address,u64 size,std::span<const std::pair<VAddr,u64>> retained,
    std::optional<VAddr> retiring_buffer){
    std::lock_guard lock(execution_mutex_);
    const auto pages=GuestProtectionPages(address,size);
    if(!pages||!release_cpu_pages_)
        throw std::invalid_argument("Invalid image-only read-only watch retirement");
    address=pages->address;size=pages->size;
    const auto end=address+size;std::vector<std::pair<VAddr,VAddr>> keep;
    const auto retain_pages=[&](VAddr first,VAddr last){
        if(first>=last)return;
        const auto pages=GuestProtectionPages(first,last-first);
        if(!pages)throw std::invalid_argument("Invalid retained buffer proof range");
        keep.emplace_back(std::max(address,pages->address),std::min(end,pages->address+pages->size));
    };
    for(const auto& [base,bytes]:retained){
        const auto protected_range=GuestProtectionPages(base,bytes);
        if(!protected_range)
            throw std::invalid_argument("Invalid retained image watch range");
        const auto first=std::max(address,protected_range->address),last=std::min(end,protected_range->address+protected_range->size);
        if(first<last)keep.emplace_back(first,last);
    }
    for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<end;++it){
        const auto& mapping=it->second;
        if(retiring_buffer&&mapping.base==*retiring_buffer)continue;
        const auto first=std::max(address,mapping.base),last=std::min(end,mapping.base+mapping.size);
        // A native arena may own only selected GPU pages. Keeping its whole
        // envelope here would strand an unrelated image's RO guard after that
        // image and all CPU proofs have retired, making legal stores fault.
        for(const auto& [begin,finish]:mapping.gpu_owned_ranges)
            retain_pages(std::max(first,mapping.base+begin),std::min(last,mapping.base+finish));
        if(mapping.cpu_watched||mapping.cpu_watch_pending){retain_pages(first,last);continue;}
        if(!mapping.cpu_partial_proof)continue;
        auto cursor=first;
        for(const auto& [begin,finish]:mapping.cpu_dirty_ranges){
            const auto dirty_first=std::clamp(mapping.base+begin,first,last);
            const auto dirty_last=std::clamp(mapping.base+finish,first,last);
            if(cursor<dirty_first)retain_pages(cursor,dirty_first);
            cursor=std::max(cursor,dirty_last);
        }
        if(cursor<last)retain_pages(cursor,last);
    }
    // Unprotect only image-only gaps. Existing buffer proofs/ownership keep
    // their OS protection continuously, without a restore/rearm store window.
    std::sort(keep.begin(),keep.end());auto cursor=address;
    for(const auto& [first,last]:keep){
        if(cursor<first&&!release_cpu_pages_(cursor,first-cursor))
            throw std::runtime_error("Cannot retire an image-only read-only watch gap");
        cursor=std::max(cursor,last);
    }
    if(cursor<end&&!release_cpu_pages_(cursor,end-cursor))
        throw std::runtime_error("Cannot retire an image-only read-only watch tail");
}
void Rasterizer::InvalidateCpuBufferPages(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!size)return;
    if(address>UINT64_MAX-size)throw std::out_of_range("CPU dirty page range overflow");
    const auto invalidate=[&](Mapping& m){
        InvalidateFrameReadback(m);
        m.protected_backing_epoch=0;
        const auto forget=[&]{
            if(&m!=preparing_buffer_envelope_){ReleaseCpuBufferWatches(m.base,m.size);return;}
            // This private target has not acquired its own guard yet. Current
            // mappings still own physical protection until publication.
            m.cpu_watched=m.cpu_watch_pending=m.cpu_partial_proof=false;m.cpu_dirty_ranges.clear();
            RememberSpan(m.cpu_dirty_ranges,0,m.size);
        };
        const auto begin=std::max(m.base,address&~VAddr(4095));
        const auto last=std::min(m.base+m.size,address+size);
        const auto end=last+std::min((-last)&u64(4095),m.base+m.size-last);
        if(m.gpu_owned||m.gpu_cpu_diverged||HasPendingImageCommit(begin,end-begin)){
            // A real store changes CPU ownership even when its value matches
            // the snapshot. Retiring an image watch must not erase this fact.
            auto a=begin-m.base,b=end-m.base;auto next=m.cpu_upload_ranges.lower_bound(a);
            if(next!=m.cpu_upload_ranges.begin()&&std::prev(next)->second>=a)--next;
            while(next!=m.cpu_upload_ranges.end()&&next->first<=b){
                a=std::min(a,next->first);b=std::max(b,next->second);next=m.cpu_upload_ranges.erase(next);}
            m.cpu_upload_ranges.emplace(a,b);
            ++m.cpu_upload_generation;
        }
        if(m.cpu_watch_pending){forget();return;}
        if(!m.cpu_watched&&!m.cpu_partial_proof)return;
        // A dirty marker describes contents, not current OS permissions.
        // A shared image/buffer proof can rearm this page after the first
        // invalidation. A real CPU store must retire that current RO guard too.
        if(!release_cpu_pages_||!release_cpu_pages_(begin,end-begin)){
            forget();return;}
        auto a=begin-m.base,b=end-m.base;auto next=m.cpu_dirty_ranges.lower_bound(a);
        if(next!=m.cpu_dirty_ranges.begin()&&std::prev(next)->second>=a)--next;
        while(next!=m.cpu_dirty_ranges.end()&&next->first<=b){
            a=std::min(a,next->first);b=std::max(b,next->second);next=m.cpu_dirty_ranges.erase(next);}
        m.cpu_dirty_ranges.emplace(a,b);m.cpu_watched=false;m.cpu_partial_proof=true;
        Diagnostics::Count("buffer-watch.cpu-page-invalidations");
        Diagnostics::Count("buffer-watch.cpu-page-invalidated.bytes",end-begin);
    };
    const auto pages=GuestProtectionPages(address,size);
    const auto dirty_begin=pages?pages->address:address;
    const auto dirty_end=pages?pages->address+pages->size:address+size;
    for(auto it=FirstOverlapping(dirty_begin);it!=mappings_.end()&&it->first<dirty_end;++it)invalidate(it->second);
    if(preparing_buffer_envelope_&&preparing_buffer_envelope_->base<dirty_end&&
       dirty_begin<preparing_buffer_envelope_->base+preparing_buffer_envelope_->size)invalidate(*preparing_buffer_envelope_);
}
void Rasterizer::InstallCpuWatch(Mapping& mapping){
    if(mapping.cpu_watched||!watch_buffer_||mapping.cpu_snapshot.size()!=mapping.size)return;
    const auto& watch=mapping.cpu_partial_proof&&rewatch_buffer_?rewatch_buffer_:watch_buffer_;
    if(!watch(mapping.base,mapping.size))return;
    // Protection is active BEFORE verifying the proof. A store in the gap
    // between the previous snapshot and Track must not become a watch hit.
    Diagnostics::Count("buffer-watch.establish-compare.bytes",mapping.size);
    if(memcmp(mapping.cpu_snapshot.data(),reinterpret_cast<const void*>(mapping.base),size_t(mapping.size))==0)
        {mapping.cpu_watched=true;mapping.cpu_partial_proof=false;mapping.cpu_dirty_ranges.clear();}
    else{release_buffer_(mapping.base,mapping.size);mapping.cpu_partial_proof=false;mapping.cpu_dirty_ranges.clear();
        Diagnostics::Count("buffer-watch.establish-changed");}
}
void Rasterizer::ResolveGpuBuffers(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!size)return;
    if(address>UINT64_MAX-size)throw std::out_of_range("GPU buffer resolution range overflow");
    // Find/DropCached keep mappings disjoint. Use the same predecessor rule as
    // DropCached: a request can begin inside one earlier mapping, but unrelated
    // resident buffers must not be scanned on every guest backing validation.
    auto first=mappings_.lower_bound(address);
    if(first!=mappings_.begin()){
        const auto previous=std::prev(first);
        if(previous->second.size>address-previous->first)first=previous;
    }
    std::vector<VAddr> requested;
    bool needs_download=false;
    SCOPE_EXIT {
        for(const auto base:requested){resolving_buffer_ranges_.erase(base);resolving_buffers_.erase(base);}
    };
    for(auto it=first;it!=mappings_.end()&&it->first<address+size;++it){
        auto& mapping=it->second;
        if(!mapping.gpu_owned||resolving_buffers_.contains(it->first))continue;
        const auto wanted=IsGdsRange(mapping.base,mapping.size)?
            std::optional<GuestProtectionRange>{GuestProtectionRange{address,size}}:
            GuestProtectionPages(address,size);
        if(!wanted)throw std::invalid_argument("Invalid GPU buffer resolution pages");
        const auto begin=std::max(mapping.base,wanted->address);
        const auto end=std::min(mapping.base+mapping.size,wanted->address+wanted->size);
        if(begin>=end)continue;
        std::map<u64,u64> owned;
        for(const auto& [owned_begin,owned_end]:mapping.gpu_owned_ranges){
            const auto selected_begin=std::max(begin-mapping.base,owned_begin);
            const auto selected_end=std::min(end-mapping.base,owned_end);
            if(selected_begin<selected_end)RememberSpan(owned,selected_begin,selected_end);
        }
        if(owned.empty())continue;
        requested.push_back(it->first);resolving_buffers_.insert(it->first);
        resolving_buffer_ranges_.emplace(it->first,owned);
        ConsumeFrameReadback(mapping,owned);
        for(const auto& [selected_begin,selected_end]:owned)
            for(const auto& [dirty_begin,dirty_end]:mapping.dirty_ranges)
                needs_download|=selected_begin<dirty_end&&dirty_begin<selected_end;
    }
    // Finish also writes CPU-visible dirty mappings. Their virtual CheckBacking
    // calls return here while an unrelated owned mapping is still resolving.
    // Only this request's new GPU-owned overlaps may start a reconciliation;
    // the outer set is not evidence that this CPU range needs another Finish.
    // Retain outer ownership, and unwind only our additions even on failure.
    if(requested.empty())return;
    // Strong guards cover native envelopes, while GPU writes are tracked per
    // sharp/page. Like original MemoryTracker clean-page invalidation, retiring
    // a clean guard needs no download or GPU submission. Physical CPU stores
    // remain separate from the live native resource until the ordered upload.
    if(needs_download)Finish();
    // Downloaded pages become a read-only CPU equality proof. Other protected
    // pages in the same native arena remain GPU-owned, matching Vulkan's
    // page-granular RegionManager instead of draining the whole allocation.
    for(const auto base:requested){
        auto found=mappings_.find(base);if(found==mappings_.end())continue;
        auto& mapping=found->second;
        const auto ranges=resolving_buffer_ranges_.at(base);
        for(const auto& [begin,end]:ranges){
            if(!IsGdsRange(base,mapping.size)){
                const auto protected_address=base+begin,protected_size=end-begin;
                if(transfer_buffer_watch_&&transfer_buffer_watch_(protected_address,protected_size,true)){
                    mapping.cpu_partial_proof=true;
                    Diagnostics::Count("buffer-watch.partially-downgraded.bytes",protected_size);
                }else if(release_buffer_){
                    release_buffer_(protected_address,protected_size);
                    RememberSpan(mapping.cpu_dirty_ranges,begin,end);mapping.cpu_partial_proof=true;
                }
            }
            UnmarkGpuOwned(mapping,begin,end-begin);
        }
        if(!mapping.gpu_owned&&mapping.cpu_partial_proof&&mapping.cpu_dirty_ranges.empty()){
            mapping.cpu_watched=true;mapping.cpu_partial_proof=false;
            Diagnostics::Count("buffer-watch.downgraded");
        }
    }
}
void Rasterizer::MarkShaderBufferWrites(std::span<const ShaderBufferRequest> requests){
    std::lock_guard lock(execution_mutex_);
    bool needs_restore=false;
    // A prewrite image snapshot can Finish and downgrade reserved B to RO.
    // Reacquire its existing mapping before declaring any new shader writes;
    // no Find/reservation may replace resources already selected for this bind.
    for(const auto& request:requests)if(request.written){
        auto it=mappings_.upper_bound(request.address);
        if(it==mappings_.begin())throw std::logic_error("Shader write lost its prepared native buffer");--it;
        auto& mapping=it->second;
        if(request.address-mapping.base>mapping.size||request.size>mapping.size-(request.address-mapping.base))
            throw std::logic_error("Shader write lost its selected buffer envelope");
        (void)request.WriteRange();
        const auto pages=GuestProtectionPages(request.address,request.size);
        const bool page_range=pages&&pages->address>=mapping.base&&pages->address-mapping.base<=mapping.size&&
            pages->size<=mapping.size-(pages->address-mapping.base);
        const auto owned_offset=page_range?pages->address-mapping.base:request.address-mapping.base;
        const auto owned_size=page_range?pages->size:request.size;
        needs_restore|=bool(own_buffer_)&&!IsGdsRange(mapping.base,mapping.size)&&
            !OwnsGpuRange(mapping,owned_offset,owned_size);
    }
    if(needs_restore){
    std::set<VAddr> prepared;std::vector<Mapping*> mappings;
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Upload,"restore-prepared-buffer-writes");
    for(const auto& request:requests)if(request.written){
        auto it=mappings_.upper_bound(request.address);--it;
        if(prepared.insert(it->first).second)mappings.push_back(&it->second);
    }
    const auto previous_preparations=preparing_buffer_contents_.size();
    SCOPE_EXIT {preparing_buffer_contents_.resize(previous_preparations);};
    for(auto* mapping:mappings)preparing_buffer_contents_.push_back(mapping);
    for(auto* entry:mappings){auto& mapping=*entry;
        if(!own_buffer_||IsGdsRange(mapping.base,mapping.size))continue;
        const bool uav=std::ranges::any_of(requests,[&](const auto& r){return r.unordered_access&&
            r.address>=mapping.base&&r.address-mapping.base<=mapping.size&&r.size<=mapping.size-(r.address-mapping.base);});
        PrepareBufferContents(mapping,true,uav,false,requests,std::span<const BufferWrite>{},diagnostic);
    }
    }
    for(const auto& request:requests)if(request.written){
        auto it=mappings_.upper_bound(request.address);--it;
        const auto write=request.WriteRange();
        MarkDirty(it->second,true,write.address-it->first,write.size);
    }
}
bool Rasterizer::BuffersMayDefer(std::span<const ShaderBufferRequest> requests){
    std::lock_guard lock(execution_mutex_);
    for(const auto& request:requests)if(request.written){
        auto it=mappings_.upper_bound(request.address);
        if(it==mappings_.begin()){Diagnostics::Count("buffer-defer.no-mapping");return false;}--it;
        if(request.address-it->first>it->second.size||
           request.size>it->second.size-(request.address-it->first)){
            Diagnostics::Count("buffer-defer.not-covered");return false;
        }
        const auto pages=GuestProtectionPages(request.address,request.size);
        const bool page_range=pages&&pages->address>=it->first&&pages->address-it->first<=it->second.size&&
            pages->size<=it->second.size-(pages->address-it->first);
        const auto owned_offset=page_range?pages->address-it->first:request.address-it->first;
        const auto owned_size=page_range?pages->size:request.size;
        const auto owned=OwnsGpuRange(it->second,owned_offset,owned_size);
        if(!owned){
            Diagnostics::Count("buffer-defer.not-owned");
            fprintf(stderr,"DX12 buffer ownership rejected: sharp=%llx+%llu mapping=%llx+%llu watched=%u dirty=%u\n",
                (unsigned long long)request.address,(unsigned long long)request.size,
                (unsigned long long)it->first,(unsigned long long)it->second.size,
                unsigned(it->second.cpu_watched),unsigned(it->second.dirty));fflush(stderr);
            return false;
        }
    }
    Diagnostics::Count("buffer-defer.accepted");
    return true;
}
void Rasterizer::Upload(Mapping& range,u64 offset,u64 size,const void* data){
    InvalidateFrameReadback(range);
    Diagnostics::Count("buffer-upload.calls");Diagnostics::Count("buffer-upload.bytes",size);
    churn_.Record(range.base,size,range.base,range.size,range.allocation_id,false,range.dirty,range.gpu_owned,scheduler_.Completed(),nullptr,size);
    const auto exceptions=std::uncaught_exceptions();
    SCOPE_EXIT {if(std::uncaught_exceptions()>exceptions)range.cpu_snapshot.clear();};
    const auto snapshot=[&](const void* source)->const void*{
        if(range.cpu_snapshot.empty()&&!offset&&size==range.size)ResizeSnapshot(range);
        if(range.cpu_snapshot.size()!=range.size)return source;
        // Snapshot the exact upload version in ordinary cached host memory.
        // Never read back a write-combined D3D12 UPLOAD heap on the CPU.
        if(source==range.cpu_snapshot.data()+offset)return source;
        memcpy(range.cpu_snapshot.WriteData()+offset,source,size_t(size));
        return range.cpu_snapshot.data()+offset;
    };
    const auto copy_to_upload=[&](ID3D12Resource* resource,void* destination){
        // Only ordinary cached, immutable witnesses are safe for workers.
        // Guest pointers and short-lived caller buffers retain the synchronous
        // path. The lease survives replacement, CPU stores and snapshot pools.
        if(size>=64*1024&&range.cpu_snapshot.size()==range.size&&
           data==range.cpu_snapshot.data()+offset){
            scheduler_.QueueHostCopy(resource,destination,range.cpu_snapshot.Share(),size_t(offset),size_t(size));
        }else memcpy(destination,data,size_t(size));
    };
    // A newly placed DEFAULT buffer starts in COMMON. Copy queues may
    // implicitly promote it to COPY_DEST and it decays back to COMMON when the
    // copy submission completes. The direct queue waits on the copy timeline,
    // so later shader work can transition it without a CPU fence wait.
    const bool copy_queue_eligible=range.gpu.state==D3D12_RESOURCE_STATE_COMMON&&
        range.gpu.subresource_states.empty();
    if(size<=4*1024*1024){
        // Resolve protected guest backing before choosing a stream slice:
        // a page-fault resolver may Finish(), which resets upload pages.
        const auto source=reinterpret_cast<VAddr>(data);
        if(IsMapped(source,size))PrepareCpuUpload(source,size);
        data=snapshot(data);
        Begin();
        constexpr u64 page_size=4*1024*1024;
        const auto reserved=(size+255)&~u64(255);
        if(upload_page_cursor_%page_size+reserved>page_size)
            upload_page_cursor_=(upload_page_cursor_+page_size-1)&~(page_size-1);
        const auto page=size_t(upload_page_cursor_/page_size);
        if(page==upload_pages_.size()){Diagnostics::Count("buffer-staging.new");upload_pages_.push_back(std::make_unique<UploadArena>(device_.Get(),page_size));}
        else Diagnostics::Count("buffer-staging.reuse");
        auto slice=upload_pages_[page]->Allocate(reserved);
        upload_page_cursor_+=reserved;
        copy_to_upload(slice.resource,slice.cpu);
        if(copy_queue_eligible){
            scheduler_.SubmitCopy([&](ID3D12GraphicsCommandList* copy){
                DX12_API_CALL(copy->CopyBufferRegion(range.gpu.handle.Get(),offset,
                    slice.resource,slice.offset,size));
            });
            return;
        }
        range.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(range.gpu.handle.Get(),offset,slice.resource,slice.offset,size));
        return;
    }
    // Resolve guest page ownership on the caller: workers must never fault
    // while the caller holds ExecutionMutex and waits for their copies.
    const auto source=reinterpret_cast<VAddr>(data);
    if(IsMapped(source,size))PrepareCpuUpload(source,size);
    data=snapshot(data);
    if(memory_perf_.enabled&&size<=memory_perf_.cpu_pool/4){
        const auto source=reinterpret_cast<VAddr>(data);
        if(IsMapped(source,size))PrepareCpuUpload(source,size);
        auto best=large_upload_pool_.end();
        for(auto it=large_upload_pool_.begin();it!=large_upload_pool_.end();++it)
            if(!it->used&&it->capacity>=size&&(best==large_upload_pool_.end()||it->capacity<best->capacity))best=it;
        if(best==large_upload_pool_.end()&&!memory_pressure_&&size<=memory_perf_.cpu_pool/4-large_upload_pool_bytes_){
            auto arena=std::make_unique<UploadArena>(device_.Get(),size);
            large_upload_pool_.push_back({std::move(arena),size,false});large_upload_pool_bytes_+=size;
            best=std::prev(large_upload_pool_.end());Diagnostics::Count("buffer-staging.large-new");
        }else if(best!=large_upload_pool_.end()){
            Diagnostics::Count("buffer-staging.large-reuse");++memory_reuse_.staging;
        }
        if(best!=large_upload_pool_.end()){
            data=snapshot(data);auto slice=best->arena->Allocate(size);best->used=true;
            copy_to_upload(slice.resource,slice.cpu);
            if(copy_queue_eligible)scheduler_.SubmitCopy([&](ID3D12GraphicsCommandList* copy){
                DX12_API_CALL(copy->CopyBufferRegion(range.gpu.handle.Get(),offset,slice.resource,slice.offset,size));});
            else{range.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
                DX12_API_CALL(RecordingList()->CopyBufferRegion(range.gpu.handle.Get(),offset,slice.resource,slice.offset,size));}
            return;
        }
    }
    Diagnostics::Count("buffer-staging.large-new");
    Begin();auto upload=Buffer(device_.Get(),size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    void* dest=nullptr;D3D12_RANGE empty{0,0};Check(DX12_API_CALL(upload.handle->Map(0,&empty,&dest)));
    bool deferred{};
    SCOPE_EXIT {if(!deferred)DX12_API_CALL(upload.handle->Unmap(0,nullptr));};
    if(range.cpu_snapshot.size()==range.size&&data==range.cpu_snapshot.data()+offset){
        // The worker retains the mapped resource and unmaps only after its
        // last write. Submit waits this same epoch before executing the copy.
        scheduler_.QueueHostCopy(upload.handle.Get(),dest,range.cpu_snapshot.Share(),size_t(offset),size_t(size),true);
        deferred=true;
    }else{
        const unsigned jobs=std::min(14u,std::max(1u,std::thread::hardware_concurrency()/2));
        std::array<unsigned,14> indices{};std::iota(indices.begin(),indices.end(),0u);
        const auto copy=[&](unsigned job){const auto first=size*job/jobs,last=size*(job+1)/jobs;
            memcpy(static_cast<std::byte*>(dest)+first,static_cast<const std::byte*>(data)+first,size_t(last-first));};
        if(jobs==1)copy(0);else std::for_each(std::execution::par,indices.begin(),indices.begin()+jobs,copy);
    }
    // Retain before recording/submitting. Growing the retention vector can
    // throw; after submission that must never release the copy source early.
    uploads_.push_back(upload);
    auto* upload_resource=uploads_.back().handle.Get();
    if(copy_queue_eligible){
        scheduler_.SubmitCopy([&](ID3D12GraphicsCommandList* copy){
            DX12_API_CALL(copy->CopyBufferRegion(range.gpu.handle.Get(),offset,upload_resource,0,size));
        });
        return;
    }
    range.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(range.gpu.handle.Get(),offset,upload_resource,0,size));
}
void Rasterizer::PrepareBufferContents(Mapping& mapping,bool written,bool uav,bool fresh,
    std::span<const ShaderBufferRequest> requests,std::optional<std::span<const BufferWrite>> known_writes,
    Diagnostics::Scope& diagnostic){
    preparing_buffer_contents_.push_back(&mapping);
    SCOPE_EXIT {preparing_buffer_contents_.pop_back();};
    const auto base=mapping.base;
    diagnostic.Stage("backing-validation");
    diagnostic.Detail("address=%llx bytes=%llu fresh=%u written=%u",(unsigned long long)base,
        (unsigned long long)mapping.size,unsigned(fresh),unsigned(written));
    ValidateGpuBacking(base,mapping.size,written);
    // Ordinary readable watches and strong B ownership already exclude CPU
    // writes. Do not walk PageWatch or count their existing wins as borrowed
    // image-proof hits. This path is only for otherwise-unproved physical B.
    auto backing_epoch=protected_backing_epoch_&&!mapping.cpu_watched&&!mapping.gpu_owned&&
        HasGpuImageOverlap(base,mapping.size)?protected_backing_epoch_(base,mapping.size):0;
    bool borrowed_proof=backing_epoch&&backing_epoch==mapping.protected_backing_epoch&&
        mapping.cpu_snapshot.size()==mapping.size&&mapping.cpu_upload_ranges.empty();
    mapping.protected_backing_epoch=0; // Failed/reentrant preparation cannot publish an unfinished proof.
    bool cpu_proven_unchanged=mapping.cpu_watched||borrowed_proof;
    const auto retain_partial_cpu_watch=[&]{
        if(!((base|mapping.size)&4095))return false;
        // A rounded readonly witness cannot be upgraded as one logical B
        // owner. Retain its RO gaps; the exact writable page ranges below
        // acquire strong ownership individually. Releasing the envelope here
        // would also discard retained image/neighbour witnesses.
        mapping.cpu_watched=false;mapping.cpu_partial_proof=true;
        Diagnostics::Count("buffer-watch.partial-upgrade");return true;
    };
    if(written&&mapping.cpu_watched){
        if(transfer_buffer_watch_&&transfer_buffer_watch_(base,mapping.size,false)){
            mapping.cpu_watched=false;MarkGpuOwned(mapping,0,mapping.size);
            Diagnostics::Count("buffer-watch.upgraded");
        }else if(!retain_partial_cpu_watch()){
            // No proof survives a release of protection. Use the snapshot
            // fallback after a rejected or unavailable atomic transfer.
            ReleaseCpuBufferWatches(base,mapping.size);cpu_proven_unchanged=false;
            Diagnostics::Count("buffer-watch.transfer-fallback");
        }
    }
    const auto* cpu=reinterpret_cast<const std::byte*>(base);
    // For a read-only binding, establish protection before comparing or
    // uploading. CPU writers then fault until this execution lock is
    // released, so a second whole-buffer comparison is unnecessary.
    // Pending protection is deliberately separate from a published proof:
    // reentrant alias reconciliation must be able to invalidate it, and
    // neither a nested binding nor an exception may reuse unfinished data.
    const auto& compare_watch=mapping.cpu_partial_proof&&rewatch_buffer_?rewatch_buffer_:watch_buffer_;
    const bool protected_compare=!mapping.gpu_owned&&!mapping.cpu_watched&&
        compare_watch&&compare_watch(base,mapping.size);
    // Guard acquisition can reenter the memory owner. A physical publication
    // during that callback invalidates this command's hit, not just future hits.
    if(borrowed_proof&&protected_backing_epoch_(base,mapping.size)!=backing_epoch){
        borrowed_proof=false;cpu_proven_unchanged=mapping.cpu_watched;
    }
    if(borrowed_proof)Diagnostics::Count("buffer-snapshot.protected-backing-hit.bytes",mapping.size);
    if(protected_compare)mapping.cpu_watch_pending=true;
    bool proof_published=false;
    SCOPE_EXIT {
        if(protected_compare&&!proof_published&&(mapping.cpu_watch_pending||mapping.cpu_watched)){
            ReleaseCpuBufferWatches(base,mapping.size);
        }
    };
    std::vector<std::byte> physical_cpu;
    if(HasGpuImageOverlap(base,mapping.size)&&(!mapping.gpu_owned||!mapping.cpu_upload_ranges.empty())&&
       (!mapping.cpu_upload_ranges.empty()||(!mapping.cpu_watched&&!cpu_proven_unchanged))){
        // A host buffer envelope may include an unrelated exclusive I outside
        // the original sharp. Reading its protected guest VA would fault and
        // resolve I into B. Original CopySparseMemory uploads physical B only;
        // take that same backing without changing I's class/history/ownership.
        physical_cpu.resize(size_t(mapping.size));
        for(;;){
            const auto before=protected_backing_epoch_?protected_backing_epoch_(base,mapping.size):0;
            if(!ReadDmaBacking(base,physical_cpu))
                throw std::runtime_error("Buffer envelope lost physical backing beside a native image");
            Diagnostics::Count("buffer-snapshot.physical-image-overlap.bytes",mapping.size);
            if(before&&protected_backing_epoch_(base,mapping.size)!=before)continue;
            backing_epoch=before;break;
        }
        cpu=physical_cpu.data();
    }
    diagnostic.Stage("snapshot-compare");
    if(!mapping.cpu_upload_ranges.empty()){
        // Original CPU dirty tracking uploads whole touched pages. The
        // GPU-only formatted version is not a CPU equality proof; a
        // same-value store must restore its CPU page without replacing
        // untouched GPU-only pages. Keep markers if an upload throws.
        while(!mapping.cpu_upload_ranges.empty()){
            const auto generation=mapping.cpu_upload_generation;
            const auto cpu_pages=mapping.cpu_upload_ranges;
            for(const auto& [begin,end]:cpu_pages){
                Upload(mapping,begin,end-begin,cpu+begin);
                Diagnostics::Count("buffer-snapshot.cpu-store-upload.bytes",end-begin);
            }
            // A physical-alias callback can report another actual CPU
            // store while Upload reads protected memory. Revisit the
            // marker set before publishing a prepared GPU view.
            if(generation==mapping.cpu_upload_generation)mapping.cpu_upload_ranges.clear();
        }
    }
    if(!mapping.gpu_owned&&!mapping.cpu_watched&&!cpu_proven_unchanged){
        if(protected_compare&&mapping.cpu_watch_pending&&mapping.cpu_partial_proof&&
           mapping.cpu_snapshot.size()==mapping.size){
            // Only holes lost their protection. Protect them again BEFORE
            // looking at data, then compare/upload those pages alone.
            const auto dirty_pages=mapping.cpu_dirty_ranges;
            for(const auto& [begin,end]:dirty_pages){
                Diagnostics::Count("buffer-snapshot.bytes",end-begin);
                if(memcmp(mapping.cpu_snapshot.data()+begin,cpu+begin,size_t(end-begin)))
                    Upload(mapping,begin,end-begin,cpu+begin);
            }
            Diagnostics::Count("buffer-snapshot.partial-proof");
        }
        // Reentrant reconciliation can invalidate the partial proof while
        // Upload reads memory. In that case rescan the complete snapshot.
        if(!protected_compare||!mapping.cpu_watch_pending||!mapping.cpu_partial_proof||
           mapping.cpu_snapshot.size()!=mapping.size){
        Diagnostics::Count("buffer-snapshot.bytes",mapping.size);
        const bool unchanged=mapping.cpu_snapshot.size()==mapping.size&&
            memcmp(mapping.cpu_snapshot.data(),cpu,size_t(mapping.size))==0;
        Diagnostics::Count(unchanged?"buffer-snapshot.unchanged":"buffer-snapshot.changed");
        if(!unchanged){
            Diagnostics::Count("buffer-content.changed");
            diagnostic.Stage("snapshot-upload");
            if(!fresh&&mapping.cpu_snapshot.size()==mapping.size){
                constexpr u64 block=4096;
                for(u64 offset=0;offset<mapping.size;){
                    const auto bytes=std::min(block,mapping.size-offset);
                    if(memcmp(mapping.cpu_snapshot.data()+offset,cpu+offset,size_t(bytes))==0){offset+=bytes;continue;}
                    const auto first=offset;offset+=bytes;
                    while(offset<mapping.size){
                        const auto next=std::min(block,mapping.size-offset);
                        if(memcmp(mapping.cpu_snapshot.data()+offset,cpu+offset,size_t(next))==0)break;
                        offset+=next;
                    }
                    Upload(mapping,first,offset-first,cpu+first);
                }
            }else{
                Upload(mapping,0,mapping.size,cpu);
            }
        }
        }
    }else if(cpu_proven_unchanged)Diagnostics::Count("buffer-snapshot.watch-hit");
    if(protected_compare&&mapping.cpu_watch_pending&&mapping.cpu_snapshot.size()==mapping.size){
        mapping.cpu_watch_pending=false;mapping.cpu_watched=true;
        mapping.cpu_partial_proof=false;mapping.cpu_dirty_ranges.clear();
        Diagnostics::Count("buffer-watch.protected-compare");
        Diagnostics::Count("buffer-watch.establish-compare-avoided.bytes",mapping.size);
    }
    // A read-only use of a preceding GPU result must retain dirty ownership.
    diagnostic.Stage("ownership-and-transition");
    if(written&&mapping.cpu_watched&&!mapping.gpu_owned){
        if(transfer_buffer_watch_&&transfer_buffer_watch_(base,mapping.size,false)){
            mapping.cpu_watched=false;MarkGpuOwned(mapping,0,mapping.size);Diagnostics::Count("buffer-watch.upgraded");
        }else if(!retain_partial_cpu_watch()){
            ReleaseCpuBufferWatches(base,mapping.size);Diagnostics::Count("buffer-watch.transfer-fallback");
        }
    }
    if(mapping.cpu_watch_pending)ReleaseCpuBufferWatches(base,mapping.size);
    if(written){
        for(const auto& request:requests)if(request.written&&request.address>=mapping.base&&
            request.address-mapping.base<=mapping.size&&request.size<=mapping.size-(request.address-mapping.base)){
            const auto pages=GuestProtectionPages(request.address,request.size);
            const bool page_range=pages&&pages->address>=mapping.base&&pages->address-mapping.base<=mapping.size&&
                pages->size<=mapping.size-(pages->address-mapping.base);
            const auto owned_address=page_range?pages->address:mapping.base;
            const auto owned_size=page_range?pages->size:mapping.size;
            const auto offset=owned_address-mapping.base;
            if(!OwnsGpuRange(mapping,offset,owned_size)&&own_buffer_){
                const bool owned=own_buffer_(owned_address,owned_size);
                if(owned)MarkGpuOwned(mapping,offset,owned_size);
                Diagnostics::Count(owned?"buffer-own.success":"buffer-own.failure");
            }
        }
        if(known_writes){
            for(const auto& write:*known_writes)if(write.address>=mapping.base&&write.address+write.size<=mapping.base+mapping.size)
                MarkDirty(mapping,true,write.address-mapping.base,write.size);
        }else for(const auto& request:requests)if(request.written&&request.address>=mapping.base&&
            request.address+request.size<=mapping.base+mapping.size){const auto write=request.WriteRange();
            MarkDirty(mapping,true,write.address-mapping.base,write.size);}}
    else if(!mapping.gpu_owned)InstallCpuWatch(mapping);
    mapping.gpu.Transition(RecordingList(),uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if(backing_epoch&&!mapping.gpu_owned&&!mapping.cpu_watched&&mapping.cpu_snapshot.size()==mapping.size&&
       mapping.cpu_upload_ranges.empty()&&protected_backing_epoch_(base,mapping.size)==backing_epoch)
        mapping.protected_backing_epoch=backing_epoch;
    proof_published=true;
}
bool Rasterizer::RematerializeBufferEnvelope(VAddr address,u64 size,bool physical_dma,bool defer_owned){
    const auto end=address+size;
    auto first=FirstOverlapping(address);
    if(first==mappings_.end()||first->first>=end)return false;
    bool clipped=false,diverged=false;
    for(auto it=first;it!=mappings_.end()&&it->first<end;++it){
        if(resolving_buffers_.contains(it->first)||
           std::ranges::find(preparing_buffer_contents_,&it->second)!=preparing_buffer_contents_.end())
            throw std::logic_error("Cannot replace a buffer envelope while its backing is resolving");
        clipped|=it->first<address||it->second.size>end-it->first;
        diverged|=it->second.gpu_cpu_diverged;
    }
    if(clipped){
        if(diverged)throw std::runtime_error("Buffer access-class split needs GPU-preserving physical residency");
        return false;
    }
    ValidateGpuBacking(address,size,false);
    if(!physical_dma)PrepareGpuBacking(address,size,false);
    // Pinned arena growth copies GPU-owned contents on the pending timeline;
    // it does not make actual guest writes CPU-visible just to grow storage.
    // Unprotected/failed sources retain the ordinary reconciliation boundary.
    first=FirstOverlapping(address);
    for(auto it=first;it!=mappings_.end()&&it->first<end;++it)
        defer_owned&=it->second.gpu_owned&&!it->second.initial_upload_failed&&
            it->second.cpu_snapshot.size()==it->second.size&&it->second.cpu_upload_ranges.empty();
    if(!defer_owned){ResolveGpuBuffers(address,size);Finish();}
    first=FirstOverlapping(address);
    if(first==mappings_.end()||first->first>=end)return false;
    std::vector<Mapping*> sources;
    std::vector<std::byte> backing(static_cast<size_t>(size));
    if(!ReadDmaBacking(address,backing)){
        if(physical_dma)throw std::runtime_error("DMA buffer growth lost physical backing");
        for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<end;++it)
            if(it->second.gpu_cpu_diverged)
                throw std::runtime_error("GPU-only buffer growth lost readable physical backing");
        return false; // ordinary CPU-equal host temporaries retain their old path
    }
    first=FirstOverlapping(address);
    for(auto it=first;it!=mappings_.end()&&it->first<end;++it){
        auto& source=it->second;
        if(source.base<address||source.size>end-source.base||
           (!defer_owned&&(source.gpu_owned||source.dirty)))
            throw std::logic_error("Buffer envelope changed during backing reconciliation");
        if(defer_owned&&(!source.gpu_owned||!source.cpu_upload_ranges.empty()))
            return RematerializeBufferEnvelope(address,size,physical_dma,false);
        sources.push_back(&source);
    }
    if(sources.empty())return false;
    std::map<VAddr,Mapping> replacement;
    auto& target=replacement.emplace(address,Mapping{address,size,AcquireBuffer((size+3)&~u64(3))}).first->second;
    target.backing_version=std::make_shared<const BufferBackingVersion>(address,size,target.gpu.handle.Get());
    target.allocation_id=acquired_allocation_id_;target.last_use=++use_clock_;
    target.cpu_snapshot=backing;
    std::map<VAddr,VAddr> proven;
    bool guarded=false;u64 replaced_bytes{};
    auto retained=std::make_shared<std::vector<Resource>>();retained->reserve(sources.size()+1);
    retained->push_back(target.gpu);
    std::set<const BufferBackingVersion*> inherited;
    for(const auto* source:sources){
        replaced_bytes+=source->size;retained->push_back(source->gpu);
        if(source->initial_upload_failed)continue;
        if(!source->backing_version)throw std::logic_error("Buffer growth source lost its backing generation");
        if(inherited.insert(source->backing_version.get()).second)
            target.preserved_versions.push_back(source->backing_version);
        // Weak ancestry expires with the last outstanding prepared view. It
        // neither retains old GPU allocations nor grows forever across frames.
        for(const auto& weak:source->preserved_versions)if(auto version=weak.lock();version&&inherited.insert(version.get()).second)
            target.preserved_versions.push_back(version);
        if(source->cpu_snapshot.size()!=source->size)
            throw std::logic_error("Buffer growth source has no complete CPU snapshot");
        const auto offset=source->base-address;
        memcpy(target.cpu_snapshot.WriteData()+offset,source->cpu_snapshot.data(),size_t(source->size));
        target.gpu_cpu_diverged|=source->gpu_cpu_diverged;
        if(defer_owned)for(const auto& [begin,stop]:source->dirty_ranges)
            RememberSpan(target.dirty_ranges,offset+begin,offset+stop);
        guarded|=source->cpu_watched||source->cpu_watch_pending||source->cpu_partial_proof;
        if(source->cpu_watched)RememberSpan(proven,offset,offset+source->size);
        else if(source->cpu_partial_proof){
            u64 cursor{};
            for(const auto& [begin,stop]:source->cpu_dirty_ranges){
                if(cursor<begin)RememberSpan(proven,offset+cursor,offset+begin);cursor=stop;
            }
            if(cursor<source->size)RememberSpan(proven,offset+cursor,offset+source->size);
        }
        // Dirty CPU tracking is page-granular in the pinned buffer domain.
        // Joining mappings must not clip a forced store to the old tiny view.
        for(const auto& [begin,stop]:source->cpu_upload_ranges){
            const auto page_begin=std::max(address,(source->base+begin)&~VAddr(4095));
            const auto last=source->base+stop;
            const auto page_end=std::min(end,last+std::min((-last)&u64(4095),end-last));
            RememberSpan(target.cpu_upload_ranges,page_begin-address,page_end-address);
        }
    }
    if(!target.cpu_upload_ranges.empty())target.cpu_upload_generation=1;
    u64 cursor{};
    for(const auto& [begin,stop]:proven){
        if(cursor<begin)RememberSpan(target.cpu_dirty_ranges,cursor,begin);cursor=stop;
    }
    if(cursor<size)RememberSpan(target.cpu_dirty_ranges,cursor,size);
    std::map<VAddr,VAddr> index;index.emplace(address,end);
    target.dirty=!target.dirty_ranges.empty();
    std::set<VAddr> dirty_index;if(target.dirty)dirty_index.insert(address);
    std::vector<std::pair<VAddr,u64>> acquired;
    acquired.reserve(sources.size()+1);
    bool published=false;
    const auto release_acquired=[&]{for(const auto& [base,bytes]:acquired)release_buffer_(base,bytes);acquired.clear();};
    SCOPE_EXIT {if(!published)release_acquired();};
    preparing_buffer_contents_.reserve(preparing_buffer_contents_.size()+sources.size());
    // Allocate callback storage and index/map nodes before any new protection.
    // Old and new placed storage survive copies, exceptional exits and pooling.
    RetainUntilFinish([retained](u64){(void)retained;});
    cursor=address;u64 copied_bytes{};
    for(auto* source:sources){
        if(source->initial_upload_failed)continue;
        if(cursor<source->base)Upload(target,cursor-address,source->base-cursor,backing.data()+cursor-address);
        const auto state=source->gpu.state;
        source->gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        target.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(RecordingList()->CopyBufferRegion(target.gpu.handle.Get(),source->base-address,
            source->gpu.handle.Get(),0,source->size));
        copied_bytes+=source->size;
        source->gpu.Transition(RecordingList(),state);
        cursor=source->base+source->size;
    }
    if(cursor<end)Upload(target,cursor-address,end-cursor,backing.data()+cursor-address);
    // Borrow existing source guards without opening an RW window. Owned
    // sources keep their guards; acquire only disjoint physical gaps. Prepare
    // all storage/index nodes before callbacks can change OS protection.
    target.cpu_partial_proof=guarded;
    bool protected_all{};
    {
        auto* previous=std::exchange(preparing_buffer_envelope_,&target);
        const auto depth=preparing_buffer_contents_.size();
        preparing_buffer_contents_.insert(preparing_buffer_contents_.end(),sources.begin(),sources.end());
        SCOPE_EXIT {preparing_buffer_contents_.resize(depth);preparing_buffer_envelope_=previous;};
        if(defer_owned){
            bool owned=true;auto next=address;
            const auto acquire=[&](VAddr begin,VAddr stop){
                if(begin==stop||IsGdsRange(begin,stop-begin))return true;
                if(!own_buffer_||!own_buffer_(begin,stop-begin))return false;
                acquired.emplace_back(begin,stop-begin);return true;
            };
            for(auto* source:sources){
                if(owned&&next<source->base)owned=acquire(next,source->base);
                next=source->base+source->size;
            }
            if(owned&&next<end)owned=acquire(next,end);
            // A backing callback can reconcile a preceding source. Never
            // publish stale write ownership after such a reentrant operation.
            for(auto* source:sources)owned&=OwnsGpuRange(*source,0,source->size)&&source->cpu_upload_ranges.empty();
            owned&=target.cpu_upload_ranges.empty();
            if(owned)MarkGpuOwned(target,0,target.size);
        }else protected_all=watch_buffer_&&watch_buffer_(address,size);
    }
    if(defer_owned&&!OwnsGpuRange(target,0,target.size)){
        release_acquired();
        return RematerializeBufferEnvelope(address,size,physical_dma,false);
    }
    target.cpu_partial_proof|=protected_all;
    if(!target.cpu_partial_proof)target.cpu_dirty_ranges.clear();
    // No allocation or owner callback remains between guard transfer and
    // publication. CPU faults acquire ExecutionMutex and observe the new map.
    {
        std::lock_guard index_lock(memory_index_mutex_);
        for(auto* source:sources)indexed_buffers_.erase(source->base);
        indexed_buffers_.insert(index.extract(address));
    }
    for(auto* source:sources)dirty_mappings_.erase(source->base);
    if(target.dirty)dirty_mappings_.insert(dirty_index.extract(address));
    auto it=FirstOverlapping(address);
    while(it!=mappings_.end()&&it->first<end)it=mappings_.erase(it);
    auto inserted=mappings_.insert(replacement.extract(address));
    published=true;
    cached_buffer_bytes_=cached_buffer_bytes_-replaced_bytes+size;
    Diagnostics::Count("buffer-envelope.gpu-preserved.bytes",copied_bytes);
    if(defer_owned)Diagnostics::Count("buffer-envelope.owned-deferred.bytes",copied_bytes);
    churn_.Record(address,size,address,size,inserted.position->second.allocation_id,true,false,false,scheduler_.Completed());
    return true;
}
std::vector<Rasterizer::ShaderBufferView> Rasterizer::PrepareShaderBuffers(std::span<const ShaderBufferRequest> requests,
    std::optional<std::span<const BufferWrite>> known_writes) {
    return PrepareShaderBuffersImpl(requests,known_writes,false);
}
std::vector<Rasterizer::ShaderBufferView> Rasterizer::PreparePhysicalDmaBuffers(std::span<const ShaderBufferRequest> requests){
    for(const auto& request:requests)if(request.written||!request.unordered_access||!request.resident||request.publish_dma_residency)
        throw std::invalid_argument("Physical DMA preparation requires nonpublishing readonly resident ranges");
    return PrepareShaderBuffersImpl(requests,std::span<const BufferWrite>{},true);
}
std::vector<Rasterizer::ShaderBufferView> Rasterizer::PrepareShaderBuffersImpl(std::span<const ShaderBufferRequest> requests,
    std::optional<std::span<const BufferWrite>> known_writes,bool physical_dma) {
    std::lock_guard streaming_lock(execution_mutex_);
    if(preparing_buffer_envelope_)
        throw std::logic_error("Cannot prepare resource bindings during buffer guard transfer");
    for(const auto& request:requests)(void)request.WriteRange();
    if(known_writes)for(const auto& write:*known_writes){
        if(!write.size||write.address>UINT64_MAX-write.size||
           !std::any_of(requests.begin(),requests.end(),[&](const auto& r){
               return r.written&&write.address>=r.address&&write.address-r.address<=r.size&&
                   write.size<=r.size-(write.address-r.address);}))
            throw std::invalid_argument("Known write is outside writable resource envelope");
    }
    if(stream_buffers_enabled_){
        std::vector<bool> streamed(requests.size());std::vector<ShaderBufferRequest> resident;
        u64 stream_bytes{};
        for(size_t i=0;i<requests.size();++i){const auto& request=requests[i];
            bool eligible=!request.resident&&request.address&&request.size&&request.size<=16384&&!request.written&&!request.unordered_access&&
                request.address<=UINT64_MAX-request.size&&request.address%4==0&&request.size%4==0&&IsMapped(request.address,request.size);
            if(eligible)for(const auto& other:requests)if((other.written||other.resident)&&
                request.address<other.address+other.size&&other.address<request.address+request.size){eligible=false;break;}
            if(eligible&&HasGpuOwnedOverlap(request.address,request.size))eligible=false;
            streamed[i]=eligible;if(eligible)stream_bytes+=(request.size+255)&~u64(255);else resident.push_back(request);
        }
        if(stream_bytes&&stream_bytes<=1024*1024){
            std::shared_ptr<UploadArena> page;
            for(const auto& candidate:shader_stream_pages_)if(candidate.use_count()==1){page=candidate;break;}
            if(!page&&shader_stream_pages_.size()<256){page=std::make_shared<UploadArena>(device_.Get(),1024*1024);shader_stream_pages_.push_back(page);}
            if(page){
                // Retire all required CPU-visible dependencies BEFORE allocating
                // stream slices. A view lease pins pages across attachment Finish
                // and the later draw/dispatch, including asynchronous submission.
                struct StreamingGuard{bool& enabled;StreamingGuard(bool& e):enabled(e){enabled=false;}~StreamingGuard(){enabled=true;}} guard(stream_buffers_enabled_);
                auto remaining=resident.empty()?std::vector<ShaderBufferView>{}:PrepareShaderBuffers(resident,known_writes);
                for(size_t i=0;i<requests.size();++i)if(streamed[i]){
                    CheckBacking(requests[i].address,requests[i].size,false);
                    PrepareCpuUpload(requests[i].address,requests[i].size);}
                page->Reset(scheduler_.Completed());
                std::vector<ShaderBufferView> result;result.reserve(requests.size());size_t index{};
                for(size_t i=0;i<requests.size();++i){
                    if(!streamed[i]){result.push_back(std::move(remaining[index++]));continue;}
                    auto slice=page->Allocate((requests[i].size+255)&~u64(255));
                    memcpy(slice.cpu,reinterpret_cast<const void*>(requests[i].address),size_t(requests[i].size));
                    result.push_back({slice.resource,slice.offset,requests[i].size,page});
                }
                return result;
            }
        }
    }
    Diagnostics::Scope diagnostic(Diagnostics::Kind::BufferViews,"validate-ranges");
    std::lock_guard lock(execution_mutex_);
    RefreshBufferBudget();
    bool has_srv=false,has_uav=false,preserve_version=false,valid=true;
    for(const auto& request:requests)(request.unordered_access?has_uav:has_srv)=true;
    if(has_srv&&has_uav)for(const auto& request:requests){
        valid&=request.address&&request.size&&request.address<=UINT64_MAX-request.size&&
            !(request.address%4)&&!(request.size%4)&&IsMapped(request.address,request.size)&&
            (!request.written||request.unordered_access);
    }
    if(valid&&has_srv&&has_uav){
        for(const auto& request:requests){
            for(auto it=FirstOverlapping(request.address);it!=mappings_.end()&&it->first<request.address+request.size;++it)
                preserve_version|=PreserveBufferBacking(it->second)||it->second.gpu_owned;
            if(!request.unordered_access)for(const auto& other:requests)if(other.unordered_access&&
                request.address<other.address+other.size&&other.address<request.address+request.size)
                throw std::runtime_error("Overlapping guest SRV/UAV buffer aliases need compiler access unification");
        }
        if(preserve_version){
            // These logical bytes do not overlap, but their native arena can.
            // Keep the complete B version/owner and copy only SRV reads to
            // private GPU storage. Writes still update the canonical arena.
            auto canonical=std::vector<ShaderBufferRequest>(requests.begin(),requests.end());
            for(auto& request:canonical)request.unordered_access=true;
            auto result=PrepareShaderBuffersImpl(canonical,known_writes,physical_dma);
            for(size_t i=0;i<requests.size();++i)if(!requests[i].unordered_access)
                result[i]=SnapshotShaderBufferRead(requests[i],result[i]);
            return result;
        }
    }
    std::vector<std::pair<VAddr,VAddr>> ranges;
    for(const auto& request:requests) {
        if(!request.address||!request.size||request.address>UINT64_MAX-request.size||
           request.address%4||request.size%4||!IsMapped(request.address,request.size)||
           (request.written&&!request.unordered_access))
            throw std::invalid_argument("Invalid native shader guest buffer range: address="+std::to_string(request.address)+
                " bytes="+std::to_string(request.size)+" mapped="+std::to_string(IsMapped(request.address,request.size))+
                " written="+std::to_string(request.written)+" uav="+std::to_string(request.unordered_access));
        ValidateGpuBacking(request.address,request.size,request.written);
        VAddr first=request.address,last=request.address+request.size;
        // Arena rounding is a materialization policy, not a binding contract.
        // An exact writable envelope can already cover a later read-only sharp
        // while rounding that sharp would extend outside it. Keep its backing,
        // ownership and equality proof; the batch-wide SRV/UAV check below still
        // splits allocations with incompatible simultaneous native states.
        auto covering=mappings_.upper_bound(first);
        const bool resident=covering!=mappings_.begin()&&
            (--covering)->second.size>=first-covering->first&&
            last-first<=covering->second.size-(first-covering->first)&&
            !covering->second.initial_upload_failed;
        // Stable 64KiB residency windows for large read-only PS4 suballocations.
        // Small offset/length changes must not destroy the entire backing buffer.
        // Expand only into Core-confirmed committed readable memory, and never
        // across another binding's incompatible SRV/UAV or writable range.
        if(buffer_arenas_enabled_&&!resident&&!request.written&&request.size>=65536&&last<=UINT64_MAX-65535){
            const VAddr arena_first=first&~VAddr(65535),arena_last=(last+65535)&~VAddr(65535);
            bool allowed=arena_last-arena_first<=Shader::NativeDma::MaxBufferBytes&&
                IsMapped(arena_first,arena_last-arena_first)&&CoversSpan(committed_readable_,arena_first,arena_last);
            if(allowed)for(const auto& other:requests)if(other.address<arena_last&&arena_first<other.address+other.size&&
                (other.written||other.unordered_access!=request.unordered_access)){allowed=false;break;}
            if(allowed&&HasGpuOwnedOverlap(arena_first,arena_last-arena_first))allowed=false;
            if(allowed){first=arena_first;last=arena_last;}

            // A PS4 allocation is frequently rebound through slightly shifted
            // sharps.  Replacing the old native slice with the exact new span
            // makes two partially overlapping read-only views evict one another
            // forever.  Grow once to their union so subsequent views are
            // covered by one stable resource.  GPU-owned storage and a batch
            // containing incompatible access classes retain the conservative
            // reconciliation path below.
            bool grew=true;
            while(grew){
                grew=false;
                auto overlap=mappings_.lower_bound(first);
                if(overlap!=mappings_.begin())--overlap;
                for(;overlap!=mappings_.end()&&overlap->first<last;++overlap){
                    const auto mapping_end=overlap->first+overlap->second.size;
                    if(mapping_end<=first||overlap->second.gpu_owned||overlap->second.dirty||
                       overlap->second.initial_upload_failed)continue;
                    const auto union_first=std::min(first,overlap->first);
                    const auto union_last=std::max(last,mapping_end);
                    if(union_last-union_first>Shader::NativeDma::MaxBufferBytes||
                       !IsMapped(union_first,union_last-union_first)||
                       !CoversSpan(committed_readable_,union_first,union_last))continue;
                    bool compatible=true;
                    for(const auto& other:requests)if(other.address<union_last&&
                        union_first<other.address+other.size&&
                        (other.written||other.unordered_access!=request.unordered_access)){
                        compatible=false;break;
                    }
                    if(!compatible)continue;
                    if(union_first!=first||union_last!=last){
                        first=union_first;last=union_last;grew=true;break;
                    }
                }
            }
        }
        // PageWatch ownership requires the complete host page envelope before
        // native descriptors are selected. The logical sharp/write is unchanged.
        if(request.written&&own_buffer_){
            const auto pages=GuestProtectionPages(request.address,request.size);
            bool allowed=pages&&pages->size<=Shader::NativeDma::MaxBufferBytes&&
                IsMapped(pages->address,pages->size)&&
                CoversSpan(committed_readable_,pages->address,pages->address+pages->size)&&
                CoversSpan(committed_writable_,pages->address,pages->address+pages->size);
            if(allowed)for(const auto& other:requests)
                if(other.address<pages->address+pages->size&&pages->address<other.address+other.size&&
                   other.unordered_access!=request.unordered_access){allowed=false;break;}
            if(allowed){first=std::min(first,pages->address);last=std::max(last,pages->address+pages->size);}
        }        ranges.emplace_back(first,last);
    }
    std::sort(ranges.begin(),ranges.end());std::vector<std::pair<VAddr,VAddr>> merged;
    for(auto range:ranges) {
        if(!merged.empty()&&range.first<merged.back().second)merged.back().second=std::max(merged.back().second,range.second);
        else merged.push_back(range);
    }
    // Keep the complete old B allocation, including tails absent from this
    // batch. Compatible requests may grow to its union; mixed native SRV/UAV
    // callers still require separate resources and must not inherit that union.
    ranges.clear();
    for(auto [first,last]:merged){
        auto begin=first,end=last;bool grew=true;
        while(grew){
            grew=false;
            const auto aligned=begin&~VAddr(15);
            if(aligned!=begin&&IsMapped(aligned,end-aligned)){begin=aligned;grew=true;}
            for(auto it=FirstOverlapping(begin);it!=mappings_.end()&&it->first<end;++it){
                const auto a=std::min(begin,it->first),b=std::max(end,it->first+it->second.size);
                if(a!=begin||b!=end){begin=a;end=b;grew=true;}
            }
        }
        bool srv=false,uav=false;
        for(const auto& request:requests)if(request.address<end&&begin<request.address+request.size)
            (request.unordered_access?uav:srv)=true;
        if(!(srv&&uav)&&end-begin<=Shader::NativeDma::MaxBufferBytes&&IsMapped(begin,end-begin)){
            first=begin;last=end;
        }
        ranges.emplace_back(first,last);
    }
    std::sort(ranges.begin(),ranges.end());merged.clear();
    for(const auto range:ranges){
        if(!merged.empty()&&range.first<merged.back().second)merged.back().second=std::max(merged.back().second,range.second);
        else merged.push_back(range);
    }
    u64 total=0;for(auto [first,last]:merged){auto size=last-first;
        if(size>UINT64_MAX-total)throw std::out_of_range("Native shader buffer batch size overflow");total+=size;}
    // A shrinking OS target cannot evict resources used by this very command.
    // Trim inactive mappings first, while preserving the complete active batch.
    // The placed allocator still enforces the physical arena growth budget.
    auto cache_budget=std::max(buffer_cache_budget_,total);
    for(auto [first,last]:merged) {
        bool srv=false,uav=false;for(const auto& request:requests)if(request.address>=first&&request.address+request.size<=last)
            (request.unordered_access?uav:srv)=true;
        if(srv&&uav)throw std::runtime_error("Overlapping guest SRV/UAV buffer aliases need compiler access unification");
    }
    // The next submission has not been recorded yet. Retire the preceding
    // dirty ranges before recreating slices, so aliases share one GPU resource.
    diagnostic.Stage("retire-previous-buffers");
    for(const auto& request:requests){
        auto cached=mappings_.upper_bound(request.address);
        const bool protected_resident=cached!=mappings_.begin()&&(--cached)->second.size>=request.address-cached->first&&
            request.size<=cached->second.size-(request.address-cached->first)&&
            (cached->second.gpu_owned||cached->second.cpu_watched);
        if(!physical_dma&&!protected_resident)PrepareGpuBacking(request.address,request.size,request.written);
    }
    // Bound retained constants/descriptors while allowing a useful packet of
    // read-only passes to reach the GPU together instead of waiting per draw.
    const bool cpu_visible_dirty=std::any_of(dirty_mappings_.begin(),dirty_mappings_.end(),
        [&](VAddr base){return !mappings_.at(base).gpu_owned;});
    if(cpu_visible_dirty||read_only_batches_>=32){Diagnostics::Count(cpu_visible_dirty?"buffer-finish.cpu-visible":"buffer-finish.batch-limit");Finish();}
    ++read_only_batches_;
    // Plan every replacement before retaining any final batch mappings.
    // Sources remain alive through budget admission and the preserving copy.
    diagnostic.Stage("retire-overlaps");
    std::set<VAddr> rebuild;
    for(auto [first,last]:merged){
        auto found=mappings_.upper_bound(first);bool covered=false;
        if(found!=mappings_.begin()){--found;covered=first-found->first<=found->second.size&&
            last-first<=found->second.size-(first-found->first);}
        if(covered){
            bool srv=false,uav=false;
            for(const auto& request:requests)if(request.address>=found->first&&
                request.address+request.size<=found->first+found->second.size)
                (request.unordered_access?uav:srv)=true;
            // Disjoint logical SRV/UAV slices must not inherit a previously
            // larger allocation with incompatible simultaneous native states.
            if(srv&&uav)covered=false;
        }
        if(!covered)rebuild.insert(first);
    }
    // Reserve only genuinely new backing. Counting already resident requests
    // twice evicted every large (>256 MiB) unchanged batch on every draw.
    u64 additional{},replaced_bytes{};std::set<VAddr> replaced;
    for(auto [first,last]:merged){
        if(!rebuild.contains(first))continue;
        additional+=last-first;
        for(auto it=FirstOverlapping(first);it!=mappings_.end()&&it->first<last;++it)
            if(replaced.insert(it->first).second)replaced_bytes+=it->second.size;
    }
    const auto savings=replaced_bytes>additional?replaced_bytes-additional:0;
    additional=additional>replaced_bytes?additional-replaced_bytes:0;
    cache_budget=AdmitBufferCache(merged,cache_budget,additional,savings);
    diagnostic.Stage("materialize-resident-backing");
    diagnostic.Detail("requests=%zu merged=%zu bytes=%llu additional=%llu cached=%llu",requests.size(),merged.size(),
        (unsigned long long)total,(unsigned long long)additional,(unsigned long long)CachedBufferBytes());
    std::vector<VAddr> created;
    std::set<VAddr> active_mappings;
    for(auto [first,last]:merged){
        auto found=mappings_.upper_bound(first);bool cached=false;
        if(found!=mappings_.begin()){--found;cached=first-found->first<=found->second.size&&last-first<=found->second.size-(first-found->first);}
        if(rebuild.contains(first)){
            if(RematerializeBufferEnvelope(first,last-first,physical_dma))cached=true;
            else{DropCached(first,last-first,true,"envelope");cached=false;}
        }
        auto& mapping=Find(first,last-first,false,cache_budget,physical_dma);if(!cached)created.push_back(mapping.base);
        active_mappings.insert(mapping.base);
    }
    for(const auto base:active_mappings) {
        auto& mapping=mappings_.at(base);
        bool active=false,written=false,uav=false;for(const auto& r:requests)if(r.address>=base&&r.address+r.size<=base+mapping.size){active=true;written|=r.written;uav|=r.unordered_access;}
        if(!active)continue;
        const bool fresh=std::find(created.begin(),created.end(),base)!=created.end();
        if(!physical_dma)PrepareBufferContents(mapping,written,uav,fresh,requests,known_writes,diagnostic);
    }
    diagnostic.Stage("emit-buffer-views");
    std::vector<ShaderBufferView> views;views.reserve(requests.size());
    for(const auto& request:requests) {
        auto& mapping=Find(request.address,request.size,false,cache_budget,physical_dma);
        views.push_back({mapping.gpu.handle,request.address-mapping.base,request.size,{},mapping.gpu.allocation,mapping.backing_version});
    }
    // DEFAULT storage alone is not residency: private streams and envelopes
    // reserve physical memory without executing an original EnsureResident.
    if(physical_dma)SynchronizePreparedDmaBuffers(requests,views);
    else DmaMemory().ObserveResidentSharps(requests);
    return views;
}
std::optional<Rasterizer::ShaderBufferView> Rasterizer::PreparedDmaBuffer(VAddr address,u64 size)const{
    std::lock_guard lock(execution_mutex_);
    if(!size||address>UINT64_MAX-size)return {};
    auto it=mappings_.upper_bound(address);
    if(it==mappings_.begin())return {};--it;
    const auto& mapping=it->second;
    if(mapping.initial_upload_failed||!mapping.gpu.handle||address-mapping.base>mapping.size||
       size>mapping.size-(address-mapping.base))return {};
    return ShaderBufferView{mapping.gpu.handle,address-mapping.base,size,{},mapping.gpu.allocation,mapping.backing_version};
}
Rasterizer::ShaderBufferView Rasterizer::SnapshotShaderBufferRead(const ShaderBufferRequest& request,const ShaderBufferView& source){
    if(request.written||request.unordered_access||source.lease||source.read_snapshot||!source.backing_version)
        throw std::logic_error("Invalid private GPU shader-read source");
    if(memory_pressure_)TrimPools();
    std::shared_ptr<Resource> copy;
    for(const auto& candidate:shader_read_copy_pool_)if(candidate.use_count()==1&&candidate->handle->GetDesc().Width>=request.size&&
        candidate->handle->GetDesc().Width-request.size<=std::max<u64>(65536,request.size/4)&&
        (!copy||candidate->handle->GetDesc().Width<copy->handle->GetDesc().Width))copy=candidate;
    if(!copy){
        copy=std::make_shared<Resource>(AcquireBuffer(request.size));
        const auto bytes=copy->handle->GetDesc().Width;
        if(shader_read_copy_pool_.size()<256&&bytes<=128*MemoryMiB-shader_read_copy_pool_bytes_){
            shader_read_copy_pool_.push_back(copy);shader_read_copy_pool_bytes_+=bytes;}
    }
    auto snapshot=std::make_shared<ShaderReadSnapshot>(ShaderReadSnapshot{copy,request.address,request.size,source});
    ShaderBufferView result{copy->handle,0,request.size,{},snapshot,source.backing_version,snapshot};
    return RefreshPreparedShaderBuffers(std::span(&request,1),std::span(&result,1)).front();
}
void Rasterizer::RefreshShaderReadSnapshot(const ShaderBufferRequest& request,const ShaderBufferView& view,Mapping& mapping){
    const auto& snapshot=view.read_snapshot;
    // Retain both placed allocations before recording, including a canonical
    // generation newer than the snapshot's original selected source.
    RetainUntilFinish([source=mapping.gpu,snapshot](u64){(void)source;(void)snapshot;});
    const auto previous=mapping.gpu.state;
    mapping.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    snapshot->gpu->Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(RecordingList()->CopyBufferRegion(snapshot->gpu->handle.Get(),0,mapping.gpu.handle.Get(),
        request.address-mapping.base,request.size));
    mapping.gpu.Transition(RecordingList(),previous);
    snapshot->gpu->Transition(RecordingList(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Diagnostics::Count("buffer-read.private-gpu-copy.bytes",request.size);
}
std::vector<Rasterizer::ShaderBufferView> Rasterizer::RefreshPreparedShaderBuffers(
    std::span<const ShaderBufferRequest> requests,std::span<const ShaderBufferView> selected){
    std::lock_guard lock(execution_mutex_);
    if(requests.size()!=selected.size())throw std::invalid_argument("Final buffer range/view count mismatch");
    std::vector<ShaderBufferView> result;result.reserve(requests.size());
    std::map<VAddr,bool> states;
    for(size_t i=0;i<requests.size();++i){
        const auto& request=requests[i];const auto& previous=selected[i];
        (void)request.WriteRange();
        if(!previous.resource||previous.size!=request.size||
           previous.offset>previous.resource->GetDesc().Width||
           previous.size>previous.resource->GetDesc().Width-previous.offset)
            throw std::logic_error("Invalid preceding prepared buffer view");
        if(previous.lease){
            if(request.written||request.unordered_access||previous.read_snapshot)
                throw std::logic_error("Immutable CPU stream cannot become writable");
            result.push_back(previous);continue;
        }
        if(const auto& snapshot=previous.read_snapshot;snapshot){
            if(request.written||request.unordered_access||request.address!=snapshot->address||request.size!=snapshot->size||
               previous.offset||!snapshot->gpu||previous.resource.Get()!=snapshot->gpu->handle.Get()||
               previous.allocation.get()!=snapshot.get()||snapshot->source.read_snapshot||snapshot->source.lease||
               previous.backing_version!=snapshot->source.backing_version)
                throw std::logic_error("Private GPU shader-read view lost its original identity");
        }
        const auto& origin=previous.read_snapshot?previous.read_snapshot->source:previous;
        const auto& version=origin.backing_version;
        if(!version||origin.resource.Get()!=version->resource||origin.size!=request.size||request.address<version->base||request.address-version->base!=origin.offset||
           origin.offset>version->size||request.size>version->size-origin.offset)
            throw std::logic_error("Prepared buffer view lost its original range generation");
        auto it=mappings_.upper_bound(request.address);
        if(it==mappings_.begin())throw std::logic_error("Final buffer backing was retired");--it;
        const auto& mapping=it->second;
        const bool preserved=mapping.backing_version==version||
            std::ranges::any_of(mapping.preserved_versions,[&](const auto& weak){return weak.lock()==version;});
        if(!preserved||mapping.initial_upload_failed||request.address-mapping.base>mapping.size||
           request.size>mapping.size-(request.address-mapping.base))
            throw std::logic_error("Final buffer backing was replaced without GPU-preserving continuity");
        if(previous.read_snapshot){RefreshShaderReadSnapshot(request,previous,it->second);result.push_back(previous);}
        else result.push_back({mapping.gpu.handle,request.address-mapping.base,request.size,{},mapping.gpu.allocation,mapping.backing_version});
        states[mapping.base]|=request.unordered_access||bool(previous.read_snapshot);
    }
    // Finish/retile/capture can leave an otherwise valid arena in COPY_SOURCE.
    // Preserve already prepared IA/indirect read bits on shared read-only B.
    // A UAV/IA conflict must still use the existing separate index snapshot.
    for(const auto& [base,uav]:states){auto& mapping=mappings_.at(base);
        const auto state=uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            (mapping.gpu.state&D3D12_RESOURCE_STATE_GENERIC_READ)|
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        mapping.gpu.Transition(RecordingList(),state);
    }
    return result;
}
void Rasterizer::SynchronizePreparedDmaBuffers(std::span<const ShaderBufferRequest> requests,
    std::span<const ShaderBufferView> views){
    std::lock_guard lock(execution_mutex_);
    if(requests.size()!=views.size())throw std::invalid_argument("DMA synchronization range/view count mismatch");
    std::map<VAddr,bool> active;
    for(size_t i=0;i<requests.size();++i){
        const auto& request=requests[i];const auto& view=views[i];
        if(view.lease)continue; // immutable CPU streams are not resident arenas
        if(!request.size||request.address>UINT64_MAX-request.size)
            throw std::invalid_argument("DMA synchronization range overflow");
        auto it=mappings_.upper_bound(request.address);
        if(it==mappings_.begin())throw std::logic_error("DMA synchronization lost a prepared mapping");--it;
        const auto& mapping=it->second;
        if(const auto& snapshot=view.read_snapshot;snapshot){
            const auto& source=snapshot->source;const auto& version=source.backing_version;
            const bool preserved=version&&(mapping.backing_version==version||
                std::ranges::any_of(mapping.preserved_versions,[&](const auto& weak){return weak.lock()==version;}));
            if(request.written||request.unordered_access||snapshot->address!=request.address||snapshot->size!=request.size||
               !snapshot->gpu||view.resource.Get()!=snapshot->gpu->handle.Get()||view.offset||view.size!=request.size||
               view.allocation.get()!=snapshot.get()||source.lease||source.read_snapshot||!preserved||mapping.initial_upload_failed||
               source.resource.Get()!=version->resource||source.size!=request.size||view.backing_version!=version||
               request.address<version->base||request.address-version->base!=source.offset||
               source.offset>version->size||request.size>version->size-source.offset||
               request.address-mapping.base>mapping.size||request.size>mapping.size-(request.address-mapping.base))
                throw std::logic_error("DMA synchronization lost its private shader-read source");
            active[mapping.base]=true;continue;
        }
        if(request.address-mapping.base>mapping.size||request.size>mapping.size-(request.address-mapping.base)||
           view.resource.Get()!=mapping.gpu.handle.Get()||view.offset!=request.address-mapping.base||view.size!=request.size)
            throw std::logic_error("DMA synchronization cannot replace an already bound resource");
        active[mapping.base]|=request.unordered_access;
    }
    // Pinned SynchronizeDmaBuffers runs after BindTextures and only uploads
    // CPU-dirty B. It does not format-sync I, declare writes or recreate arenas.
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Resources,"post-stage-dma-content");
    for(const auto& [base,uav]:active){
        auto& mapping=mappings_.at(base);
        if(!mapping.gpu_owned&&!mapping.cpu_watched&&HasGpuImageOverlap(base,mapping.size)){
            auto epoch=protected_backing_epoch_?protected_backing_epoch_(base,mapping.size):0;
            if(epoch&&epoch==mapping.protected_backing_epoch&&mapping.cpu_snapshot.size()==mapping.size&&
               mapping.cpu_upload_ranges.empty()){
                Diagnostics::Count("buffer-snapshot.protected-backing-hit.bytes",mapping.size);
                mapping.gpu.Transition(RecordingList(),uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                continue;
            }
            mapping.protected_backing_epoch=0;
            // Legacy opaque images can still own NOACCESS guest pages. Read
            // physical B without resolving/downloading that I through a CPU
            // fault. Stream comparisons in bounded chunks, retaining B1 when
            // physical CPU bytes match the original B0 snapshot.
            const bool unknown=mapping.cpu_snapshot.size()!=mapping.size;
            if(unknown)ResizeSnapshot(mapping);
            for(;;){
                epoch=protected_backing_epoch_?protected_backing_epoch_(base,mapping.size):0;
                const auto generation=mapping.cpu_upload_generation;
                const auto stores=mapping.cpu_upload_ranges;
                std::array<std::byte,4096> physical;
                for(u64 offset=0;offset<mapping.size;offset+=physical.size()){
                    const auto bytes=std::min<u64>(physical.size(),mapping.size-offset);
                    if(!ReadDmaBacking(base+offset,std::span(physical).first(size_t(bytes))))
                        throw std::runtime_error("DMA synchronization lost protected physical guest backing");
                    Diagnostics::Count("buffer-snapshot.bytes",bytes);
                    const bool stored=std::ranges::any_of(stores,[&](const auto& range){
                        return range.first<offset+bytes&&offset<range.second;});
                    if(unknown||stored||memcmp(mapping.cpu_snapshot.data()+offset,physical.data(),size_t(bytes)))
                        Upload(mapping,offset,bytes,physical.data());
                }
                if(generation==mapping.cpu_upload_generation&&
                   (!epoch||protected_backing_epoch_(base,mapping.size)==epoch)){
                    mapping.cpu_upload_ranges.clear();break;
                }
            }
            if(epoch&&protected_backing_epoch_(base,mapping.size)==epoch)
                mapping.protected_backing_epoch=epoch;
            mapping.gpu.Transition(RecordingList(),uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }else PrepareBufferContents(mapping,false,uav,false,{},std::span<const BufferWrite>{},diagnostic);
    }
    for(size_t i=0;i<requests.size();++i)if(views[i].read_snapshot)
        (void)RefreshPreparedShaderBuffers(requests.subspan(i,1),views.subspan(i,1));
}
Resource Rasterizer::CaptureShaderBuffer(VAddr address,u64 bytes,bool publish_dma_residency){
    std::lock_guard lock(execution_mutex_);
    ShaderBufferRequest request{address,bytes,false,true,true};
    request.publish_dma_residency=publish_dma_residency;
    auto views=PrepareShaderBuffers(std::span(&request,1));
    return CapturePreparedShaderBuffer(address,bytes);
}
std::vector<std::pair<VAddr,u64>> Rasterizer::GpuOwnedBufferProtectionRanges(VAddr address,u64 bytes)const{
    std::lock_guard lock(execution_mutex_);
    const auto pages=GuestProtectionPages(address,bytes);
    if(!pages)throw std::invalid_argument("Invalid GPU buffer protection query");
    std::vector<std::pair<VAddr,u64>> result;
    auto it=mappings_.upper_bound(pages->address);
    if(it!=mappings_.begin()&&std::prev(it)->first+std::prev(it)->second.size>pages->address)--it;
    for(;it!=mappings_.end()&&it->first<pages->address+pages->size;++it){
        const auto& mapping=it->second;
        if(!mapping.gpu_owned||IsGdsRange(mapping.base,mapping.size))continue;
        for(const auto& [begin,end]:mapping.gpu_owned_ranges){
            const auto first=mapping.base+begin,last=mapping.base+end;
            if(!result.empty()&&result.back().first+result.back().second==first)result.back().second=last-result.back().first;
            else result.emplace_back(first,last-first);
        }
    }
    return result;
}
bool Rasterizer::IsGpuBufferModified(VAddr address,u64 bytes)const{
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address>UINT64_MAX-bytes)throw std::invalid_argument("Invalid GPU buffer modification query");
    // Pinned RegionManager queries 4 KiB GPU-modified pages. Only source
    // selection uses those page bounds; exact download/write spans stay exact.
    const auto first=address&~u64(4095),last=(address+bytes-1)|u64(4095);
    for(const auto base:dirty_mappings_){const auto& mapping=mappings_.at(base);
        if(base>last||base+mapping.size<=first)continue;
        for(const auto& [begin,end]:mapping.dirty_ranges)
            if(base+begin<=last&&first<base+end)return true;
    }
    return false;
}
void Rasterizer::ValidatePreparedShaderBufferRange(VAddr address,u64 bytes){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address%4||bytes%4||address>UINT64_MAX-bytes)
        throw std::invalid_argument("Invalid prepared image source range");
    auto found=mappings_.upper_bound(address);
    if(found==mappings_.begin())throw std::logic_error("Planned image source was not prepared");--found;
    const auto& source=found->second;
    if(address-source.base>source.size||bytes>source.size-(address-source.base))
        throw std::logic_error("Late image binding exceeds the prepared buffer envelope");
}
Rasterizer::PreparedBufferBinding Rasterizer::BorrowPreparedShaderBuffer(VAddr address,u64 bytes){
    std::lock_guard lock(execution_mutex_);
    const auto view=PreparedDmaBuffer(address,bytes);
    if(!view||address%4||bytes%4)throw std::logic_error("Indirect input is not a prepared aligned B range");
    auto found=mappings_.upper_bound(address);--found;
    auto& mapping=found->second;
    if(view->resource.Get()!=mapping.gpu.handle.Get()||view->backing_version!=mapping.backing_version)
        throw std::logic_error("Prepared B backing version changed during selection");
    RetainUntilFinish([backing=mapping.gpu,version=mapping.backing_version](u64){});
    return {&mapping.gpu,view->offset,bytes,view->backing_version};
}
Resource Rasterizer::CapturePreparedShaderBuffer(VAddr address,u64 bytes){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address%4||bytes%4||bytes>UINT64_MAX-D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT+1)
        throw std::invalid_argument("Invalid native buffer snapshot range");
    auto found=mappings_.upper_bound(address);
    if(found==mappings_.begin())throw std::logic_error("Guest buffer was not prepared");--found;
    auto& source=found->second;
    if(address-source.base>source.size||bytes>source.size-(address-source.base))
        throw std::logic_error("Prepared guest buffer envelope was replaced");
    const auto capacity=(bytes+D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1)&
        ~u64(D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT-1);
    // AcquireBuffer reclaims only retired unpinned pool pages, then checks the
    // actual adapter budget. It neither changes prepared mappings nor Finish-es
    // the command list, so the authoritative source selected above stays valid.
    auto snapshot=AcquireBuffer(capacity);
    RetainUntilFinish([backing=source.gpu,snapshot](u64){(void)backing;(void)snapshot;});
    snapshot.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
    source.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    DX12_API_CALL(RecordingList()->CopyBufferRegion(snapshot.handle.Get(),0,source.gpu.handle.Get(),
        address-source.base,bytes));
    snapshot.Transition(RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    return snapshot;
}
void Rasterizer::TransitionPreparedShaderBuffer(VAddr address,u64 bytes,D3D12_RESOURCE_STATES state){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address>UINT64_MAX-bytes)throw std::invalid_argument("Invalid prepared buffer transition range");
    auto found=mappings_.upper_bound(address);
    if(found==mappings_.begin())throw std::logic_error("Transitioning an unprepared guest buffer");--found;
    auto& mapping=found->second;
    if(address-mapping.base>mapping.size||bytes>mapping.size-(address-mapping.base))
        throw std::logic_error("Prepared buffer transition lost its envelope");
    mapping.gpu.Transition(RecordingList(),state);
}
void Rasterizer::CopyToPreparedShaderBuffer(VAddr address,u64 bytes,Resource& source,D3D12_RESOURCE_STATES final_state,
    bool resident_only){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address>UINT64_MAX-bytes||address%4||bytes%4||!source.handle||
       source.handle->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||source.handle->GetDesc().Width<bytes)
        throw std::invalid_argument("Invalid formatted image/buffer transfer");
    auto found=mappings_.upper_bound(address);
    if(found==mappings_.begin())throw std::logic_error("Formatted guest buffer was not prepared");--found;
    auto& mapping=found->second;
    if(address-mapping.base>mapping.size||bytes>mapping.size-(address-mapping.base)||
       mapping.gpu.handle.Get()==source.handle.Get())
        throw std::logic_error("Prepared formatted buffer envelope was replaced");
    const auto spans=resident_only?DmaMemory().ResidentIntersections(address,bytes):
        std::vector<std::pair<VAddr,u64>>{{address,bytes}};
    if(!spans.empty()){
        InvalidateFrameReadback(mapping);
        RetainUntilFinish([backing=mapping.gpu,source](u64){(void)backing;(void)source;});
        source.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        mapping.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        for(const auto& [first,size]:spans)
            DX12_API_CALL(RecordingList()->CopyBufferRegion(mapping.gpu.handle.Get(),first-mapping.base,
                source.handle.Get(),first-address,size));
        mapping.gpu_cpu_diverged=true;
    }
    // This read-only cache synchronization does not mark new guest writes.
    // Existing buffer ownership/dirty ranges decide subsequent CPU visibility.
    mapping.gpu.Transition(RecordingList(),final_state);
}
void Rasterizer::FillPreparedShaderBuffer(VAddr address,u64 bytes,u32 value,D3D12_RESOURCE_STATES final_state){
    std::lock_guard lock(execution_mutex_);
    if(!bytes||address%4||bytes%4||bytes/4>UINT_MAX)
        throw std::invalid_argument("Invalid formatted metadata buffer fill");
    auto found=mappings_.upper_bound(address);
    if(found==mappings_.begin())throw std::logic_error("Metadata guest buffer was not prepared");--found;
    auto& mapping=found->second;
    if(address-mapping.base>mapping.size||bytes>mapping.size-(address-mapping.base))
        throw std::logic_error("Prepared metadata buffer envelope was replaced");
    InvalidateFrameReadback(mapping);
    struct FillLease{Resource backing;Descriptors visible,cpu;
        FillLease(ID3D12Device* device,Resource resource):backing(std::move(resource)),
            visible(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true),
            cpu(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,false){}};
    auto lease=std::make_shared<FillLease>(device_.Get(),mapping.gpu);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.Format=DXGI_FORMAT_R32_UINT;
    uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.FirstElement=(address-mapping.base)/4;
    uav.Buffer.NumElements=UINT(bytes/4);
    DX12_API_CALL(device_->CreateUnorderedAccessView(mapping.gpu.handle.Get(),nullptr,&uav,lease->visible.Cpu(0)));
    DX12_API_CALL(device_->CreateUnorderedAccessView(mapping.gpu.handle.Get(),nullptr,&uav,lease->cpu.Cpu(0)));
    RetainUntilFinish([lease](u64){(void)lease;});
    auto* list=RecordingList();mapping.gpu.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12DescriptorHeap* heaps[]{lease->visible.Heap()};DX12_API_CALL(list->SetDescriptorHeaps(1,heaps));
    const UINT values[]{value,value,value,value};
    DX12_API_CALL(list->ClearUnorderedAccessViewUint(lease->visible.Gpu(0),lease->cpu.Cpu(0),
        mapping.gpu.handle.Get(),values,0,nullptr));
    mapping.gpu_cpu_diverged=true;
    // Original readonly texel-buffer synthesis changes the cache arena only.
    // Preserve its existing dirty/CPU-visible ownership, including clean reads.
    mapping.gpu.UavBarrier(list);mapping.gpu.Transition(list,final_state);
}
Rasterizer::IndexBinding Rasterizer::ObtainIndexBuffer(VAddr address,UINT bytes,DXGI_FORMAT format){
    std::lock_guard lock(execution_mutex_);
    const UINT element=format==DXGI_FORMAT_R16_UINT?2:format==DXGI_FORMAT_R32_UINT?4:0;
    if(!element||!bytes||address%element||bytes%element||!IsMapped(address,bytes))
        throw std::invalid_argument("Invalid original guest index range");
    if(bytes>16384||IsGpuBufferModified(address,bytes)){
        auto result=PreparedIndexBuffer(address,bytes,format);
        const ShaderBufferRequest resident{address,bytes,false,true,true};
        DmaMemory().ObserveResidentSharps(std::span(&resident,1));return result;
    }
    // CopySparseMemory reads physical B0. Neither formatted B1 nor an owned I
    // may become the source of the original clean small index stream.
    constexpr u64 capacity=65536;const auto reserved=(u64(bytes)+255)&~u64(255);
    std::shared_ptr<UploadArena> page;
    for(auto& slot:index_stream_pages_){
        if(slot.arena.use_count()==1){slot.arena->Reset(scheduler_.Completed());slot.used=0;}
        if(slot.used<=capacity-reserved){slot.used+=reserved;page=slot.arena;break;}
    }
    if(!page){
        page=std::make_shared<UploadArena>(device_.Get(),capacity);
        if(index_stream_pages_.size()<256)index_stream_pages_.push_back({page,reserved});
    }
    const auto slice=page->Allocate(reserved);
    if(!ReadDmaBacking(address,std::span(static_cast<std::byte*>(slice.cpu),bytes)))
        throw std::runtime_error("Private index stream lost physical backing");
    Diagnostics::Count("index-buffer.private-cpu-stream");Diagnostics::Count("index-buffer.private-cpu-stream.bytes",bytes);
    return {&page->Backing(),slice.offset,bytes,format,page};
}
Rasterizer::IndexBinding Rasterizer::PreparedIndexBuffer(VAddr address,UINT bytes,DXGI_FORMAT format){
    std::lock_guard lock(execution_mutex_);
    const UINT element=format==DXGI_FORMAT_R16_UINT?2:format==DXGI_FORMAT_R32_UINT?4:0;
    if(!element||!bytes||address%element||bytes%element)throw std::invalid_argument("Invalid resident guest index range");
    auto it=mappings_.upper_bound(address);
    if(it==mappings_.begin())throw std::logic_error("Guest index buffer was not prepared");--it;
    if(address-it->first>it->second.size||bytes>it->second.size-(address-it->first))
        throw std::logic_error("Prepared guest index envelope was replaced");
    auto& source=it->second.gpu;const auto offset=address-it->first;
    Diagnostics::Count("index-buffer.cpu-copy-avoided.bytes",bytes);
    if(source.state!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS){
        Diagnostics::Count("index-buffer.resident-bind");return {&source,offset,bytes,format,source.allocation};}
    // D3D12 cannot combine UAV and IA reads. Snapshot the indices on the GPU
    // before this draw's shader writes, matching the preceding CPU snapshot
    // semantics without materializing GPU-owned guest memory on the CPU.
    std::shared_ptr<Resource> copy;
    for(const auto& candidate:index_copy_pool_)if(candidate.use_count()==1&&candidate->handle->GetDesc().Width>=bytes&&
        (!copy||candidate->handle->GetDesc().Width<copy->handle->GetDesc().Width))copy=candidate;
    if(!copy){const u64 capacity=(u64(bytes)+65535)&~u64(65535);
        copy=std::make_shared<Resource>(Buffer(device_.Get(),capacity,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST));
        if(capacity<=128*MemoryMiB-index_copy_pool_bytes_){index_copy_pool_.push_back(copy);index_copy_pool_bytes_+=capacity;}
        Diagnostics::Count("index-buffer.copy-allocation");
    }else Diagnostics::Count("index-buffer.copy-pool-reuse");
    auto* list=RecordingList();source.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    copy->Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
    DX12_API_CALL(list->CopyBufferRegion(copy->handle.Get(),0,source.handle.Get(),offset,bytes));
    source.Transition(list,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    copy->Transition(list,D3D12_RESOURCE_STATE_INDEX_BUFFER);
    Diagnostics::Count("index-buffer.uav-gpu-copy");Diagnostics::Count("index-buffer.uav-gpu-copy.bytes",bytes);
    return {copy.get(),0,bytes,format,copy};
}
void Rasterizer::ExecuteCompute(const ShaderPipeline& pipeline,ResourceBindings& bindings,u32 x,u32 y,u32 z,bool wait) {
    std::lock_guard lock(execution_mutex_);
    if(!pipeline.IsCompute()||!x||!y||!z||x>65535||y>65535||z>65535)
        throw std::invalid_argument("Invalid native compute pipeline or dispatch dimensions");
    pipeline.Bind(RecordingList(),bindings);const auto timestamp=scheduler_.BeginTimestamp("compute-dispatch");
    DX12_API_CALL(scheduler_.Begin()->Dispatch(x,y,z));scheduler_.EndTimestamp(timestamp,"compute-dispatch");
    // A global barrier over every historical dirty mapping grows with the
    // cache and serializes unrelated work.  Barrier only resources referenced
    // by this dispatch; later CPU access is synchronized by the ownership
    // resolver and its fence.
    for(const auto base:dirty_mappings_) {
        auto& resource=mappings_.at(base).gpu;
        if(bindings.References(resource.handle.Get()))resource.UavBarrier(scheduler_.Begin());
    }
    // Initial guest integration reconciles writes synchronously. The binding
    // table may safely leave scope once this call returns.
    if(wait){Finish();ValidateDebugMessages();}
}
void Rasterizer::RegisterMemory(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size)throw std::invalid_argument("invalid registered memory");
    CancelImageBackingCommits(address,size);ForgetBacking(address,size);
    // core/memory.cpp also calls this AFTER impl.Unmap. Never dereference here.
    // Notify backing-page change; stale cached data must not overwrite new pages.
    DropCached(address,size,false);
}
void Rasterizer::MapMemory(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);
    if(!a||!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid mapped range");auto end=a+s;
    CancelImageBackingCommits(a,s);ForgetBacking(a,s);
    auto it=mapped_ranges_.lower_bound(a);
    if(it!=mapped_ranges_.begin()){auto prev=std::prev(it);if(prev->second>=a)it=prev;}
    while(it!=mapped_ranges_.end()&&it->first<=end){a=std::min(a,it->first);end=std::max(end,it->second);it=mapped_ranges_.erase(it);}
    mapped_ranges_.emplace(a,end);
    {std::lock_guard lock(memory_index_mutex_);indexed_ranges_=mapped_ranges_;}
}
bool Rasterizer::IsMapped(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);
    if(!s||a>UINT64_MAX-s)return false;auto it=mapped_ranges_.upper_bound(a);if(it==mapped_ranges_.begin())return false;--it;return a+s<=it->second;
}
void Rasterizer::TraceMapping(Mapping& m,const char* reason){
    Diagnostics::Count("buffer-eviction.count");
    Diagnostics::Count(reason==std::string_view("budget")?"buffer-eviction.budget":reason==std::string_view("envelope")?"buffer-eviction.envelope":reason==std::string_view("failed-reuse")?"buffer-eviction.failed-reuse":reason==std::string_view("dma-global-clear")?"buffer-eviction.dma-global-clear":"buffer-eviction.overlap");
    churn_.Record(m.base,m.size,m.base,m.size,m.allocation_id,false,m.dirty,m.gpu_owned,scheduler_.Completed(),reason);
}
void Rasterizer::DropCached(VAddr a,u64 s,bool reconcile,const char* reason){
    auto first=mappings_.lower_bound(a);
    if(first!=mappings_.begin()){auto previous=std::prev(first);if(previous->first+previous->second.size>a)first=previous;}
    if(first==mappings_.end()||first->first>=a+s)return;
    for(auto it=first;it!=mappings_.end()&&it->first<a+s;++it)
        if(std::ranges::find(preparing_buffer_contents_,&it->second)!=preparing_buffer_contents_.end())
            throw std::logic_error("Cannot drop a buffer during active content preparation");
    ResolveGpuBuffers(a,s);
    if(reconcile)Finish();else{auto token=Flush();if(token)scheduler_.Wait(token);}
    while(first!=mappings_.end()&&first->first<a+s){TraceMapping(first->second,reason);cached_buffer_bytes_-=first->second.size;
        dirty_mappings_.erase(first->first);RemoveBufferIndex(first->first);RetireBuffer(first->second);first=mappings_.erase(first);}
}
void Rasterizer::UnmapMemory(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);
    if(!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid unmapped range");CancelImageBackingCommits(a,s);ForgetBacking(a,s);DropCached(a,s,true);auto end=a+s;
    auto it=mapped_ranges_.lower_bound(a);if(it!=mapped_ranges_.begin())--it;
    std::vector<std::pair<VAddr,VAddr>> survivors;
    while(it!=mapped_ranges_.end()&&it->first<end){auto begin=it->first,stop=it->second;if(stop<=a){++it;continue;}if(begin<a)survivors.emplace_back(begin,a);if(stop>end)survivors.emplace_back(end,stop);it=mapped_ranges_.erase(it);}
    for(const auto& [begin,stop]:survivors)mapped_ranges_.emplace(begin,stop);
    {std::lock_guard lock(memory_index_mutex_);indexed_ranges_=mapped_ranges_;}
}
void Rasterizer::FillBuffer(VAddr a,u32 s,u32 value,bool gds){
    std::lock_guard lock(execution_mutex_);
    if(a%4||s%4)throw std::invalid_argument("FillBuffer requires dword alignment");
    if(!s)return;
    if(!gds){
        if(a>UINT64_MAX-s||!IsMapped(a,s))throw std::out_of_range("Unmapped fill range");
        bool gpu_modified=false;
        for(const auto base:dirty_mappings_){const auto& mapping=mappings_.at(base);
            if(base<a+s&&a<base+mapping.size){gpu_modified=true;break;}}
        if(!gpu_modified){
            // Match Vulkan's CPU fill for memory without prior GPU writes.
            // Retire cached readers before evicting their native backing.
            CheckBacking(a,s,true);DropCached(a,s,false);
            std::fill_n(reinterpret_cast<u32*>(a),s/4,value);return;
        }
    }
    if(!gds){
        // Original ObtainBuffer(...,true) prepares writable arena storage.
        // Reuse the same preserving envelope/ownership path as shader writes;
        // a direct Find would evict or reject preceding formatted GPU bytes.
        const ShaderBufferRequest request{a,s,true,true,true};
        const BufferWrite write{a,s};
        PrepareShaderBuffers(std::span(&request,1),std::span(&write,1));
    }
    auto& r=Find(a,s,gds);
    auto real=gds?a+reinterpret_cast<VAddr>(gds_.data()):a;
    std::vector<u32> words(s/4,value);Upload(r,real-r.base,s,words.data());MarkDirty(r,true,real-r.base,s);
}
void Rasterizer::CopyBuffer(VAddr dst,VAddr src,u32 s,bool dg,bool sg){
    std::lock_guard lock(execution_mutex_);
    if(!s)return;
    if((dg&&(dst>gds_.size()||s>gds_.size()-dst))||(sg&&(src>gds_.size()||s>gds_.size()-src)))
        throw std::out_of_range("GDS copy range");
    auto da=dg?dst+reinterpret_cast<VAddr>(gds_.data()):dst;
    auto sa=sg?src+reinterpret_cast<VAddr>(gds_.data()):src;
    if(da>UINT64_MAX-s||sa>UINT64_MAX-s)throw std::out_of_range("copy address overflow");
    const auto gpu_modified=[&](VAddr address){
        for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<address+s;++it)
            if(it->second.dirty||it->second.gpu_owned)return true;
        return false;
    };
    if(!dg&&!sg&&!gpu_modified(da)&&!gpu_modified(sa)&&!HasGpuImageOverlap(sa,s)){
        ValidateGpuBacking(sa,s);ValidateGpuBacking(da,s,true);
        // Retain clean GPU readers and their old versions. A following binding
        // observes changed CPU bytes through the existing snapshot/watch path.
        PrepareGpuBacking(da,s,true);
        CheckBacking(sa,s,false);CheckBacking(da,s,true);
        InvalidateCpuBufferPages(da,s);
        std::memmove(reinterpret_cast<void*>(da),reinterpret_cast<const void*>(sa),s);
        Diagnostics::Count("cp-copy.cpu.bytes",s);
        return;
    }
    const BufferCopy copy{da,sa,s};CopyBufferBatchImpl(std::span(&copy,1),false);
}
void Rasterizer::CopyBufferBatch(std::span<const BufferCopy> copies) {
    CopyBufferBatchImpl(copies,true);
}
void Rasterizer::CopyBufferBatchImpl(std::span<const BufferCopy> copies,bool submit_when_deferred) {
    std::lock_guard lock(execution_mutex_);
    if(copies.empty())return;
    constexpr u64 coalesce_extent=64*1024*1024;
    bool destinations_may_defer=true;
    std::map<VAddr,VAddr> destinations;
    // Validate the whole dispatch before recording any destination writes.
    for(const auto& copy:copies){
        // D3D12 buffer copies are byte-addressed.  PS4 copy shaders routinely
        // use byte and half-word strides, so imposing a dword alignment here
        // rejects valid packets even though CopyBufferRegion supports them.
        if(!copy.bytes)
            throw std::invalid_argument("Native GPU copy region is empty");
        ValidateGpuBacking(copy.source,copy.bytes);ValidateGpuBacking(copy.destination,copy.bytes,true);
        // Preflight the minimum raw-view envelopes for EVERY packet too;
        // a late partial-page failure must not follow earlier GPU writes.
        if(copy.source+copy.bytes>UINT64_MAX-3||copy.destination+copy.bytes>UINT64_MAX-3)
            throw std::out_of_range("GPU copy envelope overflow");
        const auto source_begin=copy.source&~VAddr(3),destination_begin=copy.destination&~VAddr(3);
        ValidateGpuBacking(source_begin,((copy.source+copy.bytes+3)&~VAddr(3))-source_begin);
        ValidateGpuBacking(destination_begin,((copy.destination+copy.bytes+3)&~VAddr(3))-destination_begin,true);
        Diagnostics::Count("copy-requested.bytes",copy.bytes);
        RememberSpan(destinations,copy.destination,copy.destination+copy.bytes);
    }
    for(const auto& copy:copies)NotifyGpuBufferWrite(copy.destination,copy.bytes);
    for(const auto& [begin,end]:destinations)Diagnostics::Count("copy-destination.union-bytes",end-begin);
    for(size_t first=0;first<copies.size();) {
        const auto& initial=copies[first];
        VAddr src_begin=initial.source,src_end=initial.source+initial.bytes;
        VAddr dst_begin=initial.destination,dst_end=initial.destination+initial.bytes;
        size_t end=first+1;
        for(;end<copies.size()&&end-first<256;++end){
            const auto& c=copies[end];
            auto sb=std::min(src_begin,c.source),se=std::max(src_end,c.source+c.bytes);
            auto db=std::min(dst_begin,c.destination),de=std::max(dst_end,c.destination+c.bytes);
            if(se-sb>coalesce_extent||de-db>coalesce_extent)break;
            src_begin=sb;src_end=se;dst_begin=db;dst_end=de;
        }
        std::vector<ShaderBufferRequest> requests;
        bool coalesced=true;
        try{ValidateGpuBacking(src_begin,src_end-src_begin);ValidateGpuBacking(dst_begin,dst_end-dst_begin,true);}
        catch(const std::exception&){coalesced=false;}
        if(coalesced){
            // Like the original copy HLE, materialize bounded common operands,
            // not one committed GPU allocation for every tiny copy region.
            // PrepareShaderBuffers creates raw views and therefore requires a
            // dword envelope; CopyBufferRegion below retains the exact bytes.
            auto aligned_src_begin=src_begin&~VAddr(3),aligned_src_end=(src_end+3)&~VAddr(3);
            auto aligned_dst_begin=dst_begin&~VAddr(3),aligned_dst_end=(dst_end+3)&~VAddr(3);
            // Prefer complete guest pages when their backing is accessible.
            // PageWatch can then keep copy results GPU-owned until an actual
            // CPU access; exact byte ranges remain the fallback at PRT edges.
            if(src_end<=UINT64_MAX-4095&&dst_end<=UINT64_MAX-4095){
                const auto page_src_begin=src_begin&~VAddr(4095),page_src_end=(src_end+4095)&~VAddr(4095);
                const auto page_dst_begin=dst_begin&~VAddr(4095),page_dst_end=(dst_end+4095)&~VAddr(4095);
                try{
                    ValidateGpuBacking(page_src_begin,page_src_end-page_src_begin);
                    ValidateGpuBacking(page_dst_begin,page_dst_end-page_dst_begin,true);
                    aligned_src_begin=page_src_begin;aligned_src_end=page_src_end;
                    aligned_dst_begin=page_dst_begin;aligned_dst_end=page_dst_end;
                    Diagnostics::Count("copy-hle.page-envelope");
                }catch(const std::exception&){Diagnostics::Count("copy-hle.exact-envelope");}
            }
            requests.push_back({aligned_src_begin,aligned_src_end-aligned_src_begin,false,true});
            requests.push_back({aligned_dst_begin,aligned_dst_end-aligned_dst_begin,true,true});
        }else for(size_t i=first;i<end;++i){const auto& c=copies[i];
            // These are copy operands, not simultaneous shader SRV/UAV bindings.
            // Allow overlapping operands to materialize the same native storage.
            const auto source_begin=c.source&~VAddr(3),source_end=(c.source+c.bytes+3)&~VAddr(3);
            const auto destination_begin=c.destination&~VAddr(3),destination_end=(c.destination+c.bytes+3)&~VAddr(3);
            requests.push_back({source_begin,source_end-source_begin,false,true});
            requests.push_back({destination_begin,destination_end-destination_begin,true,true});
        }
        std::vector<BufferWrite> writes;writes.reserve(end-first);
        for(size_t i=first;i<end;++i)writes.push_back({copies[i].destination,copies[i].bytes});
        const auto views=PrepareShaderBuffers(requests,std::span<const BufferWrite>(writes));
        destinations_may_defer&=BuffersMayDefer(requests);
        u64 scratch_bytes{};
        for(size_t i=first;i<end;++i){const auto n=coalesced?0:(i-first)*2;
            if(views[n].resource.Get()==views[n+1].resource.Get())scratch_bytes=std::max<u64>(scratch_bytes,copies[i].bytes);}
        Resource scratch;
        if(scratch_bytes){
            scratch=Buffer(device_.Get(),scratch_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            uploads_.push_back(scratch);
        }
        for(size_t i=first;i<end;++i){const auto& c=copies[i];
            auto& source=Find(c.source,c.bytes,false);auto& destination=Find(c.destination,c.bytes,false);
            Begin();source.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            if(&source==&destination){
                // Legacy D3D12 states forbid reading and writing one resource
                // in the same copy. Scratch also preserves memmove semantics.
                scratch.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);
                DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(scratch.handle.Get(),0,source.gpu.handle.Get(),c.source-source.base,c.bytes));
                scratch.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);
                destination.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);
                DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(destination.gpu.handle.Get(),c.destination-destination.base,scratch.handle.Get(),0,c.bytes));
            }else{
                destination.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);
                DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(destination.gpu.handle.Get(),c.destination-destination.base,source.gpu.handle.Get(),c.source-source.base,c.bytes));
            }
            MarkDirty(destination,true,c.destination-destination.base,c.bytes);
        }
        first=end;
    }
    // Keep every packet in this HLE dispatch on one ordered GPU timeline.
    // Whole guarded destinations remain GPU-owned and are reconciled by their
    // first CPU access. Partial/unprotected destinations retain the eager
    // readback required for guest memory coherence.
    if(destinations_may_defer){Diagnostics::Count("copy-hle.deferred");if(submit_when_deferred)Flush();}
    else{Diagnostics::Count("copy-hle.reconciled");Finish();}
}
u64 Rasterizer::Flush(){
    std::lock_guard lock(execution_mutex_);
    if(recorded_marker_scopes_){auto* list=scheduler_.Begin();
        while(recorded_marker_scopes_){GpuMarkers::End(list);--recorded_marker_scopes_;}}
    // Publication and completed-retirement callbacks can submit internal GPU
    // transfers recursively. Detach this batch before invoking any of them:
    // a nested Flush must not replay its outer leases/publications, nor may
    // the outer Flush erase callbacks added for the next command list.
    auto retirements=std::move(deferred_retirements_);
    auto publications=std::move(submission_callbacks_);
    deferred_retirements_={};submission_callbacks_={};
    // Submit copies all leases before execution and latches any failure. If
    // it throws, executed work retains its scheduler-owned copies; an uncopied
    // batch was never executed and cannot be retried through the failure latch.
    return scheduler_.Submit(retirements,publications);
}
void Rasterizer::CommitImageBacking(VAddr address,std::span<const std::byte> bytes,
    const std::function<bool(VAddr,std::span<std::byte>)>& read,
    const std::function<bool(VAddr,std::span<const std::byte>)>& write){
    std::lock_guard lock(execution_mutex_);
    if(!address||bytes.empty()||address>UINT64_MAX-bytes.size()||!read||!write)
        throw std::invalid_argument("Invalid independent image backing commit");
    struct Snapshot {VAddr base;u64 allocation;std::vector<std::byte> bytes;};
    std::vector<Snapshot> snapshots;
    for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<address+bytes.size();++it){
        auto& mapping=it->second;
        if(mapping.cpu_snapshot.size()!=mapping.size){
            Snapshot snapshot{it->first,mapping.allocation_id,std::vector<std::byte>(size_t(mapping.size))};
            if(!read(mapping.base,snapshot.bytes))throw std::runtime_error("Image commit lost its CPU buffer baseline");
            snapshots.push_back(std::move(snapshot));
        }
    }
    if(!write(address,bytes))throw std::runtime_error("Cannot commit independent image through physical backing");
    for(auto& snapshot:snapshots){auto it=mappings_.find(snapshot.base);
        if(it!=mappings_.end()&&it->second.allocation_id==snapshot.allocation)
            it->second.cpu_snapshot=std::move(snapshot.bytes);
    }
    for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<address+bytes.size();++it){
        auto& mapping=it->second;
        const auto first=std::max(address,mapping.base),last=std::min(address+bytes.size(),mapping.base+mapping.size);
        if(last<=first)continue;
        InvalidateFrameReadback(mapping);
        if(mapping.cpu_snapshot.size()==mapping.size)
            memcpy(mapping.cpu_snapshot.WriteData()+first-mapping.base,bytes.data()+first-address,size_t(last-first));
        // This GPU image download is not a CPU store. Remember the new CPU
        // baseline without uploading it into the preceding buffer arena.
        // Actual CPU store markers/proof holes and GPU dirty ranges survive.
        mapping.gpu_cpu_diverged=true;
    }
    Diagnostics::Count("image-download.cpu-baseline.bytes",bytes.size());
}
void Rasterizer::DeferImageBacking(u64 fence,VAddr address,u64 bytes,u64 retained_bytes,
    std::function<bool(VAddr,std::span<std::byte>)> read,
    std::function<bool(VAddr,std::span<const std::byte>)> write,
    std::function<void(u64,std::span<std::byte>)> resolve){
    std::lock_guard lock(execution_mutex_);
    if(!fence||fence>scheduler_.Submitted()||!address||!bytes||address>UINT64_MAX-bytes||
       retained_bytes>UINT64_MAX-bytes||!read||!write||!resolve)
        throw std::invalid_argument("Invalid retired priority image backing commit");
    auto ticket=std::make_shared<PendingImageCommit>(PendingImageCommit{address,bytes});
    std::erase_if(pending_image_tickets_,[](const auto& weak){return weak.expired();});
    pending_image_tickets_.push_back(ticket);
    const auto key=std::pair{address,bytes};auto& count=pending_image_commits_[key];
    if(count==UINT32_MAX)throw std::overflow_error("Priority image commit count overflow");
    ++count;bool handed_off{};
    const auto release_pending=[this,key]{auto it=pending_image_commits_.find(key);
        if(it!=pending_image_commits_.end()&&!--it->second)pending_image_commits_.erase(it);};
    SCOPE_EXIT {if(!handed_off)release_pending();};
    auto commit=[this,address,bytes,read=std::move(read),write=std::move(write),
        resolve=std::move(resolve),release_pending,ticket=std::move(ticket)](u64 completed){
        std::lock_guard lock(execution_mutex_);SCOPE_EXIT {release_pending();};
        if(!ticket->backing_valid){Diagnostics::Count("image-download.priority-backing-replaced");return;}
        std::vector<std::byte> result(size_t(bytes),std::byte{});
        if(!read(address,result)){Diagnostics::Count("image-download.priority-unmapped");return;}
        resolve(completed,result);CommitImageBacking(address,result,read,write);
        Diagnostics::Count("image-download.priority-committed");
    };
    const auto cost=bytes+retained_bytes;constexpr u64 limit=64*1024*1024;
    if(cost>limit||priority_image_bytes_->load()>limit-cost){
        scheduler_.Wait(fence);handed_off=true;commit(fence);
        Diagnostics::Count("image-download.priority-budget-fallback");return;
    }
    auto counter=priority_image_bytes_;counter->fetch_add(cost);
    auto lease=std::shared_ptr<void>(counter.get(),[counter,cost](void*){counter->fetch_sub(cost);});
    scheduler_.Defer(fence,[commit=std::move(commit),lease=std::move(lease)](u64 completed){commit(completed);},true);
    handed_off=true;Diagnostics::Count("image-download.priority-queued");
}
void Rasterizer::CommitBufferReadback(Mapping& mapping,Resource& resource,
    std::span<const ReadbackSpan> ranges,bool owned){
    if(ranges.empty())return;
    u64 first=UINT64_MAX,last{};
    for(const auto& range:ranges){first=std::min(first,range.offset);
        last=std::max(last,range.offset+range.end-range.begin);}
    void* data{};const D3D12_RANGE all{SIZE_T(first),SIZE_T(last)};
    Check(DX12_API_CALL(resource.handle->Map(0,&all,&data)));
    SCOPE_EXIT {const D3D12_RANGE none{};DX12_API_CALL(resource.handle->Unmap(0,&none));};
    const bool gds=IsGdsRange(mapping.base,mapping.size);
    for(const auto& range:ranges){
        const auto* source=static_cast<const std::byte*>(data)+range.offset;
        if(owned&&!gds){
            if(!commit_buffer_(mapping.base+range.begin,std::span(source,size_t(range.end-range.begin))))
                throw std::runtime_error("Cannot commit GPU buffer through PS4 physical backing");
        }else{
            if(!owned)CheckBacking(mapping.base+range.begin,range.end-range.begin,true);
            memcpy(reinterpret_cast<void*>(mapping.base+range.begin),source,size_t(range.end-range.begin));
        }
        Diagnostics::Count("buffer-commit.bytes",range.end-range.begin);
    }
    // Preserve exact physical equality witnesses, forced CPU-store markers
    // and GPU-only formatted bytes outside the downloaded spans.
    if(mapping.cpu_snapshot.size()!=mapping.size){ResizeSnapshot(mapping);
        memcpy(mapping.cpu_snapshot.WriteData(),reinterpret_cast<const void*>(mapping.base),size_t(mapping.size));}
    for(const auto& range:ranges)memcpy(mapping.cpu_snapshot.WriteData()+range.begin,
        static_cast<const std::byte*>(data)+range.offset,size_t(range.end-range.begin));
    for(const auto& range:ranges)ForgetSpan(mapping.dirty_ranges,range.begin,range.end);
    mapping.dirty=!mapping.dirty_ranges.empty();
    if(!mapping.dirty){dirty_mappings_.erase(mapping.base);mapping.frame_readback.reset();}
    if(!owned)InstallCpuWatch(mapping);
}
void Rasterizer::ConsumeFrameReadback(Mapping& mapping,const std::map<u64,u64>& wanted){
    auto copy=mapping.frame_readback;
    if(!protected_backing_epoch_||!copy||!copy->fence||copy->fence>scheduler_.Completed()||
       copy->serial!=mapping.contents_serial||mapping.cpu_snapshot.size()!=mapping.size)return;
    std::vector<ReadbackSpan> selected;
    for(const auto& [dirty_begin,dirty_end]:mapping.dirty_ranges)
        for(const auto& [wanted_begin,wanted_end]:wanted){
            const auto begin=std::max(dirty_begin,wanted_begin),end=std::min(dirty_end,wanted_end);
            if(begin>=end)continue;
            const auto captured=std::find_if(copy->spans.begin(),copy->spans.end(),[&](const auto& span){
                return span.begin<=begin&&span.end>=end;});
            // Do not retire a partially captured request: the ordinary Finish
            // path must obtain every missing current byte, with its own wait.
            if(captured==copy->spans.end())return;
            if(protected_backing_epoch_&&(!captured->backing_epoch||captured->backing_epoch!=
                protected_backing_epoch_(mapping.base+captured->begin,captured->end-captured->begin)))return;
            selected.push_back({begin,end,captured->offset+begin-captured->begin});
        }
    if(selected.empty())return;
    CommitBufferReadback(mapping,copy->readback,selected,true);
    Diagnostics::Count("buffer-readback.frame-consumed");
}
void Rasterizer::OnFrame(){
    std::lock_guard lock(execution_mutex_);
    if(!protected_backing_epoch_)return;
    if(frame_readback_epoch_==UINT64_MAX)throw std::overflow_error("Frame readback epoch overflow");
    ++frame_readback_epoch_;scheduler_.Collect();
    constexpr u64 budget=16*1024*1024;
    struct Candidate {Mapping* mapping;std::vector<ReadbackSpan> spans;u64 bytes;};
    std::vector<Candidate> candidates;
    for(const auto base:dirty_mappings_){auto& mapping=mappings_.at(base);
        // Unused completed staging ages out without recopying the same version.
        if(mapping.frame_readback&&mapping.frame_readback->fence&&
           mapping.frame_readback->fence<=scheduler_.Completed()&&
           frame_readback_epoch_-mapping.frame_readback->epoch>2)mapping.frame_readback.reset();
        if(!mapping.gpu_owned||IsGdsRange(base,mapping.size)||mapping.frame_readback||
           mapping.last_prefetched_serial==mapping.contents_serial)continue;
        Candidate candidate{&mapping,{},0};
        for(const auto& [dirty_begin,dirty_end]:mapping.dirty_ranges){
            for(const auto& [owned_begin,owned_end]:mapping.gpu_owned_ranges){
                const auto begin=std::max(dirty_begin,owned_begin),end=std::min(dirty_end,owned_end);
                if(begin>=end)continue;
                if(end-begin>budget-candidate.bytes){candidate.bytes=budget+1;break;}
                const auto epoch=protected_backing_epoch_?protected_backing_epoch_(base+begin,end-begin):0;
                if(protected_backing_epoch_&&!epoch){candidate.bytes=budget+1;break;}
                candidate.spans.push_back({begin,end,candidate.bytes,epoch});
                candidate.bytes=(candidate.bytes+end-begin+63)&~u64(63);
            }
            if(candidate.bytes>budget)break;
        }
        if(candidate.bytes&&candidate.bytes<=budget)candidates.push_back(std::move(candidate));
    }
    // CPU-facing scalar/small outputs retain a chance when a large rendering
    // buffer exceeds the optional staging budget. Required reads never drop.
    std::stable_sort(candidates.begin(),candidates.end(),[](const auto& a,const auto& b){return a.bytes<b.bytes;});
    bool recorded{};
    for(auto& candidate:candidates){
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width=candidate.bytes;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;
        desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto allocation=device_->GetResourceAllocationInfo(0,1,&desc).SizeInBytes;
        if(!allocation||allocation==UINT64_MAX)throw std::runtime_error("Invalid frame readback allocation");
        if(allocation>budget||frame_readback_bytes_->load()>budget-allocation)continue;
        auto& mapping=*candidate.mapping;
        auto copy=std::make_shared<FrameReadback>();copy->source=mapping.gpu;
        copy->readback=Buffer(device_.Get(),candidate.bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        copy->spans=std::move(candidate.spans);copy->serial=mapping.contents_serial;copy->epoch=frame_readback_epoch_;
        copy->accounting=frame_readback_bytes_;copy->allocation_bytes=allocation;
        frame_readback_bytes_->fetch_add(allocation);
        // Own both placed source and staging BEFORE recording. The submission
        // callback gives the copy its actual fence even if the mapping retires.
        RetainUntilFinish([copy](u64){(void)copy;});
        NotifyOnSubmission([copy](u64 fence){copy->fence=fence;});
        mapping.gpu.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        for(const auto& span:copy->spans)DX12_API_CALL(RecordingList()->CopyBufferRegion(
            copy->readback.handle.Get(),span.offset,mapping.gpu.handle.Get(),span.begin,span.end-span.begin));
        mapping.frame_readback=std::move(copy);mapping.last_prefetched_serial=mapping.contents_serial;
        recorded=true;Diagnostics::Count("buffer-readback.frame-prefetched.bytes",candidate.bytes);
    }
    if(recorded)Flush();
}
void Rasterizer::Finish(){
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Reconcile,"execution-lock");
    std::lock_guard lock(execution_mutex_);
    read_only_batches_=0;
    diagnostic.Stage("record-buffer-readbacks");
    struct Readback {Mapping* mapping;Resource resource;std::vector<std::pair<u64,u64>> ranges;bool owned;};
    std::vector<Readback> readbacks;
    for(const auto base:dirty_mappings_){auto& r=mappings_.at(base);
        if(r.gpu_owned&&!resolving_buffers_.contains(base))continue;
        std::vector<std::pair<u64,u64>> selected;
        if(r.gpu_owned){
            const auto ranges=resolving_buffer_ranges_.find(base);
            if(ranges==resolving_buffer_ranges_.end())continue;
            for(const auto& [dirty_begin,dirty_end]:r.dirty_ranges)
                for(const auto& [wanted_begin,wanted_end]:ranges->second){
                    const auto begin=std::max(dirty_begin,wanted_begin),end=std::min(dirty_end,wanted_end);
                    if(begin<end)selected.emplace_back(begin,end);
                }
        }else selected.assign(r.dirty_ranges.begin(),r.dirty_ranges.end());
        if(selected.empty())continue;
        Begin();
        if(!r.readback.handle)r.readback=Buffer(device_.Get(),r.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        r.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        for(const auto& [begin,end]:selected)
            {Diagnostics::Count("buffer-readback.bytes",end-begin);
            DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(r.readback.handle.Get(),begin,r.gpu.handle.Get(),begin,end-begin));}
        readbacks.push_back({&r,r.readback,std::move(selected),r.gpu_owned});}
    diagnostic.Detail("cached_buffers=%zu dirty_buffers=%zu",mappings_.size(),readbacks.size());
    diagnostic.Stage("submit-and-wait");
    auto token=Flush();if(token)scheduler_.Wait(token);
    diagnostic.Stage("cpu-buffer-writeback");
    for(auto& job:readbacks){
        std::vector<ReadbackSpan> ranges;ranges.reserve(job.ranges.size());
        for(const auto& [begin,end]:job.ranges)ranges.push_back({begin,end,begin});
        CommitBufferReadback(*job.mapping,job.resource,ranges,job.owned);
    }
    scheduler_.Collect();
    uploads_.clear();
    for(auto& page:large_upload_pool_){page.arena->Reset(scheduler_.Completed());page.used=false;}
    TrimPools();
    // Every recorded list has completed here. Constant descriptors may retain
    // the arena resource, but their GPU reads have retired before the reset.
    if(constants_)DX12_API_CALL(constants_->Reset(scheduler_.Completed()));
    for(auto& page:upload_pages_)page->Reset(scheduler_.Completed());
    upload_page_cursor_=0;
    if(memory_perf_.enabled){
        const auto keep=memory_pressure_?0:size_t(memory_perf_.cpu_pool/(4*4*MemoryMiB));
        if(upload_pages_.size()>keep)upload_pages_.resize(keep);
    }
}
bool Rasterizer::PrepareCpuUpload(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);if(!IsMapped(a,s))return false;
    ResolveGpuBuffers(a,s);
    for(const auto base:dirty_mappings_){const auto& mapping=mappings_.at(base);
        if(base<a+s&&a<base+mapping.size){Finish();break;}}
    return true;
}
bool Rasterizer::ReadMemory(VAddr a,u64 s,bool){
    std::lock_guard lock(execution_mutex_);if(!IsMapped(a,s))return false;
    // Match BufferCache::ReadMemory: retire scattered GPU-modified islands
    // in the CPU request's 512 KiB download window. ResolveGpuBuffers itself
    // stays precise for GPU ownership transfers and never materializes holes.
    constexpr u64 window_size=512*1024;
    const VAddr begin=IsGdsRange(a,s)?a:a&~(window_size-1);
    const VAddr window_end=begin>UINT64_MAX-window_size?UINT64_MAX:begin+window_size;
    const VAddr end=IsGdsRange(a,s)?a+s:std::max(a+s,window_end);
    ResolveGpuBuffers(begin,end-begin);
    // As in the original buffer cache, CPU reads of clean ranges do not drain
    // the whole queue. Recorded writes are marked dirty before submission.
    for(const auto base:dirty_mappings_){const auto& mapping=mappings_.at(base);
        if(base<a+s&&a<base+mapping.size){Finish();break;}}
    return true;}
bool Rasterizer::InvalidateMemory(VAddr a,u64 s,bool){
    return ApiTrace::Call("MEMORY", "Rasterizer::InvalidateMemory", [&] {
    if(a&&s&&a<=UINT64_MAX-s){
        std::lock_guard index_lock(memory_index_mutex_);
        if(CoversSpan(indexed_ranges_,a,a+s)){
            auto view=indexed_buffers_.lower_bound(a);
            if(view!=indexed_buffers_.begin()&&std::prev(view)->second>a)--view;
            if(view==indexed_buffers_.end()||view->first>=a+s){
                // Recheck actual committed/protected pages, including PRT holes.
                // No GPU state or backing cache is touched on this path.
                CheckHostSpan(a,s,true);return true;
            }
        }
    }
    std::lock_guard lock(execution_mutex_);
    if(!IsMapped(a,s))return false;
    CheckBacking(a,s,true);
    auto first=mappings_.lower_bound(a);
    if(first!=mappings_.begin()){
        auto previous=std::prev(first);
        if(previous->first+previous->second.size>a)first=previous;
    }
    // No cached view can carry a stale CPU copy or dirty GPU neighbours here.
    // Leave fresh file destinations entirely CPU-resident without a queue wait.
    if(first==mappings_.end()||first->first>=a+s)return true;
    // Preserve the CPU write before Finish reconciles prior GPU writes. The CPU
    // invalidated range wins; untouched bytes retain the previous GPU result.
    const bool dirty_overlap=std::any_of(dirty_mappings_.begin(),dirty_mappings_.end(),[&](VAddr base){
        return base<a+s&&a<base+mappings_.at(base).size;});
    if(dirty_overlap){
        CheckBacking(a,s,true);std::vector<std::byte> cpu_write(s);memcpy(cpu_write.data(),reinterpret_cast<void*>(a),s);
        Finish();memcpy(reinterpret_cast<void*>(a),cpu_write.data(),s);
    }
    // CPU invalidation must not materialize a GPU resource for a file-loading
    // destination. Finish above preserved dirty GPU neighbours when necessary.
    // Preserve resident allocations and the previous CPU version. On the next
    // binding, only changed 4 KiB runs are uploaded; CPU writes need not drain
    // clean GPU readers because they use independent, already copied storage.
    return true;
    }, a, s);
}
u32 Rasterizer::ReadDataFromGds(u32 offset){
    std::lock_guard lock(execution_mutex_);
    if(offset>gds_.size()-4)throw std::out_of_range("GDS read range");
    ResolveGpuBuffers(reinterpret_cast<VAddr>(gds_.data())+offset,4);
    Find(offset,4,true);Finish();u32 value;memcpy(&value,gds_.data()+offset,4);return value;}
}


