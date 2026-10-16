# Vulkan / native D3D12 parity audit

## Current image-source publication checkpoint - 2026-10-17

Production `2498c6fafe567e13a6b8fe71937aa967a1632b9b` connects version-specific image B-source residency: prewarm/capture reserves physical backing and retains deferred effects; actual image binding consumes them, including inherited independent I conversions and prewrite shader/depth/stencil sources. Publication preserves retry state, discards overwritten-version effects, checks existing source backing and does not replace native views. Primary and separate clean committed Release linked; six fixture TUs passed C++ /Zs in each checkout. All new runtime/shader/GPU/game/XInput/UI/FPS acceptance remains not_run.129 independent implementation/behavior records and nine gap IDs remain. Latest complete external runtime: F:/PS4Dev/handoff/image-source-publication-20261017/artifacts-2498c6f. Do not auto-install/select. Next: complete legal tiny DMA backing before final selected views, preserve GPU B/witnesses/fence leases, then the existing source-gap queue. SESSION_HANDOFF.md is authoritative; older sections preserve history.


## Current semantic B residency checkpoint - 2026-10-17

Production `d01969e9e7c5cb5336b3937f4f2c9aa85b35f768` separates shader-buffer semantic residency from physical DEFAULT preallocation, publishes original buffers in bind order, and masks formatted I-to-B copies to the pages resident at that point. Clean small shader reads use private full DWORD envelopes; host/domain reservations and private I conversion captures do not publish pages. Final refreshed images consume masked B, with selected native states restored. Primary and clean committed Release linked; five fixture TUs passed C++ /Zs in both checkouts. All new runtime/GPU/game/XInput/UI/FPS results remain not_run. Full port unfinished;129 implementation/behavior records and nine gap IDs retained. Latest full external runtime: F:/PS4Dev/handoff/semantic-buffer-residency-20261017/artifacts-d01969e. Do not auto-install/select cores. SESSION_HANDOFF.md is authoritative; older sections preserve history.


## Current preserving B growth checkpoint - 2026-10-17

Production558baa3/f16630d preserves complete preceding GPU B ranges, old CPU snapshots, forced CPU-store pages and RO witnesses across compatible batch envelope growth. Only new holes initialize from physical B; old allocations survive the copy fence. Existing actual guest-write reconciliation remains. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only, no new runtime/GPU/game/XInput/UI or FPS result. Latest full external runtime: F:/PS4Dev/handoff/buffer-envelope-preservation-20261017/artifacts-f16630d. Never auto-install/select it. Full port remains unfinished: semantic DMA residency publication, tiny full-subpage materialization and stable final views, direct/split/pressure B residency, auxiliary SRV unification, opaque depth/stencil and the existing nine-gap queue remain.

Authoritative proof and exact continuation: SESSION_HANDOFF.md. Older continuation entries retain history.


## Current invalid-I pressure checkpoint - 2026-10-17

Production `284be47d1bfacd32bdc8a871759bf6953a1edcf4` connects pressure eviction of invalid independent I without downloading stale pixels into physical B. Actual cache admission shares ReclaimResidentMemory with ReserveMemory; preserve pins, completion, callback identity and budget refresh are retained. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution or FPS result. Latest full external runtime: F:/PS4Dev/handoff/invalid-image-pressure-20261017/artifacts-284be47. Never auto-install/select it. Valid GPU-modified tiled I remains excluded from pressure download by the pinned Vulkan rule; older broad statements calling that exclusion a gap are superseded. Late tiny DMA materialization/stable B residency, SRV auxiliary unification, opaque depth/stencil and existing nine-gap queue remain unfinished.129 implementation/behavior records and prior behavior evidence are retained.

Authoritative evidence and exact C++ continuation: SESSION_HANDOFF.md. Older current/next sections preserve history.


## Current late resident DMA checkpoint - 2026-10-17

Production `7cc493f72c96a0492cc165166987ad4c7f38fa88` publishes fully materialized late DMA pages at BeforeBind: complete64KiB pages or4KiB subpages, exact existing mapping/offset coverage, original selected-view identity guards, no reservation/retile, single CPU-dirty B synchronization and placed-allocation retention. Primary and clean committed Release linked; four fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution. Latest external full runtime: F:/PS4Dev/handoff/late-dma-publication-20261017/artifacts-7cc493f. Never auto-install/select it. Remaining: complete-page materialization for tiny late sharps, late arena replacement, auxiliary SRV/UAV unification, opaque depth/stencil and tiled independent pressure/retirement, then existing converted/shifted/clipped/nine-gap backlog.129 groups remain separate implementation/behavior records:59 with concrete gaps,70 without a separately established production change. Full port remains unfinished.

