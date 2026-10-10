# Native D3D12 guest renderer

This backend implements the separate fork's `RasterizerInterface` and VideoOut presentation through D3D12/DXGI. It is selected unconditionally by the full emulator. There is no Vulkan renderer or runtime fallback.

Implemented paths include mapped guest buffers and GDS, copies/fills, GCN shader translation to validated DXIL, native graphics/compute pipelines, guest resource descriptors, tiled textures with mips/arrays/volumes, supported BC and packed formats, render targets, depth/stencil, native swapchain presentation and a D3D12 UI overlay. Unsupported guest states fail explicitly. The native-port hardware tests check actual output through GPU readback.

The low-level `Rasterizer` buffer implementation rejects raw draw/dispatch calls; the integrated `GuestRasterizer` supplies shader/resource translation and submits the supported operations through native D3D12 pipelines. These are different layers of the same native backend.

The implementation reconciles written guest attachment memory synchronously after draws. Transfers, constants, scratch and unchanged depth/stencil uploads are reused; read-only depth/stencil skips writeback. Display scaling runs on the GPU; eligible coherent display textures avoid CPU detiling/reupload. Substantial synchronization overhead and incomplete compatibility remain. Native-only Bloodborne title/menu pixels were verified. Observed uniform-CMASK, null-resource and depth-bounds failures are resolved; saved-world rendering is unverified. No playable-game or 60 FPS claim is made. 64 checks pass, including isolated XeSS SDK execution; XeSS/XMX is not integrated into guest rendering.

Build the full core with `native-port/Build-Core.cmd`, and run isolated checks with `native-port/Build-Check.cmd`. See `NATIVE_DX12_PORT_STATUS.md` at the project root for deployment and verification details.
