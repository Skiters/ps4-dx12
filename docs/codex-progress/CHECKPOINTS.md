# Checkpoints

## Current packed-float/RGB32 checkpoint - 2026-10-15

SESSION_HANDOFF.md's top section is authoritative; older next-task entries are historical.
06536f2 and6ded358 are pushed: exact packed-float2/4/8-sample transfers with actual cast lists,
readonly RGB9E5 physical ownership, and readonly RGB32 guest12/host16 transfer with separate
independent-image retention. Page/backing/fence/physical-domain guards remain. Source-reviewed
only, NOT compiled/installed/run. All129 groups remain partial/open. Preview24 stays installed
unchanged; no new FPS or game result. User execution gate remains active.
Evidence and final HEAD/remote/residual recovery: F:/PS4Dev/handoff/packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Next source task: CMASK6/7 selected +INF conversion in DecodeGuestColorClear; see
rgb32/NEXT_SOURCE_TASK.md. Finite RTZ is done; NaN encoding and format34 clear stay open.



## Current packed-MSAA source checkpoint - 2026-10-15

SESSION_HANDOFF.md's packed-MSAA section is authoritative; older sections are historical.
Source1f43d43 adds finite CMASK6/7 deterministic RTZ,59fdc53 adds packed MSAA independent
versions/retained signed10 words and nonzero packed/sRGB clears; fe51ce4 extends
native sampled16-18 and retained expanded19. Source review only: NOT built/installed/run.
All129 contracts remain partial/open. Preview24 EXE70eeeff04e89 remains installed unchanged;
no new FPS/test/game result. The user test/GPU/game/UI gate remains active.
Evidence/provenance: F:/PS4Dev/handoff/packed-msaa-20261015; final HEAD/remote/residual
recovery hashes are in CHECKPOINT_FINAL.json. Broad dirty integration and22 staged
network deletions remain preserved. Next source task: exact packed-float independent
MSAA format6/34 with actual native castable-format contracts, then existing partial-alias
queue. Do not infer a missing Vulkan feature from its intentional whole-MSAA GC skip.


## Current source checkpoint - 2026-10-15, after preview24

SESSION_HANDOFF.md's complete-port section is authoritative. Source commits35b5de4,
d7d9064,440dd53,ad6808e and e359447 are pushed: stable WAIT_REG_MEM operands, shared typed/raw CMASK clears,
8/16/32-bit independent MSAA transfers with scratch pressure retirement/recreation,
pinned color-write-mask mapping, exact packed-float CMASK6/7, raw RGBA8 MSAA transfer
and nonzero clear with retained resources/fences; stale storage_retired reset on conversion.
Current changes are NOT compiled/installed/run.
Preview24 remains the latest installed executable; no new game/FPS or test results.
The user execution gate remains active. Evidence and pending source follow-ups:
F:/PS4Dev/handoff/complete-port-20261015. Preserve the broad dirty integration and
22 pre-existing staged deletions. Existing129 contract groups remain partial/open.
Next source task: independent packed10/packed16 MSAA reinterpretation through existing
transfer/tiler contracts, with raw guest words/masks and private padding preserved.
Final HEAD/upstream and residual hashes: complete-port-20261015/CHECKPOINT_FINAL.json.


## Latest package — 2026-10-15

Preview24 is built and installed at the user's subsequent request: source a3f1d5e plus
packaged dirty patch, EXE SHA256 70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Release compile/link and static package/discovery checks succeeded. No runtime tests,
game or desktop automation; no new FPS. Selected core/settings and originals preserved.
See SESSION_HANDOFF.md and eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Earlier source-only compilation/install statements below are historical.

## Active continuation — 2026-10-15: source work, execution deferred

SESSION_HANDOFF.md is authoritative. The user stopped ALL test/game execution and
desktop automation until the full transfer is ready for final testing. New backing-write,
PM4 completion, culling, R32 MSAA, shared mixed/uniform CMASK GPU clears and fence changes
are WIP, source-reviewed only; no build/tests after that instruction. Historical326/346
and subsequent targeted results are unchanged. Full parity/world correctness remain open.
Original22 staged deletions retained; source/provenance at
F:/PS4Dev/handoff/eop-continuation-20261015. CMASK source commit 479064d93bd2.
Next: exact raw CB clear-word mapping for more numeric formats/component swaps;
then remaining MSAA/alias/cache contracts. Preserve user execution gate; no new audit.

