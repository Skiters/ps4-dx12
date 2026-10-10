# SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project

# SPDX-License-Identifier: GPL-2.0-or-later



set(BUILD_SHARED_LIBS OFF)

set(BUILD_TESTING OFF)

set_directory_properties(PROPERTIES

    EXCLUDE_FROM_ALL ON

    SYSTEM ON

)



# Set CMP0069 policy to "NEW" in order to ensure consistent behavior when building external targets with LTO enabled

set(CMAKE_POLICY_DEFAULT_CMP0069 NEW)



if (MSVC)

    # Silence "deprecation" warnings

    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)

endif()



# Boost

if (NOT TARGET Boost::headers)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/ext-boost" "ext-boost")

endif()



# spdlog

set(SPDLOG_NO_EXCEPTIONS ON)

set(SPDLOG_DISABLE_GLOBAL_LOGGER ON)

if (TARGET fmt::fmt)

    set(SPDLOG_FMT_EXTERNAL ON)

endif()

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/spdlog" "spdlog" EXCLUDE_FROM_ALL SYSTEM)

target_compile_definitions(spdlog PUBLIC SPDLOG_FUNCTION=__func__)



# fmtlib

if (NOT TARGET fmt::fmt)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/fmt" "fmt")

endif()



# FFmpeg

if (NOT TARGET FFmpeg::ffmpeg)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/ffmpeg-core" "ffmpeg-core")

    add_library(FFmpeg::ffmpeg ALIAS ffmpeg)

endif()



# LibAtrac9

