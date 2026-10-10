# Checkpoints

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
