# Native guest texture cache subset

`DescribeGuestTexture` consumes the real `AmdGpu::Image` structure. It translates
2D/2DArray dimensions, guest pitch, mip/array counts, pow2 padding, view mip/slice
ranges and component swizzles. Descriptor compression, alternate/Neo tiling,
interlacing, cube/3D/MSAA/PRT and unsupported formats reject explicitly.

Formats supported: R8/RG8/RGBA8 UNORM/SNORM/UINT/SINT, RGBA8 sRGB,
R16/RG16/RGBA16 float and R32 float/uint/sint. No fake block-compression decode or
depth promotion is performed. Depth resource metadata requires a separate path.

Layout and address logic follows `texture_cache/tile.h`, `ImageInfo::UpdateSize`
and the original `host_shaders/tiling.comp`: aligned linear, 8x8 Thin and Display
microtiles, base-mode Thin2D/Display2D macrotiles (8 pipes, 16 banks, no alternate
swizzle), per-array-layer bank rotation, small-mip fallback to microtiling, and
the original mip-major guest storage. D3D12 subresources are reordered to native
mip-plus-layer indexing. GPU resources have real mip levels/array layers and
native SRVs preserve the guest component mapping and view range.

The cache accepts a caller-provided byte span rather than reading guest pointers.
The memory manager must first reconcile GPU-written buffers and safely fetch sparse
guest pages. Upload staging is retained until `Submitted`'s fence completes; every
later graphics/compute use must call `Used` with its submission fence. Invalidation
rejects pending/in-flight use. Caller serializes the device command context and
keeps the cache/resources alive through completion. The cache uses explicit 128MiB
per-texture and 256MiB aggregate guest-size guards; these are bounds rather than a
driver VRAM budget manager. No GPU-dirty texture readback/retiling, texture-buffer
physical alias ownership or automatic CPU-write tracking is implemented.

Validation on Intel Arc B390:

- Actual AmdGpu image metadata -> independently golden-encoded PS4 Thin 8x8
  microtile fixture -> native 3-mip/2-array-layer texture -> exact GPU readback of
  every visible texel. Includes view creation, short input and in-flight guards.
- CPU Thin2D macro/micro addressing matched the **original shadPS4 detiler shader**
  compiled offline through glslang -> SPIR-V -> Mesa -> signed DXIL, dispatched
  directly in D3D12. All 26,428 visible texels across 4 mips and 2 array layers
  matched. Display2D specialization passed the same comparison.

This validates real PS4 tiling algorithms on synthetic guest fixtures. It does not
claim that captured Bloodborne textures, rendering or game boot already work.
`Guest-Texture-Test.cmake` integrates three hardware tests in the separate build.
