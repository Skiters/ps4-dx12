# Vulkan / native D3D12 parity audit

## Active grouped writable BC view delta - 2026-10-16

Production WIP now groups selected writable UINT mips per native BC parent into one
maximum-prefix/full-layer child and one parent replacement. UAVs retain their selected mip
and layer subsets; array transitions use the child's actual mip count. This extends
single-view handling to multiple/duplicate writable slots and layer subsets. Multiple
disjoint parents/unrelated images are allowed; mixed overlapping readonly domains, buffers,
DMA and broader feedback remain open. Five meaningful GCN cases are authored/unrun.
Source review only: no build, runtime validation, complete group or new FPS evidence.
All129 groups remain partial/open. Exact continuation: active SESSION_HANDOFF.md section.

## Current BC block expansion coverage - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

| Contract | Production continuation | Acceptance |
|---|---|---|
| Pinned ExpandImage producer | Exact UINT64/128 I -> BC through private GPU snapshot | Source review only |
| Reverse/numeric interpretation | UINT block views and samecodec BC variants preserve raw bytes | Authored/unrun |
| Physical ownership | Canonical UINT geometry; actual BC remains readonly | Source review only |
| Source/history/failure | Shared selection predicate, pins, successful ownership before retirement | Source review only |
| Fallback isolation | Different codec keeps old I/new B; unsupported blocks fail explicitly | Corrected source finding |
| Multi-mip/linear/expanded/odd | Single-mip tiled nonexpanded scope only | Remaining adaptation |

Two additional real-GCN BC1/BC3 scenarios are authored only. No full BC/alias/cache equivalence
or game/performance acceptance is claimed. See SESSION_HANDOFF.md and saved block proof.



## Current MSAA depth/stencil transfer coverage - 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

| Contract | Production continuation | Acceptance |
|---|---|---|
| New placed MSAA depth | Full selected plane clear before upload shader draw | Source review only |
| Plane/layer preservation | One-layer DSV, single clear flag; partial larger targets retained | Authored/unrun |
| Stencil sample extraction | Unsigned SRV selects uint4 and retains G-to-X mapping | Authored/unrun |
| First use and exact contents | Fresh D16/D32, DS order, independent sample oracle, padding | Uncompiled/unrun |
| Generic partial fresh target | Initialization witness not implemented; current production initial upload is full | Open |

No full depth/MSAA/feedback/physical-alias equivalence or performance result is claimed.
See SESSION_HANDOFF.md, source proofs and msaa-depth-init-20261016 evidence; all earlier
PASS results below remain historical. No core was built, installed or selected.



## Current heap-pressure coverage - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
No configure/build/package/compiler/test/GPU/game/XInput/UI execution. All129 groups
remain partial/open; source/Git/hash reviews do not establish runtime correctness/FPS.

| Contract | Production continuation | Acceptance |
|---|---|---|
| Actual heap fitting | Reduced/remaining budget, typed exhaustion, proper MSAA heap alignment | Source review only |
| Cache recovery | Retired storage then eligible LRU/source reclaim, measurable retry progress | Authored/unrun |
| Recursive lifetime | Caller/victim pins and identity-based iterator reacquisition | Source review only |
| Idle heap pressure | SetBudget releases only wholly unleased excess commitment | Authored/unrun |
| Pinned pressure envelope | Compact linear single-sample physical prefix; no B merge | Runtime unverified |
| MSAA depth initialization | First upload shader draw lacks required initialization | Confirmed next source gap |

No universal MSAA pressure retirement or full memory parity is claimed. Pinned linear
MSAA copy attempt lacks valid sample extraction; guards stay. Normal fence-retired heap
reuse satisfies the advanced alias synchronization model; metadata initialization is a
separate requirement. See SESSION_HANDOFF.md and pressure-retirement-20261016 proof.


## Current readonly history coverage - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only continuation: no configure/build/package, fixture/test/compiler execution,
GPU/game/profiling/XInput/UI operation. All129 contract groups remain partial/open.
No current runtime PASS, image acceptance, FPS or performance result is claimed.

| Contract | Production coverage added | Acceptance |
|---|---|---|
| Modified/dirty identity | Sticky entry history across invalidation; pinned compatible replacement | Source review only |
| Readonly refresh | Explicit current-B provenance; direct independent reacquisition without fake write | Authored/unrun |
| Interior buffer writer | Clean I retained for direct and existing packed conversion sources | Authored/unrun |
| Reference color classes | Equal physical identity for ordinary8..128-bit views; existing transfer/history | Source review only |
| Failure/lifetime | Dirty-source formatted/retile rejection; witness pins and existing fences | Source review only |
| Original image writes | Own/readbacks follow original writable slots, not cache description | Source review only |

