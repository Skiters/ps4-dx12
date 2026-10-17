# Implementation and verification registry — 2026-10-17

## Current final-buffer / physical-DMA checkpoint — 2026-10-17

This section supersedes older current/next statements; SESSION_HANDOFF.md remains authoritative. Source9791682 selects native buffer descriptors only after attachment preparation and validates original ranges against explicit full-copy allocation ancestry. Source0d5b4693e0784bdc7e01aaa9db2d6b2275851367 materializes missing legal64KiB/4KiB DMA backing from physical B before that final selection, preserving independent I and preceding GPU B1. Existing initialized backing is not rescanned here; BindResident retains its strict guard and single post-stage synchronization. Stage effects/private streams/bounds/CPU visibility/fence leases remain.

Nine related native fixtures passed (envelope/ownership/generations, DMA, two new graphics late-growth/private-snapshot controls, readonly and UAV indexed draw, independent I/B, buffer-view lifetime and scheduler lifetime). DMA additionally passed with the exact runtime Mesa converter; its normal fixture converter has a distinct hash despite identical pinned source/patch metadata, so identities/results are recorded separately. Complete primary and separate clean Release compiled/linked;54 dependencies,2283 compile inputs, zero primary-source inputs in clean build. Runtime `F:/PS4Dev/handoff/attachment-backing-20261017/artifacts-0d5b469`, EXE `abfa3efbf468e2e6ce24e001e172e4842e602e0f818da658ff07c9daa98a151e`, PDB `4a83d098e0106176d831a306c3bafb4879145e7b2a592a1901e57d08060fe4e0`; exact core runtime/game/FPS acceptance not_run. Evidence `F:/PS4Dev/handoff/attachment-backing-20261017`, RELEASE_PROOF_0d5b469.json and NATIVE_VERIFICATION_0d5b469.json. No game/app/UI/XInput/profiler run, core auto-install/selection or user-process restart this continuation.

Full port remains unfinished:129 implementation/behavior records, nine gap IDs. Next: initial opaque DMA request batches still call the legacy CPU image resolution path before ordinary shader preparation. Separate their physical B intent without dropping real shader-buffer BindBuffers effects, then adapt ordered opaque depth/stencil I/B domains. Direct Find, mixed SRV/UAV split, pressure B backing, private IA/auxiliary and other existing gap IDs remain. Do not redo the completed final-view or late physical-materialization mechanism, original crash replay or129-group audit.


## Current crash / GPU-contract checkpoint — 2026-10-17

Authoritative current state: SESSION_HANDOFF.md, crash evidence `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z`. This section supersedes older current/next claims and the old all-tests-deferred policy. Narrow regression and GPU/readback checks are now authorized; full suite and expanded gameplay acceptance remain final. No per-commit game launches, automatic core installation/selection, original Vulkan/save changes or user-app restart.

Production935fb69 restores existing prepared writable B ownership after prewrite reads/Finish, without replacing selected resources or bypassing the ownership assertion; fatal messages synchronously reach stderr/log and the launcher captures per-launch streams/manifest/hex exit/report. db0812c preserves signed integer clear bits through compatible UINT views and explicit UAV ordering. Production8fb9c79 reserves complete legal DMA backing for late image sources in ordered common native I/B domains before final views, preserving original semantic publication phases.

The original exactd01969e replay trapped in ExecuteGuestBufferCompute: `Independent image version lost its writable buffer ownership before execution`, followed by Liverpool coroutine UNREACHABLE -> int3. Original EXE/PDB/runtime, profile copy, logs, exception/context/stacks and dump are preserved. The exact935fb69 clean binary survived a120s bounded diagnostic startup exposure without this exception; its owned tree was stopped at deadline. No world/input/pixel/FPS acceptance, and that result is not transferred to8fb9c79.

Six exact native fixture binaries are preserved with PASS records in `narrow-regressions-8fb9c79.json`: envelope/semantic B, DMA (including late image full/partial backing), independent I/B, buffer view lifetime, scheduler lifetime and PageWatch/GPU ownership. The latter also covers typed/converted image clears and gated resource lifetime;34.08s, not the former20s budget. Normalized half ties have tightly specified alternatives; original expected bytes/negative checks/padding remain. Clean committed Release at8fb9c79ea55c8a71d5befebffa0797de98f59523,54 pinned dependencies,2283 compile inputs, zero primary checkout inputs. Full runtime `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z/artifacts-8fb9c79`; EXE SHA256 `3e822bf2a7c46fcc8c8c7dc242c36df4ba895f65ce8dbd7ae03be1032f5a0fab`, PDB `765be1db16d597c59bd40f376e511e8a763d13dd0fd4cdbe18893dfb8c6a8e09`. This exact core is not game-tested or automatically installed.

Full port is unfinished:129 independent implementation/behavior records, nine gap IDs retained. Next: extend pre-view source-footprint/backing planning beyond ordered common I/B domains to attachment-only/converted/BC/opaque source receipts; inspect GuestRasterizer::Draw submit_replay and ExecuteGuestResources before changing final views. Keep existing native identity/SRV-UAV/ownership guards. Direct Find/mixed-class/pressure B backing and private IA/auxiliary unification remain. App DLL was locked by the active user process: old DLL/PDB pair remains installed, new diagnostics pair staged; no process restart. Apply script/details in SESSION_HANDOFF.md.


## Current image-source publication checkpoint - 2026-10-17

Production `2498c6fafe567e13a6b8fe71937aa967a1632b9b` connects version-specific image B-source residency: prewarm/capture reserves physical backing and retains deferred effects; actual image binding consumes them, including inherited independent I conversions and prewrite shader/depth/stencil sources. Publication preserves retry state, discards overwritten-version effects, checks existing source backing and does not replace native views. Primary and separate clean committed Release linked; six fixture TUs passed C++ /Zs in each checkout. All new runtime/shader/GPU/game/XInput/UI/FPS acceptance remains not_run.129 independent implementation/behavior records and nine gap IDs remain. Latest complete external runtime: F:/PS4Dev/handoff/image-source-publication-20261017/artifacts-2498c6f. Do not auto-install/select. Next: complete legal tiny DMA backing before final selected views, preserve GPU B/witnesses/fence leases, then the existing source-gap queue. SESSION_HANDOFF.md is authoritative; older sections preserve history.


## Current semantic B residency checkpoint - 2026-10-17

Production `d01969e9e7c5cb5336b3937f4f2c9aa85b35f768` separates shader-buffer semantic residency from physical DEFAULT preallocation, publishes original buffers in bind order, and masks formatted I-to-B copies to the pages resident at that point. Clean small shader reads use private full DWORD envelopes; host/domain reservations and private I conversion captures do not publish pages. Final refreshed images consume masked B, with selected native states restored. Primary and clean committed Release linked; five fixture TUs passed C++ /Zs in both checkouts. All new runtime/GPU/game/XInput/UI/FPS results remain not_run. Full port unfinished;129 implementation/behavior records and nine gap IDs retained. Latest full external runtime: F:/PS4Dev/handoff/semantic-buffer-residency-20261017/artifacts-d01969e. Do not auto-install/select cores. SESSION_HANDOFF.md is authoritative; older sections preserve history.


## Current preserving B growth checkpoint - 2026-10-17

Production558baa3/f16630d preserves complete preceding GPU B ranges, old CPU snapshots, forced CPU-store pages and RO witnesses across compatible batch envelope growth. Only new holes initialize from physical B; old allocations survive the copy fence. Existing actual guest-write reconciliation remains. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only, no new runtime/GPU/game/XInput/UI or FPS result. Latest full external runtime: F:/PS4Dev/handoff/buffer-envelope-preservation-20261017/artifacts-f16630d. Never auto-install/select it. Full port remains unfinished: semantic DMA residency publication, tiny full-subpage materialization and stable final views, direct/split/pressure B residency, auxiliary SRV unification, opaque depth/stencil and the existing nine-gap queue remain.

Authoritative proof and exact continuation: SESSION_HANDOFF.md. Older continuation entries retain history.


## Current invalid-I pressure checkpoint - 2026-10-17

Production `284be47d1bfacd32bdc8a871759bf6953a1edcf4` connects pressure eviction of invalid independent I without downloading stale pixels into physical B. Actual cache admission shares ReclaimResidentMemory with ReserveMemory; preserve pins, completion, callback identity and budget refresh are retained. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution or FPS result. Latest full external runtime: F:/PS4Dev/handoff/invalid-image-pressure-20261017/artifacts-284be47. Never auto-install/select it. Valid GPU-modified tiled I remains excluded from pressure download by the pinned Vulkan rule; older broad statements calling that exclusion a gap are superseded. Late tiny DMA materialization/stable B residency, SRV auxiliary unification, opaque depth/stencil and existing nine-gap queue remain unfinished.129 implementation/behavior records and prior behavior evidence are retained.

Authoritative evidence and exact C++ continuation: SESSION_HANDOFF.md. Older current/next sections preserve history.


## Current late resident DMA checkpoint - 2026-10-17

Production `7cc493f72c96a0492cc165166987ad4c7f38fa88` publishes fully materialized late DMA pages at BeforeBind: complete64KiB pages or4KiB subpages, exact existing mapping/offset coverage, original selected-view identity guards, no reservation/retile, single CPU-dirty B synchronization and placed-allocation retention. Primary and clean committed Release linked; four fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution. Latest external full runtime: F:/PS4Dev/handoff/late-dma-publication-20261017/artifacts-7cc493f. Never auto-install/select it. Remaining: complete-page materialization for tiny late sharps, late arena replacement, auxiliary SRV/UAV unification, opaque depth/stencil and tiled independent pressure/retirement, then existing converted/shifted/clipped/nine-gap backlog.129 groups remain separate implementation/behavior records:59 with concrete gaps,70 without a separately established production change. Full port remains unfinished.

This section supersedes older continuation entries. Exact evidence and next source action: SESSION_HANDOFF.md.


## Current planned resident DMA checkpoint - 2026-10-17

Production50bdc81/a564b6c/34232f8 connect synchronization of already planned resident DMA B after image binding, preserve CPU streams and physical guest backing, and safely borrow/release PageWatch image/buffer witnesses. Opaque depth/stencil retains the legacy exclusive-image/physical-backing path; it is not completed independent transfer. Primary and separate clean committed Release linked at34232f8. For these changes runtime/GPU/game/XInput/UI tests are not_run; fixture TUs passed C++ /Zs only. Earlier behavior evidence is retained. Latest full external runtime: F:/PS4Dev/handoff/ordered-dma-sync-20261017/artifacts-34232f8. Never auto-install/select cores. Next: stable late DMA residency/table publication before final native descriptors; then remaining opaque/tiled-pressure/converted/shifted transfer guards. Registry:129 groups,59 connected_with_known_gaps,70 with no separately established change, nine broader gap IDs. The DMA group now explicitly records the proven late-page implementation gap; unrun behavior is separate. Full port remains unfinished.

