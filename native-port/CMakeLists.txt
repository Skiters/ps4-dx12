cmake_minimum_required(VERSION 3.24)
project(shadps4_native_dx12_port LANGUAGES CXX)
include(CTest)
if(NOT WIN32)
    message(FATAL_ERROR "Initial native D3D12 backend requires Windows")
endif()
get_filename_component(PORT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
include_directories("${PORT_ROOT}/src")
add_executable(native-api-trace-test api_trace_test.cpp)
target_compile_features(native-api-trace-test PRIVATE cxx_std_20)
add_test(NAME native_api_trace COMMAND native-api-trace-test)
add_executable(native-api-trace-limits-test api_trace_limits_test.cpp)
target_compile_features(native-api-trace-limits-test PRIVATE cxx_std_20)
foreach(mode limit open-failure write-failure)
    add_test(NAME native_api_trace_${mode} COMMAND native-api-trace-limits-test ${mode})
endforeach()
add_executable(native-fair-mutex-test fair_recursive_mutex_test.cpp)
target_compile_features(native-fair-mutex-test PRIVATE cxx_std_20)
add_test(NAME native_fair_recursive_mutex COMMAND native-fair-mutex-test)
set_tests_properties(native_fair_recursive_mutex PROPERTIES TIMEOUT 20)
add_executable(native-page-watch-test page_watch_test.cpp)
target_compile_features(native-page-watch-test PRIVATE cxx_std_20)
add_test(NAME native_page_ownership_and_cache COMMAND native-page-watch-test)
set_tests_properties(native_page_ownership_and_cache PROPERTIES TIMEOUT 20)
set(SHADPS4_DEPENDENCY_ROOT "${PORT_ROOT}/../shadps4-xess-src" CACHE PATH "Read-only build dependencies")
enable_language(C)
add_executable(native-net-epoll-test net_epoll_wait_test.cpp
    "${SHADPS4_DEPENDENCY_ROOT}/externals/ext-wepoll/wepoll.c")
target_include_directories(native-net-epoll-test PRIVATE "${SHADPS4_DEPENDENCY_ROOT}/externals/ext-wepoll")
target_compile_features(native-net-epoll-test PRIVATE cxx_std_20)
target_link_libraries(native-net-epoll-test PRIVATE ws2_32)
add_test(NAME native_net_epoll_wait COMMAND native-net-epoll-test)
set_tests_properties(native_net_epoll_wait PROPERTIES TIMEOUT 10)
add_subdirectory("${SHADPS4_DEPENDENCY_ROOT}/externals/sirit" sirit EXCLUDE_FROM_ALL)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(sirit PRIVATE /WX- -Wno-unused-command-line-argument)
endif()
set(SHADPS4_MESA_CONVERTER_DLL "${PORT_ROOT}/../mesa-dxil-bary-build/src/microsoft/spirv_to_dxil/spirv_to_dxil.dll" CACHE FILEPATH "Native Mesa compiler DLL")
set(SHADPS4_GLSLANG_EXECUTABLE "" CACHE FILEPATH "Standalone GLSL fixture compiler; never linked to the core")
set(SHADPS4_REFERENCE_BUILD_DIR "${SHADPS4_DEPENDENCY_ROOT}/build-xess" CACHE PATH "Read-only fmt headers")
add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
add_compile_options(/utf-8)
add_subdirectory("${PORT_ROOT}/src/shader_recompiler/backend/dxil" dxil)
include("${PORT_ROOT}/src/shader_recompiler/backend/dxil/CMake-Linked.cmake")
add_library(shadps4_dx12_guest_shader OBJECT
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_shader.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_shader_frontend.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_shader_cache.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_pipeline_cache.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_rasterizer.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_backend.cpp"
    "${PORT_ROOT}/src/video_core/guest_shader_runtime.cpp")
target_compile_features(shadps4_dx12_guest_shader PRIVATE cxx_std_23)
target_include_directories(shadps4_dx12_guest_shader PRIVATE
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
add_library(shadps4_native_d3d12 STATIC
    "${PORT_ROOT}/src/video_core/renderer_d3d12/rasterizer.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_dma.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/command_scheduler.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/memory_allocator.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/backend_factory.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_presenter.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/presenter.cpp")
target_compile_features(shadps4_native_d3d12 PUBLIC cxx_std_20)
target_compile_definitions(shadps4_native_d3d12 PUBLIC FMT_HEADER_ONLY)
target_include_directories(shadps4_native_d3d12 PUBLIC
    "${PORT_ROOT}/src" "${SHADPS4_REFERENCE_BUILD_DIR}/_deps/fmt-src/include")
target_link_libraries(shadps4_native_d3d12 PUBLIC d3d12 dxgi)
add_executable(native-diagnostics-test "${CMAKE_CURRENT_SOURCE_DIR}/diagnostics_test.cpp")
target_link_libraries(native-diagnostics-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_diagnostics_nested_threads COMMAND native-diagnostics-test)
target_sources(shadps4_native_d3d12 PRIVATE "${PORT_ROOT}/src/video_core/renderer_d3d12/resource_bindings.cpp")
target_sources(shadps4_native_d3d12 PRIVATE "${PORT_ROOT}/src/video_core/renderer_d3d12/shader_pipeline.cpp")
target_sources(shadps4_native_d3d12 PRIVATE "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_graphics_state.cpp")
add_executable(native-stencil-lowering-test stencil_lowering_test.cpp)
target_link_libraries(native-stencil-lowering-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_stencil_bitwise_lowering COMMAND native-stencil-lowering-test)
target_sources(shadps4_native_d3d12 PRIVATE "${PORT_ROOT}/src/video_core/renderer_d3d12/graphics_submission.cpp")
target_sources(shadps4_native_d3d12 PRIVATE "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_viewports.cpp")
add_executable(dx12-guest-viewports-test "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_viewports_test.cpp")
target_link_libraries(dx12-guest-viewports-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_guest_viewports COMMAND dx12-guest-viewports-test)
target_include_directories(shadps4_native_d3d12 PRIVATE "${SHADPS4_DXC_INCLUDE_DIR}")
add_executable(shadps4_native_dx12_host "${CMAKE_CURRENT_SOURCE_DIR}/host_main.cpp")
set_target_properties(shadps4_native_dx12_host PROPERTIES OUTPUT_NAME "shadPS4-DX12")
target_link_libraries(shadps4_native_dx12_host PRIVATE shadps4_native_d3d12 user32)
add_test(NAME native_host_protocol COMMAND shadps4_native_dx12_host --probe)
add_executable(native-launcher-child-test "${CMAKE_CURRENT_SOURCE_DIR}/launcher_child_test.cpp")
target_compile_features(native-launcher-child-test PRIVATE cxx_std_23)
add_test(NAME native_launcher_forwarding COMMAND "${CMAKE_COMMAND}"
    "-DROOT=${CMAKE_CURRENT_BINARY_DIR}"
    "-DHOST=$<TARGET_FILE:shadps4_native_dx12_host>"
    "-DCHILD=$<TARGET_FILE:native-launcher-child-test>"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/Launcher-Test.cmake")
add_executable(shadps4_dx12_resource_test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/test.cpp")
target_link_libraries(shadps4_dx12_resource_test PRIVATE shadps4_native_d3d12)
add_executable(shadps4_dx12_guest_presenter_test "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_presenter_test.cpp")
target_link_libraries(shadps4_dx12_guest_presenter_test PRIVATE shadps4_native_d3d12 user32)
add_test(NAME native_guest_presenter COMMAND shadps4_dx12_guest_presenter_test)
add_executable(shadps4_dx12_capabilities "${PORT_ROOT}/src/video_core/renderer_d3d12/capabilities_test.cpp")
target_link_libraries(shadps4_dx12_capabilities PRIVATE shadps4_native_d3d12)
add_test(NAME native_capabilities COMMAND shadps4_dx12_capabilities)
add_executable(native-buffer-budget-test buffer_budget_test.cpp)
target_link_libraries(native-buffer-budget-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_large_buffer_cache_budget COMMAND native-buffer-budget-test)
set_tests_properties(native_large_buffer_cache_budget PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
add_executable(native-buffer-view-lifetime-test buffer_view_lifetime_test.cpp)
add_executable(native-buffer-failure-test buffer_failure_test.cpp)
add_executable(native-multisample-readback-test multisample_readback_test.cpp)
target_link_libraries(native-multisample-readback-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_multisample_sample_readback COMMAND native-multisample-readback-test)
target_link_libraries(native-buffer-failure-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_buffer_allocation_failure COMMAND native-buffer-failure-test)
set_tests_properties(native_buffer_allocation_failure PROPERTIES TIMEOUT 120)
target_link_libraries(native-buffer-view-lifetime-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_buffer_view_lifetime COMMAND native-buffer-view-lifetime-test)
enable_testing()
set(SHADPS4_DXC_RUNTIME_DLL "${PORT_ROOT}/../fsr-sdk-local/sdk/tools/binary_store/dxcompiler.dll" CACHE FILEPATH "DXC runtime DLL for native checks")
get_filename_component(DXC_DLL "${SHADPS4_DXC_RUNTIME_DLL}" ABSOLUTE)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures")
add_test(NAME native_resources COMMAND shadps4_dx12_resource_test)
add_test(NAME dxil_compile COMMAND shadps4_dxil_shader_test "${DXC_DLL}")
set_tests_properties(dxil_compile PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures" FIXTURES_SETUP dxil)
add_test(NAME dxil_gpu_draw COMMAND shadps4_dxil_draw_test)
set_tests_properties(dxil_gpu_draw PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures" FIXTURES_REQUIRED dxil)

include("${PORT_ROOT}/src/video_core/renderer_d3d12/Texture-Test.cmake")
include("${PORT_ROOT}/src/video_core/renderer_d3d12/Compute-Test.cmake")
add_executable(shadps4_dx12_present_test "${PORT_ROOT}/src/video_core/renderer_d3d12/presenter_test.cpp")
target_link_libraries(shadps4_dx12_present_test PRIVATE shadps4_native_d3d12 user32)
add_test(NAME native_present COMMAND shadps4_dx12_present_test)
set(_xess_sdk "${PORT_ROOT}/../xess-native-sdk-v3.0.2")
if(EXISTS "${_xess_sdk}/include/xess/xess_d3d12.h" AND EXISTS "${_xess_sdk}/libxess.dll")
    add_executable(native-xess-execution-probe "${CMAKE_CURRENT_SOURCE_DIR}/xess_execution_probe.cpp")
    target_include_directories(native-xess-execution-probe PRIVATE "${_xess_sdk}/include")
    target_link_libraries(native-xess-execution-probe PRIVATE shadps4_native_d3d12)
    add_test(NAME native_xess_sdk_execution COMMAND native-xess-execution-probe "${_xess_sdk}/libxess.dll")
    set_tests_properties(native_xess_sdk_execution PROPERTIES ENVIRONMENT "SHADPS4_NATIVE_D3D12_VALIDATION=1" TIMEOUT 120)
endif()
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures-ir")
add_test(NAME real_ir_compile COMMAND shadps4_dxil_ir_test "${DXC_DLL}")
set_tests_properties(real_ir_compile PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures-ir" FIXTURES_SETUP real_ir)
add_test(NAME real_ir_gpu_draw COMMAND shadps4_dxil_draw_test)
set_tests_properties(real_ir_gpu_draw PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/fixtures-ir" FIXTURES_REQUIRED real_ir)
set(SHADPS4_GUEST_SHADER_FIXTURES "C:/Games/Bloodborne-Portable/user/cache/CUSA03173" CACHE PATH "Optional read-only guest shader fixtures")
get_filename_component(MESA_CONVERTER "${SHADPS4_MESA_CONVERTER_DLL}" ABSOLUTE)
if(EXISTS "${MESA_CONVERTER}" AND EXISTS "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000073ecc9_0.spv")
    add_test(NAME guest_vertex_dxil COMMAND shadps4_dxil_spirv_test "${MESA_CONVERTER}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000073ecc9_0.spv" vertex)
    add_test(NAME guest_pixel_dxil COMMAND shadps4_dxil_spirv_test "${MESA_CONVERTER}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x00000000006c079b_0.spv" pixel)
    add_test(NAME guest_invalid_dxil_rejected COMMAND shadps4_dxil_spirv_test "${MESA_CONVERTER}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000089e4b2_0.spv" pixel unused expect-reject)
    add_test(NAME guest_unsupported_barycentric_rejected COMMAND shadps4_dxil_spirv_test "${MESA_CONVERTER}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000000d506a1_0.spv" pixel unused expect-reject)
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/Barycentric-Test.cmake")
include("${PORT_ROOT}/src/video_core/renderer_d3d12/Guest-Texture-Test.cmake")
add_executable(native-page-owned-gpu-test page_owned_gpu_test.cpp
    "${PORT_ROOT}/src/video_core/renderer_d3d12/memory_surface_provider.cpp")
target_link_libraries(native-page-owned-gpu-test PRIVATE shadps4_native_guest_textures shadps4_native_d3d12)
add_test(NAME native_gpu_page_ownership COMMAND native-page-owned-gpu-test)
add_test(NAME native_gpu_page_ownership_360p COMMAND native-page-owned-gpu-test lowres)
set_tests_properties(native_gpu_page_ownership_360p PROPERTIES ENVIRONMENT "SHADPS4_NATIVE_D3D12_VALIDATION=1" TIMEOUT 20)
set_tests_properties(native_gpu_page_ownership PROPERTIES ENVIRONMENT "SHADPS4_NATIVE_D3D12_VALIDATION=1" TIMEOUT 20)
add_executable(native-coherent-sampled-view-test "${CMAKE_CURRENT_SOURCE_DIR}/coherent_sampled_view_test.cpp")
target_link_libraries(native-coherent-sampled-view-test PRIVATE shadps4_native_guest_textures)
add_test(NAME native_coherent_sampled_attachment COMMAND native-coherent-sampled-view-test)
add_executable(dx12-memory-surface-provider-test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/memory_surface_provider_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/memory_surface_provider.cpp")
target_link_libraries(dx12-memory-surface-provider-test PRIVATE shadps4_native_d3d12 shadps4_native_guest_textures)
add_test(NAME native_coherent_gpu_display COMMAND dx12-memory-surface-provider-test)
get_filename_component(MESA_LINKED_CONVERTER "${SHADPS4_MESA_CONVERTER_DLL}" ABSOLUTE)
if(EXISTS "${MESA_LINKED_CONVERTER}" AND EXISTS "${SHADPS4_GUEST_SHADER_FIXTURES}/0x00000000bc234799_0.spv")
    add_test(NAME guest_linked_tessellation COMMAND shadps4_dxil_linked_test
        "${MESA_LINKED_CONVERTER}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x00000000bc234799_0.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000c1888bd_0.spv")
endif()

add_executable(dx12-resource-bindings-test "${PORT_ROOT}/src/video_core/renderer_d3d12/resource_bindings_test.cpp" "${_compute_fixture}")
target_link_libraries(dx12-resource-bindings-test PRIVATE shadps4_native_d3d12 shadps4_dxil_compiler)
add_test(NAME native_reflected_resource_bindings COMMAND dx12-resource-bindings-test "${_compute_fixture}" "${_compute_mesa}" "${DXC_DLL}")
add_executable(dx12-guest-buffer-dispatch-test "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_buffer_dispatch_test.cpp" "${_compute_fixture}")
target_link_libraries(dx12-guest-buffer-dispatch-test PRIVATE shadps4_native_d3d12 shadps4_dxil_compiler)
add_test(NAME native_guest_buffer_dispatch COMMAND dx12-guest-buffer-dispatch-test "${_compute_fixture}" "${_compute_mesa}" "${DXC_DLL}")
set(_alias_fixture "${CMAKE_CURRENT_BINARY_DIR}/buffer_alias_fixture.spv")
add_custom_command(OUTPUT "${_alias_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${CMAKE_CURRENT_SOURCE_DIR}/buffer_alias_fixture.comp" -o "${_alias_fixture}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/buffer_alias_fixture.comp" VERBATIM)
add_executable(native-buffer-alias-test "${CMAKE_CURRENT_SOURCE_DIR}/buffer_alias_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp" "${_alias_fixture}")
target_include_directories(native-buffer-alias-test PRIVATE $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(native-buffer-alias-test PRIVATE cxx_std_23)
target_link_libraries(native-buffer-alias-test PRIVATE shadps4_native_d3d12 shadps4_native_guest_textures shadps4_dxil_compiler)
add_test(NAME native_guest_unified_buffer_aliases COMMAND native-buffer-alias-test "${_alias_fixture}" "${MESA_LINKED_CONVERTER}" "${DXC_DLL}")
add_executable(dx12-guest-metadata-dispatch-test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_metadata_dispatch_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp" "${_compute_fixture}")
target_include_directories(dx12-guest-metadata-dispatch-test PRIVATE
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(dx12-guest-metadata-dispatch-test PRIVATE cxx_std_23)
target_link_libraries(dx12-guest-metadata-dispatch-test PRIVATE shadps4_native_d3d12 shadps4_native_guest_textures shadps4_dxil_compiler)
add_test(NAME native_guest_metadata_dispatch COMMAND dx12-guest-metadata-dispatch-test "${_compute_fixture}" "${_compute_mesa}" "${DXC_DLL}")
set(_wave_fixture "${CMAKE_CURRENT_BINARY_DIR}/compute_wave32_fixture.spv")
add_custom_command(OUTPUT "${_wave_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${PORT_ROOT}/src/video_core/renderer_d3d12/compute_wave32_fixture.comp" -o "${_wave_fixture}"
    DEPENDS "${PORT_ROOT}/src/video_core/renderer_d3d12/compute_wave32_fixture.comp" VERBATIM)
target_sources(dx12-guest-metadata-dispatch-test PRIVATE "${_wave_fixture}")
add_test(NAME native_guest_forced_wave32 COMMAND dx12-guest-metadata-dispatch-test "${_wave_fixture}" "${MESA_LINKED_CONVERTER}" "${DXC_DLL}" wave32)
set(_guest_image_fixture "${CMAKE_CURRENT_BINARY_DIR}/guest_image_compute_fixture.spv")
add_custom_command(OUTPUT "${_guest_image_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_image_compute_fixture.comp" -o "${_guest_image_fixture}"
    DEPENDS "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_image_compute_fixture.comp" VERBATIM)
add_executable(dx12-guest-image-compute-test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_image_compute_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp" "${_guest_image_fixture}")
target_include_directories(dx12-guest-image-compute-test PRIVATE
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(dx12-guest-image-compute-test PRIVATE cxx_std_23)
target_link_libraries(dx12-guest-image-compute-test PRIVATE shadps4_native_guest_textures shadps4_dxil_compiler)
add_test(NAME native_guest_image_compute COMMAND dx12-guest-image-compute-test "${_guest_image_fixture}" "${_compute_mesa}" "${DXC_DLL}")
set(_writable_alias_fixture "${CMAKE_CURRENT_BINARY_DIR}/writable_image_alias.spv")
add_custom_command(OUTPUT "${_writable_alias_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${CMAKE_CURRENT_SOURCE_DIR}/writable_image_alias.comp" -o "${_writable_alias_fixture}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/writable_image_alias.comp" VERBATIM)
add_executable(native-writable-image-alias-test writable_image_alias_test.cpp
    "${PORT_ROOT}/src/shader_recompiler/backend/spirv/emit_spirv_composite.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/passes/resource_patching_pass.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/ir_emitter.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/basic_block.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/patch.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/spirv/emit_spirv_image.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp" "${_writable_alias_fixture}"
    "${PORT_ROOT}/src/video_core/amdgpu/pixel_format.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/spirv/spirv_emit_context.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/dxil/tests/ir_test_diagnostics.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/value.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/microinstruction.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/opcodes.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/type.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/attribute.cpp")
target_include_directories(native-writable-image-alias-test PRIVATE
    "${PORT_ROOT}"
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(native-writable-image-alias-test PRIVATE cxx_std_23)
target_compile_definitions(native-writable-image-alias-test PRIVATE FMT_HEADER_ONLY NDEBUG)
target_link_libraries(native-writable-image-alias-test PRIVATE shadps4_native_guest_textures shadps4_dxil_compiler sirit)
add_test(NAME native_writable_image_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}")
add_test(NAME native_emitted_writable_image_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" emitted)
add_test(NAME native_typed_writable_image_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" typed)
add_test(NAME native_mixed_image_subresources COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" mixed)
add_test(NAME native_storage_image_mip_reads COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" dynamic)
add_test(NAME native_nonuniform_storage_image_mips COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" nonuniform)
add_test(NAME native_classified_image_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified)
add_test(NAME native_classified_image_view_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-view)
add_test(NAME native_classified_image_mip_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-mip)
add_test(NAME native_classified_image_typed_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-typed)
add_test(NAME native_classified_image_swizzle_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-swizzle)
add_test(NAME native_classified_image_write_swizzle_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-write-swizzle)
add_test(NAME native_classified_image_integer_one COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-one)
add_test(NAME native_classified_image_signed_one COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-signed-one)
add_test(NAME native_classified_image_float_one COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-float-one)
add_test(NAME native_classified_image_srgb_alias COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-srgb)
add_test(NAME native_classified_image_srgb_swizzle COMMAND native-writable-image-alias-test "${_writable_alias_fixture}" "${_compute_mesa}" "${DXC_DLL}" classified-srgb-swizzle)
add_executable(native-image-access-classification-test image_access_classification_test.cpp
    "${PORT_ROOT}/src/shader_recompiler/ir/passes/resource_patching_pass.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/ir_emitter.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/basic_block.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/value.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/microinstruction.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/opcodes.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/type.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/attribute.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/patch.cpp"
    "${PORT_ROOT}/src/video_core/amdgpu/pixel_format.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/dxil/tests/ir_test_diagnostics.cpp")
target_include_directories(native-image-access-classification-test PRIVATE "${PORT_ROOT}" $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(native-image-access-classification-test PRIVATE cxx_std_23)
target_compile_definitions(native-image-access-classification-test PRIVATE FMT_HEADER_ONLY NDEBUG)
add_test(NAME native_image_access_classification COMMAND native-image-access-classification-test)
set(_owned_image_fixture "${CMAKE_CURRENT_BINARY_DIR}/owned_image.spv")
add_custom_command(OUTPUT "${_owned_image_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${CMAKE_CURRENT_SOURCE_DIR}/owned_image.comp" -o "${_owned_image_fixture}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/owned_image.comp" VERBATIM)
add_executable(native-owned-image-dispatch-test owned_image_dispatch_test.cpp
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp" "${_owned_image_fixture}")
target_include_directories(native-owned-image-dispatch-test PRIVATE $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(native-owned-image-dispatch-test PRIVATE cxx_std_23)
target_link_libraries(native-owned-image-dispatch-test PRIVATE shadps4_native_guest_textures shadps4_dxil_compiler)
add_test(NAME native_owned_image_dispatch COMMAND native-owned-image-dispatch-test "${_owned_image_fixture}" "${_compute_mesa}" "${DXC_DLL}")
set_tests_properties(native_owned_image_dispatch PROPERTIES TIMEOUT 30)
set(_owned_sample_fixture "${CMAKE_CURRENT_BINARY_DIR}/owned_image_sample.spv")
add_custom_command(OUTPUT "${_owned_sample_fixture}"
    COMMAND "${_compute_glslang}" -V -S comp "${CMAKE_CURRENT_SOURCE_DIR}/owned_image_sample.comp" -o "${_owned_sample_fixture}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/owned_image_sample.comp" VERBATIM)
target_sources(native-owned-image-dispatch-test PRIVATE "${_owned_sample_fixture}")
add_test(NAME native_owned_image_sample COMMAND native-owned-image-dispatch-test "${_owned_image_fixture}" "${_compute_mesa}" "${DXC_DLL}" "${_owned_sample_fixture}")
set_tests_properties(native_owned_image_sample PROPERTIES TIMEOUT 30)
add_executable(native-typeless-texture-test typeless_texture_test.cpp)
target_compile_features(native-typeless-texture-test PRIVATE cxx_std_23)
target_link_libraries(native-typeless-texture-test PRIVATE shadps4_native_guest_textures)
add_test(NAME native_typeless_texture_formats COMMAND native-typeless-texture-test)
add_executable(native-subresource-states-test subresource_states_test.cpp)
target_compile_features(native-subresource-states-test PRIVATE cxx_std_23)
target_link_libraries(native-subresource-states-test PRIVATE shadps4_native_guest_textures)
add_test(NAME native_mixed_subresource_states COMMAND native-subresource-states-test)
add_test(NAME native_enhanced_mixed_subresource_states COMMAND native-subresource-states-test enhanced)
set_tests_properties(native_enhanced_mixed_subresource_states PROPERTIES SKIP_RETURN_CODE 77)
add_executable(native-alias-barrier-test alias_barrier_test.cpp)
target_compile_features(native-alias-barrier-test PRIVATE cxx_std_23)
target_link_libraries(native-alias-barrier-test PRIVATE shadps4_native_d3d12)
add_test(NAME native_placed_buffer_alias_barriers COMMAND native-alias-barrier-test)
add_test(NAME native_enhanced_placed_buffer_alias_barriers COMMAND native-alias-barrier-test enhanced)
set_tests_properties(native_enhanced_placed_buffer_alias_barriers PROPERTIES SKIP_RETURN_CODE 77)
add_executable(dx12-guest-graphics-resources-test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp")
target_include_directories(dx12-guest-graphics-resources-test PRIVATE
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(dx12-guest-graphics-resources-test PRIVATE cxx_std_23)
target_link_libraries(dx12-guest-graphics-resources-test PRIVATE shadps4_native_guest_textures shadps4_dxil_compiler)
add_test(NAME native_guest_graphics_resources COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}")
add_test(NAME native_guest_cross_stage_image_alias COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" cross-stage-alias)
add_test(NAME native_guest_linear_color_attachment COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" linear-color)
add_test(NAME native_guest_readonly_buffer_image_alias COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" readonly-alias)
add_test(NAME native_guest_depth_resources COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth)
add_test(NAME native_guest_htile_clear COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" htile)
add_test(NAME native_guest_depth_stale_stencil COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth-stale-stencil)
add_test(NAME native_guest_depth_bias COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth-bias)
add_test(NAME native_guest_fractional_depth_bias COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth-bias-fractional)
add_test(NAME native_guest_depth_clear COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" clear)
add_test(NAME native_guest_depth_bounds_pass COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth-bounds-pass)
add_test(NAME native_guest_depth_bounds_reject COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" depth-bounds-reject)
add_test(NAME native_guest_stencil_resources COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil)
add_test(NAME native_guest_stencil_clear COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-clear)
add_test(NAME native_guest_stencil_replace_op COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-replace)
add_test(NAME native_guest_stencil_ones COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-ones)
foreach(op and or xor xnor nand nor)
    add_test(NAME native_guest_stencil_${op} COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-${op})
endforeach()
add_test(NAME native_guest_stencil_independent_faces COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-faces)
add_test(NAME native_guest_stencil_back_only COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" stencil-back-only)
add_test(NAME native_guest_attachment_retirement COMMAND dx12-guest-graphics-resources-test "${DXC_DLL}" attachments)
add_executable(dx12-shader-pipeline-test "${PORT_ROOT}/src/video_core/renderer_d3d12/shader_pipeline_test.cpp")
target_link_libraries(dx12-shader-pipeline-test PRIVATE shadps4_native_d3d12 shadps4_dxil_compiler)
add_test(NAME native_sampled_graphics_pipeline COMMAND dx12-shader-pipeline-test "${DXC_DLL}")
add_executable(native-dma-shader-test dma_shader_test.cpp
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_compute.cpp"
    "${PORT_ROOT}/src/video_core/amdgpu/pixel_format.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/spirv/spirv_emit_context.cpp"
    "${PORT_ROOT}/src/shader_recompiler/backend/dxil/tests/ir_test_diagnostics.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/value.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/microinstruction.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/opcodes.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/type.cpp"
    "${PORT_ROOT}/src/shader_recompiler/ir/attribute.cpp")
target_compile_features(native-dma-shader-test PRIVATE cxx_std_23)
target_compile_definitions(native-dma-shader-test PRIVATE FMT_HEADER_ONLY NDEBUG)
target_include_directories(native-dma-shader-test PRIVATE $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_link_libraries(native-dma-shader-test PRIVATE shadps4_native_d3d12 shadps4_native_guest_textures shadps4_dxil_compiler sirit)
add_test(NAME native_dma_shader_addressing COMMAND native-dma-shader-test "${MESA_LINKED_CONVERTER}" "${DXC_DLL}")
add_executable(dx12-guest-pipeline-test
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_pipeline_test.cpp"
    "${PORT_ROOT}/src/video_core/renderer_d3d12/guest_shader_cache.cpp")
target_include_directories(dx12-guest-pipeline-test PRIVATE "${SHADPS4_DXC_INCLUDE_DIR}"
    $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_compile_features(dx12-guest-pipeline-test PRIVATE cxx_std_23)
target_link_libraries(dx12-guest-pipeline-test PRIVATE shadps4_native_d3d12 shadps4_dxil_compiler)
if(EXISTS "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000050287a64_2.spv")
    add_test(NAME native_guest_graphics_pipeline COMMAND dx12-guest-pipeline-test
        "${MESA_LINKED_CONVERTER}" "${DXC_DLL}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000050287a64_2.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000003deaa573_1.spv")
    add_test(NAME native_guest_tessellation_pipeline COMMAND dx12-guest-pipeline-test
        "${MESA_LINKED_CONVERTER}" "${DXC_DLL}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000071c96598_0.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000004cd21a0f_0.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x00000000bc234799_0.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000c1888bd_0.spv")
    add_test(NAME native_guest_geometry_pipeline COMMAND dx12-guest-pipeline-test
        "${MESA_LINKED_CONVERTER}" "${DXC_DLL}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000002db65555_0.spv"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x00000000073ddffc_0.spv" - -
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000001635154c_0.spv")
    add_test(NAME native_guest_vertex_only_pipeline COMMAND dx12-guest-pipeline-test
        "${MESA_LINKED_CONVERTER}" "${DXC_DLL}" -
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000089cda9e4_2.spv")
    add_test(NAME native_guest_compute_pipeline COMMAND dx12-guest-pipeline-test
        "${MESA_LINKED_CONVERTER}" "${DXC_DLL}"
        "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000f6d2ebf_0.spv")
endif()
