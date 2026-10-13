# SESSION HANDOFF — active full-port checkpoint, 2026-10-13

Authoritative continuation for this repository. The latest human request resumed
development and explicitly permits GPU/regression/game checks after logical changes.
The emergency-only stop and old whole-port-before-any-test gate are superseded.
Older current-state headings in other notes are historical. No new overall audit was run.

## Active packed-gap continuation — 2026-10-13 evening

This section supersedes the older point-capture checkpoint below. Source commit
`3f1249b` was pushed to origin/fix/mixed-cmask. It preserves seven selected files,
including their previously uncommitted integration; it is explicitly WIP.
The 22 pre-existing staged network deletions remain outside this commit.
Exact new-session delta, before/current sources and index backup:
`F:/PS4Dev/handoff/packed-gaps-20261013` (`session-only.patch`, `changed-sources.zip`).

- The saved EmitImageWrite assertion was a malformed hand-emitted fixture:
  its sampled-output single-mip UAV received LOD=0 while metadata said None.
  Pinned Vulkan and ResourcePatchingPass already drop LOD for a fixed view;
  packed_image_test.cpp now follows that contract, retaining dynamic mip/sample
  addressing and every content/padding expectation. The production assertion
  was NOT bypassed. Do not repeat the earlier speculative emitter-LOD rewrite.
- Production EmitImageRead now reconstructs exact guest 5/6-bit UNORM values
  after native B5/B6 unfiltered fetch (including SRV component selection).
  Filtered samples are not requantized. Formats16/17/18 linear+tiled+MSAA:9/9 PASS.
- LayoutGuestTexture/HasExactPackedExpansion/GuestTiler/GuestMultisample now
  support sampled format19 MSAA through existing GPU float4 expansion and raw
  sample transfers. Exact guest packed backing, sample/layer layout and padding
  are retained. Format19 sampled+backing tests:7/7 PASS, including former two
  unsupported-RT backing failures. Full guest format19 render-target blending/
  quantization is still open; the fallback is explicitly for sampled resources.
- Stage24/pipeline4/metadata-v33 invalidates old shader semantics. Core build
  succeeded (command-20261013-223852-41016.log), executable SHA256
  `25ca1dee9a3d0c9348bc8dba01850e67604476f6ed2bc54c1cc1b5644fee2872`.
- Packed10 UNORM MSAA passes the current rerun without a separate fix. Its old
  mismatch is not declared resolved; retain as non-reproduced evidence.
- Final configured suite: **324/344 PASS,20 FAIL**. Native subset: **209/227
  PASS,18 FAIL**. This combines retained first49 and same-selection CTest -F;
  signed-packed sampled timed out60s. It is not a wholly green suite or a claim
  that all35 native improvements since174/227 are due to this session's fixes.
  Full log command-20261013-224303-101900.log; detailed failure list/provenance:
  F:/PS4Dev/handoff/packed-gaps-20261013/suite-summary.json and suite-*.log.
- Stronger final packed fixture rejects NaN and passes no LOD for MSAA; all12
  linear/tiled/MSAA cases16–19 PASS in56.17s, raw command-20261013-225604-72632.log.
  This fixture-only hardening followed the full suite; production code unchanged.
- Exactly one new game run, bloodborne-packed-gaps, exited0 after123.8697s.
  Executable25ca1dee..., PID48864, XInput daemon14484/session
  da65fd4a5f2d48a9b79a8aa7277ee20a. A200ms + neutral acknowledged; controller
  removed on game exit. Original20 seed saves verified unchanged; no remote
  session, driver/service or original Vulkan changes.
- **BLACK IMAGE CONFIRMED** in both engine-generated screenshots: game-only
  source225745 and output-with-overlays225822 (PNG files in run/user/screenshots).
  Both were viewed. PM4 draw/dispatch, GPU submissions and Present continue.
  2794 paired completed guest frames/93.101581s yields~29.99949 completions/s,
  median33.363ms,P9535.459ms; this is BLACK-FRAME THROUGHPUT, not world/gameplay
  FPS. No gameplay response or matched speedup accepted. The defect is present
  by the snapshot/source-to-present boundary; exact first producer remains unknown.
  Do not attribute it solely to Windows.Graphics.Capture or claim recovery.