This section supersedes older continuation entries. Exact evidence and next source action: SESSION_HANDOFF.md.


## Current planned resident DMA checkpoint - 2026-10-17

Production50bdc81/a564b6c/34232f8 connect synchronization of already planned resident DMA B after image binding, preserve CPU streams and physical guest backing, and safely borrow/release PageWatch image/buffer witnesses. Opaque depth/stencil retains the legacy exclusive-image/physical-backing path; it is not completed independent transfer. Primary and separate clean committed Release linked at34232f8. For these changes runtime/GPU/game/XInput/UI tests are not_run; fixture TUs passed C++ /Zs only. Earlier behavior evidence is retained. Latest full external runtime: F:/PS4Dev/handoff/ordered-dma-sync-20261017/artifacts-34232f8. Never auto-install/select cores. Next: stable late DMA residency/table publication before final native descriptors; then remaining opaque/tiled-pressure/converted/shifted transfer guards. Registry:129 groups,59 connected_with_known_gaps,70 with no separately established change, nine broader gap IDs. The DMA group now explicitly records the proven late-page implementation gap; unrun behavior is separate. Full port remains unfinished.

This section supersedes older current/next entries; full evidence and exact continuation: SESSION_HANDOFF.md.


## Current per-stage image/buffer checkpoint - 2026-10-17

Production1678638 connects original per-stage BindBuffers -> BindTextures effects for common native I/B allocations, intermediate image refreshes, initial cache-prefix retile, clean private streams and final exact-base invalidation. Primary and clean committed Release linked;48 authored helper transfer/cache controls and existing GCN/alias TUs are C++ typechecked only. No runtime/GPU/game/XInput execution. Latest external runtime: F:/PS4Dev/handoff/ordered-graphics-stages-20261017/artifacts-1678638. Remaining ordered DMA synchronization, converted/BC domains, shifted geometry and reader-arena clipping stay explicit in the nine-gap registry. Next: the ordered_stages&&uses_dma guard, GuestDmaMemory::AppendRequests/RecordFaults and pinned BufferCache::SynchronizeDmaBuffers. Full evidence/continuation: SESSION_HANDOFF.md. Never auto-install/select new cores; full port remains unfinished.

This current section supersedes older current/next statements below, which preserve historical evidence.


## Current partial-page image checkpoint - 2026-10-17

Production7420e32 connects partial-page independent image retention: page protection envelopes, complete physical-B boundary witnesses, lazy rearm-before-compare, CPU-winner preservation and nested pre-store deferral. Exact-base GPU invalidation and distinct I/B versions remain. Primary and separate clean Release linked; PageWatch/alias/GCN TUs C++ typechecked only. Runtime/game/XInput acceptance is not_run. Latest full external runtime: F:/PS4Dev/handoff/partial-image-watch-20261017/artifacts-7420e32. Full evidence/continuation: SESSION_HANDOFF.md. Next: multi-stage BindBuffers/BindTextures ordering at ExecuteGuestResources guards444/618, then remaining shifted-geometry/nine-gap backlog. No auto-install/select or full-port completion claim.

This section supersedes older current/next statements below; those entries retain historical evidence.


## Current shader mip-prefix checkpoint — 2026-10-17

Productione780a6a connects shared maximum uncompressed mip backing, cached I-tail/provenance retention, original SRV/UAV/LOD bounds and pre-view growth plus ordered B1-prefix+B0-tail snapshots. Stage metadata27. Primary and clean Release linked; prefix/identity/classification controls C++ typechecked only, no tests/game. Latest external runtime: F:/PS4Dev/handoff/shader-mip-prefix-20261017/artifacts-e780a6a. Full evidence/continuation: SESSION_HANDOFF.md. Next: partial-page independent image ownership/protection envelopes and shared-page CPU-fault guards; current retention limits, multi-stage and shifted geometry gaps remain. No auto-install/select or full-port completion claim.


## Current physical image identity checkpoint — 2026-10-17

Productionb85f076 replaces whole-T# alias identity with explicit pinned physical storage fields, covering ignored control/counter/reserved differences and reusing existing coherent UAV/manual-sampling paths. Stage metadata26. Primary and clean Release linked; alias/classification TUs C++ typechecked, no test/GPU/game execution. Latest external runtime F:/PS4Dev/handoff/image-storage-identity-20261017/artifacts-b85f076; full evidence/continuation in SESSION_HANDOFF.md. Next: connect same-base compatible uncompressed mip-prefix aliases through metadata, maximum shared backing and original view ranges before relaxing geometry guards. Nine broader gaps remain; no auto-install/select and no full-port completion claim.


