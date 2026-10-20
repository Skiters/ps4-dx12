#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_rasterizer.h"
#include "shader_recompiler/native_ordered_color.h"
#include "video_core/amdgpu/liverpool.h"
#include "guest_graphics_state.h"
#include "guest_viewports.h"
#include "guest_point_capture.h"
#include "guest_indirect_arguments.h"
#include "guest_index_range.h"
#include "guest_index_expand.h"
#include "guest_indirect_pack.h"
#include "primitive_stream_arguments.h"
#include "guest_image_readback.h"
#include "guest_attachment_retirement.h"
#include "guest_image_operations.h"
#include "guest_image_ownership.h"
#include "guest_dma.h"
#include "guest_page_range.h"
#include "common/scope_exit.h"
#include "guest_color_clear.h"
#include "guest_raw_color_clear.h"
#include "diagnostics.h"
#include "display_capture.h"
#include "texture_resources.h"
#include <sstream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <map>
#ifdef SHADPS4_NATIVE_DX12_CORE
#include "core/memory.h"
#include "core/emulator_settings.h"
#endif
namespace NativeDX12 {
std::optional<u64> GuestRasterizer::ReplaceShader(u64 id,std::span<const u32> words,bool patched){
    std::lock_guard lock(execution_mutex_);Finish();return pipelines_.ReplaceShader(id,words,patched);
}
u64 GuestRasterizer::ClampMemoryRange(VAddr address,u64 bytes){
#ifdef SHADPS4_NATIVE_DX12_CORE
    return Core::Memory::Instance()->ClampRangeSize(address,bytes);
#else
    return Rasterizer::ClampMemoryRange(address,bytes);
#endif
}
namespace {
Shader::Backend::DX12::SpirvConvertOptions NativeOptions(const Shader::Profile& profile) {
    Shader::Backend::DX12::SpirvConvertOptions o;o.runtime_viewport_flips=true;
    o.shader_model_minor=6;o.compute_wave_size=profile.native_compute_wave_size?profile.native_compute_wave_size:32;o.native_storage_buffer_uavs=true;
    o.native_shader_clock_backend=profile.native_shader_clock_backend;return o;
}
bool Overlap(u64 a,u64 as,u64 b,u64 bs) {
    if(a>UINT64_MAX-as||b>UINT64_MAX-bs)throw std::out_of_range("Guest draw memory extent overflow");
    return a<b+bs&&b<a+as;
}
bool ImageOverlap(VAddr address,u64 size,const GuestTextureCache::Entry& image) {
    return Overlap(address,size,image.desc.address,image.layout.guest_bytes)||
        (image.desc.stencil_address&&Overlap(address,size,image.desc.stencil_address,image.stencil_layout.guest_bytes));
}
}
GuestRasterizer::GuestRasterizer(ID3D12Device* device,AmdGpu::Liverpool& gpu,
    const Shader::Profile& profile,std::wstring converter,std::wstring dxc)
    :GuestBufferAccess(device),device_(device),gpu_(gpu),frontend_(profile),shaders_(converter,NativeOptions(profile)),
     pipelines_(device,std::move(dxc),frontend_,shaders_),textures_(device,std::move(converter)) {
    native_alpha_factor_=profile.native_alpha_blend_factor;
    EnableHostSpanCache(true);
    textures_.SetRecordingListProvider([this]{return RecordingList();});
    textures_.SetCopySubmission([this](const auto& record){SubmitCopyCommands(record);});
    textures_.SetHostTextureCopy([this](auto* resource,void* mapped,auto slices,auto footprint,UINT rows,UINT64 row_bytes){
        QueueHostTextureCopy(resource,mapped,std::move(slices),footprint,rows,row_bytes);
    });
    textures_.SetHostBufferCopy([this](auto* resource,void* mapped,auto source,size_t bytes){
        QueueHostBufferCopy(resource,mapped,std::move(source),bytes);
    });
    textures_.SetPendingRetirement([this]{
        Finish();const auto fence=Flush();if(fence)textures_.Submitted(fence);
        RecordingList();
    });
    textures_.SetPressureWait([this]{
        Rasterizer::Finish();textures_.Collect(CompletedFence());RecordingList();
        Diagnostics::Count("image-download.pressure-budget-wait");return CompletedFence();
    });
    if(!profile.needs_buffer_offsets)
        throw std::invalid_argument("Native guest backend requires compiler buffer offset adjustment");
#ifdef SHADPS4_NATIVE_DX12_CORE
    const auto* pages=&Core::Memory::Instance()->GetAddressSpace().BackingPages();
    const auto has=[pages](VAddr address,u64 size){
        if(!size||address>=(1ull<<40)||size>(1ull<<40)-address)return false;
        for(auto current=address;current<address+size;){auto* entry=pages->find(current>>14);
            if(!entry||!*entry)return false;current+=std::min<u64>(16384-current%16384,address+size-current);}
        return true;
    };
    SetBackingAccess(has,[pages,has](VAddr address,std::span<std::byte> result){
        if(!has(address,result.size()))return false;
        while(!result.empty()){const auto amount=std::min<u64>(16384-address%16384,result.size());
            memcpy(result.data(),*pages->find(address>>14)+address%16384,amount);address+=amount;result=result.subspan(amount);}
        return true;
    },[this,pages,has](VAddr address,std::span<const std::byte> source){
        if(!has(address,source.size()))return false;
        gpu_.TraceBackingWrite(address,source.size());
        while(!source.empty()){const auto amount=std::min<u64>(16384-address%16384,source.size());
            memcpy(*pages->find(address>>14)+address%16384,source.data(),amount);address+=amount;source=source.subspan(amount);}
        return true;
    });
#endif
}
GuestRasterizer::~GuestRasterizer(){try{DrainPriorityOperations();}catch(...) {}
    try{ResolveGpuBuffers(0,UINT64_MAX);DiscardBufferImageVersions(0,UINT64_MAX,false);ResolveMemory(0,UINT64_MAX);Finish();}catch(...) {/* retire before shader/texture ownership ends */}}
void GuestRasterizer::SetBackingAccess(std::function<bool(VAddr,u64)> has,
    std::function<bool(VAddr,std::span<std::byte>)> read,std::function<bool(VAddr,std::span<const std::byte>)> write){
    std::lock_guard lock(execution_mutex_);
    read_backing_=std::move(read);
    write_backing_=[this,write=std::move(write)](VAddr address,std::span<const std::byte> bytes){
        if(page_watch_)page_watch_->InvalidateBackingEpoch(address,bytes.size());
        return write(address,bytes);
    };
    SetBufferBackingAccess(std::move(has),write_backing_);
    EnableBufferStreaming(true);
    EnableBufferArenas(true);
    textures_.SetImmutableWatch([this](auto& image){
        ReleaseCpuBufferWatches(image.desc.address,image.layout.guest_bytes);
        if(page_watch_&&has_backing_(image.desc.address,image.layout.guest_bytes)&&
            page_watch_->Track(image.desc.address,image.layout.guest_bytes,true))image.cpu_watched=true;
    },[this](auto& image){ReleaseImmutableWatch(image.desc.address,image.layout.guest_bytes);});
    textures_.SetOwnershipResolver([this](auto& image){
        ResolveImage(image);
        // Upload already received this command-list object. Resolution closes
        // and submits it; reopen it before Upload records further commands.
        RecordingList();
    });
    textures_.SetPressureRetirement([this](auto& image){RetireImageForPressure(image);RecordingList();});
    textures_.SetOwnershipAcquirer([this](auto& image){return OwnImage(image);});
    textures_.SetTransferSubmissionPublisher([this]{
        NotifyOnSubmission([this](u64 fence){textures_.Submitted(fence);});
    });
    textures_.SetBufferResidencyPublisher([this](auto ranges){
        std::vector<ShaderBufferRequest> requests;requests.reserve(ranges.size());
        for(const auto& [address,bytes]:ranges){
            if(!bytes||address>UINT64_MAX-bytes)throw std::out_of_range("Image B-source residency overflow");
            if(!PreparedDmaBuffer(address,bytes))
                throw std::logic_error("Prepared image B-source backing changed before its original bind");
            requests.push_back({address,bytes,false,true,true});
        }
        DmaMemory().ObserveResidentSharps(requests);
    });
    textures_.SetIndependentVersionTransfer([this](auto& image,VAddr address,u64 bytes){
        // Retile into a private GPU buffer seeded from the physical buffer
        // domain. This does not commit image pixels into that buffer domain.
        auto initial=CaptureShaderBuffer(image.desc.address,image.layout.guest_bytes,false);
        auto version=textures_.RetileBufferVersion(image,initial,RecordingList(),CompletedFence());
        if(address!=image.desc.address||bytes!=image.layout.guest_bytes){
            auto requested=CaptureShaderBuffer(address,bytes,false);
            const auto begin=std::max(address,image.desc.address);
            const auto end=std::min(address+bytes,image.desc.address+image.layout.guest_bytes);
            if(begin>=end)throw std::logic_error("Independent image transfer lost its overlap");
            version.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
            requested.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
            DX12_API_CALL(RecordingList()->CopyBufferRegion(requested.handle.Get(),begin-address,
                version.handle.Get(),begin-image.desc.address,end-begin));
            version=std::move(requested);
        }
        version.Transition(RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return version;
    },[this](auto& image){return OwnIndependentImage(image);},
      [this](auto& image){ReleaseIndependentImageWatches(image);});
    textures_.SetIndependentOwnershipTransfer([this](auto& image,auto& replaced){
        return OwnIndependentImage(image,&replaced);
    });
    const auto image_transfer=[this](auto& image,const auto& desc,const auto& layout,bool cached){
        const bool seed_resident=desc.samples==1&&(desc.levels>image.desc.levels||desc.layers>image.desc.layers)&&
            IsGpuBufferModified(desc.address,layout.guest_bytes);
        const auto capture=[this](u64 address,u64 bytes){
                ShaderBufferRequest request{address,bytes,false,true,true};request.publish_dma_residency=false;
                if(textures_.IsPlannedImageBind())ValidatePreparedShaderBufferRange(address,bytes);
                // Refresh current CPU/store-marker contents within the already
                // reserved envelope; a late snapshot must not reuse stale B.
                PreparePhysicalDmaBuffers(std::span(&request,1));
                return CapturePreparedShaderBuffer(address,bytes);
            };
        auto captured=cached?textures_.CaptureCachedDepthRecreationVersion(image,desc,layout,RecordingList(),CompletedFence(),capture):
            textures_.CaptureIndependentImageVersion(image,desc,layout,RecordingList(),CompletedFence(),capture);
        // Pinned single-sample growth RefreshImage obtains target B before I is
        // copied. Preserve that additional EnsureResident effect until bind;
        // private allocation/capture alone must not publish it early.
        if(seed_resident)captured.seed_residency.emplace_back(desc.address,layout.guest_bytes);
        return captured;
    };
    textures_.SetIndependentImageTransfer([image_transfer](auto& image,const auto& desc,const auto& layout){
        return image_transfer(image,desc,layout,false);});
    textures_.SetCachedDepthRecreationTransfer([image_transfer](auto& image,const auto& desc,const auto& layout){
        return image_transfer(image,desc,layout,true);});
    textures_.SetImageBufferSources([this](u64 address,u64 bytes){
        ShaderBufferRequest request{address,bytes,false,true,true};request.publish_dma_residency=false;
        PreparePhysicalDmaBuffers(std::span(&request,1));
    },[this](u64 address,u64 bytes){
        ValidatePreparedShaderBufferRange(address,bytes);
        const bool resident=IsGpuBufferModified(address,bytes);
        ShaderBufferRequest request{address,bytes,false,true,true};request.publish_dma_residency=false;
        PreparePhysicalDmaBuffers(std::span(&request,1));
        return GuestTextureCache::BufferImageSource{CapturePreparedShaderBuffer(address,bytes),resident};
    });
    textures_.SetIndependentBlockExpansion([this](auto& image,const auto& desc,const auto& layout){
        // Pinned ObtainBufferForImage chooses B only for GPU-modified target
        // pages. A resident arena alone is not proof that B is the refresh seed.
        Resource seed;GuestTextureCache::BufferResidency residency;
        if(IsGpuBufferModified(desc.address,layout.guest_bytes)){
            seed=CaptureShaderBuffer(desc.address,layout.guest_bytes,false);
            residency.emplace_back(desc.address,layout.guest_bytes);
        }
        else{
            std::vector<std::byte> bytes(size_t(layout.guest_bytes));
            if(!read_backing_(desc.address,bytes))throw std::runtime_error("BC refresh lost physical backing");
            seed=Buffer(device_.Get(),layout.guest_bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
            auto upload=Buffer(device_.Get(),layout.guest_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            void* mapped{};D3D12_RANGE empty{};Check(upload.handle->Map(0,&empty,&mapped));
            std::memcpy(mapped,bytes.data(),bytes.size());upload.handle->Unmap(0,nullptr);
            RetainUntilFinish([seed,upload](u64){(void)seed;(void)upload;});
            DX12_API_CALL(RecordingList()->CopyBufferRegion(seed.handle.Get(),0,upload.handle.Get(),0,layout.guest_bytes));
        }
        // Retile consumes the same R prefix, overwrites only logical source I,
        // then merges it back. Target-only mips and newly visible padding stay R.
        auto prefix=textures_.RetileBufferVersion(image,seed,RecordingList(),CompletedFence());
        prefix.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        seed.Transition(RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        DX12_API_CALL(RecordingList()->CopyBufferRegion(seed.handle.Get(),0,prefix.handle.Get(),0,image.layout.guest_bytes));
        seed.Transition(RecordingList(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        return GuestTextureCache::BlockExpansion{std::move(seed),std::move(residency)};
    });
    textures_.SetImageWriteNotifier([this](auto& image){QueueLinearImageDownload(image);});
    const auto retain_image=[this](const auto& desc,const auto& layout){
        const bool depth=IsIndependentGuestDepthStorage(desc,layout);
        return page_watch_&&!desc.render_target&&(!desc.depth_target||depth)&&!desc.stencil_address&&
            !desc.one_dimensional&&(desc.samples==1||
                GuestTextureCache::CanTransferIndependentMultisample(desc,layout))&&
            (IsNativeReadonlyGuestBlockStorage(desc,layout)||
             ((desc.data_format==6||desc.data_format==34)&&desc.number_format==7)||
             ((desc.data_format==9)&&(desc.number_format==0||desc.number_format==1||desc.number_format==4||desc.number_format==5))||
             ((desc.data_format>=16&&desc.data_format<=19)&&desc.number_format==0)||
             ((desc.data_format==4||desc.data_format==11||desc.data_format==13||desc.data_format==14)&&
              (desc.number_format==4||desc.number_format==5||desc.number_format==7))||
             ((desc.data_format==1||desc.data_format==3||desc.data_format==10)&&
              (desc.number_format==0||desc.number_format==1||desc.number_format==4||desc.number_format==5||
               (desc.data_format==10&&desc.number_format==9)))||
             ((desc.data_format==2||desc.data_format==5||desc.data_format==12)&&
              (desc.number_format==0||desc.number_format==1||desc.number_format==4||desc.number_format==5||desc.number_format==7)))&&
            !layout.expanded_bytes_per_pixel&&GuestProtectionPages(desc.address,layout.guest_bytes)&&
            layout.guest_bytes<=64*1024*1024&&has_backing_(desc.address,layout.guest_bytes);
    };
    const auto readonly_word=[this](const auto& desc,const auto& layout){
        return !desc.writable&&layout.HostBytesPerPixel()==4&&
            textures_.CanRetainReadonlyWordStorage(desc);
    };
    textures_.SetBufferAliasRetention([retain_image,readonly_word](const auto& desc,const auto& layout){
        return (desc.writable||readonly_word(desc,layout))&&retain_image(desc,layout);
    });
    textures_.SetIndependentImageRetention([retain_image,readonly_word](const auto& desc,const auto& layout){
        return (desc.writable||desc.data_format==13||IsNativeReadonlyGuestBlockStorage(desc,layout)||
            IsIndependentGuestDepthStorage(desc,layout)||readonly_word(desc,layout))&&
            retain_image(desc,layout);
    });
}
bool GuestRasterizer::OwnImage(GuestTextureCache::Entry& image){
    if(image.gpu_owned&&image.buffer_alias_version)return true;
    if(HasDmaReadOverlap(image.desc.address,image.layout.guest_bytes)||
       (image.desc.stencil_address&&HasDmaReadOverlap(image.desc.stencil_address,image.stencil_layout.guest_bytes))){
        // A bound ReadConst bank continues to read B while this attachment
        // writes I. Do not replace B's authority with exclusive image pages.
        if(OwnIndependentImage(image))return true;
        // Split stencil/MSAA depth and other unadapted transfer domains retain
        // their existing ownership path until that port is connected.
    }
    // A newly published DMA B can overlap an attachment that was already
    // exclusively owned. Reconsider that contract before the warm-owner fast
    // path; otherwise its later resolution would publish I over the separate B.
    if(image.gpu_owned)return true;
    if(image.buffer_alias_version){
        return OwnIndependentImage(image);
    }
    // Ownership is a coherence rule, not a size optimization. A 640x360
    // color target is below 1 MiB; eagerly downloading it after every draw
    // defeats the low-resolution path. Track still rejects partial pages,
    // overlapping ranges and incompatible memory protection.
    if(!page_watch_||!has_backing_(image.desc.address,image.layout.guest_bytes)||
       (image.desc.stencil_address&&!has_backing_(image.desc.stencil_address,image.stencil_layout.guest_bytes)))return false;
    ResolveGpuBuffers(image.desc.address,image.layout.guest_bytes);
    if(image.desc.stencil_address)ResolveGpuBuffers(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    ReleaseImmutableWatch(image.desc.address,image.layout.guest_bytes);
    if(image.desc.stencil_address)ReleaseImmutableWatch(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    if(!ApiTrace::Call("MEMORY","GpuImagePageOwnership",[&]{return page_watch_->Track(image.desc.address,image.layout.guest_bytes);},image.desc.address,image.layout.guest_bytes))return false;
    try {
        if(image.desc.stencil_address&&!page_watch_->Track(image.desc.stencil_address,image.stencil_layout.guest_bytes)){
            page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);return false;
        }
    }catch(...){page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);throw;}
    image.SetGpuOwned(true);return true;
}
bool GuestRasterizer::OwnIndependentImage(GuestTextureCache::Entry& image,GuestTextureCache::Entry* replaced){
    // A cached image can keep a version distinct from its physical/resident
    // buffer, including a read-only descriptor or ordinary owned attachment.
    const auto owner=DescribeGuestImageOwnership(image);
    if(!owner||!CanRetainGuestImageOwnership(textures_,*owner))return false;
    // Repeated image uses validate the physical owner, including a retained
    // read-only descriptor, without changing an overlapping buffer's guard.
    if(image.buffer_alias_version){image.SetGpuOwned(true);WatchBufferImageVersions(image.desc.address,image.layout.guest_bytes);return true;}
    if(!page_watch_)return false;
    if(!AcquireGuestImageOwnership(textures_,image,*page_watch_,has_backing_,
        [this](u64 address,u64 bytes){return GpuOwnedBufferProtectionRanges(address,bytes);},replaced)){
        if(image.gpu_owned)throw std::runtime_error("Cannot transfer independent image plane ownership to read-only protection");
        return false;
    }
    // The image equality witness ends, but protection stays in place. CPU
    // buffer witnesses still describe their unchanged physical/GPU version.
    WatchBufferImageVersions(image.desc.address,image.layout.guest_bytes);
    return true;
}
bool GuestRasterizer::OwnImageClear(GuestTextureCache::Entry& image){
    // Pinned HLE clears happen before ordinary buffer binding and alone do not
    // enqueue the legacy FindTexture/FindRenderTarget download.
    if(!OwnIndependentImage(image))return false;
    image.NoteLogicalImageWrite();
    Diagnostics::Count("image-clear.independent-ownership");return true;
}
void GuestRasterizer::QueueLinearImageDownload(GuestTextureCache::Entry& image){
#ifdef SHADPS4_NATIVE_DX12_CORE
    if(!EmulatorSettings.IsReadbackLinearImagesEnabled())return;
    if(image.gpu_owned&&image.desc.address&&
       (image.desc.tile_mode==8||image.desc.width<=8)&&image.desc.samples==1&&
       !image.desc.depth_target&&!image.desc.stencil_address&&image.layout.block_extent==1&&
       image.texture.handle&&image.texture.handle->GetDesc().SampleDesc.Count==1&&
       (!image.layout.expanded_bytes_per_pixel||
        (HasExactPackedExpansion(image.desc,image.layout)&&image.expanded_guest_backing.handle&&image.expanded_written_mask.handle))&&
       (image.buffer_alias_version||OwnIndependentImage(image)))
        image.linear_download_pending=true;
#endif
}
void GuestRasterizer::DownloadIndependentImage(GuestTextureCache::Entry& image){
    GuestTextureCache::Entry* selected[]{&image};
    DownloadIndependentImages(selected);
}
void GuestRasterizer::DownloadIndependentImages(std::span<GuestTextureCache::Entry*> images){
    if(images.empty())return;
    // Settle preceding physical-buffer publications once, then record the
    // independent image copies together. Their CPU visibility boundary is
    // still this call: no guest fence/label can pass an unfinished download.
    Rasterizer::Finish();
    struct Download {GuestTextureCache::Entry* image;std::unique_ptr<GuestLinearImageReadback> readback;};
    std::vector<Download> pending;
    constexpr u64 budget=64*1024*1024;
    u64 retained{};
    const auto complete=[&]{
        if(pending.empty())return;
        const auto fence=Flush();textures_.Submitted(fence);
        for(auto& download:pending)download.readback->Submitted(fence);
        Rasterizer::Finish();
        // Keep cache iteration order, including overlapping compact spans.
        // Seed each result after the preceding commit so padding and physical
        // buffer witnesses retain exactly the former sequential semantics.
        for(auto& [image,readback]:pending){
            std::vector<std::byte> bytes(size_t(readback->Bytes()));
            if(!read_backing_(image->desc.address,bytes))
                throw std::runtime_error("Linear image download lost its physical backing");
            readback->Resolve(CompletedFence(),bytes);
            CommitImageBacking(image->desc.address,bytes,read_backing_,write_backing_);
            image->linear_download_pending=false;
            Diagnostics::Count("image-download.linear-independent");
        }
        pending.clear();retained=0;
    };
    for(auto* image:images){
        auto readback=std::make_unique<GuestLinearImageReadback>(device_.Get(),*image,RecordingList());
        // A single download already has its own size checks. Bound additional
        // batching to one budget plus that final individual allocation.
        retained=std::min(budget,retained+std::min(budget,readback->AllocationBytes()));
        pending.push_back({image,std::move(readback)});
        if(retained==budget)complete();
    }
    complete();
}
void GuestRasterizer::ProcessDownloadImages(){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    std::vector<GuestTextureCache::Entry*> pending;
    for(auto& image:textures_.Entries())if(image->linear_download_pending){
        if(!image->gpu_owned||!image->buffer_alias_version){image->linear_download_pending=false;continue;}
        // Submission/fence downloads expose physical CPU bytes while the
        // image and the preceding resident buffer keep their own versions.
        pending.push_back(image.get());
    }
    DownloadIndependentImages(pending);
}
void GuestRasterizer::ReleaseIndependentImageWatches(GuestTextureCache::Entry& image){
    ReleaseGuestImageOwnership(textures_,image,[this](u64 address,u64 bytes,const auto& retained){
        ReleaseUnusedReadOnlyWatches(address,bytes,retained);});
}
void GuestRasterizer::ReleaseImageProtection(GuestTextureCache::Entry& image){
    const auto retained=ImageProtectionRanges(&image);
    for(const auto& plane:image.ProtectionPlanes())if(plane.bytes)
        ReleaseUnusedReadOnlyWatches(plane.address,plane.bytes,retained);
}
std::vector<std::pair<VAddr,u64>> GuestRasterizer::ImageProtectionRanges(const GuestTextureCache::Entry* exclude)const{
    return GuestImageProtectionRanges(textures_,exclude);
}
void GuestRasterizer::RememberImageBoundary(GuestTextureCache::Entry& image,VAddr address,u64 size){
    if(!image.gpu_owned||!image.buffer_alias_version)return;
    for(const auto& plane:image.ProtectionPlanes())if(plane.bytes&&
        RememberGuestImageBoundary(*plane.saved,plane.address,plane.bytes,address,size,read_backing_)){
            image.buffer_alias_watch=false;image_watch_pending_=true;
        }
}
void GuestRasterizer::ValidateIndependentImageWatches(){
    std::lock_guard lock(execution_mutex_);
    if(!image_watch_pending_||validating_image_watches_)return;
    validating_image_watches_=true;
    SCOPE_EXIT {validating_image_watches_=false;};
    std::vector<GuestTextureCache::Entry*> dirty;
    std::vector<VAddr> changed_pages;
    for(auto& image:textures_.Entries())if(image->HasUnprotectedCpuPages()){
        if(!image->gpu_owned||!image->buffer_alias_version){
            image->unprotected_cpu_pages.clear();image->stencil_unprotected_cpu_pages.clear();continue;}
        const auto changed=ValidateGuestImageBoundaries(image->ProtectionPlanes(),
            read_backing_,[this](u64 page,u64 size){return page_watch_->TrackReadOnly(page,size,true);});
        image->buffer_alias_watch=true;
        if(!changed.empty()){
            changed_pages.insert(changed_pages.end(),changed.begin(),changed.end());
            image->MarkLogicalDirty();image->linear_download_pending=false;dirty.push_back(image.get());
        }
    }
    image_watch_pending_=false;
    // A lost image witness also ends every clean CPU buffer proof on that
    // physical page. Record CPU winners before Finish can reconcile GPU B.
    std::sort(changed_pages.begin(),changed_pages.end());
    changed_pages.erase(std::unique(changed_pages.begin(),changed_pages.end()),changed_pages.end());
    for(const auto page:changed_pages)InvalidateCpuBufferPages(page,4096);
    if(!dirty.empty())Finish();
    for(auto* image:dirty)ReleaseIndependentImageWatches(*image);
    // Buffer proof retirement can release a shared page; restore all surviving
    // independent images after the dirty versions have relinquished ownership.
    if(!dirty.empty())WatchBufferImageVersions(0,UINT64_MAX);
}
void GuestRasterizer::RetireImageForPressure(GuestTextureCache::Entry& image){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    if(!image.gpu_owned)return;
    if(!image.buffer_alias_version||image.desc.tile_mode!=8||image.desc.samples!=1||
       (image.layout.expanded_bytes_per_pixel&&!HasExactPackedExpansion(image.desc,image.layout))||
       (image.desc.depth_target&&!IsGuestDepthPlaneStorage(image.desc,image.layout))||
       (image.desc.stencil_address&&!image.desc.depth_target)){
        ResolveImage(image);return;
    }
    auto source=textures_.RetainPressureSource(image);
    auto readback=std::make_shared<GuestLinearImageReadback>(device_.Get(),image,RecordingList());
    const auto fence=Flush();textures_.Submitted(fence);readback->Submitted(fence);
    // The cache entry may disappear immediately, but the placed source and its
    // residency account survive until the copy fence. Ordinary collection can
    // free GPU storage while the priority worker waits for the execution mutex.
    RetainUntilFence(fence,[readback,lease=std::move(source.lease)](u64 completed){readback->ReleaseSource(completed);});
    if(source.bytes>UINT64_MAX-readback->AllocationBytes())throw std::overflow_error("Pressure download allocation overflow");
    DeferImageBacking(fence,image.desc.address,readback->Bytes(),source.bytes+readback->AllocationBytes(),
        read_backing_,write_backing_,[readback](u64 completed,std::span<std::byte> bytes){
            readback->ReleaseSource(completed);readback->Resolve(completed,bytes);});
    ReleaseIndependentImageWatches(image);
    Diagnostics::Count("image-download.priority-pressure-retired");
}
void GuestRasterizer::OnSubmit(){
    std::lock_guard lock(execution_mutex_);ProcessDownloadImages();Rasterizer::OnSubmit();
}
void GuestRasterizer::OnFence(){
    std::lock_guard lock(execution_mutex_);ProcessDownloadImages();Rasterizer::OnFence();
}
void GuestRasterizer::ResolveImage(GuestTextureCache::Entry& image){
    std::lock_guard lock(execution_mutex_);ValidateIndependentImageWatches();if(!image.gpu_owned)return;
    Diagnostics::Scope resolve_diagnostic(Diagnostics::Kind::Readback,"resolve-owned-image");
    resolve_diagnostic.Detail("address=%llx bytes=%llu size=%ux%u levels=%u layers=%u tile=%u independent=%u",
        (unsigned long long)image.desc.address,(unsigned long long)image.layout.guest_bytes,
        image.desc.width,image.desc.height,image.desc.levels,image.desc.layers,image.desc.tile_mode,
        unsigned(image.buffer_alias_version));
    if(image.buffer_alias_version){
        if((image.desc.tile_mode!=8&&image.desc.width>8)||image.desc.samples!=1||
           (image.layout.expanded_bytes_per_pixel&&!HasExactPackedExpansion(image.desc,image.layout))||
           (image.desc.depth_target&&!IsGuestDepthPlaneStorage(image.desc,image.layout))||
           (image.desc.stencil_address&&!image.desc.depth_target))
            throw std::runtime_error("Independent buffer/image version needs a domain-preserving format/pressure resolver");
        resolve_diagnostic.Stage("independent-linear-download");
        DownloadIndependentImage(image);
        ReleaseIndependentImageWatches(image);
        return;
    }
    ApiTrace::Call("MEMORY","ResolveGpuOwnedImage",[&]{
        resolve_diagnostic.Stage("full-retirement-wait");
        Rasterizer::Finish();
        std::vector<std::byte> result(size_t(image.layout.guest_bytes));
        if(!read_backing_(image.desc.address,result))throw std::runtime_error("GPU-owned image backing was removed before retirement");
        std::vector<std::byte> stencil;
        if(image.desc.stencil_address){stencil.resize(size_t(image.stencil_layout.guest_bytes));
            if(!read_backing_(image.desc.stencil_address,stencil))throw std::runtime_error("GPU-owned stencil backing was removed before retirement");}
        resolve_diagnostic.Stage("full-retirement-copy");
        GuestImageReadback readback(device_.Get(),image,RecordingList());
        const auto fence=Flush();textures_.Submitted(fence);readback.Submitted(fence);Rasterizer::Finish();
        resolve_diagnostic.Stage("full-retirement-retile");
        readback.Resolve(CompletedFence(),result,stencil);
        resolve_diagnostic.Stage("full-retirement-commit");
        if(!write_backing_(image.desc.address,result))throw std::runtime_error("Cannot commit GPU-owned image through physical backing");
        if(image.desc.stencil_address&&!write_backing_(image.desc.stencil_address,stencil))throw std::runtime_error("Cannot commit GPU-owned stencil through physical backing");
        if(!image.desc.writable)image.RememberSnapshot(result,stencil);image.SetGpuOwned(false);
        page_watch_->Untrack(image.desc.address,image.layout.guest_bytes);
        if(image.desc.stencil_address)page_watch_->Untrack(image.desc.stencil_address,image.stencil_layout.guest_bytes);
    },image.desc.address,image.layout.guest_bytes);
}
void GuestRasterizer::ResolveMemory(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    for(auto& image:textures_.Entries())if(image->gpu_owned&&!image->buffer_alias_version&&ImageOverlap(address,size,*image))ResolveImage(*image);
}
void GuestRasterizer::DiscardBufferImageVersions(VAddr address,u64 size,bool gpu_write, std::source_location origin){
    std::lock_guard lock(execution_mutex_);std::vector<GuestTextureCache::Entry*> versions;
    if(!size)return;
    for(auto& image:textures_.Entries())if(ImageOverlap(address,size,*image)&&
        (!gpu_write||image->desc.address==address)){
        // Pinned CPU/GPU dirty invalidation retains GpuModified even after the
        // old version has lost ownership. A later readonly refresh can validate
        // that same logical image again; pressure retirement alone cannot.
        image->MarkLogicalDirty(origin);
        image->linear_download_pending=false;
        if(image->gpu_owned&&image->buffer_alias_version)versions.push_back(image.get());
    }
    // CPU writes retire the old image's uses before invalidating it. A GPU
    // buffer write instead preserves the preceding descriptors/allocations
    // through their existing leases and invalidates the logical version only.
    if(!gpu_write&&!versions.empty())Finish();
    for(auto* image:versions){
        if(image->buffer_alias_watch){
            for(const auto& plane:image->ProtectionPlanes())if(plane.bytes)
                ReleaseCpuBufferWatches(plane.address,plane.bytes);
        }
        // Retain other image witnesses as well as stronger buffer guards.
        // A whole-range read-only untrack would silently unprotect a sibling
        // sampled view sharing these pages.
        ReleaseIndependentImageWatches(*image);
    }
    WatchBufferImageVersions(address,size);
    if(!gpu_write){
        std::vector<VAddr> released_pages;
        for(auto& image:textures_.Entries()){
            RememberImageBoundary(*image,address,size);
            for(const auto& plane:image->ProtectionPlanes())if(plane.bytes)
                for(const auto& [page,saved]:*plane.saved)
                    if(ShareGuestProtectionPages(address,size,page,4096))released_pages.push_back(page);
        }
        // The store that raised the fault must be allowed to finish. Images
        // sharing only its page now carry bounded, full-byte B witnesses.
        std::sort(released_pages.begin(),released_pages.end());
        released_pages.erase(std::unique(released_pages.begin(),released_pages.end()),released_pages.end());
        for(const auto page:released_pages){
            InvalidateCpuBufferPages(page,4096);
            ReleaseImageWatches(page,4096);
            if(!page_watch_->UntrackReadOnly(page,4096,true))
                throw std::runtime_error("Cannot release a CPU image write page");
        }
    }
}
void GuestRasterizer::WatchBufferImageVersions(VAddr address,u64 size){
    for(auto& image:textures_.Entries())if(image->gpu_owned&&image->buffer_alias_version){
        const auto planes=image->ProtectionPlanes();
        if(std::ranges::none_of(planes,[&](const auto& plane){return plane.bytes&&
            ShareGuestProtectionPages(address,size,plane.address,plane.bytes);}))continue;
        for(const auto& plane:planes)if(plane.bytes){
            if(!page_watch_||!has_backing_(plane.address,plane.bytes))
                throw std::runtime_error("Independent image plane protection lost backing");
            VisitGuestImageProtectedRanges(plane,[&](u64 begin,u64 bytes){
                if(!page_watch_->TrackReadOnly(begin,bytes,true))
                    throw std::runtime_error("Cannot protect an independent image plane against CPU writes: address="+
                        std::to_string(plane.address)+" bytes="+std::to_string(plane.bytes)+
                        " uid="+std::to_string(image->image_uid));
            });
        }
        image->buffer_alias_watch=!image->HasUnprotectedCpuPages();
    }
}
void GuestRasterizer::ReleaseBufferPageWatch(VAddr address,u64 size){
    const auto retained=ImageProtectionRanges();
    // Transfer a strong B guard to surviving I witnesses without restoring RW
    // between Untrack and Track. Read-only partial proofs may already have holes.
    if(!page_watch_->ChangeMode(address,size,true)){
        const auto pages=GuestProtectionPages(address,size);
        if(!pages)throw std::runtime_error("Invalid shared buffer protection retirement");
        for(auto page=pages->address;page<pages->address+pages->size;){
            MEMORY_BASIC_INFORMATION region{};
            if(!VirtualQuery(reinterpret_cast<void*>(page),&region,sizeof(region))||region.Protect==PAGE_NOACCESS)
                throw std::runtime_error("Cannot demote a shared GPU buffer protection range");
            page=std::min(pages->address+pages->size,
                u64(reinterpret_cast<uintptr_t>(region.BaseAddress))+region.RegionSize);
        }
    }
    ReleaseUnusedReadOnlyWatches(address,size,retained,address);
    WatchBufferImageVersions(address,size);
}
void GuestRasterizer::ResolveCpuFault(VAddr address,bool writing){
    ValidateIndependentImageWatches();
    SCOPE_EXIT {
        MEMORY_BASIC_INFORMATION region{};
        if(writing&&std::getenv("SHADPS4_PAGE_FAULT_TRACE")&&
           VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region))&&region.Protect==PAGE_READONLY){
            static std::atomic<unsigned> reports{};
            if(reports.fetch_add(1)<16){
                fprintf(stderr,"DX12 CPU fault retained owners address=%llx strong_buffers=%zu\n",
                    (unsigned long long)address,GpuOwnedBufferProtectionRanges(address,1).size());
                for(const auto& image:textures_.Entries())for(const auto& plane:image->ProtectionPlanes())
                    if(plane.bytes&&ShareGuestProtectionPages(address,1,plane.address,plane.bytes))
                        fprintf(stderr,"DX12 CPU fault image uid=%llu plane=%llx+%llu gpu=%u alias=%u cpu_watch=%u dirty=%u boundary=%u origin=%s\n",
                            (unsigned long long)image->image_uid,(unsigned long long)plane.address,
                            (unsigned long long)plane.bytes,unsigned(image->gpu_owned),unsigned(image->buffer_alias_version),
                            unsigned(image->cpu_watched),unsigned(image->logical_dirty),unsigned(image->HasUnprotectedCpuPages()),
                            image->logical_dirty_origin);
                fflush(stderr);
            }
        }
    };
    // The faulting instruction has not executed its store yet. A nested image
    // resolver/Finish must not certify and rearm its new boundary snapshot
    // before the VEH returns; validation resumes at the next access/command.
    const auto validating=std::exchange(validating_image_watches_,true);
    SCOPE_EXIT {validating_image_watches_=validating;};
    const bool separate=std::ranges::any_of(textures_.Entries(),[address](const auto& image){
        if(!image->gpu_owned||!image->buffer_alias_version)return false;
        return std::ranges::any_of(image->ProtectionPlanes(),[address](const auto& plane){return plane.bytes&&
            ShareGuestProtectionPages(address,1,plane.address,plane.bytes);});});
    if(!separate){
        if(writing)DiscardBufferImageVersions(address,1,false);
        GuestBufferAccess::ResolveCpuFault(address,writing);return;
    }
    if(!writing||IsGpuBufferModified(address,1))Rasterizer::ReadMemory(address,1);
    else ResolveGpuBuffers(address,1);
    if(writing){DiscardBufferImageVersions(address,1,false);InvalidateCpuBufferPages(address,1);ReleaseImageWatches(address,1);}
    ResolveMemory(address,1);if(!writing)WatchBufferImageVersions(address,1);
}
void GuestRasterizer::PrepareGpuBacking(VAddr address,u64 size,bool writing){
    ValidateIndependentImageWatches();
    GuestBufferAccess::PrepareGpuBacking(address,size,writing);
}
void GuestRasterizer::PreserveImagesForGpuBuffer(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    for(auto& image:textures_.Entries()){
        if(!image->gpu_owned||image->buffer_alias_version||!ImageOverlap(address,size,*image))continue;
        // Original raw buffer binding does not retire or dirty a valid I just
        // because a later sharp overlaps its pages. Transfer this same entry's
        // protection to independent I/B before acquiring the buffer guard;
        // no guest bytes, metadata, original GpuModified or native allocation
        // are changed. PageWatch preserves any stronger existing B owner.
        if(image->HasValidLogicalImage()&&OwnIndependentImage(*image))continue;
        // Unadapted physical domains keep their existing explicit resolver.
        ResolveImage(*image);
    }
}
void GuestRasterizer::NotifyGpuBufferWrite(VAddr address,u64 size){
    DiscardBufferImageVersions(address,size,true);
}
void GuestRasterizer::CheckBacking(VAddr address,u64 size,bool writing){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    const auto validating=std::exchange(validating_image_watches_,true);
    SCOPE_EXIT {validating_image_watches_=validating;};
    if(writing&&Diagnostics::Enabled()){
        static std::atomic<unsigned> write_callers{};
        for(const auto& image:textures_.Entries())if(image->gpu_owned&&ImageOverlap(address,size,*image)){
            if(write_callers.fetch_add(1)<32){
                fprintf(stderr,"DX12 image backing write caller=%p address=%llx bytes=%llu image=%llx image_bytes=%llu\n",
                    __builtin_return_address(0),(unsigned long long)address,(unsigned long long)size,
                    (unsigned long long)image->desc.address,(unsigned long long)image->layout.guest_bytes);
                fflush(stderr);
            }
            break;
        }
    }
    if(writing)ResolveGpuBuffers(address,size);
    if(writing)DiscardBufferImageVersions(address,size,false);
    GuestBufferAccess::CheckBacking(address,size,writing);
}
bool GuestRasterizer::HasGpuImageOverlap(VAddr address,u64 size)const {
    return textures_.HasGuestRangeOverlap(address,size,true);
}
void GuestRasterizer::ReleaseImmutableWatch(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    ReleaseCpuBufferWatches(address,size);
    ReleaseImageWatches(address,size);
}
void GuestRasterizer::ReleaseImageWatches(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    for(auto& image:textures_.Entries())if(image->cpu_watched&&ShareGuestProtectionPages(address,size,image->desc.address,image->layout.guest_bytes)){
        image->cpu_watched=false;ReleaseImageProtection(*image);
    }
}
void GuestRasterizer::DiscardImages(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);
    const bool overlaps=std::any_of(textures_.Entries().begin(),textures_.Entries().end(),[&](const auto& image){
        return ImageOverlap(address,size,*image);});
    if(!overlaps)return;
    Rasterizer::Finish();
    for(auto& image:textures_.Entries())if(image->gpu_owned&&ImageOverlap(address,size,*image)){
        image->SetGpuOwned(false);page_watch_->Untrack(image->desc.address,image->layout.guest_bytes);
        if(image->desc.stencil_address)page_watch_->Untrack(image->desc.stencil_address,image->stencil_layout.guest_bytes);}
    textures_.Invalidate(address,size,CompletedFence());
}
bool GuestRasterizer::PrepareCpuUpload(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);ValidateIndependentImageWatches();ResolveMemory(address,size);
    const bool readable=Rasterizer::PrepareCpuUpload(address,size);if(readable)WatchBufferImageVersions(address,size);return readable;
}
bool GuestRasterizer::ReadMemory(VAddr address,u64 size,bool assume){
    std::lock_guard lock(execution_mutex_);ValidateIndependentImageWatches();ResolveMemory(address,size);
    const bool readable=Rasterizer::ReadMemory(address,size,assume);if(readable)WatchBufferImageVersions(address,size);return readable;
}
bool GuestRasterizer::InvalidateMemory(VAddr address,u64 size,bool assume){
    std::lock_guard lock(execution_mutex_);ValidateIndependentImageWatches();
    const auto validating=std::exchange(validating_image_watches_,true);
    SCOPE_EXIT {validating_image_watches_=validating;};
    DiscardBufferImageVersions(address,size,false);
    InvalidateCpuBufferPages(address,size);ReleaseImageWatches(address,size);
    // A real CPU access has already resolved the guard before the write. Keep
    // the independent fresh-file fast path when no image owns these pages.
    if(page_watch_&&page_watch_->Contains(address))ResolveMemory(address,size);
    return Rasterizer::InvalidateMemory(address,size,assume);
}
void GuestRasterizer::NotifyCpuWrite(VAddr address,u64 size){
    std::lock_guard lock(execution_mutex_);DiscardBufferImageVersions(address,size,false);Rasterizer::NotifyCpuWrite(address,size);
}
void GuestRasterizer::RegisterMemory(VAddr address,u64 size){DiscardBufferImageVersions(address,size,false);ReleaseImmutableWatch(address,size);DiscardImages(address,size);Rasterizer::RegisterMemory(address,size);}
void GuestRasterizer::UnmapMemory(VAddr address,u64 size){std::lock_guard lock(execution_mutex_);ResolveGpuBuffers(address,size);DiscardBufferImageVersions(address,size,false);ReleaseImmutableWatch(address,size);textures_.RemoveMeta(address,size);ResolveMemory(address,size);DiscardImages(address,size);Rasterizer::UnmapMemory(address,size);}
void GuestRasterizer::FillBuffer(VAddr address,u32 size,u32 value,bool gds){
    std::lock_guard lock(execution_mutex_);
    Rasterizer::FillBuffer(address,size,value,gds);
    if(!gds&&size)textures_.ClearMeta(address);
}
void GuestRasterizer::PrepareMemoryProtection(VAddr address,u64 size){ResolveGpuBuffers(address,size);DiscardBufferImageVersions(address,size,false);ReleaseImmutableWatch(address,size);ResolveMemory(address,size);DiscardImages(address,size);}
Resource* GuestRasterizer::CoherentDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch){
    std::lock_guard lock(execution_mutex_);ValidateIndependentImageWatches();Finish();
    return textures_.CoherentDisplayTexture(address,width,height,pitch,CompletedFence());
}
Resource* GuestRasterizer::PrepareDisplayTexture(VAddr address,UINT width,UINT height,UINT pitch,
                                                  ID3D12CommandQueue* consumer){
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    const auto submitted=Flush();
    if(submitted)textures_.Submitted(submitted);
    textures_.Collect(CompletedFence());
    auto* texture=textures_.CoherentDisplayTexture(address,width,height,pitch,
        submitted?submitted:CompletedFence());
    if(texture&&submitted)MakeQueueWait(consumer,submitted);
    return texture;
}
void GuestRasterizer::CaptureDisplayCandidates(VAddr address,uint64_t id,Resource* selected){
    if(!DisplayCapture::Selected(id))return;
    std::lock_guard lock(execution_mutex_);
    Finish();textures_.Collect(CompletedFence());
    unsigned input_count{};
    const auto display=selected?selected->handle->GetDesc():D3D12_RESOURCE_DESC{};
    for(auto& entry:textures_.Entries())if(entry->desc.address==address||
        (DisplayCapture::Inputs(id)&&entry->desc.render_target&&entry->desc.width==display.Width&&
         entry->desc.height==display.Height&&input_count++<16)){
        auto& image=*entry;const auto& d=image.desc;const auto native=image.texture.handle->GetDesc();
        std::ostringstream info;
        info<<"snapshot="<<id<<" address="<<std::hex<<d.address<<" display_address="<<address<<" resource="<<image.texture.handle.Get()
            <<" allocation="<<image.texture.allocation.get()
            <<std::dec<<" uid="<<image.image_uid<<" width="<<d.width<<" height="<<d.height
            <<" pitch="<<d.pitch<<" tile="<<d.tile_mode<<" data="<<d.data_format<<" number="<<d.number_format
            <<" bank_swizzle="<<d.bank_swizzle<<" alt_tile="<<d.alt_tile<<" raw32="<<d.native_raw32
            <<" storage_height="<<d.storage_height<<" storage_slice_bytes="<<d.storage_slice_bytes
            <<" levels="<<d.levels<<" layers="<<d.layers<<" first_mip="<<d.first_mip<<" first_layer="<<d.first_layer
            <<" samples="<<d.samples<<" rt="<<d.render_target<<" writable="<<d.writable
            <<" owned="<<image.gpu_owned<<" alias="<<image.buffer_alias_version<<" pending="<<image.pending
            <<" fence="<<image.fence<<" completed="<<CompletedFence()<<" guest_bytes="<<image.layout.guest_bytes
            <<" native_format="<<native.Format<<" selected="<<(&image.texture==selected)
            <<" writer="<<image.diagnostic_writer<<" vs="<<std::hex<<image.diagnostic_vs<<" ps="<<image.diagnostic_ps<<std::dec;
        const auto stage="candidate-"+std::to_string(image.image_uid);
        if(native.MipLevels!=1||native.DepthOrArraySize!=1||native.SampleDesc.Count!=1||d.depth_target){
            DisplayCapture::Save(id,stage,{},info.str()+" readback=unsupported-subresources");continue;
        }
        const auto prior=image.texture.state;
        auto transfer=TextureTransfer::Create(device_.Get(),image.texture,true);
        transfer.Readback(RecordingList(),image.texture);image.texture.Transition(RecordingList(),prior);
        Finish();std::vector<std::byte> bytes(size_t(transfer.row_bytes)*transfer.rows);
        transfer.ReadRows(bytes.data(),bytes.size(),transfer.row_bytes);
        DisplayCapture::Save(id,stage,bytes,info.str()+" row_bytes="+std::to_string(transfer.row_bytes));
    }
}
bool GuestRasterizer::MaterializeOwnedColorClear(const AmdGpu::ColorBuffer& b,
    const GuestTextureDesc& d,const GuestTextureLayout& l,const GuestColorClearPlan& plan,
    std::span<std::byte> metadata) {
    // Update an initialized native color owner, retaining a separate B version
    // when this target came from native depth recreation. Decode the CB map
    // with the same codec as physical materialization, before changing planes.
    const auto clear=DecodeGuestColorClear(b);
    if(!page_watch_||!d.render_target||d.depth_target||d.stencil_address||d.writable||
       d.one_dimensional||d.volume_depth||d.levels!=1||d.first_mip||
       !clear||d.UsesRaw32()||
       d.data_format!=clear->data_format||d.number_format!=clear->number_format||
       l.bytes_per_pixel!=clear->pixel_bytes||l.expanded_bytes_per_pixel||l.block_extent!=1||
       b.info.cmask_addr_type||b.info.compression||b.info.dcc_enable||
       !has_backing_(d.address,l.guest_bytes))return false;
    const auto meta_bytes=GuestCmaskBytes(b),slice_bytes=meta_bytes/b.NumSlices();
    if(metadata.size()!=meta_bytes||d.first_layer>=b.NumSlices()||!d.view_layers||
       d.view_layers>b.NumSlices()-d.first_layer||
       Overlap(d.address,l.guest_bytes,b.CmaskAddress(),meta_bytes))return false;
    if(!plan.Pending())return false;
    GuestTextureCache::Entry* image{};
    for(auto& entry:textures_.Entries())if(entry->gpu_owned&&ImageOverlap(d.address,l.guest_bytes,*entry)){
        // A competing alias is not evidence that this image is the last
        // writer. Let the existing reconciliation path determine ownership.
        if(image||(entry->buffer_alias_version&&!textures_.HasCurrentNativeContents(*entry))||
           entry->layout.guest_bytes!=l.guest_bytes||
           !CompatibleGuestAttachmentStorage(entry->desc,d))return false;
        image=entry.get();
    }
    if(!image||!image->texture.handle)return false;
    const auto native=image->texture.handle->GetDesc();
    if(native.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||native.Width!=d.width||native.Height!=d.height||
       native.MipLevels!=1||native.DepthOrArraySize!=d.layers||native.SampleDesc.Count!=d.samples||
       !(native.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))return false;
    std::shared_ptr<GuestRawColorClear::Program> raw_program;
    if(!clear->rtv_compatible){
        if(!raw_color_clear_)raw_color_clear_=std::make_unique<GuestRawColorClear>(device_.Get());
        raw_program=raw_color_clear_->Prepare(image->texture,d.data_format);
        if(!raw_program)return false;
    }
    // Use exactly the CPU materializer's validated tile plan. Rectangles are
    // clipped at logical edges; expanded tiles are not included in mixed layers.
    std::map<UINT,std::vector<D3D12_RECT>> regions;
    for(const auto layer:plan.uniform_layers)
        regions[layer].push_back({0,0,LONG(d.width),LONG(d.height)});
    for(const auto& tile:plan.tiles)
        regions[tile.layer].push_back({LONG(tile.x),LONG(tile.y),
            LONG(std::min(tile.x+8,d.width)),LONG(std::min(tile.y+8,d.height))});
    ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;heap_desc.NumDescriptors=UINT(regions.size());
    Check(DX12_API_CALL(device_->CreateDescriptorHeap(&heap_desc,IID_PPV_ARGS(&heap))));
    const auto step=device_->GetDescriptorHandleIncrementSize(heap_desc.Type);
    auto handle=heap->GetCPUDescriptorHandleForHeapStart();
    for(const auto& region:regions){
        auto view=d;view.first_layer=region.first;view.view_layers=1;
        auto rtv=image->Rtv(view);if(raw_program)rtv.Format=raw_program->format;
        DX12_API_CALL(device_->CreateRenderTargetView(image->texture.handle.Get(),&rtv,handle));handle.ptr+=step;
    }
    // RTV clear writes all samples of each selected layer. Unsupported raw
    // integer/F32 values never reach the float-based native clear API.
    auto pin=textures_.PinBinding(*image);
    // Begin may collect older submissions; do it before publishing this
    // recording's pending ownership, then use that same list throughout.
    auto* list=RecordingList();
    RetainUntilFinish([heap,resource=image->texture.handle,allocation=image->texture.allocation,pin,raw_program](u64){
        (void)heap;(void)resource;(void)allocation;(void)pin;(void)raw_program;
    });
    // Stamp the submitted fence before old retirements run. A completion
    // callback must not relabel newer pending texture work with an old fence.
    NotifyOnSubmission([this](u64 fence){textures_.Submitted(fence);});
    image->pending=true;
    image->texture.Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);
    handle=heap->GetCPUDescriptorHandleForHeapStart();
    for(const auto& region:regions){
        const auto& rectangles=region.second;
        if(raw_program)GuestRawColorClear::Record(list,*raw_program,handle,d.width,d.height,rectangles,*clear);
        else DX12_API_CALL(list->ClearRenderTargetView(handle,clear->rtv.data(),UINT(rectangles.size()),rectangles.data()));
        handle.ptr+=step;
    }
    image->NoteLogicalImageWrite(); // The actual clear is a GPU write, including a GpuModified0 copy.
    image->diagnostic_writer=image->diagnostic_vs=image->diagnostic_ps=0;
    // Color pages stay guarded and GPU-owned. ResolveImage later merges these
    // logical texels into existing backing, preserving padding/untouched layers.
    // Publish expanded metadata only after the color writes are in queue order.
    for(const auto layer:plan.uniform_layers)
        std::fill_n(metadata.begin()+size_t(slice_bytes*layer),size_t(slice_bytes),std::byte{0xff});
    for(const auto& tile:plan.tiles)metadata[tile.byte]|=std::byte(15u<<tile.shift);
    InvalidateMemory(b.CmaskAddress(),meta_bytes);
    return true;
}
void GuestRasterizer::MaterializeColorClear(const AmdGpu::ColorBuffer& b,
    const GuestTextureDesc& d,const GuestTextureLayout& l,bool native_only) {
    if(!b.info.fast_clear||!b.cmask_base_address)return;
    const auto meta_bytes=GuestCmaskBytes(b);
    // Fast-clear enable is persistent state, not evidence of a new clear.
    // Inspect the small metadata plane before resolving the color surface;
    // expanded (0xff) metadata must leave the render target GPU-resident.
    if(!ReadMemory(b.CmaskAddress(),meta_bytes))
        throw std::runtime_error("Native CMASK backing is not mapped");
    ValidateHostMemory(b.CmaskAddress(),meta_bytes,true);
    GuestColorClearPlan plan;
    try {
        plan=PlanGuestColorClear(b,d,l,
            {reinterpret_cast<const std::byte*>(b.CmaskAddress()),size_t(meta_bytes)});
    } catch(...) {
        // Opt-in, bounded reproduction data; keep the original failure intact.
        if(const auto* directory=std::getenv("SHADPS4_CMASK_CAPTURE")) {
            const auto path=std::filesystem::path(directory);
            std::filesystem::create_directories(path);
            std::ofstream(path/"color-buffer.bin",std::ios::binary).write(
                reinterpret_cast<const char*>(&b),sizeof(b));
            std::ofstream(path/"descriptor.bin",std::ios::binary).write(
                reinterpret_cast<const char*>(&d),sizeof(d));
            std::ofstream(path/"cmask.bin",std::ios::binary).write(
                reinterpret_cast<const char*>(b.CmaskAddress()),size_t(meta_bytes));
            if(l.guest_bytes<=64*1024*1024&&ReadMemory(d.address,l.guest_bytes))
                std::ofstream(path/"color.bin",std::ios::binary).write(
                    reinterpret_cast<const char*>(d.address),size_t(l.guest_bytes));
            fprintf(stderr,"CMASK capture: color=%llx meta=%llx size=%ux%u pitch=%u height=%u tile=%u format=%u linear=%u addr_type=%u meta_bytes=%llu\n",
                (unsigned long long)d.address,(unsigned long long)b.CmaskAddress(),d.width,d.height,d.pitch,d.storage_height,d.tile_mode,d.data_format,
                b.info.cmask_is_linear,b.info.cmask_addr_type,(unsigned long long)meta_bytes);
        }
        throw;
    }
    if(!plan.Pending())return;
    if(MaterializeOwnedColorClear(b,d,l,plan,
        {reinterpret_cast<std::byte*>(b.CmaskAddress()),size_t(meta_bytes)}))return;
    if(native_only)throw std::runtime_error("Recreated native color target has no exact pending CMASK clear path");
    if(!ReadMemory(d.address,l.guest_bytes))
        throw std::runtime_error("Native color backing is not mapped");
    ValidateHostMemory(d.address,l.guest_bytes,true);
    static unsigned clear_traces{};
    if(std::getenv("SHADPS4_NATIVE_STARTUP_TRACE")&&clear_traces++<16){
        fprintf(stderr,"DX12 CMASK surface=%llx meta=%llx bytes=%llu first=%02x clear=%08x,%08x\n",
            (unsigned long long)d.address,(unsigned long long)b.CmaskAddress(),
            (unsigned long long)meta_bytes,unsigned(*reinterpret_cast<const unsigned char*>(b.CmaskAddress())),b.clear_word0,b.clear_word1);fflush(stderr);
    }
    if(MaterializeGuestColorClear(b,d,l,
        {reinterpret_cast<std::byte*>(d.address),size_t(l.guest_bytes)},
        {reinterpret_cast<std::byte*>(b.CmaskAddress()),size_t(meta_bytes)})) {
        InvalidateMemory(d.address,l.guest_bytes);
        InvalidateMemory(b.CmaskAddress(),meta_bytes);
    }
}
void GuestRasterizer::ResolveColorTargets(){
    std::lock_guard lock(execution_mutex_);
    const auto& r=gpu_.regs;const auto& source_buffer=r.color_buffers[0];
    const auto& destination_buffer=r.color_buffers[1];
    if(!source_buffer||!destination_buffer)throw std::runtime_error("Guest color resolve requires MRT0 and MRT1");
    auto source_desc=DescribeGuestColorBuffer(source_buffer,gpu_.last_cb_extent[0]);
    auto destination_desc=DescribeGuestColorBuffer(destination_buffer,gpu_.last_cb_extent[1]);
    auto source_layout=LayoutGuestTexture(source_desc),destination_layout=LayoutGuestTexture(destination_desc);
    ValidateHostMemory(source_desc.address,source_layout.guest_bytes);
    ValidateHostMemory(destination_desc.address,destination_layout.guest_bytes,true);
    MaterializeColorClear(source_buffer,source_desc,source_layout);
    auto& source=textures_.Upload(source_desc,{reinterpret_cast<const std::byte*>(source_desc.address),
        size_t(source_layout.guest_bytes)},RecordingList(),CompletedFence());
    auto& destination=textures_.Upload(destination_desc,{reinterpret_cast<const std::byte*>(destination_desc.address),
        size_t(destination_layout.guest_bytes)},RecordingList(),CompletedFence());
    for(auto& entry:textures_.Entries())if(entry.get()!=&destination&&entry.get()!=&source&&entry->gpu_owned&&
        ImageOverlap(destination.desc.address,destination.layout.guest_bytes,*entry))ResolveImage(*entry);
    if(!OwnImage(destination))throw std::runtime_error("Cannot acquire guest resolve destination ownership");
    if(&source==&destination||
       source.desc.data_format!=destination.desc.data_format||source.desc.number_format!=destination.desc.number_format||
       source.desc.width>destination.desc.width||source.desc.height>destination.desc.height)
        throw std::runtime_error("Native color resolve needs distinct compatible source and destination images");
    textures_.SetBackingSamples(destination,1,RecordingList(),false);
    auto active_source=source.desc;active_source.samples=source.texture.handle->GetDesc().SampleDesc.Count;
    const auto layers=std::min(source.desc.view_layers,destination.desc.view_layers);
    auto* list=RecordingList();
    if(active_source.samples==1){
        source.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
        destination.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
        for(UINT layer=0;layer<layers;++layer){
            D3D12_TEXTURE_COPY_LOCATION input{};input.pResource=source.texture.handle.Get();
            input.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;input.SubresourceIndex=source.desc.first_layer+layer;
            D3D12_TEXTURE_COPY_LOCATION output{};output.pResource=destination.texture.handle.Get();
            output.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;output.SubresourceIndex=destination.desc.first_layer+layer;
            D3D12_BOX region{0,0,0,source.desc.width,source.desc.height,1};
            DX12_API_CALL(list->CopyTextureRegion(&output,0,0,0,&input,&region));
        }
    }else if(source.desc.number_format==4||source.desc.number_format==5){
        // Vulkan integer resolve selects a sample, rather than averaging integer
        // values. Extract sample zero without conversion through float values.
        struct IntegerResolve {
            std::vector<Resource> buffers;
            std::vector<std::shared_ptr<GuestMultisample::Lease>> leases;
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        };
        auto retained=std::make_shared<IntegerResolve>();retained->buffers.reserve(layers);retained->leases.reserve(layers);
        retained->footprint=GuestMultisample::Footprint(active_source,source.layout);
        for(UINT layer=0;layer<layers;++layer){
            retained->buffers.push_back(GuestTiler::Scratch(device_.Get(),
                GuestMultisample::LinearBytes(active_source,retained->footprint)));
            retained->leases.push_back(source.multisample->Prepare(source.texture,retained->buffers.back(),
                active_source,source.layout,source.desc.first_layer+layer,0,false));
        }
        RetainUntilFinish([retained](u64){});
        for(UINT layer=0;layer<layers;++layer){
            auto& buffer=retained->buffers[layer];retained->leases[layer]->Record(list,source.texture,buffer);
            buffer.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
            destination.texture.Transition(list,D3D12_RESOURCE_STATE_COPY_DEST);
            D3D12_TEXTURE_COPY_LOCATION input{};input.pResource=buffer.handle.Get();
            input.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;input.PlacedFootprint=retained->footprint;
            D3D12_TEXTURE_COPY_LOCATION output{};output.pResource=destination.texture.handle.Get();
            output.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;output.SubresourceIndex=destination.desc.first_layer+layer;
            DX12_API_CALL(list->CopyTextureRegion(&output,0,0,0,&input,nullptr));
        }
    }else{
        D3D12_FEATURE_DATA_FORMAT_SUPPORT support{source.layout.format};
        Check(DX12_API_CALL(device_->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support))));
        if(!(support.Support1&D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE))
            throw std::runtime_error("Native color format lacks native multisample resolve");
        ComPtr<ID3D12GraphicsCommandList1> resolve_list;Check(DX12_API_CALL(list->QueryInterface(IID_PPV_ARGS(&resolve_list))));
        source.texture.Transition(list,D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        destination.texture.Transition(list,D3D12_RESOURCE_STATE_RESOLVE_DEST);
        D3D12_RECT region{0,0,LONG(source.desc.width),LONG(source.desc.height)};
        for(UINT layer=0;layer<layers;++layer)DX12_API_CALL(resolve_list->ResolveSubresourceRegion(
            destination.texture.handle.Get(),destination.desc.first_layer+layer,0,0,
            source.texture.handle.Get(),source.desc.first_layer+layer,&region,source.layout.format,D3D12_RESOLVE_MODE_AVERAGE));
    }
    source.pending=destination.pending=true;
}
void GuestRasterizer::Draw(bool indexed,u32 index_offset) {
    try {DrawInternal(indexed,index_offset,nullptr,0,0);}
    catch(const std::exception& e){fprintf(stderr,"Native DX12 Draw failed: %s\n",e.what());fflush(stderr);throw;}
}
void GuestRasterizer::DrawInternal(bool indexed,u32 index_offset,const DrawArguments* indirect,
    u16 vertex_sgpr,u16 instance_sgpr,const NativeIndirectBatch* batch) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Draw,"execution-lock");
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    const auto& r=gpu_.regs;
    const bool indirect_draw=indirect||batch;
    diagnostic.Detail("primitive=%u count=%u indexed=%u cb0=%llx",u32(r.primitive_type),r.num_indices,
        unsigned(indexed),(unsigned long long)r.color_buffers[0].Address());
    diagnostic.Stage("guest-state");
    const auto operation=ClassifyGuestDraw(r);
    if(operation==GuestDrawOperation::Skip)return;
    if(operation==GuestDrawOperation::FastClear) {
        const auto& b=r.color_buffers[0];
        if(b) {
            const auto d=DescribeGuestColorBuffer(b,gpu_.last_cb_extent[0]);
            MaterializeColorClear(b,d,LayoutGuestTexture(d));
        }
        return;
    }
    if(operation==GuestDrawOperation::Resolve){
        diagnostic.Stage("color-resolve");ResolveColorTargets();return;
    }
    const bool color_disabled=r.color_control.mode==AmdGpu::ColorControl::OperationMode::Disable;
    const bool copy_depth=r.depth_render_override.force_z_dirty&&r.depth_render_override.force_z_valid&&
        r.depth_buffer.DepthValid()&&r.depth_buffer.DepthWriteValid()&&
        r.depth_buffer.DepthAddress()!=r.depth_buffer.DepthWriteAddress();
    const bool copy_stencil=r.depth_render_override.force_stencil_dirty&&r.depth_render_override.force_stencil_valid&&
        r.depth_buffer.StencilValid()&&r.depth_buffer.StencilWriteValid()&&
        r.depth_buffer.StencilAddress()!=r.depth_buffer.StencilWriteAddress();
    if(color_disabled&&(copy_depth||copy_stencil)){
        diagnostic.Stage("depth-stencil-override-copy");
        auto source=DescribeGuestDepthBuffer(r.depth_buffer,r.depth_view,gpu_.last_db_extent);
        const auto depth_layout=LayoutGuestTexture(source);
        const auto copy_plane=[&](VAddr from,VAddr to,u64 bytes){
            ReadMemory(from,bytes);ValidateHostMemory(from,bytes);
            PrepareMemoryProtection(to,bytes);ValidateHostMemory(to,bytes,true);
            memmove(reinterpret_cast<void*>(to),reinterpret_cast<const void*>(from),size_t(bytes));
            Rasterizer::InvalidateMemory(to,bytes);
        };
        if(copy_depth)copy_plane(source.address,r.depth_buffer.DepthWriteAddress(),depth_layout.guest_bytes);
        if(copy_stencil){const auto stencil=DescribeGuestStencil(source);const auto layout=LayoutGuestTexture(stencil);
            copy_plane(stencil.address,r.depth_buffer.StencilWriteAddress(),layout.guest_bytes);}
        return;
    }
    static u32 traced_draws{};
    const bool trace=std::getenv("SHADPS4_NATIVE_STARTUP_TRACE")&&(++traced_draws<=16||traced_draws%512==0);
    if(trace){fprintf(stderr,"DX12 startup draw %u: primitive=%u count=%u cbmode=%u mask=%08x depth=%u stencil=%u\n",
        traced_draws,u32(r.primitive_type),r.num_indices,u32(r.color_control.mode),r.color_target_mask.raw,
        r.depth_control.depth_enable,r.depth_control.stencil_enable);fflush(stderr);}
    DrawArguments draw=indirect?*indirect:DrawArguments{r.num_indices,r.num_instances.NumInstances(),
        indexed?0:r.index_offset,0,indexed?s32(r.index_offset):0};
    // A native batch has no CPU draw-count approximation. Only GPU packets
    // determine its counts/instances; fixed preparation uses explicit bounds.
    if(batch)draw={0,0};
    if((!batch&&(!draw.count||!draw.instances))||r.primitive_type==AmdGpu::PrimitiveType::None)return;
    const auto depth_tests=GuestDepthTests(r);
    const bool has_depth=depth_tests.HasAttachment();
    if(Diagnostics::Enabled()&&((r.depth_control.stencil_enable&&!depth_tests.stencil)||
        (r.depth_control.depth_enable&&!depth_tests.depth))) {
        static u32 reported{};
        if(reported++<8)fprintf(stderr,"DX12 guest DB gate: depth_enable=%u valid=%u stencil_enable=%u valid=%u z_base=%llx s_base=%llx z_format=%u s_format=%u\n",
            r.depth_control.depth_enable,unsigned(r.depth_buffer.DepthValid()),r.depth_control.stencil_enable,
            unsigned(r.depth_buffer.StencilValid()),(unsigned long long)r.depth_buffer.DepthAddress(),
            (unsigned long long)r.depth_buffer.StencilAddress(),u32(r.depth_buffer.z_info.format),u32(r.depth_buffer.stencil_info.format));
    }
    // Normal PS4 draws bind the read-base image as their depth target.  The
    // separate write bases are consumed only by the force-dirty/force-valid
    // DepthStencilCopy command handled above (the Vulkan backend follows the
    // same split).  Do not reject an otherwise ordinary draw merely because
    // those dormant registers contain a different address.
    auto viewport=BuildGuestViewports(r);
    if(trace){const auto& v=viewport.viewports[0];const auto& s=viewport.scissors[0];
        fprintf(stderr,"DX12 startup draw %u: viewport=%g,%g,%g,%g scissor=%ld,%ld,%ld,%ld flip=%x cb0=%llx\n",traced_draws,
            v.TopLeftX,v.TopLeftY,v.Width,v.Height,s.left,s.top,s.right,s.bottom,viewport.flip_mask,(unsigned long long)r.color_buffers[0].Address());fflush(stderr);}
    diagnostic.Stage("shader-translation");
    std::optional<GuestPointCaptureStages> capture;
    GuestGraphicsStages guest;
    try{guest=frontend_.BuildGraphics(gpu_,vertex_sgpr,instance_sgpr,indirect_draw);}
    catch(const Shader::NativePointCapture::Required&){
        if(batch)throw std::logic_error("Native indirect batch was admitted with an unbounded capture stage");
        capture=frontend_.BuildPointCaptureGraphics(r,vertex_sgpr,instance_sgpr,indirect_draw);
        guest=capture->stream_patch_vertices?capture->stream_producer:capture->producer;
        // Keep original producer reflection for ownership/feedback analysis,
        // even though the GS-only consumer no longer contains those stages.
        guest.stages[u32(Shader::SwStage::Geometry)]=capture->producer.stages[u32(Shader::SwStage::Geometry)];
        guest.mrt_mask=capture->replay.mrt_mask;
        guest.ordered_color_mask=capture->replay.ordered_color_mask;
        guest.stages[u32(Shader::SwStage::Fragment)]=capture->replay.stages[u32(Shader::SwStage::Fragment)];
    }
    diagnostic.Stage("attachment-validation");
    if(trace){fprintf(stderr,"DX12 startup draw %u: guest shaders translated\n",traced_draws);fflush(stderr);}
    std::vector<GuestResourceStage> stages;
    for(const auto* s:guest.stages)if(s) {
        if(s->info.has_fetch_shader)throw std::runtime_error("Native guest external fetch shader needs input assembly");
        stages.push_back({&s->info,s->bindings_start});
    }
    std::array<GuestTextureDesc,9> descriptions{};
    std::array<GuestTextureLayout,9> layouts{};
    std::array<bool,8> deferred_color_clear{};
    std::array<bool,8> color_feedback{};
    std::array<bool,8> gpu_color_feedback;gpu_color_feedback.fill(true);
    std::array<DXGI_FORMAT,8> formats{};UINT target_count{};
    for(UINT i=0;i<8;++i)if(r.color_control.mode!=AmdGpu::ColorControl::OperationMode::Disable&&
        r.color_buffers[i]&&r.color_target_mask.GetMask(i)&&(guest.mrt_mask&(1u<<i))) {
        descriptions[i]=DescribeGuestColorBuffer(r.color_buffers[i],gpu_.last_cb_extent[i]);
        layouts[i]=LayoutGuestTexture(descriptions[i]);formats[i]=layouts[i].format;target_count=i+1;
    }
    MergeGuestColorAttachmentLayers(std::span(descriptions.data(),target_count));
    for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN){
        descriptions[i]=textures_.ResolveDepthViewStorage(descriptions[i]);
        layouts[i]=LayoutGuestTexture(descriptions[i]);
        ValidateGpuBacking(descriptions[i].address,layouts[i].guest_bytes,true);
        for(UINT j=0;j<i;++j)if(formats[j]!=DXGI_FORMAT_UNKNOWN&&
            Overlap(descriptions[i].address,layouts[i].guest_bytes,descriptions[j].address,layouts[j].guest_bytes)){
            const auto& a=descriptions[i];const auto& b=descriptions[j];
            const bool disjoint=a.first_layer>=b.first_layer+b.view_layers||
                b.first_layer>=a.first_layer+a.view_layers;
            if(!disjoint||!CompatibleGuestAttachmentStorage(a,b))
                throw std::runtime_error("Overlapping guest color attachment views require compatible disjoint subresources");
        }
        for(const auto& stage:stages) {
            for(const auto& image:stage.info->images) {
                const auto d=DescribeGuestTexture(image.GetSharp(*stage.info));const auto l=LayoutGuestTexture(d);
                const auto ordered_mask=capture?capture->replay.ordered_color_mask:guest.ordered_color_mask;
                const bool disjoint=!(ordered_mask&(1u<<i))&&DisjointGuestColorSampledViews(descriptions[i],layouts[i],d,l);
                if(!image.is_written&&!disjoint&&Overlap(d.address,l.guest_bytes,descriptions[i].address,layouts[i].guest_bytes)){
                    color_feedback[i]=true;
                    gpu_color_feedback[i]=gpu_color_feedback[i]&&!(ordered_mask&(1u<<i))&&
                        CompatibleGuestColorSampledMapping(descriptions[i],layouts[i],d,l);
                }
            }
        }
    }
    DXGI_FORMAT depth_format=DXGI_FORMAT_UNKNOWN;
    bool depth_feedback=false;
    GuestTextureDesc stencil_desc;GuestTextureLayout stencil_layout;
    std::span<std::byte> stencil_memory;
    if(has_depth) {
        descriptions[8]=DescribeGuestDepthBuffer(r.depth_buffer,r.depth_view,gpu_.last_db_extent);
        // Declared stencil allocation does not mean this draw accesses it.
        // Keep depth-only passes eligible for GPU page ownership. Re-enabling
        // stencil selects the combined view and Upload resolves the old depth
        // owner before initializing that view from coherent guest memory.
        if(!depth_tests.stencil)descriptions[8].stencil_address=0;
        // Resolve the owning allocation before alias checks and prewrite
        // snapshots. A smaller DSV does not shrink the cached physical image.
        descriptions[8]=textures_.ResolveDepthViewStorage(descriptions[8]);
        layouts[8]=LayoutGuestTexture(descriptions[8]);
        ValidateHostMemory(descriptions[8].address,layouts[8].guest_bytes);
        if(descriptions[8].stencil_address){
            stencil_desc=DescribeGuestStencil(descriptions[8]);stencil_layout=LayoutGuestTexture(stencil_desc);
            ValidateHostMemory(stencil_desc.address,stencil_layout.guest_bytes);
            // The texture cache resolves incompatible image ownership before
            // CPU access. Retire buffer aliases without downloading an exact
            // GPU-owned depth/stencil attachment on every draw.
            Rasterizer::ReadMemory(stencil_desc.address,stencil_layout.guest_bytes);
            stencil_memory={reinterpret_cast<std::byte*>(stencil_desc.address),size_t(stencil_layout.guest_bytes)};
        }
        depth_format=descriptions[8].stencil_address?DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            layouts[8].bytes_per_pixel==4?DXGI_FORMAT_D32_FLOAT:DXGI_FORMAT_D16_UNORM;
        for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN&&
            Overlap(descriptions[8].address,layouts[8].guest_bytes,descriptions[i].address,layouts[i].guest_bytes))
            throw std::runtime_error("Guest depth/color alias requires shared storage");
        for(const auto& stage:stages) {
            for(const auto& image:stage.info->images) {
                const auto d=DescribeGuestTexture(image.GetSharp(*stage.info));const auto l=LayoutGuestTexture(d);
                if(Overlap(d.address,l.guest_bytes,descriptions[8].address,layouts[8].guest_bytes)){
                    if(!image.is_written&&!r.depth_control.depth_write_enable&&
                       CompatibleGuestSampledStorage(descriptions[8],d))depth_feedback=true;
                }
            }
        }
    }
    auto state=BuildGuestGraphicsState(r,std::span(formats.data(),target_count),depth_format,
        r.ls_hs_config.hs_input_control_points,native_alpha_factor_);
    const u32 shader_ordered_mask=capture?capture->replay.ordered_color_mask:guest.ordered_color_mask;
    if(state.ordered_color_mask!=shader_ordered_mask)
        throw std::logic_error("Native ordered color shader/state target masks disagree");
    if(state.discard)return;
    const bool clear_stencil=depth_tests.stencil&&r.depth_render_control.stencil_clear_enable;
    const bool explicit_depth_clear=depth_tests.depth&&r.depth_render_control.depth_clear_enable&&
        r.depth_control.depth_write_enable;
    if(has_depth){
        ValidateGpuBacking(descriptions[8].address,layouts[8].guest_bytes,state.WritesDepth(explicit_depth_clear));
        if(descriptions[8].stencil_address)
            ValidateGpuBacking(stencil_desc.address,stencil_layout.guest_bytes,state.WritesStencil(clear_stencil));
    }
    bool depth_attachment_snapshot=false;
    bool shader_depth_overlap=false,shader_stencil_overlap=false;
    const auto record_depth_writer=[&](VAddr address,u64 bytes){
        if(!has_depth||!address||!bytes)return;
        const bool depth_overlap=Overlap(address,bytes,descriptions[8].address,layouts[8].guest_bytes);
        const bool stencil_overlap=descriptions[8].stencil_address&&
            Overlap(address,bytes,stencil_desc.address,stencil_layout.guest_bytes);
        if((depth_overlap&&state.WritesDepth(explicit_depth_clear))||
           (stencil_overlap&&state.WritesStencil(clear_stencil)))
            throw std::runtime_error("Native simultaneous shader/depth-stencil attachment writes require shared ownership");
        shader_depth_overlap|=depth_overlap;shader_stencil_overlap|=stencil_overlap;
        depth_attachment_snapshot|=depth_overlap||stencil_overlap;
    };
    for(const auto& stage:stages)for(const auto& buffer:stage.info->buffers)if(buffer.is_written&&!buffer.IsSpecial()){
        const auto sharp=buffer.GetSharp(*stage.info);const auto bytes=sharp.GetSize();
        if(!sharp.base_address||!bytes)continue;
        for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN&&
            Overlap(sharp.base_address,bytes,descriptions[i].address,layouts[i].guest_bytes))
            throw std::runtime_error("Native simultaneous buffer/color-attachment writes require shared ownership");
        record_depth_writer(sharp.base_address,bytes);
    }
    for(const auto& stage:stages)for(const auto& image:stage.info->images)if(image.is_written){
        const auto sharp=image.GetSharp(*stage.info);if(!sharp)continue;
        const auto desc=DescribeGuestTexture(sharp);const auto layout=LayoutGuestTexture(desc);
        record_depth_writer(desc.address,layout.guest_bytes);
    }
    // Match FindImage -> ResolveDepthOverlap BEFORE shader buffer binding and
    // prewrite snapshots. A dirty cached native allocation is a recreation copy
    // source, never proof of current independent I/guest-memory ownership.
    for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN){
        // Shader buffer/image binds precede the original RT FindImage. Do not
        // publish its future native class or metadata during physical prewarm.
        auto* source=textures_.FindCachedDepthRecreationSource(descriptions[i],layouts[i]);
        if(!source)source=textures_.FindIndependentUploadSource(descriptions[i],layouts[i]);
        deferred_color_clear[i]=source&&source->desc.depth_target;
        if(!deferred_color_clear[i])MaterializeColorClear(r.color_buffers[i],descriptions[i],layouts[i]);
    }
    auto depth_binding_plan=has_depth?textures_.PlanCachedImageBinding(descriptions[8],layouts[8]):nullptr;
    Resource depth_gpu_snapshot,stencil_gpu_snapshot;
    bool depth_buffer_source=false,stencil_buffer_source=false;
    std::vector<std::byte> depth_guest_snapshot,stencil_guest_snapshot;
    std::optional<u32> depth_meta_snapshot;
    if(depth_attachment_snapshot&&!depth_binding_plan){
        GuestTextureCache::Entry* depth_source{};
        if(GuestTextureCache::CanUploadGpuSnapshot(descriptions[8],layouts[8]))
            for(const auto& candidate:textures_.Entries())if(textures_.HasCurrentDepthContents(*candidate)&&
                !candidate->HasUnprotectedCpuPages()&&
                CompatibleGuestAttachmentStorage(candidate->desc,descriptions[8])&&
                MatchesGuestMipLayout(candidate->layout,layouts[8])){
                if(depth_source&&depth_source->texture.handle.Get()!=candidate->texture.handle.Get())
                    throw std::runtime_error("Ambiguous valid depth/stencil snapshot source");
                depth_source=candidate.get();
            }
        auto depth_source_pin=depth_source?textures_.PinBinding(*depth_source):std::shared_ptr<void>{};
        depth_buffer_source=IsGpuBufferModified(descriptions[8].address,layouts[8].guest_bytes);
        stencil_buffer_source=descriptions[8].stencil_address&&
            IsGpuBufferModified(stencil_desc.address,stencil_layout.guest_bytes);
        // Binding a writer invalidates guest-memory equality proofs, and cache
        // uploads can replace overlapping entries. Keep independent prewrite
        // contents rather than a cache pointer. HTILE is also a writer: keep its mask
        // across replacement and reject a pending clear before shader writes.
        const auto meta=r.depth_buffer.z_info.tile_surface_enable?
            r.depth_htile_data_base.GetAddress():0;
        if(meta){
            auto* entry=depth_source;
            if(!entry){
                if(!ReadMemory(descriptions[8].address,layouts[8].guest_bytes))
                    throw std::out_of_range("Native depth snapshot has no readable guest backing");
                entry=&textures_.Upload(descriptions[8],
                    std::span(reinterpret_cast<const std::byte*>(descriptions[8].address),size_t(layouts[8].guest_bytes)),
                    RecordingList(),CompletedFence(),stencil_memory,false,nullptr,false,false,
                    GuestTextureCache::UploadPurpose::Preparation);
            }
            textures_.RegisterMeta(*entry,GuestTextureCache::MetaType::HTile,meta,entry->htile_initial_mask);
            const auto info=textures_.GetMetaInfo(meta);
            if(!info)throw std::logic_error("Native depth snapshot lost its HTILE owner");
            depth_meta_snapshot=info->clear_mask;
            if(shader_depth_overlap&&depth_tests.depth&&r.depth_view.slice_start<32&&
                (*depth_meta_snapshot&(u32(1)<<r.depth_view.slice_start)))
                throw std::runtime_error("Native simultaneous buffer/HTILE depth clear requires shared ownership");
        }
        diagnostic.Stage("shader-depth-prewrite-snapshot");
        const bool gpu_planes=GuestTextureCache::CanUploadGpuSnapshot(descriptions[8],layouts[8]);
        const auto capture_gpu=[&](VAddr address,u64 bytes){
            ShaderBufferRequest request{address,bytes,false,true,true};request.publish_dma_residency=false;
            // These are raw B sources for an image, not another shader-buffer
            // bind. Preserve B1 without replaying formatted I effects or
            // publishing residency before the actual depth/stencil bind.
            PreparePhysicalDmaBuffers(std::span(&request,1));
            auto snapshot=CapturePreparedShaderBuffer(address,bytes);
            Diagnostics::Count("shader-depth.gpu-snapshot.bytes",bytes);return snapshot;
        };
        // Capture all GPU versions first. Each input has its own native
        // resource and stays immutable while current shader writes target B.
        if(depth_source){
            // Valid I wins over unrelated resident B. Seed only private
            // padding from B; no capture publishes I pixels or guest writes.
            auto initial=capture_gpu(descriptions[8].address,layouts[8].guest_bytes);
            depth_gpu_snapshot=textures_.CaptureDepthPlane(*depth_source,0,initial,RecordingList(),CompletedFence());
            if(descriptions[8].stencil_address){
                auto stencil_initial=capture_gpu(stencil_desc.address,stencil_layout.guest_bytes);
                stencil_gpu_snapshot=textures_.CaptureDepthPlane(*depth_source,1,stencil_initial,RecordingList(),CompletedFence());
            }
            depth_buffer_source=stencil_buffer_source=false;
            Diagnostics::Count("shader-depth.native-image-snapshot");
        }else if(gpu_planes&&depth_buffer_source)
            depth_gpu_snapshot=capture_gpu(descriptions[8].address,layouts[8].guest_bytes);
        if(!depth_source&&gpu_planes&&stencil_buffer_source)
            stencil_gpu_snapshot=capture_gpu(stencil_desc.address,stencil_layout.guest_bytes);
        const auto capture=[&](VAddr address,u64 bytes,std::vector<std::byte>& result){
            if(!ReadMemory(address,bytes))
                throw std::out_of_range("Native depth/stencil snapshot has no readable guest backing");
            ValidateHostMemory(address,bytes);
            result.resize(size_t(bytes));memcpy(result.data(),reinterpret_cast<const void*>(address),result.size());
            Diagnostics::Count("shader-depth.cpu-snapshot.bytes",bytes);
        };
        if(!depth_gpu_snapshot.handle)
            capture(descriptions[8].address,layouts[8].guest_bytes,depth_guest_snapshot);
        if(descriptions[8].stencil_address&&!stencil_gpu_snapshot.handle)
            capture(stencil_desc.address,stencil_layout.guest_bytes,stencil_guest_snapshot);
        std::vector<ShaderBufferRequest> backing;
        if(depth_buffer_source)backing.push_back({descriptions[8].address,layouts[8].guest_bytes,false,true,true});
        if(stencil_buffer_source)backing.push_back({stencil_desc.address,stencil_layout.guest_bytes,false,true,true});
        for(auto& request:backing)request.publish_dma_residency=false;
        if(!backing.empty())PreparePhysicalDmaBuffers(backing);
    }
    GuestTextureCache::Entry* feedback_depth{};
    std::shared_ptr<void> feedback_depth_pin;
    if(depth_feedback&&!depth_attachment_snapshot&&!depth_binding_plan){
        feedback_depth=&textures_.Upload(descriptions[8],
            std::span(reinterpret_cast<const std::byte*>(descriptions[8].address),size_t(layouts[8].guest_bytes)),
            RecordingList(),CompletedFence(),stencil_memory,false,nullptr,false,false,
            GuestTextureCache::UploadPurpose::Preparation);
        feedback_depth_pin=textures_.PinBinding(*feedback_depth);
    }
    // D3D12 cannot place one subresource in RENDER_TARGET and SHADER_RESOURCE
    // states simultaneously.  Materialize the previous color contents and
    // evict its native attachment view before resource binding; the sampled
    // upload then becomes an immutable snapshot and the callback creates a
    // distinct RTV for this draw. Feedback layout alone does not promise
    // rasterization-ordered communication between fragment invocations. A
    // snapshot may cover defined pre-draw/disjoint-pixel reads, but this CPU
    // publication changes the physical-memory path and is not full parity.
    // Replace that publication with retained GPU contents; preserve the pinned
    // renderer's actual synchronization instead of inventing stronger ordering.
    for(UINT i=0;i<target_count;++i)if(color_feedback[i]&&!gpu_color_feedback[i]){
        ResolveMemory(descriptions[i].address,layouts[i].guest_bytes);
        DiscardImages(descriptions[i].address,layouts[i].guest_bytes);
    }
    bool write_depth_stencil=false;
    u32 depth_write_planes{};
    Resource index_storage;IndexBinding index_view{};
    VAddr resident_index_address{};UINT resident_index_bytes{};
    DXGI_FORMAT resident_index_format=DXGI_FORMAT_UNKNOWN;
    std::vector<ShaderBufferRequest> index_requests;
    if(batch){
        const auto plan=GuestIndirectPackPlan::Make(indexed,batch->stride,batch->max_count,batch->with_runtime);
        index_requests.push_back({batch->address,plan.source_bytes,false,false,true});
        index_requests.back().publish_dma_residency=false;
        if(batch->count_address){index_requests.push_back({batch->count_address,4,false,false,true});
            index_requests.back().publish_dma_residency=false;}
    }
    const bool expand_primitive=GuestPrimitiveNeedsExpansion(r.primitive_type);
    const bool strip=state.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP||state.topology==D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    const bool custom_strip_restart=indexed&&r.enable_primitive_restart&&strip&&
        r.primitive_restart_index!=(r.index_buffer_type.index_type==AmdGpu::IndexType::Index16?0xffffu:0xffffffffu);
    const bool rewrite_restart=indexed&&r.enable_primitive_restart&&
        (!strip||custom_strip_restart);
    if(custom_strip_restart){
        // Replacing the guest marker with the fixed D3D strip-cut value also
        // cuts legitimate 0xffff/0xffffffff vertex indices. Expand the strips
        // instead, preserving winding and resetting it at every restart.
        state.topology=state.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP?
            D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    }
    std::optional<GuestIndexExpansion> gpu_expansion;
    const u32 generated_index_first=draw.first;
    if(expand_primitive&&(!indexed||(!r.enable_primitive_restart&&
       r.index_buffer_type.swap_mode==AmdGpu::IndexSwapMode::None))){
        const auto plan=GuestIndexExpansion::Make(r.primitive_type,draw.count);
        if(!plan.output_count)return;
        gpu_expansion=plan;
    }
    const bool resident_indices=indexed&&(!expand_primitive||gpu_expansion.has_value())&&!rewrite_restart&&
        r.index_buffer_type.swap_mode==AmdGpu::IndexSwapMode::None;
    const auto index_range=[&](UINT element){
        const auto range=GuestIndexRange::Make(r.index_base_address.Address<VAddr>(),index_offset,
            draw.count,r.max_index_size,element,indirect_draw);
        u64 available=range.bytes;
#ifdef SHADPS4_NATIVE_DX12_CORE
        available=ClampMemoryRange(range.address,available);
#endif
        if(!batch)available=std::min(available,u64(draw.count)*element);
        available&=~u64(element-1);
        if(available>UINT_MAX||range.address>UINT64_MAX-available-3)
            throw std::out_of_range("Guest index range or native view size overflow");
        return std::pair{range.address,UINT(available)};
    };
    if(gpu_expansion&&!indexed){
        draw.count=gpu_expansion->output_count;draw.first=0;draw.base_vertex=0;
    }else if(resident_indices){
        const auto type=r.index_buffer_type.index_type;
        if(type!=AmdGpu::IndexType::Index16&&type!=AmdGpu::IndexType::Index32)
            throw std::runtime_error("Invalid guest index element type");
        const UINT element=type==AmdGpu::IndexType::Index16?2:4;
        const auto [address,bytes]=index_range(element);
        resident_index_address=address;resident_index_bytes=bytes;
        resident_index_format=element==2?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT;
        if(bytes){const auto first=address&~VAddr(3),last=(address+bytes+3)&~VAddr(3);
            index_requests.push_back({first,last-first,false,false,true,u8(address-first),bytes});
            // Source selection happens after shader/attachment binding effects.
            index_requests.back().publish_dma_residency=false;
        }else{
            resident_index_address=0;
            index_storage=Buffer(device_.Get(),4,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
            index_view={&index_storage,0,0,resident_index_format};
        }
        draw.first=0;
        if(gpu_expansion)draw.count=gpu_expansion->output_count;
    }else if(indexed||expand_primitive) {
        const bool narrow=r.index_buffer_type.index_type==AmdGpu::IndexType::Index16;
        if(indexed&&!narrow&&r.index_buffer_type.index_type!=AmdGpu::IndexType::Index32)
            throw std::runtime_error("Invalid guest index element type");
        std::vector<u32> logical;logical.reserve(draw.count);
        if(indexed){
            const auto [address,bytes]=index_range(narrow?2:4);
            const u64 source_bytes=u64(draw.count)*(narrow?2:4);
            std::vector<std::byte> source(size_t((source_bytes+3)&~u64(3)),std::byte{});
            if(bytes){ReadMemory(address,bytes);ValidateHostMemory(address,bytes);
                memcpy(source.data(),reinterpret_cast<const void*>(address),size_t(bytes));}
            const auto mode=r.index_buffer_type.swap_mode;
            if(mode==AmdGpu::IndexSwapMode::Swap16){for(size_t i=0;i+1<bytes;i+=2)std::swap(source[i],source[i+1]);}
            else if(mode==AmdGpu::IndexSwapMode::Swap32){for(size_t i=0;i+3<bytes;i+=4){std::swap(source[i],source[i+3]);std::swap(source[i+1],source[i+2]);}}
            else if(mode==AmdGpu::IndexSwapMode::SwapWord){for(size_t i=0;i+3<bytes;i+=4){std::swap(source[i],source[i+2]);std::swap(source[i+1],source[i+3]);}}
            for(u32 i=0;i<draw.count;++i){u32 value{};if(narrow){u16 v;memcpy(&v,source.data()+i*2,2);value=v;}else memcpy(&value,source.data()+i*4,4);logical.push_back(value);}
        }else for(u32 i=0;i<draw.count;++i)logical.push_back(draw.first+i);
        if(expand_primitive||rewrite_restart)
            logical=ExpandGuestPrimitive(r.primitive_type,logical,indexed&&r.enable_primitive_restart,r.primitive_restart_index);
        if(logical.empty())return;
        const bool output_narrow=!expand_primitive&&indexed&&narrow;
        if(logical.size()>UINT_MAX/(output_narrow?2u:4u))
            throw std::out_of_range("Expanded guest indices exceed the native index view size");
        std::vector<std::byte> converted((logical.size()*(output_narrow?2u:4u)+3)&~size_t(3),std::byte{});
        for(size_t i=0;i<logical.size();++i)if(output_narrow){const auto v=u16(logical[i]);memcpy(converted.data()+i*2,&v,2);}
            else memcpy(converted.data()+i*4,&logical[i],4);
        index_storage=Buffer(device_.Get(),converted.size(),D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
        void* mapped{};D3D12_RANGE none{};Check(DX12_API_CALL(index_storage.handle->Map(0,&none,&mapped)));
        memcpy(mapped,converted.data(),converted.size());DX12_API_CALL(index_storage.handle->Unmap(0,nullptr));
        index_view={&index_storage,0,UINT(logical.size()*(output_narrow?2u:4u)),output_narrow?DXGI_FORMAT_R16_UINT:DXGI_FORMAT_R32_UINT};
        draw.count=UINT(logical.size());draw.first=0;
        if(!indexed)draw.base_vertex=0;
    }
    diagnostic.Stage("pipeline-creation");
    const auto* geometry_stage=guest.stages[u32(Shader::SwStage::Geometry)];
    const bool expanded_points=capture.has_value()||(geometry_stage&&!geometry_stage->auxiliary&&geometry_stage->runtime.hw.gs.native_point_expansion&&
       geometry_stage->info.stores.GetAny(Shader::IR::Attribute::PointSize))||
       (guest.stages[u32(Shader::SwStage::Vertex)]&&
       guest.stages[u32(Shader::SwStage::Vertex)]->runtime.sw.vs.native_point_expansion&&
       geometry_stage&&geometry_stage->auxiliary);
    if(expanded_points)
        state.pipeline.RasterizerState.FrontCounterClockwise=!bool(viewport.flip_mask&1);
    const auto point_viewports=expanded_points?BuildGuestPointViewports(viewport.viewports):std::vector<D3D12_VIEWPORT>{};
    const ShaderPipeline::StencilFaces stencil{state.stencil_read_mask_front,state.stencil_write_mask_front,
        state.stencil_read_mask_back,state.stencil_write_mask_back,state.separate_stencil_faces};
    const auto source_state=state;
    if(capture){state.topology=D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        state.pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        state.pipeline.IBStripCutValue=D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;}
    const auto& pipeline=pipelines_.Graphics(capture?capture->replay:guest,state.pipeline,state.depth_bounds,stencil,batch&&batch->with_runtime);
    if(trace){fprintf(stderr,"DX12 startup draw %u: native pipeline ready\n",traced_draws);fflush(stderr);}
    Shader::PushData push{};const auto& c=r.viewport_control;const auto& v=r.viewports[0];
    push.xoffset=c.xoffset_enable?v.xoffset:0;push.xscale=c.xscale_enable?v.xscale:1;
    push.yoffset=c.yoffset_enable?v.yoffset:0;push.yscale=c.yscale_enable?v.yscale:1;
    push.blend_constants=r.blend_constants;
    GraphicsRuntime runtime{};runtime.first_vertex=indexed?u32(draw.base_vertex):draw.first;
    runtime.base_instance=draw.first_instance;runtime.indexed=indexed;
    runtime.yz_flip_mask=viewport.flip_mask;runtime.SetViewports(viewport.viewports,expanded_points,viewport.clip_transforms);
    std::array<GuestTextureCache::Entry*,9> targets{};
    std::array<bool,9> readback_rows{};
    std::vector<std::unique_ptr<GuestImageReadback>> readbacks;
    static const auto probe_start=GetTickCount64();
    static unsigned probe_count{};
    std::error_code probe_error;
    const bool probe=GetTickCount64()-probe_start>=30000&&probe_count<512&&
        std::filesystem::exists("scene-probe.enabled",probe_error);
    ComPtr<ID3D12QueryHeap> probe_heap;
    std::vector<Resource> probe_results;
    if(probe){
        D3D12_QUERY_HEAP_DESC query{};query.Type=D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;query.Count=1;
        Check(device_->CreateQueryHeap(&query,IID_PPV_ARGS(&probe_heap)));
    }
    diagnostic.Stage("resource-upload-and-draw");
    DrawArguments output_draw=draw;
    // A bounded producer graph after the NEXT snapshot trigger. Copy actual
    // shader-bound resources before this draw and its outputs afterwards into
    // the same command stream; do not infer inputs from compatible RTs.
    struct BoundCapture {std::string name,metadata;TextureTransfer transfer;};
    std::vector<BoundCapture> bound_captures;
    struct BoundBufferCapture {std::string name,metadata;ResourceBindings::BufferCapture transfer;};
    std::vector<BoundBufferCapture> bound_buffer_captures;
    uint64_t bound_capture_id{};
    GuestReplayAttachments replay_attachments;
    // A logical draw samples its pre-draw feedback contents even when PS
    // execution is partitioned. Keep the first snapshot, not each slice's
    // partially updated render target.
    std::map<ID3D12Resource*,Resource> replay_feedback_snapshots;
    std::array<ColorAttachment,8> attachments{};
    DepthAttachment depth{};
    struct OrderedMultisampleTransfer {
        GuestTextureCache::Entry* entry{};
        std::shared_ptr<GuestMultisample::StorageLease> lease;
    };
    std::vector<OrderedMultisampleTransfer> ordered_multisample;
    bool expanded_indices_prepared{};
    const auto prepare_indices=[&](ResourceBindings& bindings){
        if(gpu_expansion){
            // One immutable expansion spans all original producer instances
            // and final replay. A producer may write the guest index alias;
            // it must not change the index snapshot for later instances.
            if(expanded_indices_prepared)return;
            bindings.Prepare();
            if(!index_expand_)index_expand_=std::make_unique<GuestIndexExpandPass>(device_.Get());
            index_storage=AcquireTransientBuffer(u64(gpu_expansion->output_count)*4);
            if(indexed){
                const auto source=resident_index_bytes?ObtainIndexBuffer(resident_index_address,resident_index_bytes,resident_index_format):IndexBinding{};
                RetainUntilFinish([source](u64){});
                const u32 element=resident_index_format==DXGI_FORMAT_R16_UINT?2:4;
                index_expand_->Record(*this,*gpu_expansion,source.resource,source.offset,
                    resident_index_bytes/element,element,index_storage);
            }else index_expand_->RecordGenerated(*this,*gpu_expansion,generated_index_first,index_storage);
            index_view={&index_storage,0,gpu_expansion->output_count*4,DXGI_FORMAT_R32_UINT};
            expanded_indices_prepared=true;
        }else if(resident_index_address)
            index_view=ObtainIndexBuffer(resident_index_address,resident_index_bytes,resident_index_format);
    };
    bool replay_prepared{};
    const auto prepare_replay=[&](ResourceBindings& bindings) {
        if(replay_prepared)return;
        if(index_view.resource)bindings.Retain(index_view.resource->handle.Get());
        for(UINT i=0;i<target_count;++i)if(formats[i]!=DXGI_FORMAT_UNKNOWN) {
            auto& entry=replay_attachments.Select(textures_,i,[&]() -> GuestTextureCache::Entry& {return textures_.Upload(descriptions[i],
                std::span(reinterpret_cast<const std::byte*>(descriptions[i].address),size_t(layouts[i].guest_bytes)),
                RecordingList(),CompletedFence());});
            bindings.RetainAllocation(replay_attachments.BindingPin(i));bindings.RetainAllocation(entry.texture.allocation);
            const auto& buffer=r.color_buffers[i];
            if(replay_attachments.Beginning()&&deferred_color_clear[i]){
                if(!OwnIndependentImage(entry))throw std::runtime_error("Recreated color target cannot retain native clear ownership");
                MaterializeColorClear(buffer,descriptions[i],layouts[i],true);
            }
            // CMASK bytes are materialized before upload. Keep their owner in
            // the same image cache while preserving the physical clear path.
            if(replay_attachments.Beginning()){
                textures_.RegisterMeta(entry,GuestTextureCache::MetaType::CMask,
                    buffer.info.fast_clear?buffer.CmaskAddress():0,0);
                textures_.RegisterMeta(entry,GuestTextureCache::MetaType::FMask,
                    buffer.info.compression?buffer.FmaskAddress():0);
                textures_.ConsumeMeta(buffer.CmaskAddress(),descriptions[i].first_layer);
            }
            OwnImage(entry);
            if(replay_attachments.Beginning())textures_.SetBackingSamples(entry,state.pipeline.SampleDesc.Count,RecordingList());
            if(entry.texture.handle->GetDesc().SampleDesc.Count!=state.pipeline.SampleDesc.Count)
                throw std::runtime_error("Native attachment fallback requires matching physical pipeline samples");
            targets[i]=&entry;
            if(state.ordered_color_mask&(1u<<i)){
                if(descriptions[i].samples==1){
                    const auto first=descriptions[i].first_layer;
                    const auto count=descriptions[i].view_layers;
                    for(UINT layer=first;layer<first+count;++layer)
                        entry.texture.Transition(RecordingList(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                 layer*entry.desc.levels);
                    const auto view=entry.OrderedColorUav(descriptions[i]);
                    bindings.Uav(Shader::NativeOrderedColor::Register(i),
                                 Shader::NativeOrderedColor::DescriptorSet,
                                 entry.texture.handle.Get(),view);
                }else{
                    auto transfer=std::ranges::find(ordered_multisample,&entry,
                        &OrderedMultisampleTransfer::entry);
                    if(transfer==ordered_multisample.end()){
                        auto lease=textures_.PrepareMultisampleStorage(entry,descriptions[i]);
                        lease->Record(RecordingList(),entry.texture,entry.storage_samples,
                                      entry.storage_linear,false);
                        ordered_multisample.push_back({&entry,std::move(lease)});
                    }
                    for(UINT sample=0;sample<descriptions[i].samples;++sample){
                        auto& plane=entry.storage_samples.at(sample);
                        bindings.RetainAllocation(plane.allocation);
                        bindings.Uav(Shader::NativeOrderedColor::Register(i,sample),
                                     Shader::NativeOrderedColor::DescriptorSet,
                                     plane.handle.Get(),entry.StorageUav(descriptions[i],sample));
                    }
                }
            }else attachments[i]={&entry.texture,entry.Rtv(descriptions[i])};
        }
        if(has_depth) {
            const auto depth_bytes=depth_attachment_snapshot?std::span<const std::byte>(depth_guest_snapshot):
                std::span(reinterpret_cast<const std::byte*>(descriptions[8].address),size_t(layouts[8].guest_bytes));
            const auto stencil_bytes=depth_attachment_snapshot?std::span<const std::byte>(stencil_guest_snapshot):
                std::span<const std::byte>(stencil_memory);
            auto& entry=replay_attachments.Select(textures_,8,[&]() -> GuestTextureCache::Entry& {return depth_binding_plan?textures_.BindPlannedImage(*depth_binding_plan,RecordingList(),CompletedFence()):
                feedback_depth?*feedback_depth:textures_.Upload(descriptions[8],
                depth_bytes,RecordingList(),CompletedFence(),stencil_bytes,
                depth_attachment_snapshot,depth_gpu_snapshot.handle?&depth_gpu_snapshot:nullptr,
                false,false,GuestTextureCache::UploadPurpose::Binding,
                stencil_gpu_snapshot.handle?&stencil_gpu_snapshot:nullptr);});
            if(depth_binding_plan){
                const auto requested=descriptions[8];descriptions[8]=entry.desc;
                descriptions[8].first_mip=requested.first_mip;descriptions[8].first_layer=requested.first_layer;
                descriptions[8].view_layers=requested.view_layers;descriptions[8].min_lod=requested.min_lod;descriptions[8].swizzle=requested.swizzle;
                layouts[8]=entry.layout;
                if(descriptions[8].stencil_address){stencil_desc=DescribeGuestStencil(descriptions[8]);stencil_layout=LayoutGuestTexture(stencil_desc);}
            }
            if(depth_buffer_source){
                const ShaderBufferRequest resident{descriptions[8].address,layouts[8].guest_bytes,false,true,true};
                DmaMemory().ObserveResidentSharps(std::span(&resident,1));
            }
            if(stencil_buffer_source){
                const ShaderBufferRequest resident{stencil_desc.address,stencil_layout.guest_bytes,false,true,true};
                DmaMemory().ObserveResidentSharps(std::span(&resident,1));
            }
            textures_.PublishBufferResidency(entry);
            bindings.RetainAllocation(replay_attachments.BindingPin(8));bindings.RetainAllocation(entry.texture.allocation);
            const auto meta=r.depth_buffer.z_info.tile_surface_enable?
                r.depth_htile_data_base.GetAddress():0;
            if(replay_attachments.Beginning())textures_.RegisterMeta(entry,GuestTextureCache::MetaType::HTile,meta,
                depth_meta_snapshot.value_or(entry.htile_initial_mask));
            const bool metadata_clear=replay_attachments.Beginning()&&textures_.ConsumeMeta(meta,r.depth_view.slice_start);
            const bool clear_depth=replay_attachments.Beginning()&&depth_tests.depth&&((r.depth_render_control.depth_clear_enable&&
                r.depth_control.depth_write_enable)||metadata_clear);
            const bool draw_clear_stencil=replay_attachments.Beginning()&&clear_stencil;
            write_depth_stencil|=state.WritesDepthStencil(clear_depth,draw_clear_stencil);
            const u32 draw_write_planes=u32(state.WritesDepth(clear_depth))|u32(state.WritesStencil(draw_clear_stencil))*2;
            depth_write_planes|=draw_write_planes;
            if(clear_depth&&!explicit_depth_clear)
                ValidateGpuBacking(descriptions[8].address,layouts[8].guest_bytes,true);
            if((shader_depth_overlap&&(draw_write_planes&1))||(shader_stencil_overlap&&(draw_write_planes&2)))
                throw std::logic_error("Native depth snapshot acquired a competing attachment writer");
            if(metadata_clear&&Diagnostics::Enabled()){
                static unsigned reported{};
                if(reported++<64)fprintf(stderr,"DX12 HTile clear depth=%llx meta=%llx slice=%u value=%g\n",
                    (unsigned long long)r.depth_buffer.DepthAddress(),(unsigned long long)meta,
                    r.depth_view.slice_start,r.depth_clear);
            }
            // Host read-only flags follow the operations which can actually
            // write each plane, as the original target derives its storage
            // access from DepthControl rather than dormant view hints.
            // Clears happen before shader/attachment reads; their plane is
            // writable for that separate operation, not for the draw itself.
            const bool read_only_depth=!state.WritesDepth();
            const bool read_only_stencil=!state.WritesStencil();
            targets[8]=&entry;depth={&entry.texture,entry.Dsv(descriptions[8],read_only_depth,read_only_stencil),
                read_only_depth,read_only_stencil,
                clear_depth,r.depth_clear,draw_clear_stencil,UINT8(r.stencil_clear)};
        }
        for(UINT i=0;i<targets.size();++i)if(targets[i]&&(i!=8||(write_depth_stencil&&!depth_attachment_snapshot))){
            OwnImage(*targets[i]);QueueLinearImageDownload(*targets[i]);
        }
        bindings.Prepare();
        std::set<ID3D12Resource*> feedback_copies;
        for(UINT i=0;i<target_count;++i)if(color_feedback[i]&&gpu_color_feedback[i]&&targets[i]){
            auto& image=*targets[i];auto& source=image.texture;auto* native=source.handle.Get();
            if(!bindings.HasTextureSrvs(native)||!feedback_copies.insert(native).second)continue;
            if(bindings.HasShaderUav(native))throw std::runtime_error("Color feedback with storage-image access needs separate alias handling");
            if(const auto saved=replay_feedback_snapshots.find(native);saved!=replay_feedback_snapshots.end()){
                if(!bindings.RemapTextureSrvs(native,saved->second))
                    throw std::logic_error("Color feedback lost its continuation shader views");
                continue;
            }
            if(!replay_attachments.Beginning())
                throw std::logic_error("Color feedback introduced after logical draw began");
            auto copy=textures_.AllocateColorFeedbackCopy(image,CompletedFence());
            // Retain before recording: both source allocation and copy must
            // survive failure/unwinding and the eventual submission fence.
            RetainUntilFinish([source,copy](u64){(void)source;(void)copy;});
            if(!bindings.SnapshotColorSrvs(RecordingList(),source,copy))
                throw std::logic_error("Color feedback lost its prepared shader views");
            replay_feedback_snapshots.emplace(native,std::move(copy));
            Diagnostics::Count("image-feedback.gpu-snapshot");
        }
        if(!capture)prepare_indices(bindings);
        replay_prepared=true;
    };
    const auto submit_replay=[&](ResourceBindings& bindings,bool final_submission=true,
        const StreamDrawArguments* actual_records=nullptr) {
        prepare_replay(bindings);bindings.Prepare();
        if(!replay_attachments.Beginning())depth.clear_depth=depth.clear_stencil=false;
        static std::set<u64> captured_bound_draws,capture_producers;
        static bool capture_root_added{};
        static u64 capture_bytes{};
        const auto* capture_ps=std::getenv("SHADPS4_NATIVE_DISPLAY_CAPTURE_PS");
        const auto* fragment=guest.stages[u32(Shader::SwStage::Fragment)];
        const auto capture_id=DisplayCapture::next_start.load();
        if(capture_ps&&!capture_root_added&&DisplayCapture::Inputs(capture_id)){
            capture_producers.insert(std::strtoull(capture_ps,nullptr,16));capture_root_added=true;
        }
        const bool capture_bound=capture_ps&&fragment&&captured_bound_draws.size()<8&&
            capture_producers.contains(fragment->info.pgm_hash)&&!captured_bound_draws.contains(fragment->info.pgm_hash);
        std::string pass_name;
        if(capture_bound){std::ostringstream name;name<<"bound-"<<std::hex<<fragment->info.pgm_hash;pass_name=name.str();}
        const auto record_capture=[&](GuestTextureCache::Entry& image,const std::string& role,UINT planes){
            const auto native=image.texture.handle->GetDesc();const auto& d=image.desc;
            std::ostringstream metadata;
            metadata<<"group="<<capture_id<<" role="<<role<<" address="<<std::hex<<d.address
                <<" resource="<<image.texture.handle.Get()<<" allocation="<<image.texture.allocation.get()
                <<" pass_ps="<<fragment->info.pgm_hash<<" writer_vs="<<image.diagnostic_vs
                <<" writer_ps="<<image.diagnostic_ps<<std::dec<<" uid="<<image.image_uid
                <<" writer="<<image.diagnostic_writer<<" shader_planes="<<planes
                <<" width="<<d.width<<" height="<<d.height<<" pitch="<<d.pitch
                <<" tile="<<d.tile_mode<<" data="<<d.data_format<<" number="<<d.number_format
                <<" bank_swizzle="<<d.bank_swizzle<<" alt_tile="<<d.alt_tile
                <<" levels="<<d.levels<<" layers="<<d.layers<<" first_mip="<<d.first_mip
                <<" first_layer="<<d.first_layer<<" samples="<<d.samples<<" depth="<<d.depth_target
                <<" owned="<<image.gpu_owned<<" alias="<<image.buffer_alias_version<<" pending="<<image.pending
                <<" fence_before="<<image.fence<<" native_format="<<native.Format;
            const auto name=pass_name+"-"+role+"-"+std::to_string(image.image_uid);
            const bool two_planes=native.Format==DXGI_FORMAT_R32G8X24_TYPELESS||
                native.Format==DXGI_FORMAT_D32_FLOAT_S8X24_UINT||native.Format==DXGI_FORMAT_R24G8_TYPELESS||
                native.Format==DXGI_FORMAT_D24_UNORM_S8_UINT;
            // ShaderPlanes is a conservative DS hazard mask; color SRVs also
            // carry mask3. The native format determines copy plane count.
            if(native.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||native.MipLevels!=1||
               native.DepthOrArraySize!=1||native.SampleDesc.Count!=1||d.stencil_address||two_planes){
                DisplayCapture::Save(capture_id,name,{},metadata.str()+" readback=unsupported-subresources");return;
            }
            const auto prior=image.texture.state;
            auto transfer=TextureTransfer::Create(device_.Get(),image.texture,true);
            if(transfer.total_bytes>256*1024*1024-capture_bytes){
                DisplayCapture::Save(capture_id,name,{},metadata.str()+" readback=capture-budget");return;
            }
            capture_bytes+=transfer.total_bytes;
            bindings.Retain(transfer.staging.handle.Get());
            transfer.Readback(RecordingList(),image.texture);image.texture.Transition(RecordingList(),prior);
            bound_captures.push_back({name,metadata.str(),std::move(transfer)});
        };
        if(capture_bound){
            captured_bound_draws.insert(fragment->info.pgm_hash);bound_capture_id=capture_id;
            auto buffer_captures=bindings.CaptureBufferInputs(RecordingList(),4096,
                std::min<u64>(256*1024,256*1024*1024-capture_bytes),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            for(auto& buffer:buffer_captures){
                const auto kind=buffer.type==D3D12_DESCRIPTOR_RANGE_TYPE_UAV?"uav":"cbv";
                std::ostringstream metadata;
                metadata<<"group="<<capture_id<<" role=buffer-input pass_ps="<<std::hex<<fragment->info.pgm_hash
                    <<" resource="<<buffer.source.Get()<<std::dec<<" type="<<kind<<" reg="<<buffer.reg
                    <<" space="<<buffer.space<<" index="<<buffer.index<<" offset="<<buffer.offset
                    <<" view_bytes="<<buffer.view_bytes<<" captured_bytes="<<buffer.bytes;
                auto name=pass_name+"-buffer-"+kind+"-"+std::to_string(buffer.reg)+"-"+
                    std::to_string(buffer.space)+"-"+std::to_string(buffer.index);
                capture_bytes+=buffer.bytes;
                bound_buffer_captures.push_back({std::move(name),metadata.str(),std::move(buffer)});
            }
            std::unordered_set<ID3D12Resource*> actual_inputs;
            auto texture_captures=bindings.CaptureTextureInputs(RecordingList(),256*1024*1024-capture_bytes);
            for(auto& input:texture_captures){
                const auto native=input.source.handle->GetDesc();actual_inputs.insert(input.source.handle.Get());
                std::ostringstream metadata;
                metadata<<"group="<<capture_id<<" role=input pass_ps="<<std::hex<<fragment->info.pgm_hash
                    <<" resource="<<input.source.handle.Get()<<std::dec<<" srv_reg="<<input.reg
                    <<" space="<<input.space<<" index="<<input.index<<" view_format="<<input.view.Format
                    <<" view_dimension="<<input.view.ViewDimension<<" width="<<native.Width<<" height="<<native.Height
                    <<" levels="<<native.MipLevels<<" layers="<<native.DepthOrArraySize<<" samples="<<native.SampleDesc.Count
                    <<" native_format="<<native.Format;
                const auto cached=std::ranges::find_if(textures_.Entries(),[&](const auto& image){return image->texture.handle.Get()==input.source.handle.Get();});
                if(cached!=textures_.Entries().end()){
                    metadata<<" address="<<std::hex<<(*cached)->desc.address<<" writer_ps="<<(*cached)->diagnostic_ps
                        <<std::dec<<" uid="<<(*cached)->image_uid;
                    if((*cached)->diagnostic_ps)capture_producers.insert((*cached)->diagnostic_ps);
                }else metadata<<" cache_entry=not-enumerated";
                auto name=pass_name+"-srv-"+std::to_string(input.reg)+"-"+std::to_string(input.space)+"-"+std::to_string(input.index);
                if(!input.omitted.empty()){
                    DisplayCapture::Save(capture_id,name,{},metadata.str()+" readback="+input.omitted);continue;
                }
                metadata<<" subresource="<<input.transfer.subresource<<" copied_width="<<input.transfer.footprint.Footprint.Width
                    <<" copied_height="<<input.transfer.footprint.Footprint.Height;
                capture_bytes+=input.transfer.total_bytes;
                bound_captures.push_back({std::move(name),metadata.str(),std::move(input.transfer)});
            }
            UINT captured_inputs{};
            for(auto& entry:textures_.Entries())if(const auto planes=bindings.ShaderPlanes(entry->texture.handle.Get());planes&&
                (entry->desc.levels!=1||entry->desc.layers!=1||
                 bindings.ShaderReferences(entry->texture.handle.Get(),0,0,0)||
                 bindings.ShaderReferences(entry->texture.handle.Get(),0,0,1))&&captured_inputs++<16){
                if(entry->diagnostic_ps)capture_producers.insert(entry->diagnostic_ps);
                if(!actual_inputs.contains(entry->texture.handle.Get()))record_capture(*entry,"input",planes);
            }
            // ROP reads of destination contents are dependencies too. A blur
            // can add black bloom to an already fogged target; its sampled
            // texture alone does not identify the scene color producer.
            for(UINT i=0;i<target_count;++i)if(targets[i]){
                if(targets[i]->diagnostic_ps)capture_producers.insert(targets[i]->diagnostic_ps);
                record_capture(*targets[i],"target-before"+std::to_string(i),0);
            }
            std::ostringstream sharps;
            for(UINT i=0;i<target_count;++i)if(targets[i]){
                const auto& blend=state.pipeline.BlendState.RenderTarget[i];
                sharps<<"target="<<i<<" blend="<<blend.BlendEnable<<" logic="<<blend.LogicOpEnable
                    <<" src="<<blend.SrcBlend<<" dst="<<blend.DestBlend<<" op="<<blend.BlendOp
                    <<" src_alpha="<<blend.SrcBlendAlpha<<" dst_alpha="<<blend.DestBlendAlpha
                    <<" alpha_op="<<blend.BlendOpAlpha<<" write_mask="<<unsigned(blend.RenderTargetWriteMask)<<"\n";
            }
            for(const auto& stage:stages)for(const auto& image:stage.info->images){
                const auto sharp=image.GetSharp(*stage.info);if(!sharp)continue;
                const auto d=DescribeGuestTexture(sharp);
                sharps<<"stage_hash="<<std::hex<<stage.info->pgm_hash<<" address="<<d.address<<std::dec
                    <<" writable="<<image.is_written<<" width="<<d.width<<" height="<<d.height
                    <<" pitch="<<d.pitch<<" tile="<<d.tile_mode<<" data="<<d.data_format<<" number="<<d.number_format
                    <<" mip="<<d.first_mip<<" levels="<<d.levels<<" layer="<<d.first_layer<<" view_layers="<<d.view_layers<<"\n";
            }
            for(const auto& stage:stages)for(size_t i=0;i<stage.info->buffers.size();++i){
                const auto& buffer=stage.info->buffers[i];
                sharps<<"buffer_stage_hash="<<std::hex<<stage.info->pgm_hash<<std::dec<<" slot="<<i
                    <<" type="<<unsigned(buffer.buffer_type)<<" writable="<<buffer.is_written;
                if(buffer.buffer_type==Shader::BufferType::Guest){const auto sharp=buffer.GetSharp(*stage.info);
                    sharps<<" address="<<std::hex<<sharp.base_address<<std::dec<<" bytes="<<sharp.GetSize()
                        <<" stride="<<sharp.stride<<" records="<<sharp.num_records;}
                sharps<<"\n";
            }
            DisplayCapture::Save(capture_id,pass_name+"-requested-views",{},sharps.str());
            DisplayCapture::Save(capture_id,pass_name+"-fragment.spv",std::as_bytes(fragment->EffectiveSpirv()),"Effective production fragment SPIR-V");
            const auto& dxil=shaders_.Graphics(guest).stages[u32(Shader::SwStage::Fragment)].bytecode;
            DisplayCapture::Save(capture_id,pass_name+"-fragment.dxil",std::as_bytes(std::span(dxil)),"Linked production fragment DXIL");
        }
        if(probe){
            probe_results.push_back(Buffer(device_.Get(),sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS),
                D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST));
            RecordingList()->BeginQuery(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
        }
        ExecuteGraphics(pipeline,bindings,state,
            std::span(attachments.data(),pipeline.TargetFormats().size()),has_depth?&depth:nullptr,
            expanded_points?point_viewports:viewport.viewports,viewport.scissors,r.blend_constants,output_draw,{},
            !capture&&index_view.resource?&index_view:nullptr,false,actual_records);
        if(probe){
            RecordingList()->EndQuery(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0);
            RecordingList()->ResolveQueryData(probe_heap.Get(),D3D12_QUERY_TYPE_PIPELINE_STATISTICS,0,1,probe_results.back().handle.Get(),0);
        }
        replay_attachments.Submitted();
        for(UINT i=0;i<target_count;++i)if(targets[i]&&(state.ordered_color_mask&(1u<<i))&&
            descriptions[i].samples==1)targets[i]->texture.UavBarrier(RecordingList());
        for(const auto& transfer:ordered_multisample){
            for(auto& plane:transfer.entry->storage_samples)plane.UavBarrier(RecordingList());
            transfer.lease->Record(RecordingList(),transfer.entry->texture,
                transfer.entry->storage_samples,transfer.entry->storage_linear,true);
        }
        // A recorded attachment writer changes original GpuModified history,
        // even when recreation copied a CPU-initialized (GpuModified0) source.
        for(UINT i=0;i<target_count;++i)if(targets[i])targets[i]->NoteLogicalImageWrite();
        if(targets[8]&&write_depth_stencil&&(!depth_attachment_snapshot||depth_binding_plan))targets[8]->NoteLogicalImageWrite();
        if(std::getenv("SHADPS4_NATIVE_DISPLAY_CAPTURE")){
            static u64 writer{};const auto serial=++writer;
            const auto* vs=guest.stages[u32(Shader::SwStage::Vertex)];
            const auto* ps=guest.stages[u32(Shader::SwStage::Fragment)];
            for(UINT i=0;i<target_count;++i)if(targets[i]){
                targets[i]->diagnostic_writer=serial;
                targets[i]->diagnostic_vs=vs?vs->info.pgm_hash:0;
                targets[i]->diagnostic_ps=ps?ps->info.pgm_hash:0;
            }
        }
        if(capture_bound)for(UINT i=0;i<target_count;++i)if(targets[i])
            record_capture(*targets[i],"output"+std::to_string(i),0);
        // Keep readback recording inside the resource callback, before its
        // post-draw ownership finalization, as in the original single draw.
        // A multipart caller records only the final attachment contents.
        if(final_submission){
            std::set<GuestTextureCache::Entry*> downloaded_targets;
            for(UINT i=0;i<targets.size();++i)if(targets[i]&&!targets[i]->gpu_owned&&(i!=8||write_depth_stencil)&&
                downloaded_targets.insert(targets[i]).second){
                readbacks.push_back(std::make_unique<GuestImageReadback>(device_.Get(),*targets[i],RecordingList()));
                readback_rows[i]=true;
            }
        }
    };
    if(capture&&capture->stream_patch_vertices){
        const auto& geometry=capture->producer.stages[u32(Shader::SwStage::Geometry)]->runtime.hw.gs;
        // Auxiliary patches have fixed unit factors; original HS/DS factors
        // require the complete hardware-tessellation bound instead. Preserve
        // the original producer draw even for incomplete/empty patches.
        u32 input_width=3;
        if(capture->stream_tessellated)switch(geometry.in_primitive){
        case AmdGpu::PrimitiveType::PointList:input_width=1;break;
        case AmdGpu::PrimitiveType::LineList:input_width=2;break;
        case AmdGpu::PrimitiveType::TriangleList:input_width=3;break;
        default:throw std::invalid_argument("Captured tessellation has an invalid generated primitive topology");
        }
        const auto range=capture->stream_tessellated?
            TessellatedPointStreamRange(capture->stream_patch_vertices,draw.count,draw.instances,geometry.in_vertex_data_size,input_width):
            AuxiliaryPointStreamRange(capture->stream_patch_vertices,draw.count,draw.instances,geometry.in_vertex_data_size);
        const u32 stride=range.stride;
        const auto one=Shader::NativePointCapture::Plan::Make(1,geometry.num_invocations,geometry.output_vertices,1);
        const u64 capture_budget=std::max<u64>(64*1024*1024,one.bytes);
        const PrimitiveStreamPartitions partitions(range,geometry.num_invocations,geometry.output_vertices,capture_budget);
        const auto stream_layout=pipelines_.PrimitiveStreamLayout(capture->stream_producer);
        auto stream_state=NativePointCapturePass::ProducerState(source_state);
        stream_state.pipeline.StreamOutput=stream_layout.Desc();
        const auto& stream_pipeline=pipelines_.Graphics(capture->stream_producer,stream_state.pipeline);
        const auto consumer_state=NativePointCapturePass::StreamConsumerState(source_state,range.vertices_per_primitive);
        const auto& consumer_pipeline=pipelines_.Graphics(capture->producer,consumer_state.pipeline);
        std::vector<GuestResourceStage> logical_stages;
        for(const auto* stage:capture->stream_producer.stages)if(stage)logical_stages.push_back({&stage->info,stage->bindings_start});
        for(const auto* stage:capture->producer.stages)if(stage)logical_stages.push_back({&stage->info,stage->bindings_start});
        for(const auto* stage:capture->replay.stages)if(stage)logical_stages.push_back({&stage->info,stage->bindings_start});
        const std::array<const ShaderPipeline*,3> domains{&stream_pipeline,&consumer_pipeline,&pipeline};
        const auto logical_layout=pipelines_.LogicalLayout(domains);
        if(!primitive_stream_)primitive_stream_=std::make_unique<PrimitiveStreamPass>(device_.Get());
        if(!point_capture_)point_capture_=std::make_unique<NativePointCapturePass>(device_.Get());
        ExecuteGuestGraphicsResources(device_.Get(),*this,stream_pipeline,logical_stages,push,runtime,
            [&](ResourceBindings& common){
                // Attachment preparation can grow B. Freeze the common shader
                // views only after it has selected/initialized the targets.
                prepare_replay(common);
                auto arena=point_capture_->Reserve(*this,partitions.At(0).capture.bytes);
                const auto stream=primitive_stream_->Capture(*this,stride,range.capacity_vertices,range.vertices_per_primitive,
                    [&](PrimitiveStreamLease& lease){
                    auto bindings=common.ForkForReplay(stream_pipeline.Layout());
                    RetainUntilFinish([bindings](u64){});
                    prepare_indices(*bindings);
                    if(index_view.resource){bindings->Retain(index_view.resource->handle.Get());bindings->RetainAllocation(index_view.resource->allocation);}
                    const StreamOutputBinding target{&lease.data,&lease.filled_size,0,u64(stride)*range.capacity_vertices,0};
                    ExecuteGraphics(stream_pipeline,*bindings,stream_state,{},nullptr,viewport.viewports,viewport.scissors,
                        r.blend_constants,draw,{},index_view.resource?&index_view:nullptr,false,nullptr,&target);
                });
                auto local_runtime=runtime;local_runtime.first_vertex=local_runtime.base_instance=local_runtime.indexed=0;
                primitive_stream_->Replay(*this,stream,geometry.num_invocations,geometry.output_vertices,capture_budget,
                    [&](PrimitiveStreamLease& lease,const PrimitiveStreamPartition& part){
                        const StreamDrawArguments actual_records{&lease.arguments,sizeof(D3D12_DRAW_ARGUMENTS)};
                        const NativePointCapturePass::IndirectClear clear_dispatch{&lease.arguments,32};
                        point_capture_->Execute(*this,part.capture,DrawArguments{part.input.limit,1},consumer_state.topology,
                            [&](Resource& buffer,const DrawArguments& bounds,u64 offset,u64 bytes){
                                auto bindings=common.ForkForReplay(consumer_pipeline.Layout());
                                BindGuestGraphicsRuntime(*bindings,local_runtime);RetainUntilFinish([bindings](u64){});
                                NativePointCapturePass::BindProducer(*bindings,buffer,offset,bytes);
                                NativePointCapturePass::BindInputStream(*bindings,lease.data,u64(part.input.first)*stride,u64(part.input.limit)*stride);
                                const StreamDrawArguments indirect{&lease.arguments,0};
                                ExecuteGraphics(consumer_pipeline,*bindings,consumer_state,{},nullptr,viewport.viewports,
                                    viewport.scissors,r.blend_constants,bounds,{},nullptr,false,&indirect);
                            },[&](Resource& buffer,const DrawArguments& replay){
                                output_draw=replay;auto bindings=common.ForkForReplay(pipeline.Layout());
                                BindGuestGraphicsRuntime(*bindings,local_runtime);RetainUntilFinish([bindings](u64){});
                                NativePointCapturePass::BindReplay(*bindings,buffer,part.capture.bytes);
                                submit_replay(*bindings,part.last,&actual_records);
                            },arena,&actual_records,&clear_dispatch);
                    });
            },textures_,r.ta_bc_base.Address(),index_requests,true,logical_layout);
    }else if(capture){
        const auto& geometry=capture->producer.stages[u32(Shader::SwStage::Geometry)]->runtime.hw.gs;
        // No GS invocation exists for an incomplete primitive, but retain one
        // invalid slot so the original VS producer still executes unchanged.
        const auto primitive_bound=std::max(1u,NativePointCapturePass::PrimitiveBound(source_state.topology,draw.count));
        const auto plan=Shader::NativePointCapture::Plan::Make(primitive_bound,geometry.num_invocations,geometry.output_vertices,draw.instances);
        const auto producer_state=NativePointCapturePass::ProducerState(source_state);
        const auto& producer_pipeline=pipelines_.Graphics(capture->producer,producer_state.pipeline);
        std::vector<GuestResourceStage> producer_stages,replay_stages;
        for(const auto* stage:capture->producer.stages)if(stage)producer_stages.push_back({&stage->info,stage->bindings_start});
        for(const auto* stage:capture->replay.stages)if(stage)replay_stages.push_back({&stage->info,stage->bindings_start});
        if(!point_capture_)point_capture_=std::make_unique<NativePointCapturePass>(device_.Get());
        point_capture_->Execute(*this,plan,draw,source_state.topology,[&](Resource& buffer,const DrawArguments& instance,u64 offset,u64 bytes){
            auto producer_runtime=runtime;producer_runtime.base_instance=instance.first_instance;
            ExecuteGuestGraphicsResources(device_.Get(),*this,producer_pipeline,producer_stages,push,producer_runtime,
                [&](ResourceBindings& bindings){
                    NativePointCapturePass::BindProducer(bindings,buffer,offset,bytes);
                    bindings.Prepare();
                    prepare_indices(bindings);
                    if(index_view.resource){bindings.Retain(index_view.resource->handle.Get());bindings.RetainAllocation(index_view.resource->allocation);}
                    ExecuteGraphics(producer_pipeline,bindings,producer_state,{},nullptr,viewport.viewports,
                        viewport.scissors,r.blend_constants,instance,{},index_view.resource?&index_view:nullptr,false);
                },textures_,r.ta_bc_base.Address(),index_requests);
        },[&](Resource& buffer,const DrawArguments& replay){
            output_draw=replay;auto replay_runtime=runtime;
            replay_runtime.first_vertex=replay_runtime.base_instance=replay_runtime.indexed=0;
            // Prepare fragment resources after original guest writes, so alias
            // ownership resolution cannot leave pre-producer SRVs cached here.
            ExecuteGuestGraphicsResources(device_.Get(),*this,pipeline,replay_stages,push,replay_runtime,
                [&](ResourceBindings& bindings){NativePointCapturePass::BindReplay(bindings,buffer,plan.bytes);submit_replay(bindings);},
                textures_,r.ta_bc_base.Address());
        });
    }else if(batch){
        const auto plan=GuestIndirectPackPlan::Make(indexed,batch->stride,batch->max_count,batch->with_runtime);
        ExecuteGuestGraphicsResources(device_.Get(),*this,pipeline,stages,push,runtime,
            [&](ResourceBindings& bindings){
                // Select attachments/index storage and freeze the complete B
                // envelope before borrowing its authoritative native inputs.
                prepare_replay(bindings);bindings.Prepare();
                if(!plan.with_runtime){
                    // A shader-independent argument/count version can be
                    // consumed directly, exactly as the original indirect
                    // buffer read. Project out lifetime-only B references;
                    // any actual shader alias still takes the private pack.
                    auto direct=bindings.ForkForReplay(pipeline.Layout());
                    const auto view=PreparedDmaBuffer(batch->address,plan.source_bytes);
                    const auto counter_view=batch->count_address?PreparedDmaBuffer(batch->count_address,4):std::nullopt;
                    const auto native=view?NativeIndirectLayout::Make(indexed,view->offset,batch->stride,batch->max_count):NativeIndirectLayout{};
                    if(view&&(!batch->count_address||counter_view)&&!direct->References(view->resource.Get())&&
                       (!counter_view||!direct->References(counter_view->resource.Get()))&&
                       native.offset<=view->resource->GetDesc().Width&&native.bytes<=view->resource->GetDesc().Width-native.offset){
                        const auto source=BorrowPreparedShaderBuffer(batch->address,plan.source_bytes);
                        const auto counter=batch->count_address?std::optional{BorrowPreparedShaderBuffer(batch->count_address,4)}:std::nullopt;
                        const StreamDrawArguments packet{source.resource,source.offset,batch->max_count,batch->stride,
                            counter?counter->resource:nullptr,counter?counter->offset:0};
                        submit_replay(*direct,true,&packet);
                        RetainUntilFinish([direct](u64){});return;
                    }
                }
                if(!indirect_pack_)indirect_pack_=std::make_unique<GuestIndirectPackPass>(device_.Get());
                auto commands=AcquireTransientBuffer(plan.command_bytes);
                Resource constants;if(plan.with_runtime)constants=AcquireTransientBuffer(plan.runtime_bytes);
                auto count=AcquireTransientBuffer(4);
                const auto source=BorrowPreparedShaderBuffer(batch->address,plan.source_bytes);
                const auto counter=batch->count_address?std::optional{BorrowPreparedShaderBuffer(batch->count_address,4)}:std::nullopt;
                indirect_pack_->Record(*this,plan,*source.resource,source.offset,
                    counter?counter->resource:nullptr,counter?counter->offset:0,runtime,commands,plan.with_runtime?&constants:nullptr,count);
                const StreamDrawArguments packet{&commands,0,batch->max_count,plan.command_stride,&count,0,plan.with_runtime?&constants:nullptr};
                submit_replay(bindings,true,&packet);
            },textures_,r.ta_bc_base.Address(),index_requests,true);
    }else ExecuteGuestGraphicsResources(device_.Get(),*this,pipeline,stages,push,runtime,submit_replay,
        textures_,r.ta_bc_base.Address(),index_requests);
    if(bound_capture_id){
        Finish();ValidateDebugMessages();
        for(auto& capture:bound_buffer_captures){
            auto& transfer=capture.transfer;void* data{};D3D12_RANGE range{0,SIZE_T(transfer.bytes)};
            Check(transfer.readback.handle->Map(0,&range,&data));
            SCOPE_EXIT {D3D12_RANGE none{};transfer.readback.handle->Unmap(0,&none);};
            DisplayCapture::Save(bound_capture_id,capture.name,
                {static_cast<const std::byte*>(data),size_t(transfer.bytes)},capture.metadata+
                " completed="+std::to_string(CompletedFence()));
        }
        for(auto& readback:bound_captures){
            auto& transfer=readback.transfer;
            std::vector<std::byte> bytes(size_t(transfer.row_bytes)*transfer.rows);
            transfer.ReadRows(bytes.data(),bytes.size(),transfer.row_bytes);
            DisplayCapture::Save(bound_capture_id,readback.name,bytes,readback.metadata+
                " completed="+std::to_string(CompletedFence())+" row_bytes="+std::to_string(transfer.row_bytes)+
                " footprint_format="+std::to_string(transfer.footprint.Footprint.Format));
        }
    }
    if(probe){
        Finish();
        D3D12_QUERY_DATA_PIPELINE_STATISTICS statistics{};
        for(auto& result:probe_results){
            void* data{};D3D12_RANGE range{0,sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS)};
            Check(result.handle->Map(0,&range,&data));
            const auto sample=*static_cast<const D3D12_QUERY_DATA_PIPELINE_STATISTICS*>(data);
            D3D12_RANGE empty{};result.handle->Unmap(0,&empty);
            statistics.IAVertices+=sample.IAVertices;statistics.IAPrimitives+=sample.IAPrimitives;
            statistics.VSInvocations+=sample.VSInvocations;statistics.GSInvocations+=sample.GSInvocations;
            statistics.GSPrimitives+=sample.GSPrimitives;statistics.CInvocations+=sample.CInvocations;
            statistics.CPrimitives+=sample.CPrimitives;statistics.PSInvocations+=sample.PSInvocations;
            statistics.HSInvocations+=sample.HSInvocations;statistics.DSInvocations+=sample.DSInvocations;
            statistics.CSInvocations+=sample.CSInvocations;
        }
        static FILE* output=[](){std::filesystem::create_directories("user/log");return fopen(("user/log/scene-probe-"+std::to_string(GetCurrentProcessId())+".txt").c_str(),"w");}();
        if(output){
            const auto* vs=guest.stages[u32(Shader::SwStage::Vertex)];
            const auto* ps=guest.stages[u32(Shader::SwStage::Fragment)];
            const auto& vp=viewport.viewports[0];const auto& sc=viewport.scissors[0];
            fprintf(output,"sample=%u indexed=%u count=%u instances=%u vs=%llx ps=%llx cb=%llx targets=%u mrt=%x color_mask=%x depth=%u write=%u func=%u cull=%u flip=%x viewport=%g,%g,%g,%g scissor=%ld,%ld,%ld,%ld IA=%llu primitives=%llu VS=%llu clip_in=%llu clip_out=%llu PS=%llu\n",
                ++probe_count,unsigned(indexed),draw.count,draw.instances,
                (unsigned long long)(vs?vs->info.pgm_hash:0),(unsigned long long)(ps?ps->info.pgm_hash:0),
                (unsigned long long)r.color_buffers[0].Address(),target_count,guest.mrt_mask,r.color_target_mask.raw,
                unsigned(state.pipeline.DepthStencilState.DepthEnable),unsigned(state.pipeline.DepthStencilState.DepthWriteMask),
                unsigned(state.pipeline.DepthStencilState.DepthFunc),unsigned(state.pipeline.RasterizerState.CullMode),viewport.flip_mask,
                vp.TopLeftX,vp.TopLeftY,vp.Width,vp.Height,sc.left,sc.top,sc.right,sc.bottom,
                (unsigned long long)statistics.IAVertices,(unsigned long long)statistics.IAPrimitives,
                (unsigned long long)statistics.VSInvocations,(unsigned long long)statistics.CInvocations,
                (unsigned long long)statistics.CPrimitives,(unsigned long long)statistics.PSInvocations);
            fflush(output);
        }else ++probe_count;
    }
    if(trace){fprintf(stderr,"DX12 startup draw %u: native draw completed\n",traced_draws);fflush(stderr);}
    // Keep game-visible memory coherent until the shared native image cache is
    // integrated with guest memory faults. This path prioritizes correctness.
    diagnostic.Stage("attachment-writeback");
    try {
        if(!readbacks.empty()) {
            const auto fence=Flush();for(auto& rb:readbacks)rb->Submitted(fence);
            RetireGuestAttachmentReadbacks(*this,textures_,fence,targets);
            replay_attachments.ReleasePins();
            if(readback_rows[8]&&depth_attachment_snapshot){
                // Resolve preceding buffer owners before padding-preserving
                // plane commits. The unselected plane is neither read nor written
                // by Resolve, so its newer buffer version cannot be overwritten.
                if((depth_write_planes&1)&&!ReadMemory(descriptions[8].address,layouts[8].guest_bytes))
                    throw std::out_of_range("Native depth writeback lost its backing");
                if((depth_write_planes&2)&&!ReadMemory(stencil_desc.address,stencil_layout.guest_bytes))
                    throw std::out_of_range("Native stencil writeback lost its backing");
            }
            size_t n{};
            for(UINT i=0;i<targets.size();++i)if(readback_rows[i])readbacks[n++]->Resolve(CompletedFence(),
                std::span(reinterpret_cast<std::byte*>(descriptions[i].address),size_t(layouts[i].guest_bytes)),
                i==8?stencil_memory:std::span<std::byte>{},i==8&&depth_attachment_snapshot?depth_write_planes:3);
            for(UINT i=0;i<targets.size();++i)if(auto* target=targets[i];readback_rows[i]&&!target->desc.writable) {
                // The read-only plane is an older version than the buffer
                // writer's guest contents. Never certify the entire texture
                // against newly written guest bytes after a partial commit.
                if(i==8&&depth_attachment_snapshot){target->sampled_snapshot.clear();target->stencil_snapshot.clear();continue;}
                const auto* memory=reinterpret_cast<const std::byte*>(target->desc.address);
                target->RememberSnapshot({memory,size_t(target->layout.guest_bytes)},i==8?stencil_memory:std::span<std::byte>{});
            }
        }
    }catch(...){try{Finish();}catch(...){}throw;}
    replay_attachments.ReleasePins();
    if(trace){fprintf(stderr,"DX12 startup draw %u: guest memory reconciled\n",traced_draws);fflush(stderr);}
}
void GuestRasterizer::DrawIndirect(bool indexed,VAddr address,u32 offset,u32 stride,
    u32 max_count,VAddr count_address,u16 vertex_sgpr,u16 instance_sgpr) {
    std::lock_guard lock(execution_mutex_);
    if(!max_count)return;
    // Validate the command ABI before reading the optional GPU count. The
    // actual count, not max_count, determines the argument memory extent.
    (void)GuestIndirectLayout::Make(indexed,address,offset,stride,0);
    const auto& r=gpu_.regs;
    const bool strip=r.primitive_type==AmdGpu::PrimitiveType::TriangleStrip||r.primitive_type==AmdGpu::PrimitiveType::LineStrip;
    const bool native_restart=!indexed||!r.enable_primitive_restart||(strip&&
        r.primitive_restart_index==(r.index_buffer_type.index_type==AmdGpu::IndexType::Index16?0xffffu:0xffffffffu));
    bool native=ClassifyGuestDraw(r)==GuestDrawOperation::Rasterize&&!GuestPrimitiveNeedsExpansion(r.primitive_type)&&
        !(r.depth_render_override.force_z_dirty&&r.depth_render_override.force_z_valid)&&
        !(r.depth_render_override.force_stencil_dirty&&r.depth_render_override.force_stencil_valid)&&
        native_restart&&(!indexed||r.index_buffer_type.swap_mode==AmdGpu::IndexSwapMode::None)&&
        (address+offset)%4==0&&count_address%4==0;
    if(native){
        // Private raw-buffer addressing is bounded; larger batches retain
        // the complete CPU path rather than clipping commands to fit.
        try{const auto plan=GuestIndirectPackPlan::Make(indexed,stride,max_count,false);
            if(address+offset>UINT64_MAX-plan.source_bytes)native=false;}
        catch(const std::overflow_error&){native=false;}
    }
    if(native){
        GuestGraphicsStages stages;
        try{stages=frontend_.BuildGraphics(gpu_,vertex_sgpr,instance_sgpr,true);}
        catch(const Shader::NativePointCapture::Required&){native=false;}
        if(native){
            const auto binding=pipelines_.RuntimeBinding(stages);
            const bool runtime=binding==GraphicsRuntimeBinding::Root;
            if(binding!=GraphicsRuntimeBinding::Unsupported){
                try{(void)GuestIndirectPackPlan::Make(indexed,stride,max_count,runtime);}
                catch(const std::overflow_error&){native=false;}
                if(native){const NativeIndirectBatch batch{address+offset,count_address,stride,max_count,runtime};
                    DrawInternal(indexed,0,nullptr,vertex_sgpr,instance_sgpr,&batch);return;}
            }
        }
    }
    u32 count=max_count;
    if(count_address) {
        if(!ReadMemory(count_address,4))throw std::out_of_range("Unmapped native indirect count");
        ValidateHostMemory(count_address,4);
        u32 actual{};memcpy(&actual,reinterpret_cast<const void*>(count_address),4);count=std::min(count,actual);
    }
    if(!count)return;
    const auto layout=GuestIndirectLayout::Make(indexed,address,offset,stride,count);
    // ReadMemory resolves relevant image/buffer writes and waits only when
    // required; clean CPU arguments do not need an unconditional queue drain.
    if(!ReadMemory(layout.address,layout.bytes))throw std::out_of_range("Unmapped native indirect arguments");
    ValidateHostMemory(layout.address,layout.bytes);
    const auto snapshot=layout.Snapshot({reinterpret_cast<const std::byte*>(layout.address),size_t(layout.bytes)});
    // Snapshot every command before the first draw can write guest memory.
    for(const auto& command:snapshot) {
        const DrawArguments draw{command.count,command.instances,command.first_vertex,command.first_instance,command.base_vertex};
        DrawInternal(indexed,command.first_index,&draw,vertex_sgpr,instance_sgpr);
    }
}
bool GuestRasterizer::ComputeImageOperation(const Shader::Info& info,const u32 (&groups)[3]) {
    // Mirror Vulkan's IsComputeImageCopy/Clear recognition, with stricter
    // storage and ownership checks for native DX12 memory coherence.
    const auto& cs=gpu_.GetCsRegs();
    if(cs.num_thread_x.full!=64||groups[1]!=1||groups[2]!=1||
       cs.start_x||cs.start_y||cs.start_z||!info.images.empty())return false;
    // Native translation retains an extra read-only Flatbuf descriptor used
    // to fetch resource metadata. It is not a third guest data buffer.
    const Shader::BufferResource* guest_buffers[2]{};unsigned buffer_count{};
    for(const auto& buffer:info.buffers){
        if(buffer.IsSpecial()){if(buffer.buffer_type!=Shader::BufferType::Flatbuf||buffer.is_written)return false;continue;}
        if(buffer_count==2)return false;guest_buffers[buffer_count++]=&buffer;
    }
    if(buffer_count!=2)return false;
    const auto& a=*guest_buffers[0];const auto& b=*guest_buffers[1];
    const auto ba=a.GetSharp(info),bb=b.GetSharp(info);
    const auto find=[&](u64 address,u64 size,bool owned=false,const GuestTextureCache::Entry* like=nullptr){
        GuestTextureCache::Entry* result{};
        for(auto& entry:textures_.Entries())if(entry->desc.address==address&&entry->layout.guest_bytes==size){
            if((owned&&!entry->gpu_owned)||(like&&!GuestImageCopyCompatible(*like,*entry)))continue;
            if(owned&&result)return static_cast<GuestTextureCache::Entry*>(nullptr);
            if(!result||entry->gpu_owned||(!result->gpu_owned&&entry->last_use>result->last_use))result=entry.get();
        }
        return result;
    };
    bool copied=false;
    if(a.is_formatted&&b.is_formatted&&a.is_written!=b.is_written&&
       ba.GetSize()==bb.GetSize()&&u64(groups[0])*256==ba.GetSize()){
        auto* src=find(a.is_written?bb.base_address:ba.base_address,ba.GetSize(),true);
        if(!src)return false;
        auto* dst=find(a.is_written?ba.base_address:bb.base_address,ba.GetSize(),false,src);
        if(!dst)return false;
        for(auto& entry:textures_.Entries())if(entry.get()!=dst&&entry->gpu_owned&&ImageOverlap(dst->desc.address,dst->layout.guest_bytes,*entry))ResolveImage(*entry);
        if(!OwnImage(*dst))return false;
        if(!WholeGuestImage(*src)){
            // Texture CopyResource copies logical texels, not PS4 allocation
            // padding. Physical backing carries untouched padding even while
            // GPU ownership withholds access to the current logical texels.
            // Seed destination backing from it; later ResolveImage overlays
            // copied GPU texels while retaining the exact source padding.
            std::vector<std::byte> backing(size_t(src->layout.guest_bytes));
            if(!read_backing_(src->desc.address,backing)||!write_backing_(dst->desc.address,backing))
                throw std::runtime_error("Cannot preserve padded guest GPU image copy backing");
        }
        textures_.SetBackingSamples(*src,src->desc.samples,RecordingList());
        textures_.SetBackingSamples(*dst,dst->desc.samples,RecordingList());
        auto* list=RecordingList();
        NotifyOnSubmission([this](u64 fence){textures_.Submitted(fence);});
        src->pending=dst->pending=true;
        RecordGuestImageCopy(*src,*dst,list,true);copied=true;
    }else if(!a.is_formatted&&!a.is_written&&b.is_formatted&&b.is_written&&ba.GetSize()==16&&
        u64(groups[0])*128*(AmdGpu::NumBitsPerBlock(bb.GetDataFmt())/8)==bb.GetSize()){
        if(!ReadMemory(ba.base_address,16))return false;
        ValidateHostMemory(ba.base_address,16);float values[4];memcpy(values,reinterpret_cast<const void*>(ba.base_address),16);
        // Pinned IsComputeImageClear targets a compatible cached image, even
        // for an all-zero vector. Without that image the actual shader runs;
        // a physical CPU fill would change the separate buffer/image domains
        // and guest padding that an image clear does not write.
        auto* dst=find(bb.base_address,bb.GetSize());
        if(dst&&dst->storage_retired){
            // Pressure can retire reconstructible sample-UAV scratch while
            // retaining the authoritative image. Restore its guest sample
            // backing before recreating scratch, as normal compute binds do.
            textures_.SetBackingSamples(*dst,dst->desc.samples,RecordingList());
            textures_.EnsureMultisampleStorage(*dst);
        }
        if(!dst||dst->layout.bytes_per_pixel*8!=AmdGpu::NumBitsPerBlock(bb.GetDataFmt())||
           (!GuestImageClearCompatible(*dst)&&!GuestStorageImageClearCompatible(*dst,values)&&
            !GuestMultisampleStorageClearCompatible(*dst,values)&&
             !GuestConvertedStorageClearCompatible(*dst)&&
             !GuestConvertedMultisampleClearCompatible(*dst,values)))return false;
        if(!OwnImageClear(*dst))return false;
        // The cache may currently expose a different native sample backing.
        // Clear the complete guest resource, then let later backing changes
        // copy from that authoritative cleared sample count.
        textures_.SetBackingSamples(*dst,dst->desc.samples,RecordingList());
        // Begin can collect prior submissions. Publish this pending write only
        // after its recording list exists, and stamp it at actual submission.
        auto* list=RecordingList();
        NotifyOnSubmission([this](u64 fence){textures_.Submitted(fence);});
        dst->pending=true;
        if(GuestStorageImageClearCompatible(*dst,values)){
            auto lease=RecordGuestStorageImageClear(device_.Get(),*dst,list,values);
            RetainUntilFinish([lease=std::move(lease)](u64){});
        }else if(GuestMultisampleStorageClearCompatible(*dst,values)){
            RecordGuestMultisampleStorageClear(device_.Get(),textures_,*dst,list,values);
        }else if(GuestImageClearCompatible(*dst)){
            auto heap=RecordGuestImageClear(device_.Get(),*dst,list,values);
            RetainUntilFinish([heap=std::move(heap)](u64){});
        }else{
            textures_.ClearRawStorageImage(*dst,values,list,CompletedFence());
        }
    }else return false;
    // Retirement may run after a later list has begun recording. It can free
    // completed leases, but must not relabel newer pending work with this fence.
    RetainUntilFinish([this](u64 fence){if(fence)textures_.Collect(fence);});
    static u64 copies{},clears{};const auto count=copied?++copies:++clears;
    if(count<=8||count%1024==0){fprintf(stderr,"DX12 native image %s count=%llu shader=%llx bytes=%llu\n",
        copied?"copy":"clear",(unsigned long long)count,(unsigned long long)info.pgm_hash,
        (unsigned long long)bb.GetSize());fflush(stderr);}
    return true;
}
void GuestRasterizer::Dispatch(const u32 (&groups)[3]) {
    Diagnostics::Scope diagnostic(Diagnostics::Kind::Dispatch,"execution-lock");
    diagnostic.Detail("groups=%u,%u,%u",groups[0],groups[1],groups[2]);
    std::lock_guard lock(execution_mutex_);
    ValidateIndependentImageWatches();
    // PS4 zero-dimensional commands perform no work, unlike D3D12's validated
    // nonempty path. Do not translate/read a shader for a no-op command.
    if(!groups[0]||!groups[1]||!groups[2])return;
    const auto& program=gpu_.GetCsRegs();
    ComputeInvocation invocation{{groups[0],groups[1],groups[2]},
        {program.start_x,program.start_y,program.start_z}};
    diagnostic.Stage("shader-translation");
    auto& guest=frontend_.BuildCompute(gpu_);
    if(Diagnostics::Enabled()&&groups[0]>=60000){
        static std::vector<u64> observed;
        if(observed.size()<32&&std::find(observed.begin(),observed.end(),guest.info.pgm_hash)==observed.end()){
            observed.push_back(guest.info.pgm_hash);
            fprintf(stderr,"DX12 compute candidate hash=%llx threads=%u groups=%u buffers=%zu images=%zu\n",
                (unsigned long long)guest.info.pgm_hash,program.num_thread_x.full,groups[0],guest.info.buffers.size(),guest.info.images.size());
            for(const auto& buffer:guest.info.buffers){const auto sharp=buffer.GetSharp(guest.info);
                fprintf(stderr,"DX12 compute buffer special=%u formatted=%u written=%u address=%llx bytes=%llu fmt=%u/%u\n",
                    unsigned(buffer.IsSpecial()),unsigned(buffer.is_formatted),unsigned(buffer.is_written),
                    (unsigned long long)sharp.base_address,(unsigned long long)sharp.GetSize(),unsigned(sharp.GetDataFmt()),unsigned(sharp.GetNumberFmt()));}
            fflush(stderr);
        }
    }
    diagnostic.Detail("groups=%u,%u,%u shader=%llu buffers=%zu images=%zu",groups[0],groups[1],groups[2],
        (unsigned long long)guest.id,guest.info.buffers.size(),guest.info.images.size());
    diagnostic.Stage("image-operation-recognition");
    if(ComputeImageOperation(guest.info,groups))return;
    diagnostic.Stage("copy-shader-hle");
    if(ExecuteGuestCopyShader(*this,guest.info,invocation))return;
    // Keep original metadata-clear recognition. CMASK also runs the guest
    // writer so its physical tile codes remain available to CPU inspection
    // and partial fast-clear materialization.
    const bool reads_meta=std::any_of(guest.info.buffers.begin(),guest.info.buffers.end(),[&](const auto& b){
        return !b.IsSpecial()&&!b.is_written&&textures_.GetMetaInfo(b.GetSharp(guest.info).base_address).has_value();
    });
    diagnostic.Stage("metadata-clear-recognition");
    if(!reads_meta&&!guest.info.has_bitwise_xor){
        for(const auto& b:guest.info.buffers)if(!b.IsSpecial()&&b.is_written){
            const auto address=b.GetSharp(guest.info).base_address;
            const auto meta=textures_.GetMetaInfo(address);
            if(meta&&textures_.ClearMeta(address)){
                if(meta->type!=GuestTextureCache::MetaType::CMask)return;
                break;
            }
        }
    }
    const bool scratch=std::any_of(guest.info.buffers.begin(),guest.info.buffers.end(),
        [](const auto& b){return b.buffer_type==Shader::BufferType::SharedMemory;});
    if(scratch) {
        u64 bytes=program.SharedMemSize();
        for(auto dim:groups){if(bytes>UINT64_MAX/u64(dim))throw std::out_of_range("Native guest shared-memory dispatch size overflow");bytes*=dim;}
        invocation.shared_bytes=bytes;
    }
    diagnostic.Stage("pipeline-creation");
    const auto& pipeline=pipelines_.Compute(guest);
    diagnostic.Stage("resource-upload-dispatch-writeback");
    ExecuteGuestBufferCompute(device_.Get(),*this,pipeline,guest.info,guest.bindings_start,invocation,&textures_,
        gpu_.regs.ta_bc_base.Address());
}
void GuestRasterizer::DispatchDirect() {
    const auto& program=gpu_.GetCsRegs();const u32 groups[]{program.dim_x,program.dim_y,program.dim_z};
    try {Dispatch(groups);}
    catch(const std::exception& e){fprintf(stderr,"Native DX12 Dispatch failed: %s\n",e.what());fflush(stderr);throw;}
}
void GuestRasterizer::DispatchIndirect(VAddr address,u32 offset,u32 size) {
    std::lock_guard lock(execution_mutex_);
    if(size<12||address>UINT64_MAX-offset||address+offset>UINT64_MAX-12)
        throw std::out_of_range("Native indirect compute argument range");
    const auto source=address+offset;
    if(source%4||!ReadMemory(source,12))throw std::out_of_range("Native indirect compute arguments unmapped/unaligned");
    // Reuse the memory backend's committed/protection checks before CPU access.
    ValidateHostMemory(source,12);Finish();
    u32 groups[3];memcpy(groups,reinterpret_cast<const void*>(source),sizeof(groups));
    Dispatch(groups);
}
}
