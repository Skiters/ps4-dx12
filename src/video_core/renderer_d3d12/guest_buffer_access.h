// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "page_watch.h"
#include "guest_page_range.h"
#include "diagnostics.h"
namespace NativeDX12 {
// Production buffer/CPU access contract shared by GuestRasterizer and the
// native integration fixture. Image aliases are resolved by the guest cache.
class GuestBufferAccess : public Rasterizer {
protected:
    std::unique_ptr<PageWatch> page_watch_;
    std::function<bool(VAddr,u64)> has_backing_;
    virtual void ResolveImages(VAddr,u64) {}
    // GPU buffer admission may retain a separate current native image instead
    // of exposing it to physical CPU backing. CPU access still resolves images.
    virtual void PreserveImagesForGpuBuffer(VAddr address,u64 size){ResolveImages(address,size);}
    virtual void ReleaseImageWatches(VAddr,u64) {}
    virtual void ReleaseBufferPageWatch(VAddr address,u64 size){
        // A borrowed read-only range may span more than this mapping. Split
        // its exact pages rather than restoring protection for a neighbour.
        const auto pages=GuestProtectionPages(address,size);
        if(!pages)throw std::invalid_argument("Invalid buffer protection retirement extent");
        (void)page_watch_->ChangeMode(pages->address,pages->size,true);
        if(!page_watch_->UntrackReadOnly(pages->address,pages->size))
            throw std::runtime_error("Cannot retire a buffer guard with unresolved GPU ownership");
    }
    // Separate logical GPU versions need to distinguish exposing a preceding
    // result to a CPU reader from retiring it before a real CPU store.
    virtual void ResolveCpuFault(VAddr address,bool writing){
        ResolveGpuBuffers(address,1);
        if(writing)InvalidateCpuBufferPages(address,1);
        ReleaseImageWatches(address,1);ResolveImages(address,1);
    }
    void CheckGpuBacking(VAddr address,u64 size,bool writing) override {
        // Core's committed/right intervals authorize protected host pages.
        // Also require the physical alias while a watch masks host access.
        Rasterizer::CheckBacking(address,size,writing);
        if(page_watch_&&page_watch_->Contains(address)&&!has_backing_(address,size))
            throw std::out_of_range("Protected GPU buffer lost physical backing");
    }
    void CheckBacking(VAddr address,u64 size,bool writing) override {
        Diagnostics::Count("buffer-access.cpu-reconcile");
        ResolveGpuBuffers(address,size);
        if(writing){InvalidateCpuBufferPages(address,size);ReleaseImageWatches(address,size);}
        Rasterizer::CheckBacking(address,size,writing);
    }
    void PrepareGpuBacking(VAddr address,u64 size,bool writing) override {
        // Unsupported buffer/image aliases use physical backing conversion.
        // This does not release buffer watches or resolve resident buffers.
        PreserveImagesForGpuBuffer(address,size);
        if(writing)ReleaseImageWatches(address,size);
    }
    void SetBufferBackingAccess(std::function<bool(VAddr,u64)> has,
        std::function<bool(VAddr,std::span<const std::byte>)> write){
        has_backing_=std::move(has);
        if(!page_watch_)page_watch_=std::make_unique<PageWatch>([this](VAddr address,bool writing){
            std::lock_guard lock(execution_mutex_);
            ResolveCpuFault(address,writing);
        });
        SetProtectedBackingEpoch([this](VAddr address,u64 size){
            return has_backing_(address,size)?page_watch_->ProtectedBackingEpoch(address,size):0;
        });
        SetBufferOwnership([this](VAddr address,u64 size){
            if(address%4096||size%4096){Diagnostics::Count("buffer-own.unaligned");return false;}
            if(!has_backing_(address,size)){Diagnostics::Count("buffer-own.no-backing");return false;}
            ReleaseImageWatches(address,size);PreserveImagesForGpuBuffer(address,size);
            const bool tracked=page_watch_->TrackOwned(address,size);
            Diagnostics::Count(tracked?"buffer-own.accepted":"buffer-own.track-conflict");
            Diagnostics::Count(tracked?"buffer-own.accepted-bytes":"buffer-own.rejected-bytes",size);
            return tracked;
        },[this,write=std::move(write)](VAddr address,std::span<const std::byte> bytes){
            page_watch_->InvalidateBackingEpoch(address,bytes.size());
            return write(address,bytes);
        },[this](VAddr address,u64 size){ReleaseBufferPageWatch(address,size);});
        const auto watch_cpu_pages=[this](VAddr address,u64 size){
            // Original MemoryTracker watches physical pages, not the sharp or
            // native allocation bounds. Validate the complete page envelope:
            // neighbours may fault too and must invalidate every B witness on
            // that page. Never expand shader/resource bounds or demote I.
            const auto pages=GuestProtectionPages(address,size);
            return pages&&has_backing_(pages->address,pages->size)&&
                page_watch_->TrackReadOnly(pages->address,pages->size);
        };
        SetBufferWriteWatch(watch_cpu_pages);
        SetBufferPageRewatch(watch_cpu_pages);
        SetCpuPageRelease([this](VAddr address,u64 size){return page_watch_->UntrackReadOnly(address,size);});
        SetBufferWatchTransfer([this](VAddr address,u64 size,bool read_only){
            return has_backing_(address,size)&&page_watch_->ChangeMode(address,size,read_only);
        });
    }
public:
    using Rasterizer::Rasterizer;
    bool IsDmaReadable(VAddr address,u64 size)override{
        // Renderer-only host temporaries may have a mapped native address, but
        // ReadConst guest pointers require real physical guest backing.
        return (!has_backing_||has_backing_(address,size))&&Rasterizer::IsDmaReadable(address,size);
    }
    ~GuestBufferAccess() override {
        // Retire while PageWatch and its callbacks still exist.
        try{DrainPriorityOperations();}catch(...){}
        try{ResolveGpuBuffers(0,UINT64_MAX);Finish();ReleaseCpuBufferWatches(0,UINT64_MAX);}catch(...){}
    }
};
}