36b109c/01f916c/d55fb2d are WIP source commits, not full cache/format/renderer equivalence.
Never-modified/CPU/private-snapshot/remap controls, compressed/depth/attachment/incompatible-overlap identity,
pressure/device-loss and broader runtime acceptance remain open. See SESSION_HANDOFF.md.


## Current source coverage update - 2026-10-16

Authoritative current section; earlier current/next statements are historical.
Source work only: no build/configure/package, test execution, GPU/game, profiling,
XInput or UI operation. All129 groups remain partial/open. No runtime PASS or new FPS.

| Contract | Production change | Acceptance |
|---|---|---|
| Ordered writer extents | Short/full/long exact WriteRange; conditional readonly envelope | Authored/unrun |
| Binding-order page state | Earlier original writes and optimized-away Guest slots retained | Authored/unrun |
| Disjoint images | Per-Entry retile across multiple domains, existing guards intact |2 authored cases |
| Grouped native SSBO qualifiers | Sanitized groups preserve other targets; no incompatible old cache entries | Source review only |
| Unreflected images | Original cache preparation survives reflection pruning | Replacement procedure unrun |
| Readonly revalidation | Sticky modified history still lost after refresh | Confirmed next source gap |

All129 broad groups remain partial/open. Metadata/image operands, cross-stage ordering,
non-base simultaneous aliases, missing sparse-block history, pressure/failure and full
regressions/game verification remain open. SESSION_HANDOFF.md gives exact continuation.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

This section is authoritative; earlier current/next statements are historical.
Source commits d4b70f9,4210e31 and4ef80fc are pushed on fix/mixed-cmask.
No build/configure/package, test/fixture execution, GPU/game, XInput, profiling or UI
operation occurred. All129 registry groups remain partial/open. Source reviews and
Git/archive checks do not establish runtime correctness or performance.

| Contract | Current source boundary | Acceptance |
|---|---|---|
| Early clean readonly R before W | Original-size immutable CPU snapshots, raw and formatted | Uncompiled/unrun |
| Native SSBO alias declarations | Aliased on every native SSBO; old disk keys bypassed after rebuild | Source review only |
| Short/long readonly R with full W | Exact base/original bounds, per-reader source, one retained retile |16 authored cases, unrun |
| Arena capacity/residency | Checked4GiB capacity; same-arena/same-submit sparse backing conditional | Pressure/migration open |
| Short simultaneous writer | Saved precise next production task; original dirty intervals required | Not implemented in ordered path |

All129 registry groups stay partial/open. Metadata/image operands, cross-stage ordering,
multiple versions and broader offset/pressure/device-loss contracts remain open.
Current source/evidence and exact continuation are in SESSION_HANDOFF.md's top section.


## Current CMASK and ordered-buffer checkpoint - 2026-10-15

SESSION_HANDOFF.md's top section is authoritative; older next-task statements are historical.
Pushed source: e43ebdc (CMASK6/7 INF and canonical NaN, actual raw/typed cast-list checks)
and cbfe500 (one-stage full-range resident formatted reader/writer descriptor ordering).
Read-before-write retains image-to-buffer transfer before invalidation; write-before-read
keeps the buffer version. Early clean-small readonly snapshots remain guarded, including
raw readers preceding the writer. Existing transfer/ownership/fence checks remain.
Source-reviewed only: no build, configure, install, tests, GPU/game/XInput/UI or measurements.
All129 groups remain partial/open. Installed preview24 and selected preview23 are unchanged.
Eight real-GCN cases are authored only; the helper is new, its minimal integration into the
historical untracked attachment fixture is preserved separately and in final-residual.
Evidence/final remote HEAD/index/archive: F:/PS4Dev/handoff/cmask-exceptions-20261015/CHECKPOINT_FINAL.json.
Next production task: guest_compute.cpp early clean-small CPU-byte snapshots retaining
original logical sharp identity and separate native storage; use ordered-formatted/NEXT_SOURCE_TASK.md.
Do not rerun completed format/cache audits. The user execution gate remains active.

