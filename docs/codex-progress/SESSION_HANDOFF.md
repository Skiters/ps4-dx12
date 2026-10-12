# SESSION HANDOFF — emergency checkpoint, 2026-10-13

This is the authoritative starting point for the next session. The latest human
request stopped development for preservation only. No new game runs, profiling,
GPU tests, long builds or implementation changes were made during checkpointing.
Older sections called "Current" in the other progress documents are historical.

## A. Current project state

- Repository: `F:\PS4Dev\work\shadps4-dx12-src`, remote
  `https://github.com/Skiters/ps4-dx12.git`, branch `fix/mixed-cmask`, upstream
  `origin/fix/mixed-cmask`.
- Before checkpoint: `e84e31133320830db6268f3beefe9ddda1fb08cc`.
  Source WIP checkpoint: `c0aa3e4a6fb2de072b29ffa4d0682d5accd9c292`.
  The documentation checkpoint follows it. Its exact final SHA/status are saved
  externally in `F:\PS4Dev\handoff\full-port-emergency-20261013\CHECKPOINT_FINAL.json`;
  verify against `git rev-parse HEAD` before resuming. A document cannot contain
  the SHA of its own commit.
- Immutable original source: `F:\PS4Dev\work\shadps4-original-e0a46675`, revision
  `e0a466753a1aacbc8db8253be685bf7ab4d9daae` with pinned submodules. Do not change
  the reference or substitute the installed DLSS fork.
- Full-port registry has 126 partial groups. Last saved audit: 1,501 associations,
  13 mappings, **zero semantic closures**. Full implementation and acceptance
  remain open. See `full-port-e0a46675/STATUS.md` for preserved chronological detail.
- Historical DX12 reached the Bloodborne world, movement/camera/menu/attack and
  short backstep; rendering artifacts and unplayable speed remained. Those runs
  concern earlier executables, not the present full-port source.
- Current source compiles/links, and earlier versions of the new shader paths
  compile/sign offline. No current Bloodborne/GPU/XInput/FPS acceptance exists.
  Point capture remains opt-in, default false in production.

## B. Completed implementation and evidence

The WIP commit preserves 19 known files from the current point/capture work;
`code-checkpoint-files.json` in the external handoff lists them exactly. It is an
unfinished development checkpoint, not a correctness/performance completion.
Other broad pre-existing migrations, network deletions and desktop changes were
not swept into that commit; their provenance was not re-established here.

- Groups 123/124: `guest_point_transport.h`, `guest_point_shader.h`,
  `emit_spirv_point.h`, `spirv_emit_context.*`, `EmitGetAttribute` and
  `guest_pipeline_persistence.cpp` preserve live source/component mappings and
  qualifiers. Compatible groups pack into shared float4 host rows with explicit
  component offsets. 32 scalar sources fit eight rows. Center/centroid/sample,
  aliased and mixed interpolation preparations compile/sign; prepared GPU
  oracles were not executed. Preview 18 includes through group 124/core 232.
- Group 125: `NativePointCapture::Record` / `Plan::Make` / `Plan::Slot` in
  `src/shader_recompiler/native_point_capture.h`; SPIR-V producer helpers in
  `backend/spirv/emit_spirv_point_capture.h`; capture handling in
  `EmitSetAttribute`, `EmitVertex`, `EndPrimitive` and `DefineInputs`.
  `DefineNativePointCaptureBuffer` / `EmitNativePointCapture` use fixed
  primitive/invocation/emission slots, retain original guest loops and
  side effects, write one raw UAV record, and avoid hardware vertex emission.
  Original GS `InvocationId` input is declared when original IR reads it.
- Shared record ABI: 152 uint words / 608 bytes; valid=0, position=1..4, size=5,
  layer=6, viewport=7, clip=8..15, cull=16..23, 32 float4 parameters=24..151.
  Descriptor binding 0 / space 33. `Plan` checks arithmetic and total **byte**
  addressing against uint32 bounds. Publication requires completed producer
  work and a UAV barrier; the valid word is not atomic publication.
- `native-port/gcn_winding_test.cpp` prepares capture-/capture2- offline chains,
  rejects execution of those modes, and fixes wide MUBUF fixture store offsets.
  Full 32-source records retain all 128 values. All claims are static/offline.
