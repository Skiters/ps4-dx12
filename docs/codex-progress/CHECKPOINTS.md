# Checkpoints

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


## Per-stage source/build checkpoint - 2026-10-17

Production1678638 connects original per-stage BindBuffers -> BindTextures effects for common native I/B allocations, intermediate image refreshes, initial cache-prefix retile, clean private streams and final exact-base invalidation. Primary and clean committed Release linked;48 authored helper transfer/cache controls and existing GCN/alias TUs are C++ typechecked only. No runtime/GPU/game/XInput execution. Latest external runtime: F:/PS4Dev/handoff/ordered-graphics-stages-20261017/artifacts-1678638. Remaining ordered DMA synchronization, converted/BC domains, shifted geometry and reader-arena clipping stay explicit in the nine-gap registry. Next: the ordered_stages&&uses_dma guard, GuestDmaMemory::AppendRequests/RecordFaults and pinned BufferCache::SynchronizeDmaBuffers. Full evidence/continuation: SESSION_HANDOFF.md. Never auto-install/select new cores; full port remains unfinished.

No new FPS/frame-time/stability/image acceptance result. Primary build command-20261017-023704-34592.log; clean023731-30028; clean alias/GCN syntax023809-33988 /023809-30108, all in F:/Codex-Shared/devtools/logs. Saved runtime baselines remain authoritative.


## Partial-page image source/build checkpoint - 2026-10-17

Production7420e32 connects partial-page independent image retention: page protection envelopes, complete physical-B boundary witnesses, lazy rearm-before-compare, CPU-winner preservation and nested pre-store deferral. Exact-base GPU invalidation and distinct I/B versions remain. Primary and separate clean Release linked; PageWatch/alias/GCN TUs C++ typechecked only. Runtime/game/XInput acceptance is not_run. Latest full external runtime: F:/PS4Dev/handoff/partial-image-watch-20261017/artifacts-7420e32. Full evidence/continuation: SESSION_HANDOFF.md. Next: multi-stage BindBuffers/BindTextures ordering at ExecuteGuestResources guards444/618, then remaining shifted-geometry/nine-gap backlog. No auto-install/select or full-port completion claim.

No FPS/frame-time measurement was performed. Saved baseline remains authoritative; this is source/build verification only. Primary build command-20261017-021650-23600.log; clean build021720-34568; clean PageWatch/alias/GCN syntax021825-70668 /021825-21280 /021825-13728, all in F:/Codex-Shared/devtools/logs.


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


## Independent BC block source checkpoint - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

fdb1e27: exact independent UINT64/128<->BC and samecodec numeric bridge, canonical physical
ownership, block-aware retile and corrected generic fallback isolation.25ba00c authors two
real-GCN BC cases, uncompiled/unrun. Source reviews, exact hashes/patches/residual and actual final upstream
SHA live in bc-block-expansion-20261016/CHECKPOINT_FINAL.json. Original22 staged deletions,
inherited untracked attachment runner and broad dirty integration remain preserved.
Previous MSAA/heap/readonly checkpoints and runtime results below stay historical.



## MSAA depth/stencil source checkpoint - 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

0682d65: full replaced MSAA DS plane initialized through retained one-layer DSV before draw.
9b5b356: stencil SRV X32_TYPELESS_G8X24_UINT uses uint4 in production sample extraction.
f067efb: focused native_msaa_depth_initialization mode is authored/uncompiled/unrun. Source and
fixture reviews, snapshots/hashes, exact residual archive and actual final upstream SHA are
in F:/PS4Dev/handoff/msaa-depth-init-20261016/CHECKPOINT_FINAL.json. Original22 staged network
deletions and inherited integration remain preserved. No rebuild/install/session operation.
Historical source checkpoints and runtime evidence below remain historical and unchanged.



## Actual texture heap pressure checkpoint - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
No configure/build/package/compiler/test/GPU/game/XInput/UI execution. All129 groups
remain partial/open; source/Git/hash reviews do not establish runtime correctness/FPS.