| Contract | Current production source | Runtime acceptance |
| --- | --- | --- |
| CMASK6/7 INF/NaN | INF preserved; selected NaN canonicalized before UF10 narrowing; actual cast-list raw RTV | Not run; chosen NaN payload is not measured Vulkan equality |
| Resident formatted R/W and W/R | One compute stage, unique full-range alias, retained transfer before invalidation | Not run |
| Small clean early readonly binding | Explicit guard, including raw R before W then formatted R | Unimplemented snapshot; next source task |
| General stage/image/DMA interleaving | Outside current ordered-buffer scope | Open |



## Current packed-float/RGB32 checkpoint - 2026-10-15

Scheduler6f55ea1: pre-execution resource batches, no-allocation fence assignment, publication capture retention, terminal non-replay and best-effort drain implemented. Independent source review only; device/timeouts and runtime acceptance remain open.

Latest source finding (2026-10-16): CommandScheduler::Submit moves a retirement callback
into by-value Defer after GPU execution; deque allocation failure can destroy its last
resource lease before fence completion. Source-proven failure path, not an observed crash.
The simple copy-before-clear experiment is INCOMPLETE and was removed only after preserving
its exact patch/before/after, with inherited scheduler bytes restored by exact comparison.
Evidence: F:/PS4Dev/handoff/scheduler-retention-20261016/{REVIEW.md,DISPOSITION.md,EXPLORATORY_OWN.patch}.

Next production fix: preallocate a scheduler-owned retirement/resource batch before queue
execution, publish its fence without allocating after ExecuteCommandLists, retain ownership
on Signal/Defer/publication failures, and avoid repeated partially executed callbacks. Review
this before publishing any scheduler fix. The previous submitted BC expansion38b7c95 remains
source-only; its staging uses this existing lifetime contract and has no runtime acceptance.
New partial BC fixture authoring is in progress separately; existing committed expectations
remain unchanged. Build/test/game/GPU/UI execution remains deferred; all129 groups partial.

Current partial BC expansion source WIP: 38b7c95 (2026-10-16), superseding prior next-step
notes below. MatchesReferencePartialBlockExpansion now admits same-address UINT64/128
source to native readonly BC target with matching base4x dimensions/pitch, equal array layers,
source mip count <= target, exact common physical mip offsets/strides/tiling, and every
source copy rectangle fitting its actual BC mip extent. Source bytes form a complete target
prefix. Early source selection, sticky modified history and generic transfer share this
admission. New callback captures one target R: synchronized B iff target pages are GPU
modified, otherwise read_backing_ physical P staged privately. Retile overlays only source I
words into the SAME seed prefix; prefix merges back without changing R-only blocks/tail.
New actual BC ownership is acquired before retiring old I; pins/resources/fences stay.

Independent source review: F:/PS4Dev/handoff/bc-partial-copy-20261016/REVIEW.md. No build,
compiler, tests, game, GPU or UI execution. Failure-path device/submission lifetime and new
GPU contents remain unverified. Old fixtures do not establish partial-copy coverage. Next:
author meaningful unrun controls for UINT24->BC96 levels5 partial-block complement and
UINT32->BC128 extra tail mips, with page-complete valid layer counts and BOTH target-page
GPU-query branches using distinct I/B/P. Use independent GPU readback; do not reconcile guest
source while observing. Check full target compressed bytes, padding, tail, source guards and
parent retirement. Then continue writable selected-view authority and shifted physical aliases.
All129 groups remain partial/open, and installed preview24 remains unchanged.

Latest source WIP 3bfd8ef (2026-10-16), superseding older ownership limits below:
actual native readonly BC storage now retains complete real mip layout even when it cannot
be represented by a complete UINT64/128 chain. Canonical BC-to-UINT matching remains stricter;
selected private UINT prefixes must still match it. No BC UAV, buffer-writer permission,
expanded/linear/volume admission or partial-copy conversion was enabled. Existing backing,
page,64MiB, watch and fence gates remain. Three authored shape controls now distinguish
valid actual BC ownership from invalid complete UINT correspondence; corrupted physical
layout controls still reject. Independent source review and exact patches/hashes:
F:/PS4Dev/handoff/bc-actual-owner-20261016/REVIEW.md. Uncompiled/unrun; no runtime PASS/FPS.

Next production task: add a single target refresh seed R (GPU Bsync iff target-page GPU
modified, physical P otherwise), feed its prefix to RetileBufferVersion, and merge that
prefix back into the same full R before BC upload. Implement explicit physical-prefix and
copy-extent admission from bc-mip-bridge-20261016/NEXT_PARTIAL_BC_COPY_CONTRACT.md first.
Current callback's separate source/target captures must not serve this partial-copy path.
All129 contract groups remain partial/open; tests/build/game/UI remain deferred.

