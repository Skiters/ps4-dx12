# Next session

## Active integration checkpoint — 2026-10-17

This section supersedes older active/next instructions below. Build/compiler/link are authorized by the latest user request; tests/game/XInput/UI remain deferred until the complete implementation boundary.

Accumulated sources and required build integration are committed. Separate clean Release built successfully at `72569a5`; full identity, snapshot recovery, source commit contents and logs: [INTEGRATED_RELEASE.md](INTEGRATED_RELEASE.md). EXE SHA256 `ddf89b38fada81c5865d1dfd1d5cb13b052f855ae261ad3804d2da4114a4955a`.

The129 groups now distinguish connected implementation from behavior verified. [IMPLEMENTATION_REGISTRY.md](IMPLEMENTATION_REGISTRY.md) lists concrete known changes as upstream→native→missing semantics and prepared checks separately. Earlier “all129 partial because unrun” statements are historical and superseded. No final runtime PASS or full-port completion is claimed.

Current continuation: verify committed build script from the clean checkout, then resume expanded BC typed-view production work and the explicit remaining code backlog. Full-port goal remains active. Preview24/original Vulkan/saves/user session unchanged.


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

## Current continuation pointer - 2026-10-16

Read active SESSION_HANDOFF.md and bc-single-level-20261016/CHECKPOINT_FINAL.json; verify
actual HEAD and preserve inherited dirty integration/22 staged deletions. Single-level
private BC/word predicate is saved. Next: shifted readonly odd/small local BC views of
an already-native parent mip, starting with RebaseBlockSubresource's native_bc/expanded
filters and LayoutGuestTexture's BC fallback. Prove physical block mapping and shader/
descriptor type/span before avoiding binding-only RGBA expansion; generic upload guards
stay. No compiler/build/tests/GPU/game/UI. Earlier next-task notes are historical.

## Current BC mip continuation - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

Verify actual HEAD/upstream against bc-block-expansion-20261016/CHECKPOINT_FINAL.json.
fdb1e27's single-mip BC bridge is implemented/pushed, with2 additional authored GCN cases.
The older BC investigation omitted ResolveOverlap->ExpandImage; its proposed HLE producer
prerequisite is superseded. Do not repeat it. Next source task: extend
IsIndependentGuestBlockStorage/DescribeGuestImageOwnership and MatchesReferenceBlockImage
to forward/same-codec multiple mips, requiring per-mip block-width/height correspondence
and identical complete physical layout. Keep reverse uncompressed block-texel views at
levelCount1 until their selected mip/view integration is proved. Base4x alone is wrong for24->96 at mip4. Read NEXT_BC_MIP_CONTRACT.md;
keep codec/fallback/pin/history/retention guards and author appropriate unrun mip cases.
No build/test/game execution or UI is authorized. Preserve broad inherited residual/index,
push accurate WIP and continue existing source contracts without claiming full completion.



## Current bounded BC identity continuation - 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

Verify HEAD/upstream against msaa-depth-init-20261016/CHECKPOINT_FINAL.json and read the
SESSION_HANDOFF current section.0682d65/9b5b356 depth-init/UINT-stencil fixes are already
pushed; their focused fixture is authored only. Do not rerun source audits of these paths.
NEXT_BC_IMAGE_IDENTITY_CONTRACT.md proves no current native modified independent BC producer;
do not repeat it or widen identity alone. Next exact boundary: pinned IsComputeImageCopy /
CopyColorAndDepth versus native ComputeImageOperation, GuestImageCopyCompatible and
DescribeGuestImageOwnership. Read NEXT_SOURCE_TASK.md and adapt a valid compressed-copy
producer/ownership/retention before BC family matching. Preserve source residency, physical
padding, complete logical write and fence lifetime; do not legalize BC UAV writes or conflate
equal-block-byte codecs. Source work only; preserve dirty integration/index
and push accurate WIP. Full-port/runtime/reproducible publication remains incomplete.



## Current MSAA depth continuation - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
No configure/build/package/compiler/test/GPU/game/XInput/UI execution. All129 groups
remain partial/open; source/Git/hash reviews do not establish runtime correctness/FPS.

