# Native compute milestone

`compute_pipeline.h` creates a native D3D12 compute pipeline from DXIL, an explicit
root UAV buffer binding (u0/space0 by default), and Mesa compute runtime constants
at b0/space31. Runtime layout follows `dxil_spirv_compute_runtime_data`: dispatch
group counts and zero base group. Resource transitions and UAV barriers are real.

`Build-Compute-Test.cmd` generates a GLSL storage-buffer fixture, compiles it to
SPIR-V using the existing glslang binary, converts it via Mesa and signs/validates
the DXIL with the existing DXIL validator. There is no Vulkan loader or device.
The signed compute program dispatches 1024 invocations on Intel Arc B390 and writes
`output[index] = index * 3 + 7`; native readback verified all 1024 results exactly.

The fixture is synthetic, not a captured PS4 compute shader. This does not yet
connect guest resource metadata/workgroup registers to native dispatch, support
arbitrary UAV/SRV/CBV layouts or implement Rasterizer::DispatchDirect. The helper
assumes the compiler options' runtime CBV remains b0/space31 and zero dispatch base.
The caller retains buffers until submission completion and verifies shader bounds.
`Compute-Test.cmake` adds the wrapper-linked test to the separate native-port build.
