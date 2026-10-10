# Benchmarks

## 2026-10-10 compute dispatch cycle

Both runs used Bloodborne `CUSA03173`, the same profile seed cloned into separate directories,
the same executable location and arguments, `SHADPS4_NATIVE_DIAGNOSTICS=1`, and a 66-second
duration. Steady-state aggregation excludes reports before 6 seconds. Percentiles below are
the arithmetic mean of bounded per-window percentiles; total/average/max use all reported
steady-state calls.

| Dispatch stage | Baseline calls | Baseline avg ms | After calls | After avg ms | Change | Baseline/after mean P95 ms |
|---|---:|---:|---:|---:|---:|---:|
| copy-shader-hle | 6,422 | 4.8972 | 6,090 | 4.6275 | -5.5% | 38.197 / 36.305 |
| resource-upload-dispatch-writeback | 3,695 | 3.8074 | 3,503 | 4.0438 | +6.2% | 11.054 / 11.980 |
| shader-translation | 6,423 | 0.0031 | 6,091 | 0.0030 | -3.2% | 0.005 / 0.005 |
| pipeline-creation | 3,695 | 0.0005 | 3,503 | 0.0004 | within timer noise | 0.001 / 0.001 |
| image-operation-recognition | 6,423 | 0.0056 | 6,091 | 0.0054 | -3.6% | 0.040 / 0.038 |
| metadata-clear-recognition | 4,604 | 0.0002 | 4,366 | 0.0002 | within timer noise | 0.001 / 0.000 |

Copy-HLE counters after the fix: 1,727 recognized dispatches, 1,726 deferred, zero reported
eager reconciliations, and 4,315 validated page envelopes. Compute caches recorded 11 PSO
misses backed by 11 disk DXIL hits during startup, 3,559 PSO hits, and zero DXIL compilations.

No FPS result is reported because scene identity was not independently verified. Raw logs are
kept outside Git under `F:\PS4Dev\handoff\compute-dispatch-cycle`.

Validation: targeted metadata/compute tests 2/2 passed; full native suite 205/205 passed in
59.70 seconds. Tests enable the D3D12 Debug Layer, and the game-log scan found no D3D12 error,
corruption, device removal, native exception or dispatch failure.