Verify HEAD against pressure-retirement-20261016/CHECKPOINT_FINAL.json. c6e5429/50860ef/
e00c72d already cover actual heap-pressure recovery, eager idle trim and authored focused
acceptance; do not redo them. Read NEXT_PLACED_ALIAS_CONTRACT.md and implement first-use
MSAA depth initialization in GuestMultisample::Lease::Record / GuestTextureCache::Upload,
protecting untouched layers and stencil/other planes and retaining descriptor/transfer
leases. Prepare exact content controls, without execution. Existing unmap/remap generation
cancellation is source-covered; no speculative global alias barriers or MSAA pressure
guard lift. Continue the existing contract queue, preserve dirty integration/index, push WIP.


## Current readonly continuation - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only continuation: no configure/build/package, fixture/test/compiler execution,
GPU/game/profiling/XInput/UI operation. All129 contract groups remain partial/open.
No current runtime PASS, image acceptance, FPS or performance result is claimed.

Verify HEAD/upstream against readonly-revalidation-20261016/CHECKPOINT_FINAL.json and
read SESSION_HANDOFF.md's authoritative top.36b109c/01f916c/d55fb2d already implement modified/
dirty history and direct/packed/reference-color-class source preservation; do not redo them. Reuse exact
primary/independent source reviews and current fixtures. Complete never-modified,
CPU-write, private-snapshot, failed-retention and remap identity controls; inspect the
saved reference-class proof before any compressed/depth/attachment identity extension.
Continue GuestRasterizer::RetireImageForPressure and TextureCache ReserveMemory/
RetainPressureSource production contracts through the existing registry, preserving
independent MSAA/format domains, exact physical commits and fence/pin lifetime.
All execution stays deferred. Preserve dirty integration/index and push accurate WIP.


## Current continuation - 2026-10-16

Authoritative current section; earlier current/next statements are historical.
Source work only: no build/configure/package, test execution, GPU/game, profiling,
XInput or UI operation. All129 groups remain partial/open. No runtime PASS or new FPS.

Verify HEAD/upstream against F:/PS4Dev/handoff/ordered-writers-20261015/CHECKPOINT_FINAL.json.
Read NEXT_READONLY_REVALIDATION_SOURCE_PROOF.md there, then implement the confirmed
Entry logical-modified/dirty history and successful readonly Upload revalidation in
guest_texture_cache.h/.cpp, guest_rasterizer.cpp and post-Upload integration. Reuse
original WriteRange, transfer/pin/fence/watch contracts and24 authored cases. Differentiate
old I, buffer B and physical bytes; include same-base invalidation, readonly refresh,
shifted interior write, never-modified/CPU-write/remap controls. No-op shader replacement
procedure already documents unreflected original-slot acceptance; do not repeat its proof.
No broad audit, build or game run is authorized. Preserve index and push accurate WIP.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

This section is authoritative; earlier current/next statements are historical.
Source commits d4b70f9,4210e31 and4ef80fc are pushed on fix/mixed-cmask.
No build/configure/package, test/fixture execution, GPU/game, XInput, profiling or UI
operation occurred. All129 registry groups remain partial/open. Source reviews and
Git/archive checks do not establish runtime correctness or performance.

Start with SESSION_HANDOFF.md's top section and verify HEAD/upstream against
F:/PS4Dev/handoff/ordered-stream-20261015/CHECKPOINT_FINAL.json. Read the saved
NEXT_PARTIAL_WRITER_SOURCE_PROOF.md, then modify ExecuteGuestResources short-writer
admission and conditional readonly full-image retile envelope. Keep exact WriteRange,
whole-image invalidation at W, prewriter CPU snapshots and native SSBO Aliased metadata.
Use the proof's full-reader/minimum16KiB-block residency bound; the4GiB capacity test
alone is insufficient. Author the GPU-I-tail/physical-B-tail oracle. Existing partial
write range contracts and all prior16 cases are reused, not re-audited. Execution remains
deferred until specifically authorized. Checkpoint/push source and preserve old index.


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

