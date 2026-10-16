function(native_ags_device target)
    get_filename_component(_ags_repo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    set(SHADPS4_AGS_SDK_ROOT "" CACHE PATH "Pinned AGS SDK source directory; empty fetches the pinned revision")
    if(NOT SHADPS4_AGS_SDK_ROOT)
        include(FetchContent)
        FetchContent_Declare(shadps4_native_ags_sdk
            GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/AGS_SDK.git
            GIT_TAG 086c47ed8e427199510887230718cd7c086f9702)
        FetchContent_MakeAvailable(shadps4_native_ags_sdk)
        set(_ags_sdk "${shadps4_native_ags_sdk_SOURCE_DIR}")
    else()
        set(_ags_sdk "${SHADPS4_AGS_SDK_ROOT}")
    endif()
    set(_ags_dll "${_ags_sdk}/ags_lib/lib/amd_ags_x64.dll")
    file(SHA256 "${_ags_sdk}/ags_lib/inc/amd_ags.h" _ags_header_hash)
    file(SHA256 "${_ags_repo}/native-port/vendor/ags/amd_ags.h" _ags_compiled_header_hash)
    file(SHA256 "${_ags_dll}" _ags_dll_hash)
    if(NOT _ags_header_hash STREQUAL _ags_compiled_header_hash OR
       NOT _ags_header_hash STREQUAL "4a84c8555b925f746a38a8c327c63cc3941257ec5e021620058b35e1a1d61a15" OR
       NOT _ags_dll_hash STREQUAL "482b241ea855fdd59aaa7687c385ce4165b5340857e6a0e6d15ba27ecb65ee70")
        message(FATAL_ERROR "Native AGS SDK header/runtime identity differs from the pinned dependency")
    endif()
    target_include_directories(${target} PRIVATE "${_ags_repo}/native-port/vendor/ags")
    target_sources(${target} PRIVATE "${_ags_repo}/src/video_core/renderer_d3d12/ags_shader_clock.cpp")
    add_custom_target(shadps4_native_ags_files
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/native-compiler"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ags_dll}" "$<TARGET_FILE_DIR:${target}>/native-compiler/amd_ags_x64.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses/AGS"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ags_repo}/native-port/vendor/ags/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/AGS/LICENSE.txt")
    add_dependencies(${target} shadps4_native_ags_files)
endfunction()
