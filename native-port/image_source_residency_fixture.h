// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "video_core/renderer_d3d12/guest_texture_cache.h"
#include "video_core/renderer_d3d12/guest_dma.h"
#include "common/scope_exit.h"

namespace NativeDX12 {
// Source-authored final-suite control, not run by /Zs.
inline void RunImageSourceResidencyFixture(ID3D12Device* device,Rasterizer& renderer,VAddr base){
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    GuestTextureCache cache(device);GuestDmaMemory semantic(nullptr);
    // Cache uploads retain recorded transfers until this scope's real fence.
    SCOPE_EXIT {renderer.Finish();};
    GuestTextureDesc desc;desc.address=base;desc.width=desc.height=desc.pitch=4;
    desc.data_format=4;desc.number_format=4;desc.tile_mode=31;
    const auto layout=LayoutGuestTexture(desc);
    std::vector<std::byte> bytes(size_t(layout.guest_bytes),std::byte{0x22});
    unsigned publications{};bool fail=false;
    cache.SetBufferResidencyPublisher([&](auto ranges){
        ++publications;if(fail)throw std::runtime_error("Injected image-source publication failure");
        for(const auto& [address,size]:ranges){
            const Rasterizer::ShaderBufferRequest request{address,size,false,true,true};
            semantic.ObserveResidentSharps(std::span(&request,1));
        }
    });
    const auto upload=[&](GuestTextureCache::UploadPurpose purpose)->GuestTextureCache::Entry&{
        return cache.Upload(desc,bytes,renderer.RecordingList(),renderer.CompletedFence(),{},false,nullptr,false,false,purpose);
    };
    auto& prepared=upload(GuestTextureCache::UploadPurpose::Preparation);
    const auto uid=prepared.image_uid;
    // Represents the receipt emitted by the production GPU B-seed callback.
    const GuestTextureCache::BufferResidency receipt{{base,layout.guest_bytes}};
    prepared.pending_buffer_residency=receipt;
    require(&upload(GuestTextureCache::UploadPurpose::Preparation)==&prepared&&publications==0&&
        semantic.ResidentIntersections(base,layout.guest_bytes).empty()&&prepared.pending_buffer_residency==receipt,
        "Cache prewarm/reuse consumed image B-source residency before its bind");
    require(&upload(GuestTextureCache::UploadPurpose::Binding)==&prepared&&publications==1&&
        prepared.pending_buffer_residency.empty()&&!semantic.ResidentIntersections(base,layout.guest_bytes).empty(),
        "First real image bind omitted its prepared B-source residency");
    (void)upload(GuestTextureCache::UploadPurpose::Binding);
    require(publications==1,"Cached image binding repeated an already consumed residency effect");
    prepared.pending_buffer_residency=receipt;fail=true;bool rejected=false;
    try{(void)upload(GuestTextureCache::UploadPurpose::Binding);}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&prepared.pending_buffer_residency==receipt,"Publication failure lost the image's retry receipt");
    fail=false;(void)upload(GuestTextureCache::UploadPurpose::Binding);
    require(publications==3&&prepared.pending_buffer_residency.empty(),"Image-source publication could not retry");
    renderer.Finish();cache.Submitted(renderer.CompletedFence());
    prepared.pending_buffer_residency=receipt;bytes.front()=std::byte{0x33};
    auto& overwritten=upload(GuestTextureCache::UploadPurpose::Binding);
    require(overwritten.image_uid==uid&&overwritten.pending_buffer_residency.empty()&&publications==3,
        "Overwriting I published a receipt belonging to its preceding physical contents");
    renderer.Finish();renderer.ValidateDebugMessages();
}
}
