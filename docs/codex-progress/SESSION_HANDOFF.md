# SESSION HANDOFF — active full-port checkpoint, 2026-10-16

## Current independent BC block expansion - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

### Production changes

Pushedfdb1e27 implements the confirmed pinned ResolveOverlap -> ExpandImage -> CopyImage
producer through existing private GPU retile/snapshot/upload, retaining old GPU image I
independently of physical/resident buffer B. This supersedes the previous BC investigation's
omitted-producer prerequisite; no new HLE same-format copy producer is needed first.

MatchesReferenceBlockImage supplies one exact predicate to early source selection and
sticky modified-history inheritance. It admits UINT64/128 block words to matching BC bytes,
reverse UINT block-texel access, and numeric variants of the same BC codec. It requires
single-mip, single-sample ordinary2D/array storage, complete logical4x4 correspondence,
identical physical mip bytes/offsets/tiling/layers and no expanded native decoding. It
rejects linear8/31, odd/small/explicit padding-storage, volume/1D/attachment/depth/stencil
shapes and incompatible codec identities. Equal-size different BC codecs never become
compatible through this bridge.

DescribeGuestImageOwnership validates readonly BC using a canonical writable UINT64/128
physical geometry with width/height/pitch divided by4. Actual BC descriptor/resource stays
readonly; BC UAV/RT admission and the rasterizer's retention whitelist are unchanged. All
three ownership-retention callers already consume the canonical descriptor/layout. Existing
page-alignment, complete-backing,64MiB, watches and fence requirements stay. RetileBufferVersion
now validates row bytes in physical BC blocks, retaining the existing raw GuestTiler path.

Independent review found that the generic Upload overlap fallback could bypass the new
predicate and copy a different codec's I. It was corrected before commit: distinct BC codec
keeps old I and refreshes its new view from B; other unsupported block-domain conversions
fail explicitly. Existing non-BC fallback remains. Source pins survive private transfer and
new ownership acquisition before old retirement. Only successful refresh publishes inherited
logical modified/clean history; no physical B commit or fake shader image write is added.

### Prepared verification and evidence

Pushed25ba00c authors two additional real-GCN BC1/BC3 cases in gcn_bc_block_expansion_fixture.h,
called from the existing tracked gcn_ordered_formatted_fixture.h. They extend the existing
--ordered-formatted-owned mode; the older28 cases keep their expectations. Valid compressed
red/green endpoint words start in a UINT image; actual image writes make I differ from B,
then an interior raw buffer writer changes B without exact-base image invalidation. BC
samples test decoded texels; reverse UINT/formatted reads test compressed DWORDs and full
physical backing/padding. Authored controls are uncompiled/unrun; see fixture source proof
and independent review for exact implemented cases and limitations. The historical attachment
runner remains inherited/untracked integration, preserved in residual recovery.

F:/PS4Dev/handoff/bc-block-expansion-20261016 holds source snapshots/own.patch,
REFERENCE_EXPANSION_CONTRACT.md, INDEPENDENT_REVIEW.md, MEMORY_REVIEW.md and fixtures/.
The initial fallback finding and its final resolved disposition remain explicit. The earlier
writable mixed-domain concern was withdrawn after tracing the existing exact-base writer
admission and invalidation; do not add the speculative extra restriction. CHECKPOINT_FINAL.json
records actual final local/remote HEAD, owned hashes, original22 staged deletions and the
exact final-residual archive. Restore only in a separate checkout of its manifest.head;
never overlay the active dirty workspace. Older archives/checkpoints remain intact.

Installed preview24 remains unchanged at C:/Users/skei/AppData/Local/PS4Desktop/cores/
dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe, SHA256
70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Source provenance is still a3f1d5e plus archived integration; selected core was unchanged,
last known preview23. No new core was built or installed. Original Vulkan, saves, settings,
monitoring and the user/remote session were untouched. PDB/runtime paths remain in the
preview24 installation manifest. The pushed source still requires broad inherited residual
integration: clean checkout alone is not a reproducible full core. Historical326/346
(native211/229), three targeted passes, Vulkan/DX12 captures/FPS are separate older evidence.
Bloodborne world/image/controls/FPS acceptance and historical EOP attribution remain open.

### Exact next source continuation

Extend the BC block bridge beyond single-mip only after reading NEXT_BC_MIP_CONTRACT.md.
Update IsIndependentGuestBlockStorage / canonical ownership and MatchesReferenceBlockImage
with per-mip logical block correspondence and full physical mip identity. Extend forward
and same-codec BC chains first; reverse uncompressed block-texel views retain levelCount1
until selected single-mip view integration is proved (VUID07072). A global4x base
relation is insufficient: source width24 versus BC96 has source width1 at mip4 while BC
requires2 blocks. Preserve valid smaller mip blocks, source-domain choice, padding, codec
isolation, failure atomicity and the existing block-aware retile path. Add meaningful
unrun mip controls, without changing old fixture expectations. Broader linear/expanded/odd,
BC shader-write/attachment and arbitrary overlap cases remain separate unimplemented or
unverified contracts; do not silently widen them or mark this group complete.

Reuse the previous MSAA depth-init/UINT-stencil fixes and authored focused mode from
msaa-depth-init-20261016. Do not repeat BC producer/format, mapped-generation, geometry,
heap-pressure, readonly-history, writable depth-feedback or blanket alias-barrier audits.
No build/test/game/controller/desktop execution is authorized yet. Continue other existing
production contracts when a runtime dependency blocks one item; keep the full-port goal active.

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read this
SESSION_HANDOFF current section, STATE/NEXT_SESSION and registry; compare actual HEAD/upstream
with bc-block-expansion-20261016/CHECKPOINT_FINAL.json. Preserve all dirty integration/index
and use the separate recovery archive only in another checkout. Tests/build/package/compiler,
GPU/game/XInput/UI remain deferred. fdb1e27 BC block bridge and prior MSAA fixes are source
changes with authored/unrun verification. Continue the precise BC multi-mip production task
using NEXT_BC_MIP_CONTRACT.md; do not redo saved proofs or invent broad codec compatibility.
Use saved Vulkan/DX12 baselines; only when execution is authorized build with pinned tooling,
run affected components/regressions, minimal isolated virtual-XInput gameplay and real completed
frames versus the saved baseline, preserving Vulkan and user saves/session. Commit/push
coherent WIP and checkpoint exact continuation; full renderer parity remains incomplete.



## Current MSAA depth initialization and unsigned stencil extraction - 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

### Production changes

Pushed0682d65 initializes the fully replaced MSAA depth/stencil plane before its upload
shader draw. GuestMultisample::Lease::Record transitions to DEPTH_WRITE then uses its
retained one-layer DSV with exactly DEPTH or STENCIL clear flag. Actual fullWidth/Height
and MipLevels1 are required. Other planes/layers and pixels outside a partial viewport
are preserved. Color discard behavior, cache identity, guest metadata, logical writes,
physical backing, descriptors and transfer/fence lifetime remain unchanged.

Pushed9b5b356 classifies X32_TYPELESS_G8X24_UINT as uint4 in ValueType. Previously actual
production plane1 MSAA readback built TEXTURE_TYPE=float4 for this unsigned stencil SRV,
then converted into an R8_UINT output. The existing G-to-X mapping stays; integer source
and output now agree. GuestImageReadback::RecordTiled's plane1 path is the reachable user.
Neither change establishes runtime correctness of depth/stencil or renderer parity.

### Prepared acceptance, not executed

Pushedf067efb registers native_msaa_depth_initialization with native-independent-version-transfer-test
--msaa-depth-init. The authored focused mode uses fresh placed D16/D32 resources without
fixture clear/discard/copy before production upload. A separate compute sample-load oracle
checks complete per-layer/sample/pixel values and a canary tail; D32/stencil are exact,
D16 tolerance1e-6 is below one normalized D16 step. Three-layer combined DS controls seed
independent high-bit stencil0x80/0xa5/0xff values, check depth-only preservation, both upload
orders where PSSpecifiedStencilRef is supported, and7x5 partial uploads into13x11 initialized
resources. A separate production stencil extraction check includes every pitched padding
byte and runs even when shader stencil export is unavailable. Required InfoQueue plus severe
message checks retain the placed initialization acceptance oracle. All are uncompiled/unrun.

### Evidence and preserved state

F:/PS4Dev/handoff/msaa-depth-init-20261016 contains source/ and stencil-type/ snapshots,
REFERENCE_CONTRACT.md, INDEPENDENT_REVIEW.md, REVIEW_STENCIL_TYPE.md and fixtures/ exact
before/after/patch/proof plus fixtures/FIXTURE_REVIEW.md. CHECKPOINT_FINAL.json records actual final
local/remote HEAD, source hashes, original22 staged deletion preservation and final-residual.
Restore that archive only into a separate checkout of its recorded HEAD using its README;
never overlay the active dirty tree. Older checkpoints and evidence remain immutable.

The initial byte-index assertion accidentally compared a short-index Git patch with a
full-index snapshot; canonical --binary --full-index comparison matches original exactly.
No foreign index/source change was made. Existing large/casting/shared integration remains
residual: a clean checkout of these commits alone is NOT a reproducible full core.

Installed preview24 remains at C:/Users/skei/AppData/Local/PS4Desktop/cores/
dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe, SHA256
70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Source provenance remains a3f1d5e plus archived integration. No new core was built, installed
or selected; selection last known preview23. PDB/runtime locations remain in
F:/PS4Dev/handoff/eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Vulkan, original saves, settings, monitoring and remote/user session were untouched.
Historical326/346(native211/229) and three targeted passes are separate older results;
Bloodborne final-world correctness, controls/FPS and historical EOP cause stay unresolved.

### Boundaries and exact continuation

Generic partial upload into an already initialized larger depth target preserves outside
pixels. Fresh partial-first-use lifecycle is not solved by this guarded clear; production
initial depth Upload creates exact logical dimensions and reaches the full-plane path.
NEXT_TRANSFER_GEOMETRY_CONTRACT.md found no confirmed production-reachable geometry or
arithmetic mismatch; do not replace preservation with blanket clears or speculative guards.
Pinned writable depth feedback must be resolved from NEXT_DEPTH_FEEDBACK_CONTRACT.md;
absence of native simultaneous writable feedback alone is not a proved parity defect.

NEXT_BC_IMAGE_IDENTITY_CONTRACT.md already proves no current native independently modified
BC producer; exact format/number direct reuse exists, family numeric reinterpretation does
not. Do not repeat that proof or widen identity alone. Next exact source boundary is pinned
IsComputeImageCopy/CopyColorAndDepth versus native ComputeImageOperation,
GuestImageCopyCompatible and DescribeGuestImageOwnership: establish/adapt a real exact
compressed-copy producer, source residency and physical-padding/logical-write/fence contract
before extending BC retention or matching. Read NEXT_SOURCE_TASK.md; never equate unrelated
BC codecs merely by block bytes or legalize BC UAV writes. Then continue the existing registry.
No broad audit, blanket alias barrier, unsupported MSAA pressure guard lift or repeated
Vulkan/DX12/game experiment is justified. Existing heap-pressure, mapped-generation, readonly
history and original-slot fixes are already saved and must not be redone without new evidence.

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read this
SESSION_HANDOFF current section, STATE/NEXT_SESSION and current registry; verify actual HEAD
and upstream against msaa-depth-init-20261016/CHECKPOINT_FINAL.json. Preserve all dirty work
and original index split. The execution gate remains active: no build/package/compiler,
tests/game/GPU/XInput/UI until final authorization. Do not redo completed source proofs or
attribute old326/346/FPS to current source. Depth initialization and UINT stencil extraction
are source fixes with authored/unrun acceptance. Continue the precise compressed-copy
producer/ownership boundary using NEXT_SOURCE_TASK.md and saved BC proof, then remaining
applicable production contracts. Use existing
Vulkan/DX12 evidence; when execution is finally authorized build with pinned tooling, run
changed components/regressions, minimal isolated virtual-XInput gameplay and real completed
frames against the saved baseline, preserving user saves/session/Vulkan. Keep the full-port
goal active, commit/push coherent WIP, retain exact residual recovery and next task.



## Current texture heap pressure and idle reclamation - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
No configure/build/package/compiler/test/GPU/game/XInput/UI execution. All129 groups
remain partial/open; source/Git/hash reviews do not establish runtime correctness/FPS.

### Production continuation

Pushedc6e5429 integrates typed TextureBudgetExhausted with actual placed-heap pressure.
Reduced budgets no longer clamp to256MiB. Heap size fits remaining commitment with
checked64KiB/4MiB alignment; MSAA heap alignment is independent of resource placement,
so small64KiB MSAA cannot reuse an incompatible64KiB-aligned heap. Device/format/
arithmetic failures retain their own errors. Cache AllocateTexture routes all ordinary,
sample-backing and packed-clear/conversion allocations through retired-pool reclamation,
existing eligible LRU eviction and bounded GPU-source retirement retry. No retry spins
without changed budget, reclaimed storage, an erased victim or retired source bytes.

ReserveMemory and AllocateTexture retain registered caller pins transitively; a chosen
victim is pinned across callbacks. A resolver can restore sample backing and recursively
evict another Entry, so eviction rechecks the chosen UID and reacquires its vector
iterator after callbacks. Newly acquired external bindings fail closed before erase.
This fixes the independently found recursive lifetime issue. Dirty independent images,
valid tiled independent images and unsupported MSAA pressure victims remain excluded.

Pushed50860ef additionally reclaims wholly unleased texture heaps inside over-budget
SetBudget, without waiting or invalidating live Resource/shared leases. Otherwise a
fully empty256MiB page could be reused indefinitely after a64KiB budget reduction,
never reaching CreatePage's trim. Allocation retry recognizes actual commitment
reclaimed by refresh as well as a larger limit.

Pushede00c72d authors native_texture_heap_pressure / native-buffer-budget-test texture-heaps.
This optional mode returns before the original large buffer batches. It covers live
Resource copy then standalone allocation lease, budget shrink/eager idle release,
remaining-capacity/RT-class fitting, exact tiny GPU texels, typed-versus-invalid failures
and conditional small-MSAA alignment. It is uncompiled/unrun. The earlier28 ordered
GCN scenarios are separate authored/unrun acceptance, not additional PASS results.

### Exact evidence and preservation

F:/PS4Dev/handoff/pressure-retirement-20261016 contains REFERENCE_CONTRACT.md,
INDEPENDENT_REVIEW.md, allocator/ and fixtures/ exact snapshots/patches/hashes/reviews.
The initial review's recursive-victim finding was corrected; its final disposition
supersedes that earlier blocker. allocator/eager-trim and NEXT_IDLE_HEAP_CONTRACT.md
retain the follow-up proof. NEXT_MAPPING_CONTRACT.md confirms existing cancellation
covers notified unmap/remap; do not reimplement native-priority-inflight-backing-generation.

Allocator format-casting and buffer fixture large-mode changes were inherited dirty
integration. Isolated indexes committed only owned changes; main argc/argv is a necessary
new fixture-mode dependency. Foreign casting/large behavior stayed residual; original22
staged deletions remain unchanged. These commits alone are NOT a standalone reproducible
full-core source publication. CHECKPOINT_FINAL.json records actual final local/remote
SHA, exact staged/index/source hashes and final-residual archive. Restore only in a
separate checkout using its README; never overwrite the live dirty tree. Older archives
remain intact.