- Installed preview21: dx12-20261013-full-port-preview-25ca1dee9a3d,
  C:/Users/skei/AppData/Local/PS4Desktop/cores/<id>/shadPS4-DX12-Core.exe,
  matching symbols/provenance;76 runtime hashes and20 copied saves verified.
  Existing PS4Desktop and previous selected package were preserved.
  Game report: F:/PS4Dev/handoff/point-capture-20261013/bloodborne-packed-gaps/RESULT.json.

The previous preview20 short run (bloodborne-short-2207) completed4495 paired
guest frames over149.785095s (~30.003/s), exit0 after180.843s; image/gameplay scene
was unverified because Windows.Graphics.Capture failed. It is not a world FPS
baseline and cannot be compared with historical1.377539FPS.

Next production work: narrow the black source boundary using the saved two
captures, current PM4/renderer logs and the existing surface path:
MemorySurfaceProvider::Snapshot -> GuestRasterizer::PrepareDisplayTexture ->
GuestPresenter::PrepareFrame/Present. Determine whether the current guest color
writer produces black or an obsolete/incorrect image version is selected for
VideoOut; fix that demonstrated ownership/render path. Do not rerun the same
game launch until a concrete change or a necessary bounded observation is ready.
Reuse native_coherent_sampled_attachment and native_guest_buffer-image-alias/
partial as existing regressions; do not invent a new profiling framework.

Other remaining failures (full list in suite-summary): odd buffer byte range,
arena residency, 10-bit VideoOut, two old texture fixtures, signed-packed sampled
timeout, CMASK, PageWatch line219, owned-image dispatch/sample forced waits,
BC volume update, MINMAX-sRGB, mixed DS/SRV planes/view ranges, dual-face culling.
Cold runtime compilation contributing to the owned-test3s threshold is only a
hypothesis; preserve fences/PageWatch while diagnosing it.
No new whole-port audit, point replay rerun or whole-group closure claim.

## Historical point-capture checkpoint

## A. Current state

- Repository `F:/PS4Dev/work/shadps4-dx12-src`; branch `fix/mixed-cmask`, upstream
  `origin/fix/mixed-cmask`, remote `https://github.com/Skiters/ps4-dx12.git`.
- Source checkpoint `0a3571a4b76bf1a6683a47f1ee409fe2555614a3`. A later documentation checkpoint may be HEAD;
  exact final HEAD/status/push evidence is in `F:/PS4Dev/handoff/point-capture-20261013/CHECKPOINT_FINAL.json`.
- Source remains dirty with broader unfinished full-port changes and pre-existing
  staged network deletions. The source commit preserves 32 selected implementation/
  fixture files; it is WIP, not a claim that the whole port is complete. Original stage split was verified unchanged.
- Registry: 128 partial groups, zero whole-port semantic closures. Last overall
  audit was 126 groups/1,501 associations/13 mappings; do not imply a fresh audit.
- Ordered point capture/replay now runs on hardware. Ordinary shaders switch to
  it only when the explicit oversized point signature/output-budget exception is raised.
- Native MSAA color upload/sample-copy now initializes fully overwritten placed
  color layers before draw. Depth/stencil and partial-plane semantics are preserved;
  complete MSAA/packed-format parity remains open.
- Current Bloodborne world/control/image/FPS acceptance is NOT established.
  Historical working-world runs concern older executables. Current launch showed
  a black window and no new completed frames in the short capture period.

## B. Implemented production changes

- `guest_shader_frontend.cpp`: `BuildPointCaptureGraphics` compiles original ES/GS
  producer and original FS plus generated replay VS/GS; restores compilation profile
  with RAII. Failed ordinary specializations cache `NativePointCapture::Required`.
- `emit_spirv_quad_rect.cpp`: generated `EmitNativePointReplayVertex/Geometry` read
  typed R32_UINT t0/space33 records; preserve position/size, sparse clip/cull,
  viewport/layer, depth and original PrimitiveID. Original guest GS is not rerun per corner.
