# Full emulator integration: actual guest CPU, GCN translator, HLE and SDL input.
if(NOT WIN32)
    message(FATAL_ERROR "Native DirectX 12 core requires Windows")
endif()
set(_native "${PROJECT_SOURCE_DIR}/src/video_core/renderer_d3d12")
set(_dxil "${PROJECT_SOURCE_DIR}/src/shader_recompiler/backend/dxil")
set(SHADPS4_MESA_DXIL_ROOT "${SHADPS4_EXTERNALS_DIR}/mesa-kosmickrisp/externals/mesa" CACHE PATH "Pinned offline Mesa SPIR-V to DXIL source headers")
set(_mesa "${SHADPS4_MESA_DXIL_ROOT}")
set(_mesa_overlay "${PROJECT_SOURCE_DIR}/native-port/vendor/mesa-api")
set(SHADPS4_DXC_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/../fsr-sdk-local/sdk/tools/ffx_shader_compiler/libs/dxc/inc" CACHE PATH "DXC SDK headers")
set(_dxc "${SHADPS4_DXC_INCLUDE_DIR}")
include("${PROJECT_SOURCE_DIR}/native-port/Host-Passes.cmake")
native_host_passes(shadps4)
include("${PROJECT_SOURCE_DIR}/native-port/Guest-Tiling.cmake")
native_guest_tiling(shadps4)
include("${PROJECT_SOURCE_DIR}/native-port/AGS-DX12.cmake")
native_ags_device(shadps4)
include("${PROJECT_SOURCE_DIR}/native-port/Mesa-DXIL.cmake")
native_mesa_runtime(shadps4)
target_sources(shadps4 PRIVATE
    "${_native}/backend_factory.cpp" "${_native}/presenter.cpp" "${_native}/guest_presenter.cpp"
    "${_native}/command_scheduler.cpp"
    "${_native}/memory_allocator.cpp"
    "${_native}/nv_shader_clock.cpp"
    "${_native}/rasterizer.cpp" "${_native}/guest_dma.cpp" "${_native}/resource_bindings.cpp" "${_native}/shader_pipeline.cpp"
    "${_native}/guest_graphics_state.cpp" "${_native}/graphics_submission.cpp" "${_native}/guest_viewports.cpp"
    "${_native}/guest_texture_cache.cpp" "${_native}/guest_image_readback.cpp" "${_native}/memory_surface_provider.cpp"
    "${_native}/guest_compute.cpp" "${_native}/guest_stage_bindings.cpp"
    "${_native}/guest_rasterizer.cpp" "${_native}/guest_backend.cpp"
    "${_native}/guest_shader_frontend.cpp" "${_native}/guest_shader_cache.cpp" "${_native}/guest_pipeline_cache.cpp"
    "${_dxil}/spirv_converter.cpp" "${_dxil}/linked_spirv_converter.cpp"
    "${PROJECT_SOURCE_DIR}/native-port/vendor/imgui_impl_dx12.cpp"
    "${PROJECT_SOURCE_DIR}/src/video_core/guest_shader_runtime.cpp")
target_sources(shadps4 PRIVATE "${PROJECT_SOURCE_DIR}/src/video_core/screenshot.cpp")
target_sources(shadps4 PRIVATE "${PROJECT_SOURCE_DIR}/src/video_core/cache_storage.cpp"
    "${PROJECT_SOURCE_DIR}/src/shader_recompiler/serialization.cpp"
    "${_native}/guest_pipeline_persistence.cpp")
target_sources(shadps4 PRIVATE "${_native}/guest_shader_debug.cpp")
target_include_directories(shadps4 PRIVATE "${_dxc}" "${_mesa_overlay}" "${SHADPS4_EXTERNALS_DIR}/imgui/backends"
    "${_mesa}/src/microsoft/spirv_to_dxil" "${_mesa}/src/microsoft/compiler"
    "${SHADPS4_EXTERNALS_DIR}/aacdec/fdk-aac/libAACdec/src")
target_compile_definitions(shadps4 PRIVATE SHADPS4_NATIVE_DX12_CORE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(shadps4 PRIVATE d3d12 dxgi ole32 d3dcompiler)

# Fail configuration if a future source-list edit restores the removed renderer.
get_target_property(_native_core_sources shadps4 SOURCES)
foreach(_source IN LISTS _native_core_sources)
    if(_source MATCHES "renderer_vulkan|imgui_impl_vulkan|dlss_layer|video_core/(buffer_cache|texture_cache|page_manager)")
        message(FATAL_ERROR "Legacy renderer source is forbidden in the native DX12 fork: ${_source}")
    endif()
endforeach()
if(SDL_VULKAN OR SDL_RENDER_VULKAN)
    message(FATAL_ERROR "Native DX12 fork requires SDL Vulkan support disabled")
endif()
target_link_options(shadps4 PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/native-core.map"
    "/DEBUG:FULL" "/PDB:${CMAKE_CURRENT_BINARY_DIR}/shadPS4-DX12-Core.pdb")
set_target_properties(shadps4 PROPERTIES OUTPUT_NAME "shadPS4-DX12-Core")