- Pinned Mesa native patch includes prior clip-size/interstage fixes, canonical
  SHA256 `d4d16e7054beb94acdf45beeb7e68b5380ac391f2aa189f01970d2ebeaf18b2b`.
  `native-port/vendor/mesa-native.json` and the patch are saved in the WIP.
  Do not patch generated Mesa build directories as the lasting source.

## C. Work interrupted immediately before checkpoint

**Group 126: captured fragment attribute reads without forwarding the full
128-component guest signature through replay VS/GS.**

Implemented: fragment input is one flat uint record ID; `EmitGetAttribute`
uses the original source/channel or original synthetic clip index. Capture FS
skips ordinary varying declarations while preserving original resource
reflection and IR defaults. Profile/runtime flags and persistence versions are
included: StageVersion 21, PipelineVersion 4, metadata-v30.

First implementation used readonly SPIR-V StorageBuffer. Existing pinned Mesa
still emitted `native_point_capture` as byte-address **UAV u0, space33**, despite
NonWritable. The final static proof correctly failed its SRV expectation.

Latest change: `DefineNativePointCaptureReader` declares a sampled uint Buffer
image, `SampledBuffer` capability, UniformConstant binding; `EmitGetAttribute`
uses `OpImageFetch`. Intended host view is **R32_UINT SRV t0, space33**, addressed
in **152-word strides**, with original word offsets. Producer stays a raw UAV.
This last change passes C++ build/link (core 237/checks 190), but its frontend
fixture, SPIR-V→DXIL signing, actual resource reflection and affine word
addresses have **not yet been verified**.

`gcn-winding-test.exe` and `point-capture-reader-final-dxil-*` artifacts still
belong to core 236 / the rejected raw-reader implementation. Do not use them as
proof of core 237. `verify-captured-fragment.py` still expects rawBufferLoad and
608-byte addressing; update the proof for typed loads and 152-word addressing
after inspecting fresh artifacts. Preserve the old failure evidence.

Not implemented: generated replay VS/GS, runtime capture allocation/clear,
root descriptors, UAV→SRV transitions, replay draw submission, validity-hole
handling, lifetime/fences/pressure, indirect/instance/tessellation/stream bounds,
and runtime equivalence. Do not enable the production flag yet.

## D. Confirmed problems and separate hypotheses

Confirmed:

- readonly SSBO translated to UAV under `native_storage_buffer_uavs=true`;
  the old proof failure is a real binding mismatch, not a parser false positive.
- Forwarding all 128 components through replay signatures reintroduces the
  register/signature budget. Existing 32-source packing covers only prepared
  sparse scalar cases, not general full float4 semantics.
- Mesa can combine valid-marker/payload writes. Concurrent readers are unsafe.
- Historical matched DX12 baseline A/A2 failed during loading on overlapping
  SRV/UAV aliases; no matched speedup was established.
- Historical B still materialized 7.115 GB at guest address 0x2d1ce3000 in its
  bounded sample. Scope totals showed heavy coherence/materialization/retirement
  work. This is evidence for that historical core only.

Hypotheses/open correctness questions:

- Typed image declaration will produce t0 SRV: pending fresh signed reflection.
- A flat record ID plus PS fetch can preserve point interpolation/precision:
  pending review of PerVertex, centroid/sample, derivatives, MSAA and coverage.
- Unit-point manual expansion can match clipping/raster coverage: unverified.
- Capture/replay can improve a real game: no measurements exist.

## E. Measurements and provenance

Main catalogue:
`F:\PS4Dev\handoff\full-port-emergency-20261013\DIAGNOSTIC_PROVENANCE.json`.
It records absolute paths, session/run IDs, source SHA, executable SHA256,
dates/scenarios/results/limits for 25 preserved datasets. Unknown fields are
explicitly unknown; neighboring original manifests are authoritative.
`evidence-files.json` indexes 111,647 existing evidence files by path, size and
file modification UTC; modification time is **not** measurement time.