Current source continuation: 179972f (2026-10-16), authoritative over older notes below.
Readonly UINT block-texel views selecting one mip now support nonzero first_mip through an
exact full physical prefix [0..k]. The helper requires first_mip=levels-1, prefix levels
within the BC parent, and identical per-mip pitch/height/offset/slice/tiling/layers and block
geometry. Complete ownership replacement keeps its original reverse-chain guard. The
private child remains dirty/non-authoritative; parent BC allocation, ownership and pending
fence resources survive. Independent source review: F:/PS4Dev/handoff/
bc-selected-prefix-20261016/REVIEW.md. No build/test/GPU/game/UI execution; no runtime PASS.

Remaining: reverse selected-view fixture coverage, writable shared-subresource coherence,
dirty-parent refresh, address-shifted mip/slice aliases, and forward partial BC copies with
the proved shared target B/P seed. Next source change must use NEXT_PARTIAL_BC_COPY_CONTRACT.md
in bc-mip-bridge-20261016; preserve one source version and target padding/tail provenance.
All129 groups remain partial/open. Earlier fixtures are authored/unrun; preview24 unchanged.

The 92af292 fixture update adds a full-region GPU baseline before BC chunk writes; authored,
unreviewed and unrun. Independent reports confirm readonly mip0 parent preservation and that
writable reverse aliases currently fail closed. Reverse-view fixture coverage, writable parity,
nonzero mips and dirty-parent behavior remain open.

## Selected BC view coverage - 2026-10-16

| Contract | Production continuation | Acceptance |
|---|---|---|
| Readonly BC parent to UINT mip0 view | Private dirty snapshot; parent remains sole owner | Source WIP 923d637; fixture ac3ca48 authored/unrun; independent review pending |
| Parent view selector normalization | Allocation mip0 prefix is checked independently of prior view first_mip | Source inspection only |
| Writable or nonzero selected alias | Coherence/dependency not implemented | Open |
| BC partial-copy untouched bytes | B/P seed choice contract saved; implementation open | Open |

No build, fixture, runtime, or game equivalence is claimed.


SESSION_HANDOFF.md's top section is authoritative; older next-task entries are historical.
06536f2 and6ded358 are pushed: exact packed-float2/4/8-sample transfers with actual cast lists,
readonly RGB9E5 physical ownership, and readonly RGB32 guest12/host16 transfer with separate
independent-image retention. Page/backing/fence/physical-domain guards remain. Source-reviewed
only, NOT compiled/installed/run. All129 groups remain partial/open. Preview24 stays installed
unchanged; no new FPS or game result. User execution gate remains active.
Evidence and final HEAD/remote/residual recovery: F:/PS4Dev/handoff/packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Next source task: CMASK6/7 selected +INF conversion in DecodeGuestColorClear; see
rgb32/NEXT_SOURCE_TASK.md. Finite RTZ is done; NaN encoding and format34 clear stay open.

| Contract | Current source boundary | Runtime acceptance |
| --- | --- | --- |
| R11G11B10 independent MSAA | Exact R32 transfers with typed/raw actual format lists | Not run |
| RGB9E5 independent MSAA | Readonly, native typed MSAA capability required | Not run; old-device emulation open |
| RGB32 independent MSAA | Readonly guest12/host16, explicit independent retention | Not run; adapter/original MSAA support unproven |
| Prepared coverage | 12 packed-float +9 RGB32 scenarios | Authored only |
| Exceptional CMASK6/7 clears | +INF next; NaN conversion open | Not run |



## Current packed-MSAA source checkpoint - 2026-10-15

SESSION_HANDOFF.md's packed-MSAA section is authoritative; older sections are historical.
Source1f43d43 adds finite CMASK6/7 deterministic RTZ,59fdc53 adds packed MSAA independent
versions/retained signed10 words and nonzero packed/sRGB clears; fe51ce4 extends
native sampled16-18 and retained expanded19. Source review only: NOT built/installed/run.
All129 contracts remain partial/open. Preview24 EXE70eeeff04e89 remains installed unchanged;
no new FPS/test/game result. The user test/GPU/game/UI gate remains active.
Evidence/provenance: F:/PS4Dev/handoff/packed-msaa-20261015; final HEAD/remote/residual
recovery hashes are in CHECKPOINT_FINAL.json. Broad dirty integration and22 staged
network deletions remain preserved. Next source task: exact packed-float independent
MSAA format6/34 with actual native castable-format contracts, then existing partial-alias
queue. Do not infer a missing Vulkan feature from its intentional whole-MSAA GC skip.

