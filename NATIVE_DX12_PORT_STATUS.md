# Native DirectX 12 port — 2026-10-09

## Latest checkpoint — 2026-10-11 GXWorker PageWatch

See [GXWORKER_PAGEWATCH.md](docs/codex-progress/GXWORKER_PAGEWATCH.md).
The original guest write fault is a confirmed queued-fault/retired-range lookup
defect. PageWatch now preserves coalesced retirement history and handles rearming
without weakening protection or skipping reconciliation. Real failing-before GPU
regressions pass; 208/208 native tests pass. COPY queue texture initialization also
now satisfies the Debug Layer. Complete-fix game run encountered a separate
loading wait: Game:Main waits on a MOMainThread-owned mutex. Playable world execution
is not yet established. Final committed-build dev launcher and evidence are under
`F:/PS4Dev/handoff/gxworker-20261011/dev-release`; exact hash/path are recorded in
the external release manifest. Installed desktop and original profiles stay unchanged.
Older deployment/timing entries below are historical, not this build's identity.

## October 10 buffer-cache cycle

The resident buffer cache no longer alternates allocations indefinitely when
two large, read-only PS4 sharps partially overlap.  An eligible CPU-coherent
mapping now grows once to the checked union and subsequent bindings retain the
same D3D12 resource.  GPU-owned, dirty, failed-upload and mixed SRV/UAV ranges
keep the conservative reconciliation path.  The regression test verifies the
resource identity and both offsets with the D3D12 Debug Layer enabled.

The focused hardware test and the complete native suite pass (205/205, 58.51s)
on the RTX 5080, and the full emulator core links successfully.  A 66-second
run from the isolated test profile produced no native fault, device removal or
D3D12 validation error.  It presented 18–34 frames per two-second interval in
the observed section, but this is not comparable to an earlier gameplay run
because the profile and scene state differ.  No same-scene FPS gain or stable
world entry is claimed.  The run instead identified compute dispatch as the
next measured cost; diagnostic stages now distinguish image-operation
recognition, copy-shader HLE and metadata-clear recognition.

The separate Windows library/PKG menu connects to a full Release shadPS4 core using D3D12/DXGI. Saved-world rendering, playable performance, native 60 FPS and frame generation remain unverified. The installed host correctly reports `gameLaunchReady=false` while allowing an experimental launch.

## Deployment

- Menu: `C:\Games\PS4-Desktop\PS4Desktop.exe`.
- Orbit entry: “PS4 — библиотека и установка PKG”.
- Host: `C:\Games\PS4-Desktop\native\shadPS4-DX12.exe`.
- Core: `C:\Games\PS4-Desktop\native\core\shadPS4-DX12-Core.exe`.
- Core SHA256: `E355644A77A7CAE0A38BE367DFFCC63BE8A9540300DEEE66E82F92D645C59D5E`.
- Separate profile/save copy: `C:\Games\PS4-Desktop\native-profile`.

The installed menu selects only dx12, with no original-core fallback. The original Vulkan application/profile are preserved. Original executable SHA256: `036CC95DFA82E701C10FAD06899E8F2B1E60EB3D4E26032E6B02EDBF42121102`. Windows, drivers, power and affinity were not modified.

## Changes and checks

The core connects guest CPU/HLE, Liverpool, GCN-to-DXIL translation, graphics/compute pipelines, mapped buffers/GDS, descriptors, tiled images, mips/arrays/volumes, BC/packed formats, blending, depth/stencil, VideoOut and native ImGui. Unsupported states fail explicitly.

October 9 changes:

- Uniform single-sample CMASK fast-clear expansion with layer/padding preservation. Mixed tile codes remain unsupported.
- Proper null image SRV/UAV descriptors and native depth-bounds testing.
- Persistent constant uploads, reusable fenced image transfer pools and readback scratch, and a bounded texture budget.
- Draw/dispatch and image readbacks share a submission. Read-only depth/stencil skips writeback; unchanged uploads are reused after comparing both CPU planes.
- GPU bilinear 1280×800 → 1920×1200 display scaling, replacing CPU pixel resizing. This is **not XeSS**.
- Bounded immutable display snapshots. Eligible coherent GPU textures are copied directly; CPU changes/incompatible layouts use checked CPU detile/upload.
- Corrected scaled-overlay resource transitions. The initial integration failed; the installed correction passes scaled-overlay readback and D3D12 validation.

