function(native_guest_tiling target)
    get_filename_component(_tiling_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(_tiling_native "${_tiling_root}/src/video_core/renderer_d3d12")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_tiling_native}/guest_tiling.hlsl")
    file(READ "${_tiling_native}/guest_tiling.hlsl" _guest_tiling_source)
    configure_file("${_tiling_root}/native-port/guest_tiling_shader.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/native-generated/guest_tiling_shader.h" @ONLY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_tiling_native}/guest_msaa_upload.hlsl" "${_tiling_native}/guest_msaa_readback.hlsl" "${_tiling_native}/guest_msaa_copy.hlsl")
    file(READ "${_tiling_native}/guest_msaa_upload.hlsl" _guest_msaa_upload_source)
    file(READ "${_tiling_native}/guest_msaa_readback.hlsl" _guest_msaa_readback_source)
    file(READ "${_tiling_native}/guest_msaa_copy.hlsl" _guest_msaa_copy_source)
    configure_file("${_tiling_root}/native-port/guest_msaa_shaders.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/native-generated/guest_msaa_shaders.h" @ONLY)
    target_sources(${target} PRIVATE "${_tiling_native}/guest_tiler.cpp" "${_tiling_native}/guest_multisample.cpp"
        "${_tiling_native}/guest_packed_transfer.cpp" "${_tiling_native}/guest_bc_decoder.cpp")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/native-generated")
    target_link_libraries(${target} PRIVATE d3dcompiler)
endfunction()
