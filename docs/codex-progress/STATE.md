# Native DX12 state

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