Continue from scheduler batch6f55ea1, superseding the incomplete scheduler experiment. Author bounded failure/lifetime controls without execution, then writable mip authority. Read SESSION_HANDOFF.md and scheduler-batch-20261016/REVIEW.md.

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

Latest: 92af292 adds a full output-region GPU baseline to the authored BC mip fixture. Review
that delta; add reverse readonly mip0 parent-preservation assertions and tighten reflected
special operand/component coverage where source semantics are known. No fixture/build execution.
Writable reverse alias parity remains source work after the readonly case.

## Continue selected BC mip views - 2026-10-16

923d637 is pushed, source-only. Review the selected mip0 private snapshot path and finish
the authored fixture's independent GPU output readback; verify parent all-mip identity. Then
continue nonzero selected mip and writable child coherence, and forward partial-copy source
seed semantics from bc-mip-bridge-20261016/NEXT_PARTIAL_BC_COPY_CONTRACT.md. Keep tests/build/game
deferred. Do not mark BC complete.


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


## Packed-gap continuation — 2026-10-13 evening

SESSION_HANDOFF.md active packed-gap section is authoritative. Source3f1249b pushed; Stage24/metadata33. Packed16–18 fetch9/9 and packed19 sampled/backing7/7 PASS. Prior ImageWrite assertion was fixture LOD-contract misuse; assertion retained. Full suite324/344 (native209/227); final finite packed12/12 PASS. One124s game run confirms black source/output despite2794 completed flips; next investigate Snapshot/PrepareDisplayTexture and image versions. Preview21 installed, old selection preserved. Remaining ownership/aliases/partial planes and complete renderer parity are open. Older headings below are historical.

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

## Start here — emergency checkpoint 2026-10-13

[SESSION_HANDOFF.md](SESSION_HANDOFF.md) is the main recovery source and overrides
the historical priorities below. Verify actual HEAD against
`F:/PS4Dev/handoff/full-port-emergency-20261013/CHECKPOINT_FINAL.json`; retain
remaining dirty changes. Source WIP c0aa3e4 preserves the known point/capture work;
patch/archive/manifest recovery is documented externally. Do not reapply patches
to the existing tree or restart the old envelope/performance investigations.

Next: compile-only gcn_winding fixture against core237, verify captured fragment
typed R32_UINT SRV t0/space33 with 152-word addressing, update its old raw-buffer
proof, then implement generated replay VS/GS and runtime allocation/clear/
barriers/lifetimes. Core237/checks190 already compile/link; typed-reader shader
signing and replay runtime are pending. No game/GPU run before the full-port gate.
Exact command, evidence paths, build identities, known failures and ready prompt
are in SESSION_HANDOFF. Emergency preservation started no new tests or builds.

## Current boundary — selectable cores; matched FPS blocked by baseline failure

Latest task authorizes isolated XInput, window focus and gameplay; it supersedes
the older restriction below. No game or owned pad remains active. ba3892f keeps
covered mappings before optional arena rounding; 221/221 regressions and the
final game/controls pass. Installed desktop version selection and runtime hash
checks work; baseline and tested final packages are preserved, old default restored.
See BENCHMARKS and F:/PS4Dev/handoff/covered-envelopes-20261012. Three allowed
game launches are exhausted. A/A2 baseline load exceptions prevent matched FPS;
do not claim speedup or transfer old passive results to the new executable.

Remaining work: attribute actual role/envelope transitions at 0x2d1ce3000
(B sample still 7.115 GB materialized) with a bounded capture of requests and
ownership, then test stable GPU-owned backing/views with correct SRV/UAV states.
RetireBuffer still requires all recorded users complete; a future asynchronous
retirement change needs explicit resource + allocation leases/fence-domain tests,
including pool-cap exhaustion and failure retry. Do not remove waits mechanically.
Image Upload already checks coherent owned/watched/pending snapshots; B contains
4096x2048 BC1 (data format 35), tile 13, pow2 layouts. A next detile change must
prove actual invalidation/contents and repeated work for those layouts, rather
than skipping aliases or importing general Vulkan detile. No such new GPU detile
or write-footprint narrowing is accepted in this checkpoint.

