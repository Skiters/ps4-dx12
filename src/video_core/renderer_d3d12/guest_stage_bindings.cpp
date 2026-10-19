// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_stage_bindings.h"
#include "guest_image_ownership.h"
#include "guest_dma.h"
#include "diagnostics.h"
#include <algorithm>
#include <map>

namespace NativeDX12 {
namespace {
bool Overlaps(VAddr a,uint64_t as,VAddr b,uint64_t bs){
    if(a>UINT64_MAX-as||b>UINT64_MAX-bs)throw std::out_of_range("Ordered stage resource extent overflow");
    return as&&bs&&a<b+bs&&b<a+as;
}
void RefreshBoundImage(Rasterizer& rasterizer,GuestTextureCache::Entry& target,
    GuestTextureCache::Entry& source){
    if(&target==&source)return;
    if(target.gpu_owned||source.gpu_owned||target.layout.guest_bytes!=source.layout.guest_bytes||
       !MatchesGuestMipLayout(target.layout,source.layout))
        throw std::logic_error("Ordered bound-image refresh lost its retired physical storage");
    const auto a=target.texture.handle->GetDesc(),b=source.texture.handle->GetDesc();
    if(a.Dimension!=b.Dimension||a.Width!=b.Width||a.Height!=b.Height||a.DepthOrArraySize!=b.DepthOrArraySize||
       a.MipLevels!=b.MipLevels||a.Format!=b.Format||a.SampleDesc.Count!=b.SampleDesc.Count||
       a.SampleDesc.Quality!=b.SampleDesc.Quality)
        throw std::runtime_error("Ordered image views require a common native physical allocation");
    // Vulkan refreshes the same image. Earlier views must observe this refresh,
    // while previously recorded I->B copies retain their earlier place in time.
    target.texture.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
    source.texture.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
    DX12_API_CALL(rasterizer.RecordingList()->CopyResource(target.texture.handle.Get(),source.texture.handle.Get()));
    const auto copy_aux=[&](Resource& destination,Resource& input){
        if(!destination.handle&&!input.handle)return;
        if(!destination.handle||!input.handle||destination.handle->GetDesc().Width!=input.handle->GetDesc().Width)
            throw std::runtime_error("Ordered image refresh lost its exact guest-word backing");
        destination.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_DEST);
        input.Transition(rasterizer.RecordingList(),D3D12_RESOURCE_STATE_COPY_SOURCE);
        DX12_API_CALL(rasterizer.RecordingList()->CopyResource(destination.handle.Get(),input.handle.Get()));
    };
    copy_aux(target.expanded_guest_backing,source.expanded_guest_backing);
    copy_aux(target.expanded_written_mask,source.expanded_written_mask);
    target.logical_gpu_modified=source.logical_gpu_modified;target.logical_dirty=source.logical_dirty;
    target.pending_buffer_residency=std::move(source.pending_buffer_residency);
    target.buffer_alias_version=target.buffer_alias_watch=false;target.linear_download_pending=false;
    target.sampled_snapshot.clear();target.stencil_snapshot.clear();target.pending=true;
    // The transient upload is a source for the bound allocation, not a second
    // authoritative independent I at the same address.
    source.MarkLogicalDirty();source.linear_download_pending=false;
}
}

