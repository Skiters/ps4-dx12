# Checkpoints

## 2026-10-11 — complete coherence package, game verification pending

- Base: 3d14e008aa4313abce0e1bf006d06089d02d2964, fix/mixed-cmask.
- F1: explicit BufferWrite spans, conservative default shader writes, persistent dirty union.
- F2: READONLY/NOACCESS transfer under PageWatch lock, fresh generation, no writable gap;
  protected downgrade after successful commit; actual upload-version snapshots and safe watch establishment.
- F3: ValidateGpuBacking checks mapped/committed/right coverage, then separate image preparation;
  CPU control records, new uploads, unsupported aliases and actual faults remain coherent.
- Production GuestBufferAccess fixture covers unchanged watch, CPU stores, 16 concurrent
  transfers, GPU chains/scratch/partial aliases, prior dirty writes, unrelated CPU-visible
  dirty storage, failed commit/retry, rights rejection, PRT hole, unmap/remap.
- Existing GPU PageWatch fixture now supplies Core's committed mapping notification;
  all original assertions, rearm and queued-fault checks remain.
- 5 targeted tests pass (package.log); full native suite 211/211 in 67.40 s
  (suite-verified.log). Core compile/link also succeeded; rebuild from commit follows.
- Frame instrumentation is opt-in and bounded, arms after world loading, separates new
  guest flip creation, copy-fence completion and Present queueing. Poll resolution 2 ms;
  Present queueing is not physical scanout. No repeated/blank frames are counted.

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