Sourcec6e5429/50860ef and authored-fixturee00c72d are pushed. Exact final SHA/remote,
original22-path index proof, reviewed worktree versus isolated staged hashes, partial-
commit provenance and residual archive are in pressure-retirement-20261016. The casting/
large-mode inherited integration was not swept into commits. Previous4ab70c5 and its
326-file residual archive remain immutable. SESSION_HANDOFF.md gives the new depth task.


## Readonly history checkpoint - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only continuation: no configure/build/package, fixture/test/compiler execution,
GPU/game/profiling/XInput/UI operation. All129 contract groups remain partial/open.
No current runtime PASS, image acceptance, FPS or performance result is claimed.

Production commits36b109c,01f916c andd55fb2d are pushed. 2f8522f adds four readonly real-GCN scenarios,
authored/unrun; fixture commit/current hashes are in readonly-revalidation-20261016.
CHECKPOINT_FINAL.json records exact final HEAD/remote/commit list/index/archive checks.
final-residual keeps the accumulated unknown-origin source integration and exact split;
original22 staged deletions remain separate. Previous4919bdb archive is immutable.


## Ordered writer and original-slot checkpoint - 2026-10-16

Authoritative current section; earlier current/next statements are historical.
Source work only: no build/configure/package, test execution, GPU/game, profiling,
XInput or UI operation. All129 groups remain partial/open. No runtime PASS or new FPS.

Source: ccacee9 exact writer ranges; e877a3f ordinal page facts/original Guest slots;
4f66433 disjoint image domains; f9ec214 legal grouped SSBO annotations;792f3fe original
image preparation. Authored-only fixtures: fc7515e(21),a895015(22),ce156e7(24 total).
Final documentation/local/remote SHAs and original-index/archive checks are in
F:/PS4Dev/handoff/ordered-writers-20261015/CHECKPOINT_FINAL.json. Historical residual
integration was preserved separately, not swept into these commits. Previous b7129d6
and ordered-stream-20261015 archive remain intact.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

This section is authoritative; earlier current/next statements are historical.
Source commits d4b70f9,4210e31 and4ef80fc are pushed on fix/mixed-cmask.
No build/configure/package, test/fixture execution, GPU/game, XInput, profiling or UI
operation occurred. All129 registry groups remain partial/open. Source reviews and
Git/archive checks do not establish runtime correctness or performance.

- d4b70f9: early CPU snapshots and12 authored ordered GCN cases.
- 4210e31: native SSBO Aliased contract and compute/linked cache invalidation.
- 4ef80fc: original short/long readonly extents with full-image writers.
- df8b741: four additional short/long GCN scenarios,16 total; authored only.
- The final documentation checkpoint SHA is in CHECKPOINT_FINAL.json.

Evidence and all before/after/deltas: F:/PS4Dev/handoff/ordered-stream-20261015.
Final residual archive preserves historical integration and original22 staged deletions.
Cache-source checkpoint used a separate index and committed only its version line.
Previous checkpoint30f5d19 and evidence cmask-exceptions-20261015 remain intact.


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



## Current packed-float/RGB32 checkpoint - 2026-10-15

6f55ea1 contains only reviewed isolated scheduler candidates; inherited scheduler patches remain dirty. Original22 staged deletions preserved. Evidence scheduler-batch-20261016; no build/test/game execution.

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

92af292 adds a complete output-region GPU readback baseline to the BC mip fixture, with no
guest-memory resolution. Source change is pushed and unrun; review this new delta before
relying on untouched-byte assertions. Prior independent production/fixture review reports are
saved under F:/PS4Dev/handoff/bc-mip-bridge-20261016.

## Selected BC mip view source checkpoint - 2026-10-16

923d637 pushed a readonly selected-mip0 conversion path that preserves the multi-mip BC
parent's authority and lifetime. It is WIP/source-only; the authored fixture oracle still
uses independent GPU readback, but remains unreviewed/unrun and lacks reverse selected-view
coverage. Evidence and exact source snapshot are in
F:/PS4Dev/handoff/bc-mip-bridge-20261016. Original staged network deletions are preserved.