## Current boundary: verified native changes; restricted gameplay acceptance

Latest user instruction (2026-10-12): tests limited to game launches. Do not use
Computer Use, activate windows or send gameplay input while the user uses the PC.
The final core passed 220/220 regressions, loaded the world and produced 1.587158
new completed frames/s in a passive 90.098142-s window. Baseline was 0.863128.
Focus/external input were uncontrolled; this is not a strictly matched stationary
comparison. Fresh movement/camera/attack acceptance remains open under this
restriction. Do not silently resume the earlier authorized XInput/UI workflow.
No game or virtual pad from this run should remain active after cleanup.

Use [CORE_CONTRACT.md](vulkan-emulation-20261011/CORE_CONTRACT.md) and
[PINNED_PROBE.json](vulkan-emulation-20261011/PINNED_PROBE.json). Compatible-source
Vulkan measurements and internal queue/PM4/transfer attribution are now saved;
do not rerun those completed steps or confuse them with installed 974-dirty.
The justified port, checks, incremental core and one final capture are complete.
Reuse the isolated profiles/builds and saved evidence; do not restart research.
Pending protection cannot be a cache hit until comparison/upload succeeds;
nested alias writes must release it. The port now preserves clean CPU pages
after writes and includes ordinary indices in the shared resident resource batch.
Do not import the old frozen patch without this distinction. If game acceptance
fails, preserve the failure and fix/revert the change before publishing it.
Automatic push applies to verified logical commits. Whole-engine parity, enemy
combat/death and exact dirty-runtime attribution remain open.

| Confirmed mechanism | DX12 action / expected effect |
|---|---|
| GCN frontend, shader/PSO cache, copy HLE, GPU ownership, fence retirement and completed flip tracking | Already implemented; preserve the existing contracts |
| Dirty CPU pages / unchanged buffer reuse | `rasterizer.cpp`, `page_watch.h`, `guest_buffer_access.h`: invalidate/rearm only dirty pages, protect before comparing, retain clean proof; reduce whole-buffer comparisons |
| Ordinary 16/32-bit index buffer binding | `guest_rasterizer.cpp`, `guest_compute.cpp`, `graphics_submission.cpp`, `rasterizer.cpp`: prepare/pin with shader resources, combine read states; UAV aliases get a pooled GPU snapshot; avoid CPU conversion/allocation/readback |
| General GPU detile, newer DrawPipe implementation and unknown installed fork modifications | Insufficient exact format/implementation evidence for a blanket port; keep existing verified paths. Snapshot comparisons dominate the measured CPU cost |

The last rearm fix removed full VirtualQuery scans of retained clean pieces;
new gaps still validate through Track and ownership-mode transfers still validate
their protection. Earlier attempts/failures are preserved outside Git. Remaining
measured costs include reconciliation submit/wait (11.40 s), resident backing
materialization (9.02 s) and overlap retirement (7.58 s) in the final window;
these scopes are inclusive, not additive. Resource upload/materialization bytes
remain high. Attribute the backing churn before another memory optimization.

## Current priority: understand Vulkan emulation before DX12 experiments

Read [the new execution/ownership research](vulkan-emulation-20261011/RESEARCH.md)
and its RESULTS.json. The complete engineering cycle and autonomous isolated
XInput gameplay are already authorized. Do not ask the user to play or reconfirm
these actions. Preserve the original installed 974f8c21-dirty runtime/profile;
use copies, verify no user game is active and bind feeder to PID/start/SHA.

Built-in diagnostics and actual EOP/VideoOut tags now provide internal reference
evidence. Resolve the remaining PM4/resource versions/completion fence/GPU clock
gaps using a pinned compatible-source Vulkan probe if exact dirty source cannot
be obtained. Keep revision differences explicit; measure bounded capture and
probe overhead. Do not treat Present, logged base FPS, generated output or flip
submission cadence as completed guest FPS. Between-submission timestamp gaps are
not hardware idle; detile callsite counts are recorder synchronizations.