**71/71 native checks pass** on local hardware, including actual XeSS SDK execution, exact readbacks, CMASK, null resources, depth bounds, CPU depth/stencil changes, display snapshot ownership, scaling, overlays, nested diagnostic scopes and coherent sampled attachment reuse. Log: `build-native-dx12-clang/native-reference-port-check.log`. Full build: `build-full-native-dx12/native-reference-port-build.log`. The separate settings suite previously passed 63/63; settings code did not change in this iteration.

Original-renderer behavior transferred to native D3D12 (source reference only; no Vulkan execution):

- The 42F2C8 run passed the earlier failure points and exposed an unconditional buffer/image alias rejection at 247.012s. Read-only buffer/image overlaps are now materialized from the same retired CPU-visible guest storage using separate native views, including cross-stage aliases. Any participating write remains rejected explicitly with addresses/access metadata; feedback semantics are not silently discarded. A real vertex raw-buffer + pixel sampled-image draw reads the same float RGBA guest backing through both views and verifies the final pixels under D3D12 validation. The reference implementation uses buffer-backed image transfers and dirty ownership; this port preserves read coherence without pretending it has full shared write ownership.
- The 42F2C8 opt-in resource-stage log localized repeated 120–137ms work to buffer preparation. An experiment retained additional snapshots of written-buffer readbacks, but the following game run still had slow preparation and some more expensive GPU-copy packets. Those extra written-buffer snapshots were removed before final deployment; they have no established game benefit. Existing read-only snapshots remain, and any GPU write now invalidates an older snapshot so a later CPU return to its old contents cannot incorrectly reuse stale GPU data. An actual native GPU-copy readback covers that stale-snapshot case. Unified-buffer tests also repeat dispatches, change CPU data without an invalidation callback, and verify written/untouched words through both standalone and linked conversion.
- Resident-buffer budget accounting previously reserved the whole requested batch a second time, clearing an unchanged >256MiB batch every draw. It now retires partial overlaps first and reserves only genuinely new backing. Under pressure it evicts inactive mappings before considering full rematerialization of oversized covered buffers. An actual 320MiB batch verifies stable GPU resource identities across repeated bindings/CPU changes; a subsequent 400MiB batch proves a protected resident resource survives pressure, then exact GPU copies verify the changed boundary words. Cache and alias budget protections remain active. Same-scene game FPS benefit remains unverified.

- The DD3227 game run passed the stencil/depth-bias blockers, then at 182.754s exposed a shared guest buffer bound as a native read-only SRV in one stage and writable UAV in another. The original buffer cache shares the underlying guest storage. Native graphics/compute compilation now unifies SSBO views as UAVs through a targeted offline-intermediate access-decoration rewrite. Actual shader instructions and guest write metadata remain unchanged; image and uniform-buffer classes are preserved. The same GPU backing can now serve read/write views with one valid D3D12 resource state. Tests execute exact aliased guest-buffer results through both standalone and linked DXIL conversions; cached guest graphics/compute pipeline checks and the sampled/writable-image execution test also use the new option.

- `vk_rasterizer.cpp::UpdateDepthStencilState` also supplies polygon offset, clamp and slope scaled by 1/16. After the DB-validity correction, the B4716B game run reached the formerly failing stencil state at 178.753s and then exposed unsupported shadow depth bias. Native rasterizer PSOs now preserve integral bias, while fractional constants use OPTIONS16-validated `D3D12_PIPELINE_STATE_FLAG_DYNAMIC_DEPTH_BIAS` and `ID3D12GraphicsCommandList9::RSSetDepthBias`. Actual GPU D32 readbacks verify both paths. Front/back selection, clamp/slope conversion and non-finite rejection are covered. Point/per-face polygon modes remain explicit unsupported states.