OrderedGuestBindings PrepareOrderedGuestStages(ID3D12Device*,Rasterizer& rasterizer,
    GuestTextureCache& textures,std::span<const GuestStageBindingSpan> stages,
    std::vector<Rasterizer::ShaderBufferRequest>& requests,std::span<const OrderedGuestBuffer> buffers,
    std::span<const OrderedGuestImage> images,bool exact_buffer_bounds,
    const std::function<VAddr(VAddr,uint64_t)>& private_cpu_stream){
    std::lock_guard lock(rasterizer.ExecutionMutex());
    if(stages.empty()||buffers.size()>requests.size())throw std::invalid_argument("Invalid ordered guest stage set");
    size_t next_buffer=0,next_image=0;
    for(const auto& stage:stages){
        if(stage.first_buffer!=next_buffer||stage.first_image!=next_image||
           stage.end_buffer<stage.first_buffer||stage.end_buffer>buffers.size()||
           stage.end_image<stage.first_image||stage.end_image>images.size())
            throw std::invalid_argument("Ordered stages do not partition original resource metadata");
        next_buffer=stage.end_buffer;next_image=stage.end_image;
    }
    if(next_buffer!=buffers.size()||next_image!=images.size())
        throw std::invalid_argument("Ordered stages omit original resource slots");
    std::vector<OrderedGuestImage> physical_images;
    for(size_t i=0;i<images.size();++i)if(!images[i].null_resource){
        const auto storage=textures.ResolveDepthViewStorage(images[i].storage);
        if(storage!=images[i].storage){
            if(physical_images.empty())physical_images.assign(images.begin(),images.end());
            physical_images[i].storage=storage;physical_images[i].layout=LayoutGuestTexture(storage);
        }
    }
    if(!physical_images.empty())images=std::span<const OrderedGuestImage>(physical_images);
    OrderedGuestBindings result;result.stream_views.resize(buffers.size());result.images.resize(images.size());
    result.pins=std::make_shared<std::vector<std::shared_ptr<void>>>();
    struct Domain {GuestTextureDesc desc;GuestTextureLayout layout;GuestTextureCache::Entry* bound{};
        uint64_t generation{},bound_generation{},initial_bytes{};Resource initial_tiled;
        struct ConvertedView {GuestTextureDesc desc;GuestTextureLayout layout;
            GuestTextureCache::Entry* bound{};uint64_t bound_generation{};};
        std::vector<ConvertedView> converted_views;
        bool has_converted_views{};
        std::shared_ptr<const GuestTextureCache::CachedImageBindingPlan> cached_plan;};
    std::map<VAddr,Domain> domains;
    const auto add_domain=[&](const GuestTextureDesc& desc,const GuestTextureLayout& layout,bool cached=false){
        auto [it,inserted]=domains.try_emplace(desc.address,Domain{desc,layout});
        if(!inserted){auto& old=it->second;
            if(cached)if(auto* original=textures.FindImageRecreationBindingSource(old.desc,old.layout);
                original&&original->desc==desc)return; // Original I, not the future domain class.
            if(old.desc.samples==1&&CompatibleGuestDepthSubview(old.desc,old.layout,desc,layout))return;
            if(desc.samples==1&&CompatibleGuestDepthSubview(desc,layout,old.desc,old.layout)){
                old.desc=desc;old.layout=layout;return;
            }
            if(!CompatibleGuestImageMipGeometry(old.desc,old.layout,desc,layout))
                throw std::runtime_error("Ordered stages have incompatible image physical geometry");
            const auto levels=std::max(old.desc.levels,desc.levels);const bool writable=old.desc.writable||desc.writable;
            if(!cached&&(desc.levels>old.desc.levels||desc.writable&&!old.desc.writable))old.desc=desc;
            old.desc.levels=levels;old.desc.writable=writable;old.layout=LayoutGuestTexture(old.desc);
        }
    };
    for(const auto& image:images)if(!image.null_resource&&
        (image.storage.writable||std::ranges::any_of(textures.Entries(),[&](const auto& cached){return cached->logical_gpu_modified&&
            (CompatibleGuestImageMipGeometry(image.storage,image.layout,cached->desc,cached->layout)||
             (image.storage.samples==1&&CompatibleGuestDepthSubview(cached->desc,cached->layout,image.storage,image.layout)));}))&&
        std::ranges::any_of(requests,[&](const auto& buffer){
        return Overlaps(image.storage.address,image.layout.guest_bytes,buffer.address,buffer.size);}))
        add_domain(image.storage,image.layout);
    for(const auto& entry:textures.Entries())if(entry->gpu_owned&&entry->HasValidLogicalImage()&&
        std::ranges::any_of(buffers,[&](const auto& buffer){const auto i=size_t(&buffer-buffers.data());
            return buffer.formatted&&!buffer.cpu_stream&&Overlaps(entry->desc.address,entry->layout.guest_bytes,requests[i].address,requests[i].size);}))
        add_domain(entry->desc,entry->layout,true);
    // Complete validation precedes cache mutations, captures or descriptor views.
    for(const auto& image:images)if(!image.null_resource){
        if(auto found=domains.find(image.storage.address);found!=domains.end()){
            auto& domain=found->second;
            if(!CompatibleGuestShaderStorage(domain.desc,image.storage)&&
               !textures.CanShareReadonlyWordStorage(domain.desc,image.storage)&&
               !CompatibleGuestShaderMipPrefixes(domain.desc,domain.layout,image.storage,image.layout)&&
               !(domain.desc.samples==1&&CompatibleGuestDepthSubview(domain.desc,domain.layout,image.storage,image.layout))){
                if(!CompatibleGuestPackedStorage(domain.desc,image.storage)&&
                   !CompatibleGuestBlockWordStorage(domain.desc,domain.layout,image.storage,image.layout))
                    throw std::runtime_error("Ordered stage converted image views require their domain transfer plan");
                domain.has_converted_views=true;
            }
        }
    }
    for(const auto& [address,domain]:domains){
        for(auto other=domains.upper_bound(address);other!=domains.end();++other)
            if(Overlaps(address,domain.layout.guest_bytes,other->first,other->second.layout.guest_bytes))
                throw std::runtime_error("Ordered stages require shifted image-domain ownership adaptation");
        GuestTextureCache::Entry geometry;geometry.desc=domain.desc;geometry.layout=domain.layout;
        if(domain.desc.stencil_address)geometry.stencil_layout=LayoutGuestTexture(DescribeGuestStencil(domain.desc));
        const auto owner=DescribeGuestImageOwnership(geometry);
        if(!owner||!CanRetainGuestImageOwnership(textures,*owner)||
           !GuestTextureCache::CanUploadGpuSnapshot(domain.desc,domain.layout,domain.desc.writable))
            throw std::runtime_error("Ordered stage image domain has no exact independent transfer/ownership");
        for(size_t i=0;i<buffers.size();++i)if(Overlaps(address,domain.layout.guest_bytes,requests[i].address,requests[i].size)){
            const auto original=requests[i].WriteRange();
            if(requests[i].written&&(original.address!=requests[i].address||original.size!=requests[i].size)&&
               (!exact_buffer_bounds||(original.address&(buffers[i].access_alignment-1))))
                throw std::runtime_error("Ordered stage narrow writer lacks exact naturally aligned shader bounds");
            if(buffers[i].formatted&&!buffers[i].cpu_stream&&
               (requests[i].address!=address||buffers[i].byte_adjust))
                throw std::runtime_error("Ordered formatted readers retain the original exact-base image contract");
        }
    }
    for(const auto& image:images)if(image.cached_plan){
        const auto found=domains.find(image.storage.address);
        if(found!=domains.end()&&found->second.desc==image.cached_plan->Storage())
            found->second.cached_plan=image.cached_plan;
    }
    const auto arm=[&]{
        rasterizer.NotifyOnSubmission([&textures](uint64_t fence){textures.Submitted(fence);});
        rasterizer.RetainUntilFinish([pins=result.pins](uint64_t){(void)pins;});
    };
    arm();
    try{
        // This batch contains original shader resources and their explicit
        // host/image envelopes only. Raw DMA B is materialized independently
        // after binding effects; it neither resolves nor acquires an I version.
        // Protect I as a distinct version before preparing/owning overlapping B.
        for(auto& [address,domain]:domains){
            if(!domain.cached_plan)domain.cached_plan=textures.PlanCachedImageBinding(domain.desc,domain.layout);
            if(domain.cached_plan)continue;
            for(const auto& entry:textures.Entries())if(entry->gpu_owned&&textures.CanRevalidateReadonly(*entry)&&
                (CompatibleGuestImageMipGeometry(domain.desc,domain.layout,entry->desc,entry->layout)||
                 (domain.desc.samples==1&&CompatibleGuestDepthSubview(entry->desc,entry->layout,domain.desc,domain.layout)))){
                result.pins->push_back(textures.PinBinding(*entry));
                if(!textures.RevalidateReadonly(*entry))throw std::runtime_error("Ordered stages cannot retain the preceding I");
            }
        }
        for(auto& [address,domain]:domains){
            // A converted view may precede the canonical domain image in the
            // original stage order. Converting here would retire that source I
            // before its descriptor is bound. Exact-size converted domains do
            // not need allocation growth; create each representation at its
            // actual BindTextures position instead.
            if(domain.has_converted_views)continue;
            if(domain.cached_plan){
                // Preserve ORIGINAL I for an earlier formatted BindBuffers.
                // Recreating its future class here would expose new metadata;
                // omitting this witness would instead return unrelated B.
                const bool prior_formatted=[&]{
                    for(const auto& stage:stages){
                        for(size_t i=stage.first_buffer;i<stage.end_buffer;++i)
                            if(buffers[i].formatted&&!buffers[i].cpu_stream&&
                               Overlaps(address,domain.layout.guest_bytes,requests[i].address,requests[i].size))return true;
                        for(size_t i=stage.first_image;i<stage.end_image;++i)
                            if(!images[i].null_resource&&images[i].storage.address==address)return false;
                    }
                    return false;
                }();
                if(auto* original=textures.FindImageRecreationBindingSource(domain.desc,domain.layout);
                    prior_formatted&&original&&original->HasValidLogicalImage()&&original->buffer_alias_version){
                    domain.initial_bytes=original->layout.guest_bytes;
                    auto initial=rasterizer.CaptureShaderBuffer(address,domain.initial_bytes,false);
                    domain.initial_tiled=textures.RetileBufferVersion(*original,initial,
                        rasterizer.RecordingList(),rasterizer.CompletedFence());
                    rasterizer.RetainUntilFinish([snapshot=domain.initial_tiled,plan=domain.cached_plan](uint64_t){(void)snapshot;(void)plan;});
                    arm();
                }
                continue;
            }
            if(auto* previous=textures.FindIndependentUploadSource(domain.desc,domain.layout)){
                // Host allocation growth must precede final B views, but the
                // original I remains the semantic source until BindTextures.
                // Preserve its old prefix/interpretation for earlier buffers;
                // do not retile a future expanded tail into B prematurely.
                if(previous->logical_gpu_modified&&(!MatchesGuestMipLayout(previous->layout,domain.layout)||
                    (!CompatibleGuestShaderStorage(previous->desc,domain.desc)&&
                     !CompatibleGuestDepthSubview(previous->desc,previous->layout,domain.desc,domain.layout)))&&
                   std::ranges::any_of(buffers,[&](const auto& buffer){const auto index=size_t(&buffer-buffers.data());
                       return buffer.formatted&&!buffer.cpu_stream&&
                           Overlaps(address,domain.layout.guest_bytes,requests[index].address,requests[index].size);})){
                    arm();domain.initial_bytes=previous->layout.guest_bytes;
                    auto initial=rasterizer.CaptureShaderBuffer(address,domain.initial_bytes,false);
                    domain.initial_tiled=textures.RetileBufferVersion(*previous,initial,
                        rasterizer.RecordingList(),rasterizer.CompletedFence());arm();
                }
                const auto guest=std::span(reinterpret_cast<const std::byte*>(address),size_t(domain.layout.guest_bytes));
                auto& entry=textures.Upload(domain.desc,guest,rasterizer.RecordingList(),rasterizer.CompletedFence(),
                    {},false,nullptr,false,false,GuestTextureCache::UploadPurpose::Preparation);
                // Preparation must not publish a future readonly I's ownership
                // before the original stage binds its buffers and images.
                if(entry.HasValidLogicalImage()&&!textures.RevalidateReadonly(entry))
                    throw std::runtime_error("Ordered stage grown I cannot retain independent ownership");
                result.pins->push_back(textures.PinBinding(entry));arm();
            }
        }
        // Save clean CPU streams before residency/ownership or any I->B copy.
        // Aliased resident buffers retain one arena, so later copies remain
        // visible through earlier resident descriptors as in the pinned backend.
        std::vector<bool> publish;
        for(const auto& request:requests)publish.push_back(request.publish_dma_residency);
        for(auto& request:requests)request.publish_dma_residency=false;
        std::map<std::tuple<VAddr,uint64_t,bool>,size_t> streams;
        for(size_t i=0;i<buffers.size();++i)if(buffers[i].read&&buffers[i].cpu_stream){
            // Keep the original DWORD envelope and byte adjustment. Copying
            // only sharp bytes would move a sub-DWORD stream under its shader.
            const auto source=requests[i];const auto key=std::tuple{source.address,source.size,source.unordered_access};
            auto [it,inserted]=streams.try_emplace(key,requests.size());
            if(inserted){
                const auto address=private_cpu_stream(source.address,source.size);
                requests.push_back({address,source.size,false,requests[i].unordered_access,true});
                requests.back().publish_dma_residency=false;
            }
            result.stream_views[i]=it->second;
        }
        for(const auto& [address,domain]:domains){
            bool uav=false;for(const auto& request:requests)if(Overlaps(address,domain.layout.guest_bytes,request.address,request.size))uav|=request.unordered_access;
            requests.push_back({address,domain.layout.guest_bytes,false,uav,true});
            requests.back().publish_dma_residency=false;
        }
        result.buffers=rasterizer.PrepareShaderBuffers(requests,std::span<const Rasterizer::BufferWrite>{});
        rasterizer.DmaMemory().PublishPreparedFaults(rasterizer);
        // Auxiliary inputs retain their caller's original residency intent.
        for(size_t i=buffers.size();i<publish.size();++i)if(publish[i]){
            auto request=requests[i];request.publish_dma_residency=true;
            rasterizer.DmaMemory().ObserveResidentSharps(std::span(&request,1));
        }
        for(const auto& stage:stages){
            if(stage.end_buffer>buffers.size()||stage.end_image>images.size())throw std::invalid_argument("Ordered stage span overflows metadata");
            for(size_t i=stage.first_buffer;i<stage.end_buffer;++i){
                const auto& request=requests[i];const auto& buffer=buffers[i];const auto original=request.WriteRange();
                if(publish[i]&&!result.stream_views[i]){
                    auto resident=request;resident.publish_dma_residency=true;
                    rasterizer.DmaMemory().ObserveResidentSharps(std::span(&resident,1));
                }
                const auto state=request.unordered_access?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                if(buffer.formatted&&!result.stream_views[i]){
                    bool metadata=false;
                    if(const auto meta=textures.GetMetaInfo(original.address)){
                        const auto value=GuestFormattedMetadataValue(*meta);
                        if(value){
                            if(buffer.byte_adjust)throw std::runtime_error("Ordered metadata synthesis requires a DWORD-aligned sharp");
                            rasterizer.FillPreparedShaderBuffer(request.address,request.size,*value,state);metadata=true;
                        }
                    }
                    if(!metadata)for(auto& [address,domain]:domains)if(Overlaps(address,domain.layout.guest_bytes,original.address,original.size)){
                        const bool old_prefix=!domain.bound&&!domain.generation&&domain.initial_tiled.handle;
                        auto* image=old_prefix?nullptr:textures.FindValidIndependentImage(domain.desc,domain.layout);
                        // Current readonly I can be retained without an
                        // original image write. It is not SafeToDownload.
                        if(!old_prefix&&(!image||!image->logical_gpu_modified))continue;
                        if(address!=original.address)throw std::runtime_error("Ordered formatted read aliases an image subresource");
                        const auto bytes=old_prefix?domain.initial_bytes:image->layout.guest_bytes;
                        if(((address+bytes-1)>>32)>((original.address+original.size-1)>>32))
                            throw std::runtime_error("Ordered formatted transfer exceeds the reader's original VA arena");
                        arm();
                        if(old_prefix)rasterizer.CopyResidentImageToPreparedShaderBuffer(address,bytes,domain.initial_tiled,state);
                        else{
                            auto initial=rasterizer.CapturePreparedShaderBuffer(address,bytes);
                            auto tiled=textures.RetileBufferVersion(*image,initial,rasterizer.RecordingList(),rasterizer.CompletedFence());
                            rasterizer.CopyResidentImageToPreparedShaderBuffer(address,bytes,tiled,state);
                        }
                        arm();
                    }
                }
                if(request.written){
                    rasterizer.MarkShaderBufferWrites(std::span(&request,1));
                    rasterizer.NotifyGpuBufferWrite(original.address,original.size);
                    // Pinned InvalidateMemoryFromGPU deliberately considers
                    // exact image bases only; CPU dirtying is range based.
                    if(auto domain=domains.find(original.address);domain!=domains.end())++domain->second.generation;
                }
            }
            for(size_t i=stage.first_image;i<stage.end_image;++i){
                const auto& image=images[i];if(image.null_resource)continue;
                auto found=domains.find(image.storage.address);if(found==domains.end())continue;
                auto& domain=found->second;
                const bool canonical=CompatibleGuestShaderStorage(domain.desc,image.storage)||
                    textures.CanShareReadonlyWordStorage(domain.desc,image.storage)||
                    CompatibleGuestShaderMipPrefixes(domain.desc,domain.layout,image.storage,image.layout)||
                    (domain.desc.samples==1&&CompatibleGuestDepthSubview(domain.desc,domain.layout,image.storage,image.layout));
                Domain::ConvertedView* converted{};
                if(!canonical){
                    if(!CompatibleGuestPackedStorage(domain.desc,image.storage)&&
                       !CompatibleGuestBlockWordStorage(domain.desc,domain.layout,image.storage,image.layout))
                        throw std::runtime_error("Ordered image view lost its validated converted domain");
                    auto it=std::ranges::find_if(domain.converted_views,[&](const auto& view){
                        return view.desc==image.storage;});
                    if(it==domain.converted_views.end()){
                        domain.converted_views.push_back({image.storage,image.layout});
                        converted=&domain.converted_views.back();
                    }else converted=&*it;
                }
                GuestTextureCache::Entry* entry=converted?converted->bound:domain.bound;
                auto& bound_generation=converted?converted->bound_generation:domain.bound_generation;
                if(!entry||bound_generation!=domain.generation){
                    arm();GuestTextureCache::Entry* refreshed{};
                    if(converted){
                        if(!domain.generation&&textures.FindIndependentUploadSource(image.storage,image.layout)){
                            const auto guest=std::span(reinterpret_cast<const std::byte*>(image.storage.address),
                                size_t(image.layout.guest_bytes));
                            refreshed=&textures.Upload(image.storage,guest,rasterizer.RecordingList(),
                                rasterizer.CompletedFence());
                        }else{
                            auto snapshot=rasterizer.CapturePreparedShaderBuffer(image.storage.address,
                                image.layout.guest_bytes);
                            refreshed=&textures.Upload(image.storage,{},rasterizer.RecordingList(),
                                rasterizer.CompletedFence(),{},true,&snapshot,image.storage.writable,true);
                        }
                    }else if(domain.cached_plan){
                        refreshed=&textures.BindPlannedImage(*domain.cached_plan,rasterizer.RecordingList(),rasterizer.CompletedFence());
                    }else if(textures.FindIndependentUploadSource(domain.desc,domain.layout)){
                        const auto guest=std::span(reinterpret_cast<const std::byte*>(domain.desc.address),size_t(domain.layout.guest_bytes));
                        refreshed=&textures.Upload(domain.desc,guest,rasterizer.RecordingList(),rasterizer.CompletedFence());
                    }else{
                        const Rasterizer::ShaderBufferRequest resident{domain.desc.address,domain.layout.guest_bytes,false,true,true};
                        rasterizer.DmaMemory().ObserveResidentSharps(std::span(&resident,1));
                        auto snapshot=rasterizer.CapturePreparedShaderBuffer(domain.desc.address,domain.layout.guest_bytes);
                        refreshed=&textures.Upload(domain.desc,{},rasterizer.RecordingList(),rasterizer.CompletedFence(),{},
                            true,&snapshot,domain.desc.writable,true);
                    }
                    result.pins->push_back(textures.PinBinding(*refreshed));
                    if(entry)RefreshBoundImage(rasterizer,*entry,*refreshed);
                    else if(converted)entry=converted->bound=refreshed;
                    else entry=domain.bound=refreshed;
                    bound_generation=domain.generation;arm();
                }
                result.images[i]=entry;
                textures.PublishBufferResidency(*entry);
                if(image.written){
                    // The writable image must acquire/demote its actual page
                    // guard before publishing the independent I/B class.  A
                    // pre-set buffer_alias_version makes OwnImage take its
                    // warm fast path and can leave the old exclusive I guard
                    // behind; later image retirement then cannot untrack the
                    // supposedly read-only tail.
                    if(!textures.OwnIndependentShaderImage(*entry))
                        throw std::runtime_error("Ordered stage writable I cannot keep independent ownership");
                }else if(textures.CanRevalidateReadonly(*entry)&&!textures.RevalidateReadonly(*entry))
                    throw std::runtime_error("Ordered stage refreshed readonly I cannot retain its logical history");
            }
        }
        for(const auto& request:requests)rasterizer.TransitionPreparedShaderBuffer(request.address,request.size,
            request.unordered_access?D3D12_RESOURCE_STATE_UNORDERED_ACCESS:
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        arm();return result;
    }catch(...){arm();throw;}
}
}