All owned games, pads and collectors for this checkpoint stopped. Ordinary
Vulkan teardown exceptions remain unresolved. The new GPU-label session had
collision-limited navigation, not confirmed enemy combat. Complete those scenario
gaps and a matched Vulkan/DX12 scene/config/cache protocol before claiming parity
or speed ratios. Parked local patches/builds under performance-recovery-20261011
are unverified and must not be treated as accepted production work. Continue the
measured DX12 fix/test/gameplay loop after Vulkan attribution is sufficient.
Standing automatic push applies after verified logical commits; no force push.

## Continue the installed DLSS companion

Read vulkan-live-monitor-20261011/VULKAN_LIVE_MONITOR_REPORT.md, HUD_VALIDATION.md,
SESSION_MANIFEST.json and tools/vulkan_reference/XINPUT_CONTROL.md first. The
habitual shortcut is integrated; do not install over the wrapped shortcut or
transplant c7 hooks into the installed 974f8c21-dirty binary. User authorized
autonomous game/control acceptance here; that is distinct from replacing the
core or running incompatible Mode B instrumentation. Explicit virtual-pad runs
must use current verified PID/start, new directory, trusted client SHA and bounded
commands. Do not assume XInput index implies the guest player slot; verify game
reaction. Ordinary monitoring remains passive. Two game exit failures are recorded;
matching internal diagnostics need the exact producer's dirty source/build
manifest and an isolated profile. Base FPS/latency and internal cost are still
unmeasured. Commits for completed verified work are pushed to configured upstream
by the user's standing preference; no force-push or history rewrite.

## Continue the prepared Vulkan reference profiler

Read vulkan-reference-profiler-20261011/REPORT.md, TELEMETRY_COVERAGE.md,
EVENT_SCHEMA.md, DX12_PERFORMANCE_PARITY_MATRIX.md and CONTROLLED_AB_PROTOCOL.md.
One agent. Do not rebuild/start games/open UI while the user plays. Last collector
and game exited: complete 1254-sample capture finalized, automation deleted.
For another explicitly requested passive session, use the start script to verify
new PID/SHA and create a new segment. Use actual PID + advancing status, not stale status.
Mode B exact source/build/isolated profile/launch scripts are prepared in the
handoff. Ask for explicit game-run authorization before executing that launch.
Complete causal frame/dependency/graphics timestamp gaps before claiming full
internal timings. Measure profiler-on/off overhead; keep heavy queries separate
from performance baseline. Watch-proof cost is historically measured in DX12;
indices/detile/indirect source gaps need actual new cost/usage, not GPU percentages.

## Vulkan execution investigation saved — 2026-10-11

Read [the exact-version research](vulkan-session-20261011/RESEARCH.txt) before implementing Vulkan parity.
Reference `c7e065d1` is the observed executable revision; readback mode is Relaxed.
Priorities: bounded operation/dependency attribution, ordinary resident indices, GPU detile for observed
layouts, stable buffer backing/page validity, and GPU indirect execution when opcode counts justify it.
Preserve CPU/GPU ownership, aliases, PRT and fence lifetime; do not remove correctness mechanisms to match Relaxed performance.


## Final — next step after the high-memory experiment

The requested residency/cache experiment is complete; do not repeat the earlier
F1–F3 or implementation-pending work below. Read final BENCHMARKS.md and
F:/PS4Dev/handoff/high-memory-20261011/FINAL-REPORT.md. Preserve the final C launcher.
Do not raise budgets further without new evidence. The remaining large snapshot
and backing-validation CPU work needs complete-frame critical-path attribution.
Optional next implementation: direct ordinary 16/32-bit index residency at
GuestRasterizer::DrawInternal. Integrate it into the common request batch and actual
resource-state owner; test offsets/base vertex, GPU-written indices, aliases,
restart/endian fallbacks and held fence leases before another game comparison.
Single-agent work; auto-push verified commits per the user's standing instruction.


## Active — finish high-memory mode comparison

