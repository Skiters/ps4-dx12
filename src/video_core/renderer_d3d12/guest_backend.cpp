#include "common/api_trace.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "guest_backend.h"
#include "guest_rasterizer.h"
#include "memory_surface_provider.h"
#include "shader_recompiler/native_dma.h"
#include "shader_recompiler/backend/dxil/spirv_converter.h"
#include "nv_shader_clock.h"
#include "ags_shader_clock.h"
namespace NativeDX12 {
std::unique_ptr<Backend> CreateGuestBackend(const BackendConfiguration& config,AmdGpu::Liverpool& gpu,
    std::wstring converter,std::wstring dxc) {
    if(config.backend!="dx12"||!config.window||!config.width||!config.height)
        throw std::invalid_argument("Invalid native guest backend configuration");
    auto result=std::make_unique<Backend>();result->device=CreateHardwareDevice(&result->driver_lifetime,config.device);
    result->capabilities=Capabilities::Query(result->device.Get());const auto& caps=result->capabilities;
    const bool wave32=caps.min_wave_lanes<=32&&caps.max_wave_lanes>=32;
    const bool wave64=caps.min_wave_lanes<=64&&caps.max_wave_lanes>=64;
    if(caps.shader_model<D3D_SHADER_MODEL_6_6||!caps.wave_operations||(!wave32&&!wave64))
        throw std::runtime_error("Native guest translator requires SM6.6 and a supported 32- or 64-lane compute wave");
    if(caps.resource_binding_tier<D3D12_RESOURCE_BINDING_TIER_3)
        throw std::runtime_error("Native guest DMA descriptor address space requires resource binding tier 3");
    Shader::Profile profile{};profile.max_viewport_width=profile.max_viewport_height=16384;
    profile.max_shared_memory_size=32768;profile.supported_spirv=0x00010500;
    profile.subgroup_size=caps.min_wave_lanes==64?64:32;profile.support_int8=profile.support_int16=true;
    profile.support_int64=true;profile.support_float16=caps.native_16bit;
    profile.support_float64=caps.native_float64;
    const auto native_features=Shader::Backend::DX12::ConverterNativeFeatures(converter);
    profile.native_compute_wave_size=wave64&&(native_features&Shader::Backend::DX12::NativeFeatureComputeWave64)?64:wave32?32:0;
    if(!profile.native_compute_wave_size)throw std::runtime_error("Native compiler does not provide the adapter's compute wave size");
    if(NvShaderClockAvailable(result->device.Get())&&(native_features&Shader::Backend::DX12::NativeFeatureNvShaderClock))
        profile.native_shader_clock_backend=1;
    else if(AmdShaderClockAvailable(result->device.Get())&&(native_features&Shader::Backend::DX12::NativeFeatureAmdShaderClock))
        profile.native_shader_clock_backend=2;
    profile.supports_shader_subgroup_clock=profile.native_shader_clock_backend!=0;
    profile.supports_shader_stencil_export=caps.pixel_shader_stencil_ref;
    profile.supports_buffer_int64_atomics=caps.buffer_int64_atomics;
    profile.supports_fragment_shader_barycentric=caps.barycentrics;
    profile.needs_manual_interpolation=caps.barycentrics;
    profile.needs_lds_barriers=true;profile.needs_buffer_offsets=true;
    profile.inline_fetch_shader=true;
    profile.native_dma_buffers=true;profile.sparse_page_shift=Shader::NativeDma::PageShift;
    profile.native_coherent_images=true;
    profile.native_viewport_clip_conversion=true;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(DX12_API_CALL(result->device->CreateCommandQueue(&q,IID_PPV_ARGS(&result->presentation_queue))));
    result->rasterizer=std::make_unique<GuestRasterizer>(result->device.Get(),gpu,profile,std::move(converter),std::move(dxc));
    result->rasterizer->SetMarkersEnabled(config.device.host_markers,config.device.guest_markers);
    result->owned_surfaces=std::make_unique<MemorySurfaceProvider>(result->device.Get(),result->presentation_queue.Get(),
        *result->rasterizer,config.width,config.height);
    result->presenter=std::make_unique<GuestPresenter>(result->device.Get(),result->presentation_queue.Get(),config.window,
        config.width,config.height,*result->owned_surfaces);
    static_cast<GuestPresenter*>(result->presenter.get())->SetHostMarkersEnabled(config.device.host_markers);
    return result;
}
}