| Added contract | Source state | Runtime state |
| --- | --- | --- |
| CMASK6/7 finite rounding | RTZ CPU/RTV policy implemented | Driver tie parity unverified |
| Independent packed10 MSAA | Raw/native words + retained signed expansion | Authored cases unrun |
| Independent packed16 MSAA | Raw16, native16-18, retained expanded19 | Authored cases unrun |
| Nonzero packed/sRGB MSAA clear | Typed clear, exact word, all-sample broadcast | Numeric/device acceptance unrun |
| Whole independent MSAA eviction | Existing skip retained | Separate extension, no closure |


## Current source checkpoint - 2026-10-15, after preview24

SESSION_HANDOFF.md's complete-port section is authoritative. Source commits35b5de4,
d7d9064,440dd53,ad6808e and e359447 are pushed: stable WAIT_REG_MEM operands, shared typed/raw CMASK clears,
8/16/32-bit independent MSAA transfers with scratch pressure retirement/recreation,
pinned color-write-mask mapping, exact packed-float CMASK6/7, raw RGBA8 MSAA transfer
and nonzero clear with retained resources/fences; stale storage_retired reset on conversion.
Current changes are NOT compiled/installed/run.
Preview24 remains the latest installed executable; no new game/FPS or test results.
The user execution gate remains active. Evidence and pending source follow-ups:
F:/PS4Dev/handoff/complete-port-20261015. Preserve the broad dirty integration and
22 pre-existing staged deletions. Existing129 contract groups remain partial/open.
Next source task: independent packed10/packed16 MSAA reinterpretation through existing
transfer/tiler contracts, with raw guest words/masks and private padding preserved.
Final HEAD/upstream and residual hashes: complete-port-20261015/CHECKPOINT_FINAL.json.

| Contract delta | Source state | Runtime state |
| --- | --- | --- |
| Typed/shared raw CMASK, numeric and component mapping | Implemented bounded formats | Unrun; ownership/numeric boundaries remain |
| Independent MSAA component transfers | Extended exact8/16/32 families | Unrun; unsupported domains remain |
| MSAA pressure | Scratch retire/recreate only | Unrun; whole-image eviction open |
| Packed-float CMASK6/7 | Exact finite representable domain | Unrun; rounding/specials/34 open |
| Raw RGBA8 MSAA transfer | R32 word views integrated | Six prepared cases unrun |
| Raw RGBA8 MSAA nonzero clear | Typed conversion and sample broadcast | Four prepared cases unrun; sRGB/nonfinite/overflow open |
| PM4 WAIT predicate snapshot | Graphics and compute changed | Unrun; EOP cause unknown |
| CB color-write-mask scatter | Pinned mapping integrated |1152+3 cases prepared, unrun |


## Latest package — 2026-10-15

Preview24 is built and installed at the user's subsequent request: source a3f1d5e plus
packaged dirty patch, EXE SHA256 70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Release compile/link and static package/discovery checks succeeded. No runtime tests,
game or desktop automation; no new FPS. Selected core/settings and originals preserved.
See SESSION_HANDOFF.md and eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Earlier source-only compilation/install statements below are historical.

## Active continuation — 2026-10-15: source work, execution deferred

SESSION_HANDOFF.md is authoritative. The user stopped ALL test/game execution and
desktop automation until the full transfer is ready for final testing. New backing-write,
PM4 completion, culling, R32 MSAA, shared mixed/uniform CMASK GPU clears and fence changes
are WIP, source-reviewed only; no build/tests after that instruction. Historical326/346
and subsequent targeted results are unchanged. Full parity/world correctness remain open.
Original22 staged deletions retained; source/provenance at
F:/PS4Dev/handoff/eop-continuation-20261015. CMASK source commit 479064d93bd2.
Next: exact raw CB clear-word mapping for more numeric formats/component swaps;
then remaining MSAA/alias/cache contracts. Preserve user execution gate; no new audit.


## Active black-frame continuation — 2026-10-14