Installed preview24 still has SHA25670eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf
at C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe.
Source provenance remains a3f1d5e plus archived integration, not current source. Selection
was unchanged (last known preview23). PDB/runtime paths remain in the prior installation
manifest. Vulkan, original saves, settings, monitoring and remote session were untouched.
Historical326/346(native211/229) and three targeted passes remain old separate evidence.
Bloodborne final-world/image/FPS/controls acceptance and historical EOP cause remain open.

### Exact next production change

Read NEXT_PLACED_ALIAS_CONTRACT.md. Normal fence-retired heap reuse fits Microsoft's
advanced placed-resource model across distinct ExecuteCommandLists; no blanket missing
alias-barrier fix is justified by absent Aliasing call sites. The separate confirmed gap
is new placed MSAA DEPTH initialization: Upload accepts depth-only R32 samples2, then
GuestMultisample::Lease::Record(upload=true) transitions to DEPTH_WRITE and DrawInstanced
while its discard path explicitly excludes depth. No required clear/discard/full copy
precedes that initial depth draw. Implement correct first-use depth metadata initialization
in guest_multisample.cpp / Upload integration, preserving full-layer versus partial
regions and every other depth/stencil plane. Reuse existing transfer leases/state/DSVs;
never discard stencil or old layer contents merely to initialize a different plane.
Author exact per-sample depth/stencil/layer acceptance without executing it while gated.

Pinned GC valid modified LINEAR single-sample download writes compact mip0 physical
prefix without merging resident B; tiled valid modified images are retained. Generic
linear-MSAA download in pinned source attempts an invalid multisampled copyImageToBuffer
without extraction/resolve/sample multiplier. This is a reference limitation, not proof
for lifting native MSAA eviction guards. Continue the existing registry after the depth
initialization fix; broader formats, identity/aliases/pressure/device-loss and runtime
acceptance remain open.

### NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read this authoritative top
and pressure-retirement-20261016/CHECKPOINT_FINAL.json; verify current HEAD/upstream and
preserve the dirty/index split. Reuse saved Vulkan/DX12 data and completed source proofs.
Implement the confirmed initial MSAA depth initialization gap from NEXT_PLACED_ALIAS_CONTRACT.md,
then continue production contracts. Do not repeat completed mapping/heap-budget/alias-
synchronization investigations. All execution stays deferred. When authorized, build
changed targets, run focused and required regressions, then minimal existing-XInput
Bloodborne checks with completed guest frames excluding FG against saved baseline.
Preserve Vulkan/saves, checkpoint/push coherent WIP and exact residual source. Full port
and reproducible publication remain incomplete until all applicable acceptance succeeds.


## Current readonly history and packed-source continuation - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only continuation: no configure/build/package, fixture/test/compiler execution,
GPU/game/profiling/XInput/UI operation. All129 contract groups remain partial/open.
No current runtime PASS, image acceptance, FPS or performance result is claimed.

### Production state

Pushed36b109c adds Entry logical_gpu_modified/logical_dirty independently of ownership.
Own records original conservative storage-write history even if physical acquisition
falls back. CPU overlap and exact original-base GPU invalidation retain modified history
while marking dirty, including history-only Entries and writing CPU-fault paths.
Successful current Upload restores validity; compatible history witnesses are pinned
across allocation. Generic/private snapshots stay dirty. Explicit current-B snapshots
require same-base original writer provenance plus a compatible dirty modified witness.
Readonly restoration invokes independent acquisition directly, without fake Own/write
notification or linear download. Original writable slots alone drive shader Own/readback.
HLE clear notes its real write. Failed/partial uploads cannot authorize formatted retile:
selected formatted sources and central RetileBufferVersion reject logical_dirty.

Pushed01f916c preserves clean I beside an interior writer even when the requested readonly
view requires existing packed GPU conversion rather than direct native view reuse.
FindIndependentUploadSource first uses direct reuse, then admitted same-base clean packed
sources with complete equal physical mip geometry. Different authoritative backings fail
closed. Upload prefers that same conversion source and its existing domain-preserving
fallback; a conversion-required resource is never bound as an incompatible direct view.
Capture-skipping and replacement witnesses retain cache identities through preparation.
No new format/sample/domain admission is implied by this selection correction.

## Callback drain and authored submission controls - 2026-10-16

Collect now invokes every already-ready retirement callback once even if an earlier one
throws, then reports the first callback failure. It previously destroyed the uncalled tail.
Independent source review: scheduler-batch-20261016/callback-drain/REVIEW.md. New authored
scheduler_submission_fixture.h covers normal/throwing publication with an external queue
gate, three real upload/readback copies, callback-only lease weak references, no replay,
exact fence results, and a throwing retirement followed by another ready retirement.
Debug messages are attributed by cursor, old messages retained, overflow rejected. Fixture
source review remains distinct from runtime acceptance: no build/test/GPU execution.
Fault-injected allocation/Signal failure and device timeout remain unverified. Existing
priority tests and scheduler integration were preserved outside these scoped commits.

Next production contract is writable BC selected-view transaction per
F:/PS4Dev/handoff/bc-writable-view-20261016/REFERENCE_CONTRACT.md. Child writes must merge
into one parent-I seeded replacement; mixed parent/child reads in one dispatch and multiple
children require explicit coherence/grouping, otherwise fail closed before dispatch.

## Scheduler submission lease block - 6f55ea1, 2026-10-16

Retirement callbacks are now copied into scheduler-owned pending nodes BEFORE copy/direct
queue execution. All node/vector allocations and rollback occur before submission. Nodes
park at UINT64_MAX until a known direct fence is assigned under lock without allocation.
An additional no-op node holds copies of every publication callback's captures until that
fence, even if publication throws and caller unwinds. Any Submit exception latches terminal
recording failure: Begin/SubmitCopy/Submit reject replay. Destructor best-effort drain uses
fresh copy/direct signals and waits, never re-executes lists. Device removal/timeout teardown
remains unverified. Conservative pre-execution allocation failure also terminates recording.

Independent review covered live and exact isolated committed candidates. Existing inherited
scheduler diagnostics/priority changes remain separate in the dirty workspace. Original22
staged deletions were preserved byte-for-byte. Exact candidates/patches/review/commit identity:
F:/PS4Dev/handoff/scheduler-batch-20261016. Uncompiled/unrun; no runtime PASS or FPS claim.
Previous simple scheduler-retention experiment is superseded.

Next: implement meaningful authored failure controls for prequeue allocation, Signal failure,
throwing publication and non-replay/resource lifetime (execute only when authorized), then
continue writable selected-mip authority and remaining renderer contracts. New partial BC
fixture3488541 is authored/source-reviewed, not run; uncovered false-query B==P limitation
remains. All129 groups remain partial/open. Preview24/core selection unchanged.

Partial BC fixture is now authored and independently source-reviewed (2026-10-16).
New gcn_bc_partial_expansion_fixture.h adds both real target GPU-page query branches for
UINT24->BC96,5mips,2layers,36864bytes, three uncovered final-mip blocks perlayer, full
compressed/padding readback and physical64KiB oracle. Source proof/review/snapshots:
F:/PS4Dev/handoff/bc-partial-copy-20261016/fixtures/. False-branch uncovered B==P remains
a documented limit; special reflected operands are not asserted. Extra terminalmips and
reverse selected views still lack fixture coverage. No build or test execution. Existing
expectations retained. Next production work remains durable scheduler submission batches
from scheduler-retention-20261016/DISPOSITION.md; no unsafe exploratory fix is live.

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

## BC mip fixture oracle review update - 2026-10-16

92af292 adds a GPU snapshot baseline of the complete 4 KiB output region before the
formatted chunk walk. Each chunk still records and compares the full output region after
Finish, so writes outside the selected 1 KiB destination window are now checked against a
run-local GPU baseline. The earlier independent review confirmed the readback source and
destination lifetimes are fence-safe and avoid guest-memory reconciliation. The baseline
addition itself has not been independently reviewed or run. Fixture still ignores special
reflected operands and does not assert sampler/component-mask metadata; those are review-noted
coverage limits. It also still has no reverse multi-mip UINT-view scenario.

The independent production review corrected an earlier concern: writable UINT reverse access
from a clean multi-mip BC parent fails closed in the block-domain guard before acquiring a
child or retiring the parent. The real gap is missing writable parity, which needs shared
parent/subresource authority or explicit writeback. Nonzero views and dirty parents remain
fail-closed. Review report: F:/PS4Dev/handoff/bc-mip-bridge-20261016/
SELECTED_MIP_IMPLEMENTATION_REVIEW.md; fixture report: FIXTURE_REVIEW.md.


## Selected BC mip view continuation - 2026-10-16

Source WIP 923d637 is pushed. A readonly UINT block-texel view selecting mip0 from a
multi-mip BC parent now has a dedicated private snapshot path. It preserves the full BC
parent as the sole authoritative owner, leaves the copied child logically dirty, and does
not acquire child ownership or retire the parent. Prefix layout is normalized to allocation
mip0 even when the parent's original shader view selected another mip. This is source-only
review; no compile, fixture, runtime, or game acceptance.

The current authored multi-mip fixture still needs its output oracle corrected: guest
ReadMemory can resolve a covering GPU mapping and collapse source/target dirty distinctions.
Author is switching the chunk check to independent native GPU readback while retaining the
full physical backing checks. After freeze, review whether the fixture exercises readonly
selected-mip0 reverse access and confirms every parent mip remains intact. Exact evidence is
in F:/PS4Dev/handoff/bc-mip-bridge-20261016/selected-source and fixture source snapshots.

Next source boundary: prove writable UINT alias behavior and nonzero selected mip ranges;
they remain fail-closed/unimplemented. Partial-chain forward copies must choose uncovered
bytes from the pinned Vulkan refresh source (synchronized B when GPU-modified, P otherwise),
using one shared target seed; see NEXT_PARTIAL_BC_COPY_CONTRACT.md. No alias coherence or
full BC parity is claimed. The original22 staged deletions were verified byte-identical.


Pushedd55fb2d additionally preserves pinned-compatible ordinary uncompressed color
interpretations (8/16/32/64/96/128 guest bits), including R32_UINT to R16G16_UINT.
Full physical geometry and normalized image identity must match. This is an explicit
pinned same-image class witness, not arbitrary native overlap. Compressed blocks,
depth/stencil and attachment identity expansion are excluded. Non-direct sources use
existing independent retile/detile; compatible sticky modified history follows the
explicit replacement. Native format/sample/retention guards still govern admission.

### Evidence and verification

Evidence root: F:/PS4Dev/handoff/readonly-revalidation-20261016.
SOURCE_CONTRACT.md, INDEPENDENT_SOURCE_REVIEW.md, cache/, production/ and packed-source/
contain exact deltas/hashes, primary source proof and independent reviews. The
reference-class/ review and NEXT_SOURCE_BOUNDARY.md establish the now-implemented
R32/RG16 source identity correction. The first
review's missing dirty-source guards were corrected; its final disposition supersedes
that earlier finding. 2f8522f commits four additional real-GCN readonly-history scenarios, authored only
in gcn_readonly_revalidation_fixture.h and the existing ordered-formatted-owned mode:
exact-base prewrite-B refresh, interior-I preservation, R32-to-RGBA8 packed preservation
and R32-to-RG16 exact component preservation. All four remain uncompiled/unrun; together
with earlier24 there are28 authored scenarios, not28 PASS results. The initial seed
native_raw32 assertion was corrected from primary frontend source, with prior source
and explanation preserved in fixtures/before-raw32-fix/. All source/manual/Git/hash/archive
checks are distinct from runtime evidence. See fixtures/ for exact current case manifest.

The original22 staged network deletions and accumulated unknown-origin integration are
preserved. CHECKPOINT_FINAL.json records exact final HEAD/upstream, commits, source/index
checks and final-residual archive identity. Restore only into a separate checkout using
its README; never overlay the live dirty tree. Older archives remain unchanged.

Installed core is still preview24, source a3f1d5e plus archived dirty integration,
SHA25670eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf at
C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe.
Current source is not built or installed. Selection was not changed (last known preview23).
PDB/runtime provenance: eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Original Vulkan, saves, settings, monitors and remote session were untouched.
Historical326/346(native211/229) and three targeted passes remain old separate evidence.
Bloodborne world-render/FPS/controls acceptance and historical EOP attribution remain open.

### Exact continuation and deferred controls

Do not reimplement this modified/dirty history or repeat completed direct/packed source
proofs. Reuse the new source-selection boundary evidence before extending any remaining
read_independent reinterpretation case: direct, packed and reference-compatible ordinary
color classes are now covered; arbitrary incompatible overlap is still not the same
logical pinned image merely because native transfer exists. Add/finish
never-modified sampled, CPU-write, private-snapshot, failed-retention and unmap/remap
controls using the current fixture helpers, without execution while the gate remains.
Then continue GuestRasterizer::RetireImageForPressure and TextureCache ReserveMemory/
RetainPressureSource contracts: extend remaining admitted independent MSAA/format
retirement only with a domain-preserving physical-commit/lifetime mechanism; the current
compact path is single-sample linear and broader retirement remains open. Reuse saved
pressure/source-residency evidence and existing leases; do not restart a general audit. Broader formats/MSAA/attachment/aliasing/cross-stage/metadata
contracts and device-loss/pressure/runtime acceptance remain open.

### NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read this authoritative top
section and readonly-revalidation-20261016/CHECKPOINT_FINAL.json; verify actual HEAD and
upstream, preserve the original staged/unstaged/untracked split. Reuse saved Vulkan/DX12
measurements and source reviews; do not repeat completed audits. Continue the specific
readonly-history acceptance controls/source-selection boundary and existing production
C++ pressure/retirement queue from this checkpoint. Keep build/tests/game/UI/XInput
deferred until authorized. Once authorized, build changed targets, execute focused then
required regressions and minimal existing-XInput game checks; compare completed guest
frames excluding FG with saved baseline. Preserve original Vulkan/saves, checkpoint and
push coherent WIP; never claim full port completion from source review alone.


## Current ordered ranges, descriptor state and compiler groups - 2026-10-16

Authoritative current section; earlier current/next statements are historical.
Source work only: no build/configure/package, test execution, GPU/game, profiling,
XInput or UI operation. All129 groups remain partial/open. No runtime PASS or new FPS.

### Production changes and source evidence

- ccacee9: exact-base DWORD-aligned short/full/long writer sharps preserve original
  WriteRange and binding bounds. A readonly full-image preparation envelope is added
  only for a scheduled retile lacking coverage. Reader minimum16KiB-block coverage
  proves direct sparse backing; the earlier covering-writer branch remains conditional
  on pending binding history. Already resident Vulkan memory stays shared across arena
  migration; different handles alone do not separate contents.
- e877a3f: stream selection includes initial GPU-modified state AND every strictly
  earlier original writer's4KiB query pages. Byte-disjoint same-page writes can make
  a later small reader resident. Both formatted image/metadata synchronization gates
  use this ordinal fact. Optimized-away original Guest buffer slots still prepare
  cache state, invalidation and declared write spans; actual descriptors require Layout.
  Fallback access respects a live SRV-only guest class; production uses unified UAVs.
- 4f66433: multiple disjoint independent image domains can prepare in one compute
  command. Retile deduplicates by Entry identity, not globally. Per-domain unique image,
  exact-base UAV, capacity/ownership/format and no-image-operands/DMA guards remain.
  All private captures and retained retiles precede logical writer invalidation.