This section supersedes older current/next entries; full evidence and exact continuation: SESSION_HANDOFF.md.


Pinned reference: `e0a466753a1aacbc8db8253be685bf7ab4d9daae`. All 129 groups have committed implementation/integration sources.
Release core built from clean commit `72569a531e1c262fc828e80d56400f7f1507d75a`. Desktop C# and prepared fixtures were retained, not executed by this core build.

`connected` records integration, not exhaustive semantic closure. An empty change list means no specific remaining production change established for that group.
Historical review notes remain in contracts.json. Unrun behavior is never counted as missing production implementation.
The known backlog below retains real native guards; this registry does not certify the whole port complete.

## Concrete remaining production changes

| ID | Upstream symbol → native symbol | Missing semantics |
|---|---|---|
| color-attachment-feedback | Vulkan::Rasterizer::BindResources / BeginRendering → NativeDX12::GuestRasterizer::Draw / ExecuteGuestResources | Shared color attachment and shader feedback through the same physical storage; prewrite copies alone do not provide ordered per-fragment feedback. |
| formatted-alias-ordering | Vulkan::BufferCache::EnsureResident / SynchronizeMemoryFromImage / Rasterizer::BindResources / TextureCache::ResolveOverlap / ExpandImage -> NativeDX12::Rasterizer::PrepareShaderBuffers / RematerializeBufferEnvelope / Find / GuestDmaMemory::BindResident / ExecuteGuestResources | Initial raw DMA request batches with opaque I still use legacy CPU resolution before ordinary shader preparation; GPU-preserving direct Find overlap, mixed native SRV/UAV splitting and inactive GPU-only B pressure backing; original private CPU index streams and SRV-only auxiliary DMA unification; ordered opaque depth/stencil I/B transfers; converted or BC word-view stage domains without a common native image allocation, shifted bases, different physical geometries and clipped transfers outside the connected reader-arena proof remain. Shader-buffer residency publishes in original bind order and formatted I-to-B copies affect only already resident pages. Image prewarm now retains version-specific deferred B-source effects; actual image binding consumes them, including inherited independent conversion requirements and prewrite GPU/CPU/depth/stencil sources. Clean small shader streams retain private full DWORD envelopes; host/domain reservations and private I conversions do not publish. Compatible batches preserve preceding GPU B and CPU snapshots/store markers. Fully materialized late DMA publication and invalid I pressure discard remain connected. Valid GPU-modified tiled I pressure retention follows pinned GC and is not another port gap. Formatted readers retain the pinned exact-base assertion. Narrow GPU subcontracts have executed evidence; final broad/game acceptance remains deferred. |
| depth-color-shared-storage | Vulkan::TextureCache::FindDepthTarget / FindRenderTarget / CopyColorAndDepth → NativeDX12::GuestRasterizer::Draw | Simultaneous overlapping depth/color attachments and buffer/HTILE depth-clear writers need shared storage/ownership; they are explicitly rejected. Opaque depth/stencil raw-DMA overlap retains the legacy physical-backing/exclusive-image path; ordered independent I/B transfer for this domain is unfinished. Same-plane depth write feedback is not newly required: the pinned renderer explicitly excludes depth feedback. |
| signed-packed10-attachment | Vulkan::SurfaceFormat / GraphicsPipeline / Rasterizer::UpdateDynamicState → NativeDX12::LayoutGuestTexture / BuildGuestGraphicsState | R10G10B10A2 SNORM attachment writes, exact per-fragment quantization, blend and logic behavior; sampled/storage signed packed paths are already connected. |
| packed19-msaa-attachment | Vulkan::SurfaceFormat / GraphicsPipeline → NativeDX12::LayoutGuestTexture / GuestTextureCache::UploadVersion | Packed format19 multisample render-target writes need exact native quantization/blending where B4G4R4A4 MSAA RTV is unavailable; sampled expansion is already implemented. |
| polygon-point-fill | Vulkan::Rasterizer::UpdateDynamicState → NativeDX12::BuildGuestGraphicsState | Polygon-mode point fill must preserve polygon clipping and point rasterization; ordinary VS/GS point primitives already use implemented expansion/capture. |
| mixed-blend-constants | Vulkan::GraphicsPipeline / Rasterizer::UpdateDynamicState → NativeDX12::BuildGuestGraphicsState | Simultaneous constant-color and constant-alpha RGB blend factors on adapters without native ALPHA_FACTOR require an exact fallback; native supporting adapters are handled. |
| other-vendor-clock | Shader::Backend::SPIRV::EmitMemtime → Shader::Backend::SPIRV::EmitMemtime / NativeDX12 shader clock selection | Hardware S_MEMTIME integration for adapters outside NVIDIA NVAPI and AMD AGS, when their pinned Vulkan path exposes the corresponding clock capability. Never substitute a constant or CPU time. |
| point-capture-stage-domain | Vulkan::PipelineCache::GetGraphicsPipeline / Shader::Backend::SPIRV::EmitEmitVertex → NativeDX12::GuestShaderFrontend::BuildPointCaptureGraphics | Extend oversized point-output capture fallback to the tessellation+geometry stage domain already accepted by pinned graphics; the currently connected capture route requires EsGs. Dynamic ring scalar-offset paths and complete indirect-batch bounds still need adaptation in that fallback. On-chip geometry and stream output are excluded because the pinned renderer skips them. |

## All 129 groups

Only concrete change IDs from the table above are listed. `—` means none established, not runtime PASS.

| # | Group | Implementation | Remaining production changes | Behavior |
|---|---|---|---|---|
| 1 | common-pm4-completion-and-backing-writes | connected | — | not_run |
| 2 | native-texture-binding-lifetime | connected | — | not_run |
| 3 | native-image-depth-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, depth-color-shared-storage | not_run |
| 4 | native-readonly-depth-buffer-prewrite-snapshots | connected_with_known_gaps | depth-color-shared-storage | not_run |
| 5 | native-gpu-buffer-image-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, formatted-alias-ordering | partial_gpu_verified |
| 6 | native-anisotropic-manual-storage-filtering | connected | — | not_run |
| 7 | native-buffer-image-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, formatted-alias-ordering, depth-color-shared-storage | partial_gpu_verified |
| 8 | native-unit-anisotropy-storage-filtering | connected | — | not_run |
| 9 | native-r32-rgba8-shader-format-reinterpretation | connected_with_known_gaps | color-attachment-feedback | not_run |
| 10 | native-integer-nearest-image-sampling | connected | — | not_run |
| 11 | native-packed10-coherent-numeric-shader-views | connected_with_known_gaps | color-attachment-feedback | not_run |
| 12 | native-graphics-device-settings | connected | — | not_run |
| 13 | native-gpu-debug-markers | connected | — | not_run |
| 14 | native-signed-packed-sampled-textures | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 15 | native-packed10-storage | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 16 | native-signed-packed-integer-color | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 17 | native-signed-packed-backing-conversion | connected_with_known_gaps | color-attachment-feedback | not_run |
| 18 | native-packed10-disjoint-shader-views | connected_with_known_gaps | color-attachment-feedback | not_run |
| 19 | common-sysmodule-id-validation | connected | — | not_run |
| 20 | common-game-path-discovery | connected | — | not_run |
| 21 | common-kernel-network-time | connected | — | not_run |
| 22 | common-amdgpu-format-tables | connected | — | not_run |
| 23 | native-shared-color-attachment-layers | connected_with_known_gaps | color-attachment-feedback | not_run |
| 24 | native-cp-buffer-copy | connected | — | not_run |
| 25 | native-packed-float-bit-transfers | connected | — | not_run |
| 26 | common-original-videoout-cadence | connected | — | partial_gpu_verified |
| 27 | common-windows-mapping-protections | connected | — | not_run |
| 28 | common-gcn-arithmetic-and-subgroup-memory-ordering | connected | — | not_run |
| 29 | immutable-reference-gpu-completion-capture | connected | — | not_run |
| 30 | native-bc-volume-and-extent | connected | — | not_run |
| 31 | desktop-core-runtime-identity | connected | — | not_run |
| 32 | amd-layout | connected | — | not_run |
| 33 | guest-tiling-transfers | connected | — | not_run |
| 34 | guest-multisample | connected_with_known_gaps | packed19-msaa-attachment | not_run |
| 35 | native-render-state-and-rasterizer-discard | connected_with_known_gaps | polygon-point-fill, mixed-blend-constants | not_run |
| 36 | native-compute-wave-size | connected | — | not_run |
| 37 | guest-hardware-clock | connected_with_known_gaps | other-vendor-clock | not_run |
| 38 | guest-surface-metadata | connected_with_known_gaps | depth-color-shared-storage | not_run |
| 39 | host-fsr-and-post-process | connected | — | not_run |
| 40 | native-screenshot-capture | connected | — | not_run |
| 41 | native-rgb96-storage | connected | — | not_run |
| 42 | native-imgui-display-and-lifetime | connected | — | not_run |
| 43 | common-cpu-and-mp3 | connected | — | not_run |
| 44 | native-big-picture-launcher | connected | — | not_run |
| 45 | native-pipeline-preload | connected | — | not_run |
| 46 | native-packed16-images | connected_with_known_gaps | color-attachment-feedback, packed19-msaa-attachment | not_run |
| 47 | native-shader-debug-and-reload | connected | — | not_run |
| 48 | native-buffer-extents-and-index-bounds | connected | — | not_run |
| 49 | native-dma-page-table-growth | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 50 | native-depth-stencil-shader-plane-access | connected_with_known_gaps | depth-color-shared-storage | not_run |
| 51 | native-depth-shader-subresource-access | connected_with_known_gaps | color-attachment-feedback, depth-color-shared-storage | partial_gpu_verified |
| 52 | native-independent-buffer-image-versions | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 53 | native-independent-formatted-buffer-sync | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 54 | native-formatted-metadata-arena-synthesis | connected | — | not_run |
| 55 | native-independent-r32-mip-chains | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 56 | native-independent-r32-array-layers | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 57 | native-independent-r32-vector-formats | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 58 | native-independent-r32-volume-slices | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 59 | native-independent-rgb32-native-expansion | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 60 | native-independent-rgba8-normalized-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 61 | native-independent-rg16-rgba16-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 62 | native-independent-formatted-buffer-larger-extent | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 63 | native-independent-formatted-buffer-short-views | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 64 | native-independent-small-integer-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 65 | native-independent-subdword-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 66 | native-independent-srgb-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 67 | native-independent-packed10-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 68 | native-independent-packed16-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 69 | native-independent-r11-packed-float-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 70 | native-independent-logical-partial-buffer-writes | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 71 | native-mixed-page-buffer-ownership | connected | — | partial_gpu_verified |
| 72 | native-exact-subdword-buffer-write-ranges | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 73 | native-actual-cpu-store-formatted-buffer-pages | connected | — | partial_gpu_verified |
| 74 | native-zero-clear-original-image-domain | connected | — | not_run |
| 75 | native-image-clear-complete-subresources-padding | connected | — | partial_gpu_verified |
| 76 | native-nonzero-wide-storage-image-clears | connected | — | partial_gpu_verified |
| 77 | native-nonzero-typed-narrow-image-clears | connected | — | partial_gpu_verified |
| 78 | native-raw-rgba8-converted-image-clears | connected | — | not_run |
| 79 | native-packed16-sampled-volumes | connected | — | not_run |
| 80 | native-packed16-nonzero-image-clears | connected | — | not_run |
| 81 | native-packed10-converted-image-clears | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 82 | native-srgb-image-clears | connected | — | partial_gpu_verified |
| 83 | native-readonly-image-clear-promotion | connected | — | not_run |
| 84 | native-compact-linear-image-download | connected | — | not_run |
| 85 | native-queued-independent-image-downloads | connected | — | not_run |
| 86 | native-priority-retirement-drain | connected | — | not_run |
| 87 | native-priority-linear-pressure-commit | connected | — | not_run |
| 88 | native-rgb32-and-wide-readonly-clears | connected | — | not_run |
| 89 | native-expanded-packed-image-clears | connected | — | not_run |
| 90 | native-packed16-sampled-one-dimensional | connected | — | not_run |
| 91 | native-priority-inflight-backing-generation | connected | — | not_run |
| 92 | native-nonblocking-linear-pressure-source-residency | connected | — | partial_gpu_verified |
| 93 | native-queued-linear-color-attachment-domains | connected | — | not_run |
| 94 | native-compact-exact-expanded-packed-downloads | connected | — | not_run |
| 95 | native-source-residency-format-and-sample-admission | connected | — | not_run |
| 96 | native-image-source-gpu-modification-selection | connected | — | not_run |
| 97 | native-r32-float-multisample-image-clears | connected | — | not_run |
| 98 | native-image-clear-physical-channel-swizzles | connected | — | not_run |
| 99 | native-exact-expanded-pressure-source-residency | connected | — | not_run |
| 100 | native-multisample-storage-uav-image-clears | connected | — | not_run |
| 101 | native-exported-vs-point-size-geometry | connected_with_known_gaps | polygon-point-fill | not_run |
| 102 | native-sized-point-emulated-clip-forwarding | connected | — | not_run |
| 103 | native-sized-point-render-target-layer-forwarding | connected | — | not_run |
| 104 | native-sized-point-dynamic-viewport-extents | connected | — | not_run |
| 105 | native-guest-geometry-point-expansion | connected_with_known_gaps | polygon-point-fill, point-capture-stage-domain | not_run |
| 106 | native-geometry-cache-borrowed-copy-span-elimination | connected | — | not_run |
| 107 | native-point-misc-valid-varying-slot | connected | — | not_run |
| 108 | native-late-per-viewport-homogeneous-conversion | connected | — | not_run |
| 109 | native-point-window-transform-and-viewport-edge-footprint | connected_with_known_gaps | polygon-point-fill | not_run |
| 110 | native-point-parameter-liveness-and-sparse-clip-interface | connected | — | not_run |
| 111 | native-gcn-point-geometry-live-output-budget | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 112 | native-acyclic-point-gs-emission-bound | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 113 | native-unit-point-gs-hardware-emission | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 114 | native-point-gs-consumer-component-widths | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 115 | native-point-gs-sparse-component-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 116 | native-point-gs-aliased-fragment-input-declarations | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 117 | native-point-gs-qualified-parameter-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 118 | native-clip-disabled-viewport-transforms | connected | — | not_run |
| 119 | native-point-center-private-distances | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 120 | native-linked-clip-input-signatures | connected | — | not_run |
| 121 | native-point-dense-distance-interface | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 122 | native-unit-point-dense-distance-outputs | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 123 | native-point-vs-qualified-parameter-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 124 | native-point-cross-parameter-varying-packing | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 125 | native-point-ordered-capture-producer | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 126 | native-point-captured-fragment-attribute-fetch | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 127 | native-point-capture-replay-submission | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 128 | native-placed-color-transfer-initialization | connected_with_known_gaps | packed19-msaa-attachment | not_run |
| 129 | native-present-modes | connected | — | not_run |