## Current mixed-version checkpoint — 2026-10-17

Productionc238eef connects single-stage mixed formatted buffer/image versions with compatible physical layouts: interior W retains I; an exact-base W after retile refreshes images from B1, with final-copy-before-shader-read barriers. Primary and clean committed Release linked;32 new GCN controls are C++ typechecked only. Latest runtime: F:/PS4Dev/handoff/mixed-image-buffer-ordering-20261017/artifacts-c238eef. Full identity/evidence/continuation: SESSION_HANDOFF.md. Next: overlapping image SRV/UAV access unification (the still-live ExecuteGuestResources guard, pinned force_general); multi-stage ordering and other existing gaps remain. Never auto-install/select cores; renderer/game/XInput execution is still deferred. No new FPS or full-port acceptance claim.


## Current narrow-writer checkpoint — 2026-10-17

Production6a7dff0/7304bb2 connects exact original-sharp shader bounds (CBV space34, metadata25) and naturally aligned contained byte/halfword writer envelopes in the existing compute-only common-UAV image domain. Primary and clean committed Release passed;16 additional GCN controls are C++ typechecked, not executed. Full runtime: F:/PS4Dev/handoff/native-buffer-bounds-20261017/artifacts-7304bb2; identity/evidence and precise remaining source entry point are in SESSION_HANDOFF.md. The nine broader backlog IDs remain; no runtime/FPS acceptance or full-port completion is claimed. Do not auto-install/select cores. Next: remaining mixed image/buffer shared storage and ownership in ExecuteGuestResources; keep existing guards until adapted. Builds are allowed; final renderer/game/XInput tests remain deferred.


## Current ordered interior writer checkpoint — 2026-10-17

Manual-folder policy remains authoritative: never automatically install/select new core builds. The folder selector is installed in PS4Desktop.exe; source48396fd/a545b9f, installation evidence desktop-folder-core-20261017. No renderer/game test or user UI automation was run.

Production commit `daa76fc50a3d07978a8804d7a6e3646dbe4c647a` extends ExecuteGuestResources for contained DWORD interior buffer writers in a single compute-stage, buffer-only common-UAV image domain. Pinned TextureCache::InvalidateMemoryFromGPU dirties I only at the original exact sharp base. Binding order now compares the first such writer, so an earlier offset writer can leave I eligible for formatted retile. Every small CPU-clean readonly stream remains private, even after a writer on another page. Full prewrite I/B snapshot, physical padding, original descriptor bounds/dirty ranges, pins and fence callbacks stay. The prepared transfer is consumed after buffer preparation without reselecting a potentially invalidated logical entry.

Eight real-GCN controls extend gcn_ordered_formatted_fixture.h: R/W and W/R, small/large, clean/dirty B, I != B, later formatted reread, retained I probes, exact four-byte writer/out-of-bounds store and full64KiB physical/padding oracle. They are authored, not executed. C++ /Zs passed for the containing GCN fixture and BC volume fixture with actual Release include/define/toolchain inputs in both primary and clean checkouts. This is typechecking only, not DXIL conversion/fixture link/GPU acceptance. Renamed local small/far identifiers in two fixture headers after real SDK macro errors; no SDK macro was disabled.

Incremental primary Release and clean committed Release both compiled/linked (exit0). Clean source has no tracked dirt and its2282 compile commands contain no primary checkout input. Saved complete EXE/PDB/runtime/compiler/licenses: `F:/PS4Dev/handoff/ordered-offset-writers-20261017/artifacts-daa76fc`. EXE SHA256 `74e783a09fd2e3ed5f7695235acf4eefa99bd515f7e9d4d7d9b21abcb599b9b3`; PDB `a1a1cf1446a8f6603e3f1e0bd359055149868b5cc3282bd7984aa3d553ff6d26`. It is untested in game and not installed/selected. Old6cbaef7 artifacts remain unchanged.

Exact evidence: F:/PS4Dev/handoff/ordered-offset-writers-20261017/, RELEASE_PROOF.json, release-identity.json, source-before-commit.patch/manifest, original and clean fixture response/command files. Build log command-20261017-010800-71224.log; clean GCN/BC syntax logs command-20261017-010938-20920.log / command-20261017-010938-72452.log. Logs are in F:/Codex-Shared/devtools/logs. No new FPS, image, control, lifetime or performance acceptance claim.

