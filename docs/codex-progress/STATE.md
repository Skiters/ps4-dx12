# Native DX12 state

## Final — high-memory mode measured, 2026-10-11

Supersedes earlier pending sections. Source 9527c901a50256016072c4b67464946f78061ab1, clean tested core.
Final A/B/C/D completed FPS: 0.6440 / 0.8400 / 0.7505 / 0.8078.
Best observed mode B: +30.44% FPS; materialized bytes/completed frame
reduced 97.21%. Main 4-GiB mode C/A difference is +16.53%.
Full final high-memory suite 216/216. Default mode remains disabled; no installed
program replacement. See BENCHMARKS.md for measurements, limits and executable hash.
Old visual artifacts and unplayable speed remain. Original 20 save hashes match.


## Active — high-memory experiment, 2026-10-11

Intermediate B on 7a550c9 substantially reduces materialization but stays at 0.591 FPS.
The follow-up bounds full-map overlap checks identified in B; component regression
passes (10.24M -> 10k visited candidates). Rebenchmark the final core before claiming FPS.

Starting HEAD c31111c, branch fix/mixed-cmask. F1–F3 remain in place.
SHADPS4_DX12_MEMORY_PERF=1 enables lazy DXGI-budgeted buffer residency;
SHADPS4_DX12_BUFFER_BUDGET_MB accepts caps up to 8192 MiB (default 4096
when enabled). Mode off preserves the old adaptive policy unless an explicit
lower cap is supplied; the 512-MiB control is therefore explicit.
SHADPS4_DX12_CPU_POOL_MB bounds retained snapshot/upload pools (default 512 MiB).
Active snapshots and in-flight transfers are separate, required working memory.

Implemented: cold-first eviction, exact-sized large heaps, safe idle-heap trimming,
snapshot and large staging reuse; aligned DMA copies reuse the exact-footprint
batch path instead of clearing unrelated residency above the legacy 64-MiB cutoff.
No speculative expansion across PRT holes or new GPU-owned coalescing.
Budget reserves max(2 GiB, DXGI budget/8) after accounting for other process
resources. The physical arena cap includes 25% headroom over the requested
logical cache; a shortage reduces the effective target. Active bindings and
outstanding leases survive a reduced target; growth can fail explicitly if safe
reclamation cannot fit a required allocation. No limit is allocated at startup.

Component regressions reproduced both hot-buffer eviction and tiny-DMA global
cache destruction, then passed. High-memory full suite: 215/215, 70.88 s.
Its first attempt exposed large-heap fragmentation (214/215); fixed, not hidden.
Game comparison is IN PROGRESS; no high-memory FPS improvement claimed yet.
Evidence: F:/PS4Dev/handoff/high-memory-20261011. Installed program/saves unchanged.



## Active — F1–F3 implemented and measured, 2026-10-11

Supersedes historical next-priority sections below. Code commit `67d0ddc2df0294c9839f769821e7414681d43661`,
clean Release rebuilt and run twice. Native suite 211/211 in 67.40 s.
F1 exact copy footprints, F2 protected watch transfer and F3 GPU backing validation
are implemented and have failing-before/passing-after production regressions.
Two same-scene baseline runs: 0.6070/0.6415 completed FPS; fixed: 0.6444/0.6652.
Pooled observed increase 4.93%; readback per completed frame reduced
77.45%. This is a small observed gain with two runs per version,
not a statistical guarantee, and still far below playable performance.
World, camera and movement work; original color/edge artifacts remain. No new visible
regression identified, but correct pixels and input latency are not proven.
Baseline-2 failed during loading (c0000096, eboot+0x1196254); fixed runs had no native
exception markers. Debug Layer was used in native tests, not performance game runs.
Use `F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd`.
Core SHA256 `c921b7e819a9c0d557ee23dae5a7f99075776f9a8b7e4f881c0d7e4705b1ccc5`.
See BENCHMARKS.md and external final-verification.json for evidence and limits.
Original core and all 20 save files unchanged. No push. F4/F5 not redesigned.
The audit ZIP was unavailable; no candidate files are claimed to have been read.

## Final verification — world and control in committed ab1bf1a

Supersedes the precommit outcome below: the dev launcher reaches the saved world,
responds to camera/movement for over two minutes, and handles the original instruction's
faults (18 observed, 3 on GXWorker:1). LEVEL 2 confirmed; pixel correctness and playable
performance are not. Focus next on complete-frame attribution of buffer coherence.
The intermittent loading mutex wait did not recur; its cause remains unknown.
See GXWORKER_PAGEWATCH.md and external final-verification.json. No push performed.