SESSION_HANDOFF.md's top section is authoritative; older next-task entries are historical.
06536f2 and6ded358 are pushed: exact packed-float2/4/8-sample transfers with actual cast lists,
readonly RGB9E5 physical ownership, and readonly RGB32 guest12/host16 transfer with separate
independent-image retention. Page/backing/fence/physical-domain guards remain. Source-reviewed
only, NOT compiled/installed/run. All129 groups remain partial/open. Preview24 stays installed
unchanged; no new FPS or game result. User execution gate remains active.
Evidence and final HEAD/remote/residual recovery: F:/PS4Dev/handoff/packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Next source task: CMASK6/7 selected +INF conversion in DecodeGuestColorClear; see
rgb32/NEXT_SOURCE_TASK.md. Finite RTZ is done; NaN encoding and format34 clear stay open.



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

Source WIP commits:756359c0a410,9960b080783a,f1e68d4,29540d917543,479064d93bd2. Final HEAD/upstream and archive hashes are recorded in eop-continuation-20261015/CHECKPOINT_FINAL.json.


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


## Packed-gap checkpoint — 2026-10-13 evening

Source3f1249b pushed; Stage24/metadata33. Current full suite324/344, native209/227; final finite packed12/12 PASS. Preview21 SHA25ca1dee9a3d0c9348bc8dba01850e67604476f6ed2bc54c1cc1b5644fee2872. One123.8697s game run exited0, original20 saves unchanged, pad neutral/removed. Two engine captures black:2794 completed flips in93.101581s (~30/s) are NOT gameplay FPS. No matched performance comparison. See SESSION_HANDOFF and F:/PS4Dev/handoff/packed-gaps-20261013 for exact evidence, failures and next source-boundary task.

## Active continuation checkpoint — 2026-10-13

[SESSION_HANDOFF.md](SESSION_HANDOFF.md) is authoritative. Source WIP `0a3571a4b76bf1a6683a47f1ee409fe2555614a3`
adds production ordered point capture/replay and placed MSAA color-transfer initialization.
Stage23/pipeline4/metadata-v32. Registry128 partial groups; no new overall audit or semantic closure.
Six offline chains and864 point GPU cases pass; complete native suite before final MSAA
fix174/227,53fail. Later targeted MSAA successes do not make it green. Current Bloodborne
world/control/image/FPS not verified (black window). Continue EmitImageWrite LOD assertion
with native_packed_16_msaa, then packed/alias correctness and owned-image waits.
Latest human request authorizes incremental GPU/game checks; old emergency/test gates below are historical.
Source/code/diagnostics/build recovery: F:/PS4Dev/handoff/point-capture-20261013.

Targeted post-init backing:46/48 passed,208.566s; two format19 RT capability failures.
Preview19 installed: dx12-20261013-full-port-preview-34d785e8b1dc,76 runtime hashes/20 copied saves verified, previous selection preserved.

## 2026-10-13 — emergency full-port session handoff

Authoritative recovery: [SESSION_HANDOFF.md](SESSION_HANDOFF.md).
Base e84e31133320830db6268f3beefe9ddda1fb08cc; source WIP
c0aa3e4a6fb2de072b29ffa4d0682d5accd9c292,
`wip(dx12): preserve current performance development state` (19 known files).
The following documentation commit is named
`chore(dx12): checkpoint development state for next Codex session`;
its actual SHA/status/upstream outcome are external in CHECKPOINT_FINAL.json.

Full before-state: 253 tracked-change paths/155 new paths, staged+unstaged binary
patches, original index and SHA256 source ZIP at
`F:/PS4Dev/handoff/full-port-emergency-20261013`. Other changes are deliberately
left outside the WIP because broad pre-existing ownership was not re-established.
Final residual patches/manifests and RESTORE.md preserve their exact restoration.
Latest core237/PDB/MAP/dependencies are copied there; preview18/default/older cores,
installed Vulkan and original saves remain. Group126 typed SRV reader C++ links;
shader proof/replay runtime are pending. No completion/FPS improvement claim.
No emergency build, game launch or profiling; unknown PresentMon processes retained.

## Final high-memory verification — 2026-10-11