Source WIP commits:756359c0a410,9960b080783a,f1e68d4,29540d917543,479064d93bd2. Final HEAD/upstream and archive hashes are recorded in eop-continuation-20261015/CHECKPOINT_FINAL.json.


## Active black-frame continuation — 2026-10-14

SESSION_HANDOFF.md active section is authoritative; older priorities/results below are historical.
Source fixes e267eb8, capture metadata24bf8c0, producer/ROP capture 1fb65f7b10fc;
prior integration WIP5b6f47e. All pushed to origin/fix/mixed-cmask, without22 pre-existing staged network deletions.
Stage24/pipeline4/metadata34. Menus recovered; final world remains fog/sky; G-buffer contains player/environment.
No accepted world controls, FPS or speedup. Full suite326/346 (native211/229) before final depth/fixture changes;
three later targeted passes are separate, not a new full-suite result. All128 contract groups remain partial/open.
Preview23 dx12-20261014-full-port-preview-bde5503581e2, SHA256 bde5503581e2178aa854703d9ec6823287d6b9ea243a61b41bc8e2f767ace760;76 runtime hashes/20 copied saves verified.
Previous selected version/settings, saves and installed Vulkan preserved. Data/code recovery: F:/PS4Dev/handoff/black-continuation-20261014.
AvPlayer source commit952234f517d5: old queue AV/new atomic queue PASS; latest game failed a
separate EventWriteEop backing-write assertion before world/ROP capture. No accepted latest-game milestone.
Current first blocker: EventWriteEop::SignalFence -> Core::Memory::TryWriteBacking failure (liverpool.cpp:705).
Then actual scene-color attachment producers/ROP before blur; depth is finite0.98016..1.
Do not blame PS8c08444c solely: its almost-black bloom input can preserve pre-existing fogged destination.


## Packed-gap checkpoint — 2026-10-13 evening

Source3f1249b pushed; Stage24/metadata33. Current full suite324/344, native209/227; final finite packed12/12 PASS. Preview21 SHA25ca1dee9a3d0c9348bc8dba01850e67604476f6ed2bc54c1cc1b5644fee2872. One123.8697s game run exited0, original20 saves unchanged, pad neutral/removed. Two engine captures black:2794 completed flips in93.101581s (~30/s) are NOT gameplay FPS. No matched performance comparison. See SESSION_HANDOFF and F:/PS4Dev/handoff/packed-gaps-20261013 for exact evidence, failures and next source-boundary task.

## Active continuation checkpoint — 2026-10-13

[SESSION_HANDOFF.md](SESSION_HANDOFF.md) is authoritative. Source WIP `0a3571a4b76bf1a6683a47f1ee409fe2555614a3`
adds production ordered point capture/replay and placed MSAA color-transfer initialization.
Stage23/pipeline4/metadata-v32. Registry128 partial groups; no new overall audit or semantic closure.
Six offline chains and864 point GPU cases pass; complete native suite before final MSAA
fix174/227,53fail. Later targeted MSAA successes do not make it green. Current Bloodborne
world/control/image/FPS not verified (black window). Continue EmitImageWrite LOD assertion
with native_packed_16_msaa, then packed/alias correctness and owned-image waits.
Latest human request authorizes incremental GPU/game checks; old emergency/test gates below are historical.
Source/code/diagnostics/build recovery: F:/PS4Dev/handoff/point-capture-20261013.

Targeted post-init backing:46/48 passed,208.566s; two format19 RT capability failures.
Preview19 installed: dx12-20261013-full-port-preview-34d785e8b1dc,76 runtime hashes/20 copied saves verified, previous selection preserved.

## 2026-10-13 — emergency full-port session handoff

Authoritative recovery: [SESSION_HANDOFF.md](SESSION_HANDOFF.md).
Base e84e31133320830db6268f3beefe9ddda1fb08cc; source WIP
c0aa3e4a6fb2de072b29ffa4d0682d5accd9c292,
`wip(dx12): preserve current performance development state` (19 known files).
The following documentation commit is named
`chore(dx12): checkpoint development state for next Codex session`;
its actual SHA/status/upstream outcome are external in CHECKPOINT_FINAL.json.