## Current checkpoint — 2026-10-11 GXWorker retirement race

Supersedes the previous priority below. Original `eboot+0x26b2b0b` is confirmed
class C: a shorter retired watch hid an earlier covering interval while a real CPU
write exception was pending. Coalesced retirement history, generation-aware rearming,
and COPY queue texture state correction are implemented. Before/after GPU regressions
and 208/208 native suite pass. Complete-fix game run instead remained in loading,
with Game:Main waiting for a MOMainThread-owned mutex; no playable-world claim.
See GXWORKER_PAGEWATCH.md for evidence, limits and final dev-launch recipe.
Only local commits are authorized; do not push. Original installation/saves untouched.

## Current checkpoint — 2026-10-11 saved-world loading

From `ee765f4`: rebuilt core and tested copies of the original DX12 saved-character
profile (Skip Intro, partial render 0). Confirmed recursive ResolveGpuBuffers/Finish
writeback exhausted the GPU-worker stack. Request-local reconciliation now prevents
unrelated nested backing checks from restarting Finish; exception cleanup preserves
outer state. Baseline regression fails, fixed regression and all 208 native tests pass.
Fixed game run stopped at a separate GXWorker:1 write fault, `eboot+0x26b2b0b`, before
a world frame. No full-frame/FPS improvement established. Installed desktop core is
still the old binary; tested rebuilds are isolated. See FULL_FRAME_PROFILE.md for
hashes, stacks, timings and next operation. This supersedes older priorities below.

## Integration checkpoint 2026-10-11

2026-10-11 update: core builds; 208/208 tests pass. ResolveGpuBuffers now searches
only overlapping disjoint mappings (26.789 -> 0.366 ms / 10,000 test lookups).
No game FPS improvement established. Title/setup artifacts remain; guest audio-thread
fault repeats at eboot+0x20b56d2; separate seed-home run fails on mixed CMASK codes.
See GAMEPLAY_BASELINE.md; historical runtime claims below are not current gameplay proof.

Functional port checkpoint A01: GuestRasterizer now applies the Vulkan draw-filter ordering
through ClassifyGuestDraw. Fast clear/decompress and resolve keep priority; PrimitiveType::None
then exits before depth/stencil override-copy. The state classifier has seven regression cases.
No Bloodborne world-entry or FPS improvement is claimed from this source-level correctness fix.

Parity audit correction (2026-10-10): the historical GPU timings below are not validated.
An open command context could reuse its previous fence when collecting query results.
Collection now requires a closed submitted context; the regression reproduces the old
failure. See VULKAN_DX12_PARITY.md and PORTING_ROADMAP.md for the initial audit scope.

Updated 2026-10-10 from baseline `5b0f555`.

The native D3D12 backend now records bounded per-stage CPU wall-time distributions when
`SHADPS4_NATIVE_DIAGNOSTICS=1`. The two-second report includes calls, total, average,
maximum, P50, P95 and P99, plus counters for compute PSO/DXIL caches, descriptor pages,
GPU submissions and copy-shader HLE traffic. Diagnostics remain dormant otherwise.

Bloodborne's recognized copy shader was the largest measured compute-dispatch stage.
`Rasterizer::CopyBufferBatch` now materializes a 4 KiB page envelope when the complete
source and destination pages are validated. Exact `CopyBufferRegion` ranges are unchanged.
Whole page destinations can remain GPU-owned after submission and reconcile on their first
CPU access; PRT edges and partial/unprotected backing retain the eager `Finish()` path.

Confirmed in the controlled 66-second run: 1726 of 1727 copy-HLE dispatches used deferred
reconciliation, no DXIL recompilation occurred after disk-cache loading, and no D3D12 debug,
device-removal or native exception marker was logged.

Direct-list contexts now collect bounded D3D12 timestamp queries asynchronously when native
diagnostics are enabled. Results are read only after the context fence completes, so measuring
compute does not introduce a GPU wait. In the controlled sample the compute dispatch itself
averaged about 0.0064 ms; CPU resource preparation and coherence dominate instead.

Copy-queue uploads are accumulated until the consuming direct-list submission. One copy fence
and one direct-queue wait preserve upload-before-use ordering for the batch. The controlled
46-second comparison reduced copy submissions from 4,046 to 2,678 (-33.8%), while upload calls
and bytes remained effectively unchanged. Full native validation passes 205/205.
