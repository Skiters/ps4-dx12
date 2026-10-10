# Standalone native-port compiler modules. Include from the separate native-port build.
# SHADPS4_REFERENCE_BUILD_DIR points to the existing read-only dependency build.
if(NOT WIN32)
    message(FATAL_ERROR "Native DXIL compiler currently requires Windows")
endif()
get_filename_component(SHADPS4_DXIL_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(SHADPS4_DXIL_REPO_ROOT "${SHADPS4_DXIL_SOURCE_ROOT}/..")
set(SHADPS4_DXC_INCLUDE_DIR "${SHADPS4_DXIL_REPO_ROOT}/../fsr-sdk-local/sdk/tools/ffx_shader_compiler/libs/dxc/inc" CACHE PATH "Existing DXC API headers")
set(SHADPS4_DEPENDENCY_ROOT "${SHADPS4_DXIL_REPO_ROOT}" CACHE PATH "Read-only reference source dependencies")
add_library(shadps4_dxil_compiler STATIC hlsl_emitter.cpp dxil_compiler.cpp)
set(SHADPS4_MESA_DXIL_ROOT "${SHADPS4_DXIL_REPO_ROOT}/../mesa-dxil-source" CACHE PATH "Offline Mesa SPIR-V to DXIL source headers")
target_sources(shadps4_dxil_compiler PRIVATE spirv_converter.cpp)
target_include_directories(shadps4_dxil_compiler PRIVATE
    "${SHADPS4_MESA_DXIL_ROOT}/src/microsoft/spirv_to_dxil"
    "${SHADPS4_MESA_DXIL_ROOT}/src/microsoft/compiler")
target_compile_features(shadps4_dxil_compiler PUBLIC cxx_std_20)
target_include_directories(shadps4_dxil_compiler PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" PRIVATE "${SHADPS4_DXC_INCLUDE_DIR}")
target_link_libraries(shadps4_dxil_compiler PRIVATE ole32)
add_library(shadps4_dxil_ir_adapter OBJECT ir_adapter.cpp)
target_compile_features(shadps4_dxil_ir_adapter PUBLIC cxx_std_23)
target_include_directories(shadps4_dxil_ir_adapter PRIVATE
    "${SHADPS4_DXIL_SOURCE_ROOT}"
    "${SHADPS4_DEPENDENCY_ROOT}/externals/magic_enum/include"
    "${SHADPS4_DEPENDENCY_ROOT}/externals/ext-boost"
    "${SHADPS4_DEPENDENCY_ROOT}/externals/gcn/include"
    "${SHADPS4_DEPENDENCY_ROOT}/externals/half/include"
    "${SHADPS4_REFERENCE_BUILD_DIR}/_deps/fmt-src/include")
file(GLOB SHADPS4_BOOST_INCLUDE_DIRS "${SHADPS4_DEPENDENCY_ROOT}/externals/ext-boost/libs/*/include")
target_include_directories(shadps4_dxil_ir_adapter PRIVATE ${SHADPS4_BOOST_INCLUDE_DIRS})
target_compile_definitions(shadps4_dxil_ir_adapter PRIVATE NDEBUG)
add_executable(shadps4_dxil_shader_test tests/test.cpp)
target_link_libraries(shadps4_dxil_shader_test PRIVATE shadps4_dxil_compiler)
add_executable(shadps4_dxil_draw_test tests/gpu_test.cpp)
target_compile_features(shadps4_dxil_draw_test PRIVATE cxx_std_20)
target_link_libraries(shadps4_dxil_draw_test PRIVATE d3d12 dxgi)
add_executable(shadps4_dxil_ir_test tests/ir_test.cpp tests/ir_test_diagnostics.cpp
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/basic_block.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/microinstruction.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/value.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/opcodes.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/attribute.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/patch.cpp"
    "${SHADPS4_DXIL_SOURCE_ROOT}/shader_recompiler/ir/type.cpp")
target_compile_features(shadps4_dxil_ir_test PRIVATE cxx_std_23)
target_compile_definitions(shadps4_dxil_ir_test PRIVATE FMT_HEADER_ONLY NDEBUG)
target_include_directories(shadps4_dxil_ir_test PRIVATE $<TARGET_PROPERTY:shadps4_dxil_ir_adapter,INCLUDE_DIRECTORIES>)
target_link_libraries(shadps4_dxil_ir_test PRIVATE shadps4_dxil_compiler shadps4_dxil_ir_adapter)
add_executable(shadps4_dxil_spirv_test tests/spirv_test.cpp)
target_link_libraries(shadps4_dxil_spirv_test PRIVATE shadps4_dxil_compiler)
