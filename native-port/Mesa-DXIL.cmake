function(native_mesa_runtime target)
    get_filename_component(_mesa_repo "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    include("${_mesa_repo}/native-port/Native-DXC.cmake")
    native_dxc_identity()
    find_program(SHADPS4_MESON_EXECUTABLE NAMES meson.exe meson HINTS "F:/PS4Dev/.venv/Scripts" REQUIRED)
    find_program(SHADPS4_MESA_PYTHON_EXECUTABLE NAMES python.exe python HINTS "F:/PS4Dev/.venv/Scripts" REQUIRED)
    find_program(SHADPS4_MESA_NINJA_EXECUTABLE NAMES ninja.exe ninja HINTS "F:/PS4Dev/.venv/Scripts" REQUIRED)
    find_program(SHADPS4_MESA_RC_EXECUTABLE NAMES rc.exe HINTS "$ENV{WindowsSdkVerBinPath}/x64" REQUIRED NO_DEFAULT_PATH)
    get_filename_component(_dxc_root "${SHADPS4_DXC_INCLUDE_DIR}" DIRECTORY)
    set(SHADPS4_DXC_RUNTIME_DIR "${_dxc_root}/bin/x64" CACHE PATH "DXC compiler and validator runtime matching the configured SDK")
    foreach(_dll dxcompiler.dll dxil.dll)
        if(NOT EXISTS "${SHADPS4_DXC_RUNTIME_DIR}/${_dll}")
            message(FATAL_ERROR "Missing native DXIL runtime dependency: ${SHADPS4_DXC_RUNTIME_DIR}/${_dll}")
        endif()
    endforeach()
    file(TO_CMAKE_PATH "${SHADPS4_MESA_PYTHON_EXECUTABLE}" _mesa_python)
    file(TO_CMAKE_PATH "${SHADPS4_MESA_RC_EXECUTABLE}" _mesa_rc)
    file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-msvc.ini"
        CONTENT "[binaries]\nc = 'cl'\ncpp = 'cl'\npython = '${_mesa_python}'\nwindres = '${_mesa_rc}'\n" @ONLY)
    include(ExternalProject)
    ExternalProject_Add(shadps4_native_mesa_runtime
        PREFIX "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-project"
        GIT_REPOSITORY "${SHADPS4_MESA_DXIL_ROOT}"
        GIT_TAG b628375fb1fd99476bfbaaa0532deb6fe3a6870e
        UPDATE_DISCONNECTED TRUE
        SOURCE_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-source"
        BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-build"
        PATCH_COMMAND ${CMAKE_COMMAND}
            "-DMESA_SOURCE=<SOURCE_DIR>"
            "-DMESA_PATCH=${_mesa_repo}/native-port/vendor/mesa-b628375f-native.patch"
            "-DMESA_LINKED_HEADER=${_mesa_repo}/native-port/vendor/mesa-api/spirv_to_dxil_linked.h"
            -P "${_mesa_repo}/native-port/Apply-Mesa-Native.cmake"
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=cl CXX=cl PYTHONIOENCODING=utf-8
            "${SHADPS4_MESON_EXECUTABLE}" setup "<BINARY_DIR>" "<SOURCE_DIR>"
            --native-file "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-msvc.ini"
            --buildtype=release -Dspirv-to-dxil=true -Dgallium-drivers=[] -Dvulkan-drivers=[]
            -Dllvm=disabled -Dplatforms=[] -Dopengl=false -Degl=disabled
            -Dgles1=disabled -Dgles2=disabled -Dglx=disabled -Dmicrosoft-clc=disabled -Dc_args=/wd4189
        BUILD_COMMAND "${SHADPS4_MESA_NINJA_EXECUTABLE}" -C "<BINARY_DIR>" -j 20
            src/microsoft/spirv_to_dxil/spirv_to_dxil.dll
        BUILD_ALWAYS TRUE
        BUILD_BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-build/src/microsoft/spirv_to_dxil/spirv_to_dxil.dll"
        INSTALL_COMMAND "")
    ExternalProject_Add_Step(shadps4_native_mesa_runtime source_contract
        DEPENDEES configure DEPENDERS build ALWAYS TRUE
        COMMAND ${CMAKE_COMMAND}
            "-DMESA_SOURCE=<SOURCE_DIR>"
            "-DMESA_PATCH=${_mesa_repo}/native-port/vendor/mesa-b628375f-native.patch"
            "-DMESA_LINKED_HEADER=${_mesa_repo}/native-port/vendor/mesa-api/spirv_to_dxil_linked.h"
            -P "${_mesa_repo}/native-port/Apply-Mesa-Native.cmake")
    add_custom_target(shadps4_native_compiler_files
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/native-compiler"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/native-mesa-build/src/microsoft/spirv_to_dxil/spirv_to_dxil.dll"
            "${SHADPS4_DXC_RUNTIME_DIR}/dxcompiler.dll" "${SHADPS4_DXC_RUNTIME_DIR}/dxil.dll"
            "$<TARGET_FILE_DIR:${target}>/native-compiler"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_mesa_repo}/native-port/vendor/mesa-native.json"
            "${_mesa_repo}/native-port/vendor/mesa-b628375f-native.patch"
            "${_mesa_repo}/native-port/vendor/dxc-native.json"
            "$<TARGET_FILE_DIR:${target}>/native-compiler"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/native-compiler/mesa-api"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_mesa_repo}/native-port/vendor/mesa-api/spirv_to_dxil_linked.h"
            "$<TARGET_FILE_DIR:${target}>/native-compiler/mesa-api"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses/Mesa" "$<TARGET_FILE_DIR:${target}>/licenses/DXC" "$<TARGET_FILE_DIR:${target}>/licenses/AddrLib" "$<TARGET_FILE_DIR:${target}>/licenses/PixEvents"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SHADPS4_MESA_DXIL_ROOT}/docs/license.rst"
            "$<TARGET_FILE_DIR:${target}>/licenses/Mesa/license.rst"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_mesa_repo}/native-port/vendor/addrlib/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/AddrLib/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_mesa_repo}/native-port/vendor/pix-events/LICENSE.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/PixEvents/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dxc_root}/LICENSE-LLVM.txt"
            "${_dxc_root}/LICENSE-MIT.txt" "${_dxc_root}/LICENSE-MS.txt"
            "$<TARGET_FILE_DIR:${target}>/licenses/DXC"
        DEPENDS shadps4_native_mesa_runtime)
    add_dependencies(${target} shadps4_native_compiler_files)
endfunction()