## Prepared checks (independent of remaining code changes)

Descriptions retain prior evidence wording; this integration session ran no test suite or game.

### 1. common-pm4-completion-and-backing-writes

Split physical/VMA ranges, exact map end, unsupported/gapped/empty backing, arithmetic overflow and physical alias ordering fixtures prepared but not compiled/run.
`native-port/backing_write_plan_test.cpp`

### 2. native-texture-binding-lifetime

The existing exact tiled/mip/layer GPU texture fixture additionally rejects invalidation after the actual GPU fence has completed while either shared reference of a binding lease remains, then permits it after the final release. New full-core image/depth alias modes also exercise descriptor bindings across ownership and attachment uploads. Compile/link only; no execution.
`src/video_core/renderer_d3d12/guest_texture_test.cpp`

### 3. native-image-depth-prewrite-snapshots

Four additional full-core GCN modes, readonly-depth-image and mixed-depth-image with optional -owned, issue real vertex-stage R32 float image stores before Z32 depth tests. They use 32x32 aligned image/depth backing, 8x8 coverage, optional stencil increments, three warm CPU edits, consumed HTILE, exact store/attachment/color/padding oracles and rejection of competing depth writes/clears. Compile/link only; none executed.
`native-port/gcn_attachment_alias_test.cpp`

### 4. native-readonly-depth-buffer-prewrite-snapshots

The full-core GCN attachment fixture's readonly-depth-buffer and mixed-depth-buffer modes, plus -owned variants, prepare Z16/Z32, one/four samples, optional stencil, either plane's buffer writer, full/partial buffer descriptors, both read-only hints, three warm passes with CPU edits and consumed HTILE. Mixed cases additionally write depth or increment stencil while the other plane is buffer-written. Vertex stores precede attachment reads and write identical zero DWORDs. Independent expected bytes include color pass/fail results, attachment-plane results, exact shader stores and all remaining allocation/padding. Target-plane writes/clears and a fresh pending HTILE allocation overlapping the depth writer must reject before guest writes. Compile/link only; no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 5. native-gpu-buffer-image-prewrite-snapshots

The two existing full/partial sampled-image/buffer-writer fixtures additionally require a default-heap GPU tiler input with no CPU image snapshot. Warm passes include a GPU-to-CPU upload transition sampled through a separate output buffer, exact full guest allocation/padding comparison, a CPU edit and restoration of the GPU snapshot path. The remaining opposite-writer and two-writer rejection cases are retained. Compiled only; no cases executed.
`src/video_core/renderer_d3d12/guest_image_compute_test.cpp`

### 6. native-anisotropic-manual-storage-filtering

Thirty-six actual GCN cases prepare all four limits, both axes, point/linear, zero/degenerate/negative/skew gradients, a three-tap ratio, mip interpolation, sampler/resource LOD clamps, normalized/unnormalized coordinates, wrap/mirror addressing, min/max reduction, explicit LOD/LZ bias and bias cancellation beyond native sampler limits. Separate read-only outputs and seven cube cases cover shared/read-only faces, a second cube, edge clamping and mip interpolation. An independent double-precision CPU footprint/mip/tap oracle compares output within 1e-5 while all other allocation/padding bytes remain exact. Cache cases prepare changed/restored tap limits plus manual-to-hardware-to-new-manual transition and restoration. Compiled only; none executed.
`native-port/gcn_frontend_test.cpp`

### 7. native-buffer-image-prewrite-snapshots

Five metadata-to-DXIL/GPU cases prepare both directions, full/partial tiled overlap, a preceding GPU buffer writer, three warm submissions, a CPU edit, exact full allocation/padding, retirement of a preexisting sampled-cache witness and rejection of two simultaneous writers before execution. Compilation only; no cases executed.
`src/video_core/renderer_d3d12/guest_image_compute_test.cpp`, `native-port/buffer_reader_image_writer.comp`, `native-port/buffer_reader_image_writer_test.cpp`

### 8. native-unit-anisotropy-storage-filtering

Four actual GCN gradient cases prepare point and linear one-to-one anisotropy, zero/nonzero gradients, reflected shared resources, DXIL compilation and existing contents/padding/cache checks. Compilation only; no case executed.
`native-port/gcn_frontend_test.cpp`

### 9. native-r32-rgba8-shader-format-reinterpretation

120 additional native/reference classification cases prepare both format directions and external stage hints. Thirty actual GCN numeric pairs prepare shared/separate/restored resources, both stage representations/cache identities and DXIL compilation. Seven coherent GPU emitter modes prepare before/after reads, three warm mip/layer passes, reversed binding order, exact R32 float bits including a signaling-NaN word and full guest/output padding. Compilation/linking only; no fixture executed.
`native-port/image_access_classification_test.cpp`, `native-port/packed10_image_test.cpp`, `native-port/gcn_frontend_test.cpp`, `native-port/writable_image_alias_test.cpp`

### 10. native-integer-nearest-image-sampling

Eight GCN graphics cases prepare Uint/Sint sampling in ordinary R32 and packed10 formats, each with shared and separate stage resources; reflection, typed-load sampler classification and both DXIL stages are checked. Sixteen packed10 numeric cross-stage load/store pairs additionally prepare shared/separate/restored cache variants. These fixtures are compiled only and have not executed.
`native-port/gcn_frontend_test.cpp`

### 11. native-packed10-coherent-numeric-shader-views

Thirty-six additional resource-classification cases cover all read/write numeric pairs, native/reference isolation and signed reader hints for unsigned writers. Four actual SPIR-V/DXIL/GPU fixture modes prepare mixed reads before/after writes, opposing signed/unsigned writer types, three warm passes over different mips/layers, reversed descriptor order and full physical/output padding. Core/checks and refreshed full-core frontend/cache/reload/attachment fixtures compiled only; none executed.
`native-port/image_access_classification_test.cpp`, `native-port/packed10_image_test.cpp`

### 12. native-graphics-device-settings

Three native hardware fixture modes prepare default and configured core/sync/GPU/DRED device creation, explicit selected-adapter LUID identity, unavailable-index failure without fallback, actual GPU buffer-copy contents and long/color marker scopes spanning submissions. Fixture compilation/linking passed; no mode executed. Global/game/legacy persistence acceptance, actual device-removal report and original differential diagnostic review remain pending.
`native-port/device_configuration_test.cpp`