- f9ec214: valid grouped Restrict/NonWritable no longer causes blanket SSBO rejection.
  Deterministic sanitized groups preserve other annotation payloads and original
  unrelated targets. Clone annotations precede group declarations, IDs/bound are
  checked, member pairs survive, empty applications disappear, and Aliased deduplicates.
  Shared SSBO/UBO pointee members retain the inherited shared-type annotation boundary;
  UniformBlock remains intrinsically readonly. Malformed group targets are rejected.
  Previously accepted valid modules rewrite identically, so compatible old DXIL entries
  remain usable; cache namespace stays native-dxil-cache-20261015-ssbo-alias-v2.
- 792f3fe: original image slots prepare Upload/cache/ownership/readback/lifetime even
  without surviving native reflection. Actual descriptor writes still require Layout.
  Null, zero-count/range validation, FMASK separation and sampler behavior remain.
  This fixes the preparation gate, not the distinct readonly revalidation gap below.

### Verification and preservation

fc7515e, a895015 and ce156e7 author24 real-GCN scenarios in existing
ordered-formatted-owned mode:22 single-domain cases plus2 disjoint-domain cases.
They cover exact writer/OOB bounds, GPU-I-tail versus physical-B-tail, short/long reads,
prior same-page binding writes, repeated aliases and whole64KiB padding. Case22 now
establishes a real64KiB raw residency envelope and rejects premature retirement; the
unwarmed setup defect and exact earlier source are retained. All24 remain uncompiled/
unrun. Grouped-SPIRV validation/conversion and no-resource replacement acceptance are
also deferred. Source/manual/Git checks are not runtime evidence.

Evidence: F:/PS4Dev/handoff/ordered-writers-20261015/EVIDENCE_INDEX.md; exact before/after,
patches/hashes and independent reviews are in production/, fixtures/, binding-order/,
disjoint-domains/, grouped-ssbo/ and unreflected-images/. CHECKPOINT_FINAL.json records
actual final local/remote HEAD, commits, original index comparison and archive hash.
final-residual preserves historical nonignored sources/assets and staged/unstaged/deleted
state. Restore only in a separate checkout using its README. Original22 staged network
deletions remain separate; unknown-origin source integration is preserved.

Latest known installed core remains preview24 (source a3f1d5e plus archived dirty patch),
SHA25670eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf at
C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe.
Current source was not built/installed. Selection was not changed (last known preview23).
PDB/runtime provenance remains eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Vulkan, saves, settings, monitors and the remote session were not changed by this work.
Historical326/346(native211/229) plus three targeted passes remain separate old results.
No new guest frame, image, controls, FPS or stability evidence exists; old EOP cause is unknown.

### Exact next production task — readonly image revalidation

Read F:/PS4Dev/handoff/ordered-writers-20261015/NEXT_READONLY_REVALIDATION_SOURCE_PROOF.md.
Pinned Vulkan keeps GpuModified through same-base buffer invalidation and readonly
RefreshImage; native ReleaseIndependentImageWatches loses that history and readonly
Upload never restores formatted-retile eligibility. This is confirmed with live reflected
images too. Implement sticky logical modified/dirty history separately from current
ownership in guest_texture_cache.h/.cpp Entry/Own/Upload, guest_rasterizer.cpp invalidation
and post-Upload revalidation. Preserve compatible identity, PageWatch, exact write spans,
backing, fences/pins and prewrite snapshots; never mark every sampled image modified.
Authored acceptance must distinguish same-base refresh then shifted interior writer,
never-modified, CPU-write and remap controls. Broader offsets/cross-stage/image operands,
metadata order, pressure/device-loss and full-suite/game acceptance remain open.

### NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read this authoritative top
section and CHECKPOINT_FINAL.json; verify actual HEAD/upstream and preserve the dirty
index/tree. Reuse saved Vulkan/DX12 evidence without repeated audits. Implement the
confirmed readonly-revalidation task from NEXT_READONLY_REVALIDATION_SOURCE_PROOF.md,
then continue the existing contract queue. Keep all execution deferred until specifically
authorized. Once authorized, build changed targets, run focused and required regressions,
then minimal existing-XInput game checks with completed guest frames excluding FG against
the saved baseline. Checkpoint/push source and exact continuation; do not claim full parity.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

This section is authoritative; earlier current/next statements are historical.
Source commits d4b70f9,4210e31 and4ef80fc are pushed on fix/mixed-cmask.
No build/configure/package, test/fixture execution, GPU/game, XInput, profiling or UI
operation occurred. All129 registry groups remain partial/open. Source reviews and
Git/archive checks do not establish runtime correctness or performance.

### Production continuation

- d4b70f9: ExecuteGuestResources preserves every early clean readonly buffer as an
  immutable CPU snapshot before overlapping writer invalidation. This includes raw
  readers with no later formatted descriptor. Guest requests, write ranges and ordinal
  remain unchanged; only native descriptor binding selects an appended private view.
  Snapshots use original byte sizes and normal default-resource UAV preparation;
  temporary storage and prepared resources retain their existing submission lifetime.
- 4210e31: NativeStorageBufferUavs adds Aliased20 to every native SSBO variable and
  removes direct Restrict19 plus prior NonWritable24. Existing direct/grouped Aliased,
  other qualifiers, images/UBOs and actual instructions remain. Legacy grouped
  BufferBlock is recognized. Affected grouped Restrict/NonWritable is explicitly
  unsupported rather than silently rewriting unrelated resources. Both standalone and
  linked native conversion use this helper; the shared/Vulkan emitter is untouched.
- Mesa's GLSL450 SSBO default ACCESS_RESTRICT is the confirmed source-level mismatch.
  No GPU optimizer miscompile was observed. GuestShaderCache's native cache version
  is now native-dxil-cache-20261015-ssbo-alias-v2 so future compute/graphics compilation
  cannot reuse the old disk entries. No existing cache files were removed or changed.
  Only this one line was committed from that historically dirty cache source file.
- 4ef80fc: readonly exact-base sharps can be shorter or longer than the retained image;
  every overlapping writer still covers the complete image. Each reader uses its own
  original size and GPU-modified query for the16KiB stream decision. A clean prefix can
  stream privately while a dirty/large formatted reader causes one full-image retile.
  Private captures precede retile and original-range NotifyGpuBufferWrite. Shader bounds
  and long tails remain original; full-image transfer never widens a writer's dirty span.
- An early resident formatted reader must prove full-image capacity inside its selected
  4GiB VA arena pages. This is distinct from sparse physical residency. Pinned full-writer
  residency plus SubmitPendingArenaBinds covers the earlier copy only with the same arena
  and no intervening submission. Pressure/migration/intermediate submission equivalence
  remains open; independent ownership alone does not prove earlier sparse residency.

### Authored acceptance and measured state

The existing ordered-formatted-owned helper now contains16 authored real-GCN cases:
original large/small x clean/dirty x R/W-or-W/R; four small raw/formatted R/W/R cases;
and four short/long cases. The latter cover clean64-byte prefix with a dirty tail page,
dirty64-byte prefix, clean20KiB read over a4KiB image, and mixed private64-byte/resident20KiB
reads. They assert frontend descriptor order and original extents, same-sharp rereads,
short OOB bounds, preserved long tails, post-write image reload before CPU reconciliation,
and whole64KiB allocation/padding. One original case exercises the unowned output's
synchronous fallback. All16 are authored only, not compiled/run. Their minimal integration
remains in the historical untracked gcn_attachment_alias_test.cpp; prior own integration
patch and complete integrated file remain preserved, not swept into source commits.

No new FPS/frame-time, image, controls, GPU-validation or stability evidence exists.
Historical326/346(native211/229) and three later targeted passes remain separate old
results. Last accepted Bloodborne world evidence remains fog/sky, G-buffer geometry and
finite depth; no current gameplay/FPS acceptance. Historical EOP rejection cause remains
unknown. Earlier CMASK INF/NaN and transfer fixes retain their prior source-only status.

### Preservation and recovery

Evidence root F:/PS4Dev/handoff/ordered-stream-20261015 contains production/, fixtures/,
shader-alias/, short-views/, SHORT_VIEW_SOURCE_PROOF.md and NEXT_PARTIAL_WRITER_SOURCE_PROOF.md.
CHECKPOINT_FINAL.json records final HEAD/upstream, commit list, source hashes, original
index comparison and final-residual archive hash. final-residual preserves all nonignored
historical source/assets, binary patches and deletion manifest. Do not restore it over
this dirty tree; use its README in a separate checkout of manifest.head. Original22 staged
network deletions stay separate. Historical unknown-origin edits remain unmodified.
Previous complete checkpoint/evidence: F:/PS4Dev/handoff/cmask-exceptions-20261015.

Installed latest remains preview24, ID dx12-20261015-full-port-preview-70eeeff04e89,
C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe,
SHA25670eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Its source is a3f1d5e plus archived dirty integration, not these later commits. Selected
core remains preview23. Installation/PDB/MAP/runtime provenance remains
F:/PS4Dev/handoff/eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Vulkan, saves, settings, monitoring, current processes and remote desktop are untouched.

### Exact next source task

Read NEXT_PARTIAL_WRITER_SOURCE_PROOF.md; do not repeat the buffer/CMASK/format audit.
In guest_compute.cpp::ExecuteGuestResources, adapt exact-base DWORD-sized short writers
under the existing one-stage/no-images/no-DMA/unique-version boundary. Keep original
WriteRange and dirty spans. Only a scheduled prewriter image retile may add a readonly
full-image residency envelope. Start with full readers or images inside the reader's
minimum16KiB sparse block; do not infer general sparse residency from the4GiB capacity
check. W-before-R must invalidate the whole old image even for a one-DWORD writer.
Author the focused oracle: after resident R then short W, GPU image tail may be I while
physical CPU tail remains B. Preserve earlier private snapshots and common-buffer aliases.
Broader non-base/sub-DWORD simultaneous writers, metadata ordering, image operands,
multiple image identities, cross-stage ordering and pressure/device-loss remain open.

Next exact read command (source review only):
`Get-Content -LiteralPath 'F:\PS4Dev\handoff\ordered-stream-20261015\NEXT_PARTIAL_WRITER_SOURCE_PROOF.md'`

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask.
Read the authoritative top of SESSION_HANDOFF.md and CHECKPOINT_FINAL.json, verify actual
HEAD/upstream and preserve the dirty tree/index. Reuse saved Vulkan/DX12 evidence and
NEXT_PARTIAL_WRITER_SOURCE_PROOF.md; do not repeat completed audits. Continue the exact-base
short-writer production C++ task with precise dirty ranges and conditional readonly retile
envelope, then source-review and checkpoint/push. Keep all execution deferred until new
specific user authorization. Once authorized, build affected targets, run the focused
regressions, then only minimal existing-XInput game checks and compare completed guest
frames (without FG) against the saved baseline. Do not claim full parity or current FPS
from old results. Preserve source, evidence and exact continuation before ending.


## Current CMASK and ordered-buffer source checkpoint - 2026-10-15

This top section is authoritative; older active/next-task statements below are historical.
Repository F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. CMASK source commit
e43ebdc and ordered-buffer source commit cbfe500 are pushed. Final source/checkpoint commits, actual remote SHA, original index
comparison and residual hashes are recorded in this checkpoint's CHECKPOINT_FINAL.json.
Immutable Vulkan reference remains e0a466753a1aacbc8db8253be685bf7ab4d9daae at
F:/PS4Dev/work/shadps4-original-e0a46675. Reuse the saved evidence; no general audit restart.

### CMASK production changes

- guest_color_clear.h::DecodeGuestColorClear preserves original6/7 extraction and Swizzle.
  Selected exponent31/mantissa0 stays positive infinity. Selected NaN is canonical quiet
  UF11 0x7e0 before B narrowing to quiet UF10 0x3f0; even the smallest input payload cannot
  become infinity. CPU materialization packs the same final mapped word. Discarded NaNs
  do not force raw output; initialized alpha and previous finite RTZ behavior remain.
- guest_raw_color_clear.h::GuestRawColorClear::Prepare accepts the actual Resource and
  checks its native description and retained R11G11B10_FLOAT plus R32_UINT cast list
  before cache reuse. Numeric RTV clears handle infinity. Raw integer RTV clears retain
  the explicitly chosen NaN word. Missing cast capability declines the owned fast path
  before clear commands/metadata changes and keeps the existing resolved CPU path.
- guest_rasterizer.cpp::MaterializeOwnedColorClear passes the real texture. Existing
  unique ordinary owner, CMASK rectangle/layer/sample, PageWatch, backing, pending-fence
  and retained resource/heap checks remain. No independent-owner fast-clear claim.
- Chosen all-INF word is0xf83e07c0; all canonical NaN is0xfc3f07e0. Canonical NaN is an
  explicit permitted native policy, NOT measured Vulkan-driver/PS4 payload equality.
  Format34 clear usage/scale remains separate; sampled RGB9E5 does not authorize RTV34.

### Ordered formatted-buffer production continuation

- cbfe500: guest_compute.cpp::ExecuteGuestResources retains request ordinal. A formatted
  reader before the first overlapping writer captures/retile-copies the still-valid image
  into a private Resource. Writer invalidation then runs with original sharp ranges; the
  retained result is installed into the final common UAV arena before dispatch. A writer
  before the reader schedules no old-image transfer. Repeated readers use one transfer.
- The admitted case is one compute stage, no image operands/DMA/graphics auxiliary
  buffers, one unique overlapping independent image, and exact-base/full-range original
  sharps with real UAV reflection. Existing exact transfer/ownership rules apply across
  admitted formats; no arbitrary new R32 production whitelist was introduced.
- Every early overlapping readonly descriptor, including a raw reader before W then
  formatted R, must be resident: >16KiB or actually IsGpuBufferModified. Small clean early
  CPU-stream snapshots remain explicitly rejected; small W-before-R needs no stale image.
- Preflight precedes captures/invalidation. Capture happens before final native views.
  Cache pins, pending leases and actual submission callbacks cover retained transfer
  resources, pressure-triggered Finish, and partial-recording exceptions. A pointer pin
  alone does not retain logical image eligibility. Existing normal synchronous fallback
  remains valid for unrelated small/unowned output buffers; no forced all-output ownership.
- Existing no-writer formatted full/short views and metadata precedence remain. General
  per-stage buffer/image interleaving, metadata writers, partial simultaneous aliases,
  multiple independent identities and strong allocation/device-loss rollback remain open.

### Evidence and execution boundary

Evidence root: F:/PS4Dev/handoff/cmask-exceptions-20261015. EVIDENCE_INDEX.md links the
saved primary conversion proof, final CMASK source review, before/after sources and hashes.
NEXT_SOURCE_TASK.md records the exact pinned ordered-buffer proof, not a request to redo it.
ordered-formatted/ contains the subsequent source/fixture/review evidence.

guest_cmask_test.cpp adds77 fixed codec and77 CPU materialization cases, plus ten GPU
case definitions at samples1/4 (20 variants). These cover single and mixed INF/NaN, all
swaps, smallest/largest NaN payloads, nonzero selected layers, complete padding and
idempotence. Actual resource cast-list negatives and submission-drain lifetime coverage
are authored. Device capability, RTV/PSO acceptance and device-loss handling are unproven.