- `native_point_capture.h`, `emit_spirv_point_capture.h`: 152 words/608 bytes per
  record; valid tag is original PrimitiveID+1; checked byte-address/instance bounds.
- New `guest_point_capture.cpp/.h` and `guest_point_capture_clear.hlsl`:
  budgeted placed scratch reuse, tag-only CS clear, disjoint producer instances,
  UAV ordering, typed-SRV replay, original attachments bound/cleared once,
  descriptor/allocation/resource leases retained through intermediate Finish/final fences.
  No normal CPU scratch readback or unconditional additional Finish.
- `guest_rasterizer.cpp`: production Required fallback, original-resource producer
  preparation followed by fragment/replay preparation, correct point front-facing
  orientation. Tessellation/on-chip/multiple GS streams remain unsupported here.
- `ring_access_elimination.cpp`: immediate scalar SOFFSET contributes to coherent
  ES/GS ring addresses. Previously wide attribute stores overwrote position words.
  Dynamic scalar offsets remain open.
- `rasterizer.h`, `guest_compute.cpp`, `guest_rasterizer.cpp`: memory extents are
  checked by their owner rather than initializing a process-global guest singleton
  in standalone renderers. Host ranges reject oversized descriptors; Core uses PS4 ClampRangeSize.
- `guest_pipeline_persistence.cpp`: Stage23/pipeline4/metadata-v32 invalidates old
  captured-record/ring-address semantics. Shader signing/serialization fixtures updated.
- `guest_multisample.cpp/.h`: transfer leases retain physical array layer separately
  from row-pitch constants. Full color slice upload/copy transitions to RT then
  discards exactly that overwritten subresource before the native shader draw.
  `multisample_upload.h` initializes its fully overwritten whole color array.
- Fixtures now check actual CPU-copy/GPU-detile pending work, whole PageWatch pages,
  native VideoOut SRV mapping and UNORM gamma clamping; image/padding/sample comparisons remain.

## C. Last unfinished work

MSAA placed-resource initialization is implemented and targeted checks improved.
The next production blocker exposed by those checks is `EmitImageWrite` LOD fallback
at `src/shader_recompiler/backend/spirv/emit_spirv_image.cpp:610`.
`native_packed_16_msaa`, `_17_msaa`, `_18_msaa`, `_19_msaa` now reach an assertion
requiring DynamicIndex mip fallback. Determine the actual emitted LOD and mip/view
metadata; implement correct constant/dynamic/native-MSAA storage selection using
the existing Vulkan/shared emitter semantics. Do not bypass the image write.

Also unfinished: packed10 UNORM word preservation; packed16/17/18 sampled
normalization; format19 multisample RT capability/fallback; owned-image dispatch
completion waits; buffer/image prewrite snapshots; CMASK, DS/SRV planes and residency.

## D. Proven issues versus hypotheses

Proven: last complete native suite has 53 failures; exact names/reasons are in
`C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final-native-summary-20261013.json`.
After it, targeted MSAA tests show three packed10 modes passing, one UNORM mismatch
and four packed-format LOD assertions; shared color layers passes. Retain both pre-
and post-fix logs. Init errors, format capability failures, padding/value mismatches
and forced waits are separate problems, not one cause.

Hypotheses only: the Bloodborne black window may involve launch visibility,
presentation or renderer/guest progress. It is not established that the new point
path caused it, that the game reached Hunter's Dream, or that current FPS improved.
First run used Start-Process WindowStyle Hidden; screenshot helper later failed
`IGraphicsCaptureItemInterop.CreateForMonitor` (0x80070057). A future interactive
validation should use an observed visible test window and avoid blind input.

## E. Measurements and provenance

- Final complete suite BEFORE final MSAA init fix: 174/227 passed, 53 failed,
  822.034 s, raw log `F:/Codex-Shared/devtools/logs/command-20261013-172342-31160.log`.
  Earlier resumed suite: 110/227 passed, 117 failed, 665.41 s. This is test evidence,
  not a performance improvement; many removed failures were outdated fixture assumptions.
- Nine unique point modes x96 cold/warm/facing/flip/cull cases =864 hardware cases.
  Includes two GS invocations and three draw instances; whole 64-KiB guest output,
  atomic counters, depth/stencil and padding checked. Known half-UNORM midpoint
  accepts 127/128 only at explicitly expected color bytes; other bytes stay exact.
