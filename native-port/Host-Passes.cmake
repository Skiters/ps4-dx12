function(native_host_passes target)
    get_filename_component(_host_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    include("${_host_root}/native-port/Native-DXC.cmake")
    native_dxc_identity()
    get_filename_component(_host_dxc_root "${SHADPS4_DXC_INCLUDE_DIR}" DIRECTORY)
    find_program(SHADPS4_DXC_EXECUTABLE NAMES dxc.exe dxc HINTS "${_host_dxc_root}/bin/x64" REQUIRED NO_DEFAULT_PATH)
    set(_host_native "${_host_root}/src/video_core/renderer_d3d12")
    set(_host_fsr "${_host_root}/src/video_core/host_shaders/fsr")
    set(_host_generated "${CMAKE_CURRENT_BINARY_DIR}/native-generated")
    file(MAKE_DIRECTORY "${_host_generated}")
    foreach(_pass Easu Rcas)
        string(TOUPPER "${_pass}" _definition)
        set(_shader "${_host_generated}/host_fsr_${_pass}.dxil")
        set(_header "${_host_generated}/host_fsr_${_pass}.h")
        add_custom_command(OUTPUT "${_header}" "${_shader}"
            COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_2 -E main -HV 2018 -enable-16bit-types -O3 -WX
                -DSAMPLE_${_definition}=1 -I "${_host_fsr}" -Fo "${_shader}" "${_host_native}/host_fsr.hlsl"
            COMMAND ${CMAKE_COMMAND} "-DINPUT=${_shader}" "-DOUTPUT=${_header}"
                "-DSYMBOL=HostFsr${_pass}" -P "${_host_root}/native-port/Embed-Binary.cmake"
            DEPENDS "${_host_native}/host_fsr.hlsl" "${_host_fsr}/ffx_a.h" "${_host_fsr}/ffx_fsr1.h"
                "${_host_root}/native-port/Embed-Binary.cmake"
            VERBATIM)
        target_sources(${target} PRIVATE "${_header}")
    endforeach()
    target_sources(${target} PRIVATE "${_host_native}/host_fsr.cpp")
    set(_capture_shader "${_host_generated}/guest_point_capture_clear.dxil")
    set(_capture_header "${_host_generated}/guest_point_capture_clear.h")
    add_custom_command(OUTPUT "${_capture_header}" "${_capture_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E main -HV 2018 -O3 -WX
            -Fo "${_capture_shader}" "${_host_native}/guest_point_capture_clear.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_capture_shader}" "-DOUTPUT=${_capture_header}"
            "-DSYMBOL=NativePointCaptureClear" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_point_capture_clear.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_capture_header}" "${_host_native}/guest_point_capture.cpp")
    set(_capture_args_shader "${_host_generated}/guest_point_capture_dispatch.dxil")
    set(_capture_args_header "${_host_generated}/guest_point_capture_dispatch.h")
    add_custom_command(OUTPUT "${_capture_args_header}" "${_capture_args_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E build_dispatch -HV 2018 -O3 -WX
            -Fo "${_capture_args_shader}" "${_host_native}/guest_point_capture_clear.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_capture_args_shader}" "-DOUTPUT=${_capture_args_header}"
            "-DSYMBOL=NativePointCaptureDispatch" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_point_capture_clear.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_capture_args_header}")
    set(_stream_shader "${_host_generated}/primitive_stream_arguments.dxil")
    set(_stream_header "${_host_generated}/primitive_stream_arguments_shader.h")
    add_custom_command(OUTPUT "${_stream_header}" "${_stream_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E main -HV 2018 -O3 -WX
            -Fo "${_stream_shader}" "${_host_native}/primitive_stream_arguments.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_stream_shader}" "-DOUTPUT=${_stream_header}"
            "-DSYMBOL=NativePrimitiveStreamArguments" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/primitive_stream_arguments.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_stream_header}" "${_host_native}/primitive_stream_arguments.cpp")
    set(_partition_shader "${_host_generated}/primitive_stream_partition.dxil")
    set(_partition_header "${_host_generated}/primitive_stream_partition_shader.h")
    add_custom_command(OUTPUT "${_partition_header}" "${_partition_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E partition -HV 2018 -O3 -WX
            -Fo "${_partition_shader}" "${_host_native}/primitive_stream_arguments.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_partition_shader}" "-DOUTPUT=${_partition_header}"
            "-DSYMBOL=NativePrimitiveStreamPartition" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/primitive_stream_arguments.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_partition_header}")
    set(_index_shader "${_host_generated}/guest_index_expand.dxil")
    set(_index_header "${_host_generated}/guest_index_expand_shader.h")
    add_custom_command(OUTPUT "${_index_header}" "${_index_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E main -HV 2018 -O3 -WX
            -Fo "${_index_shader}" "${_host_native}/guest_index_expand.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_index_shader}" "-DOUTPUT=${_index_header}"
            "-DSYMBOL=NativeGuestIndexExpand" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_index_expand.hlsl" "${_host_native}/guest_index_read.hlsli" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_index_header}" "${_host_native}/guest_index_expand.cpp")
    set(_restart_shader "${_host_generated}/guest_index_restart.dxil")
    set(_restart_header "${_host_generated}/guest_index_restart_shader.h")
    add_custom_command(OUTPUT "${_restart_header}" "${_restart_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E main -HV 2018 -O3 -WX
            -Fo "${_restart_shader}" "${_host_native}/guest_index_restart.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_restart_shader}" "-DOUTPUT=${_restart_header}"
            "-DSYMBOL=NativeGuestIndexRestart" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_index_restart.hlsl" "${_host_native}/guest_index_read.hlsli" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_restart_header}")
    set(_indirect_shader "${_host_generated}/guest_indirect_pack.dxil")
    set(_indirect_header "${_host_generated}/guest_indirect_pack_shader.h")
    add_custom_command(OUTPUT "${_indirect_header}" "${_indirect_shader}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E main -HV 2018 -O3 -WX
            -Fo "${_indirect_shader}" "${_host_native}/guest_indirect_pack.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_indirect_shader}" "-DOUTPUT=${_indirect_header}"
            "-DSYMBOL=NativeGuestIndirectPack" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_indirect_pack.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_indirect_header}" "${_host_native}/guest_indirect_pack.cpp")
    set(_indirect_dispatch "${_host_generated}/guest_indirect_dispatch.dxil")
    set(_indirect_dispatch_header "${_host_generated}/guest_indirect_dispatch_shader.h")
    add_custom_command(OUTPUT "${_indirect_dispatch_header}" "${_indirect_dispatch}"
        COMMAND "${SHADPS4_DXC_EXECUTABLE}" -T cs_6_0 -E build_dispatch -HV 2018 -O3 -WX
            -Fo "${_indirect_dispatch}" "${_host_native}/guest_indirect_pack.hlsl"
        COMMAND ${CMAKE_COMMAND} "-DINPUT=${_indirect_dispatch}" "-DOUTPUT=${_indirect_dispatch_header}"
            "-DSYMBOL=NativeGuestIndirectDispatch" -P "${_host_root}/native-port/Embed-Binary.cmake"
        DEPENDS "${_host_native}/guest_indirect_pack.hlsl" "${_host_root}/native-port/Embed-Binary.cmake"
        VERBATIM)
    target_sources(${target} PRIVATE "${_indirect_dispatch_header}")
    target_include_directories(${target} PRIVATE "${_host_generated}")
endfunction()