Full before-state: 253 tracked-change paths/155 new paths, staged+unstaged binary
patches, original index and SHA256 source ZIP at
`F:/PS4Dev/handoff/full-port-emergency-20261013`. Other changes are deliberately
left outside the WIP because broad pre-existing ownership was not re-established.
Final residual patches/manifests and RESTORE.md preserve their exact restoration.
Latest core237/PDB/MAP/dependencies are copied there; preview18/default/older cores,
installed Vulkan and original saves remain. Group126 typed SRV reader C++ links;
shader proof/replay runtime are pending. No completion/FPS improvement claim.
No emergency build, game launch or profiling; unknown PresentMon processes retained.

## Final high-memory verification — 2026-10-11

Initial c31111c -> 7a550c9 residency/pools -> 9527c90 bounded overlap lookups.
Final source 9527c901a50256016072c4b67464946f78061ab1; all four runs use SHA256 db7e55e2e8f070bf3739a2e8aa50bb8b0f67cd2b393cc34a19cb25ccabcc44df.
216/216 final high-memory tests. Same-scene final FPS A/B/C/D:
0.6440 / 0.8400 / 0.7505 / 0.8078; C/A +16.53% observed,
materialization per completed frame -96.91%. One run/mode; no significance claim.
No new visible regression identified; old artifacts remain. Standalone C dev build,
original installation and all 20 saves preserved. Raw evidence/report outside Git.


## 2026-10-11 — bounded residency lookups after measured B run

Parent 7a550c9. B reduced materialization ~97% per completed frame but not average
FPS. Retained mappings made full-map validation scans visible. FirstOverlapping
now bounds ReleaseCpuBufferWatches, NotifyCpuWrite and GPU-owned overlap queries
used by streaming/arena validation. Disjoint interval and containing predecessor
semantics are preserved. Regression fails before (10.24 million candidates), passes
after (10 thousand); 27.958 -> 0.353 ms in the component sample. Final game comparison
remains outstanding; index optimization is not included in this package.

## 2026-10-11 — high-memory implementation checkpoint

- Parent c31111c; single-agent work, dedicated dev profiles and immutable save seed.
- Configurable DXGI-aware cache/arena budget; no upfront allocation, pressure trimming
  respects both completed fences and externally held allocation leases.
- Fixed address-order hot eviction and 64-MiB DMA whole-cache clear in experimental mode.
  Both regressions fail before and pass after (test-before.log / test-after.log).
- High-memory full suite first exposed 160-MiB buffers wasting 256-MiB heaps;
  compact large heaps fix it. suite-high-memory-fixed.log: 215/215, 70.88 s.
- Real GPU-gated eviction test, snapshot/staging content verification, PRT edge,
  zero-budget/live-lease checks added; all previous tests retained.
- Game performance comparison pending; ordinary diagnostic A is archived with its
  source diff, new header, executable hash and per-frame/counter evidence.



## 2026-10-11 — F1–F3 complete, Release world comparison

- Parent 3d14e008aa4313abce0e1bf006d06089d02d2964; implementation 67d0ddc2df0294c9839f769821e7414681d43661.
- F1: CopyBufferBatch passes BufferWrite footprints separately from shader resource envelopes;
  default unknown shader writes remain conservative. Previous dirty union is retained.
- F2: PageWatch::ChangeMode transfers READONLY/NOACCESS under its lock, advances generation,
  preserves retired intervals and never introduces a writable gap. Commit downgrade is
  also protected. Upload snapshots use the exact staged version in cached CPU memory;
  watch establishment validates after protection is installed. Failed commit keeps ownership.
- F3: ValidateGpuBacking checks mapping, committed backing and guest access rights independently
  of CPU reconciliation. GuestBufferAccess is shared production code, not a copied fixture override.
  GuestRasterizer prepares image aliases separately; real CPU reads/uploads/control records
  and unsupported buffer/image conversions retain reconciliation and fences.
- Before -> after: disjoint F1 12288 -> 64 bytes, overlapping union 48 bytes;
  unknown prior writable envelope remains 12288. F2 compared span 4096 -> 0;
  resident A->B commits before CPU 1 -> 0. Logs: f1-before.log, f23-before.log, package.log.
- Production fixture: real CPU stores, 16 concurrent watch transfers, GPU chain/scratch,
  partial/page-edge aliases, unrelated CPU-visible dirty buffer, failed commit/retry,
  rights rejection, PRT hole and unmap/remap. Existing queued faults/rearm/retired-history
  tests remain. Old physical-alias fixture now sends Core's committed mapping notification.