- Fresh metadata32 six signed producer/FS/replay chains and typed SRV proof:
  `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final-offline-20261013`,
  `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final32-proof.jsonl` (32 original source/component offsets).
- Final point GPU logs: `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final32-gpu-*.log` and
  `point-capture-final32-three-instances-gpu.log`; latter raw log
  `F:/Codex-Shared/devtools/logs/command-20261013-174006-85624.log`, 8.56 s.
- Post-init: `msaa-initialization-regressions-20261013.log` (3/8 pass, 57.491 s),
  `msaa-initialization-packed-backing-20261013.log` (14/16 pass, 74.764 s),
  `msaa-initialization-all-backing-20261013.log` and shared-layer raw log
  `F:/Codex-Shared/devtools/logs/command-20261013-175340-15484.log` (pass, 15.062 s).
  The complete targeted backing rerun passed46/48 in208.566s; remaining format19
  sample2 cases explicitly lack native RT support. Raw log:
  `F:/Codex-Shared/devtools/logs/command-20261013-175541-94968.log`.
- Game run `F:\PS4Dev\handoff\point-capture-20261013/bloodborne-current`: executable
  `d2684254a886ad3102792cab2f2b16edd2c707c999b0e19f1f95ea555f45cbe5`, build from
  dirty source based on 676d628; manifest/patch/logs/exit/capture/XInput are local.
  XInput session `a503e4b259e24d31a0eb34d6eac5af3d`, PID41608, feeder70912,
  date 2026-10-13; input A and neutral verified, reaction not verified. Exited0 by
  owned CloseMainWindow, feeder neutral/detached. No valid current completed-guest-FPS
  sample; do not report zero FPS or compare this to a world baseline. FG disabled.
- Reuse existing Vulkan evidence `docs/codex-progress/vulkan-session-20261011` and
  `F:/PS4Dev/handoff/performance-recovery-20261011`. Historical DX12 B:124 completed
  frames/90.015605s =1.377539 FPS, median interval727ms/P95765ms. A/A2 failed loading;
  no matched speedup comparison exists. Passive143/90.098142s =1.587158 had uncontrolled input/focus.

## F. Verification boundaries

Core/check builds pass. Six final offline chains and nine final point GPU modes pass.
Metadata/buffer dispatch and resident indices16/32/UAV/UAV-write pass. Current full
suite is NOT green; do not turn targeted reruns into a fabricated full-suite result.
No current game-world, gameplay response, stable image or FPS acceptance.
No original Vulkan or original saves changed. Partial-render disabling was not used.

## G. Preserved builds and files

- Latest experimental core `F:\PS4Dev\work\shadps4-dx12-src\build-migration-core\shadPS4-DX12-Core.exe`, SHA256 `34d785e8b1dcee941daa8b2447ec9bd164dff59a6bfa39d525b548a7eaf06d29`;
  matching `shadPS4-DX12-Core.pdb`, `native-core.map`, `native-compiler/` alongside.
  Build logs `point-capture-metadata32-core-build-20261013.log` and
  `msaa-initialization-core-build-20261013.log` in `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work`.
- Immutable original source `F:/PS4Dev/work/shadps4-original-e0a46675`, SHA
  e0a466753a1aacbc8db8253be685bf7ab4d9daae. Installed Vulkan/DLSS/FG
  `F:/Apps/shadPS4-DLSS-DLAA-1.8.4/shadPS4.exe` remains untouched.
- Historical working-world DX12:
  `C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261012-ba3892f-covered/shadPS4-DX12-Core.exe`.
- Existing desktop `C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/PS4Desktop.exe`;
  installed preview18 `dx12-20261013-full-port-preview-519d8356217d` remains immutable.
  New preview19 is installed as `dx12-20261013-full-port-preview-34d785e8b1dc`,
  display name `DX12 — перенос №19 — 13.10.2026`, same latest executable SHA256.
  Its directory is `C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261013-full-port-preview-34d785e8b1dc`;
  matching symbols/provenance/runtime files are retained there. Isolated profile:
  `C:/Users/skei/AppData/Local/PS4Desktop/core-profiles/dx12-20261013-full-port-preview-34d785e8b1dc/native-profile`.
  All76 runtime-file hashes and20 copied saves verified; prior selected package/settings
  selection and original save hashes preserved. No gameplay success claimed for preview19.