| Historical evidence | Result | Trust boundary |
|---|---|---|
| Covered-envelope DX12 B, ba3892f, SHA256 866e800e5f7f97fb7c3613697869883178091888ca82290d1aeec4567a26aa77 | 124 frames / 90.015605 s; 1.377539 FPS; median 727.008 ms, P95 765.871 ms | A/A2 failed; no matched speedup; no FG |
| Protected-page/index DX12, 0c340b4, SHA256 9438103c6eca8b5453971a55fc20159ce19091a75fc3784120d6cee50ad4c1c0 | 143 / 90.098142 s; 1.587158 FPS; earlier passive baseline 0.863128 | Focus/input uncontrolled; not a baseline for current source |
| Covered-envelope B resource scopes | 266,769,064 materialized / 278,011,416 uploaded / 14,048,893 readback bytes per frame; inclusive submit/wait 11.924 s, retirement 8.102 s, materialization 11.572 s | Nested scopes and differing windows; do not sum or infer causal speedup |
| Compatible-source Vulkan c7e065d1, SHA256 7a9811b94ab1b5da3dade41c7af5720c0916e064b10d66082b09e159ca4dc724 | Saved tagged VideoOut, queue/PM4, BufferCache, TextureCache, MemoryTracker, transfers and GPU detile attribution | Use PINNED_PROBE.json; capped guest FPS does not prove zero overhead; clocks uncalibrated; differs from installed974-dirty |
| Installed DLSS/FG 974-dirty, SHA256 7d350796bdd27515f85951226fed73a7167487f0eb5b590e1d0cfb2504eab773 | HUD/collector and XInput controls observed in prior sessions | Base/pre-FG guest FPS and independent displayed-frame rate not measured |

Raw sets remain at `F:\PS4Dev\handoff\covered-envelopes-20261012`,
`performance-recovery-20261011`, `high-memory-20261011`, `gxworker-20261011`,
`vulkan-live-monitor`, `vulkan-reference-profiler-20261011`, `compute-dispatch-cycle`,
`resource-cycle`, `migration-validation` and other paths in the catalogue.
`important-evidence-index.json` separately indexes crash evidence, XInput,
monitor programs and run metadata. No old results were deleted or repeated.

## F. Latest verification

- Core 237: `point-capture-reader-core-237.log`, wrapper
  `command-20261013-025120-27688.log`, 5.381 s, passed C++ compile/link.
- Checks 190: `point-capture-reader-checks-190.log`, wrapper
  `command-20261013-025120-107444.log`, 4.613 s, passed compile/link;
  existing deprecated warnings retained. These are build ordinals, not numbers
  of executed regression tests.
- Before final typed-reader patch, core 236/checks189, all ten full-core
  fixtures compiled/linked; 54 ordinary chains and six capture producer +
  separately compiled fragment chains signed offline. No device was created.
- Group125 producer proof passed five modes: one ES atomic, one GS atomic,
  original loops/slot arithmetic, zero hardware emissions, all original values.
- Group126 audit passed before final typed-reader patch, wrapper
  `command-20261013-024807-53592.log`, 21.585 s; 1,501 associations/13 mappings/
  zero closures. No final audit rerun during emergency preservation.
- Failed: raw capture-FS SRV expectation. Saved failure/old artifacts are retained.
- Not run: typed-reader signed proof, replay runtime, current regression
  execution/GPU validation, Bloodborne/FPS/gameplay/stability. Historical suites
  220/220 and 221/221 are evidence for their old exact builds only.

Logs/artifacts/scripts from this work remain under
`C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work`;
raw command logs under `F:\Codex-Shared\devtools\logs`.
194 continuation scripts/logs/proofs were also copied to external
`full-port-emergency-20261013\continuation-tools`.

## G. Next specific implementation

1. Rebuild only the stale gcn_winding CPU fixture against core237. Regenerate
   capture fragment artifacts; verify signed t0/space33 and correct original
   source/channel word addresses. Update `verify-captured-fragment.py` and the
   group126 contract text to describe a typed SRV instead of readonly raw SSBO.
2. Implement generated replay VS/GS using `guest_point_shader.h`,
   `guest_shader_frontend.*`, `guest_pipeline_cache.*` and shared capture ABI.
   VS supplies SV_VertexID record ID; GS reads validity and original point fields,
   applies original transforms and emits position + flat record ID only.