### 13. native-gpu-debug-markers

The new real native device fixture prepares nested host/guest scopes, a 1,400-character label, an actual ARGB color marker, GPU buffer copy, flush/finish/reopen/pop sequencing and debug-error checks. Compilation/linking passed; execution and native tool capture decoding remain unrun.
`native-port/device_configuration_test.cpp`

### 14. native-signed-packed-sampled-textures

Integration fixture reads actual expanded GPU texture buffers and validates signed 10-bit values, alpha endpoints, 1D/2D/3D mips/layers, distinct MSAA samples, cold/warm/changed source, typed SRV creation, retained original bytes/padding and debug messages. Multisample values are read through an actual GPU transfer buffer instead of unsupported multisample texture-to-buffer copying. Compile/link only; actual GCN sample/filter/gather/LOD/swizzle and adapter equivalence remain unrun.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`

### 15. native-packed10-storage

Twenty actual emitter/DXIL/native-runtime cases cover four numeric variants in tiled, linear, MSAA, 1D and 3D modes. They validate initial typed reads, normalized endpoints/rounding, signed integer endpoints, store-load-store sequencing, cold/warm reuse, every touched mip/layer/sample word, untouched texels/padding, integer bit patterns and debug messages. Existing unsigned typeless-clear contents/view-reuse checks remain unchanged. Compilation/linking only; no case has executed.
`native-port/packed10_image_test.cpp`, `native-port/typeless_texture_test.cpp`, `native-port/writable_image_alias_test.cpp`

### 16. native-signed-packed-integer-color

An additional signed-color mode of the existing full-core GCN fixture emits actual negative/positive signed endpoints and two-bit alpha to three simultaneous MRT array layers, testing one/four samples, original CB-slot permutations, cold/warm real GuestRasterizer::Draw, eager readback and all untouched bytes. The original unsigned fixture mode remains intact. Fixture compilation/linking succeeded; it has not executed.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 17. native-signed-packed-backing-conversion

Thirty-two additional packed10 modes of the existing real GPU/PageWatch fixture cover signed-only and mixed signed/unsigned transitions, linear/tiled, one/two samples and direct 1D/3D shapes. Six signed or nine mixed transitions include expanded reinterpretation and return to writable storage. Prepared assertions cover original negative-minimum words, actual expanded or packed single-sample GPU values, all array/mip/sample/depth bytes, untouched padding, zero intermediate ownership resolves or CPU faults, persistent-resource accounting after collection, final CPU visibility and debug messages. Existing sixteen packed16 cases remain intact. Compilation/linking passed; none executed.
`native-port/packed_backing_test.cpp`

### 18. native-packed10-disjoint-shader-views

Eight actual emitter/DXIL/runtime cases cover Unorm/Snorm/Uint/Sint sampled interpretations with signed writes, disjoint array layers or disjoint mips, intentionally writer-first image metadata, three warm dispatches, sampled numeric values, written packed words and all untouched texture/output bytes. Fixture compilation/linking passed; no case executed. Existing alias fixtures remain enabled.
`native-port/packed10_image_test.cpp`

### 19. common-sysmodule-id-validation

No separate prepared check recorded in this group; final acceptance not run.

### 20. common-game-path-discovery

No separate prepared check recorded in this group; final acceptance not run.

### 21. common-kernel-network-time

No separate prepared check recorded in this group; final acceptance not run.

### 22. common-amdgpu-format-tables

No separate prepared check recorded in this group; final acceptance not run.

### 23. native-shared-color-attachment-layers

Existing graphics-resource integration fixture shared-layers uses real MRT pixel exports, three differently ordered layer views, one/four samples, cold/warm cache identity, exact tiled contents/padding and required debug-layer messages. New full-core gcn_attachment_alias additionally invokes actual Liverpool registers, original GCN frontend, GuestRasterizer::Draw and eager guest readback for shared targets. Older graphics-resource branches remain intact. Compile/link only.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 24. native-cp-buffer-copy

`native-port/copy_dirty_ranges_test.cpp`, `native-port/guest_buffer_access_test.cpp`

### 25. native-packed-float-bit-transfers

Core and full-core fixture compiled/linked only. Prepared exact NaN/finite/shared-exponent physical words across linear/macro layouts, three layers/mips, one/four samples, warm uploads, CPU changes, sample-zero broadcast, untouched padding and retained backings. Active readback bytes are poisoned before comparison.
`native-port/packed_float_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 26. common-original-videoout-cadence

Core, standalone targets and desktop compiled only. Prepared presenter fixtures cover default coupled policy and lifetime-stable explicit independent mode; desktop fixtures reject altered capture controls.
`src/video_core/renderer_d3d12/guest_presenter_test.cpp`

### 27. common-windows-mapping-protections

Full-core fixture compiled and linked only. It prepares actual 64/16-KiB mapped splits and checks neighbouring PAGE_READONLY, PAGE_NOACCESS and PAGE_GUARD protections, exact physical contents/padding, remapped middle permissions and alias lookup.
`native-port/address_space_protections_test.cpp`

### 28. common-gcn-arithmetic-and-subgroup-memory-ordering

Full-core actual GCN fixture compiled/linked only, preparing dynamic positive/negative zero, infinities and multiple NaNs, finite arithmetic, 12 comparison ballots, dynamic neighbouring LDS visibility, 32/64-lane modes, cold/warm/CPU changes, metadata restoration and untouched guest padding.
`native-port/gcn_arithmetic_test.cpp`

### 29. immutable-reference-gpu-completion-capture

Original Release, explicit observer DLL, native core, actual-fence capture fixture and desktop compiled/linked only. Exact Flip RVA/extent/call target, preserved Request register/index offset, machine-code fingerprint and artifact hashes were derived offline from the actual original PE/MAP/PDB. Prepared delayed DIRECT fence interval, old/pending frame exclusion, duplicate/failure and desktop integrity fixtures have not run.
`native-port/guest_frame_trace_test.cpp`, `tools/vulkan_reference/completion_layer.cpp`, `tools/vulkan_reference/completion_layer.def`, `tools/vulkan_reference/completion-layer.json`, `tools/vulkan_reference/CMakeLists.txt`, `tools/vulkan_reference/resolve_completion_symbol.py`

### 30. native-bc-volume-and-extent

Compiled-only actual hardware fixture compares Texture3D Load and trilinear/mip SampleLevel with independent native BC array decoding across all BC families, sRGB/signed/float variants, linear/macro/thick layouts, partial blocks, five mips/depth slices and cold/warm/CPU-change. Its HLSL compiled offline with pinned DXC and strict warnings.
`native-port/bc_volume_test.cpp`

### 31. desktop-core-runtime-identity

Desktop compiled with zero warnings/errors. Prepared catalogue/profile fixtures cover both backends, legacy rollback, corruption, identity mismatches, untracked runtime, preserved graphics/source profile, per-version save copy/reuse/shared-save preservation and diagnostic cleanup. No fixture executed. At the user's explicit request one experimental current snapshot was packaged into the existing installed catalogue, with 76 hash-verified files, static PE dependency checks, matching PDB/MAP, dirty source patch and 20 copied save files; installed source saves/settings hashes were preserved.

### 32. amd-layout

No separate prepared check recorded in this group; final acceptance not run.

### 33. guest-tiling-transfers

No separate prepared check recorded in this group; final acceptance not run.

### 34. guest-multisample

No separate prepared check recorded in this group; final acceptance not run.

### 35. native-render-state-and-rasterizer-discard

Compiled-only GPU fixtures cover RGB/alpha MIN/MAX zero/one combinations, UNORM/SRGB, actual vertex UAV effects under discard and both-face culling, real bounded 16/32-bit IBV reads, cold/warm contents and untouched padding. Stencil preparation includes conflicting and unreachable ReplaceOp selection and post-clear write suppression.
`native-port/graphics_contract_test.cpp`, `native-port/stencil_lowering_test.cpp`, `native-port/gcn_winding_test.cpp`, `src/video_core/renderer_d3d12/shader_pipeline_test.cpp`

### 36. native-compute-wave-size

A compile-only full-core GCN fixture covers 128 threads, full-mask MBCNT, upper-half ballot, prefix count, fixed lane63/47, dynamic uniform lane47, divergent EXEC/firstlane, cold/warm dispatch, metadata restore, guest padding and D3D12 errors. The fixture has not executed.
`native-port/wave64_test.cpp`

### 37. guest-hardware-clock

No separate prepared check recorded in this group; final acceptance not run.

### 38. guest-surface-metadata

`src/video_core/renderer_d3d12/guest_cmask_test.cpp`

### 39. host-fsr-and-post-process

No separate prepared check recorded in this group; final acceptance not run.

### 40. native-screenshot-capture

No separate prepared check recorded in this group; final acceptance not run.

### 41. native-rgb96-storage

No separate prepared check recorded in this group; final acceptance not run.

### 42. native-imgui-display-and-lifetime

No separate prepared check recorded in this group; final acceptance not run.

### 43. common-cpu-and-mp3

No separate prepared check recorded in this group; final acceptance not run.

### 44. native-big-picture-launcher

No separate prepared check recorded in this group; final acceptance not run.

### 45. native-pipeline-preload

No separate prepared check recorded in this group; final acceptance not run.

### 46. native-packed16-images

No separate prepared check recorded in this group; final acceptance not run.

### 47. native-shader-debug-and-reload

No separate prepared check recorded in this group; final acceptance not run.

### 48. native-buffer-extents-and-index-bounds

No separate prepared check recorded in this group; final acceptance not run.

### 49. native-dma-page-table-growth

Resident-first-command, GPU-only B preservation, dirty CPU tail, late full64KiB/partial4KiB publication, initialized tiny-tail exclusion, selected-resource identity and SRV/UAV rejection controls are authored in native-port/dma_shader_test.cpp. Protected physical-backing and host-temporary rejection controls are in native-port/guest_buffer_access_test.cpp. All C++ typechecked only. Smaller late sharps, late arena replacement and SRV-only auxiliary unification remain production gaps; shader conversion/GPU acceptance not run.

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 50. native-depth-stencil-shader-plane-access

The plane-feedback mode compiles/linkably prepares real HLSL depth and stencil loads into RGBA32F while the other attachment plane writes. Twelve cases combine three warm versions with and without a beginning clear of the sampled plane. Independent full guest depth/stencil/padding and sampled color expectations include the cleared version; a sampled writable-plane variant must reject before recording. No tests or GPU execution.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`

### 51. native-depth-shader-subresource-access

Twelve depth-view-ranges cases prepare independent same-mip/different-layer and same-layer/different-mip shader loads versus attachment writes over three warm versions. The old descriptor is overwritten before draw. Exact complete guest contents/padding, sampled RGBA32F values and unrelated native states are checked; changing the DSV to the sampled subresource must reject before recording. Compile/link only, not executed.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`

