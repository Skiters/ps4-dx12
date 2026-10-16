// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "guest_texture_cache.h"
#include "rasterizer.h"
#include <functional>

namespace NativeDX12 {
inline std::optional<uint32_t> GuestFormattedMetadataValue(const GuestTextureCache::MetaDataInfo& meta){
    if(meta.type==GuestTextureCache::MetaType::HTile)return 0xf;
    if(meta.type!=GuestTextureCache::MetaType::CMask)return std::nullopt;
    switch(meta.num_samples){
    case 1:case 8:return 0xffffffff;
    case 2:return 0xdddddddd;
    case 4:return 0xeeeeeeee;
    default:return 0; // Pinned optional remains engaged for unsupported counts.
    }
}
struct GuestStageBindingSpan {
    size_t first_buffer, end_buffer, first_image, end_image;
};
struct OrderedGuestBuffer {
    bool read, formatted, cpu_stream;
    uint32_t byte_adjust, access_alignment;
    uint64_t original_bytes;
};
struct OrderedGuestImage {
    GuestTextureDesc storage;
    GuestTextureLayout layout;
    bool written, null_resource;
};
struct OrderedGuestBindings {
    std::vector<Rasterizer::ShaderBufferView> buffers;
    std::vector<std::optional<size_t>> stream_views;
    std::vector<GuestTextureCache::Entry*> images;
    std::shared_ptr<std::vector<std::shared_ptr<void>>> pins;
};
// Materialize one stable buffer batch, then replay the original per-stage
// BindBuffers -> BindTextures cache operations before recording a single draw.
OrderedGuestBindings PrepareOrderedGuestStages(ID3D12Device*, Rasterizer&, GuestTextureCache&,
    std::span<const GuestStageBindingSpan>, std::vector<Rasterizer::ShaderBufferRequest>&,
    std::span<const OrderedGuestBuffer>, std::span<const OrderedGuestImage>, bool exact_buffer_bounds,
    const std::function<VAddr(VAddr,uint64_t)>& private_cpu_stream,
    std::span<const Rasterizer::ShaderBufferRequest> dma_reads={});
}