66a94bd: native-port/gcn_ordered_formatted_fixture.h adds eight authored real-GCN scenarios:
large/small x GPU-modified/CPU-clean x R/W or W/R. Seven expect normal ordered behavior;
small clean R/W expects the exact guard before mutation. Seed/probe commands distinguish
B from I. The tested dispatch has one invocation, buffers only, observed loads before
store, and an asserted actual frontend descriptor order with deliberately reversed SGPR
placement. Post-write image reload precedes target CPU readback; a complete64KiB oracle
includes padding, untouched bytes and rejection state. No repeated-reader/pressure/device-
loss or nondeferred-output runtime acceptance is claimed.

The helper is a new task source. Its minimal mode/include integration is in the pre-existing
untracked native-port/gcn_attachment_alias_test.cpp; do NOT sweep that broad historical
file into this task's commit. Exact before/after, own patch and hashes are under
ordered-formatted/fixtures/, and final-residual preserves the integrated file. No new
CTest registration/configuration was performed. Authored mode: ordered-formatted-owned.

No build/configure/package, fixtures/tests, GPU/game, profiling, XInput or UI control was
performed. The user execution gate remains active until new specific authorization.
Source/manual/static review, Git/JSON and archive checks are not runtime acceptance.
All129 registry groups remain partial/open. No new FPS, frame time, image, controls or
stability result exists. Historical326/346(native211/229) plus three later targeted passes
remain old, separate results. Last accepted Bloodborne world evidence remains fog/sky,
G-buffer geometry and finite depth. EOP rejection cause is unknown; WAIT operand
stabilization does not establish an EOP fix.

### Installed build and preservation

Latest installed core remains DX12 - перенос №24 - 15.10.2026, ID
 dx12-20261015-full-port-preview-70eeeff04e89, source a3f1d5e plus its archived dirty patch.
C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe
SHA25670eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
Current source changes are not installed. Selected core remains preview23. Original saves,
Vulkan, settings, monitors and remote session are untouched. PDB/MAP/runtime provenance:
F:/PS4Dev/handoff/eop-continuation-20261015/preview24-installed/INSTALLATION.json.

Broad residual integration and22 original staged network deletions remain preserved;
old guest_depth_metadata.h deletion remains. Remote commits alone do not reconstruct
this accumulated integration. final-residual/ contains full patches, nonignored residual
source/assets and an exact-byte manifest. Recover ONLY into a separate fresh checkout of
manifest.head: staged.patch with --index, then unstaged.patch, then untracked ZIP entries.
Never restore over this workspace. .serena remains local; previous archives stay intact.

### Next exact source task

Continue the early clean-small snapshot mechanism in guest_compute.cpp at
reads_before_writer, using ordered-formatted/NEXT_SOURCE_TASK.md. Preserve original
logical sharp address/size/ordinal separately from private native binding storage. Early
readonly <=16KiB clean sharps must bind their own CPU-byte snapshot; the writer and later
reads retain the common arena. Reuse existing TemporaryMemory/retirement only after
checking compatible default-resource/UAV storage and original metadata/write semantics.
Do not bind an UPLOAD resource as a UAV or turn an early CPU stream into an image retile.
Then extend the authored GCN fixture, source-review and checkpoint normally.

Do not repeat completed CMASK finite/INF/NaN classification or packed-float/RGB32 transfer
audits. Do not treat native RGBA32 storage as guest13/14 alias compatibility or sampled
RGB9E5 as writable/RT34. Non-base image sync is asserted in pinned Vulkan. Reuse existing
measurements; no tests/game/desktop control until specifically authorized.

### NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 from the top of docs/codex-progress/SESSION_HANDOFF.md. Verify
current HEAD/upstream and preserve the dirty integration/index; restore only from the
saved checkpoint into a separate checkout if needed. Reuse the pinned Vulkan proofs and
saved measurements. Resume guest_compute.cpp early clean-small readonly binding snapshots
from F:/PS4Dev/handoff/cmask-exceptions-20261015/ordered-formatted/NEXT_SOURCE_TASK.md;
large/dirty descriptor ordering and CMASK6/7 exceptional clears are already source work.
Make production C++ changes, author focused fixtures without running them, review and
checkpoint/push. Do not repeat completed audits or launch build/tests/game/XInput/UI while
the execution gate remains active. After specific execution authorization, build, run the
minimal relevant regressions, use existing isolated XInput for necessary game checks, and
compare completed guest frames without FG to the saved baseline. Never call source review
or an older binary current runtime acceptance.



## Active packed-float and RGB32 source checkpoint - 2026-10-15

This is the authoritative continuation; older active/next-task statements below are historical.
Repository F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Source commits 06536f2 and
6ded358 are pushed to origin/fix/mixed-cmask. Final checkpoint HEAD, actual remote SHA,
index equality and residual source hashes are recorded in F:/PS4Dev/handoff/packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Immutable Vulkan reference remains e0a466753a1aacbc8db8253be685bf7ab4d9daae at
F:/PS4Dev/work/shadps4-original-e0a46675.

### Production changes, source-reviewed only

- 06536f2: GuestTextureCache::CanTransferIndependentMultisample admits native R11G11B10
  and sampled-only RGB9E5 for matching 2/4/8 samples, one mip, 2D/arrays. UINT word views
  preserve finite/special payloads rather than doing a floating-point round trip.
  Upload requires actual typed MULTISAMPLE_LOAD, render-target and sample-count support.
  GuestMultisample::Prepare/PrepareCopy/PrepareStorage validate the exact typed native
  resource and its retained typed plus R32_UINT format list. Both copy resources and all
  storage planes are checked. A declared format list cannot manufacture typed RGB9E5
  MSAA support on old hardware; unsupported-device emulation remains open.
- guest_image_ownership.h::DescribeGuestImageOwnership separates physical ownership
  from shader usage. Readonly RGB9E5 uses an ownership-only R32_UINT witness with exact
  guest bytes, logical bytes, block extent and every mip pitch/height/offset/slice/thickness
  checked. The actual Entry descriptor/layout/native views stay RGB9E5 and readonly.
  Pinned Vulkan puts RGB9E5 and R32 in the same 32-bit compatibility class; this does not
  authorize IMAGE_WRITE34. Existing expanded packed10/19 physical owners remain valid.
- 6ded358: readonly RGB32 UINT/SINT/FLOAT independent MSAA versions use guest12/host16
  storage. Existing UINT4 transfers preserve the three DWORDs; the generic tiler stores
  only 12 guest bytes and leaves neighbouring texels/padding intact. Host alpha is
  synthetic and the sampled view still supplies constant one. No original-word snapshot
  is necessary because this expansion preserves all guest components exactly.
- SetIndependentImageRetention/CanRetainIndependentImage now explicitly separate image
  ownership from simultaneous writable buffer/image admission. GuestRasterizer shares
  the real PageWatch, complete-page, backing, size and format guards between the two
  callbacks. OwnIndependentImage and RetileBufferVersion use independent retention;
  writable snapshots still use CanRetainBufferAlias. Readonly RGB32 ownership retains
  its actual 96-bit geometry; it is never replaced by R32 or RGBA32 guest geometry.
- GuestMultisample validates RGB32's matching typed layout, guest12/host16 stride and
  actual RGBA32_TYPELESS native allocation in upload/readback, both copy ends and sample
  storage. Native RGBA32 storage does not establish guest format13/14 alias compatibility:
  pinned RGB32 is _96BIT, RGBA32 is _128BIT. Actual writable/RT13 MSAA stays excluded.
  Earlier single-sample behavior is retained; no new guest write-format parity is claimed.

No HLSL source changed: these blocks reuse existing production UINT transfers and tiler.
The existing partial-progress upload exception model is unchanged; strong rollback is not
proven. Whole independent MSAA image pressure eviction remains intentionally skipped, as
in the pinned tiled GPU-modified GC policy; reconstructible sample scratch is separate.

### Evidence, fixtures and execution boundary

Evidence root: F:/PS4Dev/handoff/packed-float-msaa-20261015. Before/after copies, deltas and hashes are in reviewed-source/
and rgb32/reviewed-source/. Bounded reference proofs: reference-and-cast-lifetime.md and
rgb32-scope.md. Independent source reviews: SOURCE_REVIEW.md, FIXTURE_SOURCE_REVIEW.md,
rgb32/SOURCE_REVIEW.md and rgb32/FIXTURE_SOURCE_REVIEW.md. The review found an obsolete
--msaa-vector32 readonly13 rejection; it now rejects writable13 and preserves the existing
16-sample negative case. No unresolved source-proven defect remains in these deltas.

Authored/registered, NOT compiled or run:
- --msaa-packed-float: 12 cases, 2/4/8 samples; 6 to readonly6/writable6/readonly34 and
  R32UINT to readonly34. Exact special words, layers, physical/private padding, scratch
  retirement/recreation, incomplete cast lists, geometry rejection and readonly usage.
- --msaa-rgb32: 9 readonly13 cases, 2/4/8 samples times UINT/SINT/FLOAT. Internal UINT4
  upload seeds an independent image with poisoned host alpha; it is not guest IMAGE_WRITE13.
  Exact RGB words, synthetic destination alpha, whole physical/private allocations, fresh
  seeded repeated retile after Collect, forbidden writable/RT13 and guest16 rejection.
  This mode does not claim storage-pressure testing because readonly13 has no UAV planes.

The user execution gate remains: no build/configure/package, fixtures/tests, GPU/game,
XInput or desktop control without new applicable authorization. No such execution occurred.
No new performance/FPS/image/control result exists. Source review and Git/JSON/preservation
checks are not runtime acceptance. All129 registry groups remain partial/open.
Historical suite326/346(native211/229) plus three later targeted passes remain separate old
results. Last accepted Bloodborne world evidence remains fog/sky, G-buffer geometry and
finite depth; no accepted current world controls/FPS milestone. EOP rejection cause remains
unknown, and WAIT operand stabilization does not prove an EOP fix.

### Installed build and preservation

Latest installed core remains DX12 - перенос №24 - 15.10.2026, ID
 dx12-20261015-full-port-preview-70eeeff04e89, source a3f1d5e plus its archived dirty patch.
Path C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe;
SHA256 70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
It does NOT contain later source commits. Selected core remains preview23. Package/PDB/MAP
provenance remains in eop-continuation-20261015/preview24-installed/INSTALLATION.json.
Original saves, Vulkan, settings, monitors and remote session were not touched.

Broad accumulated dirty integration and22 original staged network deletions are preserved;
old guest_depth_metadata.h deletion remains. Remote commits alone do not reconstruct that
integration. Current final-residual contains full patches, nonignored residual source/assets
and a manifest. Recover ONLY into a separate fresh checkout of manifest.head: apply staged
patch with --index, then unstaged patch, then untracked ZIP entries. Never restore over this
tree. Local .serena state remains in the workspace; previous handoff archives remain intact.

### Next exact source task and prohibited repetition

Continue guest_color_clear.h::DecodeGuestColorClear for selected original CMASK6/7 +INF,
using rgb32/NEXT_SOURCE_TASK.md and the saved pinned clear proof. Finite RTZ is already
implemented. Separate exponent31/mantissa0 infinity from NaN after the original Swizzle;
confirm only the infinity storage rule, then use explicit float infinity and canonical
UF11/UF10 infinity words. Merely deleting the guard is wrong: existing ldexp would turn
exponent31 into a finite value. NaN payload/canonicalization stays open until established;
raw bit-preserving MSAA transfer does not prove numeric clear conversion. Format34 clear
usage/scale remains separate. Then resume the existing partial-alias/cache queue.

Next action: in this repository run git rev-parse HEAD and git status --short, compare to
CHECKPOINT_FINAL.json, then edit the named decoder and its focused authored fixtures.
Do not repeat packed16 nibble-order, typed/raw cast lifetime, RGB32 geometry/class inventory,
position-export/display-copy investigations or complete Vulkan audits without new evidence.



## Active packed-MSAA source checkpoint - 2026-10-15

This section is authoritative and supersedes older active/next-priority statements below.
Repo F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask; immutable reference
F:/PS4Dev/work/shadps4-original-e0a46675 at e0a466753a1aacbc8db8253be685bf7ab4d9daae.
New source commits: 1f43d43 (finite CMASK6/7 RTZ), 59fdc53 (packed MSAA versions/clears),
fe51ce4 (sampled native16-18 and expanded19). All are WIP source changes;
final checkpoint HEAD/actual remote SHA and residual hashes are recorded externally in
F:/PS4Dev/handoff/packed-msaa-20261015/CHECKPOINT_FINAL.json.

### Execution boundary and installed core

The user still prohibits tests, fixture execution, GPU/game launches, XInput and desktop
control while using the PC. No new build/configure/package was authorized or performed.
Source review and Git checks are NOT runtime acceptance. No new FPS, frame timing, image
or control result exists. The one-time preview24 build authorization is already consumed.
Latest installed core stays DX12 - перенос №24 - 15.10.2026, ID
dx12-20261015-full-port-preview-70eeeff04e89, at
C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89/shadPS4-DX12-Core.exe.
EXE SHA256 70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf.
It contains a3f1d5e plus the saved package dirty patch, NOT these later source changes.
Selected core remains preview23; settings, original saves, installed Vulkan, monitors and
remote desktop session were not touched. Package/PDB/MAP provenance remains in
F:/PS4Dev/handoff/eop-continuation-20261015/preview24-installed/INSTALLATION.json.

### Implemented production C++ in this continuation

- guest_color_clear.h::DecodeGuestColorClear: all finite original CMASK6/7 values now
  use deterministic toward-zero UF11-to-UF10 conversion after the original Swizzle.
  CPU packing and RTV use the same prequantized B value; finite max65024 becomes64512.
  This is specification-permitted native rounding, NOT a proven saved-Vulkan-driver
  rounding match. Exceptional INF/NaN and original34 still need separate contracts.
- guest_texture_cache.cpp::CanTransferIndependentMultisample: matching2/4/8 samples,
  one-mip2D/arrays admit exact raw/native packed10, writable packed16 guest2/host4 R32,
  native sampled16/17/18 UNORM guest/host2, readonly expanded signed10 and packed19.
  Native16/17/18 use existing PACKED_MODE1/2 sample code recovery; original18 rotation
  stays in the tiler. Native19 attachments and mutable expanded views stay outside this
  contract. Actual capability/sample support checks remain in native resource paths.
- GuestTextureCache::Upload: readonly expanded10/19 MSAA snapshots retain original
  guest words and exact written-byte masks. Existing expanded linear scratch first
  performs raw detile/retile, then normal expansion. Original19 capture uses guest2/
  host4 raw words without swapping nibbles. Buffers have real allocation accounting;
  leases and barriers precede uses; RetileBufferVersion merges logical bytes over a
  separately seeded physical version. Duplicate SNORM -1 encodings and padding are
  never reconstructed from expanded floats. CPU and GPU upload paths are covered in
  source; a separate CPU-upload runtime case is still needed when execution is allowed.
- guest_multisample.cpp validates raw packed native R32 backing and exact guest/host
  layout before views. No bpp-only admission, hardware resolve or physical-buffer write
  substitutes for an independent image version.
