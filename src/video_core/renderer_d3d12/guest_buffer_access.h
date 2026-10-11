// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "rasterizer.h"
#include "page_watch.h"
#include "diagnostics.h"
namespace NativeDX12 {
// Production buffer/CPU access contract shared by GuestRasterizer and the
// native integration fixture. Image aliases are resolved by the guest cache.
class GuestBufferAccess : public Rasterizer {
protected:
    std::unique_ptr<PageWatch> page_watch_;
    std::function<bool(VAddr,u64)> has_backing_;
    virtual void ResolveImages(VAddr,u64) {}
    virtual void ReleaseImageWatches(VAddr,u64) {}
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
        if(writing){ReleaseCpuBufferWatches(address,size);ReleaseImageWatches(address,size);}
        Rasterizer::CheckBacking(address,size,writing);
    }
    void PrepareGpuBacking(VAddr address,u64 size,bool writing) override {
        // Unsupported buffer/image aliases use physical backing conversion.
        // This does not release buffer watches or resolve resident buffers.
        ResolveImages(address,size);
        if(writing)ReleaseImageWatches(address,size);
    }
    void SetBufferBackingAccess(std::function<bool(VAddr,u64)> has,
        std::function<bool(VAddr,std::span<const std::byte>)> write){
        has_backing_=std::move(has);
        if(!page_watch_)page_watch_=std::make_unique<PageWatch>([this](VAddr address){
            std::lock_guard lock(execution_mutex_);
            ResolveGpuBuffers(address,1);ReleaseCpuBufferWatches(address,1);
            ReleaseImageWatches(address,1);ResolveImages(address,1);
        });
        SetBufferOwnership([this](VAddr address,u64 size){
            if(address%4096||size%4096){Diagnostics::Count("buffer-own.unaligned");return false;}
            if(!has_backing_(address,size)){Diagnostics::Count("buffer-own.no-backing");return false;}
            ReleaseImageWatches(address,size);ResolveImages(address,size);
            const bool tracked=page_watch_->Track(address,size);
            Diagnostics::Count(tracked?"buffer-own.accepted":"buffer-own.track-conflict");
            Diagnostics::Count(tracked?"buffer-own.accepted-bytes":"buffer-own.rejected-bytes",size);
            return tracked;
        },std::move(write),[this](VAddr address,u64 size){page_watch_->Untrack(address,size);});
        SetBufferWriteWatch([this](VAddr address,u64 size){
            if(address%4096||size%4096||!has_backing_(address,size)||page_watch_->Contains(address))return false;
            return page_watch_->Track(address,size,true);
        });
        SetBufferWatchTransfer([this](VAddr address,u64 size,bool read_only){
            return has_backing_(address,size)&&page_watch_->ChangeMode(address,size,read_only);
        });
    }
public:
    using Rasterizer::Rasterizer;
    ~GuestBufferAccess() override {
        // Retire while PageWatch and its callbacks still exist.
        try{ResolveGpuBuffers(0,UINT64_MAX);Finish();ReleaseCpuBufferWatches(0,UINT64_MAX);}catch(...){}
    }
};
}
