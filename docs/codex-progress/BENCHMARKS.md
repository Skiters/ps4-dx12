# Benchmarks

## 2026-10-11 reconciliation recursion checkpoint

Baseline `ee765f4`, forced rebuilt core; fixed core uses request-local reconciliation.
Regression fails before and passes after; native suite 208/208 in 96.62s with Debug Layer.
This duration is validation, not performance evidence; the game overlapped part of it.
Baseline loading window at t=153.615 (~2s): 523 draws, 233 dispatches, 634 direct and
29 copy submissions; 1,279 buffer misses, 692,477,504 materialized bytes,
2,559,965,256 snapshot bytes, 389,699,720 readback bytes; fence waits 114.473ms/1,267 calls.
Fixed final window: snapshot comparison 605.094ms, materialization 182.217ms,
overlap retirement 108.483ms, snapshot upload 162.979ms. These are nested stage totals,
not identical-scene comparisons or frame times. No before/after full-frame result exists.
Exclude debugger pauses; compute timestamps do not cover complete GPU frames.
See FULL_FRAME_PROFILE.md for evidence and limitations.

## Integration checkpoint 2026-10-11

2026-10-11: native-buffer-range-lookup-test, 1,024 resident disjoint mappings,
10,000 interior lookups, Debug Layer on: CPU wall total 26.789 -> 0.366 ms.
One sample each; no percentiles or FPS claim. Finish/barriers/ownership unchanged.
Full native suite 208/208 (60.06 s), core build passed. Integration profiles/stages
differed and still failed: no controlled frame-time or transfer/submission delta.

## Parity correctness checkpoints

PARITY-E01: core build succeeds; native suite 206/206, 64.75 seconds.
PARITY-A01: core build succeeds; native suite 207/207, 54.09 seconds.
These are validation durations, not game performance benchmarks. No new CPU/GPU frame
measurements or FPS comparison were performed for these correctness changes.

## Correction: GPU timing validity at 774d095

The parity audit found that CollectTimestamps could consume a recording context using its
old completed fence (or zero on first use). A regression reproduced this before the fix.
The historical 0.0064 ms GPU number below is therefore UNVERIFIED and must not be used to
rule out a GPU bottleneck. CPU timing and submission counters have a separate mechanism.
No replacement performance measurement is claimed in this correctness cycle.

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
