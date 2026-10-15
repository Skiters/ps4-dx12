# SESSION HANDOFF — active full-port checkpoint, 2026-10-15

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