- GuestConvertedMultisampleClearCompatible/ClearRawStorageImage: nonzero raw packed10,
  packed16 and finite rawRGBA8 SRGB MSAA clears now use existing typed single-sample
  clear/packing followed by per-layer/all-sample exact R32 broadcast. Synthetic packed
  clear layout is single-sample before broadcast, including layer slice sizes. Packed10
  integer bounds are RGB10/A2; normalized inputs must be finite. Existing budgets,
  descriptors, copy leases, format-transfer retirement and authoritative image UID stay.

No new HLSL file was edited in this continuation: these paths reuse the production
packed tiler/MSAA shaders. No algorithm is claimed verified merely because it is authored.

### Evidence, verification and known limits

Evidence root F:/PS4Dev/handoff/packed-msaa-20261015 contains before/after copies,
isolated deltas, hashes, source proofs and independent review notes. Important subsets:
cmask-rounding/, srgb-msaa-clear/, packed-msaa-clear/, packed16-sampled/,
source-review-expanded.md, source-review-fixture.md and stage1/stage2 snapshots.
Static source reviews and git diff --check found no remaining concrete blocker in the
committed deltas. Upload retains its existing partial-progress exception model; strong
rollback/fault injection has NOT been established.

Authored/registered only, NOT compiled or run: --msaa-packed10(6),
--msaa-packed10-expanded(6), --msaa-packed16-raw(12),
--msaa-packed16-sampled(12), --msaa-packed-clear(16); rawRGBA8 clear expanded4->6
with SRGB2/4. Cases include all samples/layers, bounded integer rejection, actual host
representations, private guest bytes/padding, unchanged physical buffers, scratch
pressure/fence retirement, and repeated retile after Collect with fresh padding.
CMASK RTZ fixed vectors include subnormal/normal/exponent/overflow and component swaps.

Historical runtime results remain unchanged: full suite326/346(native211/229) at the
older boundary plus three later targeted passes, not a new aggregate. Bloodborne menus
recovered historically; last accepted world evidence is fog/sky with G-buffer geometry
and finite depth. No accepted current world FPS/control milestone. EOP backing-write
rejection cause remains unknown; WAIT packet operand stabilization is not an EOP fix.
All129 existing registry groups remain partial/open; no overall parity/percentage claim.

### Next concrete source work

Continue existing packed-float independent MSAA contracts in guest_texture_cache.cpp
CanTransferIndependentMultisample/Upload and guest_multisample.cpp Prepare/PrepareCopy:
format6/34 exact R32 transfer requires the actual retained castable native format set.
Use existing PackedFloatCasts and Resource::AllowsCast guards; do not admit by four-byte
size alone or approximate packed floats via a numeric resolve. Then continue the existing
partial-alias/cache contract queue using saved reference and measurements. Whole native
MSAA independent-image pressure eviction is a separate extension: current ReserveMemory
intentionally skips such victims, matching the pinned tiled-GPU-modified GC exclusion.
Do not call that skip a missing Vulkan feature or resolve into the physical buffer to
free memory. Existing reconstructible sample-storage retirement is already implemented.

Next action: verify actual HEAD/status against CHECKPOINT_FINAL.json, then inspect only
those named functions and native-independent-r11-packed-float-image-versions in
 tools/porting/contracts.json. No new broad audit or repeated Vulkan baseline run.
Do not rerun resolved position-export/display-copy investigations, prior format19
nibble-order audit, or completed exact8/16/32 transfer inventory without new evidence.

### Preservation

The broad accumulated integration remains dirty intentionally. Original22 staged network
deletions are preserved separately from new commits; old unstaged guest_depth_metadata.h
deletion remains. No reset/clean/force-push, original-save write or deletion was performed.
The remote commits alone do not reconstruct that historical integration: use this source
workspace or final-residual/manifest.json + source-files.zip + staged/unstaged patches.
Recovery is ONLY into a separate fresh checkout of manifest.head: apply staged.patch with
--index, then unstaged.patch; restore only untracked ZIP entries. Never apply over this tree.
Previous archives complete-port-20261015, eop-continuation-20261015 and
black-continuation-20261014 remain intact. Exact final staged equality and ZIP hashes are
recorded in CHECKPOINT_FINAL.json; .serena local state remains in the original workspace.


## Active source continuation - 2026-10-15, complete-port checkpoint

This section supersedes the active-priority statements below; older sections remain evidence.
Source commits: `35b5de4`, `d7d9064`, `440dd53`, `ad6808e` and `e359447`,
pushed to `origin/fix/mixed-cmask`. The repository remains
`F:/PS4Dev/work/shadps4-dx12-src`. Reference is read-only
`F:/PS4Dev/work/shadps4-original-e0a46675` at `e0a466753a1aacbc8db8253be685bf7ab4d9daae`.

**Execution gate:** the user is using the PC and prohibited tests, game/GPU execution,
XInput and desktop automation. This continuation has performed source edits/review and
Git checks only. No new configuration, build, package, runtime fixture or game launch.
The one-time preview24 build authorization was already used. Preview24 remains installed
and unchanged; NONE of the post-preview24 source commits below are in that executable.
Do not ask to run tests while independent source work remains. Do not claim runtime parity.

### Completed source changes, pending compilation and acceptance

- `35b5de4`, `Liverpool::ProcessGraphics/ProcessCompute`: snapshot WAIT_REG_MEM packets
  before coroutine yields/WaitVoLabel. Packet operands are stable; predicate reads still
  read live guest memory/registers. REWIND remains dynamic. Pinned EOS/GDS is one u32 at
  a byte offset after Finish; no invented multiword/IRQ extension. EOP cause is not fixed.
- `d7d9064`, `guest_color_clear.h::DecodeGuestColorClear`: shared original CB clear
  decoding/component mapping for CPU materialization and ordinary GPU-owned CMASK.
  Admitted R/RG/RGBA8/16/32 numeric formats, packed10 UNORM/UINT, packed16 formats16-19
  UNORM. Includes narrow integer rules, SNORM canonical minimum, sRGB conversion,
  absent-alpha mapping and exact raw float/integer lanes. Undefined packed negative
  narrow SINT and unproved numeric formats retain existing fallback.
- `guest_raw_color_clear.h::GuestRawColorClear`: UINT RTV draw writes raw 16/32-bit
  component storage when float ClearRenderTargetView cannot preserve integer precision,
  denormals or special float bits. Cached PSO/root/descriptor/resource leases are retained
  through submission; native support is queried by the runtime path, not assumed.
- `GuestRasterizer::MaterializeOwnedColorClear` and `MaterializeGuestColorClear`: use the
  same codec and validated uniform/mixed CMASK tile plan. Logical rectangles, selected
  layers/all samples, padding, unique ordinary owner, PageWatch and fences remain guarded.
  Independent versions, competing owners, remapped sample backing, TC/compression/DCC
  are still outside this owned path. Raw shader/typed rounding have not executed.
- `GuestTextureCache::CanTransferIndependentMultisample`: matching 2/4/8-sample one-mip
  2D/array independent image transfer now admits R/RG/RGBA8/16/32 exact UINT storage
  families; UNORM/SNORM/UINT/SINT, 16/32 FLOAT, RGBA8 SRGB as applicable. All samples and
  private physical-seeded padding are retained, with no physical buffer reconciliation.
- `RetireMultisampleStorage/EnsureMultisampleStorage`: reclaim only completed unpinned
  uniquely leased reconstructible sample-UAV scratch, then allocate a whole replacement
  before publishing it. Authoritative MSAA image, UID and CPU-write ownership stay live.
  Normal compute and HLE paths restore scratch before use. Whole native MSAA eviction
  is still open; do not describe scratch reclamation as full pressure parity.
- `BuildGuestGraphicsState`: guest color masks now use the pinned `Swizzle().ApplyMask`
  scatter, independently of shader export gather. Absent guest CB format keeps explicit
  native-only masks. Standalone target wiring includes the shared pixel-format table.
  Format19 already repacks physical nibbles in existing texture transfer: no extra swizzle.

### Prepared verification and evidence

No test in this continuation was compiled or run. New fixture coverage is authored only:
CMASK format/swizzle/physical padding/layers/sample cases and raw UINT RTV readback;
independent `--msaa-pressure`, `--msaa-vector32`, `--msaa-vector16`, `--msaa-vector8`;
1152 color-mask combinations and three state boundaries. Main core build, fixture
builds, GPU validation and Bloodborne correctness/FPS remain pending authorization.

Source review and `git diff --check` succeeded for the committed delta. These are not
runtime tests. Historical full suite326/346 (native211/229) and three subsequent targeted
passes remain separate old results. Historical world still shows fog/sky while G-buffer
has geometry; no accepted new world controls/FPS/speedup. No new measurements to compare.

Evidence root: `F:/PS4Dev/handoff/complete-port-20261015`:
`pm4`, `cmask-before`, `cmask-reference.md`, `cmask-fixtures`, `cmask-packed16`,
`msaa-pressure`, `msaa-formats`, `msaa-formats16`, `msaa-formats8`, `color-write-mask`,
`format19-continuation.md`, source-batch committed patch and original index snapshot.
These hold before/after sources, deltas, hashes and bounded reference proofs, not test PASS.
The final checkpoint manifest will record final HEAD/upstream and residual archive hashes.

The broad pre-existing integration is still dirty and must be preserved, including exactly
22 pre-existing staged network deletions and one old unstaged deletion. New commits do
not silently absorb those changes. A fresh residual source archive preserves current
tracked patches, untracked source and the original index split. `.serena` is local state,
not runtime source. Do not restore an archive over the current dirty tree.

### Current continuation and unresolved boundaries

The bounded follow-ups are now also committed and pushed:

- `440dd53`: original CMASK6/7 finite values exactly representable after component
  mapping in UF11/UF11/UF10. Packed subnormal widening/even-mantissa narrowing is exact.
  Odd-mantissa rounding, overflow and INF/NaN still use historical fallback, not claimed
  as parity. Format34's pinned scale2^(E-10) and ImageRead-only use are an explicit blocker.
- `ad6808e`: raw RGBA8 MSAA transfers now select R32_UINT for their already packed DWORD
  backing, with actual R32 resource/layout validation in Prepare/PrepareCopy. Ordinary
  RGBA8 retains its four-component view. Six new `--msaa-rawrgba8` cases cover raw/ordinary
  versions, layers/samples/padding and pressure restoration; registered but unrun.

Evidence: `raw-rgba8-msaa/SOURCE_IMPLEMENTATION.md` and
`cmask-packed-float/source-contract.md`, with before/after sources, deltas and hashes.
The earlier d7d9064 raw-RGBA8 exclusion is superseded by ad6808e.

- `e359447`: nonzero raw RGBA8 MSAA HLE clear reuses typed single-sample RGBA8
  clear, existing packed conversion to raw R32 and single-to-MSAA copy across every
  sample/layer. UINT0..255/SINT-128..127 union-bit inputs and finite UNORM/SNORM are
  admitted. Raw sRGB/nonfinite/out-of-range values retain shader fallback. The image
  resource/UID/physical buffer stay intact. Intermediate budget and all descriptor,
  resource and copy leases are retained before recording and retired on the existing
  fence. `ConvertPackedBacking` resets obsolete `storage_retired` on replacement.
  Four `--msaa-rawrgba8-clear` cases cover numeric rejection, lifetime/accounting,
  scratch refresh/pressure, format replacement, padding and physical-domain identity.
  They are authored/registered only. Independent source review found no proven defect;
  this is not compilation, GPU acceptance or performance evidence.

The e359447 checkpoint also preserves the **related pre-existing** generic clear helpers
in `guest_image_operations.h` and the previously untracked `guest_packed_transfer.cpp`.
Do not attribute that entire file history to this new clear delta. Exact before files,
original worktree patches and isolated new `own-*.patch` are in `raw-rgba8-msaa-clear`;
`SOURCE_IMPLEMENTATION.md` and `SOURCE_REVIEW.md` distinguish the change and limitations.

Next concrete source task: domain-preserving independent packed10/packed16 MSAA
reinterpretation. Start at `GuestTextureCache::CanTransferIndependentMultisample`,
`RetileBufferVersion`, `GuestMultisample::TransferFormat/Prepare` and the now preserved
`GuestPackedTransfer`. Reuse existing exact packed transfer/tiler handling, admitting
only format/sample combinations with a demonstrated complete read/write contract.
Do not generalize the current raw-component predicate by byte width alone. Preserve
expanded original guest words/written masks, all samples, metadata and private padding.
Raw sRGB HLE clear, UF11-to-UF10 rounding/specials, shared-exponent clear, depth/stencil,
RGB32 expansion,16 samples, final scene-color/ROP and EOP runtime attribution remain open.
Whole independent native MSAA eviction remains a separate native pressure extension;
the pinned GC itself skips tiled GPU-modified images, so do not invent a missing Vulkan
capability from that skip or call scratch-only reclamation full native eviction.

Exact restart action (read-only, no re-audit):

```powershell
Set-Location 'F:\PS4Dev\work\shadps4-dx12-src'
git rev-parse HEAD
git status --short
Get-Content 'F:\PS4Dev\handoff\complete-port-20261015\raw-rgba8-msaa-clear\SOURCE_IMPLEMENTATION.md'
```

`tools/porting/contracts.json` remains the existing129-group registry; none of these
source changes closes a complete group or constitutes game acceptance.

Do not repeat general parity inventory, format19 host swizzle investigation, resolved
position-export/display-copy attribution or old identical game runs without new evidence.
After source work, preserve the delta, local checkpoint and upstream push without force.
When final execution is explicitly allowed, build affected targets first, run minimal
relevant fixtures and then the existing copied-profile/virtual-XInput Bloodborne path.
Measure completed guest frames without FG, retaining old baselines and original saves.


## Latest installed package — 2026-10-15, preview24

This update supersedes the earlier source-only build/install status below. The user
subsequently requested the latest test core installed in PS4Desktop and confirmation
of GitHub push. Build/install is authorized; tests, game and desktop automation remain
deferred. Release core compiled/linked successfully in 39.367s with four build jobs.
No core, regression fixture or game was launched after this build.

- Installed name: **DX12 — перенос №24 — 15.10.2026**, status Experimental.
- Package: C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261015-full-port-preview-70eeeff04e89
- Core SHA256: 70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf
- Source: a3f1d5eb5e73b4a16eb70cc6062b1b25c85b2ea7 plus exact full dirty-source patch
  in package provenance. This includes the PM4/backing/culling/MSAA/CMASK changes below.
- Static verification: all76 manifest files, PE dependency closure, source identity;
  installed desktop metadata-only --list-cores discovers this version with Error=null.
- Separate profile and20 save files copied from selected preview23, original hashes
  unchanged. Selected core/settings, old packages, installed desktop/Vulkan preserved.
- In the app open «Версии ядра». If the dialog was already open, close and reopen that
  dialog to refresh its list. The new package is installed, not selected automatically.
- Build/provenance/install/discovery: F:/PS4Dev/handoff/eop-continuation-20261015/preview24-installed/INSTALLATION.json.
  Historical CHECKPOINT_FINAL.json and final-residual remain the earlier source checkpoint;
  this follow-up changes documentation only. Installation.json records current follow-up HEAD.

Compilation is now confirmed for production core sources; new standalone fixtures were
not built/run. No new runtime, frame-rate or whole-port acceptance. Continue the same
typed CMASK/source contract below while respecting the execution gate.

## Active continuation — 2026-10-15: source work, execution deferred