- `vk_rasterizer.cpp::UpdateDepthStencilState` gates guest depth/stencil enable bits on valid planes. The prior A8BA diagnostic run eventually failed at 364.187s with `Native guest stencil test requires a stencil-capable attachment`. `GuestDepthTests` now applies equivalent validity checks before native attachment selection, clear decisions and PSO translation. Tests cover stale stencil flags on a real D32 GPU draw, missing addresses/formats, an entirely unbound DB and valid stencil preservation; valid stencil/native-format mismatch still fails explicitly.
- `vk_shader_hle.cpp::ExecuteCopyShaderHLE` batches bounded source/destination extents. The D3D12 equivalent records ordered `CopyBufferRegion` operations, shares common buffers within 64MiB extents, and handles same-resource copies via reusable scratch. Batch size is bounded at 256 commands. Coalescing across inaccessible backing falls back to individually validated regions. The native coherence model still performs a fence/readback once per packet. Tests include 257 chained overlapping copies, a prior GPU-written source, untouched bytes, whole-packet prevalidation, guest u32 wrap/zero-byte commands and odd-byte-stride CPU fallback, with D3D12 debug validation enabled.
- `texture_cache.cpp` derives garbage-collection pressure from adapter memory. Native texture storage now reads DXGI's adapter budget, checked at most once per second when allocating, and uses half of it capped at 2GiB. Without budget support it retains 512MiB. Texture backing estimates, CPU coherence snapshots and transfer pools remain accounted; in-flight entries cannot be evicted. The local Intel adapter reported a ~15GiB process budget and selected the 2GiB cache limit. See [QueryVideoMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiadapter3-queryvideomemoryinfo) and [D3D12 copy states](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copybufferregion).

The first GPU-copy integration created one small native allocation per region and regressed loading; it was replaced before final deployment by shared bounded operands. The E8C9E6 run passed all three observed graphics failure points and continued beyond 420 seconds without native command/validation errors, but still visibly showed the loading/item screen. This is not saved-world or native FPS proof.

The following 430E39 diagnostic-stage build ended at about 52 seconds in HavokWorkerThread with access violation `0xc0000005`, native RVA `0x840d5e`. Its exact map resolves this address to `ZydisDecoderDecodeInstruction+0x1ae`; no native D3D12 command error was logged. Investigation found that `TryPatchJit` selected the nearest preceding patch module without verifying that RIP was below the module end, allowing unrelated faults to enter guest decoding with an underflowed length. JIT/AOT now require actual module containment, and AOT ranges cannot cross its end. Opt-in unhandled-fault reporting includes register/fault-address details and at most 24 stack words, read safely through ReadProcessMemory. This corrects a concrete bounds defect; it does not establish the underlying cause of the original Havok fault.

Latest native-startup investigation (XeSS/XMX/FG deferred):

- Opt-in `SHADPS4_NATIVE_DIAGNOSTICS=1`: bounded 2-second aggregate summaries, current per-thread operation/stage, slow-operation timings and exception context. No frame capture or command-history cache. Normal launches do not start this worker.
- Draw/dispatch, buffer reconciliation, image detile/retile, GPU fences, frame preparation and presentation are instrumented. `SHADPS4_NATIVE_LOG_FILTER` overrides module logging only for the diagnostic process; saved profiles are unchanged.
- Resource preparation now has distinct opt-in stages for metadata/layout, buffer views, descriptor heap allocation, buffer/constants descriptors, image descriptors, sampler descriptors, command recording, GPU retirement and temporary-buffer retirement. Counts are bounded aggregate data, not per-frame/history caches; ordinary launches remain disabled.
- D3D12 validation messages are copied to stderr. Warnings and errors have separate budgets, so repeated warnings cannot hide a later error.
- Logs identified repeated 64MiB 4096×4096 depth-image CPU detile/upload operations (one CPU detile ~148ms, total upload ~217ms in that diagnostic run). New read-only view borrowing reuses a retired native attachment only after full CPU snapshot and physical-layout comparison. Changed CPU memory, stencil planes or incompatible layouts retain the checked upload path. GPU clear/readback tests verify swizzled views and CPU-write fallback.
- A buffer reconciliation snapshot had 5214 cached buffers and zero dirty buffers. Dirty mappings and active batch mappings are now indexed, cached byte totals maintained incrementally, and invalidation searches only overlapping address ranges. Partial overlap rematerialization is verified with actual GPU copies; no dirty writes/fences are bypassed.
- Default UAV buffers now start in COMMON with explicit later transitions, correcting the repeated runtime warning about ignored initial UAV state.

The 6E074F diagnostic run continued beyond 555 seconds with zero native command/validation errors and no unhandled exception. It still showed the loading/item screen and continued GPU work, including expensive image readback/CPU coherence. Saved-world pixels and playable FPS were not established. Its opt-in diagnostic logs are `C:\Games\PS4-Desktop\native-profile\reference-resident-{stdout,stderr}.log`; its evidence is `outputs/ps4-desktop-deployment/reference-resident-run.json`.