Do not repeat F1–F3. The current single-agent task is the high-memory experiment.
Code and production regressions are implemented. Complete clean-core identity,
then fresh same-seed 512/2048/4096-MiB runs; 8192 only if DXGI budget permits.
Use F:/PS4Dev/handoff/high-memory-20261011/prepare.py and capture.ps1;
same immutable coherence-20261011/seed, saved world, 1280x800, partial render 0.
Remove validation/API tracing variables from performance child environments.
Do not run builds/tests during a game measurement. Retain the bounded top-16
first-4096-address sample caveat; cache counters are exhaustive, sample is not.
Normalize work by new GPU-completed frames, not repeated presentation.
Index-buffer fast path remains a separate optional follow-up after cache results.
The user's standing automatic-push authorization applies after verified commits.



## Active — remaining frame cost after verified F1–F3

The requested F1–F3 package is complete in `67d0ddc`; do not repeat its implementation
or treat the historical GXWorker fault as the active blocker unless it recurs.
Read the current BENCHMARKS.md tables and external coherence-20261011/final-verification.json.
One next investigation: attribute the remaining ~81k buffer materializations / ~26 GB
per 95–99 s world window to changing request envelopes and cache eviction in
PrepareShaderBuffers/Find/DropCached. Record a bounded sample of allocation/range
changes per completed frame before proposing any F5 DMA/descriptor-bank redesign.
Snapshot work (including safe watch establishment) remains large. F4 resident indices
and F5 redesign are not part of this completed change; do not reduce BankCount or drop pages.
Current world remains ~0.65 FPS, with color/edge artifacts. Modest observed speedup
does not establish playability or pixel correctness. Preserve the clean tested core:
F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd.
Baseline-2 loading c0000096 is a separate intermittent observation; other two baseline
and both fixed runs reached the world. No unrelated exception-suppression workaround.
No push or installation replacement is authorized. Original saves remain untouched.

## Final-run priority — slow world updates after ab1bf1a

The final committed build now reaches the world, responds to control beyond 120 s,
and handles original-instruction protection faults without crashing. Read the final
sections of GXWORKER_PAGEWATCH.md and final-verification.json first. Do not repeat
the original fault investigation unless it recurs. The next single priority is
full guest CPU/PM4/GPU/flip correlation, then attribution of buffer snapshot comparison
and materialization to identical world frames. No performance improvement is claimed.
The precommit mutex wait below is an intermittent fallback investigation, not the
final launch result. Preserve the isolated dev launcher and original saves. No push.

## Current priority — loading wait after confirmed PageWatch fix

This overrides all older plans and push instructions below. **Do not push.**

1. Read GXWORKER_PAGEWATCH.md and external
   `F:/PS4Dev/handoff/gxworker-20261011/final-verification.json` for the final
   committed-build result. Use `dev-release/Launch-Bloodborne.cmd`, exact SHA
   and matching map/PDB. Preserve originals, partial render 0 and removed assertion NOP.
2. Original GXWorker failure is a proven retired-range lookup defect; preserve
   coalesced history, generation handling, recursion guard and mixed 0/F CMASK.
   Targeted real-fault regression fails before and passes after; 208/208 passed.
3. If the loading wait repeats, examine the saved Game:Main -> MOMainThread mutex
   ownership chain, raw thread contexts and guest call sites. Owner is sleeping via
   sceKernelUsleep; GPU worker waits for commands. Identify the guest condition and
   lock lifecycle before modifying HLE; never force unlock or bypass the wait.
4. Once the world updates, correlate real guest flip IDs across SubmitFlipInternal,
   Liverpool PM4, resource preparation, GPU submission/fences and presentation.
   Attribute snapshot comparisons/materialization to complete frames before optimizing.
   Present counts and nested stage totals are not FPS. Correct world pixels remain open.

The earlier generation-only world/control run crashed at t~357 s. It must not be
reported as successful validation of the complete fix. Exact final-build observations
are in the external verification report; no LEVEL 5–7 is inferred from tests.

## Current priority — saved-character loading after recursion fix

This supersedes every older plan below. Do not repeat the setup-profile/audio audit.

1. Check current Git HEAD and read FULL_FRAME_PROFILE.md and GAMEPLAY_BASELINE.md.
   Preserve mixed CMASK and the request-local ResolveGpuBuffers recursion fix.