Initial c31111c -> 7a550c9 residency/pools -> 9527c90 bounded overlap lookups.
Final source 9527c901a50256016072c4b67464946f78061ab1; all four runs use SHA256 db7e55e2e8f070bf3739a2e8aa50bb8b0f67cd2b393cc34a19cb25ccabcc44df.
216/216 final high-memory tests. Same-scene final FPS A/B/C/D:
0.6440 / 0.8400 / 0.7505 / 0.8078; C/A +16.53% observed,
materialization per completed frame -96.91%. One run/mode; no significance claim.
No new visible regression identified; old artifacts remain. Standalone C dev build,
original installation and all 20 saves preserved. Raw evidence/report outside Git.


## 2026-10-11 — bounded residency lookups after measured B run

Parent 7a550c9. B reduced materialization ~97% per completed frame but not average
FPS. Retained mappings made full-map validation scans visible. FirstOverlapping
now bounds ReleaseCpuBufferWatches, NotifyCpuWrite and GPU-owned overlap queries
used by streaming/arena validation. Disjoint interval and containing predecessor
semantics are preserved. Regression fails before (10.24 million candidates), passes
after (10 thousand); 27.958 -> 0.353 ms in the component sample. Final game comparison
remains outstanding; index optimization is not included in this package.

## 2026-10-11 — high-memory implementation checkpoint

- Parent c31111c; single-agent work, dedicated dev profiles and immutable save seed.
- Configurable DXGI-aware cache/arena budget; no upfront allocation, pressure trimming
  respects both completed fences and externally held allocation leases.
- Fixed address-order hot eviction and 64-MiB DMA whole-cache clear in experimental mode.
  Both regressions fail before and pass after (test-before.log / test-after.log).
- High-memory full suite first exposed 160-MiB buffers wasting 256-MiB heaps;
  compact large heaps fix it. suite-high-memory-fixed.log: 215/215, 70.88 s.
- Real GPU-gated eviction test, snapshot/staging content verification, PRT edge,
  zero-budget/live-lease checks added; all previous tests retained.
- Game performance comparison pending; ordinary diagnostic A is archived with its
  source diff, new header, executable hash and per-frame/counter evidence.



## 2026-10-11 — F1–F3 complete, Release world comparison

- Parent 3d14e008aa4313abce0e1bf006d06089d02d2964; implementation 67d0ddc2df0294c9839f769821e7414681d43661.
- F1: CopyBufferBatch passes BufferWrite footprints separately from shader resource envelopes;
  default unknown shader writes remain conservative. Previous dirty union is retained.
- F2: PageWatch::ChangeMode transfers READONLY/NOACCESS under its lock, advances generation,
  preserves retired intervals and never introduces a writable gap. Commit downgrade is
  also protected. Upload snapshots use the exact staged version in cached CPU memory;
  watch establishment validates after protection is installed. Failed commit keeps ownership.
- F3: ValidateGpuBacking checks mapping, committed backing and guest access rights independently
  of CPU reconciliation. GuestBufferAccess is shared production code, not a copied fixture override.
  GuestRasterizer prepares image aliases separately; real CPU reads/uploads/control records
  and unsupported buffer/image conversions retain reconciliation and fences.
- Before -> after: disjoint F1 12288 -> 64 bytes, overlapping union 48 bytes;
  unknown prior writable envelope remains 12288. F2 compared span 4096 -> 0;
  resident A->B commits before CPU 1 -> 0. Logs: f1-before.log, f23-before.log, package.log.
- Production fixture: real CPU stores, 16 concurrent watch transfers, GPU chain/scratch,
  partial/page-edge aliases, unrelated CPU-visible dirty buffer, failed commit/retry,
  rights rejection, PRT hole and unmap/remap. Existing queued faults/rearm/retired-history
  tests remain. Old physical-alias fixture now sends Core's committed mapping notification.
- Native suite 211/211 in 67.40 s; suite-verified.log. D3D12 Debug Layer targeted tests pass.
- Bounded opt-in frame trace excludes blank/repeated frames and observes completion of
  each immutable frame's copy fence (including guest queue dependencies), with 2 ms polling.
  Created / PM4-ready / fence-complete / Present-queued are separate. Queueing is not scanout.
- Same-scene baseline and fixed Release each have two measured world runs; see BENCHMARKS.md.
  The baseline's extra failed loading attempt is retained, not silently discarded.