- Native suite 211/211 in 67.40 s; suite-verified.log. D3D12 Debug Layer targeted tests pass.
- Bounded opt-in frame trace excludes blank/repeated frames and observes completion of
  each immutable frame's copy fence (including guest queue dependencies), with 2 ms polling.
  Created / PM4-ready / fence-complete / Present-queued are separate. Queueing is not scanout.
- Same-scene baseline and fixed Release each have two measured world runs; see BENCHMARKS.md.
  The baseline's extra failed loading attempt is retained, not silently discarded.
- Verified launcher: F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd;
  actual child core in its core/ directory, implementation 67d0ddc, clean at build,
  SHA256 c921b7e819a9c0d557ee23dae5a7f99075776f9a8b7e4f881c0d7e4705b1ccc5.
- Only local commits. Original installed executable and 20 save files verified unchanged.

## 2026-10-11 — recursive buffer reconciliation

- Parent: `ee765f40925fbe7c32aa3ede718161202e6a2a78`, branch `fix/mixed-cmask`.
- Output: containing commit (`git log -1 --format=%H -- docs/codex-progress/FULL_FRAME_PROFILE.md`).
- Proven cause: an outer nonempty resolving_buffers_ made unrelated CheckBacking
  requests call Finish recursively; 1,943 repeated sequences exhausted the worker stack.
- Change: Rasterizer::ResolveGpuBuffers tracks request-local new overlaps; scope cleanup
  preserves outer state and releases this request's entries after success or exception.
- Regression: actual GPU copies, exact bytes/padding, nested backing checks, commit failure
  and retry. Old code fails; fixed code passes. Core built; native suite 208/208, 96.62s.
- Game: original saved-character profile copies, rebuilt baseline/fixed binaries, partial 0.
  Fixed run exits at GXWorker:1 `eboot+0x26b2b0b`; no world/FPS improvement established.
- Evidence, executable hashes and timing limits: FULL_FRAME_PROFILE.md.
- Next: capture original GXWorker fault context and PageWatch ownership/protection.
- User authorized commit/push after verified changes; remote delivery is verified separately.

## Integration checkpoint 2026-10-11

2026-10-11 — parent a824f9f65668d61080d2e90488435589efcd4f90, clean initial tree.
Subject: perf(dx12): bound observed guest buffer reconciliation lookups.
Live GPU-worker samples at ~10,831 mappings motivated lower_bound/predecessor lookup.
New regression covers exclusive boundaries, ownership and preserved data; 208/208
native tests and core build passed. Microbenchmark 26.789 -> 0.366 ms / 10,000 calls.
Three integration runs documented in GAMEPLAY_BASELINE.md; no world-entry improvement.
Commit identity is this entry's containing commit (git log); no push performed.

## PARITY-A01 — 2026-10-10 Vulkan draw-filter precedence

- Input SHA: `e43ba4d`; output: commit containing this entry
  (`git log -1 --format=%H -- native-port/draw_filter_test.cpp`).
- Epic: A. Upstream `Vulkan::Rasterizer::FilterDraw` at the pinned reference revision.
- Adaptation: special color clear/decompress/resolve remains before primitive rejection;
  None now stops GuestRasterizer::DrawInternal before depth/stencil override-copy.
- C++ changes: guest_graphics_state.h, guest_rasterizer.cpp, draw_filter_test.cpp.
- Regression: seven production-classifier cases, including None with stale depth/stencil
  override flags and special CB operations. This is not a full GuestRasterizer execution test.
- Core compiled and linked; full native suite 207/207 passed in 54.09 seconds.
- No game run, new milestone or performance result claimed. End-to-end memory-side-effect
  fixture and deterministic Vulkan/DX12 command comparison remain the next priority.
- Local commit subject: `port(dx12): match Vulkan draw filter precedence`; no push.

## PARITY-E01 — 2026-10-10 scheduler ownership audit

- Input SHA: `774d0951ccb40caf47921856ede9c2446203699e`.
- Output SHA: the commit containing this entry (`git log -1 --format=%H -- native-port/scheduler_lifetime_test.cpp`).
- Epic: E; initial source map for A/B/C/D/F.
- Reference: upstream `0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa`,
  `Scheduler::UploadCommandBuffer/SubmitExecution` upload-before-primary and timeline ownership.
- DX12: `CommandScheduler::CollectTimestamps`, `AcquireContext`, `SubmitCopy`;
  recording callbacks in Rasterizer, GuestRasterizer and GuestTextureCache now return void.
