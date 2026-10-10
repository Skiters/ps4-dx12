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

## 2026-10-10 resource preparation and copy submission cycle

Both Bloodborne runs used cloned copies of one profile seed, identical arguments, a 46-second
duration, and diagnostics enabled. Aggregation excludes the first six seconds. This is a
controlled startup/load sample, not a fixed-camera FPS benchmark.

| Metric | Baseline | Batched copy uploads | Change |
|---|---:|---:|---:|
| Copy queue submissions | 4,046 | 2,678 | -33.8% |
| Direct queue submissions | 11,953 | 11,980 | +0.2% |
| Upload calls | 16,914 | 16,993 | +0.5% |
| Upload bytes | 5.254 GB | 5.204 GB | -1.0% |
| Resource bindings avg CPU wall | 2.1775 ms | 2.0052 ms | -7.9% |
| Buffer views avg CPU wall | 0.9203 ms | 0.8700 ms | -5.5% |
| Buffer reconcile avg CPU wall | 0.6725 ms | 0.6373 ms | -5.2% |
| Fence wait avg CPU wall | 0.5619 ms | 0.5174 ms | -7.9% |
| Whole dispatch avg CPU wall | 5.8671 ms | 5.8551 ms | -0.2% |
| Compute GPU execution avg | 0.0064 ms | 0.0064 ms | unchanged |

The stable upload volume and 4,056 recorded copy operations after batching show that work was
coalesced rather than omitted. No FPS result is reported. Raw logs remain outside Git under
`F:\PS4Dev\handoff\resource-cycle`.

Validation: targeted resource/metadata/wave32 tests 3/3 passed; full native suite 205/205
passed in 60.03 seconds. No D3D12 debug, device-removal, native-exception or dispatch-failure
marker occurred in the controlled game run.