This section supersedes older active priorities and test instructions below.
The user explicitly stopped ALL test/game execution and desktop automation on 2026-10-15
while using the PC. Continue source integration; defer runtime verification until the end
of the full transfer unless the user changes this instruction. No new compilation has
been performed after that instruction either. Do not launch tests to fill this report.

Full Vulkan/D3D12 parity and correct Bloodborne world remain incomplete. The earlier
326/346 full-suite result and later targeted PASS remain historical, unchanged.
New changes below have source review only, no compile/runtime acceptance.

Production changes:
- Core::PlanBackingWrite and MemoryManager::TryWriteBacking validate the complete virtual
  and physical extent before any copy. Gaps/unsupported VMAs/overflow/end-of-map reject
  without a written prefix; source offsets and physical alias traversal are preserved.
  Existing mapping mutex stays held through validation and stores. This closes a
  source-proven partial-success bug shared with pinned Vulkan; it does NOT establish
  the cause of the historical EOP false return. A focused fixture is prepared, unrun.
- Liverpool decodes EOP/EOS/RELEASE_MEM values and WRITE_DATA payloads before OnFence;
  graphics/CE packet advancement retains the decoded length across writes/yields/IRQs.
  RELEASE_MEM scalar stores retain ordinary guest-memory/PageWatch access; its GDS copy
  now finishes before write-confirm IRQ. All prior synchronization/assertions remain.
- Guest graphics state preserves RectList's pinned Vulkan cull exemption and classifies
  GS line/point output and tessellation isolines before emulating cull-both. Shader stages
  still execute when rasterization is discarded. Prepared state checks are unrun.