3. Integrate capture/replay through `graphics_submission.cpp`,
   `guest_rasterizer.cpp`, `resource_bindings.*`, scheduler/allocator ownership:
   clear invalid slots, run original ES/GS once, UAV barrier/transition, replay
   with original PS/resources/attachments/depth/stencil/viewports/scissors.
   Do not replay guest side effects or mechanically remove waits.
4. Prove allocation/fence pressure and checked instance/indirect/restart/stream
   bounds. Never use draw.count as a safe tessellation amplification bound.
   Preserve original VS side-effect/cache frequency for indexed draws.
5. Finish required full-port integration gates before GPU/game validation;
   then minimal targeted regressions, existing isolated XInput scenario and
   completed-guest-frame comparison against saved compatible evidence. A failed
   old baseline must not be replaced by a fabricated matched comparison.

GraphicsRuntime layout remains authoritative: Mesa first36 bytes, flip mask36,
float4 viewport arrays offsets48/304/560, size816, CBV allocation1024.
Reuse Release Ninja `build-migration-core` / `build-migration-checks`, clang-cl
22.1.3 with existing MSVC19.51 ABI/VS18/SDK10.0.26100. Do not change compiler/ABI
or reconfigure trees to obtain navigation. `point-capture-replay-design.md` is
preserved in continuation-tools; do not rederive that design.

## H. Exact next action and recovery

On the existing machine/tree, **do not apply patches again**. First:

```powershell
Set-Location 'F:\PS4Dev\work\shadps4-dx12-src'
git rev-parse HEAD
git status --short
Get-Content -Raw 'F:\PS4Dev\handoff\full-port-emergency-20261013\CHECKPOINT_FINAL.json'
```

After verifying HEAD and reading this file, the first development command is:

```powershell
& 'F:\Codex-Shared\devtools\bin\codex-dev.cmd' run 'C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work\Compile-Port-Fixture.cmd' gcn_winding
```

This is compile-only, not a game/GPU run. If the original task directory is
unavailable, use the preserved continuation-tools copy; native paths are explicit.

Before-checkpoint snapshot directory:
`F:\PS4Dev\handoff\full-port-emergency-20261013` contains `staged.patch`,
`unstaged.patch`, `head-working.patch`, `index-before.bin`, full status/log/stat
outputs, `working-files-before.zip` and its per-file SHA256 manifest. Snapshot
contains 253 tracked-change paths and 155 new files, including deletions.

Final residual patches/status/untracked manifest and `RESTORE.md` in that same
directory describe exact restoration onto a **clean separate checkout at the
final checkpoint SHA**. Apply final staged patch with `--index`, then final
unstaged patch; restore only residual untracked entries from the archive.
Never extract the entire before-checkpoint archive over an existing tree or
replace its index blindly. New files now committed must not be reintroduced
from the old archive. Existing unknown changes remain in the current tree.

Normal upstream push is authorized by the user's persistent AGENTS preference;
checkpoint validation checks preservation, not semantic completion. Actual push
outcome/remote SHA is recorded in CHECKPOINT_FINAL.json, without force or merge.

## Saved builds, symbols and profiles

- Historical last game-verified DX12:
  `C:\Users\skei\AppData\Local\PS4Desktop\cores\dx12-20261012-ba3892f-covered\shadPS4-DX12-Core.exe`;
  SHA256 866e800e5f7f97fb7c3613697869883178091888ca82290d1aeec4567a26aa77.
- Latest installed experimental preview18:
  `C:\Users\skei\AppData\Local\PS4Desktop\cores\dx12-20261013-full-port-preview-519d8356217d\shadPS4-DX12-Core.exe`;
  SHA256 519d8356217d04c9b8c12251bbd1efc014ac4f86d8325e08d3eca7614a180f87.
  Matching profile: `C:\Users\skei\AppData\Local\PS4Desktop\core-profiles\dx12-20261013-full-port-preview-519d8356217d\native-profile`.
- Latest development core237, **not installed or game-verified**:
  `F:\PS4Dev\handoff\full-port-emergency-20261013\experimental-core-237\shadPS4-DX12-Core.exe`;
  SHA256 a639d91da0561b213cbfa7075fbbf6959b1ba560c15c12e67e92cfcfbc4b6d4a.
  Matching `shadPS4-DX12-Core.pdb`, `native-core.map`, Mesa and root runtime DLLs
  preserved alongside. Original build remains in `build-migration-core`.
