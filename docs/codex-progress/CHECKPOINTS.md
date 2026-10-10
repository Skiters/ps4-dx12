# Checkpoints

## 2026-10-10 — compute copy reconciliation

- Baseline: `01c0374` (`Stabilize partially overlapping DX12 buffer mappings`).
- Scope: opt-in dispatch-stage distributions and counters; page-aligned copy materialization;
  deferred reconciliation for GPU-owned copy destinations; coherence regression coverage.
- Result: copy-shader HLE CPU wall time decreased 5.5% in the controlled steady-state sample.
- Validation: targeted 2/2; full native 205/205; no D3D12 validation errors in the 66-second run.
- Local commit subject: `perf(dx12): reduce measured compute dispatch overhead`.
- Remote push: not performed.