Nine broader concrete gap IDs remain in IMPLEMENTATION_REGISTRY.md; the formatted ordering ID now excludes the connected DWORD subdomain. Continue the remaining original sharp-byte range/domain transfer contract, starting from ShaderBufferRequest::WriteRange and the current compatible_uavs predicate, then the existing buffer-byte store lowering. Formatted readers remain exact-base (pinned buffer_cache.cpp:409 assertion). Prove byte/halfword envelope and logical invalidation separately before admitting sub-DWORD writers; do not weaken DMA/multi-stage/mixed-image/layout/ambiguous-owner guards without their full transfer adaptation. Reuse the existing exact-range GCN fixtures, rather than restarting the global renderer audit. Build/compiler remain permitted; final suite/game/XInput stays deferred to the agreed full-implementation boundary. Full-port goal remains active.


## Active integration checkpoint — 2026-10-17

This section supersedes older active/next instructions below. Build/compiler/link are authorized by the latest user request; tests/game/XInput/UI remain deferred until the complete implementation boundary.

Accumulated sources and required build integration are committed. Separate clean Release built successfully at `72569a5`; full identity, snapshot recovery, source commit contents and logs: [INTEGRATED_RELEASE.md](INTEGRATED_RELEASE.md). EXE SHA256 `ddf89b38fada81c5865d1dfd1d5cb13b052f855ae261ad3804d2da4114a4955a`.

The129 groups now distinguish connected implementation from behavior verified. [IMPLEMENTATION_REGISTRY.md](IMPLEMENTATION_REGISTRY.md) lists concrete known changes as upstream→native→missing semantics and prepared checks separately. Earlier “all129 partial because unrun” statements are historical and superseded. No final runtime PASS or full-port completion is claimed.

Current continuation: verify committed build script from the clean checkout, then resume expanded BC typed-view production work and the explicit remaining code backlog. Full-port goal remains active. Preview24/original Vulkan/saves/user session unchanged.


### Post-integration production continuation — 2026-10-17

The committed Release build script passed in the clean checkout at `ff8534f`; identity is `F:/PS4Dev/handoff/integrated-release-20261016/SCRIPT_RELEASE_IDENTITY.json`, log `command-20261017-003210-70240.log`. Its EXE SHA256 is `86a67a4cbe3383ba03446453569ea3c1d67f18216eb72169fa3a3416932fdeae`.

Continued production work: expanded BC1/2/3/7 UNORM/sRGB CPU snapshots share one RGBA8 typeless resource. Full same-codec physical-layout proof and actual resource-format checks gate typed views. Complete compressed-byte/padding comparison gates reuse, including identical pending immutable uploads without overwriting staging. Original owner/descriptor identity stays; signed BC4/5/6H FLOAT decode variants remain separate, and native BC parent-prefix rules are unchanged. Logical modified history survives compatible expanded numeric views.

Incremental primary Release compiled/linked the new production code (exit0, `command-20261017-003847-60380.log`). Extended existing BC volume checks are authored only. Clean Release also passed at `6cbaef71857b04275be11013e62c05c2b3c672b3` (exit0, `command-20261017-004316-56776.log`); full saved package `F:/PS4Dev/handoff/integrated-release-20261016/artifacts-6cbaef7`, EXE SHA256 `7751a7b565c94920c3ac15271dba0088308506a1a1717406aa8c1eb2aaf450c8`. Next production target: `ExecuteGuestResources` in guest_compute.cpp, the independent formatted image/buffer partial-or-simultaneous-write ordering guard. Adapt offset buffer writers / overlapping planned alias requests using pinned BufferCache::SynchronizeMemoryFromImage semantics; keep formatted readers exact-base (upstream buffer_cache.cpp:409 asserts it), and read NEXT_ORDERED_BUFFER_WRITERS.md, retaining separate prewrite image I/buffer B, exact dirty ranges, complete padding, CPU guards and fence leases. Reuse saved contracts; do not repeat the global audit. No final tests/game or app installation yet; the full-port implementation boundary remains open.


## Active BC typed sampled views checkpoint - 2026-10-16

Authoritative current section. Full-port goal remains active/all129 groups partial/open.
Source-only gate: no configure/build/compiler/helper/fixture/tests/GPU/game/XInput/UI.
No new runtime PASS/FPS/validation. Preview24, original Vulkan and saves unchanged.

