# Vulkan / native D3D12 parity audit

## Observed Vulkan session — 2026-10-11

The running Bloodborne v0.19.0 log identifies exact source `c7e065d1b415be16c23e260a21f1dd8bbfc4cb57`.
The [execution/coherence investigation](vulkan-session-20261011/RESEARCH.txt) records source-level DX12 gaps,
observed residency/detiler/pipeline activity, and implementation contracts. Supporting log excerpts,
101 passive telemetry samples, their summary, game configuration and hash manifest are preserved
in [vulkan-session-20261011](vulkan-session-20261011/manifest.json).
This session used Relaxed readback. It establishes neither per-operation costs nor a matched FPS comparison.
The older audit below uses a different reference revision and remains historical.


## Integration checkpoint 2026-10-11

2026-10-11: bounded ResolveGpuBuffers search is a tested local optimization, not
proof of full Vulkan regional tracking parity. CMASK is now an observed game blocker:
GuestColorClearPending/MaterializeGuestColorClear reject mixed slices; upstream
0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa EliminateFastClear uses IsMetaCleared/TouchMeta
and native clears. Mixed-tile addressing/coherent semantic tracking remains absent.
No paired Vulkan/DX12 replay completed; audio-thread fault is not proven renderer divergence.

Audit start: 2026-10-10. DX12 baseline: `774d0951ccb40caf47921856ede9c2446203699e`.
Upstream reference: `shadps4-emu/shadPS4@0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa`.
The local upstream directory is an extracted source archive, not a Git checkout.
The six inspected rasterizer/scheduler/buffer-cache/memory-tracker/texture-cache/page-manager
files were compared against that exact GitHub revision and match after newline normalization.
The dependency checkout at `74c07f5e97ced220404d4baa2f8cf652463bbaa4` is a different fork;
it is not used as an interchangeable upstream semantic reference.

This is an initial source-based map, NOT an exhaustive audit or a completed functional port.
No actual Bloodborne PM4 sequence has yet been captured and differentially replayed.
EQUIVALENT below is restricted to the named tested invariant, never an entire subsystem.

| Function / scope | Upstream algorithm | Native D3D12 implementation | Status / missing guarantee | Required evidence / adaptation | Bloodborne impact / effort |
|---|---|---|---|---|---|
| Direct draw | `vk_rasterizer::Draw`, FilterDraw, BindResources, GetDrawOffsets | `GuestRasterizer::DrawInternal`, state translation and native resource binding | UNVERIFIED: no same-input pixel comparison | PM4 fixture with offsets, instances, exact render readback | Possible world-entry relevance; medium |
| Draw filter ordering | `FilterDraw` rejects PrimitiveType::None before depth override copy | ClassifyGuestDraw now returns Skip before override-copy branch, preserving CB-operation priority | Fixed source-level mismatch; EQUIVALENT classification for the seven tested states | CPU state matrix passed; end-to-end GuestRasterizer memory-side-effect fixture remains needed | Occurrence in Bloodborne unknown; small |
| Indirect draw | GPU argument/count buffers and indirect commands | CPU-coherent argument snapshot followed by DrawInternal loop | PARTIAL: GPU residency/performance differs; pixel equivalence unverified | GPU-written args, count clamp, indexed offsets, self-modifying argument hazards | Unknown incidence; large |
| Indirect dispatch | BindResources then GPU indirect argument fetch | ReadMemory/Finish, CPU dimensions, direct Dispatch | UNVERIFIED: path also enters direct shader HLE | Compare GPU-written args and shader HLE eligibility against real execution | Potential sync cost; medium |
| Upload session ordering | `Scheduler::SubmitExecution` emits session.upload before session.primary, signals one timeline | Pending copy list, copy fence, direct queue wait, direct fence | EQUIVALENT only for tested two-copy visibility and copy-only completion | New native_scheduler_lifetime exact 8-byte readback; preserve resource leases | Critical lifetime invariant; checked |
| Query retirement | Vulkan submits ended sessions before completed-tick retirement | Context fence plus timestamp readback | INCORRECT at baseline: old fence can retire an open context; fixed this cycle | Regression fails before fix, passes after; invalidate old GPU timing claims | Profiling reliability; small |
| Copy submission result | Upload command recording carries no independent completed fence | SubmitCopy formerly returned zero for pending work | PARTIAL API contract corrected to void; no old caller consumed result | Completion remains next direct Submit fence; compiler checks all callers | No proven runtime failure from zero; small |
| Guest buffer dirty ranges | `MemoryTracker::ForEachUploadRange/ForEachDownloadRange`, `gpu_modified_ranges` intersection | Mapping dirty_ranges, snapshots, PageWatch, ResolveGpuBuffers/Finish | PARTIAL: DX12 already has dirty intervals, but reconciliation granularity differs | Attribute actual written/read intervals before porting regional state; preserve aliases and PRT | High-volume transfers plausible; large |
| Sparse buffers | ObtainBuffer obtains/residents arena blocks; sparse memory copies | Checked host spans and materialized D3D12 buffer ranges | UNVERIFIED equivalence at sparse edges | Hole/partial-page fixtures; retain original guards | Unknown blocker; large |
| Texture refresh | RefreshImage uses dirty flags, per-mip regions, separate GPU/CPU dirtiness | GuestTextureCache upload, detile, ownership and CPU snapshots | PARTIAL: broad existing format tests do not establish alias parity | Exact pixel tests across mip/layer aliases, CPU writes and GPU writes | Likely relevant; large |
| Fast clear / resolve | FilterDraw selects EliminateFastClear/Resolve; FmaskDecompress includes upstream TODO | MaterializeColorClear and ResolveColorTargets | PARTIAL; FMASK feature cannot be called fully supported upstream | Same metadata + sample patterns, exact pixel compare | Possible; medium/large |
| Page ownership | PageManager counts read/write watchers and updates protection | Native PageWatch and callbacks into buffer/image reconciliation | UNVERIFIED full equivalence | CPU partial writes, aliased GPU writers, pending-fence faults | Critical; large |
| Shader pipeline | Shared TranslateProgram and SPIR-V emitter | GuestShaderFrontend already calls shared TranslateProgram/EmitSPIRV, then DXIL | UNVERIFIED across complete GCN instruction set, not a missing shared frontend | Instruction/result fixtures, stage interface and atomics comparison | Critical; large |
| Blend / depth state | Vulkan pipeline/dynamic state | GuestGraphicsState, explicit unsupported blend combinations | PARTIAL; mixed constant-alpha/color restrictions are explicit | Port only demonstrated state combinations with GPU pixel tests | Occurrence unknown; medium |

Do not add totals of nested diagnostics. The earlier GPU compute average of 0.0064 ms
must be remeasured after the query ownership fix. CPU wall-time and submission counts
are separate measurements and are not invalidated by the GPU timestamp bug.

## Mixed CMASK checkpoint — 2026-10-11

DX12 adaptation added per-layer/per-tile CMASK planning in `guest_texture_cache.cpp`. It follows Vulkan's semantic model (tracked clear ownership, clear only when metadata says the tile is clear, retire the metadata state after materialization) while using AMD GCN AddrLib-style pipe-interleaved addresses for the native guest metadata plane. Vulkan itself tracks `IsMetaCleared`/`TouchMeta` and issues a native image clear; it does not decode arbitrary mixed bytes.
