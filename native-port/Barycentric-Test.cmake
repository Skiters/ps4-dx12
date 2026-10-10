get_filename_component(MESA_PATCHED_CONVERTER "${SHADPS4_MESA_CONVERTER_DLL}" ABSOLUTE)
if(EXISTS "${MESA_PATCHED_CONVERTER}")
    set(_shader_fixtures "${PORT_ROOT}/src/shader_recompiler/backend/dxil/tests/barycentric")
    set(_glslang "${SHADPS4_GLSLANG_EXECUTABLE}")
    set(_spv_outputs)
    foreach(_mode pervertex barycentric)
        set(_case "${CMAKE_CURRENT_BINARY_DIR}/fixtures-${_mode}")
        file(MAKE_DIRECTORY "${_case}")
        add_custom_command(OUTPUT "${_case}/vertex.spv" "${_case}/pixel.spv"
            COMMAND "${_glslang}" -V "${_shader_fixtures}/test.vert" -o "${_case}/vertex.spv"
            COMMAND "${_glslang}" -V "${_shader_fixtures}/${_mode}.frag" -o "${_case}/pixel.spv"
            DEPENDS "${_shader_fixtures}/test.vert" "${_shader_fixtures}/${_mode}.frag" VERBATIM)
        list(APPEND _spv_outputs "${_case}/vertex.spv" "${_case}/pixel.spv")
        add_test(NAME ${_mode}_vertex_compile COMMAND shadps4_dxil_spirv_test
            "${MESA_PATCHED_CONVERTER}" "${_case}/vertex.spv" vertex "${_case}/vertex.dxil")
        add_test(NAME ${_mode}_pixel_compile COMMAND shadps4_dxil_spirv_test
            "${MESA_PATCHED_CONVERTER}" "${_case}/pixel.spv" pixel "${_case}/pixel.dxil")
        set_tests_properties(${_mode}_vertex_compile ${_mode}_pixel_compile PROPERTIES FIXTURES_SETUP ${_mode})
        add_test(NAME ${_mode}_gpu_correctness COMMAND shadps4_dxil_draw_test ${_mode})
        set_tests_properties(${_mode}_gpu_correctness PROPERTIES
            WORKING_DIRECTORY "${_case}" FIXTURES_REQUIRED ${_mode})
    endforeach()
    add_custom_target(native_barycentric_fixtures ALL DEPENDS ${_spv_outputs})
    if(EXISTS "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000089e4b2_0.spv")
        add_test(NAME guest_pervertex_fixed COMMAND shadps4_dxil_spirv_test "${MESA_PATCHED_CONVERTER}"
            "${SHADPS4_GUEST_SHADER_FIXTURES}/0x000000000089e4b2_0.spv" pixel)
        add_test(NAME guest_barycentric_fixed COMMAND shadps4_dxil_spirv_test "${MESA_PATCHED_CONVERTER}"
            "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000000d506a1_0.spv" pixel)
        add_test(NAME guest_unsafe_hull_rejected COMMAND shadps4_dxil_spirv_test "${MESA_PATCHED_CONVERTER}"
            "${SHADPS4_GUEST_SHADER_FIXTURES}/0x0000000034121ac6_0.spv" hull unused expect-reject)
    endif()
endif()