2. Use a new copy of `C:/Users/skei/AppData/Local/PS4Desktop/native-profile/user`,
   with its existing character and Skip Intro. Rewrite absolute General paths. Remove
   only assertion NOP `0x024B56D2` from the copied patch; preserve original saves/config.
   The remaining 1280x800 patch includes behavioral changes, a comparison caveat.
3. Build the current core and verify the launched executable hash/path. Installed
   desktop core was not updated by this checkpoint. Prior fixed executable/map/PDB:
   `F:/PS4Dev/handoff/freeze-20261011/fixed-core/`. Set partial render explicitly to 0.
4. Attach before GXWorker:1 write fault at `eboot+0x26b2b0b` (last address `0x24347e980`).
   Capture original context, page protection, ownership and all stacks. Determine whether
   PageWatch should handle the write or the guest address is invalid; fix only with evidence.
5. Validate with targeted regression and rerun the same saved-world scenario. After a
   stable world frame, complete CPU/PM4/GPU/present correlation and full-frame measurements.
   Neither Present counts nor nested stage totals prove FPS; old 0.0064ms remains invalid.

Prior run: baseline stack overflow proven; fixed regression and 208/208 native tests pass.
Fixed game run ended earlier at a different guest fault, so no game improvement is proven.
Separate captured CMASK code 2 and audio mutex lifetime issues remain in the report.
All session-owned game/debugger processes stopped. Commit and push verified changes per
the user's standing authorization. Keep evidence/binaries/saves outside Git.

## Integration checkpoint 2026-10-11

2026-10-11 priority update (supersedes older plan below):
1. Read GAMEPLAY_BASELINE.md. Freeze one copied full profile/home and rewrite absolute
   General paths before either backend launch. Preserve original Vulkan saves/config.
2. Capture real crash stack and preceding assertion condition at eboot+0x20b56d2,
   Mv_AudioPlayThread; investigate relevant AvPlayer/audio HLE results. Do not suppress
   the null write. LLDB needs bundled Python 3.11 on PATH; attach 0x80000003 is not
   the game fault. Use the exact build map. Current fault logger stack words are zero.
3. Separate renderer blocker: capture bounded CMASK contents, target descriptor and
   metadata writes at setup. Compare upstream EliminateFastClear/TextureCache tracking.
   Exact-pixel fixture required; do not skip mixed metadata or clear whole mixed surfaces.
4. Paired isolated Vulkan run and bounded semantic capture remain outstanding.
5. Preserve lookup/batching/query ownership/draw-filter fixes; repeat identical scenario
   after target regression and native suite. No FPS claim without a verified game scene.
Evidence: F:/PS4Dev/handoff/gameplay-a824f9f, gameplay-range-after,
gameplay-range-saved-profile. Historical pre-e43ba4d GPU times remain invalid.

Start with PORTING_ROADMAP.md and VULKAN_DX12_PARITY.md. Extend the corrected draw classifier
with a GuestRasterizer memory-side-effect integration fixture. Do not use the old 0.0064 ms GPU timing as evidence; repeat
timestamp measurements only with the corrected submitted-context lifetime checks.

Continue from the copy-batching checkpoint and prioritize the volume of guest-memory coherence
work inside `resource-upload-dispatch-writeback`.

1. Attribute the guest addresses and request sizes responsible for the largest snapshot,
   upload and readback byte counts, with bounded sampling rather than per-call logging.
2. Determine whether shader metadata overstates writable ranges or whether the game really
   consumes the complete GPU-written regions on CPU.
3. Preserve the new asynchronous GPU timestamps; expand them only around a confirmed GPU-side
   candidate because compute execution currently averages about 0.0064 ms.
4. Investigate safe dirty-range precision only after proving which bytes are written. Do not
   narrow ownership based on assumptions about shader access.
5. Keep copy batching queue order, allocator fence ownership, overlap handling and exact guest
   memory coherence covered by the native tests.

## CMASK-M01 continuation

Verify the equation against a captured Bloodborne CMASK surface (the opt-in `SHADPS4_CMASK_CAPTURE` path is available) and run the post-change core from the cloned `F:/PS4Dev/handoff/mixed-cmask-20261011/after` profile. Keep audio assertion evidence separate.