- Three historical fixture failures have source-level corrections only: CB sRGB uses
  its 3-bit encoding6 (not T# encoding9); cull-both expects rasterizer discard with shader
  side effects; the odd-size DMA case requests a DWORD-aligned view envelope while
  preserving its original odd copy and exact padding checks. No new PASS is claimed.

- GuestTextureCache now transfers independent R32 UINT/SINT/FLOAT MSAA domains
  with matching2/4/8 samples. GuestMultisample extracts every sample/layer, then the
  existing GuestTiler retiles into a private physical-seeded GPU buffer. It does not
  resolve/average samples or overwrite the physical buffer domain. Source leases,
  descriptors and allocations remain fence-retained; scratch UAV allocation is counted.
  Depth/stencil, other MSAA formats,16 samples and mismatched sample/backing counts
  remain explicitly rejected. The prepared --msaa fixture has18 cases and is unrun.

Before the stop instruction, diagnostic core3c26133c61fdaaf79a3c354c87e1ea29f37075fafdfb7e45fd2289781b5ca683
was compiled (raw log command-20261015-170256-69832.log) and ran155.76s, exit0 by requested
normal close. Offline title menu was visually captured; no gameplay input was sent.
EOP rejection did not recur before stop, so its failed address/mapping lifetime remains
unknown. Controller neutral/stopped/device_removed=true; all20 original seed save hashes
unchanged. See F:/PS4Dev/handoff/eop-continuation-20261015/diagnostic-game/RESULT.json.
That EXE predates the new production changes. Installed preview23/selected core remain
unchanged. No current-build/runtime-success or valid world FPS claim.

Preservation: F:/PS4Dev/handoff/eop-continuation-20261015 contains before patches/index,
exact touched-file baselines, new deltas and checkpoint metadata. Source baselines are
separate WIP commits from this session's changes. Original22 staged network deletions
remain separate and unchanged. .serena/ is local navigation metadata, not source parity.
Do not restore an archive over the current dirty tree, reset/clean, or force-push.

Next source work follows the CMASK typed-clear contract below and existing registry;
do not reopen the closed position-export/empty-depth hypotheses. EOP crash and real
scene-color/ROP attribution stay open pending eventual authorized runtime evidence.


Source commits:756359c0a410 (preserved pre-existing touched-file integration),
9960b080783a (complete backing writes/packet snapshots/culling),
f1e68d4 (independent R32 MSAA transfer). All are WIP; runtime verification deferred.
The checkpoint JSON records final documentation HEAD and remote identity.

CMASK production continuation (source only):
- The CPU materializer and owned GPU path share the existing validated clear plan.
  Uniform selected layers and mixed 0/F tiles are cleared on the existing unique
  ordinary GPU-owned RGBA8 UNORM attachment. Per-layer rectangles clip to logical
  texels; every native sample is written. Already-expanded/unselected regions remain.
  Uniform metadata slices and individual mixed-tile nibbles are consumed exactly as
  in the CPU path, after commands are recorded. No color-plane CPU readback occurs
  on this path; later guest readback still overlays physical-seeded bytes, preserving
  padding. Resource/allocation/descriptor/cache-pin leases survive the submission.
- Other numeric/component mappings, independent buffer/image versions, competing
  image owners, mismatched sample backing and TC/compressed/DCC metadata retain the
  existing fallback/rejection. No FMASK/compression semantics were guessed.
- ComputeImageOperation copy/clear and the CMASK path publish texture fences with
  NotifyOnSubmission. Retirement callbacks only retain/release/collect; they cannot
  stamp newer pending image work with an older completed fence. Recording opens
  before pending publication. This is source-reviewed, not runtime-accepted.
- The P16_32x32_8x16 (x32,y0) fixture golden nibble was corrected from 4 to 0;
  byte1024 is unchanged. Both AMD AddrLib and production equations establish this
  source result. Other expected pixels/samples/padding and production addressing
  remain unchanged. The old generic assertion does not prove its executed location.

CMASK source commit: 479064d93bd2064a6cf642e3fdfe2e92e7ff2eae. Its separate baseline commit preserves the existing
address fixtures; see cmask-source-commits.json. No source delta from this continuation
has been compiled or run after the user's stop instruction. Current installed core
is still preview23; do not install or label these source commits as a tested core.

Deferred acceptance (only at the final testing stage): exercise MaterializeOwnedColorClear
through the real GuestRasterizer after a GPU color writer, with uniform/mixed tiles,
clipped edges, multiple/untouched layers, per-sample data, metadata padding, competing
aliases and independent domains. Check physical color bytes stay unchanged until guest
readback and compare all logical/padding bytes afterwards. Existing standalone CMASK
materializer tests alone do not establish this new owned-GPU branch. Exercise older-fence
retirement while a newer image list is pending. Current source review found no MSAA
transfer delta defect; that independent review is not execution evidence.

Next source task: extend MaterializeOwnedColorClear beyond the exact RGBA8 UNORM
standard component mapping using raw CB_CLEAR_WORD0/1 packing and existing native
typed/packed clear helpers. Start with exact-format eligibility in guest_rasterizer.cpp
and guest_image_operations.h, keeping the shared plan, partial regions, sample count,
independent-domain exclusions, padding and submission lifetime rules. Integer/float/
sRGB/component-swap conversions must match pinned ColorBufferClearValue and physical
encoding; retain fallback whenever equivalence is not established. Independent MSAA
pressure/retirement and remaining contracts.json boundaries follow; no new broad audit.
Historical EOP failed address and scene-color/ROP cause still need eventual runtime
evidence, which remains deferred by the user. Do not reopen closed position/depth claims.

Preservation/proof: cmask-next-source.md, cmask-fixture-before/PROOF.md,
image-fence-source.md and the mixed-CMASK delta under this session's archive.
final-residual/ holds current remaining source files, working/staged patches, original
stage split and checksums. CHECKPOINT_FINAL.json records exact HEAD/upstream and archive
identity. The pushed WIP commits alone do not reconstruct the broad pre-existing dirty
integration; retain this tree or restore the residual archive only into a separate
clean checkout of its recorded HEAD. Never overlay it onto this working tree.

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

## A. Active project/source state

Full Vulkan-to-native-D3D12 port is incomplete. Baseline was b8d614b1d398d47a3d6045172b846f9965c502cf;
branch fix/mixed-cmask. Logical source commits: 5b6f47e0a999f9574024c8099575299f6bbe9c8e, e267eb8d05f3ba9bcab43596f94cdecb6bb055ce,
24bf8c098e5baac51262dc0afbcd2913f0a7cd9d, 1fb65f7b10fc7a4d1d9881b75684d70ae8bf811e.
AvPlayer queue/source fix: 952234f517d5fd638fb3e08cc579873ac64b75b3.
Actual final checkpoint HEAD/upstream/status is in F:/PS4Dev/handoff/black-continuation-20261014/CHECKPOINT_FINAL.json.
Many earlier full-port files remain dirty/untracked. The pushed commits alone do not reproduce this full dirty build.
Keep the tree; exact tracked/untracked contents, original index/stage split are saved in final-residual.
Recover only in a separate clean checkout of its recorded HEAD; never apply the archive over this dirty tree.

## B. Verified production corrections

- Mesa native patch, dxil_nir.c::update_writes/dxil_nir_ensure_position_writes: partial Y/Z viewport stores
  had widened to XYZW and zeroed X/W. A function-local vec4 preserves previously written position components;
  GS EmitVertex/EmitVertexWithCounter resets to undef afterwards. Native patch SHA256
  180e873aacadea12976ae3a121e34e7a9194fabaa10b3d5789cb8aaa586ee129.
  Production auxiliary RectList failed with installed old DLL (IA3/DS4, clip_out0/PS0, zero RGBA),
  now emits exact64 red pixels in both viewport orientations (clip_out2/PS64).
- page_watch.h::TrackReadOnly/UntrackReadOnly: independent image write guards accept stronger buffer NOACCESS
  without weakening it. Readable witnesses keep strict default behavior. GuestRasterizer::WatchBufferImageVersions
  uses the write-guard mode; DiscardBufferImageVersions releases through ReleaseIndependentImageWatches,
  retaining buffer guards and sibling image watches.
- GuestTextureCache::SetIndependentVersionTransfer/Upload and GuestRasterizer::SetBackingAccess:
  incompatible single-sample image views obtain a GPU-only retiled snapshot of the independent image domain,
  seeded from physical backing to preserve padding, then acquire replacement ownership before retiring source.
  No CPU download/physical-buffer commit. Real tiled R32float-quarter -> RGBA8 interpretation test preserves
  distinct physical buffer-one and all padding. MSAA transfer remains explicitly unsupported.
- GuestTextureCache::Upload admits single-sample depth mip chains; Entry::Dsv selects requested mip/layers.
  Twelve view-range cases now use production Dsv; content/padding/disjoint-state checks pass.
- Fixture corrections: stencil G component mapped to scalar R for uint HLSL SRV; GPU-detile upload leases
  accepted by coherent sampled fixture. All old pixel/plane/padding expected values retained.
- AvPlayerQueue::Size/Pop/PopIf and AvPlayerSource::GetVideoData/GetAudioData: check/peek/consume under
  queue lock, future timestamps retained, selected frame checked before retiring previous buffer;
  source state mutex serializes issuing current-frame pointers against Stop and competing consumers.
  Last native AV was symbolized with matching map/PDB to AvPlayerQueue<Frame>::Pop at RIP7000001ccaff;
  same concurrent consumers/Clear test crashed old queue3221225477, fixed queue PASS with FIFO/deadline checks.

## C. Current unfinished work and exact evidence

Initial snapshots60/61/62 in diagnostic-game prove selected native RT was already zero; immutable snapshot
and presenter source were byte-exact. RectList/DXIL clipping cause above is closed for that case.
fixed-game restored observed offline/Continue menus, then hit PageWatch guard exception; next build advanced
and hit independent-domain resolver exception; subsequent GPU transfer fixed both observed loader blockers.
domain-fixed-game ran602.36s exit0; gbuffer-capture-game362.43s exit0, G-buffer UID83/PSa291c498 shows player
and Hunter's Dream environment in encoded material/normal RGB. Final picture still sky/fog without normal player.
Linked world snapshots4192..4194 source/snapshot/presenter are exact; no late display-copy loss proved.

Old gbuffer-capture candidate address labels were wrong for NONselected images (all showed display address).
UID/resource pointer/format/writer hashes/pixels remain valid. Do not reconstruct bindings from those addresses.
24bf8c0 corrected actual address/allocation/swizzle labels; original raw capture was preserved with warning.

lighting-input-game362.61s exit0 identifies PS8c08444c input640x400 at24eff8000, producer PSa61e325d.
Its color input readback was omitted by an erroneous diagnostic interpretation of ShaderPlanes mask3;
that diagnostic guard was corrected using native plane count. Do not claim its empty BIN means black pixels.
producer-graph-game361.52s exit0 captures actual resource reads and same-draw outputs for four passes:
898dabb3 ->57f3b8c2 ->a61e325d ->8c08444c. Depth UID78/address2667e0000 has1,024,000 finite values,
min0.9801632762/max1.0. Output334 of898dabb3 is almost zero (4 nonzero half values), consistent with a
bloom/auxiliary target but NOT independently proven erroneous. Blur8c has one-color-pixel input while its
output49 contains complex sky/fog: shader inputs alone omit destination/ROP history.

ROP capture adds target-before reads and actual blend/logic/write-mask state; roots at final blit38d65b32,
discovers last recorded graphics producers from actual bindings/destinations (max8 passes,16 inputs/pass,
256MiB GPU staging quota). Ordinary draws do not enable capture or Finish. Retains staging COM leases in
bindings through GPU completion. Latest run F:\PS4Dev\handoff\black-continuation-20261014\avplayer-fixed-game, 152.87s, exit-2147483645.
ROP extension has NOT been captured in-game: rop-graph-game failed143.12s in AvPlayerQueue::Pop BEFORE NEXT,
then avplayer-fixed-game failed152.87s in EventWriteEop::SignalFence/TryWriteBacking assertion liverpool.cpp705.
No AV exception in the latter; do not claim whole game stability or a valid ROP graph. Its empty ROP summary
means no capture, not empty GPU contents. Preserve both runs and failures. Basic producer graph remains:
57f3b8c2, 898dabb3, 8c08444c, a61e325d; see PRODUCER_GRAPH_SUMMARY.json and per-pass requested-views/SPV/DXIL.
Each before/after pair shares ordered GPU commands, but graph discovery spans later frames. Same shader hash
can be reused for different passes, so this is a bounded partial graph, not one complete flip DAG.
Recorded producer labels cover graphics only; compute/copy/clear writers remain a further attribution limit.

## D. Proven problems versus hypotheses

Proven: AvPlayer non-atomic empty check/front consumption race and old-queue AV/new PASS; latest EOP backing-write
failure (address/size not recorded yet). Previous position component loss; mixed page-guard rejection; missing incompatible independent
single-sample resolver; depth mip/DSV rejection; current incomplete final image;17 baseline failures remain
unresolved after the three later targeted passes. No gameplay/control/performance acceptance.
Hypotheses: scene HDR color lost before/composited over by later lighting/sky pass; wrong resource version,
binding, buffer constants, MRT export/ROP or alpha/depth semantics. Empty depth/all-NaN explanation is not
supported by this captured finite, nonempty depth. PS8c/898 roles are not declared root causes.

## E. Measurements and games (not gameplay FPS)

All run-specific launch/exit/profile/source-patch/executable hashes/XInput session IDs and original20 save
hash checks are in RUNS.json and each RESULT.json. Dates/scenario: Bloodborne CUSA03173 v1.09, offline,
Continue from cloned seed; FG false, partial_render0. Paired completion counts are mixed startup/menu/load/
incomplete-scene throughput; readbacks/debug layer/probes alter pacing. No FPS/speedup comparison is valid.
Original Vulkan baseline/profile/performance history below is preserved; do not compare black/menu completions
against historical1.377539 FPS. Historical0.0064ms GPU inference remains withdrawn.

## F. Latest checks and diagnostic failures

Full configured suite326/346 (native211/229),20 failures,820.287s: command-20261014-043903-40692.log.
Full suite ran BEFORE final depth/fixture corrections. Later targeted coherent attachment/DS shader planes/
DS view ranges all PASS (045834-75048,045839-47208,045840-113088). Do not publish329/346 as a rerun.
Latest RectList PASS045440-82392; PageWatch ownership PASS045441-24232; independent transfer PASS045441-87992.
Native Mesa source-contract check passed after exact byte/hash patch normalization. Full suite failure list and
later results: suite-summary.json; old-timeout signed-packed60s is retained, not rerun to pad reporting.
AV queue standalone PASS0.081s (command-20261014-061035-65320); old queue AV2.351s (061035-18828).
New test makes current selection347 tests; full346-suite predates it. No second full suite was run.
Unresolved baseline: copy dirty ranges, resources/residency, two10-bit display cases, old hardware texture fixtures,
signed packed timeout, CMASK, two GPU page-ownership cases, buffer/image alias+partial, owned dispatch/sample,
BC volume, SRGB MIN/MAX and dual-face culling.
Noncausal clip/cull and local/global shadow experiments archived; global shadow caused compiler AV.
Independent transfer test's initial bad UAV CPU-handle/early-release fixture caused a GPU hang/TDR;
fixture was corrected with CPU descriptor heap and completion-before-assert, final Debug Layer test PASS.
No reboot, session/driver/service reset. Preserve raw failed logs; do not repeat those experiments.
No second full suite was run after diagnostics-only changes.

## G. Preserved binaries/profiles/tools

- Current installed preview23: C:\Users\skei\AppData\Local\PS4Desktop\cores\dx12-20261014-full-port-preview-bde5503581e2/shadPS4-DX12-Core.exe; matching symbols/shadPS4-DX12-Core.pdb, native-core.map,
  native-compiler, provenance/compiler-profile.json/source.patch and core-version.json in package.
  SHA256 bde5503581e2178aa854703d9ec6823287d6b9ea243a61b41bc8e2f767ace760. Existing PS4Desktop -> Versions -> DX12 — перенос №23 —14.10.2026.
- Preview22 C:/Users/skei/Documents/Codex/2026-10-12/skiters-ps4-dx12-fix-mixed-cmask/outputs/DX12-preview-22-20261014 remains available (functional fixes, before graph).
- Experimental game exe and matching symbols inside each run/core; last gamecc1ff355.... Package refreshes
  committed revision afterwards; compiler/renderer functional state unchanged from latest game.
- Build tree F:/PS4Dev/work/shadps4-dx12-src/build-migration-core; tests build-migration-checks. Pinned clang-cl22.1.3,
  MSVC ABI19.51, VS18/SDK26100. Reuse incremental builds; do not change compiler for clangd.
- Reference F:/PS4Dev/work/shadps4-original-e0a46675, SHAe0a466753a1aacbc8db8253be685bf7ab4d9daae;
  installed Vulkan/DLSS/FG F:/Apps/shadPS4-DLSS-DLAA-1.8.4/shadPS4.exe untouched.
- Historical working-world DX12 C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261012-ba3892f-covered/shadPS4-DX12-Core.exe.
- Seed F:/PS4Dev/handoff/coherence-20261011/seed. Existing XInput/HUD/live-monitor F:/PS4Dev/handoff/vulkan-live-monitor/program.
  Flydigi client C:/Program Files/Flydigi Space Station/driver/flydigimapclient.dll, SHAb4af07797a2e3c8150a033a5c4425530a330036891d113cb78a22d065e8b4b5a.
  Own pads neutral/stopped/device removed; all own test windows closed. User monitors/session untouched.

## H. Next exact action

First verify git status --short and git rev-parse HEAD in F:/PS4Dev/work/shadps4-dx12-src, then read
F:/PS4Dev/handoff/black-continuation-20261014/avplayer-fixed-game/RESULT.json and stdout.txt critical assertion.
Next concrete change: in src/video_core/amdgpu/liverpool.cpp EventWriteEop callback, report the failed address,
data width/value and packet data-select before the existing fatal assertion. Trace that actual range through
Core::Memory::TryWriteBacking, remap/unmap/protection and PM4/EOP lifetime/queue ordering; compare pinned Vulkan.
Current code reading: TryWriteBacking returns false when its overlapping physically backed VMA list is empty;
its IsValidMapping assertion did not fire in this run. This narrows the observation to backing/range selection,
not a demonstrated CPU PageWatch protection rejection. Address/VMA details are still missing; no root cause claimed.
Do NOT ignore the failed write, disable PageWatch or invent a fence acknowledgement. This code was already
dirty before this session; preserve its provenance/whole-tree archive and isolate any next-session delta.
After fixing that blocker, resume scene-color/ROP attribution with existing basic graph data:
F:/PS4Dev/handoff/black-continuation-20261014/producer-graph-game/display-capture/PRODUCER_GRAPH_SUMMARY.json.
Exact saved analysis command:
python "C:/Users/skei/Documents/Codex/2026-10-12/skiters-ps4-dx12-fix-mixed-cmask/work/analyze-producer-graph.py"
Inspect target-before versus output HDR/scene-color passes and actual requested-view/blend metadata;
use saved production fragment SPIR-V/DXIL (dxc -dumpbin FILE, no -all) to attribute the earliest genuine loss.
Then patch the demonstrated producer/binding/export/ROP contract in GuestRasterizer::DrawInternal,
ExecuteGuestResources, GuestTextureCache::Upload/Entry::Srv or corresponding shader emitter/compiler.
Do not rewrite display selection, disable depth/culling or blame bloom solely from a sparse input.
Add a failing-before/passing-after GPU case to existing graphics/coherent/alias tests; incremental core via
"C:/Users/skei/Documents/Codex/2026-10-12/skiters-ps4-dx12-fix-mixed-cmask/work/Build-Aliases-Core.cmd", targeted Debug Layer tests, one necessary postfix XInput test.

## I. Do not repeat

No overall port audit/HUD/profiler rebuild, EmitImageWrite speculative LOD rewrite, old F1-F3/resident-index
or memory-budget investigation without new evidence. Do not re-run identical launches or full suites to fill
reports. Preserve noaccess/independent domains, physical padding, barriers/fences/allocation/descriptor leases.
No reset/clean, forced push, deleting old captures, replacing Vulkan, modifying original saves or session reset.


Authoritative continuation for this repository. The latest human request resumed
development and explicitly permits GPU/regression/game checks after logical changes.
The emergency-only stop and old whole-port-before-any-test gate are superseded.
Older current-state headings in other notes are historical. No new overall audit was run.

## Active packed-gap continuation — 2026-10-13 evening

This section supersedes the older point-capture checkpoint below. Source commit
`3f1249b` was pushed to origin/fix/mixed-cmask. It preserves seven selected files,
including their previously uncommitted integration; it is explicitly WIP.
The 22 pre-existing staged network deletions remain outside this commit.
Exact new-session delta, before/current sources and index backup:
`F:/PS4Dev/handoff/packed-gaps-20261013` (`session-only.patch`, `changed-sources.zip`).

- The saved EmitImageWrite assertion was a malformed hand-emitted fixture:
  its sampled-output single-mip UAV received LOD=0 while metadata said None.
  Pinned Vulkan and ResourcePatchingPass already drop LOD for a fixed view;
  packed_image_test.cpp now follows that contract, retaining dynamic mip/sample
  addressing and every content/padding expectation. The production assertion
  was NOT bypassed. Do not repeat the earlier speculative emitter-LOD rewrite.
- Production EmitImageRead now reconstructs exact guest 5/6-bit UNORM values
  after native B5/B6 unfiltered fetch (including SRV component selection).
  Filtered samples are not requantized. Formats16/17/18 linear+tiled+MSAA:9/9 PASS.
- LayoutGuestTexture/HasExactPackedExpansion/GuestTiler/GuestMultisample now
  support sampled format19 MSAA through existing GPU float4 expansion and raw
  sample transfers. Exact guest packed backing, sample/layer layout and padding
  are retained. Format19 sampled+backing tests:7/7 PASS, including former two
  unsupported-RT backing failures. Full guest format19 render-target blending/
  quantization is still open; the fallback is explicitly for sampled resources.
- Stage24/pipeline4/metadata-v33 invalidates old shader semantics. Core build
  succeeded (command-20261013-223852-41016.log), executable SHA256
  `25ca1dee9a3d0c9348bc8dba01850e67604476f6ed2bc54c1cc1b5644fee2872`.
- Packed10 UNORM MSAA passes the current rerun without a separate fix. Its old
  mismatch is not declared resolved; retain as non-reproduced evidence.
- Final configured suite: **324/344 PASS,20 FAIL**. Native subset: **209/227
  PASS,18 FAIL**. This combines retained first49 and same-selection CTest -F;
  signed-packed sampled timed out60s. It is not a wholly green suite or a claim
  that all35 native improvements since174/227 are due to this session's fixes.
  Full log command-20261013-224303-101900.log; detailed failure list/provenance:
  F:/PS4Dev/handoff/packed-gaps-20261013/suite-summary.json and suite-*.log.
- Stronger final packed fixture rejects NaN and passes no LOD for MSAA; all12
  linear/tiled/MSAA cases16–19 PASS in56.17s, raw command-20261013-225604-72632.log.
  This fixture-only hardening followed the full suite; production code unchanged.
- Exactly one new game run, bloodborne-packed-gaps, exited0 after123.8697s.
  Executable25ca1dee..., PID48864, XInput daemon14484/session
  da65fd4a5f2d48a9b79a8aa7277ee20a. A200ms + neutral acknowledged; controller
  removed on game exit. Original20 seed saves verified unchanged; no remote
  session, driver/service or original Vulkan changes.
- **BLACK IMAGE CONFIRMED** in both engine-generated screenshots: game-only
  source225745 and output-with-overlays225822 (PNG files in run/user/screenshots).
  Both were viewed. PM4 draw/dispatch, GPU submissions and Present continue.
  2794 paired completed guest frames/93.101581s yields~29.99949 completions/s,
  median33.363ms,P9535.459ms; this is BLACK-FRAME THROUGHPUT, not world/gameplay
  FPS. No gameplay response or matched speedup accepted. The defect is present
  by the snapshot/source-to-present boundary; exact first producer remains unknown.
  Do not attribute it solely to Windows.Graphics.Capture or claim recovery.
- Installed preview21: dx12-20261013-full-port-preview-25ca1dee9a3d,
  C:/Users/skei/AppData/Local/PS4Desktop/cores/<id>/shadPS4-DX12-Core.exe,
  matching symbols/provenance;76 runtime hashes and20 copied saves verified.
  Existing PS4Desktop and previous selected package were preserved.
  Game report: F:/PS4Dev/handoff/point-capture-20261013/bloodborne-packed-gaps/RESULT.json.

The previous preview20 short run (bloodborne-short-2207) completed4495 paired
guest frames over149.785095s (~30.003/s), exit0 after180.843s; image/gameplay scene
was unverified because Windows.Graphics.Capture failed. It is not a world FPS
baseline and cannot be compared with historical1.377539FPS.

Next production work: narrow the black source boundary using the saved two
captures, current PM4/renderer logs and the existing surface path:
MemorySurfaceProvider::Snapshot -> GuestRasterizer::PrepareDisplayTexture ->
GuestPresenter::PrepareFrame/Present. Determine whether the current guest color
writer produces black or an obsolete/incorrect image version is selected for
VideoOut; fix that demonstrated ownership/render path. Do not rerun the same
game launch until a concrete change or a necessary bounded observation is ready.
Reuse native_coherent_sampled_attachment and native_guest_buffer-image-alias/
partial as existing regressions; do not invent a new profiling framework.

Other remaining failures (full list in suite-summary): odd buffer byte range,
arena residency, 10-bit VideoOut, two old texture fixtures, signed-packed sampled
timeout, CMASK, PageWatch line219, owned-image dispatch/sample forced waits,
BC volume update, MINMAX-sRGB, mixed DS/SRV planes/view ranges, dual-face culling.
Cold runtime compilation contributing to the owned-test3s threshold is only a
hypothesis; preserve fences/PageWatch while diagnosing it.
No new whole-port audit, point replay rerun or whole-group closure claim.

## Historical point-capture checkpoint

## A. Current state

- Repository `F:/PS4Dev/work/shadps4-dx12-src`; branch `fix/mixed-cmask`, upstream
  `origin/fix/mixed-cmask`, remote `https://github.com/Skiters/ps4-dx12.git`.
- Source checkpoint `0a3571a4b76bf1a6683a47f1ee409fe2555614a3`. A later documentation checkpoint may be HEAD;
  exact final HEAD/status/push evidence is in `F:/PS4Dev/handoff/point-capture-20261013/CHECKPOINT_FINAL.json`.
- Source remains dirty with broader unfinished full-port changes and pre-existing
  staged network deletions. The source commit preserves 32 selected implementation/
  fixture files; it is WIP, not a claim that the whole port is complete. Original stage split was verified unchanged.
- Registry: 128 partial groups, zero whole-port semantic closures. Last overall
  audit was 126 groups/1,501 associations/13 mappings; do not imply a fresh audit.
- Ordered point capture/replay now runs on hardware. Ordinary shaders switch to
  it only when the explicit oversized point signature/output-budget exception is raised.
- Native MSAA color upload/sample-copy now initializes fully overwritten placed
  color layers before draw. Depth/stencil and partial-plane semantics are preserved;
  complete MSAA/packed-format parity remains open.
- Current Bloodborne world/control/image/FPS acceptance is NOT established.
  Historical working-world runs concern older executables. Current launch showed
  a black window and no new completed frames in the short capture period.

## B. Implemented production changes

- `guest_shader_frontend.cpp`: `BuildPointCaptureGraphics` compiles original ES/GS
  producer and original FS plus generated replay VS/GS; restores compilation profile
  with RAII. Failed ordinary specializations cache `NativePointCapture::Required`.
- `emit_spirv_quad_rect.cpp`: generated `EmitNativePointReplayVertex/Geometry` read
  typed R32_UINT t0/space33 records; preserve position/size, sparse clip/cull,
  viewport/layer, depth and original PrimitiveID. Original guest GS is not rerun per corner.
- `native_point_capture.h`, `emit_spirv_point_capture.h`: 152 words/608 bytes per
  record; valid tag is original PrimitiveID+1; checked byte-address/instance bounds.
- New `guest_point_capture.cpp/.h` and `guest_point_capture_clear.hlsl`:
  budgeted placed scratch reuse, tag-only CS clear, disjoint producer instances,
  UAV ordering, typed-SRV replay, original attachments bound/cleared once,
  descriptor/allocation/resource leases retained through intermediate Finish/final fences.
  No normal CPU scratch readback or unconditional additional Finish.
- `guest_rasterizer.cpp`: production Required fallback, original-resource producer
  preparation followed by fragment/replay preparation, correct point front-facing
  orientation. Tessellation/on-chip/multiple GS streams remain unsupported here.
- `ring_access_elimination.cpp`: immediate scalar SOFFSET contributes to coherent
  ES/GS ring addresses. Previously wide attribute stores overwrote position words.
  Dynamic scalar offsets remain open.
- `rasterizer.h`, `guest_compute.cpp`, `guest_rasterizer.cpp`: memory extents are
  checked by their owner rather than initializing a process-global guest singleton
  in standalone renderers. Host ranges reject oversized descriptors; Core uses PS4 ClampRangeSize.
- `guest_pipeline_persistence.cpp`: Stage23/pipeline4/metadata-v32 invalidates old
  captured-record/ring-address semantics. Shader signing/serialization fixtures updated.
- `guest_multisample.cpp/.h`: transfer leases retain physical array layer separately
  from row-pitch constants. Full color slice upload/copy transitions to RT then
  discards exactly that overwritten subresource before the native shader draw.
  `multisample_upload.h` initializes its fully overwritten whole color array.
- Fixtures now check actual CPU-copy/GPU-detile pending work, whole PageWatch pages,
  native VideoOut SRV mapping and UNORM gamma clamping; image/padding/sample comparisons remain.

## C. Last unfinished work

MSAA placed-resource initialization is implemented and targeted checks improved.
The next production blocker exposed by those checks is `EmitImageWrite` LOD fallback
at `src/shader_recompiler/backend/spirv/emit_spirv_image.cpp:610`.
`native_packed_16_msaa`, `_17_msaa`, `_18_msaa`, `_19_msaa` now reach an assertion
requiring DynamicIndex mip fallback. Determine the actual emitted LOD and mip/view
metadata; implement correct constant/dynamic/native-MSAA storage selection using
the existing Vulkan/shared emitter semantics. Do not bypass the image write.

Also unfinished: packed10 UNORM word preservation; packed16/17/18 sampled
normalization; format19 multisample RT capability/fallback; owned-image dispatch
completion waits; buffer/image prewrite snapshots; CMASK, DS/SRV planes and residency.

## D. Proven issues versus hypotheses

Proven: last complete native suite has 53 failures; exact names/reasons are in
`C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final-native-summary-20261013.json`.
After it, targeted MSAA tests show three packed10 modes passing, one UNORM mismatch
and four packed-format LOD assertions; shared color layers passes. Retain both pre-
and post-fix logs. Init errors, format capability failures, padding/value mismatches
and forced waits are separate problems, not one cause.

Hypotheses only: the Bloodborne black window may involve launch visibility,
presentation or renderer/guest progress. It is not established that the new point
path caused it, that the game reached Hunter's Dream, or that current FPS improved.
First run used Start-Process WindowStyle Hidden; screenshot helper later failed
`IGraphicsCaptureItemInterop.CreateForMonitor` (0x80070057). A future interactive
validation should use an observed visible test window and avoid blind input.

## E. Measurements and provenance

- Final complete suite BEFORE final MSAA init fix: 174/227 passed, 53 failed,
  822.034 s, raw log `F:/Codex-Shared/devtools/logs/command-20261013-172342-31160.log`.
  Earlier resumed suite: 110/227 passed, 117 failed, 665.41 s. This is test evidence,
  not a performance improvement; many removed failures were outdated fixture assumptions.
- Nine unique point modes x96 cold/warm/facing/flip/cull cases =864 hardware cases.
  Includes two GS invocations and three draw instances; whole 64-KiB guest output,
  atomic counters, depth/stencil and padding checked. Known half-UNORM midpoint
  accepts 127/128 only at explicitly expected color bytes; other bytes stay exact.
- Fresh metadata32 six signed producer/FS/replay chains and typed SRV proof:
  `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final-offline-20261013`,
  `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final32-proof.jsonl` (32 original source/component offsets).
- Final point GPU logs: `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work/point-capture-final32-gpu-*.log` and
  `point-capture-final32-three-instances-gpu.log`; latter raw log
  `F:/Codex-Shared/devtools/logs/command-20261013-174006-85624.log`, 8.56 s.
- Post-init: `msaa-initialization-regressions-20261013.log` (3/8 pass, 57.491 s),
  `msaa-initialization-packed-backing-20261013.log` (14/16 pass, 74.764 s),
  `msaa-initialization-all-backing-20261013.log` and shared-layer raw log
  `F:/Codex-Shared/devtools/logs/command-20261013-175340-15484.log` (pass, 15.062 s).
  The complete targeted backing rerun passed46/48 in208.566s; remaining format19
  sample2 cases explicitly lack native RT support. Raw log:
  `F:/Codex-Shared/devtools/logs/command-20261013-175541-94968.log`.
- Game run `F:\PS4Dev\handoff\point-capture-20261013/bloodborne-current`: executable
  `d2684254a886ad3102792cab2f2b16edd2c707c999b0e19f1f95ea555f45cbe5`, build from
  dirty source based on 676d628; manifest/patch/logs/exit/capture/XInput are local.
  XInput session `a503e4b259e24d31a0eb34d6eac5af3d`, PID41608, feeder70912,
  date 2026-10-13; input A and neutral verified, reaction not verified. Exited0 by
  owned CloseMainWindow, feeder neutral/detached. No valid current completed-guest-FPS
  sample; do not report zero FPS or compare this to a world baseline. FG disabled.
- Reuse existing Vulkan evidence `docs/codex-progress/vulkan-session-20261011` and
  `F:/PS4Dev/handoff/performance-recovery-20261011`. Historical DX12 B:124 completed
  frames/90.015605s =1.377539 FPS, median interval727ms/P95765ms. A/A2 failed loading;
  no matched speedup comparison exists. Passive143/90.098142s =1.587158 had uncontrolled input/focus.

## F. Verification boundaries

Core/check builds pass. Six final offline chains and nine final point GPU modes pass.
Metadata/buffer dispatch and resident indices16/32/UAV/UAV-write pass. Current full
suite is NOT green; do not turn targeted reruns into a fabricated full-suite result.
No current game-world, gameplay response, stable image or FPS acceptance.
No original Vulkan or original saves changed. Partial-render disabling was not used.

## G. Preserved builds and files

- Latest experimental core `F:\PS4Dev\work\shadps4-dx12-src\build-migration-core\shadPS4-DX12-Core.exe`, SHA256 `34d785e8b1dcee941daa8b2447ec9bd164dff59a6bfa39d525b548a7eaf06d29`;
  matching `shadPS4-DX12-Core.pdb`, `native-core.map`, `native-compiler/` alongside.
  Build logs `point-capture-metadata32-core-build-20261013.log` and
  `msaa-initialization-core-build-20261013.log` in `C:\Users\skei\Documents\Codex\2026-10-12\skiters-ps4-dx12-fix-mixed-cmask\work`.
- Immutable original source `F:/PS4Dev/work/shadps4-original-e0a46675`, SHA
  e0a466753a1aacbc8db8253be685bf7ab4d9daae. Installed Vulkan/DLSS/FG
  `F:/Apps/shadPS4-DLSS-DLAA-1.8.4/shadPS4.exe` remains untouched.
- Historical working-world DX12:
  `C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261012-ba3892f-covered/shadPS4-DX12-Core.exe`.
- Existing desktop `C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/PS4Desktop.exe`;
  installed preview18 `dx12-20261013-full-port-preview-519d8356217d` remains immutable.
  New preview19 is installed as `dx12-20261013-full-port-preview-34d785e8b1dc`,
  display name `DX12 — перенос №19 — 13.10.2026`, same latest executable SHA256.
  Its directory is `C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-20261013-full-port-preview-34d785e8b1dc`;
  matching symbols/provenance/runtime files are retained there. Isolated profile:
  `C:/Users/skei/AppData/Local/PS4Desktop/core-profiles/dx12-20261013-full-port-preview-34d785e8b1dc/native-profile`.
  All76 runtime-file hashes and20 copied saves verified; prior selected package/settings
  selection and original save hashes preserved. No gameplay success claimed for preview19.
- Seed saves `F:/PS4Dev/handoff/coherence-20261011/seed`; isolated game profile
  `F:\PS4Dev\handoff\point-capture-20261013/bloodborne-current/user`; original save hashes verified while copying.
- XInput/HUD/live monitor `F:/PS4Dev/handoff/vulkan-live-monitor/program`.
  Existing user monitor/PresentMon instances are not owned; do not stop them.
- All source before the new commit: `F:\PS4Dev\handoff\point-capture-20261013/before-source-commit`:
  verified zip, working/staged/unstaged binary patches, original index and file manifest.
  Final residual archive is recorded in CHECKPOINT_FINAL.json. Do not reapply patches
  to this existing dirty tree. Use a separate clean checkout of the recorded archive HEAD
  if recovery is necessary. Old emergency backups are preserved unchanged at
  `F:/PS4Dev/handoff/full-port-emergency-20261013`.

## H. Exact next action

Read this file and final checkpoint JSON, check HEAD/status, preserve residual changes.
Then run the existing failing case (no general audit):

```powershell
& 'F:/Codex-Shared/devtools/bin/codex-dev.cmd' rtk ctest --test-dir 'F:/PS4Dev/work/shadps4-dx12-src/build-migration-checks' --output-on-failure -R native_packed_16_msaa
```

Continue production `EmitImageWrite`/native storage mip selection from its saved
assertion. Inspect only relevant IR/resource metadata and pinned Vulkan/shared
behavior. Implement real image-write support; build incrementally, verify affected
packed/MSAA/alias cases, then minimally test Bloodborne through the existing virtual
XInput in a copied profile and measure completed guest frames without FG.

## I. Do not repeat without a new failing case

No new whole-port audit or rereading old chat/architecture. Typed SRV/t0-space33/
152-word addressing, signature packing, Private pointers, literal ring SOFFSET,
point front-facing and producer/replay instancing are already proved by saved cases.
Do not repeat the raw readonly-SSBO experiment (it became a UAV). Do not weaken
memory/ownership/fence/alias operations to improve FPS. Do not silently ignore the
53 complete-suite failures. Never discard partial DS/other layers to initialize a
color transfer. Initial erroneous discard used row pitch as layer; corrected explicit
layer is verified. Do not rerun dozens of identical game launches. Never reset/clean
this tree, force-push, replace installed Vulkan, mutate original saves or delete old results.

## Historical next-session prompt — superseded2026-10-14

> Продолжи Skiters/ps4-dx12 на fix/mixed-cmask с актуального HEAD. Прочитай
> активный packed-gap раздел SESSION_HANDOFF.md и
> F:/PS4Dev/handoff/packed-gaps-20261013/CHECKPOINT_FINAL.json. Сохрани dirty tree
> и22 staged удаления. Packed16–18 fetch и sampled packed19 MSAA исправлены;
> ImageWrite assertion был нарушением контракта fixture, не переписывай emitter
> по старой гипотезе. Полный suite324/344, native209/227;12 усиленных packed PASS.
> Следующий блокер: оба штатных снимка Bloodborne чёрные при продолжающихся
> PM4/GPU/flip/Present. Локализуй и исправь первый неправильный producer/version
> в MemorySurfaceProvider::Snapshot -> GuestRasterizer::PrepareDisplayTexture ->
> GuestPresenter::PrepareFrame, используя сохранённые логи/снимки и Vulkan-эталон.
> Используй existing coherent sampled/alias regressions, затем сборку и один
> необходимый короткий изолированный XInput-прогон. Проверяй фактическую сцену,
> а не только отправку ввода;30 black completions/s не игровой FPS. Не трогай
> удалённую сессию, оригинальные сохранения/Vulkan. Продолжай production C++/HLSL,
> сохраняй и push проверенные commits, не повторяй общие аудиты.

## NEXT CODEX SESSION - START HERE

> Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask.
> Read the active complete-port section of SESSION_HANDOFF.md and
> F:/PS4Dev/handoff/complete-port-20261015/CHECKPOINT_FINAL.json, then verify actual HEAD.
> Preserve the broad dirty integration and22 original staged network deletions; use
> final-residual only for recovery into a separate clean checkout, never over this tree.
> Source commits35b5de4/d7d9064/440dd53/ad6808e/e359447 cover WAIT snapshot, typed/raw CMASK,
> independent MSAA transfer/scratch pressure, color masks, exact packed-float clears and
> raw RGBA8 MSAA transfer/nonzero clear. They are source-reviewed only, not compiled or installed. Preview24
> remains installed and unchanged. The user still prohibits tests/game/GPU/UI/XInput;
> do not request execution while independent source work remains. Start the concrete
> independent packed10/packed16 MSAA reinterpretation continuation documented above,
> preserving raw guest words/masks/private padding and existing cache/MSAA contracts. Reuse saved Vulkan/DX12 measurements and129-group contracts.json;
> do not repeat complete audits, format19 swizzle or solved position-export investigations.
> Preserve PageWatch, separate memory domains, padding, barriers, fences and resource
> lifetime. Do real production C++/HLSL work and update the current parity boundaries.
> Once final execution is explicitly allowed, build and run affected regressions first,
> then minimally verify Bloodborne with the existing copied profile/virtual XInput,
> comparing completed guest frames (without FG) with saved baseline. Do not touch original
> saves/Vulkan/session. Commit coherent WIP, push configured upstream without force,
> and checkpoint exact continuation. Full parity/world acceptance remains unfinished.

## NEXT CODEX SESSION — START HERE (packed-MSAA checkpoint, 2026-10-15)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask.
Read the top packed-MSAA section of SESSION_HANDOFF.md, STATE.md and NEXT_SESSION.md;
verify actual HEAD and residual state against F:/PS4Dev/handoff/packed-msaa-20261015/CHECKPOINT_FINAL.json.
Preserve broad dirty integration and22 staged deletions. Restore nothing over this tree.
Continue source porting from exact packed-float independent MSAA formats6/34 using
CanTransferIndependentMultisample, PackedFloatCasts and GuestMultisample actual cast guards,
then the existing partial-alias/cache queue. Use saved Vulkan/DX12 evidence; do not repeat
completed audits. Current tests/game/GPU/XInput/desktop/build gate remains until expressly
lifted. After source completion and authorized final verification, build the incremental
core, run focused regressions and minimum isolated Bloodborne/XInput trials, compare real
completed guest frames (no FG) against saved baseline, verify image/controls/stability,
release the virtual device, and install a new core only as authorized. Keep the goal active;
commit coherent WIP source work, push the configured upstream, and update this handoff.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask.
Read the top active packed-float/RGB32 section of SESSION_HANDOFF.md, STATE.md and
NEXT_SESSION.md; verify actual HEAD against packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Preserve dirty integration and22 staged deletions; do not restore anything over this tree.
06536f2/6ded358 implement packed-float MSAA and readonly RGB32 ownership/transfer in source
only. Start the precise CMASK6/7 selected +INF decoder continuation in
F:/PS4Dev/handoff/packed-float-msaa-20261015/rgb32/NEXT_SOURCE_TASK.md. Keep NaN unresolved
until its conversion contract is established; reuse finite RTZ and saved Vulkan evidence.
Do not repeat completed audits or invent writable13/34 parity. Continue real C++/HLSL work
with PageWatch, physical/image separation, exact padding, barriers, fences and lifetime intact.
The user's tests/game/GPU/XInput/desktop/build gate remains active. Once final execution is
authorized, build incrementally, run affected regressions and minimal isolated Bloodborne
trials through the existing copied profile/virtual XInput; compare completed guest frames
without FG to the saved baseline and check image, controls and stability. Preview24 remains
installed unchanged. Commit coherent source WIP, push configured upstream without force,
and preserve the exact next step. The full-port goal stays active and unfinished.
