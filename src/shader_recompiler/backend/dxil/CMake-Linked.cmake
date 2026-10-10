# Include after the base DXIL CMakeLists from the separate native-port build.
include_guard(GLOBAL)
get_filename_component(SHADPS4_LINKED_OVERLAY "${CMAKE_CURRENT_LIST_DIR}/../../../../../mesa-dxil-bary-overlay/src/microsoft/spirv_to_dxil" ABSOLUTE)
target_sources(shadps4_dxil_compiler PRIVATE "${CMAKE_CURRENT_LIST_DIR}/linked_spirv_converter.cpp")
target_include_directories(shadps4_dxil_compiler PRIVATE "${SHADPS4_LINKED_OVERLAY}")
add_executable(shadps4_dxil_linked_test "${CMAKE_CURRENT_LIST_DIR}/tests/linked_spirv_test.cpp")
target_link_libraries(shadps4_dxil_linked_test PRIVATE shadps4_dxil_compiler)