The final 4C2B47 build removes the extra written-buffer snapshots and adds protected-mapping eviction and GPU-write snapshot invalidation. Final ordinary-launch logs: `C:\Games\PS4-Desktop\native-profile\reference-final-{stdout,stderr}.log`. Matching final map: `outputs/ps4-desktop-deployment/reference-port-core.map`. Regular Orbit/menu launches remain uninstrumented. No game-world/FPS claim is made for this final rebuild.

Final ordinary launch directly rendered Bloodborne's title/offline-choice menu. Loaded graphics modules were dxgi.dll, d3d12.dll and D3D12Core.dll, with no Vulkan runtime, validation layer or native diagnostic output. The owned smoke process was stopped after recording evidence; no instrumented test remains running. Runtime/build checks are stored in `outputs/ps4-desktop-deployment/verification-native.json` and `normal-native-smoke.json`. The final installed core matches the full build, SHA256 4C2B47 above.

## Game evidence

Bloodborne title/offline/menu pixels were visually verified in the native-only `C697E56D...` build. The earlier `2C3E73C6...` build processed PS4 gamepad input and reached saved-world loading beyond the previous CMASK/null/depth-bounds failures. UI capture recovered for the latest test and directly showed loading/item-screen pixels in the native-only E8C9E6 build. Saved-world pixels/gameplay performance have not been verified. Earlier UI-capture failures are historical, not evidence about the current game state.

An earlier attempt had Game:Main waiting for a mutex owned by MOMainThread while the GPU thread was idle; that snapshot did not establish a deadlock or driver fault. Current snapshots instead show GPU readback/compute processing. Initially unresolved AvPlayer/RTC imports are dynamically registered by the existing sysmodule loader; no forced registration/bypass was added.

Logs: `C:\Games\PS4-Desktop\native-profile\scaled-overlay-{stdout,stderr}.log`. Matching map/evidence: workspace `outputs\ps4-desktop-deployment`, outside this fork. Game navigation uses the internal PS4 gamepad path, not keyboard/mouse.

## XeSS / XMX investigation

The official SDK v3.0.2 is separate in `work\xess-native-sdk-v3.0.2`; its SR DLL reports 2.0.2. It is not installed as an active guest-game upscaler.

`native-port/xess_execution_probe.cpp` supplies valid stationary synthetic color/depth/motion textures to real XeSS, executes eight 1280×800 → 1920×1200 frames through D3D12, waits for GPU completion and checks output pixels. It passes without D3D12 validation errors. This proves SDK execution, **not** guest integration, XMX utilization or frame generation.

Intel XeFX version reports `0.0.0`, including a separate comparison preloading the existing signed Intel extension DLL. XMX execution remains unverified. No vendor spoofing, replacement driver or Linux/Vulkan shim was applied. A diagnostic query reports SM6.8; SM6.10 returns E_INVALIDARG. The queried runtime therefore does not expose SM6.10 linear algebra; that does not mean the hardware lacks XMX.

Correct temporal integration needs identified pre-postprocessing color, motion vectors, depth and jitter. The current provider receives final VideoOut color including HUD. Synthetic zero motion is used **only in the stationary SDK test**, never as fabricated guest-game motion.

## Remaining work

October 9 loading investigation (current installed build):

- A one-shot stack snapshot of the user's 13-minute loading run found the GPU command processor inside host backing validation, while the guest main thread waited. Subsequent opt-in per-stage logs localized 100+ ms draw delays to `PrepareShaderBuffers` validation, not D3D12 fence waits. This is a concrete bottleneck; it is not proof that no other load blocker exists.
- Readable/writable validation uses ordered, coalesced intervals instead of a linear list. Data-only HLE copies invalidate native buffer data without invalidating host page-protection knowledge.
- The guest copy-shader HLE now retires preceding GPU writes once and executes the exact ordered byte copies on coherent CPU backing. It no longer uploads/readbacks bounded common operands containing large untouched gaps. The actual native GPU-copy implementation remains available and tested; no guest copy is skipped. Tests cover GPU-written HLE sources and subsequent native GPU reads of HLE destinations.
- Core's successful full committed mappings publish actual host backing permissions to D3D12. Those authoritative intervals are independent of the bounded opportunistic validation cache. Unmap/remap and protection changes invalidate/split them. Sparse/reserved PRT coverage is never published as committed backing. Real protection/decommit/recommit tests continue to pass.
- `BufferViews` diagnostics distinguish validation, prior GPU retirement, overlap retirement, resident allocation and refresh. These bounded aggregates remain opt-in; ordinary Orbit launches enable no diagnostic worker.
- **71/71 tests pass (10.70s)**: `build-native-dx12-clang/native-load-check.log`. Full core build: `build-full-native-dx12/native-load-build.log`. Matching map: `outputs/ps4-desktop-deployment/native-load-core.map` in the task workspace. Saved-world validation of this build is in progress; do not infer readiness from the test count.