### 52. native-independent-buffer-image-versions

The full-core separate-buffer-image-owned GCN fixture compiles/links real simultaneous vertex buffer zero and image quarter stores, then pixel image loads. Twelve cases combine four warm bit patterns, including negative float bits, with R32 float/uint/sint views and compare the entire 64-KiB physical backing/padding. Pixel export supplies float-one alpha independently of integer image alpha. Actual protected CPU reads expose the buffer without destroying the image quarter; CPU writes and subsequent exact-base buffer writes invalidate it. The guest mapping and callbacks use two physical views. Fault-kind cases distinguish real reads and injected writes. Compile/link only.
`native-port/page_watch_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 53. native-independent-formatted-buffer-sync

The real GCN independent-version fixture includes BUFFER_LOAD_FORMAT_X before CPU reconciliation (image quarter must enter the GPU buffer, and later CPU visibility sees quarter) and after CPU reconciliation (small formatted reads see the raw zero CPU bytes while image loads still see quarter). Twelve float/uint/sint cases include negative bit patterns and independent complete physical allocation/padding oracles. All compile/link only, no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 54. native-formatted-metadata-arena-synthesis

The owned readonly-depth-buffer GCN fixture adds three warm HTILE cases: real depth metadata registration, raw GPU-modified 4-KiB arena writer, BUFFER_LOAD_FORMAT_X through RGBA8 UNORM, protected CPU read of 0x0f across the arena, then CPU zero write and small formatted stream. The formatted-cmask-owned GCN mode adds twelve cases over three warm versions and 1/2/4/8 samples, with real fast-clear-enabled color metadata registration. Large clean 32-KiB formatted arenas synthesize each sample pattern; small 4-KiB clean streams read physical 0xff instead. Exact complete 64-KiB contents/padding and per-sample color expectations prove arena-only writes. Compiled/linked only, not executed.
`native-port/gcn_attachment_alias_test.cpp`

### 55. native-independent-r32-mip-chains

The full-core GCN independent-version fixture now has twenty-four basic cases over two physical chains, three numeric views and four warm bit patterns. The two-mip chain writes/reads mip one at its actual physical byte offset using original sharp base_level and raw buffer offen instructions with distinct shader hashes. Twelve additional cases produce half in image mip zero and quarter in mip one, then require formatted synchronization and a real CPU buffer fault to expose both values while every other byte/padding remains exact. Original small versus large clean formatted behavior, per-mip sampling, CPU writes and exact-base GPU invalidation are prepared. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 56. native-independent-r32-array-layers

The actual full-core GCN fixture prepares sixty basic cases: one/two layers with one/two mip chains, a six-face cube base level, FLOAT/UINT/SINT and four warm values including negative bits. Array DA instructions write/read the last layer or cube face five, raw-buffer and formatted offen instructions use its actual physical mip/layer offset, and changed programs have distinct hashes. Forty-eight additional cases diverge layer-zero/mip-zero or cube face zero half from the selected layer/mip or cube face five quarter, require formatted transfer and a protected CPU buffer read to expose both, then sample the first layer/face separately. Every case independently checks the entire 64-KiB physical allocation/padding, including untouched cube faces. Compiled/linked only, not executed.
`native-port/gcn_attachment_alias_test.cpp`

### 57. native-independent-r32-vector-formats

The actual GCN fixture prepares 180 basic and 144 divergent mip/layer/face cases over three data formats, three numeric views, one/two layers/mips and six-face cubes, with four warm patterns. RG32 rectangular chains and RGBA32 reduced dimensions fit the same complete allocation. Every channel is initialized independently from the warm bits; formatted CPU visibility expects absent image-store channels zero while raw-store visibility preserves prior channels. Separate first/selected subresource results, all untouched texels/faces, full 64-KiB bytes/padding, clean streaming threshold, CPU writes and exact-base GPU invalidation remain independent oracles. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 58. native-independent-r32-volume-slices

The actual full-core GCN fixture adds seventy-two basic and seventy-two divergent volume cases over three data formats/numeric views and four warm patterns, bringing the fixture to 252 basic and 216 divergent cases. Base-only volumes have two Z slices; two-mip volumes shrink from four to two Z slices, write/read Z one at its actual guest offset, then diverge mip-zero/Z-zero half from selected mip/Z-one quarter. Every mip-specific Z slice/channel is initialized and expected independently. Complete 64-KiB bytes/padding, untouched Z slices, formatted full-volume transfer, clean streaming, CPU write and GPU exact-base invalidation are prepared. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 59. native-independent-rgb32-native-expansion

The actual full-core GCN fixture adds thirty-six basic and twenty-four divergent RGB32 cases over three numeric views and four warm values, one/two array layers and a two-slice volume base mip, totaling 288 basic/240 divergent cases. Every logical RGB channel is initialized. Complete physical bytes/padding compare exact twelve-byte output rather than the native sixteen-byte stride, including separate first/selected slices, raw first-DWORD versus full image-store component results and real protected CPU visibility after formatted transfer. Larger RGB cube/mip fixture shapes remain outside the fixed allocation. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 60. native-independent-rgba8-normalized-versions

The actual GCN fixture adds 56 basic/48 divergent normalized cases across two numeric views, four warm values and existing mip/layer/cube/volume shapes, totaling 344 basic/288 divergent cases. Warm values use independently quantized replicated bytes, including negative SNORM. Physical quarter/half/one bytes are 64/128/255 for UNORM and 32/64/127 for SNORM; the expected SNORM half export is 129. BUFFER_LOAD_FORMAT_X uses matching RGBA8 normalized sharp views. Full image/arena/CPU bytes, absent-channel zeroing, untouched subresources, complete 64-KiB padding, real faults and exact-base GPU invalidation remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 61. native-independent-rg16-rgba16-versions

The actual GCN fixture adds 168 basic/144 divergent cases across two data formats, three numeric views and four warm values over existing mip/layer/cube/volume shapes, totaling 512 basic/432 divergent cases. Warm half values are exact IEEE encodings for zero/one/positive-negative half; normalized values use independent sixteen-bit quantization including negative SNORM. Quarter/half/one are 3400/3800/3c00 FLOAT, 16384/32768/65535 UNORM and 8192/16384/32767 SNORM. Every channel, full physical/padding bytes, complete mip/layer/face/Z transfer, real protected CPU visibility, small streaming and exact-base GPU invalidation remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 62. native-independent-formatted-buffer-larger-extent

Every one of the 512 basic full-core GCN cases adds two larger sharp observations across all implemented data/numeric formats and mip/layer/cube/volume shapes. Extended views have at least twenty KiB and at least four KiB beyond the physical image. After CPU reconciliation, the large clean view must read the image version even when its image is below the small streaming threshold, while physical CPU buffer bytes remain separate. Before CPU reconciliation, the larger GPU-modified view must transfer the whole image and expose its exact component encodings to a real CPU fault. Whole 64-KiB expected bytes/padding include the untouched extended tail. Existing 432 divergent-subresource cases remain. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 63. native-independent-formatted-buffer-short-views

Every one of 512 basic full-core GCN cases adds two short-view observations alongside existing full/long views. The short sharp ends immediately after the selected texel, preserves matching format stride/record count and is smaller than the image. After CPU reconciliation it expects zero through a <=16-KiB clean stream, or image quarter through a larger clean arena, while image reads retain quarter. Before CPU reconciliation a GPU-modified short view requires complete image retile and exact protected CPU quarter/channel results. Whole 64-KiB physical/padding expectations cover all bytes beyond the view and untouched subresources. Existing 432 divergent mip/layer/face/Z cases remain. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 64. native-independent-small-integer-image-versions

The actual full-core GCN fixture adds 168 basic/144 divergent integer cases across three data formats, UINT/SINT and four warm values, totaling 680 basic/576 divergent cases with full/short/long formatted observations. Warm integers include negative sign-extension cases. Actual byte/halfword quarter/half/one stores, CPU edits, scaled numeric exports, every channel and full physical/padding/untouched subresources are independent expectations. The color oracle now explicitly clears G/B before export for every image-load case; this fixes earlier multi-channel warm-value exports while full physical comparisons continue to verify all guest channels. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 65. native-independent-subdword-image-versions

The actual GCN fixture adds 364 basic/312 divergent cases, totaling 1044 basic/888 divergent across all implemented data/numeric and mip/layer/cube/volume shapes. Geometry derives from actual format bits to keep complete image chains page-backed. Warm initialization and first-subresource image expectations write only each texel's actual one/two bytes, avoiding adjacent-texel/padding overwrites; raw/CPU DWORD operations still update four bytes. Negative normalized/integer and exact half values, scaled integer reads, full/short/long views, every neighboring texel and complete 64-KiB physical/padding bytes remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 66. native-independent-srgb-image-versions

The full-core GCN fixture adds 28 basic/24 divergent SRGB cases, totaling 1072 basic/912 divergent. Warm images use an independent piecewise SRGB decode oracle. Quarter/half UNORM image stores expect encoded bytes 64/128, separately sampled red 13/55, matching formatted-buffer red 64/128 and exact protected CPU encodings. Full/short/long views, all components, complete allocation/padding and untouched mip/layer/face/Z expectations remain. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 67. native-independent-packed10-image-versions

The full-core GCN fixture adds 119 basic/102 divergent cases across four packed numeric formats and all implemented shapes, totaling 1191 basic/1014 divergent. Actual ten-bit quarter/half/one literals and full RGB/alpha warm packing use independent encodings and CVT/MUL integer read scaling. Additional SNORM negative-minimum warm bytes use RGB -512 and alpha -2, distinguishing retained raw encodings from their decoded saturation. Full/short/long formatted views, actual protected CPU accesses and complete physical/padding/subresources are prepared. Core/checks/fixture and exact runtime SM5.1 host shader compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 68. native-independent-packed16-image-versions

The full-core GCN fixture adds 80 basic/64 divergent packed16 cases, totaling 1271 basic/1078 divergent before the later packed-float extension. Independent bit packing, component widths/remapped positions and normalized warm values expect quarter red 66/68 and half red 132/136 rather than unquantized values. Four warm values, full/short/long buffers, all mip/layer/face contents, neighbor texels, CPU faults and complete allocation/padding are prepared. Core/checks/fixture compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 69. native-independent-r11-packed-float-image-versions

The full-core fixture adds 56 basic/48 divergent packed-float cases across both raw descriptor orders, totaling 1327 basic/1126 divergent. Independent unsigned-float exponent encodings cover quarter/half/one and warm zero/one/half/negative-clamped-zero, all RGB components and both red positions. Actual formatted FLOAT loads, full/short/long views, all mip/layer/face/Z subresources, protected CPU values and full allocation/padding remain prepared. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 70. native-independent-logical-partial-buffer-writes

All 1327 basic/1126 divergent GCN cases now repeat with full, shortest valid DWORD-store and extended raw writer sharps, totaling 3981 basic/3378 divergent. Matching formatted full/short/long observations retain their original view extents independently of writer bounds. Each case additionally writes through an actual DWORD-shifted original base and requires its own CPU result while the preceding image quarter survives; a subsequent exact-base write requires image reload to zero. Full physical bytes, trailing ranges, neighbors and untouched subresources remain independent expectations. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 71. native-mixed-page-buffer-ownership

The page-watch fixture prepares interior ownership splitting, unchanged outside read-only modes, retained range lookup, conversion back to read-only, partial release creating a writable hole, complete mixed-gap reacquisition, full original permission restoration, rejection of an originally read-only untracked page and rejection after watched OS permissions change with no partial mutation. The 3981 basic/3378 divergent full-core GCN partial/shifted cases compile against the same production API. Compiled only; no execution.
`native-port/page_watch_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 72. native-exact-subdword-buffer-write-ranges