- Verified launcher: F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd;
  actual child core in its core/ directory, implementation 67d0ddc, clean at build,
  SHA256 c921b7e819a9c0d557ee23dae5a7f99075776f9a8b7e4f881c0d7e4705b1ccc5.
- Only local commits. Original installed executable and 20 save files verified unchanged.

## 2026-10-11 — recursive buffer reconciliation

- Parent: `ee765f40925fbe7c32aa3ede718161202e6a2a78`, branch `fix/mixed-cmask`.
- Output: containing commit (`git log -1 --format=%H -- docs/codex-progress/FULL_FRAME_PROFILE.md`).
- Proven cause: an outer nonempty resolving_buffers_ made unrelated CheckBacking
  requests call Finish recursively; 1,943 repeated sequences exhausted the worker stack.
- Change: Rasterizer::ResolveGpuBuffers tracks request-local new overlaps; scope cleanup
  preserves outer state and releases this request's entries after success or exception.
- Regression: actual GPU copies, exact bytes/padding, nested backing checks, commit failure
  and retry. Old code fails; fixed code passes. Core built; native suite 208/208, 96.62s.
- Game: original saved-character profile copies, rebuilt baseline/fixed binaries, partial 0.
  Fixed run exits at GXWorker:1 `eboot+0x26b2b0b`; no world/FPS improvement established.
- Evidence, executable hashes and timing limits: FULL_FRAME_PROFILE.md.
- Next: capture original GXWorker fault context and PageWatch ownership/protection.
- User authorized commit/push after verified changes; remote delivery is verified separately.

## Integration checkpoint 2026-10-11

2026-10-11 — parent a824f9f65668d61080d2e90488435589efcd4f90, clean initial tree.
Subject: perf(dx12): bound observed guest buffer reconciliation lookups.
Live GPU-worker samples at ~10,831 mappings motivated lower_bound/predecessor lookup.
New regression covers exclusive boundaries, ownership and preserved data; 208/208
native tests and core build passed. Microbenchmark 26.789 -> 0.366 ms / 10,000 calls.
Three integration runs documented in GAMEPLAY_BASELINE.md; no world-entry improvement.
Commit identity is this entry's containing commit (git log); no push performed.

## PARITY-A01 — 2026-10-10 Vulkan draw-filter precedence

- Input SHA: `e43ba4d`; output: commit containing this entry
  (`git log -1 --format=%H -- native-port/draw_filter_test.cpp`).
- Epic: A. Upstream `Vulkan::Rasterizer::FilterDraw` at the pinned reference revision.
- Adaptation: special color clear/decompress/resolve remains before primitive rejection;
  None now stops GuestRasterizer::DrawInternal before depth/stencil override-copy.
- C++ changes: guest_graphics_state.h, guest_rasterizer.cpp, draw_filter_test.cpp.
- Regression: seven production-classifier cases, including None with stale depth/stencil
  override flags and special CB operations. This is not a full GuestRasterizer execution test.
- Core compiled and linked; full native suite 207/207 passed in 54.09 seconds.
- No game run, new milestone or performance result claimed. End-to-end memory-side-effect
  fixture and deterministic Vulkan/DX12 command comparison remain the next priority.
- Local commit subject: `port(dx12): match Vulkan draw filter precedence`; no push.

## PARITY-E01 — 2026-10-10 scheduler ownership audit

- Input SHA: `774d0951ccb40caf47921856ede9c2446203699e`.
- Output SHA: the commit containing this entry (`git log -1 --format=%H -- native-port/scheduler_lifetime_test.cpp`).
- Epic: E; initial source map for A/B/C/D/F.
- Reference: upstream `0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa`,
  `Scheduler::UploadCommandBuffer/SubmitExecution` upload-before-primary and timeline ownership.
- DX12: `CommandScheduler::CollectTimestamps`, `AcquireContext`, `SubmitCopy`;
  recording callbacks in Rasterizer, GuestRasterizer and GuestTextureCache now return void.
- Confirmed defect: unsubmitted query data was read with an old/zero fence. Regression failed
  at baseline, passed after guarding submission state and collecting before oldest-context reuse.
- No caller used SubmitCopy's zero result; removing it makes the actual completion contract
  explicit without claiming that an observed early-free crash was fixed.