1. Resolve and visually verify saved-world loading/rendering with repeatable gamepad navigation.
2. Replace per-draw CPU attachment reconciliation with coherent GPU ownership and CPU fault/alias handling. Current snapshots show this work and GPU waits; no same-scene FPS improvement is established.
3. Capture valid temporal inputs and verify Intel-specific execution before enabling guest XeSS/XMX or frame generation.

## References from other projects

- [Mesa descriptors](https://chromium.googlesource.com/external/gitlab.freedesktop.org/mesa/mesa/+/418c4cfa6708a0e0b1175e72fb8fd27d3ca1615a/src/amd/common/ac_descriptors.h): CMASK clear/expanded semantics.
- [D3D12TranslationLayer](https://github.com/microsoft/D3D12TranslationLayer/blob/master/src/ImmediateContext.cpp): null descriptors.
- [Xenia texture cache](https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/d3d12/d3d12_texture_cache.cc) and [shared memory](https://github.com/xenia-project/xenia/blob/master/src/xenia/gpu/d3d12/d3d12_shared_memory.cc): ownership/lifetime designs, not a drop-in PS4 backend.
- [Intel XeSS guide](https://github.com/intel/xess/blob/main/doc/xess_sr_developer_guide_english.md): temporal inputs/native D3D12 API.
- [Microsoft linear algebra](https://devblogs.microsoft.com/directx/d3d12-linalg-preview/): SM6.10 matrix operations. Repeated work alone does not make arbitrary guest CPU logic a matrix workload.
- [XeSS XMX Linux](https://github.com/HHHHanasak1/xess-xmx-linux): reports ONEXPLAYER 3/Arc B390 support, but requires Proton and patched Mesa ANV. Its installation does not fit this native Windows/D3D12-only fork and was not applied.

## Separation and diagnostics

Vulkan renderer/caches/UI/host shaders, DLSS layer and Vulkan submodule declarations are excluded. Recoverable copies are outside the fork in `outputs\ps4-desktop-deployment\legacy-sources-outside-fork-20261008`. SDL Vulkan backends are disabled. SPIR-V is only an offline compiler intermediate to DXIL.

Startup trace, bounded native diagnostics and D3D12 validation are independent opt-in environment settings. Normal Orbit/menu launches enable none. External stack/wait-chain tools are one-shot diagnostics, not background emulator telemetry.

## Paused by user — 2026-10-09

Development and game-loading runs are stopped at the user's request. Installed core
E355644A77A7CAE0A38BE367DFFCC63BE8A9540300DEEE66E82F92D645C59D5E is retained.
71/71 native checks passed in 10.70 seconds. The final menu was rebuilt to strip
inherited native diagnostics, validation, startup tracing, log overrides and benchmark
controller input from ordinary child launches. Library/profile/device checks pass;
menu pixels and clean menu shutdown were verified. XeSS/XMX, frame generation,
FSR/RCAS/HDR remain inactive. No game-world or native FPS claim is made.

Pause archives and their checksums are in outputs/ps4-dx12-paused-20261009 in the
workspace. Original Vulkan installation is preserved. Proposed lazy invalidation and
fair recursive locking were not implemented before pausing. The loading blocker
remains open; all test processes are stopped and evidence Active fields are false.

## Migration package — 2026-10-09

The separate Inno Setup build is on F:/PS4-Desktop-Transfer-20261009. Menu defaults
are now independent of the old game/profile/user paths. Clean user data and a
profile template replace cloning the original game installation. PKG updates/DLC
share the correct profile addcont directory. The shipped PKG worker and .NET/CRT
runtime are self-contained. Core E355... is unchanged, no game/saves/firmware/cache
are shipped, and saved-world loading remains open. Migration checks cover local
install/uninstall, empty library, native probe/core help, real synthetic PKG GUI
installation and exact standalone worker extraction without external Python.
Development dependencies are not copied into the source backup; exact pins, local
Mesa overlay and restoration/build instructions are in migration/. Full restoration
on another physical PC has not yet been verified. Historical Build-*.cmd paths are
preserved; use migration scripts and explicit DXC SDK include/runtime overrides.
