# Benchmarks

## Active — high-memory baseline (comparison pending)

2026-10-11: diagnostic A, c31111c plus archived instrumentation and explicit
512-MiB cap, high-memory disabled. Same prior seed and stationary Hunter's Dream;
30 seconds warm-up after visible world, then 91.267 seconds capture.
54 new completed frames, 0.59167 FPS; median 1734.169 ms, P95 1834.414 ms.
No dropped frame events. 63,958 materializations, 21,329,256,844 materialized bytes,
852,849 cache hits; 4,912 new GPU backing allocations, 59,046 pool reuses.
35,837 envelope retirements and 1,318 other overlap retirements were captured;
this initial instrumentation did not count whole-cache DMA clears. The final
control build adds their separate counter. 1,518 large upload allocations confirm
reusable staging work. Top sampled span rematerialized 159 times / 3.04 GB.
Counters use the diagnostic window, whose boundaries differ slightly from frames.
This is a new baseline, not the historical F1–F3 aggregate. No high-memory result yet.



## Active — final F1–F3 comparison, 2026-10-11

RTX 5080, driver 32.0.16.1692; Release /O2; Bloodborne CUSA03173 1.09, Hunter's Dream.
Four successful measured runs use fresh copies of the same full profile, original
saved character and shader cache seed; internal 1280x800, same patches, stationary
original camera, partial render 0. Only the assertion NOP at 024B56D2 was removed
from test copies. General.home_dir points into each isolated copy.
SHADPS4_NATIVE_D3D12_VALIDATION and SHADPS4_API_TRACE are absent in child environments;
startup/page-fault traces are absent too. Bounded diagnostics/frame capture arm after
visible world loading and warm-up. No build/suite overlapped any game benchmark.
There was no experimental marker in the isolated run roots; installed roots were untouched.

| Run | Seconds | New completed frames | FPS | Median ms | P95 ms |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 98.844 | 60 | 0.6070 | 1610.690 | 1703.886 |
| baseline-3 | 93.528 | 60 | 0.6415 | 1584.199 | 1650.093 |
| after-1 | 99.318 | 64 | 0.6444 | 1522.374 | 1589.832 |
| after-2 | 94.715 | 63 | 0.6652 | 1491.489 | 1580.313 |


Pooled completed-frame rate: 0.6238 -> 0.6545 FPS (+4.93%).
This is a modest observed improvement in two runs per version, with no statistical
significance claim. Neither build is playable. No 1% low claim from these small samples.
An additional baseline-2 attempt crashed before measurement (c0000096,
eboot+0x1196254); all successful windows and both fixed runs had no native exception
markers. Zero dropped trace events. Created/Present-queued counts are recorded separately
in each measurement.json and are not substituted for completed FPS.

| Run | Snapshot spans GB | Watch establishment GB | Readback GB | Upload GB | Materialized GB |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 220.238 | 0.000 | 7.454 | 26.403 | 26.391 |
| baseline-3 | 210.789 | 0.000 | 7.043 | 26.636 | 26.621 |
| after-1 | 106.981 | 75.509 | 1.727 | 26.722 | 26.699 |
| after-2 | 107.824 | 79.147 | 1.732 | 26.415 | 26.395 |


Readback per completed frame: 120.808 -> 27.238 MB
(-77.45%). Fixed commit bytes equal fixed readback bytes in each window.
The original snapshot counter records validation spans, including absent snapshots;
it is not a hardware count of bytes touched by libc memcmp. Include the new watch
establishment column when assessing total comparison work; do not call the first
column alone a 50% reduction. Fresh uploads now retain their exact staged version,
so the old/new changed-vs-unchanged call counts also have different meanings.
New fixed counters: requested copy 75.238/75.134 MB, destination union same in these
world windows; new total dirty union 1713.877/1732.287 MB includes conservative shader writes.
Watch upgrades 1159/1205, watch hits 26812/26714; establish-changed fallbacks 9/10.
GPU validations 1747736/1751272 are distinct from CPU validation/reconcile calls
344761/346824. Zero A/B commits before CPU access is proved by the production fixture,
not by claiming zero commits for the entire game. Unsupported image aliases still reconcile.
Finish reasons include batch-limit 186/189 and CPU-visible dirty 143/143; other actual
CPU consumers retain their existing waits. Full counters are in measurement.json.

| Run | Snapshot compare s | Materialization s | Overlap retirement s | CPU writeback s | Fence wait s |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 29.949 | 10.289 | 5.457 | 8.989 | 4.019 |
| baseline-3 | 27.882 | 10.210 | 5.238 | 8.622 | 3.832 |
| after-1 | 19.042 | 21.090 | 6.610 | 7.675 | 3.622 |
| after-2 | 19.246 | 20.894 | 6.874 | 7.702 | 3.694 |


CPU wall stage totals are inclusive/nested; do not sum them into sequential frame time.
Snapshot capture now occurs during materialization, so that stage is not directly
comparable in isolation. Diagnostics use 2-second windows; their start/stop boundaries
differ slightly from the frame marker (sampled ~500 ms). Logs are selected by the
actual child PID, not historical logs copied with the profile; capture remained below
the 8192-record budget. Fence polling timestamps have approximately 2 ms granularity.

Camera and movement response observed outside the measurement windows; input-to-photon
latency was not measured. Same geometry retained, original color/edge artifacts remain.
No new visible regression identified; no pixel-correctness guarantee. The remaining
~26 GB materialized/uploaded per window and snapshot/watch work warrant bounded
attribution of request-range churn, not an automatic F4/F5 redesign.

Evidence: F:/PS4Dev/handoff/coherence-20261011 (CSV, PID/process paths, per-run JSON,
screenshots, test/build logs, seed manifest, originals-verification.json, final-verification.json).
Original ab1bf1a exe archived: 4d5c6944e6fd0594ace84a45abe37ade9365510edc37f5caad46321c0f8d8097.
Instrumented baseline: 3d14e00 plus archived instrumentation-only diff/header;
exe a0e9a6f92bcd5f3e5cb9cc7c7f489b132c0967a48b6a19902ff5d45b0f2e44db.
Fixed clean source 67d0ddc2df0294c9839f769821e7414681d43661;
exe c921b7e819a9c0d557ee23dae5a7f99075776f9a8b7e4f881c0d7e4705b1ccc5.
Tested launcher: F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd;
core: F:/PS4Dev/handoff/coherence-20261011/after-2/core/shadPS4-DX12-Core.exe.
Diagnostics=1 with capture marker absent on normal launch; flags are set only in
the launcher child process. The final game was closed deliberately after control checks.
Native suite 211/211, 67.40 s. No push; original installed core and 20 saves unchanged.

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
