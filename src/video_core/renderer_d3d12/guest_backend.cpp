#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_backend.h"
#include "guest_rasterizer.h"
#include "memory_surface_provider.h"
#include "shader_recompiler/native_dma.h"
namespace NativeDX12 {
std::unique_ptr<Backend> CreateGuestBackend(const BackendConfiguration& config,AmdGpu::Liverpool& gpu,
    std::wstring converter,std::wstring dxc) {
    if(config.backend!="dx12"||!config.window||!config.width||!config.height)
        throw std::invalid_argument("Invalid native guest backend configuration");
    auto result=std::make_unique<Backend>();result->device=CreateHardwareDevice();
    result->capabilities=Capabilities::Query(result->device.Get());const auto& caps=result->capabilities;
    if(caps.shader_model<D3D_SHADER_MODEL_6_6||!caps.wave_operations||caps.min_wave_lanes>32||caps.max_wave_lanes<32)
        throw std::runtime_error("Native guest translator requires SM6.6 and a supported 32-lane compute wave");
    if(caps.resource_binding_tier<D3D12_RESOURCE_BINDING_TIER_3)
        throw std::runtime_error("Native guest DMA descriptor address space requires resource binding tier 3");
    Shader::Profile profile{};profile.max_viewport_width=profile.max_viewport_height=16384;
    profile.max_shared_memory_size=32768;profile.supported_spirv=0x00010500;
    profile.subgroup_size=32;profile.support_int8=profile.support_int16=true;
    profile.support_int64=true;profile.support_float16=caps.native_16bit;
    profile.supports_buffer_int64_atomics=caps.buffer_int64_atomics;
    profile.supports_fragment_shader_barycentric=caps.barycentrics;
    profile.needs_manual_interpolation=caps.barycentrics;
    profile.needs_lds_barriers=true;profile.needs_buffer_offsets=true;
    profile.inline_fetch_shader=true;
    profile.native_dma_buffers=true;profile.sparse_page_shift=Shader::NativeDma::PageShift;
    profile.native_coherent_images=true;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(result->device->CreateCommandQueue(&q,IID_PPV_ARGS(&result->presentation_queue))));
    result->rasterizer=std::make_unique<GuestRasterizer>(result->device.Get(),gpu,profile,std::move(converter),std::move(dxc));
    result->owned_surfaces=std::make_unique<MemorySurfaceProvider>(result->device.Get(),result->presentation_queue.Get(),
        *result->rasterizer,config.width,config.height);
    result->presenter=std::make_unique<GuestPresenter>(result->device.Get(),result->presentation_queue.Get(),config.window,
        config.width,config.height,*result->owned_surfaces);
    return result;
}
}