Every one of 3981 basic/3378 divergent prepared GCN cases additionally issues real BUFFER_STORE_BYTE at original base image+selectedOffset+1/size1 and naturally aligned BUFFER_STORE_SHORT at +2/size2. Before these writes a formatted image copy intentionally makes resident GPU quarter bytes differ from clean physical buffer zero. Protected byte reads and full-allocation comparison require exact 0x35/0x7a35 results while untouched aligned DWORD bytes stay in the CPU domain and the sampled image remains quarter. Actual opcodes were checked against pinned frontend definitions. Core/checks/fixture compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 73. native-actual-cpu-store-formatted-buffer-pages

Byte-component GCN image/buffer cases verify resident quarter through a large raw BUFFER_LOAD_DWORD plus byte mask/UINT conversion, perform a volatile same-zero CPU store and require raw zero before image refresh; raw reads cannot use small CPU streams or formatted synchronization. Subsequent refreshed image/formatted zero retains full physical oracles. Production GuestBufferAccess fixture fills a 16-KiB GPU-only arena, reconciles four declared GPU-written bytes on a real CPU read without a store marker, then stores an unchanged CPU byte in page two. Forced upload count is 4096 and full GPU readback retains three untouched GPU pages. A physical-alias callback additionally notifies page three during page-two upload; both CPU pages must reach this prepared view while first/last GPU pages persist. Current core/checks/all-ten full-core fixtures compile/link and static inventory/whitespace pass; no execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 74. native-zero-clear-original-image-domain

Pinned IsComputeImageClear has no separate zero CPU fill. Twelve prepared actual GCN two-buffer clear shapes use the old hash after independent R32 FLOAT buffer/image writes, require first/next sampled texels zero while warm physical buffer/padding bytes persist, then recreate the ordinary quarter scenario with an exact-base writer. Current core/checks/all-ten full-core fixtures compile/link, no non-deprecation diagnostics or whitespace errors; no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 75. native-image-clear-complete-subresources-padding

A real padded RGBA8 RTV fixture has width17/pitch32/height9, two mips and three layers while its cached view selects mip1/layer1. Full GuestImageReadback must show exact yellow on every logical texel and unchanged 0xa6 padding. GCN zero-clear shapes expand to 84 R32 FLOAT warm/extent/mip/layer/cube/volume cases, requiring image zero with unchanged separate physical buffer bytes before restoring the quarter scenario. Core/checks/attachment fixture compile/link; final all-ten refresh/audit pending, no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 76. native-nonzero-wide-storage-image-clears

All 84 R32 FLOAT GCN zero-clear shapes additionally clear to half: first/next sampled red128, physical buffer zero/warm padding, large raw DWORD-to-float resident GPU buffer zero before formatted sync, then exact-base quarter restoration. A production API GPU fixture prepares 27 data-format/numeric/dimension combinations (R32/RG32/RGBA32; UINT/SINT/FLOAT; 2D array/1D array/volume), three mips and a restricted cached view. Full readback requires exact 1.5/-0/-3.25/0.25 union bits plus original 0xa6 padding. Nine R32 warm cases additionally clear to negative zero and require exact 0x80000000. Current core/checks compile; final fixtures/audit pending, no execution.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/page_owned_gpu_test.cpp`

### 77. native-nonzero-typed-narrow-image-clears

81 prepared R8/RG8/RGBA8/R16/RG16/RGBA16 numeric/dimension combinations cover supported UNORM/SNORM/UINT/SINT/FLOAT views, arrays/1D/volumes, three mips and restricted cached views. Full GPU readback requires exact selected normalized/integer/sign/half-float/negative-zero texel encodings and unchanged 0xa6 guest padding. Latest compilation pending; no execution.
`native-port/page_owned_gpu_test.cpp`

### 78. native-raw-rgba8-converted-image-clears

Twelve additional API numeric/array/1D/volume cases require full exact RGBA8 output, unchanged 0xa6 padding, unchanged existing image resource identity/descriptor, temporary memory accounting and fence retirement. Actual GCN HLE zero/nonzero clear coverage adds 336 RGBA8 shapes to 84 R32 FLOAT shapes, requiring image observations and independent raw resident/physical buffer zero before formatted sync. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 79. native-packed16-sampled-volumes

Eight API format/tile volume cases prepare all four GPU channels, three mips/depth slices, raw-to-expanded-to-raw-to-expanded view changes, poisoned CPU upload inputs and exact original bits/padding retention. Actual GCN separate buffer/image coverage adds 96 basic and 96 divergent warm/writer/mip volume combinations (4077/3474 total). Core/checks and SM5.1 FXC /Ges /WX compile; no execution.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 80. native-packed16-nonzero-image-clears

Twelve added format/array/1D/volume API cases require explicit exact packed colors, three mips/restricted cached views, 0xa6 padding, resource identity and temporary allocation retirement. Actual GCN zero/nonzero HLE cases add 336 packed16 shape combinations using a correctly sized 16-bit formatted destination sharp (756 total clear combinations); sampled image output changes while raw resident/physical buffer stays zero before formatted sync. Core/checks and strict SM5.1 compiler pass; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 81. native-packed10-converted-image-clears

Twelve new API numeric/array/1D/volume cases prepare exact all-mip packed words/padding/identity/retirement. Twelve warm endpoint cases prepare normalized signed extremes, nearest-even alpha and unsigned/signed integer bounds, including two-bit alpha clamp. Actual GCN clear coverage adds 420 packed10 shape combinations (1176 total HLE clear combinations), with zero/half/integer image observations and unchanged resident/physical raw buffer before formatted sync. Core/checks and strict SM5.1 FXC compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 82. native-srgb-image-clears

Six typed/raw array/1D/volume API cases prepare exact encoded 137/188/0/255 RGB/alpha, three mips/full resource view, original image identity/descriptor/accounting and padding. Six warm cases prepare the transfer-curve boundary and linear half alpha (0x800d0a0a). Actual GCN adds 84 SRGB zero/nonzero HLE shapes (1260 total clear combinations), observing sampled linear half red128 with resident/physical buffer zero before formatted sync. Core/checks compile only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 83. native-readonly-image-clear-promotion

84 added read-only typed/SRGB numeric/dimension API cases require exact complete texels, padding, descriptor/resource identity and transfer retirement (234 API cases total; three added SRGB endpoint warm cases). 504 actual GCN cold read-only cache HLE cases prime sampled image/raw resident buffer before the first image writer, then require changed first/next image observations and unchanged original CPU/raw arena bits. Existing post-writer HLE coverage is 1260 combinations. The first zero-clear oracle now correctly preserves the preceding quarter color because compute does not draw into the color target. Core/checks compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 84. native-compact-linear-image-download

Twelve API cases cover R32 float, raw RGBA8, RGB32 and packed16 across restricted three-mip 2D/1D arrays and volumes. Independent compact CPU indexing requires every mip-zero texel, preserved row padding and untouched full-mip-chain tail. Real protected resident-buffer fixture prepares failed commit, changed physical image bytes with unchanged raw arena, no CPU-store upload and a subsequent same-value store requiring precisely one 4 KiB page upload. Production core/checks compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/guest_buffer_access_test.cpp`

### 85. native-queued-independent-image-downloads

New compile-only linear-download-owned actual GCN mode prepares eight R32-float single/array and base/two-mip-chain shapes with two initial CPU values. Both writers establish distinct versions; legacy OnFence/OnSubmit HLE clears require complete compact physical rows and unchanged padding/tail, unchanged raw arena, live cleared image, no repeat download after a read-only draw, and same-value CPU store transfer. No execution.
`native-port/gcn_attachment_alias_test.cpp`

### 86. native-priority-retirement-drain

GPU scheduler fixture prepares a priority map/physical-write callback retaining readback after caller release, a held memory lock released before drain, injected callback failure followed by successful retirement, and destructor draining a queued write. Compile-only; no execution.
`native-port/scheduler_lifetime_test.cpp`

### 87. native-priority-linear-pressure-commit

Protected production buffer fixture queues an actual GPU readback while arena/CPU are equal, holds the execution mutex to delay worker commit, performs a real same-value protected store, then requires physical bytes, precise 4 KiB GPU upload and zero retained budget after drain. Compact image cases release the source fence lease before mapping. Core/checks and fixtures compile only; no execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/page_owned_gpu_test.cpp`

New invalid-I pressure fixture: native-port/invalid_image_pressure_fixture.h via page_owned_gpu_test.cpp; actual LRU reclamation, B/CPU/padding/store-marker retention, caller/callback pins and pinned valid tiled exclusion. C++ /Zs only, not executed.

### 88. native-rgb32-and-wide-readonly-clears

45 added wide read-only/RGB32 API cases cover three numeric views and restricted three-mip 2D/1D arrays/volumes, exact physical bytes/padding, original identity and transfer retirement (279 storage-clear API cases total). Native RGB32 readback independently requires alpha one across the selected mip/layer/depth footprint; nine additional read-only negative-zero cases prepare exact bits (18 total). Actual GCN RGB32 clear cases use a correctly sized 96-bit destination sharp, cover cold read-only float promotion/repeated clears and post-writer clears with unchanged raw/physical buffer. Compile-only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 89. native-expanded-packed-image-clears

20 additional read-only packed API cases include ten expanded signed-packed10/packed16-volume cases, all full mip/layer/depth texels and untouched physical padding (299 storage-clear API cases total). Twelve additional packed10 endpoint warm cases (24 total) exercise replacement/retirement of a second exact backing. Actual GCN cold read-only coverage adds packed10 and packed16 volumes, with three consecutive clear values, preserved initial raw/CPU bytes and subsequent writer/format sync (1056 cold cases total; 1368 post-writer HLE combinations after RGB32 coverage). No execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 90. native-packed16-sampled-one-dimensional

Eight new format/linear-or-tiled 1D-array GPU cases use three mip levels/layers and restricted selected mip/layer, compare every expanded component, poison CPU upload input, then require raw-to-expanded-to-raw-to-expanded identity/ownership/exact physical source and padding. Four new read-only packed16 1D clear API cases require all resource texels and preserved padding (303 API cases total). Compile-only; no execution.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/page_owned_gpu_test.cpp`