- Compatible Vulkan reference build/profiles and exact executable identity:
  `F:\PS4Dev\handoff\performance-recovery-20261011\pinned-capture-build.json`,
  `vulkan-pinned-1`, `vulkan-pinned-trace-1`; profiler ModeB build also remains at
  `F:\PS4Dev\handoff\vulkan-reference-profiler-20261011\mode-b-build\shadps4.exe`.
  These are different references; do not interchange their hashes/results.
- Installed DLSS/FG: `F:\Apps\shadPS4-DLSS-DLAA-1.8.4\shadPS4.exe`;
  HUD `F:\PS4Dev\handoff\vulkan-live-monitor\program\LiveHud.exe`, monitor
  `Launch-LiveMonitor.ps1` / `live_metrics.py`, virtual XInput `xinput_control.py`
  in the same program directory. Their historical sessions remain alongside.
- Test seeds/saves: `F:\PS4Dev\handoff\coherence-20261011`,
  `F:\PS4Dev\handoff\save-source-snapshot`, historical run profile manifests,
  and installed per-core profiles. Preview18 copied/hash-checked 20 saves from
  the selected first preview95d72e06d1ac; originals/default/earlier builds kept.
  No originals or installed Vulkan were changed during checkpointing.

## Process cleanup

No session-owned game, developer core, feeder, profiler or trace collector
remained from the current full-port work. No virtual gamepad was created during
this work, so no owned pad needed resetting/releasing. PresentMon PIDs42552 and
80352 were observed with unknown ownership and deliberately left running.
No user game, original HUD or persistent monitor was stopped. Process inventory
is saved externally; do not infer ownership from executable names alone.

## I. Do not repeat without new evidence

- Original source audit, architecture mapping and 126-group inventory; use
  saved STATUS/contracts/inventory first, do not restart from the old chat.
- Vulkan installed-binary/source identity/HUD/FG audits and compatible-source
  queue/PM4/cache/transfer profiling. Installed974-dirty cannot inherit c7 results.
- Already diagnosed signature packing/free-slot/0xc0000409 cases; sparse packing
  and synthetic clip row fixes are saved. Reopen only on a new failing case.
- The readonly-SSBO experiment: confirmed UAV output; do not repeat unchanged.
- Capture ABI, byte-bound correction, Private pointer and wide fixture offset
  diagnoses. Original producer proofs are retained.
- Baseline A/A2 launch failures and frozen/coherence experiments. Do not remove
  fence/ownership waits, skip aliases or claim narrower GPU writes from totals.
- Old unvalidated GPU timestamp results; BENCHMARKS/STATE document their limits.
- New game/FPS/profile runs before the full-port implementation gate. Emergency
  handoff is not permission to run Bloodborne immediately.

## NEXT CODEX SESSION — START HERE

> Продолжи Skiters/ps4-dx12 с последнего checkpoint на ветке fix/mixed-cmask.
> Сначала прочитай F:\PS4Dev\work\shadps4-dx12-src\docs\codex-progress\SESSION_HANDOFF.md
> и проверь Git HEAD/status по CHECKPOINT_FINAL.json. Сохрани оставшиеся dirty
> изменения; на существующем дереве patches повторно не применять. Восстанови
> незавершённую работу группы126: свежая CPU-fixture gcn_winding, проверка typed
> R32_UINT SRV t0 space33 и 152-word адресации, затем C++ replay VS/GS и безопасный
> capture/replay runtime с allocation/clear/barriers/fences. Используй сохранённые
> Vulkan/DX12 измерения и доказательства, не повторяй завершённые исследования.
> Собери и проверь изменённые компоненты; после полного implementation gate
> проведи минимально необходимые игровые испытания через существующий виртуальный
> XInput в копии профиля. Сравни completed guest FPS с сохранённым baseline,
> учитывая его ограничения и провал A/A2. Не меняй установленный Vulkan,
> оригинальные сохранения или старые сборки. Продолжай с checkpoint, не с истории чата.