- Seed saves `F:/PS4Dev/handoff/coherence-20261011/seed`; isolated game profile
  `F:\PS4Dev\handoff\point-capture-20261013/bloodborne-current/user`; original save hashes verified while copying.
- XInput/HUD/live monitor `F:/PS4Dev/handoff/vulkan-live-monitor/program`.
  Existing user monitor/PresentMon instances are not owned; do not stop them.
- All source before the new commit: `F:\PS4Dev\handoff\point-capture-20261013/before-source-commit`:
  verified zip, working/staged/unstaged binary patches, original index and file manifest.
  Final residual archive is recorded in CHECKPOINT_FINAL.json. Do not reapply patches
  to this existing dirty tree. Use a separate clean checkout of the recorded archive HEAD
  if recovery is necessary. Old emergency backups are preserved unchanged at
  `F:/PS4Dev/handoff/full-port-emergency-20261013`.

## H. Exact next action

Read this file and final checkpoint JSON, check HEAD/status, preserve residual changes.
Then run the existing failing case (no general audit):

```powershell
& 'F:/Codex-Shared/devtools/bin/codex-dev.cmd' rtk ctest --test-dir 'F:/PS4Dev/work/shadps4-dx12-src/build-migration-checks' --output-on-failure -R native_packed_16_msaa
```

Continue production `EmitImageWrite`/native storage mip selection from its saved
assertion. Inspect only relevant IR/resource metadata and pinned Vulkan/shared
behavior. Implement real image-write support; build incrementally, verify affected
packed/MSAA/alias cases, then minimally test Bloodborne through the existing virtual
XInput in a copied profile and measure completed guest frames without FG.

## I. Do not repeat without a new failing case

No new whole-port audit or rereading old chat/architecture. Typed SRV/t0-space33/
152-word addressing, signature packing, Private pointers, literal ring SOFFSET,
point front-facing and producer/replay instancing are already proved by saved cases.
Do not repeat the raw readonly-SSBO experiment (it became a UAV). Do not weaken
memory/ownership/fence/alias operations to improve FPS. Do not silently ignore the
53 complete-suite failures. Never discard partial DS/other layers to initialize a
color transfer. Initial erroneous discard used row pitch as layer; corrected explicit
layer is verified. Do not rerun dozens of identical game launches. Never reset/clean
this tree, force-push, replace installed Vulkan, mutate original saves or delete old results.

## NEXT CODEX SESSION — START HERE

> Продолжи Skiters/ps4-dx12 на fix/mixed-cmask с актуального HEAD. Прочитай
> активный packed-gap раздел SESSION_HANDOFF.md и
> F:/PS4Dev/handoff/packed-gaps-20261013/CHECKPOINT_FINAL.json. Сохрани dirty tree
> и22 staged удаления. Packed16–18 fetch и sampled packed19 MSAA исправлены;
> ImageWrite assertion был нарушением контракта fixture, не переписывай emitter
> по старой гипотезе. Полный suite324/344, native209/227;12 усиленных packed PASS.
> Следующий блокер: оба штатных снимка Bloodborne чёрные при продолжающихся
> PM4/GPU/flip/Present. Локализуй и исправь первый неправильный producer/version
> в MemorySurfaceProvider::Snapshot -> GuestRasterizer::PrepareDisplayTexture ->
> GuestPresenter::PrepareFrame, используя сохранённые логи/снимки и Vulkan-эталон.
> Используй existing coherent sampled/alias regressions, затем сборку и один
> необходимый короткий изолированный XInput-прогон. Проверяй фактическую сцену,
> а не только отправку ввода;30 black completions/s не игровой FPS. Не трогай
> удалённую сессию, оригинальные сохранения/Vulkan. Продолжай production C++/HLSL,
> сохраняй и push проверенные commits, не повторяй общие аудиты.