Through68a2bad, native logical nonarray views use reflected one-slice array bindings,
including shifted6x6 BC SRVs; original shader dimensions/LOD and imported Texture2D
contracts stay. Current block allocates native BC1/2/3/4/5/6H/7 as typeless families
and binds fully typed same-codec SRVs (UNORM/sRGB or signed/unsigned variants).
MatchesReadonlyBcMipPrefix proves logical physical prefixes; CanBindReadonlyBcMipPrefix
also checks actual resource format and numeric GPU owner's version/watch/full retention.
FindValidIndependentImage/Upload/rebase/Entry::Srv share that binding proof. Canonical
parent identity and readonly swizzles/ranges remain; different BC codecs never merge.
Renderer admits readonly same-family physical prefixes before upload. CPU-coherent
reuse still checks complete compressed snapshots/watch; retired modified history uses
logical family proof without requiring a resident resource. Expanded BC rules unchanged.

Existing partial BC1 GCN helper now authors a GPU-only565 midtone I block and simultaneous
UNORM/sRGB reads, repeated after full formatted retile in both B/P branches. Program39
DWORDs,16 userSGPRs plus literal output V# s16..19; original sharps/roles/bounds retained.
Finite normalized/sRGB observations use RGB tolerance0.005, then freeze exact bits for
remaining full output/64KiB physical oracles. New outputs1536..1567 avoid existing ranges.
All controls UNRUN; other BC families and explicit CPU-coherent numeric-pair execution
remain unverified. Previous27 writable/shifted controls and address controls remain UNRUN.

Evidence/recovery: F:/PS4Dev/handoff/bc-typed-views-20261016/{SOURCE_CONTRACT.md,
checkpoint/COMMIT.json,CHECKPOINT_FINAL.json,final-residual}. Broad inherited integration
and exact22 staged deletions remain separately preserved. Restore only in a separate
checkout at manifest.head, then staged.patch --index, unstaged.patch and source archive.
Clean commits alone do not reproduce current core. Previous evidence/measurements remain.

Next production source target: standalone expanded BC1/2/3/7 UNORM/sRGB cached RGBA views.
Inspect UploadVersion allocation and GuestBcDecoder byte semantics before sharing typed
SRVs of one decoded typeless resource. Retain compressed-byte snapshots/watch. BC4/5/6H
signed variants require a different decode and must not reinterpret existing FLOAT pixels.
Do not relax native prefix guards to expanded images. Reuse saved evidence, no general
audit or execution. Pinned SliceOf multi-local-layer/base_array discrepancy remains open.

## Active mixed BC / UINT view coverage - 2026-10-16

All129 groups remain partial/open. Production changes are source-reviewed only.

| Contract | Current source | Verification |
|---|---|---|
| Multiple writable UINT views | One maximum-prefix/full-layer child and commit per BC parent | Authored/unrun |
| Disjoint readonly UINT SRVs | Shared child; original view range and LOD preserved | Authored/unrun |
| Same-mip layer storage reads | Existing conservative frontend UAV promotion preserved | Authored/unrun |
| Direct readonly native BC SRVs | Actual full parent; disjoint UINT writes remain private | Authored/unrun |
| Parent resource state | Restore SRV after retile preparation, before compute dispatch | Source review only |
| Ownership/lifetime | Parent pins; child nonowner; replacement acquires before retirement | Source review only |
| Unaligned private prefixes | Private upload/retile requires live full-parent UID/pin/watch and exact physical prefix | Authored/unrun |
| Shifted one-mip UINT views | Exact thin microtiled mip/slice rebase before shared grouping | Authored/unrun; multi-layer/base_array reference discrepancy open |
| Shifted/bounded native BC SRVs | Actual typed BC parent reuse; original mip/layer spans; BC-only views across graphics stages | Authored/unrun; graphics runtime unverified |
| Macro/thick shifted views | Aligned group stride, rounded layers and exact pipe/bank correction proof; canonical parent swizzle | CPU address controls authored/unrun; real-GCN coverage absent |
| One-level BC parent word views | Exact private readonly mip0 and shared dependent write/commit preserve native parent | Authored/unrun, including readonly BC+UINT in one dispatch |
| Overlapping feedback/DMA/buffers | Unsupported combinations still rejected | Remaining contracts |

Twenty-seven GCN cases are prepared, including one-level parent readonly/writable controls; none was compiled or run. No new game/FPS evidence or
complete parity claim. Current continuation and exact evidence: SESSION_HANDOFF.md.

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
