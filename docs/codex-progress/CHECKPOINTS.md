# Checkpoints

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