- Confirmed defect: unsubmitted query data was read with an old/zero fence. Regression failed
  at baseline, passed after guarding submission state and collecting before oldest-context reuse.
- No caller used SubmitCopy's zero result; removing it makes the actual completion contract
  explicit without claiming that an observed early-free crash was fixed.
- New test: native_scheduler_lifetime, open/reused timestamp contexts and exact two-copy
  readback through the next direct fence. D3D12 Debug Layer enabled.
- Full native build and tests: 206/206 passed, 64.75 seconds.
- Previous GPU timing inference withdrawn; no new FPS or Bloodborne milestone claim.
- Remaining: draw-filter ordering regression, true command differential replay, full parity audit.
- Next priority: EPIC A DrawInternal PrimitiveType::None before override-copy behavior.
- Push: not performed in this cycle.

## 2026-10-10 — compute copy reconciliation

- Baseline: `01c0374` (`Stabilize partially overlapping DX12 buffer mappings`).
- Scope: opt-in dispatch-stage distributions and counters; page-aligned copy materialization;
  deferred reconciliation for GPU-owned copy destinations; coherence regression coverage.
- Result: copy-shader HLE CPU wall time decreased 5.5% in the controlled steady-state sample.
- Validation: targeted 2/2; full native 205/205; no D3D12 validation errors in the 66-second run.
- Local commit subject: `perf(dx12): reduce measured compute dispatch overhead`.
- Remote push: not performed.

## 2026-10-10 — resource timing and upload submission batching

- Baseline: `5b0f555` (`perf(dx12): reduce measured compute dispatch overhead`).
- Scope: asynchronous D3D12 compute timestamps; buffer preparation/coherence counters;
  deferred-reconciliation coverage; copy-queue upload batching with direct-queue ordering.
- Result: copy submissions decreased 4,046 to 2,678 (-33.8%); resource-binding CPU wall
  average decreased 7.9%; whole-dispatch average was effectively unchanged.
- Validation: targeted 3/3; full native 205/205; no D3D12 validation errors in the game run.
- Local commit subject: `perf(dx12): batch buffer upload copy submissions`.
- Remote push: not performed.

## CMASK-M01 — 2026-10-11

- Input: `7984fed0e789e4ea48af2487c7d1095826aa2c3c`.
- Changes: `GuestColorClearPending`/`MaterializeGuestColorClear` now build a per-layer mixed CMASK plan; zero tiles use a GCN AddrLib-derived pipe-interleaved address, F tiles remain untouched, materialized nibbles become F. `GuestRasterizer::MaterializeColorClear` preserves the original exception and adds only opt-in bounded capture diagnostics.
- Regression: `native_guest_cmask_fast_clear` exact changed/unchanged tile checks, idempotence, untouched layer and actual D3D12 readback; full native suite passes 208/208.
- Core build passes. Bloodborne was tested from cloned/rebased profiles; the audio guest assertion recurred before an additional CMASK-stage proof. No PASS game claim.
## GXWORKER-P01 — 2026-10-11

Final verification addendum: committed code `ab1bf1a47e0ce2a0a3cd8fa528285dcd8a58b066`
reaches world/control beyond 120 s using the delivered dev launcher. The debugger
observes 18 handled original-instruction faults (3 on GXWorker:1), then detaches.
LEVEL 2 established. Pixel correctness and playable performance remain open.
See GXWORKER_PAGEWATCH.md and the external final-verification.json for exact final
duration and executable SHA256. This addendum changes documentation only.

- Input `40a8e683def56160ea110a2d5df1a48a8532c4ce`, branch fix/mixed-cmask.
- Original first-chance game context proves class C retired-range coverage loss;
  short predecessor hides large interval covering a pending valid guest write.
- PageWatch coalesces retirement history and handles newly rearmed generations;
  protection, physical backing reconciliation, fences and recursion guard stay enabled.
- COPY queue texture initial state is corrected after real Debug Layer error 1334.
- Real delayed CPU-fault GPU regression fails before, passes after with exact bytes;
  negative/lifetime/bounded-retry checks pass. Full native suite 208/208, 55.52 s.
- Partial fix reached world/input then crashed; complete fix precommit run reached
  a separate loading mutex wait. LEVEL 1 established; playable performance not claimed.
- GXWORKER_PAGEWATCH.md documents evidence and the final committed-build dev launcher.
  External release/verification manifests capture exact binary hash and launch result.
- Local commit only; no push. Original application and saves unchanged.