### 91. native-priority-inflight-backing-generation

Four actual GPU-copy fixtures hold an external fence unsignaled, require submitted-but-incomplete copy and retained priority budget, replace backing through register/map/unmap-remap/host mapping, signal the GPU gate, drain and require no read/resolve/write callback, unchanged replacement bytes and zero retained budget. Compile/link only; no execution.
`native-port/guest_buffer_access_test.cpp`

### 92. native-nonblocking-linear-pressure-source-residency

Actual GPU R32 3-layer/3-mip fixture clears the source, holds an external fence unsignaled, checks exact source accounting and shared lease copies, destroys the source cache/allocator/entries before signaling, then requires source-plus-readback priority budget, full compact physical contents, preserved padding/tail and zero priority budget after drain/collection. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`

New invalid-I pressure fixture: native-port/invalid_image_pressure_fixture.h via page_owned_gpu_test.cpp; actual LRU reclamation, B/CPU/padding/store-marker retention, caller/callback pins and pinned valid tiled exclusion. C++ /Zs only, not executed.

### 93. native-queued-linear-color-attachment-domains

Four actual GCN linear MRT cases use three shared array layers, two permutations/two warm runs, 56x16 logical pixels with padded pitch 64. They require one deduplicated fence download, unchanged old raw GPU bytes, complete physical allocation/padding, no stale empty-fence repeat, a second download after already-owned attachment rebinding on submit and actual same-value CPU store updating only its 4 KiB raw page. Existing storage/HLE fixtures compile against the shared ownership path. No execution.
`native-port/gcn_attachment_alias_test.cpp`

### 94. native-compact-exact-expanded-packed-downloads

Compact API coverage grows from 12 to 24 cases: signed packed10 arrays/1D/volumes and all four packed16 formats across raw 2D plus expanded 1D/volumes, with three mips and restricted selected mip/layer. Independent fixed packed-word expectations require every compact mip-zero texel, all padding/tail and source release before resolve. Existing RGB32/RGBA8/wide cases remain. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`

### 95. native-source-residency-format-and-sample-admission

All 24 compact real-GPU image cases additionally require the cache residency total to include a retained actual texture allocation and zero retired-source bytes after its copy fence/release. Existing shared source-copy/cache-destruction and sample-backing GPU fixtures compile against the unified admission path. No execution.
`native-port/page_owned_gpu_test.cpp`

### 96. native-image-source-gpu-modification-selection

Existing protected real-GPU arena fixture requires image physical download and read-only synthesis to leave GPU modification false. Actual GCN queued-image cases require buffer writer true and CPU readback false; one fresh alias case keeps quarter in physical CPU while raw arena is zero, retires the image for a buffer writer plus read-only image load, requires prewrite pixel quarter, raw zero, subsequent raw CPU download and full independent padding oracle. No execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 97. native-r32-float-multisample-image-clears

18 actual GPU API cases cover 2/4/8 samples, linear/tiled physical layouts and three descriptor usages, restricted layer one of three. Two consecutive quarter/negative-zero clears require exact R32 bits in every sample/layer, unchanged native resource/descriptor and complete physical padding. Existing sample/typed-clear/HLE fixtures compile against the ownership/backing rule. No execution.
`native-port/page_owned_gpu_test.cpp`

### 98. native-image-clear-physical-channel-swizzles

Twelve actual GPU API cases cover R32 FLOAT, RGBA32 FLOAT and RGBA8 UNORM, two nonidentity selector mappings including constants, linear/tiled layout and restricted mip/layer. Fixed independent physical words, all texels and padding, original resource identity and descriptor are required. Compile-only; no execution.
`native-port/page_owned_gpu_test.cpp`

### 99. native-exact-expanded-pressure-source-residency

Six externally gated cache-destruction GPU cases cover R32 float, signed packed10 and four expanded packed16 volumes. They require actual summed source allocations, shared lease accounting, source/cache destruction before gate signal, exact compact packed words/padding and zero pending commit bytes after drain. Existing 24 compact source-accounting cases compile against the wider lease. No execution.
`native-port/page_owned_gpu_test.cpp`

### 100. native-multisample-storage-uav-image-clears

24 GPU API cases cover R32 uint/sint/float/raw-float, 2/4/8 samples and linear/tiled physical layouts, two consecutive quarter-bit/negative-zero clears, every sample/layer and padding, restricted layer one, unchanged resource/descriptor and post-fence clear-lease release. Existing full-core HLE fixtures compile only. No execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 101. native-exported-vs-point-size-geometry

96 actual GPU GCN cases prepare exported one/four-pixel sizes at nonboundary centers, center outside XY clipping, four mixed viewport flip masks, three cull settings, cold/warm, front-face pixel and front/back stencil contents, auxiliary-stage serialization and exact full allocation/padding. Pipeline state fixtures require supported sized PointList with solid fill/no culling, while polygon point fill remains guarded. Compile-only; no execution.
`src/video_core/renderer_d3d12/shader_pipeline_test.cpp`, `native-port/gcn_winding_test.cpp`

### 102. native-sized-point-emulated-clip-forwarding

192 additional actual GPU GCN point cases cover native/emulated four-plane exports, positive/negative distances, one/four-pixel sizes, viewport flips, cull flags, front-facing pixel/stencil, cold/warm, auxiliary serialization and complete padding. All four emulated components are independently initialized in the fixture. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 103. native-sized-point-render-target-layer-forwarding

96 additional actual GPU GCN cases prepare one/four-pixel points routed to different layers of a three-layer color target, viewport flips, cull flags, clipped centers, cold/warm, untouched other layers/depth/stencil and full physical padding. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 104. native-sized-point-dynamic-viewport-extents

96 additional actual GPU GCN cases prepare one/four-pixel points routed to two viewports of different widths/heights, all four Y-sign masks, cull controls, front-face pixel/stencil, clipped centers, cold/warm, full guest allocation/padding and auxiliary serialization. Runtime offsets have compile-time assertions. The clip fixture also declares sufficient VGPR allocation for its last loaded component. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 105. native-guest-geometry-point-expansion

192 additional GPU winding cases prepare production GS EmitVertex with exported/default one-pixel size, mixed viewport flips, culling/front-face/stencil, clipped centers, cold/warm and full padding. These use real GCN VS/PS with an explicitly generated SPIR-V GS, not a guest GCN GS. State coverage prepares triangle input with point GS output and both cull flags. Existing auxiliary GS cases exercise the shared generator. Compile-only; no execution. Another 192 cases use actual guest GCN export/geometry/copy programs, coherent ESGS/GSVS ring stores, two GS emissions per invocation, independent ES/GS atomic counters, exported/default size, clipping and complete padding. Code metadata follows the actual token-directed offset and stays outside every program. Compile/link only; not executed.
`src/video_core/renderer_d3d12/shader_pipeline_test.cpp`, `native-port/gcn_winding_test.cpp`

### 106. native-geometry-cache-borrowed-copy-span-elimination

Actual guest GCN GS winding cases prepare metadata round trips requiring empty restored spans in runtime and specialization, preserved copy hash and nonempty unchanged live span, along with actual compiled GS/ring/atomic coverage. Existing archived/unarchived pipeline-reload fixtures remain available. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 107. native-point-misc-valid-varying-slot

A build-only compile-shaders path returns before hardware device creation and skips runtime metadata/content oracles. Offline production frontend, linked Mesa NIR/DXIL and DXIL signing validate ordinary point size, native/emulated clipping, layer/viewport, explicit GS and actual GCN ES/GS/copy variants. No GPU oracles, renderer/game launch or semantic acceptance are run. The separate direct VS viewport baseline currently fails NIR conversion and remains open.
`native-port/gcn_winding_test.cpp`

### 108. native-late-per-viewport-homogeneous-conversion

All ten existing VS/GS/PS chains compile offline through production frontend, linked NIR/DXIL and signing with the updated converter. Two additional 96-case GPU modes prepare late viewport exports, mixed Y signs, inverted viewport-one depth and w=2; fragment depth reaches an exact color-channel oracle. All preparation remains unexecuted. No GPU/device/game acceptance occurs in compile-shaders mode.
`native-port/gcn_winding_test.cpp`

### 109. native-point-window-transform-and-viewport-edge-footprint

Core/check builds, all ten full-core compile/link fixtures and offline signed VS/GS/PS chains are required. A 384-case mode prepares four viewport edges, size 1/4, two differently sized viewports, four Y-sign combinations, w=2, restricted normal/reversed depth, center rejection, cold/warm, front-facing stencil, ignored culling and full 64-KiB contents/padding. No hardware or runtime acceptance before full implementation boundary.
`native-port/gcn_winding_test.cpp`

### 110. native-point-parameter-liveness-and-sparse-clip-interface

Three additional 96-case real GCN modes export all 32 guest parameters: no fragment consumers, a live sparse flat parameter, and a live sparse parameter with emulated clip planes. Each guest VS performs one atomic increment, including center-clipped points, with full 64-KiB contents/padding oracles and cold/warm state. Shader binary metadata moves to word 192 in 256-word arrays to fit complete programs. Compilation, linking and offline signing are allowed; actual GPU oracles remain unexecuted.
`native-port/gcn_winding_test.cpp`

### 111. native-gcn-point-geometry-live-output-budget

Two new 96-case actual GCN ES/GS/copy modes export all 32 float4 parameters and emit two overlapping points, with zero or one live sparse fragment input. Original ES/GS atomics must increment once and stencil twice, including center rejection/cold/warm/full contents/padding. COPY offsets beyond twelve-bit MUBUF immediate use explicit scalar offsets handled by the original parser; 1024-word arrays put binary metadata at word 768. Core/checks, full-core links and offline signed linked chains are required; no GPU acceptance before full implementation boundary.
`native-port/gcn_winding_test.cpp`

### 112. native-acyclic-point-gs-emission-bound

