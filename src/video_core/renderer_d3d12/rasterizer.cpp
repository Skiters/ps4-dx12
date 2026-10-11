#include "common/api_trace.h"
#include "rasterizer.h"
#include <dxgi1_4.h>
#include <cstring>
#include "shader_pipeline.h"
#include "compute_pipeline.h"
#include "guest_dma.h"
#include "common/scope_exit.h"
#include <algorithm>
#include <execution>
#include <numeric>
#include <thread>
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
    bool Query(DXGI_QUERY_VIDEO_MEMORY_INFO& memory){
        const auto now=GetTickCount64();
        if(checked&&now-checked<1000&&(!event||WaitForSingleObject(event,0)!=WAIT_OBJECT_0))return false;
        checked=now;
        return adapter&&SUCCEEDED(DX12_API_CALL(adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&memory)))&&memory.Budget;
    }
};
void Rasterizer::RefreshBufferBudget(){
    std::lock_guard lock(execution_mutex_);
    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
    if(!buffer_budget_observer_->Query(memory))return;
    const auto limits=memory_perf_.Budget(memory.Budget,memory.CurrentUsage,buffer_allocator_.CommittedBytes());
    memory_pressure_=memory_perf_.enabled&&limits.cache<memory_perf_.requested;
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
Rasterizer::~Rasterizer(){try{Finish();}catch(...) {/* device loss: release resources */}}
GuestDmaMemory& Rasterizer::DmaMemory(){
    if(!dma_memory_)dma_memory_=std::make_unique<GuestDmaMemory>(device_.Get());
    return *dma_memory_;
}
bool Rasterizer::IsDmaReadable(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size||!IsMapped(address,size))return false;
    if(CoversSpan(committed_readable_,address,address+size))return true;
    auto found=mappings_.upper_bound(address);
    if(found!=mappings_.begin()){--found;if(found->second.gpu_owned&&address-found->first<=found->second.size&&
        size<=found->second.size-(address-found->first))return true;}
    try{CheckHostSpan(address,size,false);return true;}
    catch(const std::runtime_error&){return false;}
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
void Rasterizer::NotifyMemoryProtection(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    if(!size||address>UINT64_MAX-size)throw std::out_of_range("Invalid protection notification");
    ForgetBacking(address,size);
}
void Rasterizer::NotifyHostMapping(VAddr address,u64 size,bool readable,bool writable){
    std::lock_guard lock(execution_mutex_);
    if(!address||!size||address>UINT64_MAX-size||(!readable&&writable))
        throw std::invalid_argument("Invalid committed host mapping notification");
    ForgetBacking(address,size);
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
    scheduler_.Begin();
}
Rasterizer::Mapping& Rasterizer::Find(VAddr address,u64 size,bool gds,u64 batch_budget){
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
    // Bound materialization by the same address-cache capacity as the complete
    // batch; a legal large binding must not hit an unrelated 256-MiB slice cap.
    constexpr u64 max_slice=Shader::NativeDma::ResidentBufferBytes;
    const auto cache_budget=std::max(buffer_cache_budget_,batch_budget);
    if(size>max_slice||size>cache_budget)throw std::runtime_error("Native buffer slice exceeds current slice/residency budget");
    DropCached(address,size,true);
    if(CachedBufferBytes()>cache_budget-size){ResolveGpuBuffers(0,UINT64_MAX);Finish();for(auto& [_,m]:mappings_)TraceMapping(m,"budget");ClearMappings();}
    CheckBacking(address,size,false);
    // Publish before GPU allocation/upload; concurrent CPU invalidation must
    // take the serialized path as soon as a GPU view is being materialized.
    {std::lock_guard lock(memory_index_mutex_);indexed_buffers_[address]=address+size;}
    Mapping* stored{};
    try{
        Mapping range{address,size,AcquireBuffer((size+3)&~u64(3))};
        range.allocation_id=acquired_allocation_id_;range.last_use=++use_clock_;
        stored=&mappings_.emplace(address,std::move(range)).first->second;
        churn_.Record(address,size,address,size,stored->allocation_id,true,false,false,scheduler_.Completed());
    }catch(...){
        // No GPU command references this allocation yet. Do not leave the CPU
        // invalidation index advertising a mapping that was never created.
        RemoveBufferIndex(address);throw;
    }
    cached_buffer_bytes_+=size;
    try{Upload(*stored,0,size,reinterpret_cast<void*>(address));}
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
            if(!memory_perf_.enabled)throw;
            // Idle pool resources can occupy otherwise reusable arena pages.
            // Never reclaim a lease held by a view or an unfinished submission.
            for(auto it=buffer_pool_.begin();it!=buffer_pool_.end();){
                if(it->fence>completed||(it->gpu.allocation&&it->gpu.allocation.use_count()>1)){++it;continue;}
                pooled_buffer_bytes_-=it->gpu.handle->GetDesc().Width;it=buffer_pool_.erase(it);
            }
            buffer_allocator_.Trim();Diagnostics::Count("buffer-allocation.pressure-retry");
            return buffer_allocator_.Allocate(size);
        }
    }
    Diagnostics::Count("buffer-allocation.reuse");
    acquired_allocation_id_=best->allocation_id;
    pooled_buffer_bytes_-=best->gpu.handle->GetDesc().Width;
    auto result=std::move(best->gpu);buffer_pool_.erase(best);return result;
}
void Rasterizer::RetireBuffer(Mapping& mapping){
    if(mapping.gpu_owned)throw std::logic_error("Retiring GPU-owned buffer before CPU reconciliation");
    if(mapping.cpu_watched){release_buffer_(mapping.base,mapping.size);mapping.cpu_watched=false;}
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
    // Mappings are disjoint. Include a containing predecessor, then only the
    // actual intersections; retaining more GPU buffers must not make every
    // unrelated guest CPU access scan the whole resident cache.
    for(auto it=FirstOverlapping(address);it!=mappings_.end()&&it->first<address+size;++it){
        ++memory_reuse_.watch_candidates;auto& mapping=it->second;
        if(mapping.cpu_watched){release_buffer_(it->first,mapping.size);mapping.cpu_watched=false;}
    }
}
void Rasterizer::InstallCpuWatch(Mapping& mapping){
    if(mapping.cpu_watched||!watch_buffer_||mapping.cpu_snapshot.size()!=mapping.size)return;
    if(!watch_buffer_(mapping.base,mapping.size))return;
    // Protection is active BEFORE verifying the proof. A store in the gap
    // between the previous snapshot and Track must not become a watch hit.
    Diagnostics::Count("buffer-watch.establish-compare.bytes",mapping.size);
    if(memcmp(mapping.cpu_snapshot.data(),reinterpret_cast<const void*>(mapping.base),size_t(mapping.size))==0)
        mapping.cpu_watched=true;
    else{release_buffer_(mapping.base,mapping.size);Diagnostics::Count("buffer-watch.establish-changed");}
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
    SCOPE_EXIT {
        for(const auto base:requested)resolving_buffers_.erase(base);
    };
    for(auto it=first;it!=mappings_.end()&&it->first<address+size;++it)
        if(it->second.gpu_owned&&!resolving_buffers_.contains(it->first)){
            requested.push_back(it->first);
            resolving_buffers_.insert(it->first);
        }
    // Finish also writes CPU-visible dirty mappings. Their virtual CheckBacking
    // calls return here while an unrelated owned mapping is still resolving.
    // Only this request's new GPU-owned overlaps may start a reconciliation;
    // the outer set is not evidence that this CPU range needs another Finish.
    // Retain outer ownership, and unwind only our additions even on failure.
    if(requested.empty())return;
    Finish();
}
bool Rasterizer::BuffersMayDefer(std::span<const ShaderBufferRequest> requests){
    std::lock_guard lock(execution_mutex_);
    for(const auto& request:requests)if(request.written){
        auto it=mappings_.upper_bound(request.address);
        if(it==mappings_.begin()){Diagnostics::Count("buffer-defer.no-mapping");return false;}--it;
        if(!it->second.gpu_owned){Diagnostics::Count("buffer-defer.not-owned");return false;}
        if(request.address-it->first>it->second.size||
           request.size>it->second.size-(request.address-it->first)){
            Diagnostics::Count("buffer-defer.not-covered");return false;
        }
    }
    Diagnostics::Count("buffer-defer.accepted");
    return true;
}
void Rasterizer::Upload(Mapping& range,u64 offset,u64 size,const void* data){
    Diagnostics::Count("buffer-upload.calls");Diagnostics::Count("buffer-upload.bytes",size);
    churn_.Record(range.base,size,range.base,range.size,range.allocation_id,false,range.dirty,range.gpu_owned,scheduler_.Completed(),nullptr,size);
    const auto exceptions=std::uncaught_exceptions();
    SCOPE_EXIT {if(std::uncaught_exceptions()>exceptions)range.cpu_snapshot.clear();};
    const auto snapshot=[&](const void* source)->const void*{
        if(range.cpu_snapshot.empty()&&!offset&&size==range.size)ResizeSnapshot(range);
        if(range.cpu_snapshot.size()!=range.size)return source;
        // Snapshot the exact upload version in ordinary cached host memory.
        // Never read back a write-combined D3D12 UPLOAD heap on the CPU.
        memcpy(range.cpu_snapshot.data()+offset,source,size_t(size));
        return range.cpu_snapshot.data()+offset;
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
        if(IsMapped(source,size))ReadMemory(source,size);
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
        memcpy(slice.cpu,data,size);
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
    if(IsMapped(source,size))ReadMemory(source,size);
    data=snapshot(data);
    if(memory_perf_.enabled&&size<=memory_perf_.cpu_pool/4){
        const auto source=reinterpret_cast<VAddr>(data);
        if(IsMapped(source,size))ReadMemory(source,size);
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
            memcpy(slice.cpu,data,size_t(size));
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
    const unsigned jobs=size>=4*1024*1024?std::min(14u,std::max(1u,std::thread::hardware_concurrency()/2)):1u;
    std::array<unsigned,14> indices{};std::iota(indices.begin(),indices.end(),0u);
    const auto copy=[&](unsigned job){const auto first=size*job/jobs,last=size*(job+1)/jobs;
        memcpy(static_cast<std::byte*>(dest)+first,static_cast<const std::byte*>(data)+first,size_t(last-first));};
    if(jobs==1)copy(0);else std::for_each(std::execution::par,indices.begin(),indices.begin()+jobs,copy);
    DX12_API_CALL(upload.handle->Unmap(0,nullptr));
    // Retain before recording/submitting. Growing the retention vector can
    // throw; after submission that must never release the copy source early.
    uploads_.push_back(std::move(upload));
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
std::vector<Rasterizer::ShaderBufferView> Rasterizer::PrepareShaderBuffers(std::span<const ShaderBufferRequest> requests,
    std::optional<std::span<const BufferWrite>> known_writes) {
    std::lock_guard streaming_lock(execution_mutex_);
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
            bool eligible=request.address&&request.size&&request.size<=16384&&!request.written&&!request.unordered_access&&
                request.address<=UINT64_MAX-request.size&&request.address%4==0&&request.size%4==0&&IsMapped(request.address,request.size);
            if(eligible)for(const auto& other:requests)if(other.written&&
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
                    CheckBacking(requests[i].address,requests[i].size,false);ReadMemory(requests[i].address,requests[i].size);}
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
        // Stable 64KiB residency windows for large read-only PS4 suballocations.
        // Small offset/length changes must not destroy the entire backing buffer.
        // Expand only into Core-confirmed committed readable memory, and never
        // across another binding's incompatible SRV/UAV or writable range.
        if(buffer_arenas_enabled_&&!request.written&&request.size>=65536&&last<=UINT64_MAX-65535){
            const VAddr arena_first=first&~VAddr(65535),arena_last=(last+65535)&~VAddr(65535);
            bool allowed=arena_last-arena_first<=Shader::NativeDma::ResidentBufferBytes&&
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
                    if(union_last-union_first>Shader::NativeDma::ResidentBufferBytes||
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
        ranges.emplace_back(first,last);
    }
    std::sort(ranges.begin(),ranges.end());std::vector<std::pair<VAddr,VAddr>> merged;
    for(auto range:ranges) {
        if(!merged.empty()&&range.first<merged.back().second)merged.back().second=std::max(merged.back().second,range.second);
        else merged.push_back(range);
    }
    u64 total=0;for(auto [first,last]:merged){auto size=last-first;if(size>Shader::NativeDma::ResidentBufferBytes-total)
        throw std::runtime_error("Native shader buffer batch exceeds address-cache capacity: budget="+std::to_string(Shader::NativeDma::ResidentBufferBytes)+
            " requested_at_least="+std::to_string(total+size));total+=size;}
    // A shrinking OS target cannot evict resources used by this very command.
    // Trim inactive mappings first, while preserving the complete active batch.
    // The placed allocator still enforces the physical arena growth budget.
    const auto cache_budget=std::max(buffer_cache_budget_,total);
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
        if(!protected_resident)PrepareGpuBacking(request.address,request.size,request.written);
    }
    // Bound retained constants/descriptors while allowing a useful packet of
    // read-only passes to reach the GPU together instead of waiting per draw.
    const bool cpu_visible_dirty=std::any_of(dirty_mappings_.begin(),dirty_mappings_.end(),
        [&](VAddr base){return !mappings_.at(base).gpu_owned;});
    if(cpu_visible_dirty||read_only_batches_>=32){Diagnostics::Count(cpu_visible_dirty?"buffer-finish.cpu-visible":"buffer-finish.batch-limit");Finish();}
    ++read_only_batches_;
    // Retire partial-overlap cache entries before retaining any batch mappings.
    // Otherwise a later Find can erase the mapping selected for an earlier,
    // disjoint request covered by the same old larger cache allocation.
    diagnostic.Stage("retire-overlaps");
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
        if(!covered)DropCached(first,last-first,true,"envelope");
    }
    // Reserve only genuinely new backing. Counting already resident requests
    // twice evicted every large (>256 MiB) unchanged batch on every draw.
    u64 additional{};
    for(auto [first,last]:merged){
        auto found=mappings_.upper_bound(first);bool covered=false;
        if(found!=mappings_.begin()){--found;covered=first-found->first<=found->second.size&&
            last-first<=found->second.size-(first-found->first);}
        if(!covered)additional+=last-first;
    }
    if(CachedBufferBytes()>cache_budget-additional){
        // Cache eviction destroys resources referenced by recorded commands.
        // The read-only fast path above does not retire those users.
        Finish();
        std::set<VAddr> protected_mappings;
        for(auto [first,last]:merged){
            auto found=mappings_.upper_bound(first);
            if(found!=mappings_.begin()){--found;if(first-found->first<=found->second.size&&
                last-first<=found->second.size-(first-found->first))protected_mappings.insert(found->first);}
        }
        // Finish has retired all users. Drop only inactive mappings before
        // considering full rematerialization of oversized covered allocations.
        std::vector<VAddr> candidates;
        for(const auto& [base,mapping]:mappings_)if(!protected_mappings.contains(base))candidates.push_back(base);
        if(memory_perf_.enabled)std::sort(candidates.begin(),candidates.end(),[&](VAddr a,VAddr b){
            return mappings_.at(a).last_use<mappings_.at(b).last_use;
        });
        for(const auto base:candidates){
            if(CachedBufferBytes()<=cache_budget-additional)break;
            auto it=mappings_.find(base);ResolveGpuBuffers(it->first,it->second.size);
            TraceMapping(it->second,"budget");cached_buffer_bytes_-=it->second.size;dirty_mappings_.erase(it->first);RemoveBufferIndex(it->first);RetireBuffer(it->second);mappings_.erase(it);
        }
        if(CachedBufferBytes()>cache_budget-additional){ResolveGpuBuffers(0,UINT64_MAX);ClearMappings();}
    }
    diagnostic.Stage("materialize-resident-backing");
    diagnostic.Detail("requests=%zu merged=%zu bytes=%llu additional=%llu cached=%llu",requests.size(),merged.size(),
        (unsigned long long)total,(unsigned long long)additional,(unsigned long long)CachedBufferBytes());
    std::vector<VAddr> created;
    std::set<VAddr> active_mappings;
    for(auto [first,last]:merged){
        auto found=mappings_.upper_bound(first);bool cached=false;
        if(found!=mappings_.begin()){--found;cached=first-found->first<=found->second.size&&last-first<=found->second.size-(first-found->first);}
        auto& mapping=Find(first,last-first,false,cache_budget);if(!cached)created.push_back(mapping.base);
        active_mappings.insert(mapping.base);
    }
    for(const auto base:active_mappings) {
        auto& mapping=mappings_.at(base);
        bool active=false,written=false,uav=false;for(const auto& r:requests)if(r.address>=base&&r.address+r.size<=base+mapping.size){active=true;written|=r.written;uav|=r.unordered_access;}
        if(!active)continue;
        const bool fresh=std::find(created.begin(),created.end(),base)!=created.end();
        diagnostic.Stage("backing-validation");
        diagnostic.Detail("address=%llx bytes=%llu fresh=%u written=%u",(unsigned long long)base,
            (unsigned long long)mapping.size,unsigned(fresh),unsigned(written));
        ValidateGpuBacking(base,mapping.size,written);
        bool cpu_proven_unchanged=mapping.cpu_watched;
        if(written&&mapping.cpu_watched){
            if(transfer_buffer_watch_&&transfer_buffer_watch_(base,mapping.size,false)){
                mapping.cpu_watched=false;mapping.gpu_owned=true;
                Diagnostics::Count("buffer-watch.upgraded");
            }else{
                // No proof survives a release of protection. Use the snapshot
                // fallback after a rejected or unavailable atomic transfer.
                ReleaseCpuBufferWatches(base,mapping.size);cpu_proven_unchanged=false;
                Diagnostics::Count("buffer-watch.transfer-fallback");
            }
        }
        const auto* cpu=reinterpret_cast<const std::byte*>(base);
        diagnostic.Stage("snapshot-compare");
        if(!mapping.gpu_owned&&!mapping.cpu_watched&&!cpu_proven_unchanged){
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
        }else if(cpu_proven_unchanged)Diagnostics::Count("buffer-snapshot.watch-hit");
        // A read-only use of a preceding GPU result must retain dirty ownership.
        diagnostic.Stage("ownership-and-transition");
        if(written){if(!mapping.gpu_owned&&own_buffer_){
            mapping.gpu_owned=own_buffer_(mapping.base,mapping.size);
            Diagnostics::Count(mapping.gpu_owned?"buffer-own.success":"buffer-own.failure");
        }
            if(known_writes){
                for(const auto& write:*known_writes)if(write.address>=mapping.base&&write.address+write.size<=mapping.base+mapping.size)
                    MarkDirty(mapping,true,write.address-mapping.base,write.size);
            }else for(const auto& request:requests)if(request.written&&request.address>=mapping.base&&
                request.address+request.size<=mapping.base+mapping.size)
                MarkDirty(mapping,true,request.address-mapping.base,request.size);}
        else if(!mapping.gpu_owned)InstallCpuWatch(mapping);
        mapping.gpu.Transition(RecordingList(),uav?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    diagnostic.Stage("emit-buffer-views");
    std::vector<ShaderBufferView> views;views.reserve(requests.size());
    for(const auto& request:requests) {
        auto& mapping=Find(request.address,request.size,false,cache_budget);
        views.push_back({mapping.gpu.handle,request.address-mapping.base,request.size,{},mapping.gpu.allocation});
    }
    return views;
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
    ForgetBacking(address,size);
    // core/memory.cpp also calls this AFTER impl.Unmap. Never dereference here.
    // Notify backing-page change; stale cached data must not overwrite new pages.
    DropCached(address,size,false);
}
void Rasterizer::MapMemory(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);
    if(!a||!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid mapped range");auto end=a+s;
    ForgetBacking(a,s);
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
    ResolveGpuBuffers(a,s);
    if(reconcile)Finish();else{auto token=Flush();if(token)scheduler_.Wait(token);}
    while(first!=mappings_.end()&&first->first<a+s){TraceMapping(first->second,reason);cached_buffer_bytes_-=first->second.size;
        dirty_mappings_.erase(first->first);RemoveBufferIndex(first->first);RetireBuffer(first->second);first=mappings_.erase(first);}
}
void Rasterizer::UnmapMemory(VAddr a,u64 s){
    std::lock_guard lock(execution_mutex_);
    if(!s||a>UINT64_MAX-s)throw std::invalid_argument("invalid unmapped range");ForgetBacking(a,s);DropCached(a,s,true);auto end=a+s;
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
    auto& r=Find(a,s,gds);
    auto real=gds?a+reinterpret_cast<VAddr>(gds_.data()):a;
    std::vector<u32> words(s/4,value);Upload(r,real-r.base,s,words.data());MarkDirty(r,true,real-r.base,s);
}
void Rasterizer::CopyBuffer(VAddr dst,VAddr src,u32 s,bool dg,bool sg){
    std::lock_guard lock(execution_mutex_);
    if((dg&&(dst>gds_.size()||s>gds_.size()-dst))||(sg&&(src>gds_.size()||s>gds_.size()-src)))
        throw std::out_of_range("GDS copy range");
    auto da=dg?dst+reinterpret_cast<VAddr>(gds_.data()):dst;
    auto sa=sg?src+reinterpret_cast<VAddr>(gds_.data()):src;
    if(!s||s>32*1024*1024)throw std::invalid_argument("copy requires 1..32 MiB bounded slices");
    if(da>UINT64_MAX-s||sa>UINT64_MAX-s)throw std::out_of_range("copy address overflow");
    if(memory_perf_.enabled&&!(da%4)&&!(sa%4)&&!(s%4)){
        // Reuse the verified exact-footprint copy path. Its active batch pins
        // both operands, retains dirty neighbors, handles same-resource scratch
        // and reconciles before CPU access. A tiny DMA must not flush unrelated
        // resident backing merely because the total cache exceeds 64 MiB.
        const BufferCopy copy{da,sa,s};CopyBufferBatch(std::span(&copy,1));return;
    }
    // Pin both participating slices by reserving the worst-case pair budget.
    if(CachedBufferBytes()>64*1024*1024-u64(s)*2){
        Diagnostics::Count("buffer-cache.dma-global-clear");
        ResolveGpuBuffers(0,UINT64_MAX);Finish();
        for(auto& [_,mapping]:mappings_)TraceMapping(mapping,"dma-global-clear");ClearMappings();}
    // Discard existing overlapping cache spans before taking references: one
    // old span may partially cover both requests and invalidate the first Find.
    DropCached(da,s,true);DropCached(sa,s,true);
    if(da<sa+s&&sa<da+s)Find(std::min(da,sa),std::max(da,sa)+s-std::min(da,sa),false);
    auto& d=Find(dst,s,dg);auto& source=Find(src,s,sg);
    auto doff=(dg?dst+reinterpret_cast<VAddr>(gds_.data()):dst)-d.base;
    auto soff=(sg?src+reinterpret_cast<VAddr>(gds_.data()):src)-source.base;Begin();
    if(&d==&source){
        auto temp=Buffer(device_.Get(),s,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
        uploads_.push_back(temp);
        source.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(temp.handle.Get(),0,source.gpu.handle.Get(),soff,s));
        temp.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);d.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(d.gpu.handle.Get(),doff,temp.handle.Get(),0,s));
    }else{source.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);d.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_DEST);DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(d.gpu.handle.Get(),doff,source.gpu.handle.Get(),soff,s));}MarkDirty(d,true,doff,s);
}
void Rasterizer::CopyBufferBatch(std::span<const BufferCopy> copies) {
    std::lock_guard lock(execution_mutex_);
    constexpr u64 max_extent=64*1024*1024;
    bool destinations_may_defer=!copies.empty();
    std::map<VAddr,VAddr> destinations;
    // Validate the whole dispatch before recording any destination writes.
    for(const auto& copy:copies){
        // D3D12 buffer copies are byte-addressed.  PS4 copy shaders routinely
        // use byte and half-word strides, so imposing a dword alignment here
        // rejects valid packets even though CopyBufferRegion supports them.
        if(!copy.bytes||copy.bytes>max_extent)
            throw std::invalid_argument("Native GPU copy packet needs 1..64 MiB regions");
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
            if(se-sb>max_extent||de-db>max_extent)break;
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
    if(destinations_may_defer){Diagnostics::Count("copy-hle.deferred");Flush();}
    else{Diagnostics::Count("copy-hle.reconciled");Finish();}
}
u64 Rasterizer::Flush(){
    std::lock_guard lock(execution_mutex_);
    const auto submitted=scheduler_.Submit(deferred_retirements_,submission_callbacks_);
    deferred_retirements_.clear();
    submission_callbacks_.clear();
    return submitted;
}
void Rasterizer::Finish(){
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Reconcile,"execution-lock");
    std::lock_guard lock(execution_mutex_);
    read_only_batches_=0;
    diagnostic.Stage("record-buffer-readbacks");
    std::vector<std::pair<Mapping*,Resource>> readbacks;
    for(const auto base:dirty_mappings_){auto& r=mappings_.at(base);
        if(r.gpu_owned&&!resolving_buffers_.contains(base))continue;
        Begin();
        if(!r.readback.handle)r.readback=Buffer(device_.Get(),r.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
        r.gpu.Transition(scheduler_.Begin(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        for(const auto& [begin,end]:r.dirty_ranges)
            {Diagnostics::Count("buffer-readback.bytes",end-begin);
            DX12_API_CALL(scheduler_.Begin()->CopyBufferRegion(r.readback.handle.Get(),begin,r.gpu.handle.Get(),begin,end-begin));}
        readbacks.emplace_back(&r,r.readback);}
    diagnostic.Detail("cached_buffers=%zu dirty_buffers=%zu",mappings_.size(),readbacks.size());
    diagnostic.Stage("submit-and-wait");
    auto token=Flush();if(token)scheduler_.Wait(token);
    diagnostic.Stage("cpu-buffer-writeback");
    for(auto& [r,rb]:readbacks){
        const auto first_dirty=r->dirty_ranges.begin()->first,last_dirty=r->dirty_ranges.rbegin()->second;
        void* data=nullptr;D3D12_RANGE all{SIZE_T(first_dirty),SIZE_T(last_dirty)};Check(DX12_API_CALL(rb.handle->Map(0,&all,&data)));
        SCOPE_EXIT {D3D12_RANGE no_write{0,0};DX12_API_CALL(rb.handle->Unmap(0,&no_write));};
        if(r->gpu_owned){
            for(const auto& [begin,end]:r->dirty_ranges){
                if(!commit_buffer_(r->base+begin,std::span(static_cast<const std::byte*>(data)+begin,size_t(end-begin))))
                    throw std::runtime_error("Cannot commit GPU buffer through PS4 physical backing");
                Diagnostics::Count("buffer-commit.bytes",end-begin);
            }
            if(r->cpu_snapshot.size()==r->size&&transfer_buffer_watch_&&transfer_buffer_watch_(r->base,r->size,true)){
                r->cpu_watched=true;Diagnostics::Count("buffer-watch.downgraded");
            }else release_buffer_(r->base,r->size);
            r->gpu_owned=false;
        }else{for(const auto& [begin,end]:r->dirty_ranges){CheckBacking(r->base+begin,end-begin,true);
            Diagnostics::Count("buffer-commit.bytes",end-begin);
            memcpy(reinterpret_cast<void*>(r->base+begin),static_cast<const std::byte*>(data)+begin,end-begin);}}
        // Finish establishes identical CPU/GPU contents. Preserve that version
        // so the next writable binding uploads only subsequent CPU changes.
        if(r->cpu_snapshot.size()!=r->size){ResizeSnapshot(*r);
            memcpy(r->cpu_snapshot.data(),reinterpret_cast<const void*>(r->base),size_t(r->size));}
        for(const auto& [begin,end]:r->dirty_ranges)memcpy(r->cpu_snapshot.data()+begin,
            static_cast<const std::byte*>(data)+begin,size_t(end-begin));
        MarkDirty(*r,false);
        // Reconciled whole pages are identical on CPU and GPU. A write-watch
        // makes the next writable binding O(1) when the guest CPU left them
        // untouched; a real CPU store faults, removes the watch and retains the
        // existing snapshot/block-diff upload path.
        InstallCpuWatch(*r);
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
bool Rasterizer::ReadMemory(VAddr a,u64 s,bool){
    std::lock_guard lock(execution_mutex_);if(!IsMapped(a,s))return false;
    ResolveGpuBuffers(a,s);
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
    std::lock_guard lock(execution_mutex_);Find(offset,4,true);Finish();u32 value;memcpy(&value,gds_.data()+offset,4);return value;}
}


