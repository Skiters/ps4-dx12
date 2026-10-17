#include "memory_allocator.h"
#include "common/types.h"
#include <algorithm>

namespace NativeDX12 {
namespace {
constexpr u64 HeapAlignment=64ull*1024;
constexpr u64 MsaaHeapAlignment=4ull*1024*1024;
u64 AlignHeap(u64 value){
    if(value>UINT64_MAX-(HeapAlignment-1))throw std::overflow_error("D3D12 heap size overflow");
    return (value+HeapAlignment-1)&~(HeapAlignment-1);
}
u64 AlignTexture(u64 value,u64 alignment){
    if(!alignment||(alignment&(alignment-1)))throw std::invalid_argument("invalid D3D12 texture alignment");
    if(value>UINT64_MAX-(alignment-1))throw std::overflow_error("D3D12 texture allocation size overflow");
    return (value+alignment-1)&~(alignment-1);
}
D3D12_RESOURCE_DESC1 Description1(const D3D12_RESOURCE_DESC& d){
    return {d.Dimension,d.Alignment,d.Width,d.Height,d.DepthOrArraySize,d.MipLevels,
        d.Format,d.SampleDesc,d.Layout,d.Flags,{}};
}
}

PlacedBufferAllocator::Lease::~Lease(){if(page)page->Release(offset,size);}

void PlacedBufferAllocator::Page::Release(u64 offset,u64 bytes){
    std::lock_guard lock(mutex);
    auto next=free_ranges.lower_bound(offset),previous=next;
    if(previous!=free_ranges.begin())--previous;else previous=free_ranges.end();
    if(previous!=free_ranges.end()&&previous->first+previous->second==offset){
        offset=previous->first;bytes+=previous->second;free_ranges.erase(previous);
    }
    next=free_ranges.lower_bound(offset);
    if(next!=free_ranges.end()&&offset+bytes==next->first){bytes+=next->second;free_ranges.erase(next);}
    free_ranges.emplace(offset,bytes);
}

PlacedBufferAllocator::PlacedBufferAllocator(ID3D12Device* device,u64 page_size,u64 budget)
    :device_(device),page_size_(AlignHeap(page_size)),budget_(AlignHeap(budget)){
    if(!device||!page_size||budget_<page_size_)throw std::invalid_argument("invalid placed buffer allocator");
}
void PlacedBufferAllocator::SetBudget(u64 bytes){
    std::lock_guard lock(mutex_);
    // Existing leases remain valid above a reduced budget, but that old
    // commitment must not become the permanent limit for future allocations.
    budget_=AlignHeap(bytes);
}
void PlacedBufferAllocator::Trim(){
    std::lock_guard lock(mutex_);
    for(auto it=pages_.begin();it!=pages_.end();){
        if(it->use_count()==1){committed_-=(*it)->size;it=pages_.erase(it);}else ++it;
    }
}

std::shared_ptr<PlacedBufferAllocator::Page> PlacedBufferAllocator::CreatePage(u64 minimum){
    // Large non-power-of-two buffers otherwise waste nearly an entire arena
    // page each (e.g. twelve 160-MiB buffers consume twelve 256-MiB heaps).
    // Leases are the only owners besides pages_.  Drop completely unused
    // heaps before enforcing the current Windows memory budget.
    for(auto it=pages_.begin();it!=pages_.end();){
        if(it->use_count()==1){committed_-=(*it)->size;it=pages_.erase(it);}
        else ++it;
    }
    // Existing live pages already consume part of the budget. A second page
    // uses its remainder, rather than requesting the complete budget again.
    const auto available=budget_>committed_?budget_-committed_:0;
    const auto page_target=std::min(page_size_,available);
    // Compact relative to the current target: a reduced adapter budget may
    // make a small backing large enough to strand most of the next admission.
    const auto bytes=compact_large_pages_&&minimum>=page_target/2?AlignHeap(minimum):
        std::max(page_target,AlignHeap(minimum));
    if(bytes>budget_||committed_>budget_-bytes)throw BufferBudgetExhausted();
    D3D12_HEAP_DESC description{};description.SizeInBytes=bytes;description.Alignment=HeapAlignment;
    description.Properties.Type=D3D12_HEAP_TYPE_DEFAULT;
    description.Flags=D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    auto page=std::make_shared<Page>();page->size=bytes;page->free_ranges.emplace(0,bytes);
    Check(DX12_API_CALL(device_->CreateHeap(&description,IID_PPV_ARGS(&page->heap))));
    committed_+=bytes;pages_.push_back(page);return page;
}

Resource PlacedBufferAllocator::Allocate(u64 requested,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES initial){
    if(!requested)throw std::invalid_argument("zero placed buffer");
    D3D12_RESOURCE_DESC description{};description.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width=requested;description.Height=1;description.DepthOrArraySize=1;description.MipLevels=1;
    description.SampleDesc.Count=1;description.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;description.Flags=flags;
    const auto info=DX12_API_CALL(device_->GetResourceAllocationInfo(0,1,&description));
    const auto bytes=AlignHeap(info.SizeInBytes);
    std::lock_guard allocator_lock(mutex_);
    std::shared_ptr<Page> selected;u64 selected_offset{};
    for(auto& page:pages_){
        std::lock_guard page_lock(page->mutex);
        for(const auto& [offset,available]:page->free_ranges)if(available>=bytes){selected=page;selected_offset=offset;break;}
        if(selected)break;
    }
    if(!selected)selected=CreatePage(bytes);
    {
        std::lock_guard page_lock(selected->mutex);
        auto range=selected->free_ranges.find(selected_offset);
        const auto available=range->second;selected->free_ranges.erase(range);
        if(available>bytes)selected->free_ranges.emplace(selected_offset+bytes,available-bytes);
    }
    auto lease=std::make_shared<Lease>();lease->page=selected;lease->offset=selected_offset;lease->size=bytes;
    Resource resource;resource.state=initial;resource.allocation=lease;
    try{Check(DX12_API_CALL(device_->CreatePlacedResource(selected->heap.Get(),selected_offset,&description,
        initial,nullptr,IID_PPV_ARGS(&resource.handle))));}
    catch(...){resource.allocation.reset();throw;}
    return resource;
}

PlacedTextureAllocator::Lease::~Lease(){if(page)page->Release(offset,size);}
void PlacedTextureAllocator::Page::Release(u64 offset,u64 bytes){
    std::lock_guard lock(mutex);auto next=free_ranges.lower_bound(offset),previous=next;
    if(previous!=free_ranges.begin())--previous;else previous=free_ranges.end();
    if(previous!=free_ranges.end()&&previous->first+previous->second==offset){offset=previous->first;bytes+=previous->second;free_ranges.erase(previous);}
    next=free_ranges.lower_bound(offset);
    if(next!=free_ranges.end()&&offset+bytes==next->first){bytes+=next->second;free_ranges.erase(next);}
    free_ranges.emplace(offset,bytes);
}
PlacedTextureAllocator::PlacedTextureAllocator(ID3D12Device* device,u64 page_size,u64 budget)
    :device_(device),page_size_(AlignHeap(page_size)),budget_(AlignHeap(budget)){
    if(!device||!page_size||budget_<page_size_)throw std::invalid_argument("invalid placed texture allocator");
    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options{};
    if(SUCCEEDED(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12,&options,sizeof(options))))&&
       options.EnhancedBarriersSupported&&options.RelaxedFormatCastingSupported)
        DX12_API_CALL(device_->QueryInterface(IID_PPV_ARGS(&cast_device_)));
}
void PlacedTextureAllocator::SetBudget(u64 bytes){
    std::lock_guard lock(mutex_);
    // Existing leases may exceed a reduced limit; future pages must honor it.
    budget_=AlignHeap(bytes);
    // Allocate can reuse an idle page without reaching CreatePage's trim. Drop
    // unused commitment now when a lower adapter budget requires reclamation.
    if(committed_>budget_)for(auto it=pages_.begin();it!=pages_.end();){
        if(it->use_count()==1){committed_-=(*it)->size;it=pages_.erase(it);}
        else ++it;
        if(committed_<=budget_)break;
    }
}
std::shared_ptr<PlacedTextureAllocator::Page> PlacedTextureAllocator::CreatePage(
    u64 minimum,u64 alignment,D3D12_HEAP_FLAGS flags){
    if(alignment!=HeapAlignment&&alignment!=MsaaHeapAlignment)
        throw std::invalid_argument("invalid D3D12 texture heap alignment");
    const auto minimum_bytes=AlignTexture(minimum,alignment);
    for(auto it=pages_.begin();it!=pages_.end();){
        if(it->use_count()==1){committed_-=(*it)->size;it=pages_.erase(it);}
        else ++it;
    }
    const auto available=committed_<budget_?budget_-committed_:0;
    const auto normal_bytes=std::min(page_size_,available)&~(alignment-1);
    const auto bytes=std::max(normal_bytes,minimum_bytes);
    if(bytes>available)throw TextureBudgetExhausted();
    D3D12_HEAP_DESC description{};description.SizeInBytes=bytes;description.Alignment=alignment;
    description.Properties.Type=D3D12_HEAP_TYPE_DEFAULT;description.Flags=flags;
    auto page=std::make_shared<Page>();page->size=bytes;page->heap_alignment=alignment;page->flags=flags;page->free_ranges.emplace(0,bytes);
    Check(DX12_API_CALL(device_->CreateHeap(&description,IID_PPV_ARGS(&page->heap))));
    committed_+=bytes;pages_.push_back(page);return page;
}
D3D12_RESOURCE_ALLOCATION_INFO PlacedTextureAllocator::AllocationInfo(
    const D3D12_RESOURCE_DESC& description,std::span<const DXGI_FORMAT> formats)const{
    if(formats.empty())return DX12_API_CALL(device_->GetResourceAllocationInfo(0,1,&description));
    if(!cast_device_)throw std::runtime_error("Native packed-float bit transfers require supported D3D12 format-list allocation");
    if(formats.size()>UINT32_MAX)throw std::out_of_range("D3D12 texture format list is too large");
    const auto extended=Description1(description);const UINT32 count=UINT32(formats.size());
    const auto* data=formats.data();
    return DX12_API_CALL(cast_device_->GetResourceAllocationInfo3(0,1,&extended,&count,&data,nullptr));
}
Resource PlacedTextureAllocator::Allocate(const D3D12_RESOURCE_DESC& description,
    D3D12_RESOURCE_STATES initial,const D3D12_CLEAR_VALUE* clear,std::span<const DXGI_FORMAT> formats){
    if(description.Dimension==D3D12_RESOURCE_DIMENSION_BUFFER||!description.Width)
        throw std::invalid_argument("invalid placed texture description");
    const auto info=AllocationInfo(description,formats);
    if(!info.SizeInBytes||info.SizeInBytes==UINT64_MAX)throw std::runtime_error("invalid D3D12 texture allocation info");
    const auto alignment=info.Alignment,bytes=AlignTexture(info.SizeInBytes,alignment);
    // Even a small MSAA resource with 64-KiB placement needs a 4-MiB heap.
    const auto heap_alignment=description.SampleDesc.Count>1?MsaaHeapAlignment:HeapAlignment;
    const auto flags=(description.Flags&(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))?
        D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES:D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    std::lock_guard allocator_lock(mutex_);std::shared_ptr<Page> selected;u64 selected_offset{};
    for(auto& page:pages_)if(page->flags==flags&&page->heap_alignment>=heap_alignment){std::lock_guard page_lock(page->mutex);
        for(const auto& [offset,available]:page->free_ranges){
            if(offset>UINT64_MAX-(alignment-1))continue;
            const auto aligned=AlignTexture(offset,alignment);
            if(aligned>=offset&&aligned-offset<=available&&bytes<=available-(aligned-offset)){selected=page;selected_offset=aligned;break;}}
        if(selected)break;}
    if(!selected)selected=CreatePage(bytes,heap_alignment,flags);
    {std::lock_guard page_lock(selected->mutex);auto containing=selected->free_ranges.upper_bound(selected_offset);
        if(containing==selected->free_ranges.begin())throw std::logic_error("texture arena range missing");--containing;
        const auto begin=containing->first,available=containing->second,end=begin+available;selected->free_ranges.erase(containing);
        if(begin<selected_offset)selected->free_ranges.emplace(begin,selected_offset-begin);
        if(selected_offset+bytes<end)selected->free_ranges.emplace(selected_offset+bytes,end-(selected_offset+bytes));}
    auto lease=std::make_shared<Lease>();lease->page=selected;lease->offset=selected_offset;lease->size=bytes;
    Resource resource;resource.state=initial;resource.allocation=lease;
    try{
        if(formats.empty())Check(DX12_API_CALL(device_->CreatePlacedResource(selected->heap.Get(),selected_offset,&description,
            initial,clear,IID_PPV_ARGS(&resource.handle))));
        else{
            if(initial!=D3D12_RESOURCE_STATE_COMMON&&initial!=D3D12_RESOURCE_STATE_COPY_DEST)
                throw std::invalid_argument("Unsupported initial state for native format-list texture");
            const auto extended=Description1(description);
            resource.castable_formats=std::make_shared<const std::vector<DXGI_FORMAT>>(formats.begin(),formats.end());
            Check(DX12_API_CALL(cast_device_->CreatePlacedResource2(selected->heap.Get(),selected_offset,&extended,
                Resource::EnhancedLayout(initial),clear,UINT32(formats.size()),formats.data(),IID_PPV_ARGS(&resource.handle))));
            // This API creates an enhanced layout, even before the first
            // transition. COPY_DEST cannot enter legacy tracking directly.
            resource.enhanced_barriers=true;
        }
    }catch(...){resource.allocation.reset();throw;}
    return resource;
}

} // namespace NativeDX12