Three additional 96-case actual GCN ES/GS/copy modes prepare large declarations with two actual emissions: maximum 128 with no live parameters, maximum 64 with a live float4 parameter, and a real scalar-controlled loop with original maximum 64 retained. Scalar COPY offsets accumulate signed increments instead of truncating high offsets. Static SPIR-V must distinguish eight allocated corner vertices for acyclic cases from 256 for the loop, preserving one ES/GS atomic instruction and MRT output. Core/checks, fixture linking and offline signing only; GPU contents/side effects oracles remain deferred.
`native-port/gcn_winding_test.cpp`

### 113. native-unit-point-gs-hardware-emission

Three additional 96-case actual GCN ES/GS/copy modes cover declared unit maxima 256, maximum 128 with one live parameter and a real loop declared 256. Existing explicit and GCN unit modes now compile with OutputPoints and single emissions; exact GPU color/stencil/counter/padding oracles are prepared but unrun. Offline linked shader compilation/signing, static topology/bound/atomic structure and C++ compile/link checks are permitted only.
`native-port/gcn_winding_test.cpp`

### 114. native-point-gs-consumer-component-widths

Three new 96-case real scalar-loop ES/GS/copy preparations read x/z/w of a parameter with distinct component values. Original maxima 48/36/32 expand to 192/144/128 corners with position-plus-parameter component budgets 960/1008/1024. Static SPIR-V/DXIL must show matching prefix widths 1/3/4 and retained one ES/GS atomic, one loop and four lexical emits. GPU counter/color/stencil/padding and metadata oracles remain unrun; only static builds, linking and offline shader signing are permitted.
`native-port/gcn_winding_test.cpp`

### 115. native-point-gs-sparse-component-transport

Existing x/z/w loop cases use scalar transport; the w case now declares 48 guest vertices instead of 32. A new 96-case x+w loop with maximum 42 transports two distinct channels and prepares green=32/blue=191, counter/stencil and full padding oracles. Static SPIR-V/DXIL must show matching widths 1/1/1/2, correct source values and remapped PS read columns, native budgets 960/720/960/1008, unchanged one ES/GS atomic and one loop/four lexical emits. Only C++ compile/link and offline signing/static artifacts are allowed; GPU oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 116. native-point-gs-aliased-fragment-input-declarations

Two new 96-case real-loop ES/GS/copy modes prepare inputs 0.x and 1.w from parameter 0, plus a default input 0 preceding input 1.w. Initial offline compilation signed but static SPIR-V had two Location 0 variables and DXIL expected TEXCOORD0/TEXCOORD1 while GS produced only TEXCOORD0. Final offline/static evidence must show one producer/consumer semantic and correct compact columns/values; GPU color/counter/stencil/padding and cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 117. native-point-gs-qualified-parameter-transport

One new 96-case actual GCN loop reads flat x and smooth w through two inputs from parameter 0. It declares 42 guest vertices, preserves FrontFace around barycentric input VGPRs and prepares green=32/blue=191 plus counters/stencil/padding. Initial compilation signed but retained duplicate Location 0 variables, requiring static signature review. Final SPIR-V/DXIL must show separate matched scalar locations/semantics with preserved interpolation/value stores, budget 1008 and unchanged one ES/GS atomic/one loop/four lexical emits. All earlier chains must compile/sign; GPU/runtime/cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 118. native-clip-disabled-viewport-transforms

Three new 96-case chains cover sized VS, actual sized GS and unit GS with exported viewport index and disabled clipping. Full color/stencil/padding/counter oracles include centers outside the original viewport. A source-only viewport case preserves differing signed raw transforms despite identical large hardware viewports. Offline compile/sign and static DXIL must prove byte 560 plus 16 times the actual loaded index, unchanged side effects and original arithmetic; all earlier point chains and ten full-core fixtures must compile/link. An initial sized-GS fixture reused v9 for its counter and viewport index; the counter is moved to v10 before final evidence.
`src/video_core/renderer_d3d12/guest_viewports_test.cpp`, `native-port/gcn_winding_test.cpp`

### 119. native-point-center-private-distances

Four new 96-case original GCN loop preparations cover sized clip-only, cull-only, combined clip/cull and unit clip control, with 64 declared guest vertices. Sized cases can emit 256 corners within a position-only budget of 1024; unit clipping retains 64 output points and native distance semantics. Guest distance buffers, signed values, counters, stencil and padding are prepared. Source compile/link and offline signing/static SPIR-V/DXIL must show retained distance loads/comparisons, no sized clipping output signature and unchanged guest side effects. Runtime oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 120. native-linked-clip-input-signatures

Existing native point clip chain previously signed despite VS SV_ClipDistance versus GS SV_CullDistance input mismatch. Final static DXIL must prove matching producer/consumer distance semantics. One new 96-case native cull preparation checks the zero producer clip length independently. All prior actual-GS private distance, unit clipping and emulated/generic chains must compile/sign with all ten full-core fixtures compiled/linked; no GPU/cache/game oracles run.
`native-port/gcn_winding_test.cpp`

### 121. native-point-dense-distance-interface

Two new 96-case native VS preparations cover four Clip plus four Cull distances and sparse Clip indices 0/2 plus Cull indices 5/7. Sparse positive inputs are distinct .125/.375/.75/1; reverse prepares rejection. Static SPIR-V/DXIL must show matched dense 4+4 or 2+2 producer/consumer input dimensions and preserved raw source values/comparisons, no GS distance output semantics and all previous chains signed. No runtime/game oracles run.
`native-port/gcn_winding_test.cpp`

### 122. native-unit-point-dense-distance-outputs

Two new 96-case original GCN loop preparations cover combined Clip4/Cull4 and sparse Clip0/2,Cull5/7 with 64 declared points. Budget is 768 or 512 components, unchanged one ES/GS atomic and one lexical point emit. Static signed DXIL must show the original loaded values in native Clip/Cull output semantics with correct dimensions and point topology, all prior chains signed and ten full-core fixtures compiled/linked. Hardware clipping/runtime/cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 123. native-point-vs-qualified-parameter-transport

Three new 96-case original GCN VS preparations export all 32 source parameters and one VS atomic, with distinct x=.125 and w=.75 from source 0. Two aliased flat inputs and mixed flat/smooth inputs cover sized points; unit mixed uses direct VS/PS. Initial signing succeeded but static PS declared two semantics while the producer had one. Final VS/GS/PS signatures and actual stores/reads must match compact vec2 or two scalar proxies, private misc separated and unit without auxiliary GS. Existing emulated clipping, actual GS and distance modes must sign with all ten fixtures compiled/linked. No GPU/runtime oracles run.
`native-port/gcn_winding_test.cpp`

### 124. native-point-cross-parameter-varying-packing

Three new 96-case original GCN preparations read 32 different scalar sources in alternating x/y/z/w channels or 31 sources plus synthetic four-component clipping. Initial sized modes fail free-slot allocation; initial unit signing terminates with 0xc0000409. Final expected interface is eight float4 locations plus separate point misc/clip data, one VS atomic and all source reads retained. Sized/unit VS, actual GS aliases/mixed/reserved and all distance chains require fresh CPU compile/link/signing and static value/interface proof. No GPU/runtime execution.
`native-port/gcn_winding_test.cpp`

### 125. native-point-ordered-capture-producer

CPU-only ES/GS compilation will cover 256-declared loop x/w and clip/cull cases plus full 32-source copy records. Capture fixture prefixes exclude PS from the producer chain and never permit runtime execution. Wide GCN ring stores use a literal scalar offset instead of overflowing the 12-bit immediate. Actual DXIL must preserve original ES/GS atomics and loops, raw stores and primitive/invocation addressing while containing no emit/storeOutput calls. Ordinary 53 chains and ten C++ fixtures require fresh compile/link evidence.
`native-port/gcn_winding_test.cpp`

### 126. native-point-captured-fragment-attribute-fetch

Six signed typed-reader and linked replay chains; nine GPU modes, 96 cold/warm/facing/flip/cull cases each, including two GS invocations and three draw instances. Full 64-KiB guest results and stencil checked; only known half-UNORM 127/128 rounding has bounded tolerance. Broad native suite failures remain separate.
`native-port/gcn_winding_test.cpp`

### 127. native-point-capture-replay-submission

Production pass and production shader generators run in gcn_winding fixture: 864 GPU cases across nine modes; original ES/GS atomic counters and entire guest memory/stencil checked. Three-instance case verifies cached default-path Required throws before fallback.
`native-port/gcn_winding_test.cpp`

### 128. native-placed-color-transfer-initialization

native_packed_backing_16_8_2 and native_guest_shared_color_layers pass with GPU validation. Packed10 SNORM/SINT/UINT MSAA roundtrips pass; broader backing results are saved externally. Full 64-bit/padding/sample comparisons retained.
`native-port/packed_backing_test.cpp`

### 129. native-present-modes

No separate prepared check recorded in this group; final acceptance not run.

## Pinned upstream limitations (not new port requirements)

Pinned vk_pipeline_cache.cpp skips on-chip geometry and stream output. Native guards for those paths are not counted as missing upstream implementation.
Pinned vk_rasterizer.cpp explicitly excludes depth attachment feedback from its color feedback mechanism. Same-plane depth write feedback is not added as a new requirement.
Raw FMASK decoder/TC-compatible compressed metadata interpretation is not supplied by the pinned fast-clear elimination path; do not expand scope solely because native extension guards exist.

## Post-integration BC continuation

Expanded BC1/2/3/7 CPU-backed RGBA8 now uses typeless storage and typed UNORM/sRGB views of one complete immutable compressed snapshot. Physical layout and all bytes/padding are checked; signed BC4/5/6H FLOAT decodes remain separate. bc_volume_test.cpp adds opposite numeric hardware-reference and same-resource checks, authored/unrun. This does not close the nine unrelated production gaps.

## Ordered interior buffer writers — 2026-10-17

ExecuteGuestResources now admits contained DWORD interior buffer writers in the single-stage, buffer-only, common-UAV image domain. Descriptor ordering uses the first original exact-base writer for I invalidation, while prior offset writers still affect buffer query pages. All clean small reads keep their private CPU snapshots, including those after a writer on another page. Full I retile occurs before logical invalidation; prepared resident aliases share B and preserve padding/exact dirty ranges. Broader layouts, unaligned/partial domains and mixed stages/images remain explicit code gaps.

Eight real-GCN ordered controls are authored/unrun. The full containing C++ fixture and the expanded BC volume fixture passed /Zs with real Release include/define/toolchain arguments. SDK small/far macro conflicts in two fixture headers were fixed by renaming local identifiers. Production core compiled/linked separately; no GPU/game acceptance or FPS evidence.
