# Native DX12 shader backend work

This directory contains real compiler components, not a completed game renderer.

## Implemented

* `ir_adapter.cpp` consumes the existing `Shader::IR::Program` from shadPS4's GCN frontend.
* `hlsl_emitter.cpp` lowers a strictly checked straight-line subset into HLSL.
* `dxil_compiler.cpp` invokes Microsoft's DXC library and returns actual Shader Model 6 DXIL bytecode suitable for a D3D12 pipeline state.
* No Vulkan API, Vulkan device, SPIR-V compiler, or DX12/Vulkan interop is used by these components.

The supported subset includes scalar float/integer arithmetic, bitcasts, simple selection, vector construction/extraction, pixel interpolants, pixel render-target exports, vertex position/parameter exports and discard. Floating-point instruction flags currently cause rejection because blindly ignoring them changes guest semantics.

## Deliberately unsupported

Textures, buffers, atomics, subgroup operations, multiple basic blocks/Phi/control flow, geometry/tessellation/compute stages, fetch shader inputs, guest descriptors and rasterization-state mapping are not implemented. Unsupported instructions return a descriptive error with no emitted shader. This code cannot yet compile arbitrary Bloodborne shaders or boot Bloodborne by itself.

## Integration

1. Keep `Shader::TranslateProgram` and its existing GCN frontend/IR passes.
2. Call `Shader::Backend::DX12::EmitProgram(program, Stage::Vertex/Pixel)` instead of `SPIRV::EmitSPIRV` in a new D3D12 pipeline cache.
3. On a supported result, call `CompileDxil(absolute_dxcompiler_dll_path, source, stage)`.
4. Attach returned bytecode to `D3D12_GRAPHICS_PIPELINE_STATE_DESC::VS/PS`; descriptor root signatures and PS4 resource mappings still need implementation.
5. On an unsupported result, report that exact unsupported feature. Do not silently fall back to the working Vulkan application or claim DX12 game compatibility.

`Build-Test.cmd` compiles and runs a portable compiler test using the DXC DLL already present in `work/fsr-sdk-local`. It produces actual `pixel.dxil` and `vertex.dxil`, and checks rejection of unsupported texture/Phi/flags/stage cases. These tests exercise shader compilation only, not emulator gameplay or GPU frame-rate performance.

`Compile-Adapter.cmd` compiles the actual IR adapter against the existing shadPS4 source/dependency headers. It is a workspace-specific compile check, not a standalone distributable build system. Adapter integration/runtime tests remain necessary.

Validation on 2026-10-08: pixel DXIL 3028 bytes, fullscreen-triangle vertex DXIL 3060 bytes, all compiler tests passed; actual IR adapter compilation passed. The standalone GPU test created a graphics pipeline on Intel Arc B390, executed the emitted shaders with DrawInstanced, copied a 4×4 render target to CPU memory, and verified all 16 pixel colors. This is a native D3D12 shader execution test, not a Bloodborne gameplay test.

`shadps4_dxil_ir_test` additionally constructs the original shadPS4 `IR::Program`, `Block`, `Inst` and `Value` objects, invokes the actual `EmitProgram` adapter, and writes vertex/pixel DXIL to its current directory. The same D3D12 draw/readback test passed with these artifacts. The fixture links the original IR implementation sources unchanged; its diagnostic sink is standalone-test-only and aborts on IR assertion failures. Structured branches and multiple basic blocks are explicitly rejected in this test. This is still a synthetic original-IR shader, not a decoded guest GCN shader.

DXC compilation explicitly loads the sibling `dxil.dll` validator before `dxcompiler.dll`, so generated shaders are signed without relying on the current directory or shell PATH. The GPU test may enable an already-available D3D12 debug layer for test diagnostics; it installs nothing and changes no machine settings.

## Broader offline SPIR-V compiler path

`spirv_converter.h/.cpp` wraps the separately built Mesa `spirv_to_dxil.dll` C API. `ConvertSpirv` accepts an offline SPIR-V word span, explicit shader stage/entrypoint, SM6.x and validator compatibility versions, descriptor spaces, system-value modes and specialization constant bits. It returns runtime-data/draw-system-value metadata and signed, validated DXIL. On conversion or validation failure it returns diagnostics with no usable bytecode. The DLL and each allocation/COM object's lifetime are managed independently; no Vulkan runtime is loaded.

Mesa conversion alone is insufficient: it can produce a nonempty DXIL container rejected by Microsoft's validator. The wrapper therefore validates/signs each result with the explicitly loaded sibling `dxil.dll` and `dxcompiler.dll`. The `shadps4_dxil_spirv_test` target reads supplied cached shaders without editing them and returns failure for either compiler or validator errors. Optional `expect-reject` explicitly checks an unsupported fixture.

On 2026-10-08 the strict wrapper converted and validated **28 of 30 actual cached Bloodborne SPIR-V shaders**. Two were rejected: `0x000000000089e4b2_0.spv` (invalid `dx.op.loadInput.f32` operand during DXIL validation) and `0x0000000000d506a1_0.spv` (unimplemented `load_barycentric_coord_pixel`). This does not establish full-game shader coverage, resource compatibility, or correct rendering. The compiler sample is small; no game was launched for this check. Detailed results are in workspace `outputs/dx12-port-audit/wrapper-shader-tests/results.json`.

## Linked pipeline conversion and scoped Mesa corrections

The separate writable Mesa overlay now correctly lowers constant per-vertex pixel inputs and perspective pixel-center barycentrics. Both paths passed signed DXIL validation and native D3D12 triangle/readback checks. Barycentrics require Shader Model 6.1 or newer and hardware `BarycentricsSupported`; unsupported variants are rejected. The broader audit passed 525 independent shaders; the remaining 16 unique tessellation shaders require pipeline linkage.

`linked_spirv_converter.cpp` implements `ConvertLinkedStages`, using the new overlay C export `spirv_to_dxil_linked`. Each input supplies its stage, SPIR-V span, entrypoint and specializations. Shared options supply descriptor/system-value conventions and compiler versions. Stage inputs may be unordered; results preserve input order. Mesa prepares all stages, links the graphics chain in reverse stage order, then emits each DXIL container. Hull and domain stages must be supplied together. Compute cannot be mixed with graphics. The existing standalone hull rejection remains active.

Every linked output is validated and signed with Microsoft's validator. Results commit only when all stages succeed; errors return no partial stage bytecode. Mesa allocations, DXC objects and DLL lifetimes are managed in dependency order. This is an in-process C API, not a CLI subprocess or Vulkan runtime bridge.

Include `CMake-Linked.cmake` after the base backend target to add the wrapper and `shadps4_dxil_linked_test`. Workspace reproduction uses `work/dx12-shader-backend/Build-LinkedStandalone.cmd`, `outputs/dx12-port-audit/build-mesa-bary-dll.cmd` and `outputs/dx12-port-audit/Run-Linked-CAPI.ps1`; pass `-Asan` after building with `build-mesa-linked-asan.cmd` for instrumented Mesa verification.

All nine actual cached Bloodborne hull/domain pipeline pairs passed conversion, signing and validation through the new API. Tests also cover reverse input ordering, missing paired stages, duplicates and invalid module rejection. The same nine pairs passed with AddressSanitizer-instrumented Mesa (`detect_leaks=0`; the test client itself is not instrumented). Results are `outputs/dx12-port-audit/linked-capi-tests/results.json` and `asan-results.json`. These checks establish compilation coverage of the captured shader set, not full-game compatibility or tessellation GPU rendering correctness.