file(GLOB LIBATRAC9_SOURCES ${SHADPS4_EXTERNALS_DIR}/LibAtrac9/C/src/*.c)

add_library(LibAtrac9 STATIC ${LIBATRAC9_SOURCES})

target_include_directories(LibAtrac9 INTERFACE "${SHADPS4_EXTERNALS_DIR}/LibAtrac9/C/src")



# zlib

if (NOT TARGET ZLIB::ZLIB)

    set(ZLIB_ENABLE_TESTS OFF)

    set(WITH_GTEST OFF)

    set(WITH_NEW_STRATEGIES ON)

    set(ZLIB_COMPAT ON CACHE BOOL "" FORCE)

    include(FetchContent)

    FetchContent_Declare(

        ZLIB

        SOURCE_DIR "${SHADPS4_EXTERNALS_DIR}/zlib-ng"

        OVERRIDE_FIND_PACKAGE

    )

    FetchContent_MakeAvailable(ZLIB)

    add_library(ZLIB::ZLIB ALIAS zlib)

    # libpng expects this variable to exist after its find_package(ZLIB)

    set(ZLIB_INCLUDE_DIRS "${FETCHCONTENT_BASE_DIR}/zlib-build")

endif()



# FreeType

if (NOT TARGET Freetype::Freetype)

    # Keep optional deps off to reduce build surface; zlib stays enabled for compressed fonts.

    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)

    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)

    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)

    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/freetype" "freetype")

    add_library(Freetype::Freetype ALIAS freetype)

endif()



# SDL3

if (NOT TARGET SDL3::SDL3)

    set(SDL_TEST_LIBRARY OFF)

    set(SDL_PIPEWIRE OFF)

    set(SDL_AVX512F OFF)
    # This fork has a single native D3D12 renderer. Disable optional SDL Vulkan code too.
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER_VULKAN OFF CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/sdl3" "sdl3")

    # SDL globs disabled backend files as empty translation units. Exclude them
    # from this build as well, without changing the shared dependency checkout.
    foreach(_sdl_target SDL3-static SDL3-shared)
        if(TARGET ${_sdl_target})
            get_target_property(_sdl_sources ${_sdl_target} SOURCES)
            list(FILTER _sdl_sources EXCLUDE REGEX "[Vv]ulkan")
            set_property(TARGET ${_sdl_target} PROPERTY SOURCES "${_sdl_sources}")
        endif()
    endforeach()

endif()



# RenderDoc

if (NOT TARGET RenderDoc::API)

    add_library(renderdoc INTERFACE)

    target_include_directories(renderdoc INTERFACE "${SHADPS4_EXTERNALS_DIR}/renderdoc")

    add_library(RenderDoc::API ALIAS renderdoc)

endif()



# Robin-map

if (NOT TARGET tsl::robin_map)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/robin-map" "robin-map")

endif()



# Xbyak

if (NOT TARGET xbyak::xbyak)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/xbyak" "xbyak")

endif()



# MagicEnum

if (NOT TARGET magic_enum::magic_enum)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/magic_enum" "magic_enum")

endif()



# Toml11

if (NOT TARGET toml11::toml11)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/toml11" "toml11")



    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

      if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")

        get_target_property(_toml11_compile_options toml11 INTERFACE_COMPILE_OPTIONS)

        list(REMOVE_ITEM _toml11_compile_options "/Zc:preprocessor")

        set_target_properties(toml11 PROPERTIES INTERFACE_COMPILE_OPTIONS ${_toml11_compile_options})

      endif()

    endif()

endif()



# xxHash

if (NOT TARGET xxHash::xxhash)

    add_library(xxhash ${SHADPS4_EXTERNALS_DIR}/xxhash/xxhash.h ${SHADPS4_EXTERNALS_DIR}/xxhash/xxhash.c)

    target_include_directories(xxhash PUBLIC "${SHADPS4_EXTERNALS_DIR}/xxhash")

    add_library(xxHash::xxhash ALIAS xxhash)

endif()



# Zydis

if (NOT TARGET Zydis::Zydis)

    option(ZYDIS_BUILD_TOOLS "" OFF)

    option(ZYDIS_BUILD_EXAMPLES "" OFF)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/zydis" "zydis")

endif()



# sirit

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/sirit" "sirit")

if (WIN32)

    target_compile_options(sirit PRIVATE $<$<CXX_COMPILER_ID:Clang>:-Wno-error=unused-command-line-argument>)

endif()



# half

if (NOT TARGET half::half)

    add_library(half INTERFACE)

    target_include_directories(half INTERFACE "${SHADPS4_EXTERNALS_DIR}/half/include")

    add_library(half::half ALIAS half)

endif()



# libpng

if (NOT TARGET PNG::PNG)

    set(PNG_SHARED OFF CACHE BOOL "" FORCE)

    set(PNG_STATIC ON CACHE BOOL "" FORCE)

    set(PNG_TESTS OFF CACHE BOOL "" FORCE)

    set(PNG_TOOLS OFF CACHE BOOL "" FORCE)

    set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/libpng" "libpng")

    add_library(PNG::PNG ALIAS png_static)

endif()



# Dear ImGui

add_library(Dear_ImGui

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui.cpp

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui_demo.cpp

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui_draw.cpp

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui_internal.h

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui_tables.cpp

        ${SHADPS4_EXTERNALS_DIR}/imgui/imgui_widgets.cpp

)

target_include_directories(Dear_ImGui INTERFACE "${SHADPS4_EXTERNALS_DIR}/imgui/")



# Tracy

if (CMAKE_BUILD_TYPE STREQUAL "Release")

    option(TRACY_ENABLE "" OFF)

else()

    option(TRACY_ENABLE "" ON)

endif()

option(TRACY_NO_CRASH_HANDLER "" ON) # Otherwise texture cache exceptions will be treaten as a crash

option(TRACY_ON_DEMAND "" ON)

option(TRACY_NO_FRAME_IMAGE "" ON)

option(TRACY_FIBERS "" OFF) # For AmdGpu frontend profiling, disabled due to instability

option(TRACY_NO_SYSTEM_TRACING "" ON)

option(TRACY_NO_CALLSTACK "" ON)

option(TRACY_NO_CODE_TRANSFER "" ON)

option(TRACY_NO_SAMPLING "" ON)

option(TRACY_ONLY_LOCALHOST "" ON)

option(TRACY_NO_CONTEXT_SWITCH "" ON)

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/tracy" "tracy")



# pugixml

if (NOT TARGET pugixml::pugixml)

    option(PUGIXML_NO_EXCEPTIONS "" ON)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/pugixml" "pugixml")

endif()



# libusb

if (NOT TARGET libusb::usb)

    if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")

        # YOU MUST USE NATIVE LIBUSB

        # using anything else will crash instantly, also freebsd will NOT like it

        # no you cant vendor this libusb, its builtin on freebsd

        find_package(libusb)

    else()

        add_subdirectory("${SHADPS4_EXTERNALS_DIR}/libusb" "libusb")

        add_library(libusb::usb ALIAS usb-1.0)

    endif()

endif()



# Discord RPC

if (ENABLE_DISCORD_RPC)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/discord-rpc" "discord-rpc")

endif()



# GCN Headers

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/gcn" "gcn")



# stb

if (NOT TARGET stb::headers)

    add_library(stb INTERFACE)

    target_include_directories(stb INTERFACE "${SHADPS4_EXTERNALS_DIR}/stb")

    add_library(stb::headers ALIAS stb)

endif()



# hwinfo

set(HWINFO_STATIC ON)

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/hwinfo" "hwinfo")



if (APPLE OR ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")

    # date

    if (NOT TARGET date::date-tz)

        option(BUILD_TZ_LIB "" ON)

        option(USE_SYSTEM_TZ_DB "" ON)

        add_subdirectory("${SHADPS4_EXTERNALS_DIR}/date" "date")

    endif()

    if (NOT TARGET epoll-shim)

        add_subdirectory("${SHADPS4_EXTERNALS_DIR}/epoll-shim" "epoll-shim")

    endif()

endif()



#windows only

if (WIN32)

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/ext-wepoll" "ext-wepoll")

endif()



add_subdirectory("${SHADPS4_EXTERNALS_DIR}/aacdec" "aacdec")



#nlohmann json 

if (NOT TARGET nlohmann_json::nlohmann_json)

set(JSON_BuildTests OFF CACHE INTERNAL "")

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/json" "json")

endif()



# miniz

if (NOT TARGET miniz::miniz)

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/miniz" "miniz")

add_library(miniz::miniz ALIAS miniz)

endif()



# cli11

if (NOT TARGET CLI11::CLI11)

    set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/CLI11" "CLI11")

endif()



# miniupnpc

if (NOT TARGET miniupnpc::miniupnpc)

set(UPNPC_BUILD_STATIC ON CACHE BOOL "" FORCE)

set(UPNPC_BUILD_SHARED OFF CACHE BOOL "" FORCE)

set(UPNPC_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(UPNPC_BUILD_SAMPLE OFF CACHE BOOL "" FORCE)

set(UPNPC_NO_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/miniupnp/miniupnpc" "miniupnp/miniupnpc")

endif()



# minimp3

add_library(minimp3 INTERFACE)

target_include_directories(minimp3 INTERFACE "${SHADPS4_EXTERNALS_DIR}/minimp3")



# ImGuiFileDialog

add_library(ImGuiFileDialog ${SHADPS4_EXTERNALS_DIR}/ImGuiFileDialog/ImGuiFileDialog.cpp)

target_compile_definitions(ImGuiFileDialog PRIVATE USE_STD_FILESYSTEM)

target_include_directories(ImGuiFileDialog INTERFACE "${SHADPS4_EXTERNALS_DIR}/ImGuiFileDialog/")

target_link_libraries(ImGuiFileDialog PRIVATE Dear_ImGui)



#openal

if (NOT TARGET OpenAL::OpenAL)

set(ALSOFT_ENABLE_MODULES OFF CACHE BOOL "" FORCE)

set(LIBTYPE "STATIC" CACHE STRING "" FORCE)

# Disable everything we don't need

set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)

set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)

set(ALSOFT_TESTS OFF CACHE BOOL "" FORCE)

set(ALSOFT_INSTALL OFF CACHE BOOL "" FORCE)

set(ALSOFT_CONFIG OFF CACHE BOOL "" FORCE)



# Backends (platform-specific)

if (WIN32)

    set(ALSOFT_BACKEND_WASAPI ON CACHE BOOL "" FORCE)

    set(ALSOFT_BACKEND_DSOUND OFF CACHE BOOL "" FORCE)

    set(ALSOFT_BACKEND_WINMM OFF CACHE BOOL "" FORCE)

elseif (APPLE)

    set(ALSOFT_BACKEND_COREAUDIO ON CACHE BOOL "" FORCE)

else()

    set(ALSOFT_BACKEND_ALSA ON CACHE BOOL "" FORCE)

    set(ALSOFT_BACKEND_PULSEAUDIO ON CACHE BOOL "" FORCE)

    set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "" FORCE)

endif()

# Headless-safe

set(ALSOFT_BACKEND_NULL ON CACHE BOOL "" FORCE)



# Static build

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)



add_subdirectory("${SHADPS4_EXTERNALS_DIR}/openal-soft" "openal-soft" EXCLUDE_FROM_ALL)

endif()



# LibreSSL

if (TARGET LibreSSL::SSL OR TARGET LibreSSL::Crypto)

    set(SSL_DEPENDENCIES LibreSSL::SSL LibreSSL::Crypto)

elseif (TARGET OpenSSL::SSL OR TARGET OpenSSL::Crypto)

    set(SSL_DEPENDENCIES OpenSSL::SSL OpenSSL::Crypto)

else()

    set(LIBRESSL_SKIP_INSTALL ON CACHE BOOL "" FORCE)

    set(LIBRESSL_APPS OFF CACHE BOOL "" FORCE)

    set(LIBRESSL_TESTS OFF CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/libressl" "libressl" EXCLUDE_FROM_ALL)

    get_directory_property(SSL_DEPENDENCIES DIRECTORY "${SHADPS4_EXTERNALS_DIR}/libressl" DEFINITION OPENSSL_LIBS)

endif()



# cpp-httplib

add_library(Cpp_Httplib INTERFACE)

target_include_directories(Cpp_Httplib INTERFACE "${SHADPS4_EXTERNALS_DIR}/cpp-httplib/")



if (TARGET ZLIB::ZLIB)

    target_link_libraries(Cpp_Httplib INTERFACE ZLIB::ZLIB)

    target_compile_definitions(Cpp_Httplib INTERFACE CPPHTTPLIB_ZLIB_SUPPORT)

    message(STATUS "cpp-httplib: zlib support enabled (gzip response decompression)")

else()

    message(STATUS "cpp-httplib: zlib not available — gzip responses will arrive uncompressed")

endif()



if (DEFINED SSL_DEPENDENCIES)

    target_link_libraries(Cpp_Httplib INTERFACE ${SSL_DEPENDENCIES})

    target_compile_definitions(Cpp_Httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)

    message(STATUS "cpp-httplib: SSL support enabled (TLS connections)")

else()

    message(STATUS "cpp-httplib: OpenSSL/LibreSSL not available — libSceHttp HTTPS will fall back to mock at runtime.")

endif()



# abseil (for protobuf)

if (NOT TARGET absl::strings)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/abseil-cpp" "abseil-cpp" EXCLUDE_FROM_ALL)

endif()



# protobuf

set(protobuf_BUILD_TESTS              OFF CACHE INTERNAL "")

set(protobuf_BUILD_EXAMPLES           OFF CACHE INTERNAL "")

set(protobuf_BUILD_PROTOC_BINARIES    ON  CACHE INTERNAL "")

set(protobuf_INSTALL                  OFF CACHE INTERNAL "")

set(protobuf_WITH_ZLIB                OFF CACHE INTERNAL "")

set(protobuf_BUILD_SHARED_LIBS        OFF CACHE INTERNAL "")

set(protobuf_LOCAL_DEPENDENCIES_ONLY  ON  CACHE INTERNAL "")

set(protobuf_FORCE_FETCH_DEPENDENCIES OFF CACHE INTERNAL "")

if(MSVC)

set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)

endif()

add_subdirectory("${SHADPS4_EXTERNALS_DIR}/protobuf" "protobuf" EXCLUDE_FROM_ALL)



# ZArchive (.zar archive support)

if (NOT TARGET ZArchive::zarchive)

    # zstd (dependency of ZArchive)

    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)

    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)

    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)

    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)

    set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)

    set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)

    add_subdirectory("${SHADPS4_EXTERNALS_DIR}/zstd/build/cmake" "zstd/build/cmake")



    add_library(zarchive STATIC

        ${SHADPS4_EXTERNALS_DIR}/zarchive/src/zarchivereader.cpp

        ${SHADPS4_EXTERNALS_DIR}/zarchive/src/zarchivewriter.cpp

        ${SHADPS4_EXTERNALS_DIR}/zarchive/src/sha_256.c

    )

    target_include_directories(zarchive PUBLIC "${SHADPS4_EXTERNALS_DIR}/zarchive/include")

    target_include_directories(zarchive PRIVATE "${SHADPS4_EXTERNALS_DIR}/zstd/lib")

    target_link_libraries(zarchive PRIVATE libzstd_static)

    target_compile_features(zarchive PRIVATE cxx_std_20)

    add_library(ZArchive::zarchive ALIAS zarchive)

endif()