- New test: native_scheduler_lifetime, open/reused timestamp contexts and exact two-copy
  readback through the next direct fence. D3D12 Debug Layer enabled.
- Full native build and tests: 206/206 passed, 64.75 seconds.
- Previous GPU timing inference withdrawn; no new FPS or Bloodborne milestone claim.
- Remaining: draw-filter ordering regression, true command differential replay, full parity audit.
- Next priority: EPIC A DrawInternal PrimitiveType::None before override-copy behavior.
- Push: not performed in this cycle.

## 2026-10-10 — compute copy reconciliation

- Baseline: `01c0374` (`Stabilize partially overlapping DX12 buffer mappings`).
- Scope: opt-in dispatch-stage distributions and counters; page-aligned copy materialization;
  deferred reconciliation for GPU-owned copy destinations; coherence regression coverage.
- Result: copy-shader HLE CPU wall time decreased 5.5% in the controlled steady-state sample.
- Validation: targeted 2/2; full native 205/205; no D3D12 validation errors in the 66-second run.
- Local commit subject: `perf(dx12): reduce measured compute dispatch overhead`.
- Remote push: not performed.

## 2026-10-10 — resource timing and upload submission batching

- Baseline: `5b0f555` (`perf(dx12): reduce measured compute dispatch overhead`).
- Scope: asynchronous D3D12 compute timestamps; buffer preparation/coherence counters;
  deferred-reconciliation coverage; copy-queue upload batching with direct-queue ordering.
- Result: copy submissions decreased 4,046 to 2,678 (-33.8%); resource-binding CPU wall
  average decreased 7.9%; whole-dispatch average was effectively unchanged.
- Validation: targeted 3/3; full native 205/205; no D3D12 validation errors in the game run.
- Local commit subject: `perf(dx12): batch buffer upload copy submissions`.
- Remote push: not performed.

## CMASK-M01 — 2026-10-11

- Input: `7984fed0e789e4ea48af2487c7d1095826aa2c3c`.
- Changes: `GuestColorClearPending`/`MaterializeGuestColorClear` now build a per-layer mixed CMASK plan; zero tiles use a GCN AddrLib-derived pipe-interleaved address, F tiles remain untouched, materialized nibbles become F. `GuestRasterizer::MaterializeColorClear` preserves the original exception and adds only opt-in bounded capture diagnostics.
- Regression: `native_guest_cmask_fast_clear` exact changed/unchanged tile checks, idempotence, untouched layer and actual D3D12 readback; full native suite passes 208/208.
- Core build passes. Bloodborne was tested from cloned/rebased profiles; the audio guest assertion recurred before an additional CMASK-stage proof. No PASS game claim.
## GXWORKER-P01 — 2026-10-11

Final verification addendum: committed code `ab1bf1a47e0ce2a0a3cd8fa528285dcd8a58b066`
reaches world/control beyond 120 s using the delivered dev launcher. The debugger
observes 18 handled original-instruction faults (3 on GXWorker:1), then detaches.
LEVEL 2 established. Pixel correctness and playable performance remain open.
See GXWORKER_PAGEWATCH.md and the external final-verification.json for exact final
duration and executable SHA256. This addendum changes documentation only.

- Input `40a8e683def56160ea110a2d5df1a48a8532c4ce`, branch fix/mixed-cmask.
- Original first-chance game context proves class C retired-range coverage loss;
  short predecessor hides large interval covering a pending valid guest write.
- PageWatch coalesces retirement history and handles newly rearmed generations;
  protection, physical backing reconciliation, fences and recursion guard stay enabled.
- COPY queue texture initial state is corrected after real Debug Layer error 1334.
- Real delayed CPU-fault GPU regression fails before, passes after with exact bytes;
  negative/lifetime/bounded-retry checks pass. Full native suite 208/208, 55.52 s.
- Partial fix reached world/input then crashed; complete fix precommit run reached
  a separate loading mutex wait. LEVEL 1 established; playable performance not claimed.
- GXWORKER_PAGEWATCH.md documents evidence and the final committed-build dev launcher.
  External release/verification manifests capture exact binary hash and launch result.
- Local commit only; no push. Original application and saves unchanged.