SESSION_HANDOFF.md active section is authoritative; older priorities/results below are historical.
Source fixes e267eb8, capture metadata24bf8c0, producer/ROP capture 1fb65f7b10fc;
prior integration WIP5b6f47e. All pushed to origin/fix/mixed-cmask, without22 pre-existing staged network deletions.
Stage24/pipeline4/metadata34. Menus recovered; final world remains fog/sky; G-buffer contains player/environment.
No accepted world controls, FPS or speedup. Full suite326/346 (native211/229) before final depth/fixture changes;
three later targeted passes are separate, not a new full-suite result. All128 contract groups remain partial/open.
Preview23 dx12-20261014-full-port-preview-bde5503581e2, SHA256 bde5503581e2178aa854703d9ec6823287d6b9ea243a61b41bc8e2f767ace760;76 runtime hashes/20 copied saves verified.
Previous selected version/settings, saves and installed Vulkan preserved. Data/code recovery: F:/PS4Dev/handoff/black-continuation-20261014.
AvPlayer source commit952234f517d5: old queue AV/new atomic queue PASS; latest game failed a
separate EventWriteEop backing-write assertion before world/ROP capture. No accepted latest-game milestone.
Current first blocker: EventWriteEop::SignalFence -> Core::Memory::TryWriteBacking failure (liverpool.cpp:705).
Then actual scene-color attachment producers/ROP before blur; depth is finite0.98016..1.
Do not blame PS8c08444c solely: its almost-black bloom input can preserve pre-existing fogged destination.

| Mechanism | Verified native scope | Remaining | Game evidence |
|---|---|---|---|
| Position partial exports | Production RectList oldFAIL/new64pixelsPASS; components preserved | Other stages/full world | Menus recovered |
| Independent image domains | Mixed PageWatch guards; GPU retile/reinterpret preserves physical backing | MSAA/partial/pressure/aliases | Two loader exceptions cleared |
| Depth ranges | Production Dsv mip/layer, exact12casesPASS; stencil/coherent fixturesPASS | Combined MSAA/cube/full renderer | No root attribution |
| Display | Same source/snapshot/presenter bytes; bounded input+ROP captures | Correct scene-color producer | Final world stillfog |


## Packed gaps — 2026-10-13 evening

Current source3f1249b, Stage24/metadata33. Unfiltered packed16–18 normalization:9/9 GPU cases pass. Sampled packed19 MSAA GPU expansion:7/7 sampled/backing cases pass. All remain partial groups: filtered precision, full RT quantization/blending, ownership and complete-game acceptance remain open. EmitImageWrite assertion was fixture misuse, not a proven missing production LOD path. Full suite324/344, native209/227; strengthened packed12/12 PASS. Bloodborne source/output black in engine captures, no gameplay acceptance. See SESSION_HANDOFF.md for current results; older black-window and Stage23 descriptions are historical.

## Current verified boundary � 2026-10-13

Latest source/state/evidence are in [SESSION_HANDOFF.md](SESSION_HANDOFF.md).
Old emergency GPU/game prohibition is superseded by the human's resumed development request.
Full-engine Vulkan parity remains partial; new registry groups are127/128, not audited closures.

| Mechanism | Implemented | Remaining | GPU verified | Current Bloodborne verified |
| --- | --- | --- | --- | --- |
| Captured FS typed read | R32_UINT SRV, original152-word ABI/32-source components | Sample/PerVertex/derivative precision and full games | Six signed chains; packed32/counters/MRT cases | No |
| Oversized EsGs point replay | Original ES/GS capture, generated VS/GS, original FS; placement, clear, barriers and leases | Tess/on-chip/streams, complete indirect/residency/MSAA coverage | Nine modes/864 cases, two GS invocations/three draw instances | No |
| Coherent ring address | Immediate scalar SOFFSET included | Dynamic scalar offsets | Wide GS attribute/position cases | No |
| Placed MSAA color initialization | Fully overwritten color layer initialized before upload/copy draw; other layers/DS preserved | Packed19 RT support, packed normalization, LOD write, DS/partial slices | Packed backing/packed10 modes/shared-color layers targeted results | No |
| BufferCache/TextureCache/MemoryTracker | Existing native implementation retained, owner-based extents | Prewrite aliases, owned-image waits, residency and remaining53 full-suite failures | Partial; complete suite174/227 before latest MSAA fix | No current acceptance |

The current black-window launch is not a valid world baseline or FPS measurement.
Original Vulkan/saves unchanged; no feature was disabled for a speed claim.

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
