## Unaligned physical-page buffer witnesses — 2026-10-19

Latest exact-game result belongs ONLY to36a33da EXE39ff00de6d3de44c797cd5099f89713714a2b5e0b6c0b8ca559ac777a5cc0a17/PDB9980051723fc39c5391dd3e366ef5e27fe033385e9ce332265b750a2ab130797: PID57568 reached mostly black world,26 completed new guest frames/77.18774s=0.336841058FPS, invalid0/pending0. No demonstrated FPS improvement over b7bf8240.363FPS; same profile/area but camera/workload not strictly controlled. Menu opened after Start4000ms;200ms can miss guest polls at this speed. Native screenshots194439(world)/194618(menu); exact paths/hashes and frame scope in F:/PS4Dev/handoff/physical-backing-proof-20261019/game-36a33da. Own Alt+F4, process gone, feeder neutral/removed. Visual/performance FAILED.36a33da installed Experimental manifest records failure.

Production follow-up: regular readable B watches/strong B ownership now bypass the extra protected-epoch lookup.36's protected-backing-hit byte counter ALSO counted ordinary preexisting readable watch hits; do not interpret its~78GB/window as incremental savings. At t240.117 resource buffer-views622.391ms, backing-validation387.805ms, snapshot-compare82.397ms, compared3451349400B/read2317081024B (nested2s CPU counters, not comparable sequential frame times).

Confirmed remaining expensive allocation example from that run: backing-validation base0x276719940 bytes62219968, unaligned. GuestBufferAccess write-watch previously rejected such mappings outright. Now TrackReadOnly uses GuestProtectionPages and validates ALL rounded physical backing, while native mapping/shader bounds stay exact. ReleaseCpuBufferWatches/InvalidateCpuBufferPages search page intersections so real stores before/after a logical sharp invalidate every witness on that page. Unaligned written envelopes retain RO partial proofs and upgrade only actual writable page ranges; dropping the whole RO guard on failed whole-envelope upgrade lost a retained image page (caught by unchanged f2/f3 negative, corrected). Strong I is never demoted to acquire a B watch.

Verification: new native_unaligned_buffer_page_witness beforeFAIL0.43s (no watch), afterPASS0.38s. Actual GPU readback, zero warm full comparisons, genuine neighbouring CPU writes before/after an unaligned mapping, physical-page CPU winners/untouched GPU bytes, TWO disjoint native B mappings on one watched page, neighbour retirement, and negative unavailable rounded backing. Five related PageWatch/physical-epoch/f2/f3 tests PASS4.48s; existing envelope/residency/lifetime negatives retained. Main Release PASS9.884s command-20261019-195639-14728.log. Latest full-core GCN linear-attachment-owned PASS0.416s command-20261019-200537-53640.log.

GCN clear fixture now uses actual PM4 SetShReg/DispatchDirect, binds/unbinds its rasterizer and drains queue; no out-of-queue GetCsRegs/assert bypass. Earlier compact test's unchanged quarter came from missing fixture rasterizer binding (dispatch skipped), not established production corruption. With actual clear executed, it reaches a separate preexisting color export issue: R32_FLOAT0.5 source yields output UNORM8 byte127 instead of expected128 (data4/number7/layers1/mip0/check5). EXACT same case confirmed on36a33da baseline production with only saved queued-clear fixture patch (command-20261019-200503-64948.log); main200333-44932. SNORM129->128 baseline discrepancy also remains. Expected data unchanged. Clean baseline's temporary fixture patch removed with scoped reverse patch; no reset/unknown overwrite. Do not count either whole GCN matrix as passing.

Next: committed clean Release/safe sole-core replacement for unaligned witnesses; do NOT transfer36a33da game result to new EXE. No further game run until another logical mechanism/check boundary. Then verify actual whole-envelope comparison reduction and guest FPS; black scene/color conversion and remaining functional parity still open. For numeric issue inspect source image raw bits vs exported UNORM8, establish pinned export/precision contract before changing production/oracle. Avoid redoing general profiling/audit or disabling protection, copies, operations, assertions.
## Protected physical-B proof mechanism — 2026-10-19

Connected production: PageWatch::ProtectedBackingEpoch returns a non-owning exclusion witness only for complete protected coverage; it does not promise readable VA or change RO/NOACCESS ownership. InvalidateBackingEpoch records physical-alias writes separately from VEH ownership generation (unresolved fault detection remains intact). Range splitting retains physical-write history. GuestBufferAccess buffer commit and GuestRasterizer image/backing commit wrappers invalidate it. Rasterizer::PrepareBufferContents and SynchronizePreparedDmaBuffers retain a matching full CPU snapshot's epoch, avoiding repeated physical copies/compares beside opaque I; CPU dirty markers, snapshot resize, changed/missing guard coverage, host publication and reentrant guard callbacks invalidate/recheck it. Uploads, draws, copies, states and lifetime remain normal. This adapts pinned MemoryTracker CPU-dirty exclusion/ForEachUploadRange semantics; it is not a claim of complete MemoryTracker parity.

Verification: native_page_ownership_and_cache, guest_buffer_watch_transfer, guest_buffer_gpu_chain, new protected_physical_backing_epoch PASS4/4,3.75s. New proof fixture checks actual GPU B readback, distinct GPU-only B vs physical CPU values, NOACCESS I guard unchanged, zero warm direct/DMA physical rereads, explicit physical publication, same-value CPU store, guard rearm, and reentrant publication before current binding. PageWatch checks missing coverage/overflow, split history and a physical epoch bump failing to suppress an unresolved injected AV. Existing buffer-envelope/semantic-residency/reentrant CPU fixtures are included in f2/f3. Main Release PASS22.501s command-20261019-193203-26496.log. Actual GCN linear-attachment-owned PASS0.399s command-20261019-193818-57256.log: queued native MRT download, independent raw B, repeated bindings, CPU store/full padding. Initial invocation without required diagnostic counters falsely failed the combined count/content guard; fixture now enables its required counters itself. No expected data changed.

Broad separate-buffer-image-owned GCN fixture FAILS at R8_SNORM(data1/number1),layers1,mip0,writer4096,initial0.5,check9: color byte61440 actual128 vs expected129. Same exact case/value independently reproduced on clean b7bf824 production objects with only a saved fixture-print patch (command-20261019-193758-65908.log; main193604-51048). Baseline print patch removed safely; clean checkout tracked tree restored. This is a PREEXISTING numeric/rounding discrepancy, not proof of passing that matrix; expected values unchanged and cause still open. Do not claim color/performance acceptance from narrow proofs.

Reports/small baseline diagnostic patch: F:/PS4Dev/handoff/physical-backing-proof-20261019. Main fixture hashes captured in verification.json. No new game/profiler or runtime archive yet. Installed core is still b7bf824 with failed world0.363FPS/black-image status until committed clean replacement. Next: clean Release of this mechanism, safe single-core publication and one bounded same-profile world measurement; compare physical-copy/snapshot counters and completed guest FPS to saved b7bf82444.019s baseline. Black rendering and SNORM discrepancy remain explicit correctness gaps; no full audit restart.
## World acceptance FAILED at b7bf824 — 2026-10-19

Sole installed dx12-current source b7bf824 (EXEc67faaa57486765c504810a990f9c1b129fcc0cc4e38ee19a4f0d5aa35e6a0d7, PDB68c35c365db9096950d5212656fee125541e80ffa5ad38c0e01c00e72990a458), clean Release PASS19.998s command-20261019-191444-80520.log, pinned dependencies/no primary-source leak. Source pushed. Existing profile preserved; manifest explicitly marks failed game acceptance. No new full runtime archive.

Owned PID48048 started14:15:35Z, title/Continue via XInput observed, passed both earlier depth and formatted-address rejections, reached post-load scene fragments and final character silhouette. Image mostly black (native1280x800 screenshot C:/Users/skei/AppData/Local/PS4Desktop/core-profiles/external-7e8fca7a1bd828c19629fdd9/native-profile/user/screenshots/CUSA03173_20261019_191844_575_game_000000.png). Camera RX input delivered/neutral verified, reliable in-world camera/movement reaction NOT established. Marker only after post-load world fragments:16 unique fence-complete new guest flips/44.019454s =0.363475658FPS, invalid0, pending0 at stop. This excludes FG/reused presents. No same-scene Vulkan comparison. VISUAL/PERFORMANCE/CONTROL ACCEPTANCE FAILED; do not call this working or cite startup29.925FPS. Test ended by own Alt+F4, process absent, feeder stop_requested/neutral/device_removed=true.

Existing-run performance evidence, no new profiler needed: at t220.097 in a2s window, resource-bindings/buffer-views1089.225ms; backing-validation701.800ms; snapshot-compare216.043ms; physical-image-overlap snapshot reads6164102208B and snapshot comparisons9508957216B. Values are separate nested CPU-wall aggregates/counters, not additive sequential GPU frame timings. It is a confirmed costly path, not exclusive proof of every visual/FPS cause. Report/CSV/full native diagnostics F:/PS4Dev/handoff/formatted-address-20261019/game-b7bf824; frame-result.json and verification.json hold scope/hashes.

NEXT production work: Rasterizer::PrepareBufferContents rasterizer.cpp~865-970 and SynchronizePreparedDmaBuffers~1620, GuestBufferAccess::SetBufferBackingAccess write-watch callbacks, PageWatch::TrackReadOnly, and GuestRasterizer::ResolveImage/ResolveCpuFault. Full physical-B copies/equality scans recur beside exclusive NOACCESS I because readable B witnesses cannot acquire that stronger image guard. Port a domain-aware CPU-dirty/proof lifecycle or equivalent pinned MemoryTracker mechanism. DO NOT blindly pass allow_gpu=true, skip comparison, or demote I: opaque image download writes physical B and untracks I while a borrowed B equality proof would become stale; CPU read/write/remap and guard retirement must invalidate the correct proof. Existing partial-proof/reentrant guest_buffer_access fixtures and independent I/B fixtures must cover real GPU readback, same-value CPU stores, image download, boundary/lifetime/negative cases. Also keep almost-black rendering a separate open correctness defect. No repeated world launch before a finished mechanism is verified.

Temporary e92409d crash EXE/PDB removed only after reproduced GPU regression and successful passage into world; small source/runtime identities and logs retained at depth-subresources-20261019/game-e92409d/exact-crash. Other unresolved crash bundles unchanged. Current sources clean apart from preexisting unknown .serena/, CLAUDE.md and shader-reload-fixture-46128-24394234/.
## Single-stage formatted address-class synchronization — 2026-10-19

Production ExecuteGuestResources now matches pinned BufferCache formatted sync -> FindImageFromRange: readonly formatted reader candidates use original sharp address>>8 plus SafeToDownload (GpuModified and not Dirty), rather than treating any overlapping independent image as a required source. No candidate, or multiple without requested-size match, leaves ordinary B binding intact; selected non-exact original sharp still rejects. Simultaneous-writer selection/validation stays explicit. No draw/dispatch skipped, assertion suppressed, or physical I/B domains merged.

New native_single_stage_interior_formatted_reads executes real ExecuteGuestBufferCompute metadata preparation plus an empty optimized shader dispatch: eight cases offsets0/1/256/257 x modified/initialized-readonly I, compares GPU B readback/guest padding, native I readback/ownership/history, unchanged CPU physical bytes. This models original optimized-away descriptors' binding effects; it does not prove shader buffer-load lowering. Before fix FAIL10.94s on address admission; after PASS28.89s. The earlier fixture setup failure used host address outside PS4 Buffer40-bit field; fixture now reserves explicit low VA. Existing ordered-interior GPU suite PASS14.52s. One development readback requested an unprepared leading envelope; fixture now materializes ordinary B before full-envelope readback. Main full Release PASS12.163s command-20261019-191353-25472.log. Logs F:/PS4Dev/handoff/formatted-address-20261019.

Actual e92409d game combined throw lacked original request/candidate values; this correction is proven for a reachable production discrepancy, NOT yet proven to remove that exact game failure or user1FPS. Next: clean committed build, safe sole-core replacement, one Continue check and world FPS only if actual world observed. Remaining simultaneous-writer plans, shifted DSV/left merges, final parity and game correctness remain open.
## Exact e92409d clean/game boundary — 2026-10-19

Clean committed Release PASS19.560s, pinned dependencies, no primary checkout path in compile_commands/build.ninja/CMakeCache (command-20261019-190424-27952.log). Installed sole dx12-current source e92409d EXEd2b767b3200840c57769d66be59edcb38499b174a357a69fbf5644bea95aec41/PDB355a974a8cc4bd9b2ff703f778028b700ec7bde016371974412316cb3aae10bc, preserved isolated profile. Source e92409d pushed normally. No archive/runtime copy.

Owned game PID29884: title/Continue XInput observed; progressed past previous readonly depth-conversion rejection, then FAILED loading at ExecuteGuestResources guest_compute.cpp:607, Independent formatted image/buffer partial or simultaneous-write ordering is not adapted. No world, no frame capture marker, no world FPS/control/visual acceptance. XInput feeder ended with game and confirmed neutral/device removal. Small report F:/PS4Dev/handoff/depth-subresources-20261019/game-e92409d. Exact failing package/dependencies remain current; do not overwrite until required evidence is saved. Old45b failure now has reproduced narrow regression beforeFAIL/afterPASS; no need retain a full old runtime.

NEXT specific production gap: guest_compute.cpp separate_overlap/ordinary formatted reader source selection at lines585-607. Combined throw means !version OR byte_adjust; actual offending request/adjust/stage/candidate identity still missing, not established from log. Pinned BufferCache formatted sync uses TextureCache::FindImageFromRange (SafeToDownload), ForEachImageWithAddress key address>>8 followed by exact-base assertion. Do not interpret any overlapping I as a download source, remove guard blindly, copy dirty I over B, or transfer startup FPS evidence. Reuse existing GCN formatted-alias/independent-domain fixtures for exact/offset/partial readers and simultaneous-writer ordering; then one relevant game check. User1FPS remains unresolved. Continue this gap, not another full audit or repeated general profiler capture.
## Initialized readonly depth-conversion correction — 2026-10-19

Latest game45b4aa8 FAILED during Continue loading (owned PID53040). Visible title and Continue input worked; no world observed, no world FPS measured. Fatal stderr identifies CaptureIndependentImageVersion rejecting uid1485, same-base4096x4096 R32_FLOAT->D32_FLOAT: cache/gpu/alias/retention/upload/mapping all1, valid0, boundary0. No assertion suppression. Report F:/PS4Dev/handoff/depth-subresources-20261019/game-45b4aa8. Pad neutral/removed. User reports NVIDIA overlay stays about1FPS in world; unresolved. Previous797aaeb startup29.925FPS is not world acceptance.

Production correction: CaptureIndependentImageVersion now uses existing HasCurrentNativeContents (registered current native allocation, clean logical state, protected boundaries) plus independent ownership/physical mapping. Pinned TextureCache::ResolveDepthOverlap->CopyColorAndDepth does not require GpuModified for initialized contents. Do not manufacture logical_gpu_modified just to admit conversion; formatted I->B synchronization still keeps that distinction. New native_readonly_depth_conversion reproduces former exact rejection (FAIL3.92s), then PASS3.77s with actual GPU private-buffer readback, source texels plus target padding, no invented GPU write/ownership change, stale-current and dirty negatives before B capture. Related current-I/transfer/dirty-owned/dirty-cached-color-depth all PASS5/5,91.94s. Main Release compile/link PASS11.722s command-20261019-190257-56944.log. Clean committed rebuild and replacement pending. Existing installed45b4aa8 EXEc05df33ea5fc36aaa5c4c7f01a4939cc517fb5fdb56fd812bc936ca56a9c6db3/PDB971044d3d15cded82c7527fb53477464a19492e2914848a0d9ea692b78dd057f remains exact failing runtime with dependencies until replacement; no full archive.

Next: committed clean Release of readonly correction, then one controlled Continue acceptance, actual world image/control and fence-complete guest FPS. Do not repeat general logging or claim performance fixed from startup. Continue remaining shifted DSV/left-overlap and logical depth promotion gaps after this blocker; no new129-group audit.
## Shifted sampled depth views — 2026-10-19

- Upstream: ImageResource::is_depth -> promoted ImageInfo depth compatibility; MipOf/SliceOf and ResolveOverlap right branch -> keep native parent and return selected mip/layer.
- Connected DX12: sampled_depth propagation in ExecuteGuestResources; ReadonlyDepthSubresource/CompatibleGuestDepthSubview/FindDepthViewParent; full-parent ResolveDepthViewStorage; Entry::Srv remap. Current independent child takes priority. Normal-2D chance classification includes format role and complete child-extent alignment.
- Verified: actual D32 shader SRV loads/GetDimensions for shifted array layer and micro-tail mip; parent content/ownership preserved without capture/retirement; incompatible UINT and misaligned child extent use independent B; current child wins. Focused6/6 GPU tests pass in80.96s, including existing same-base PageWatch/depth-subview scenarios. New shifted fixtures use ownership adapters, not complete shifted PageWatch acceptance.
- Remaining production gaps: shifted depth DSV writes and left-overlap parent merge; do not replace these with snapshot success. New shifted MSAA/D16/paired-S scope needs independent proof; paired formats must retain original compatibility classes. Game/control/visual/performance parity remains OPEN. Evidence F:/PS4Dev/handoff/depth-subresources-20261019.

## Runtime integration delta — 2026-10-19

- Pinned TextureCache::ResolveOverlap / ImageInfo::MipOf -> HasPossibleReferenceSubresourceBase + GuestTextureCache::UploadVersion (7f32403): provably unaligned unequal-base chance overlaps preserve independent I and initialize the separate image from current B. Connected; four depth/color left/right GPU readback cases and related transfer/recreation cases pass. Real slice-boundary mapping remains explicitly rejected; full PageWatch/padding/game proof is not implied by these adapters.
- Pinned AJM BatchCancel/BatchWait cancellation contract -> AjmBatch::Execute + AjmContext::WorkerThread (797aaeb): canceled dequeued batches release completion so the existing API can return CANCELLED. Connected; focused production-executor/MPSC completion regression fails before and passes after, including no codec execution, result visibility and exception propagation. Full API/codec acceptance and causal link to user8068 are OPEN; this closes no graphics parity group by itself.
- Exact clean797aaeb EXE2c7d6aca startup-only frame capture:1701 new GPU-complete flips/56.841809s, invalid0, exit0. World/control/image/performance parity remains OPEN because window observation failed. Sole installed core is this exact experimental hash; do not transfer earlier main607f world/input evidence to it.

# Implementation and verification registry — 2026-10-17

## Registry delta - BC prefix alias transactions - 2026-10-19

Connected and focused GPU verified: pinned `ResolveOverlap` / `ExpandImage` BC-parent semantics -> `BlockViewGroup` planning in `PrepareOrderedGuestStages`, transaction-local dependent children in `GuestTextureCache::UploadVersion`, and mip-overlap-aware SRV/UAV promotion in `resource_patching_pass.cpp` -> all 27 writable BC variants plus the full ordered-formatted related suite pass. This closes the previous exact, multilevel and shifted/rebased prefix items, including duplicate-slot, native-reader and cross-transaction lifetime cases. Game/final-suite behavior is open.

Concrete gaps remain: pinned MSAA, clipped and large BC block domains -> no accepted DX12 end-to-end case yet -> require separate original-applicable descriptors and real GPU readback proving all samples/contents, padding, visibility, ordering, ownership and lifetime. The existing broad `native_resources` `invalid map<K, T> key` failure remains a separate unresolved fixture failure.


## Registry delta - multilevel BC/word prefix - 2026-10-19

Connected and focused GPU verified: pinned BC parent / terminal block-word prefix preservation -> `MatchesReadonlyBlockMipPrefix` admission through `CompatibleGuestBlockWordStorage` -> ordered early full-parent I and late current-B UINT prefix with padding and untouched tail. Exact and multilevel focused checks plus the complete ordered-stage matrix pass; whole-game/final-suite behavior is open.

Concrete gap remains: pinned shifted/rebased block mip/layer view -> `RebaseBlockSubresource` and writable parent alias code exist, but the full ordered multi-stage GCN path is not yet accepted by a focused contents/commit oracle. MSAA, clipped and large-domain variants remain separate.



## Registry delta - ordered BC/word domain - 2026-10-19

Connected and focused GPU verified: pinned `TextureCache::ResolveOverlap` / `ExpandImage` one-level BC-parent/block-word contract -> `CompatibleGuestBlockWordStorage` and converted-view handling in `PrepareOrderedGuestStages` -> separate native BC sampled I and UINT writable B allocations, original bind order, guest padding, ownership and lifetime verified by `native_ordered_bc_word_stage_bindings` and the full ordered-stage matrix.

Concrete gap remains: pinned multilevel BC parent / word-prefix preservation -> no DX12 ordered domain admission yet -> must retain untouched parent mips/layers while materializing and writing only the selected word range. Shifted address, MSAA, clipped and large-domain variants remain separate explicit gaps.



## Current ordered converted packed domains - 2026-10-19

Production **a082b64cfd67f52b692163d9762ece23d67b025c** connects same-address packed image domains that require distinct DX12 sampled and writable allocations. Implementation state: connected for `CompatibleGuestPackedStorage`, including deferred actual-bind conversion and fence-pinned earlier descriptors. Behavior state: focused GPU verified with deliberately different original-I and current-B packed10 SNORM contents, full guest padding, the existing ordered-stage matrix, independent transfer and signed packed10 regressions. Whole-game/final-suite behavior is open. `formatted-alias-ordering` remains `connected_with_known_gaps`: BC/word views, shifted/clipped/MSAA/different-geometry and large domains remain concrete production gaps.

## Current per-sample ordered color - 2026-10-19

Production **e132726c97fa794e9e53e4cc8c9d80866b47ebf2** connects per-sample ordered attachment storage and writeback for packed-SNORM blending/logic and RGBA8 independent logic. Behavior is focused GPU verified by 104 packed-SNORM and 136 RGBA8 actual GCN draws with complete sample contents and padding. Clean Release repeated both. Whole-game/final-suite behavior remains open. The signed-packed10-attachment production implementation has no remaining sample1/2/4/8 code gap; final game acceptance remains separate. Packed19 MSAA remains connected on a pinned-original-capable adapter. Continue formatted-alias-ordering late source envelope reservation.


## Current RGBA8 independent MRT logic - 2026-10-19

Production **48c9795b6901e94087b1b795ccfeb43bb08e6563** connects single-sample `RGBA8_UNORM` ordered raw-word logic and preserves each attachment's actual array-layer view. Behavior is narrowly GPU verified by 68 real GCN draws covering all 17 represented operations, independent masks `0x1/0xA/0xF`, two layer orders and full 64 KiB contents/padding. Packed-SNORM and shared signed regressions pass. Whole-game/final-suite behavior remains open. The `signed-packed10-attachment` production gap is narrowed to per-sample packed-SNORM MSAA ordered blend/logic; single-sample quantization, blending and logic are connected and focused-verified. Other formats without a legal raw-word cast remain explicit independent-logic gaps.


## Current storage correction / one current core - 2026-10-18

This FIRST section supersedes every older instruction to keep intermediate DX12 versions/manual artifact folders. Read [STORAGE_POLICY.md](STORAGE_POLICY.md) and current root AGENTS.md. Full port remains ACTIVE / INCOMPLETE; production core aa503d0, source history preserved in Git. Application selection explicitly migrated to **C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current**, same EXE SHA256 **7e8fca7a1bd828c19629fdd9299155aead41bc13fb285792532905e5b1c1f5a0**, matching PDB/runtime/licenses and v2 source/build manifest. ProfileId external-7e8fca7a1bd828c19629fdd9 preserves the existing isolated profile. Installed app finds exactly ONE core and validates19 files. No core/game started in this work; previous launch-manifest remains factual historical evidence. Exact core runtime/game/full suite NOT_RUN; prior414GCN/18native/reload acceptance remains scoped to original fixture identities.

Cleanup completed:3310 obsolete loose PE/PDB/MAP/control outputs and26 legacy installed packages removed with hashes, scope/reparse/active-process checks and retained unique source inputs. H=F:/PS4Dev/handoff/storage-cleanup-20261018 contains deletion manifests/journals, FINAL_STORAGE_PROOF.json and inventory-after-final.json. Net disk free increase C23.03GiB/F42.16GiB/D6.65GiB; checkpoint free C27.99GiB/F42.63GiB/D164.93GiB. Handoff logical data now about16.98GiB across F/C/D, primarily protected diagnostics/reference/profile/source/reports. Known physical D archive binaries retired directly; old C junctions were not followed. Games, saves/profiles, pinned original/toolchain/dependencies, current active trees and unknown .serena/CLAUDE.md preserved. d019 exact-runtime/original exception/dump/logs/input remain because the original individual rejected range is still not established; fixed development runtime copies retired.

Storage implementation: desktop/package-core.ps1 publishes only stable dx12-current, stages/validates replacement, refuses active/locked/unknown old files, shares a publication/launch mutex and removes temporary/previous runtime on success. CoreVersions.ProfileId preserves profile identity across replacement; no stale acceptance is copied. tools/porting/publish-current-core.ps1 packages from the existing Release build, validates identities, includes licenses and can explicitly select the current core. Reuse native-port/run-gcn-frontend-test.ps1 / native CTest in existing build trees with codex-dev logs and small reports. DO NOT reuse archived run-gcn.py/save-release.py full-copy helpers. Packaging safety fixture and desktop core/profile/gate/external/diagnostic regressions PASS; desktop Release0 warnings/errors; updated installed app metadata checks PASS.

Next specific port work and recovery prompt: FIRST SESSION_HANDOFF.md. Whole129-group implementation/behavior statuses remain unchanged.


## Current robust sample copy / mixed samples checkpoint - 2026-10-18

Authority: FIRST [SESSION_HANDOFF.md](SESSION_HANDOFF.md), production **aa503d01ffde04363465eff368edb3865e022ec3**. H=**D:/Codex-Handoff/ps4-dx12/mixed-samples-20261018**; VERIFICATION_FINAL.json preserves sources and exact run identities. Full port ACTIVE / INCOMPLETE;129 implementation/behavior records and8 broad gaps remain independent. Host per-index robust sample defaults and RGB32 physical alpha/cache specialization connected and narrowly GPU verified;16 mixed1/2/4/8 draw combinations and12 robust copies plus no-UAV negative accepted.414 GCN cases,18 native fixtures160.812s and shader reload5.952s PASS. Old fb738a6 content control fails required alpha1. Failed fixture/oracle corrections and all logs preserved, no arbitrary expectation changes. Manual clean Release H/artifacts-aa503d0 (18 files/54 pins/2283 inputs/zero primary sources),21.937s; EXE7e8fca7a1bd828c19629fdd9299155aead41bc13fb285792532905e5b1c1f5a0. This rebuilt core runtime/game/FPS NOT_RUN. Full448 suite/extended XInput/game remain final; no automatic install/select. Next specific existing gap: signed packed10 SNORM attachment number1; number5 SINT already connected. See H/NEXT_MECHANISM.md. Five closed C archives moved losslessly toD with original C paths retained via verified junctions (ARCHIVE_RELOCATION.json); no old results lost. Unknown .serena/ and CLAUDE.md untouched.


## Current inactive sample history pressure checkpoint - 2026-10-18

Current authority: [SESSION_HANDOFF.md](SESSION_HANDOFF.md), FIRST section. Production **fb738a62d96a1b0364422a2c0dc8a3be580b9d72**, fix/mixed-cmask; full original port **ACTIVE / INCOMPLETE**,129 independent implementation/behavior records and8 broad gaps remain. H=**C:/Users/skei/Documents/Codex/handoff/sample-backing-pressure-20261018**; H/VERIFICATION_FINAL.json retains exact identities. ReleaseRetiredStorage no longer erases inactive sample_backings as scratch: copy=false before a partial copy observes their uncovered old pixels. History stays in residency until correct whole-image eviction; other scratch/plane/whole-image reclamation remains. This preserves correctness; no FPS improvement claimed.

**native17 PASS147.598s**, incl original1/alternate4 and original8/alternate1,2 layers/all samples, completed-pressure with allocation.use_count==1, honest failed zero-budget reclaim, copy=false + actual one-pixel GPU shader copy, independent I/B and full padding. Same fixture header against old clean8f2338e fails both premature-retirement assertions BEFORE partial readback; no old pixel-corruption observation is fabricated. **GCN398 PASS**, EXE **d393f5711e73f76b8dc08e0063802f27fbb10bed5a71d5d6658ce463a991f1ff**; shader reload PASS5.715s. All4 committed source paths match native/GCN/reload snapshots. Isolated native ownership callback is synthetic; pressure+real PageWatch/game acceptance is not newly claimed.

Clean Release **H/artifacts-fb738a6**,18 manual files;54 pinned dependencies/2283 compile inputs/ZERO primary paths in compile DB/build.ninja/cache,18.718s. EXE **74f0336d950be398312cfd3c0135f596c94f458ed8609233d81cc7740649638b**; PDB **a71db632df25aaa5b0e26d33c1aac6b725d8731b8c0f9ef1ece8de37648fc040**. Exact rebuilt core runtime/game/FPS **NOT_RUN**. One new CTest raises446->447; full447/extended existing-XInput acceptance stays final. No automatic install/select, original Vulkan/save/controller/HUD/monitor/user-process changes. Older kernels/evidence remain; lossless owned-output/object/archive compression verified hashes, no deletion.

Scope correction from exact pinned fields: CB/DB stored fragments and normal graphics PSO counts are1/2/4/8; AA coverage can16 separately. T#16 is still unaccepted, but normal16 attachment/PSO work was not established by this register path. Next: actual mixed color/depth1/2/4/8 Draw/content/backing transitions, then existing queue; H/NEXT_MECHANISM.md gives exact fixture setup/symbols. Keep T#16 payload/view/transfer open; no silent count drop or full MSAA closure. No129 audit/repeated query/fixed crash replay/unchanged full suite. Older sections below are historical.


## Current requested/native sample backing checkpoint - 2026-10-18

Current authority: [SESSION_HANDOFF.md](SESSION_HANDOFF.md), FIRST section. Production **8f2338e1aa2a2fcaafe5386e01ee71e443f9363e**, fix/mixed-cmask; full original port **ACTIVE / INCOMPLETE**,129 independent implementation/behavior records and8 broad gaps remain. H=**C:/Users/skei/Documents/Codex/handoff/requested-sample-backings-20261018**, H/VERIFICATION_FINAL.json retains precise source/runtime identities. Requested alternate backing identity is separate from physical native count; original1/8 -> requested16/native8 -> original preserves the same original allocation/class/UID and every required original sample. Supported readonly1 images now retain real RTV capability for the return blit. Staged publication, owning-cache pin, actual geometry checks and refreshed recording context preserve failures/lifetime.

**398 GCN regressions** on EXE **101294c55653576d87cb67d8a98483b3136a254728a21417cfe213cc1b902ba9** PASS; **native16 PASS136.628s**, including2 new original1/8 backing scenarios with2 layers, actual GPU donor copy, all sample words/padding, copy/no-copy, forced Finish/resume and real negative admission. Shader reload PASS6.121s. Identical minimal old/new source: oldbcc4bac fails1->4->1 (missing RTV) and requested16 (unsupported count); new production passes both. This is a headless regression control, separate from the saved d019 user crash. Native16's8 changed component snapshots match final; later ninth source change is Draw's physical-count/PSO rejection guard, compiled in final GCN/core but branch not newly runtime exercised. Fixture and rebuilt-core acceptance are distinct.

Clean Release **H/artifacts-8f2338e**,18 manual files;54 pinned dependencies/2283 compile inputs/ZERO primary checkout paths in compile DB/build.ninja/cache,22.044s. EXE **d9588b41b6742040a9c5a6343cc5d3891d3f16e194259a0bc14ec9abe9287c57**; PDB **5ece06de1285e368310852c5f87cb39bc3c9c073b360198e387229bb9e900024**. Exact rebuilt core runtime/game/FPS **NOT_RUN**. Full suite now446 (one new focused CTest); full446/extended existing-XInput acceptance remains final. No auto-install/select, original Vulkan/save/user-process/controller/HUD/monitor changes.

Next: original guest16 payload upload/view/sample-index/readback and common physical graphics PSO mapping; inactive copy=false backing contents under pressure remain unaccepted. Do not simply lower native count or weaken guards; H/NEXT_MECHANISM.md and REFERENCE_BACKING_CONTRACT.json give exact symbols and saved limits. Generic fallback is NOT closed. No129 re-audit, repeated API query, fixed crash replay or unchanged full suite. Older sections below are historical.


## Current readonly MSAA writable-plane checkpoint - 2026-10-18

Authoritative current state: [SESSION_HANDOFF.md](SESSION_HANDOFF.md), FIRST section. Production **bcc4bace8e85f31bc0aa921a80008496101d8f40**, fix/mixed-cmask; full original port **ACTIVE / INCOMPLETE**,129 independent implementation/behavior records and8 broad work items remain. Original readonly raw-R9 MSAA I now supports actual R32/packed writer aliases via validated single-sample UAV planes without changing original desc/class/UID. Implementation connected and narrow behavior verified are distinct from whole-group/final acceptance.

H=**C:/Users/skei/Documents/Codex/handoff/msaa-word-writer-20261018**. H/VERIFICATION_FINAL.json: **398** actual GCN cases on EXE **a30f855a56b4318986637e57831e15d107ae6c467869b1c5ca04f799b35fbc32** (60 new R9 MSAA writers,112 R9 sample loads,64 signed packed MSAA aliases,162 prior regressions); native15 PASS128.013s; shader reload PASS5.760s. Real readback, all2/4/8 samples, both descriptor orders, GpuModified0/1, distinct I/B, full65536 padding, actual plane permission/cast negatives, two-layer clears, pending/fence/pin/allocation/transfer retention and retirement/restore. CDB preserves60 original UID/class34/7/readonly/raw,740 OwnIndependentImage and0 ResolveImage. Identical fixture against old15b1c88 fails explicit independent-ownership contract, preserved separately from historical game crash.

Clean Release **H/artifacts-bcc4bac**,18 manual files,54 pinned dependencies,2283 compile inputs/ZERO primary checkout paths (compile DB/build.ninja/cache),22.718s. EXE **48c2806f4551cf29f7d966c82a73b4fa7ac1f3d045aa33cd2f91a8de8b97f7bd**; PDB **2064693cd71c21265209e40cf52b57d5705e05eb17e5baa71aef04b01778caee**. Exact rebuilt core runtime/game/FPS **NOT_RUN**; no acceptance transfer from fixture EXEs. No automatic app install/select, original Vulkan/save/controller/HUD/monitor/user-process changes. Full445/game/XInput acceptance stays final; existing pressure test extended, total445 unchanged.

Next: generic unsupported-count fallback and precise logical/native backing/view/shader/PSO mapping; H/NEXT_MECHANISM.md contains concrete symbols and saved reference locations. Readonly MSAA writer-plane work above is completed within its stated scope. Generic fallback, converted/shifted/multiview aliases and broad renderer gaps remain open. Do not repeat129 audit, adapter queries, fixed d019 replay or unchanged full suite. Older sections below are historical.


## Current readonly R9 MSAA word-sampling checkpoint - 2026-10-18

Authoritative current source: [SESSION_HANDOFF.md](SESSION_HANDOFF.md), FIRST section. Production **15b1c8829e00f35a1d750da98e39b9a7cf628322**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**,129 independent implementation/behavior records and8 broad work items remain. Explicit raw32 readonly R9 MSAA sampling is connected/narrowly GPU verified; all requested2/4/8 samples and original logical format/access/tiling are preserved. Generic unsupported-count halving and readonly MSAA writer-alias ownership remain open, not accepted by this unit.

H=C:/Users/skei/Documents/Codex/handoff/native-sample-fallback-20261018; P=F:/PS4Dev/handoff/native-sample-fallback-20261018/packed-float8. H/VERIFICATION_FINAL.json:274 queued GCN cases incl112 new;8 packed-float raw transport cases; native14 PASS105.480s; shader reload PASS6.197s; identical final GCN fixture fails on old02a04f2 capability gate. Required source/runtime identities retained. No new game/FPS evidence. Real R9 typed IMAGE_WRITE/atomics are not newly admitted.

Clean Release H/artifacts-15b1c88,18 manual files;54 dependencies/2283 inputs/ZERO primary paths,22.657s. EXE 910a15112274a53713dd8de306b4acb4b82c2937dbf9b10cb4403e751d17079c; PDB 31cc3750176d1006b69c9eda2b8a4aeb97d2788d21eb3de9816cc6fdeae94f01. Exact rebuilt core runtime/game/FPS NOT_RUN. No automatic app install/select or original Vulkan/saves/user-process changes. Previous evidence/builds retained; owned immutable/closed-output lossless compression verified byte hashes.

Next: Preserve an ORIGINAL readonly raw-R9 MSAA I when an actual R32 MSAA writer view arrives. Adapt MatchesGuestUploadStorage, PrepareMultisampleStorage/EnsureMultisampleStorage and Entry::Uav/guest_compute per-plane bindings to validate the writer VIEW and the actual single-sample UAV plane, without setting entry.desc.writable. Retain original class/UID/GpuModified, independent B and pressure/lifetime behavior. Generic original unsupported-count halving/backing/view mapping remains a separate open contract. H/NEXT_MECHANISM.md and REFERENCE_SAMPLE_CONTRACT.json preserve exact symbols/source evidence. No129 audit, repeated adapter query, fixed crash replay or unchanged full suite. Full445/game/XInput acceptance stays final. Older sections below are historical.


## Current shared-exponent / physical buffer envelope checkpoint - 2026-10-18

Authoritative current state: [SESSION_HANDOFF.md](SESSION_HANDOFF.md), FIRST section. Source **02a04f2ebc8630d83bcbc5864de137eae82483b2**, WIP **60bfc338de5960d513d3d60508dd9e0758c14b83**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**. Direct data34 read/sample aliases, real mutable-word backing and ordinary readonly retention are connected/narrowly verified. Original GpuModified gating and physical buffer-envelope backing are fixed; original class/UID and B/I separation are GPU/CDB verified.129 independent implementation/behavior records and8 broad work items remain.

H=C:/Users/skei/Documents/Codex/handoff/shared-exp-word-20261018; B=F:/PS4Dev/handoff/shared-exp-word-20261018. H/VERIFICATION_FINAL.json verifies15 source paths and exact identities. Final standalone GCN162 PASS, native14 PASS105.270s, shader reload PASS5.658s; old controls4828eae and60bfc33 fail as expected with final fixture sources. All failures retained. No new guest FPS/game acceptance.

Clean Release H/artifacts-02a04f2,18 manual files;54 pinned dependencies/2283 inputs/ZERO primary paths,18.334s. EXE f3ba36c5375354da7c0e29f071236c1c1f7a225a7ec071aeb96e813af5a94af7; PDB b8f004fbe7c5df4395a536f8eb5056fc5de3e88cebaa25c0ed4fd8cf798a339b. Exact rebuilt core runtime/game/FPS NOT_RUN; graphics inter-stage data34 compiled only. No auto-install/select, game/UI/controller/user-process/original Vulkan/save changes.

Next: Port original per-adapter native sample fallback and its logical guest/native backing/view mapping, starting at GuestTextureCache::UploadVersion and GuestMultisample/Entry views. Use the saved same-LUID Vulkan query and pinned Image::Image / LiverpoolToVK::NumSamples; do not invent native4x RGB9E5 support or discard guest samples. Saved API evidence in F:/PS4Dev/handoff/cached-r11-word-20261018/reference-format-query. Keep logical guest geometry, original binding order, contents/history, PageWatch/barriers/fences/lifetime. Converted/shifted/MS and existing broad8 gaps remain explicit. No129 audit, fixed crash replay or unchanged full suite. See H/NEXT_MECHANISM.md. Older sections below are historical.


## Current R11 mutable-word / pre-title fatal cleanup checkpoint - 2026-10-18

Latest production **4828eaebf4cd69c9343e82f0415d1817239dae3b** includes R11 port **510a6d7547a86ebe29081d3f921c8c2adac94845**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. This FIRST section supersedes older current-state/next-task sections. H=**C:/Users/skei/Documents/Codex/handoff/cached-r11-word-20261018** (metadata, GCN archives, latest manual kernel); B=**F:/PS4Dev/handoff/cached-r11-word-20261018** (bulk native/negative/CDB archives). H/VERIFICATION_FINAL.json verifies exact hashes and scopes; H/LOCATIONS.json records storage split. Previous goal turn made verified production/checkpoint progress.

**Upstream contract -> R11 production:** pinned host_compatibility.cpp assigns B10G11R11 Ufloat to _32BIT mutable storage. NativeImageStorageAliases, resource patching and graphics pipeline view context now include data6. CompatibleGuestWritableStorage normalizes comparison copies only; actual R11 FLOAT I/class/codec/history stay intact. WritableWordCasts includes R11 typed UAV and raw R32 views on the same actual format-list resource. A once-per-cache real FORMAT_SUPPORT query admits R11 only when typed UAV view/load/store are available, preserving other word allocations on adapters without this optional format. Existing actual resource cast/permission guards, enhanced barriers, leases, fences, PageWatch and original binding order remain. Native cache key is native-dxil-cache-20261018-mutable-float-word-uav-v5. Ordinary typed R11 load/store semantics are retained; no address/game/hash workaround, feature disable or fake operation was added.

**Actual verification, identities kept separate:** port-stage GCN EXE **b8e80e57aa943ab69c4d5df52a175c976e7be9b2de2445915a11e310bc006068** passes146 cases:16 NEW bidirectional R11/packed SNORM/SINT (both descriptor orders, clean/modified I),40 RG16,24 ordinary R32,20 readonly mutable32 and46 ordered-buffer regressions. Independent authored R11 words/finite colors, changed stores, real S_BARRIER, typed/packed before/after loads, separate B and full65536 padding oracle pass unchanged.14 related native tests PASS112.043s. R11 raw transport4 cases PASS12.656s (tiles8/13, samples1/4, mips/layers/changed backing/padding/nonfinite word patterns). After cleanup commit, NEW GCN EXE **3fb85af09a4e6103a22e5d8324afde037a8419268bb784adee3e3f19376b46e6** separately passes16 R11 cases. Cleanup poisoned-storage CPU fixture passes before ElfInfo and with synthetic ElfInfo, repeated Shutdown without SDL/game/play-time writes; exact final synthetic fixture on olde9 production exitsc0000005. Latest core-object shader reload PASS5.765s,2 PSOs/changed contents/rollback/restore/metadata/same-version persistent preload. No acceptance transferred between these binaries. All9 port paths and13 latest source paths match relevant archived sources after LF normalization. Graphics inter-stage R11 context connected/compiled only; typed R11 shader special-value arithmetic not newly accepted (raw-bit transport does include nonfinite patterns).

**Original exception chain and production cleanup fix:** exact corrected old R11 fixture rejects incompatible storage in ExecuteGuestBufferCompute, first C++ e06d7363 context captured. Rethrow reaches Liverpool unhandled_exception/assertion cleanup; Core::Emulator::Shutdown then reads an uninitialized controllers pointer, producing a SECONDARY AV in ResetLightbarColors. B/old-exception-context-final contains original-cpp.dmp, application.dmp, original registers/thread/stacks, exact exe/PDB/runtime identities. This is the headless negative-control failure, DISTINCT from the already captured/fixed d019 user-game crash. src/emulator.h now initializes controllers to null. Shutdown avoids GameSerial lookup before a play-time interval starts, matching UpdatePlayTime's existing no-op contract; this also prevents an early ElfInfo assertion/recursive cleanup. Real initialized controller cleanup remains. Assertion/exception delivery and diagnostic flushes remain enabled. native-port/emulator_shutdown_test.cpp is reproducible through run-gcn-frontend-test.ps1 -Fixture emulator_shutdown; it constructs over poisoned bytes and checks both no-title states. Initial incomplete debugger trace stopped at a handled PageWatch AV, not the real crash; corrected filters deliver first-chance AV normally and capture C++/second chance. An intermediate dump-path escaping error was corrected; final two dumps exist and are hashed.

**Important remaining sample contract, not hidden:** default packed_float_test8 FAILED. Explicit r9-ms reproduces data34/tile8/samples4 rejection (color RT unsupported). Exact same-LUID API query using pinned ImageUsageFlags + Mutable/ExtendedUsage (with/without optional feedback) on RTX5080: Vulkan format122 samples0xf, format123 samples1; AMD integrated both0xf. Pinned Image::Image and LiverpoolToVK::NumSamples halve unsupported counts to1. Native4x R9 is therefore not a proved original requirement on NVIDIA, while original native-sample fallback/guest mapping remains an explicit unported/unverified path. Do not classify R11 as failed or pretend all packed-float tests passed; four explicit R11 cases passed. Do not simply disable MSAA or replace semantics to turn the failure green. Saved reference-format-query source/executable/loader hashes, device IDs/LUID and output are the continuation evidence.

**Reproducible latest Release:** H/artifacts-4828eae,18 manual files; clean detached committed source,54 pinned dependencies/2283 compile inputs/ZERO primary checkout inputs,32.478s. EXE **eb8f97f9fc298585db3ce4894dad988f11e849757388bfb47b55c77152e7d739**; PDB **6c0d8bb03438174d27c8fd2d82903e8027a03086be1ae6df37f7665aa0627b34**. Exact rebuilt CORE runtime/game/FPS **NOT_RUN**; standalone proofs remain separate. No automatic app installation/selection, no game/UI/XInput/profiler/user-process/original Vulkan/save changes; old kernels retained. Unknown .serena/ and CLAUDE.md preserved.

**Recovery/provenance:** copied local orchestrator initially used an old handoff root; two new completed R11 archives were moved with every hash checked, then remaining cases resumed. Existing authoritative old binaries/PDB/identity/source/output/raw logs stayed intact; one console mirror was reconstructed and explicitly labeled, not claimed byte-original. New archive copy ran out of space BEFORE launch; incomplete copies are not accepted. No old result was deleted. NTFS lossless compression on owned immutable archives and closed project build outputs recovered space, then every accepted file hash was verified. C/F free space is recorded at final checkpoint; avoid more uncompressed bulk copies or unnecessary builds. H/RUNNER_RECOVERY.json and DISK_RECOVERY.json retain details. Old early fixture missing SDL include/ElfInfo was corrected; only final identical synthetic old-negative and two latest positive fixtures support cleanup acceptance.

**Next concrete production work:** H/NEXT_MECHANISM.md. data34 R9G9B9E5 readonly/sampled view aliasing a real R32/packed writer: current storage alias classification/casts still exclude34; preserve exact shared-exponent decode/filter, I/B/class/history and actual capability proof. No ordinary typed R9 UAV on NVIDIA; do not advertise unsupported views. Also port original NumSamples native fallback and corresponding guest-sample/backing/view mapping where applicable, using saved per-adapter query rather than inventing native4x support. Keep converted/shifted/geometry/MS and broad8 gaps explicit. No129 audit, fixed d019 game replay, repeated incomplete debugger experiment or unchanged broad suite. Full445 suite and final existing-XInput image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final.

**NEXT CODEX SESSION — START HERE:** Read this FIRST SESSION_HANDOFF section and H/VERIFICATION_FINAL.json; verify HEAD/upstream and disk space. Continue data34 shared-exponent readonly aliases with R32/packed writer and the original per-adapter native sample fallback contract, preserving actual I/B contents, original class/history, barriers/fences/ownership and guest geometry. Use saved Vulkan/DX12 evidence and reference-format-query; no129 re-audit or fixed crash replay. Build/run narrow finished-mechanism checks with exact identities, then clean-checkout Release/checkpoint/normal push and manual kernel folders. Early cleanup now fixed/tested; do not repeat old fake first-chance PageWatch crash or missing-Elf fixture. Final full suite/game acceptance still required. Goal remains incomplete.


## Current mutable R16G16 / raw word resource checkpoint - 2026-10-18

Production source **e9d12e151802612847742313d6c6c7be0d62259f**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**,129 implementation/behavior records and8 broad work items remain. This FIRST section supersedes older next-task/current-state sections. H=**C:/Users/skei/Documents/Codex/handoff/cached-rg16-word-20261018**; H/VERIFICATION_FINAL.json holds exact identities, sources and logs. Previous goal turn made verified production/checkpoint progress; original Vulkan and all older evidence remain unchanged.

**Contract -> production implementation:** pinned host_compatibility.cpp explicitly puts R16G16 UNORM/SNORM/UINT/SINT/FLOAT and packed10/R32 in _32BIT mutable storage. Olda89 production rejects actual same-storage GCN operands as different layouts. NativeImageStorageAliases and packed alias classification now include typed data5; graphics pipeline view-context collection includes5 as well. GuestTextureCache::WritableWordCasts creates actual format-list resources containing typed R16G16 and raw R32 UAV views (plus existing direct word views). Allocation stays fully typed when using CreatePlacedResource2; enhanced barriers, state tracking, allocation leases and fences remain. CompatibleGuestWritableStorage normalizes comparison copies only; original numeric descriptors/class/codec flags and I/B authority remain. Entry::Srv/Uav require actual compatible native format/list proof for cross-format views, and Uav rejects a real resource lacking ALLOW_UNORDERED_ACCESS. No descriptor equality alone grants a cast or write permission. On adapters without format-list support existing same-family paths remain; new unsupported cross-family views fail explicitly. Native cache namespace is native-dxil-cache-20261018-mutable-word-uav-v4.

**Exact completed verification:** GCN EXE **78b03be232d3cd9b8a7b49dc964c88722b6ac4aa74691203f44e5bc225528f9f** passes130 actual queued cases:40 new cached R16G16 (5 numeric classes x packed SNORM/SINT x2 descriptor orders xGpuModified0/1),24 ordinary R32 UINT/SINT/FLOAT regressions,20 readonly mutable32 and46 ordered-buffer regressions. RG16 modes4.650/3.944/3.942/4.109/3.990s. Real seed image loads/stores, changed typed stores, S_BARRIER, packed before/after numeric channels, independent raw B and whole65536 physical/padding oracle remain. All8 committed paths match archived final GCN/native sources after LF normalization. The IDENTICAL corrected final fixture against clean committeda89 production still fails with the explicit unsupported-storage exception; exe/PDB/map/rsp/runtime/source identities are retained. This fixture breakpoint is distinct from the already captured/fixed d019 user crash.

**Ownership and native checks:** exact accepted GCN float EXE CDB trace has12 OwnIndependentImage entries, zero ResolveImage entries;8 ordinary data5/number7 metadata entries retain flags0 and stable UID per seed/write pair.14 final native regressions PASS114.592s: actual allocation/cast/UAV-permission positives/negatives, independent image transfer, buffer envelope/view lifetime/GPU-chain, DMA/preserved versions/pending fence/scheduler, packed/raw word writers, readonly mutable32/word views and access classification. Shader reload GPU fixture PASS5.101s:2 PSOs, changed contents, rollback/restore, metadata negatives and same-version persistent pipeline preload. New cross-graphics-stage data5 context is connected/compiled, not newly GPU accepted; compute cases do not prove full graphics/game parity. No cross-version cache-hit runtime claim.

**Preserved failures / oracle provenance:** initial new RG16 probe wrongly assumed alpha redirects to green. Pinned pixel_format.h::RemapSwizzle only redirects single-channel8/16/32 alpha to red; RG16 takes default, native missing alpha is one. Only that new probe expectation was corrected, with source excerpt/provenance; final old-production control still fails. Independent OldWord/SeedWord/NewWord and packed expected values were not changed to fit production. Initial native set passed13 and failed an OUTDATED ordinary R32 FLOAT rejection (already supported at a89); that case is now positive alongside SINT and RG16. Eight geometry/access/representation negatives remain, with stencil-aspect replacing supported FLOAT; real missing-cast and missing-UAV-permission negatives were added. Final14 rerun was required after frontend/test changes, not a full445-suite repeat. New fixture compilation initially missed required mip0 and the local runner had a prelaunch syntax typo; failed outputs retained. No failure hidden.

**Clean committed Release:** H/artifacts-e9d12e1,18 manual files; clean detached checkout,54 pinned dependencies/2283 compile inputs/ZERO primary-checkout inputs,29.731s. EXE **1d6023253b979d434e9510f2f0ae9aa7109530a4441667f59b91eae33612b6d9**; PDB **ea53c6d88521ddb59061717eb1355e06c6c66ce7d710828d8d842f1e1862eba8**. This exact rebuilt core runtime/game/FPS **NOT_RUN**; standalone fixture acceptance is separate. No automatic application installation/selection; previous kernels retained. No game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved.

**Next concrete mechanism:** H/NEXT_MECHANISM.md. Ordinary R11G11B10_FLOAT (data6) donor/views in original _32BIT class: current NativeImageStorageAliases/cache writable comparison/casts admit4/5/9/10 only; readonly6 support does not establish writable semantics. Extend actual legal resource casts plus shader representation/alias emission/specialization and independent before/after numeric/bit I/B/ownership/readback oracles. R9G9B9E5 (data34) lacks an ordinary typed UAV store and needs its own codec path; never advertise an unsupported UAV. Keep converted/shifted/MS/geometry and broad8 gaps explicit. Nested disjoint/revalidation/BC GCN fixtures still need real PM4 callbacks instead of off-queue GetCsRegs before running those cases. No129 audit/fixed-crash replay/unchanged broad suite.

**NEXT CODEX SESSION — START HERE:** Read this FIRST SESSION_HANDOFF section and H/VERIFICATION_FINAL.json; verify HEAD/upstream. Continue data6 R11G11B10 mutable writable word storage with actual resource format support, correct shader numeric/bit codecs and preserved original I/B/class/history, using existing evidence and a real queued GCN regression/old-production control. Data34 remains a separate explicit codec gap. Build and run narrow completed-mechanism GPU/regression checks, retain exact EXE/PDB/runtime identities, clean-checkout Release/checkpoint/normal push and manual kernel folders. Do not redo129 audit or fixed crash/game diagnosis. Keep full suite and final existing-XInput game image/input/stability/completed FPS excluding FG versus saved Vulkan baseline for final acceptance. Goal remains incomplete.


## Current ordinary R32 signed/float writable donor checkpoint - 2026-10-18

Production source **a89af96baeef956f2c6578975b9c29694d9de500**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**. This FIRST section supersedes older current-state/next-task sections;129 implementation/behavior records and8 broad work items remain. H=**C:/Users/skei/Documents/Codex/handoff/cached-r32-numeric-20261018**; H/VERIFICATION_FINAL.json contains exact identities/sources/results. Prior703f262 coherent UAV/current-B image-refresh evidence remains in ordered-stream-20261018; its82-case acceptance belongs to its own EXE, separately from the new90 cases below. Existing d019 crash was captured/fixed at935fb69; do not infer a new crash cause or replay game from fixture failures.

**Upstream contract -> implementation:** original _32BIT mutable storage and BindResources preserve I contents across numeric views, independent physical B and original image history. CompatibleGuestWritableStorage previously treated an ordinary cached R32 UINT donor as exact raw word storage but excluded ordinary SINT/FLOAT, despite their existing R32 typeless allocations/typed views. Both new real-GCN negative runs against committed201d8af (=703f262 production) fail the first modified-seed packed10 before-load, reading old bits. Production now recognizes ordinary data4/number4,5,7 in LOCAL comparison copies only. Actual descriptors, numeric formats, codec flags, resource/view types, PageWatch ownership transfer, barriers, lifetime and dirty ranges are unchanged. No format conversion, fake success, draw skip or game/address/hash exception was added.

**Executed GPU proof:** EXE **2929b3cdf3ad2f55981212f4311274b5d58b56885202322a17017798090392fc** passes8 cached SINT cases4.685s,8 cached FLOAT cases3.937s,8 UINT regression cases4.069s,20 readonly mutable32 cases4.139s and46 queued ordered cases5.543s:90 actual GCN cases on this exact EXE. Original seed OldWord/SeedWord/NewWord and independent packed SNORM/SINT expected channels are unchanged, both descriptor orders/GpuModified0/1, real image_load/store/S_BARRIER, raw B separation and all65536 physical bytes checked. R32 alpha is remapped to red by pinned swizzle; float words are finite (NaN payloads not newly accepted). Both negative/positive fixture sources match. Unchanged negative production is recoverable from recorded committedHEAD/start snapshot, rather than a claimed pre-run copy omitted by diff-only archiving.

**Ownership metadata:** saved CDB traces on exact accepted EXE for each numeric donor have12 real OwnIndependentImage entries and zero ResolveImage entries.8 ordinary data4/number5 or7 entries per trace retain native flags0 and the same UID for each seed/write pair; modified histories/ownership transitions are recorded. Trace exit0 supplies metadata evidence, while standalone runs supply GPU oracle acceptance.5 native checks PASS12.232s: image access classification, independent version transfer, buffer-view allocation lifetime and two packed/raw-word GPU writers. First native build used a nonexistent target spelling and was rejected before build; corrected CMake target built and passed, both logs retained. No broad suite repeated.

**Reproducible Release:** H/artifacts-a89af96,18 manual files; clean detached source,54 pinned dependencies/2283 compile inputs/ZERO primary checkout inputs,18.739s. EXE **a753a6e27550e5cd1f8ca20c162cb108853db12ce125add99c3b4233ebb5907c**; PDB **06c38e25d3dfacce8ce78059499495edcdeb017134bac348b60de8609e1a76aa**. Exact rebuilt core runtime/game/FPS NOT_RUN. No fixture acceptance transferred to rebuilt executable. No automatic app installation/selection; previous versions retained. No game/UI/XInput/profiler/user-process/original Vulkan/save changes. Unknown .serena/ and CLAUDE.md preserved.

**Next concrete mechanism:** ordinary writable R16G16 donors in the original32-bit mutable storage class when a packed10/raw R32 view arrives. Current CompatibleGuestWritableStorage word_format/raw_word excludes data5 and ordinary R16G16 allocation is its own typeless family; ReadonlyWordCasts currently excludes writers. Do NOT merely normalize descriptors: preserve actual legal resource format/UAV cast capability, typed numeric interpretation and old I bits. Continue guest_texture_cache.cpp ResourceCasts/ReadonlyWordCasts/allocation/Uav compatibility and actual gcn_writable_word_fixture.h donor cases, with independent endpoint before/after B/I/UID/history oracle and old-production negative control. Start with exact fixed-base single-sample direct storage; keep converted/shifted/MS/other geometry gaps explicit. Nested ordered disjoint/revalidation/BC fixtures still use off-queue GetCsRegs and need real PM4 queue callbacks before their own verification;46 core cases do not accept these. No129 re-audit or fixed crash replay.

**NEXT CODEX SESSION — START HERE:** Read this FIRST SESSION_HANDOFF section and H/VERIFICATION_FINAL.json; verify HEAD/upstream. Continue ordinary R16G16 writable donor -> packed10/R32 actual resource casting and alias compatibility, not just descriptor equality, with unchanged independent B/I/numeric/history oracles. Use pinned original and saved Vulkan/DX12 evidence; do not redo129 audit or fixed game diagnosis. Run narrow real GPU checks for finished mechanisms, build/checkpoint/normal push and preserve exact identities/manual version directory. Full suite and existing-XInput final game image/input/stability/completed FPS excluding FG versus saved Vulkan baseline remain final. Full port remains incomplete.


## Current queued GCN / coherent aliases / current-buffer image refresh - 2026-10-18

Production source **703f262325a8a7a0a3dbd279c78ed78d5d7354d8**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. This FIRST section supersedes older current-state/next-task sections. H=**C:/Users/skei/Documents/Codex/handoff/ordered-stream-20261018**; H/VERIFICATION_FINAL.json records exact sources, binary identities, logs and scopes.

**Completed mechanism and contract:** pinned BufferCache::SynchronizeMemoryFromImage/ObtainBuffer and Rasterizer::BindResources preserve descriptor ordering, separate physical B and valid I, and publish writes to later alias reads. Actual queued PM4 SetShReg/DispatchDirect now exercises46 ordered cases without bypassing Liverpool's active-queue assertion. Its original off-queue GetCsRegs failure was a fixture defect, distinct from the already captured/fixed d019 user crash. NativeStorageBufferUavs preserves Coherent on all native SSBO aliases (including readonly views), removes incompatible Restrict/NonWritable, retains load/store instructions and UBO qualifiers, and remains idempotent. Native persistent key is now native-dxil-cache-20261018-coherent-ssbo-v3; old shader artifacts cannot share this key. Vulkan conversion is unchanged.

**Confirmed production faults:** oldc23 production with the IDENTICAL final fixture fails rawR/W/formattedR at small GPU-modified sequence2: post-store raw/formatted reads see13579bdf instead of4567a9cd, although disassembled DXIL contains the store before both loads. Coherent fixes this native UAV visibility violation. A second failure after a4-byte exact-base writer showed actual B's untouched second texel still6a5b4c3d while I reloaded stale CPU10203041. ExecuteGuestResources now validates current GPU backing without premature CPU reconciliation; PlanCachedImageBinding can prepare a current-B source, and actual BindPlannedImage captures it at the real binding point. Existing valid independent I and cached-native-current I remain authoritative. Prewarming does not publish future ownership/residency, and dirty ranges are not widened. Exact-base versus interior invalidation, source pins, actual PageWatch transfer, resource state and fence lifetime remain enabled.

**Executed checks (exact identities in H):** final GCN EXE21a13cffe2a83fe924bb1e51f1aec32dad87a13309b544589e757ff929dccd04 passes46 ordered cases5.995s,20 mutable32 regressions4.381s,8 signed-word regressions4.097s,8 cached writable regressions4.235s:82 actual GPU cases total. Includes small/large, clean/dirty, descriptor orders, short/full/mixed streams, DWORD/byte/halfword writers, unchanged texel/padding and full65536 physical oracle.14 native regressions PASS56.181s: metadata helper plus memory/envelope/residency/DMA/independent image/PageWatch/pending fence/scheduler/allocation-view/access/word-writer coverage. Shader reload GPU fixture PASS5.711s:2 PSOs, changed contents, failed replacement rollback, restore, metadata negatives, real same-version persistent preload. Cross-version cache rejection is code-key proof only. All11 source-commit paths match archived acceptance sources after LF normalization; new unit source archival omission and unchanged immediate recovery are documented.

**Oracle transparency:** intermediate guest ReadMemory(output) reconciled the warmed source envelope, destroying the next ownership precondition (pinned original widens downloads to512KiB). Intermediate observation now copies actual GPU output to dedicated readback and overlays it on physical bytes before comparing unchanged independent expected data. Final whole65536 guest ReadMemory and physical comparison remain. Old production fails the SAME final observer/fixture; all earlier failures, compiler dump and untouched-texel probe remain under H. No expected words were changed to fit production.

**Clean committed Release:** H/artifacts-703f262,18 manual files; detached clean checkout,54 pinned dependencies/2283 compile inputs/ZERO primary-source inputs; incremental build21.914s. EXE **38d1e6aaef400ef46b64f946f917c42c5967310228b6a244673fa80688340658**; PDB **4dee97dde397c047e1e8e0d8ce0579e4ea1acc2cb864de55b95f17830a738a49**. Exact rebuilt core runtime/game/FPS NOT_RUN; no fixture result transferred to it. Previous versions preserved; no automatic application install/select. No game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved.

**Remaining / next actual work:** ordinary cached R32 SINT/FLOAT donor in CompatibleGuestWritableStorage (guest_texture_cache.cpp) and gcn_writable_word_fixture.h. Current local raw_word comparison accepts UsesRaw32 or ordinary data4/number4 UINT only. Add only actual typeless/UAV-compatible SINT/FLOAT with independent bit/numeric before/after GPU oracles, original class/UID/history, physical B separation and real ownership. Continue converted/multiview/shifted/MS/large and broad8 gaps;46 core cases do not accept the unrelated nested BC/other default ordered fixture. Coherent game performance is unmeasured. Do not repeat129 audit, fixed d019 game diagnosis, unchanged broad suites or original off-queue fixture failure. Narrow completed-mechanism regressions allowed; full suite/game image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final.

**NEXT CODEX SESSION — START HERE:** Read this FIRST section of SESSION_HANDOFF.md and H/VERIFICATION_FINAL.json; verify current HEAD/upstream. Continue CompatibleGuestWritableStorage ordinary R32 SINT/FLOAT donor and actual queued gcn_writable_word_fixture.h cases with preserved independent B/I/class/history oracles. Use existing Vulkan/DX12 evidence; no129 re-audit or fixed crash replay. Build and run narrow real-GPU checks, preserve exact identities and negatives, then clean-checkout Release/checkpoint/normal push. Keep whole-group acceptance separate, kernels manual, final suite/existing-XInput game acceptance and saved completed-FPS baseline comparison for final stage. Goal remains incomplete.


## Current physical shader streams / cached writable word checkpoint - 2026-10-18

Production source **c23ba1e6ddff2d879860b8e19364711f966dd519**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad production work items remain. This FIRST section supersedes older current-state sections. H=**C:/Users/skei/Documents/Codex/handoff/cached-writable-word-20261018**; H/VERIFICATION_FINAL.json is authoritative for THIS binary. Prior696ae21 cold shared-word mechanism and its complete evidence remain at writable-word-20261018.

**Confirmed new production violation:** cached ordinary R32 I(GpuModified1) plus a clean small buffer stream entered ExecuteGuestResources -> ReadMemory -> ResolveMemory -> ResolveImage, publishing I into raw physical B. Negative CDB captured source UID3, data4/number4/native flags0, and retirement before the dispatch; raw B GPU probe differed from its independent OldWord constant. Pinned BufferCache::ObtainBuffer uses MemoryManager::CopySparseMemory for a clean <=16384 stream, not image reconciliation. Additional eager writer readback could also overwrite B after execution. Ordinary R32_UINT lacks the cross-format codec flag despite exact R32 bits.

**Production correction:** Rasterizer::ReadShaderStreamBacking validates GPU-readable extent and uses the existing physical ReadDmaBacking override; both ordinary and ordered CPU-stream copies now use it, preserving DWORD envelope/byte adjustment, original stream/residency classification and temporary lifetime. Actual CPU accesses still reconcile normally. CompatibleGuestWritableStorage recognizes ordinary R32_UINT in LOCAL comparison only; actual class/history/flags never change. A real overlapping CPU stream and I writer require existing independent image capability and GuestTextureCache::OwnIndependentShaderImage -> real AcquireGuestImageOwnership/PageWatch transfer. Unsupported ownership fails explicitly; no fake success or eager overwrite. Existing staged ownership remains at its original binding point; later stages do not resurrect I. Write notification/legacy linear downloads, source pins, barriers/fences/leases and raw B remain enabled.

**Exact proof:** final GCN EXE **9d6c5896b0dff311f793cd607b74f15f378f7fb7c75c4fed73af79b23474831c** passes8 cached cases4.729s (SNORM/SINT, both descriptor orders, original GpuModified0/1 established by a separate REAL R32 seed image_load/store and raw B read, actual image_store/S_BARRIER/read-after, whole65536 physical B/output/padding); same exact EXE separately passes8 prior cold cases4.324s and12 exclusive-I cases4.353s. Positive CDB has12 real independent ownership entries, zero exclusive ResolveImage hits; modified source remains ordinary data4/number4/native flags0, same retained UID per seed/write, GpuModified1/Dirty0/alias1/watch1. Final identical fixture compiled against clean committed696ae21 production FAILS; its EXE/PDB/runtime/source identities remain in old-production-negative. No acceptance transfer between binaries.17 native tests PASS **261.043s**:10 memory/envelope/residency/DMA/PageWatch/fence/scheduler/allocation-view tests,4 representative changed-store/barrier mixed-word GPU modes, geometry/access/domain and original-profile classification, plus48 ordered-stage native adapter combinations. This adapter is separate from actual GCN stage execution.

**Oracle and fixture limits:** independent initial OldWord, seed SeedWord and final NewWord GPU constants are retained. First new fixture incorrectly expected a whole-image CPU ReadMemory to expose I in independent B. Pinned vk_rasterizer::ReadMemory calls only BufferCache; primary sources and correction preserved in GCN_CPU_ORACLE_CORRECTION.json. Final case warms a REAL64KiB B (not a small CPU stream), includes actual raw B access in the seed, downloads only output before comparing the physical alias, and checks I through actual GPU before/after loads. It does not newly accept whole native CPU image-coherence policy. Identical-final-fixture negative control proves this is not an oracle changed to match production. Intermediate failures and source snapshots remain. First CDB trace used -g and installed no breakpoints; cached-symbol-trace without -g and positive-ownership-trace are authoritative. Existing ordered-formatted-owned fixture FAILED before intended GPU dispatch because bind calls GetCsRegs outside an active queue (0x80000003); its evidence is retained, not a production regression pass or the old d019 user-kernel crash. Do not weaken that assertion.

**Clean committed Release:** H/artifacts-c23ba1e,18 manual files; clean detached source,54 pinned dependencies/2283 compile inputs/ZERO primary-source paths,25.824s. EXE **ab23d817d4bf1e59e82e5520afc56ebcf2bb69cad399ec0b5a1fb79d71df7d2b**; PDB **c55d275c865b80ae13b6243499cb068bbc5de5abee0fdecca3830bf9d50552eb**. Exact core runtime/game/FPS NOT_RUN; fixture acceptance remains separate. Earlier versions preserved, no automatic app install/select. No game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md retained; precommit staged/unstaged/source snapshots and every negative retained in local handoff directories.

**Next actual work:** H/NEXT_ORDERED_STREAM.md. Repair ONLY the existing ordered-formatted GCN fixture's queue contract (local ComputeProgram -> real SetShReg/DispatchDirect packets; original reflection inside queue), then execute the prepared stream/I/B ordering cases and fix any actual production violation at its original stage point. Native adapter48 passed; actual PM4 ordered acceptance remains open. Then extend the cached ordinary data4 donor to SINT/FLOAT only with actual typeless/UAV capability and unchanged numeric/bit/class/history oracles; current comparison newly recognizes UINT only. Continue existing converted/multiview/shifted/MS/large and broad8 gaps. No129 re-audit, old crash/game replay or unchanged broad suites. Narrow compile/GPU checks allowed; full suite and minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. Normal verified push; manual kernel versions.

## Current writable packed10/common-word checkpoint - 2026-10-18

Production source **696ae216e17148b0eaccfa86577b0c843b74dee7**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 separate implementation/behavior records and8 broad production work items remain. This FIRST section supersedes older current-state sections. H=**C:/Users/skei/Documents/Codex/handoff/writable-word-20261018**; exact evidence H/VERIFICATION_FINAL.json.

**Confirmed new failure and implementation:** original `_32BIT` compatible image storage -> NativeImageStorageAliases / ResourcePatchingPass / BuildGraphics pipeline view hints / CompatibleGuestWritableStorage / Entry::Uav / ExecuteGuestResources now agree on raw4/9/10 words. The old GPU case rejected different storage layouts; original/native classification separately showed the aliased read incorrectly left readonly beside the writer. Signed AND unsigned9 codecs and cross-stage hints select matching raw declarations. Compatibility normalizes ONLY local comparison copies/flags, preserving actual descriptors, guest geometry, permissions, image class/history, ordering and lifetime. Actual shared word UAVs keep existing barriers and codecs. Typed/expanded/packed16, depth/RT, changed shape and readonly permission negatives remain. No assertion suppression, fake operation, draw skip or game/address/hash workaround.

**Executed narrow proof:** exact native17 tests PASS59.83s:15 real GPU read-before/changed-store/ImageMemory barrier/read-after modes (4 signed/unsigned9<-R32,3 R32<-9,4 packed9<-RGBA8,4 RGBA8<-9),3 mip/layer passes with slot order changes and full physical guest/output/padding; plus geometry/permission/representation negatives and original-profile/native resource classification, including external-stage hints. Exact GCN EXE **c2b04c67ba64a4daf86829b6af399ac8eb3b3c5b0285efcba1d22edacd0c4b03** passes8 actual PM4/Liverpool GCN signed9/R32 cases4.944s: real T# table loads/image_store/S_BARRIER, independent BEFORE(+511,-511,0,-1)/AFTER(-512,0,511,+1) endpoint constants, both descriptor orders, repeated shader-specialization reuse and whole65536 bytes. Same exact GCN EXE separately passes20 direct mutable32 cases4.406s and8 signed readonly regression cases4.398s.10 related memory tests PASS46.57s (envelope/residency/DMA/PageWatch strong ownership/fence/scheduler/allocation-view/independent-image lifetime). Full identities/PDB/runtimes/source snapshots/LastTest and negatives retained; canonical-LF source equality to committed production verified. GCN image allocations reset each case: this is shader warm reuse, NOT a new claim about retaining an older native I UID/class/history. Whole-group/full game/FPS remains unverified.

**Clean committed Release:** H/artifacts-696ae21,18 files,54 pinned dependencies,2283 compile inputs/ZERO primary-source paths; build28.093s. EXE **54100342d87c1f1944752282b523e600166ba86817c1395acdc0e45fe29bfc62**; PDB **9caf18a9f9a64fd346851b089db284adedb21d0a39cfa024e355c687f4cd46ae**. Exact rebuilt core runtime/game/FPS NOT_RUN; narrow fixture results remain separate. Manual folder only; no app install/select, game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md retained. Source patches/new-header snapshots retained at H. First GCN archive omitted its new header; final separately archived run contains it and is authoritative.

**Next actual mechanism:** H/NEXT_CACHED_WORD_ALIAS.md. Extend the existing GCN writable-word case with a preceding REAL R32 image_load/image_store to create cached I with original GpuModified0/1 before the mixed writable bind. Check image class/history/ownership/native allocation and full readback, then implement only the concrete failing transition in MatchesGuestUploadStorage / CompatibleGuestShaderStorage / HasOwnedShaderStorage / source pins as required. Current raw-word comparison intentionally requires both UsesRaw32; cached ordinary R32 class may lack that flag despite exact R32 bits. This transition is source-identified, NOT yet reproduced or fixed. A sampled-only cached resource still cannot invent UAV permission; allocate/transfer with correct lifetime if required. Preserve exact-base invalidation, PS-before-VS/buffers-before-images and separate I/B publication. Continue existing converted/multiview/shifted/MS/large alias queue and broad8 gaps; no129 re-audit, old d019 crash replay or unchanged full numeric matrix. Narrow compile/GPU checks permitted; full suite and minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan remain final. Normal verified push; manually selected versions.

## Current readonly packed10 word/filtered/MS checkpoint - 2026-10-18

Production source **ec559ddffac172f9bd089b9fdf80e32249e5b4fe**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**.129 independent implementation/behavior records and8 broad production work items remain. This FIRST section supersedes older current-state sections; all historical evidence is retained. H=**C:/Users/skei/Documents/Codex/handoff/readonly-packed9-20261018**.

**Completed production mechanism:** ResourcePatchingPass/PatchImageSharp decouple native_packed10_raw from is_written; DefineImagesAndSamplers emits a readonly sampled R32_UINT word declaration, and existing EmitImageRead/UnpackNativePacked10 decode signed/normalized components. Guest swizzle applies after decoding, per texel. Existing manual sample/filter/gather/address/LOD code is connected for raw packed reads, decoding BEFORE interpolation. Specialization records packed format and guest swizzle plus manual sampler keys. GuestTextureDesc::UsesRawPacked10 / LayoutGuestTexture / CanBindReadonlyWordSrv / CanBindReadonlyWordView and raw-R32 sample-family compatibility retain the actual current I over scalar and signed packed views. Native R32 typeless/exact-format resources require no invented relaxed-casting capability; full typed-class casts still require the actual allocation list. No readonly UAV/mip-writer permission is introduced. Early ExecuteGuestResources source pins retain current native I before buffer preparation/other allocations.

**Second confirmed correction:** new GPU-modified/filtered case kept valid I but failed required formatted I->B publication. CDB captured valid ownership/history; fixture buffer reflection proved formatted1/GPU-modified1 before dispatch. Missing cached word-view proof let ValidateHostMemory reconcile B before stream classification, incorrectly selecting a CPU stream. HasOwnedShaderStorage now uses ACTUAL CanBindReadonlyWordView proof, with early source pin, preserving I, B visibility, original class/history and lifetime. Old failing EXE/PDB/inputs/output and measured ABI offsets/traces remain at H. Original expected B/I/filter bytes were unchanged. No suppressed assertion, fake success, skipped dispatch, post-filter decoding or address/game/shader-hash workaround.

**Exact narrow proof H/VERIFICATION_FINAL.json:** FINAL GCN EXE **0b97dd7cdee56525d6c4fbb870d6303f0abd04b9da155e4d07cb6d53a47295e9** passes128 actual PM4/GCN/GPU-validation cases in seven distinct runs:8 SNORM loads4.648s,8 SINT loads4.039s,16 SNORM bilinear/forced-swizzle cases4.183s,32 SNORM4x-MSAA cases4.450s,32 SINT4x-MSAA cases4.663s,20 prior direct mutable32 cases4.608s and12 exclusive-I cases4.418s. Both descriptor orders/raw-formatted B/GpuModified0-1, all four distinct MS samples, same guest code/hash across forced-swizzle specializations, independent signed/normalized corner/weight constants, full65536 physical B/I/padding checks.12 related native tests PASS72.28s: readonly permission/original-profile isolation classification, old expanded packed10 MSAA fallback, PageWatch/strong guards, buffer envelope/semantic residency, DMA, fences, scheduler and allocation/view/independent-image lifetime. Every EXE/PDB/runtime/source has its own manifest; canonical-LF source equality to this commit verified. No acceptance transfer from earlier binaries. Existing sampler algorithms are connected; every sampler/filter/address/mip/shape/MS count is not newly exhaustively tested. Whole-group/final game/FPS remain unverified.

**Clean Release:** H/artifacts-ec559dd,18 files, clean detached committed checkout,54 pinned dependencies/2283 compile inputs/ZERO primary source paths; build22.611s. EXE **f37c39a2549bdc7535941f1f9b4eda98bca91150c010fb888671f67574674179**; PDB **e216e61bdb9039843f38b1ad33136be0e7dcbd986f02ffa814106df370e8cd99**. Exact rebuilt core runtime/game/FPS NOT_RUN; fixture acceptance is separate. Earlier kernel versions preserved; manual folder only, no automatic app install/select. No game/UI/XInput/profiler/user-process/original Vulkan/save action. .serena/ and CLAUDE.md remain unknown untracked context, preserved. Source/staged/unstaged patches/snapshots and all negatives remain in H. An archive-tag collision launched no GPU test and left immutable EXE/PDB/identity/output intact; fresh accepted-* runs are authoritative.

**Next actual code:** H/NEXT_WRITABLE_WORD_ALIAS.md. Continue the existing common-native-image queue: CompatibleGuestWritableStorage currently normalizes raw4/10 but excludes raw9/4 despite R32 word representation. Prepare the minimal mixed writer/read-after-barrier regression from packed10_image_test.cpp::RunDisjointPacked10 alias-coherent, classify native/original-profile9/4 sharps, then production compatibility/UAV/cache/ownership integration and real PM4 counterpart as needed. This NEXT writable9/4 case is source-identified, not yet GPU-reproduced or fixed; do not remove its guard blindly. Keep original image class/history/UID, exact physical geometry, actual native resource/permissions, I/B publication, barriers and leases. Other converted/multiview/shifted/MS/large and broad8 gaps remain. No129 re-audit, old d019 crash replay or unchanged full numeric suite. Narrow compile/regression/GPU checks allowed; full suite and minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. Normal verified push; manual core folders.


## Current direct mutable32 and signed-word continuation - 2026-10-18

Source package **87ca49036188617722fbb31ff72f1c4181627c27**, production **8a8f3db43f062496e98cb7bc6b0b6ec27517e3d3**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 separate implementation/behavior records and8 broad production work items remain. This FIRST section supersedes earlier current-state sections; historical proof is retained. H=**C:/Users/skei/Documents/Codex/handoff/mutable32-20261018**.

**Completed production contract:** pinned FORMAT_TABLE::_32BIT / IsVulkanFormatCompatible / FindImage / ResolveOverlap -> IsDirectReadonlyWordClass / ReadonlyWordCasts / CompatibleGuestReadonlyWordStorage / CompatibleGuestImageMipGeometry, MatchesGuestUploadStorage / Entry::Srv / PrepareOrderedGuestStages and ExecuteGuestResources preflight. Direct readonly native classes4/5/6/9(UNORM/UINT)/10/34 now share the original native I, UID/class/history and typed SRVs on supported explicit D3D12 format-list allocations. Actual GCN execution found and fixed a second preflight rejection that native cache tests alone did not cover. Original I/B separation, per-stage order, CPU dirtying/protection and source/fence lifetime remain. Expanded signed9, guest writers/shared attachment feedback, MS, changed geometry/mip growth and unsupported casting are not newly admitted. No assertion removal, draw skip or invented source address/hash/game special case.

**Executed proof:** H/VERIFICATION_FINAL.json and FINAL_GCN_VERIFICATION.json. Native EXE **a8776389ca06639472fd67edfdafd1f7237afc0d138332e999903d793475c6f0**:20 GPU mutable32 cases80.89s (five typed classes versus R32, both source classes, linear/macro, selected mip/layer, GPU-copy modifiedI history, same UID/native allocation,2 ordered stages, full1MiB physical/resident B and no-cast/writer negatives) +16 affected same-data numeric cases64.91s PASS. Independent UF11/UF10 and RGB9E5 encoded words/numeric constants, not production decoder expectations.10 related envelope/semantic residency/DMA/PageWatch/strong guard/fence/allocation-view/scheduler/independent-image tests PASS47.25s with their own EXE/PDB/runtime identities. Exact initial GCN **0b5f8c2fffd9e236fb67d0e5cffbe83800108e77179e8cbbf3c384002608f892**:20 real PM4/GCN mutable32 cases4.794s and12 exclusive-I regression pass. Exact FINAL diagnostic-capable GCN **9d6bc0e428100164c6171f2e8e5e3f5c5772c78517e3ef284db09af5be2e1ec3** separately passes20+12 with whole65536 physical B/padding. Native cache/ordering tests and actual guest shader/PageWatch tests have separate scope. Clean tracked production inputs omitted by the old diff-only archive helper are explicitly reconstructed from recorded Git HEAD/earlier matching source snapshots in FINAL_GCN_VERIFICATION.json; not claimed as new measurement snapshots. Helper corrected for future captures.

**Exact remaining failure:** manual readonly-snormword-owned, FINAL GCN above, original R32 Iword0xc00805ff, interiorB marker13579bdf, readonly SNORM10/R32 pair. ExecuteGuestResources explicitly rejects `Native image aliases with different storage layouts unsupported`, then Liverpool unhandled_exception reports fatal; terminal code0xc0000005 is recorded WITHOUT inferring its later exception context (not captured). This new headless fixture is distinct from original d019 user-kernel crash. SNORM expected+1/-1/0/-1 derives signed fields+511/-511/0/-1; SINT mode prepared, not executed. Diagnostic is NOT CTest/WILL_FAIL success. Earlier GCN warm-alpha oracle was corrected solely from pinned RemapSwizzle(Format32): guest Alpha selector7 reads Red; GCN_ORACLE_SWIZZLE.json preserves original source/rationale/failed output. Native handcrafted SRV alpha1 test remains unchanged.

**Reproducible Release:** H/artifacts-87ca490,18 files, clean detached committed source,54 pinned dependencies/2283 inputs/ZERO primary-source paths. EXE **b3b4b5f8c03f0c092348f9735c812ac1d1891cd9322da1dc6959321f3e2dcdf6**; PDB **4168631bf48aa961a83585e7a1cc91d3ebfbdcd0e6cc20b1af3f31666b11f58f**. Previous clean artifacts-8a8f3db retained. This exact core runtime/game/FPS **NOT_RUN**; fixture acceptance never transfers automatically. Manual versions only; no app auto-install/select, game/UI/XInput/profiler/user process/Vulkan/save action. Precommit source/staged/unstaged snapshots, all positives/negatives and runtimes retained in H. Unknown .serena/ and CLAUDE.md preserved untracked; new archives on C, existing F build/toolchain retained.

**Next exact code:** H/NEXT_SIGNED_WORD.md. resource_patching_pass.cpp::PatchImageSharp, spirv_emit_context.cpp::DefineImagesAndSamplers, specialization.h packed ABI, GuestTextureDesc::UsesRawPacked10/LayoutGuestTexture/Entry::Srv and ExecuteGuestResources must jointly admit a true readonly raw-word representation without inventing writes/UAV permission. EmitImageRead/UnpackNativePacked10 already decode signed components; correct raw declaration, original swizzle/current I/cache history and filtering semantics still need adaptation. Never decode after interpolation or suppress the failing guard without implementation. Use saved minimal SNORM case; final SINT, filtered/shared writers and broader converted/multiview/shifted/MS/large remain explicit. No129 re-audit, original crash replay, unchanged44 numeric matrix or game repeats. Narrow compile/GPU checks allowed; full suite/expanded existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. Normal verified push; manual core selection.


## Current readonly word-view continuation - 2026-10-18

Latest production source **7b6985f03cbd459aa0a2faec05690ab5a97e0b02**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**.129 implementation/behavior records remain separate;8 broad production work items remain. This FIRST section is authoritative; preceding exclusive-image checkpoint and older sections retain historical proof. H=**C:/Users/skei/Documents/Codex/handoff/exclusive-i-20261018**.

**New production mechanism:** pinned host_compatibility.cpp FORMAT_TABLE marks R32 and RGBA8 as _32BIT; TextureCache::FindImage/ResolveOverlap preserves compatible mutable I. ReadonlyWordCasts + CompatibleGuestReadonlyWordStorage + MatchesGuestUploadStorage + Entry::Srv now keep the SAME readonly native allocation/UID/class/history across direct data4/10 views, using an explicit D3D12 format list only when supported. PrepareOrderedGuestStages admits this exact common-allocation domain; actual SRVs require the resource's cast list. Expanded/depth/BC, writes, MSAA, changed geometry/mip growth, other32-bit classes and unsupported casting remain outside this new path. Original CPU/B publication, protection/order/fence lifetime are retained; no draw skip/assert suppression or altered expected data.

**Exact verification:** H/WORD_VERIFICATION.json. Native EXE **bbf3d30c3cafa31c6b64fe65580cae49dffad76cbddc941bdc16592af3b5998a**:4 real GPU R32/RGBA8 cases16.08s (both source classes, linear/macro, selected mip/layer, original modified I established by GPU copy,2 ordered stages/interior B admission, whole1MiB independent B readback, no private I conversion/new UAV permission; missing-cast and writer negatives) +8 affected same-data numeric cases32.00s PASS. Typed GPU outputs use independent numeric constants. GCN EXE **2cb25c39a3c44de6dc702d7c23d46b6aa6403940ace2102dab7a260851b6974a**:12 actual PM4 exclusive-I numeric cases4.924s PASS; current production sources equal this commit. Native suite tests cache/ordering adapter, not full guest draw or PageWatch; actual PageWatch proof remains separately at the c209 checkpoint. Old-source word-view UID/class failure preserved at H/word-negative-*; fixture allocation/envelope authoring failures and a stale old-binary run after compiler failure are explicitly recorded, not positive evidence.

**Clean Release:** H/artifacts-7b6985f,18 manual files; detached committed checkout,54 pinned dependencies/2283 compile inputs/ZERO primary-checkout paths. EXE **8b8e875ff267240ce259bb78dc94487319e0485d2def0a14990e2ca0b57d3352**, PDB **dc80b2c99241c2c0751473312825c01076656e9889da706fb9644763d509c7d6**. Exact rebuilt core runtime/game/FPS **NOT_RUN**, separate from fixture verification. Prior artifacts-c20963d and all older cores/results preserved. Both source commits and diagnostics are recoverable; precommit patches/snapshots in H. No automatic app install/select; no game/UI/XInput/profiler/user-process/Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved untracked.

**Next concrete code:** extend ReadonlyWordCasts / CompatibleGuestReadonlyWordStorage / typed views only to other pinned _32BIT direct readonly classes (data5 R16G16 and data6 R11G11B10 first;34 RGB9E5 after its sampled/codec constraints), keeping actual resource format lists and per-channel independent GPU oracles. Then continue remaining converted/multiview/shifted/MS/large alias queue. No general129 re-audit, original crash replay or unchanged full numeric matrix. Narrow compile/GPU regression permitted; full suite and minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. Verified normal push; manually chosen kernel folders.


## Current exclusive-image preservation checkpoint - 2026-10-18

Source **c20963debe065143f2fc04acc505cd7b65974e8d**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**.129 independent implementation/behavior records and8 broad production work items remain. This first section supersedes older current-state sections; their evidence is historical. H=**C:/Users/skei/Documents/Codex/handoff/exclusive-i-20261018**.

**Confirmed cause and production correction:** CDB captured PrepareShaderBuffersImpl -> GuestRasterizer::PrepareGpuBacking -> ResolveMemory -> ResolveImage retiring standalone valid exclusive I UID2 (gpu_owned1, GpuModified1, Dirty0, cached_native_current1). An interior+4 B writer then caused typed views to decode B13579bdf instead of retained Iff0000ff. Pinned exact-base GPU invalidation does not dirty that I. GuestBufferAccess::PreserveImagesForGpuBuffer / GuestRasterizer override now transfer the SAME Entry/native allocation to existing independent I/B ownership before buffer admission, retaining contents/history/protection/fences. CPU ResolveImages/ResolveCpuFault still reconcile normally; unsupported physical domains retain explicit resolver. After trace shows same UID2 and independent alias acquisition, no early retirement. No weakened assertion, skipped GPU operation, changed oracle or game/hash/address workaround. The earlier d01969e0x80000003 has separate captured/fixed935fb69 evidence under F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z; this is not a new exit-code-based diagnosis.

**Executed narrow verification:** exact GCN EXE **e5544fd37be3cf0c28b9e4401649bed34413962b39105278679a97e73638ac6f**:12 exclusive I numeric-pair/order/raw/formatted cases4.730s,24 joint/history cases4.280s and8 cached-depth/storage cases3.951s PASS; actual PM4/GCN/GPU validation and complete65536 physical bytes/readback.10 related tests PASS48.10s: buffer envelope, semantic residency/CPU-store negatives, DMA, PageWatch strong ownership, pending fence, scheduler and allocation/view/independent-image lifetime. Each EXE/PDB/runtime/source has own identity at H/VERIFICATION_FINAL.json, related-memory/identity.json and LastTest.log. Canonical source equality to this commit checked; old failed b19 binary/output/trace retained. Unchanged177s numeric matrix was not repeated.

**Clean Release:** H/artifacts-c20963d (18 files), detached committed checkout,54 pinned dependencies/2283 compile inputs/ZERO primary source paths. EXE **a0c273078d5656f67b69bc92eebf1c5fd7486613daef7001ca60679ecab27222**; PDB **5666aedb22c8f3c0d659bd7d56545f41c2741fef9c582f4b0b291987e8436873**. This exact rebuilt core runtime/game/FPS **NOT_RUN**; fixture acceptance is separate. Manual folder only, no automatic app installation/selection. Original Vulkan/saves/user processes unchanged. .serena/ and CLAUDE.md remain unknown untracked context; preserved, not committed. Precommit staged/unstaged patches and full changed-source snapshots are in H. New archives use C because F has only about3GB free; previous kernels/evidence preserved.

**Next code:** guest_stage_bindings.cpp PrepareOrderedGuestStages converted-view domain validation and RefreshBoundImage common-allocation constraint, using existing converted/multiview/shifted/MS/large queue. Preserve original PS-before-VS, per-stage buffers-before-images, exact-base invalidation and separate I/B visibility. First establish a concrete converted-view fixture from existing forms, then production transfer and narrow GPU acceptance. Do not repeat129 audit, original crash/game run or unchanged broad matrices. Compile/narrow GPU checks allowed; full suite/expanded existing-XInput game acceptance remain final. Normal verified push; manual version folders.


## Current recording-context and readonly numeric checkpoint - 2026-10-18

Source package **b19feb36b32da792d9d395fc7c563794c5f53348**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**,129 independent implementation/behavior records,8 broad production gap items remain. SESSION_HANDOFF FIRST section authoritative; older sections historical. H=**F:/PS4Dev/handoff/readonly-numeric-20261018**.

Production21445a2: GuestTextureCache::SetRecordingListProvider / UploadVersion reacquire current scheduler context after protected guest memcpy/watch/callback can Finish. CDB captured PageWatch CPU fault in UploadVersion then validation547 at GuestTiler::Lease::Record on closed list. Original submissions/state/order/leases retained. CompatibleGuestShaderStorage / MatchesGuestUploadStorage now share ordinary nonattachment readonly numeric views over existing exact DXGI typeless allocation, preserving UID/class/I and typed SRV. Depth,expanded signedpacked,missing-family guards and no-new-UAV permission retained.62c0d3b re-marks pending receipt after reentrant submission so resumed commands/pools belong to new fence. No assertions disabled or fake success/draw skips.

Exact scope H/VERIFICATION_FINAL.json:21445a2 native **185fd8c56f22857f2a8f83882a0de8390fffc864a39c37292e5305919adc8fd7**44 GPU numeric cases177.46s,11 families xlinear/macro xfloat/uint-first,2mip/layer,real-copy I history,UID/native/class unchanged,RGB32 alpha/depth/expanded negatives; GCN **c2669d6bb12c6182a775e9a76f74741d3c3c7a4ad6d1fd5ebff7a776096445af**24 actual numeric/history/raw/formatted cases5.417s +8 CMASK18 regression4.10s; depth32.25s +7 memory/envelope/DMA/fence/lifetime tests5.83s PASS.62c0d3b separate GCN **59ad8bada6c7f257c717090a2590c1b2d981777516b7ae9621f554b6bd984f78**24 cases4.974s +native **ca6a35ca05a80abfa4644d1b961295aa452d7212e7195650cd44aae9f985ed25**independent-base4.09s PASS. No acceptance transferred between binaries or to rebuilt core.

**Still failed on latest source:** manual readonly-numeric-exclusive-owned, exact GCN **b7bd1dbb7c290ac978984f421d07730f182aa21a24bb1faa981287912fbaf878**: standalone image_store theninterior rawB writer leavesB13579bdf as expected, but UINT/UNORM I views decode that marker instead of oldff0000ff. Joint-independent seed24 passes do not close this exclusive ownership domain. Original exactbase GPU invalidation skips+4; SafeToDownload requires GpuModified&&!Dirty. Hypothesis is exclusive-I ownership reconciliation/history loss; actual flags/retirement transition still must be localized. Follow H/NEXT_EXCLUSIVE_I_TRANSFER.md. Diagnostic is intentionally NOT registered as expected-success CTest.

Clean committed Release **H/artifacts-b19feb3**,14.893s,54 pins/2283 inputs/ZERO primary paths/18 files; EXE **2dcbd7964e1f5d694bb522df463da32d1b14b85c68c1c56b9401f60d627ae5e1**, PDB **1bc87a330ca232feff9c63a7d9972fae5a2142dc53f30f12db725c6d36af9444**. Exact core runtime/game/FPS NOT_RUN. Prior214/62 kernels and all failed/positive data retained. No app auto install/select,game/UI/XInput/profiler/user process/Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved. Disk-full archive failure recovered by compressing ONLY this new H:5.006GB logical ->1.326GB physical,227 recorded archive digests and54 release file identities verified, no deletion. H/STORAGE_NOTE.json has exact state. Future large evidence may use C if F low; keep source/build trees and pinned toolchain. Narrow checks allowed; final suite/expanded game remains final; normal verified push/manual folders.


## Current native packed19 MSAA checkpoint - 2026-10-18

Source **b5029e12430db8594cbb8d475e080c2d08b0ef5a**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**. SESSION_HANDOFF FIRST section authoritative; historical sections retained. H=**F:/PS4Dev/handoff/packed19-msaa-20261018**. Original format19 B4 UNORM preserves all four4-bit channels/sample order. Production GuestMsaaUploadSource PACKED_MODE3 now normalizes alpha by15 (old GPU got000f vs expected0004). GuestTextureCache::CanTransferIndependentMultisample admits exact native B4/2-byte geometry, retaining format/sample device gates, RAW32/expanded separation and original retile/padding/order/lifetime. No float attachment approximation or unsupported-adapter bypass.

Exact native **85e0939c5afcec4eba18aa4175671a2b403d472d93899f940c9ce513885b8a99**, PDB **5f21aa3b54fcb198208c3465db887342a11fcffef93e1360df938957d8da9ddf**:6 actual AMD native19 cases(samples2/4/8 xlinear/macro,two layers),seed+ADD-blend phases,all16 alpha codes,RG-only/clipped/alternating sample mask,16016 independent typed sample-word probes plus wholeguest/B/padding readback PASS23.77s;2 existing packed16 raw/sampled MSAA tests PASS97.89s. Two separate old-source exact binaries reproduce alpha corruption and transfer-domain rejection. Own EXE/PDB/source/runtime/generated shader/LastTest manifests at H/VERIFICATION_FINAL.json. Initial narrower-alpha positive6-cases23.69s has separate identity. No PageWatch/full ownership,all blend/logic/MRT/CMASK/feedback/game/FPS claim; whole-group behavior acceptance unchanged.

Clean detached committed Release19.543s,54 pinned deps/2283 compile inputs/ZERO primary paths;18-file manual **H/artifacts-b5029e1**, EXE **a45cadb788600db50b0b2d3fee89ef0c49395b630bb2494e8b492c1574954562**, PDB **f2b74f597c102eff2ba7b6c0f2723ead9f60a9b94facc17cd0f685f725b2711c**. Exact core runtime/game/FPS NOT_RUN; no result transfer from fixtures. No auto app install/select,game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md retained. Prior core versions and test evidence untouched.

129 independent implementation/behavior rows retained. Registered native19 MSAA source gap is now resolved by native supported-adapter path; hypothetical fallback on an adapter unsupported by pinned original is not a requirement. **8 existing broad production work items remain**, not a full-port closure. Existing late-envelope guard is not itself proof of a missing code path: current capacity-union covers examined planned descriptors, unseen-growth hypothesis unconfirmed. Next actual guard: guest_stage_bindings.cpp PrepareOrderedGuestStages converted physical-domain validation / RefreshBoundImage common-allocation constraint, existing converted/multiview/shifted/MS/large alias queue. Follow H/NEXT_EXISTING_ALIAS_DOMAIN.md; no129 audit, crash replay or unchanged full matrix. Narrow GPU checks allowed; full suite/expanded existing-XInput acceptance remains final; normal verified commits/push/manual versions.


## Current source-native packed codec checkpoint - 2026-10-18

Source package **edb2d6945639ea09a0db381bbee4902a90d153a1**, branch fix/mixed-cmask; full original Vulkan-to-DX12 port **ACTIVE / INCOMPLETE**. SESSION_HANDOFF FIRST section is authoritative; prior sections are historical. H=**F:/PS4Dev/handoff/packed16-codec-20261018**. Narrow compile/regression/real GPU readback allowed; full suite/expanded game acceptance remain final.129 implementation/behavior records remain independent.

Pinned ResolveDepthOverlap / CopyColorAndDepth -> RetileImageVersion now separates SOURCE native codec/stride/depth from TARGET addressing via GuestTiler::NativeFormat / DescribeNativeFormat / Prepare. Direct nonexpanded16..19 word classes connect cached D16/D32 -> RT/RAW32 storage and color -> depth, keeping original min-mip/layer/lexicographic resources, old D despite B dirty, S exclusion, B tail/padding/meta/guard/fence leases.74519e5 production+fixtures; e277da5 original inverse image_store swizzle fixtures; edb2d69 capability-selected isolated test device. No production shader workaround or changed app adapter.

Exact identities/source-equality in H/VERIFICATION_FINAL.json. Native745:80 packed RT/UAV/reverse-copy cases PASS; two19 RT attempts fail on NVIDIA capability. Nativee277:8 sampled19->depth, paired depth and2 MSAA packed16 tests PASS. Nativeedb:16 previously inapplicable19 RT/reverse cases PASS on AMD. GCN745:24 CMASK16/17/18 cases PASS; GCNe277:32 packed16..19 actual image_store cases plus8 CMASK18 regression PASS; GCNedb:8 CMASK19 cases PASS on AMD. Source-specific binaries/results are NOT transferred to each other or rebuilt core. Two deliberately wrong SOURCE codec controls fail actual GPU bytes; original expected data unchanged.8 related envelope/preserved-version/DMA/pending-fence/allocation-failure/view-lifetime/scheduler/independent-image tests PASS11.97s. Independent whole65536 physical B/S/meta/padding checks and real PM4/GCN probes retained; no every-pixel/every-number/comp_swap/MRT/MSAA attachment/game claim.

**Scope correction:** NVIDIA5080 supports neither Vulkan B4 color attachment nor D3D12B4 RTV/blend; original has no B4 fallback. AMD supports both; isolated19 RT/CMASK passes there. Earlier universal19 RT fallback claim was an unproven inference, superseded by H/APPLICABILITY_SCOPE_CORRECTION.json; failures remain recorded. Conditional packed19 MSAA cross-API capability gap is not established/closed by single-sample results. Nine registered broad work items remain (eight known domains plus this conditional item); do not invent requirements beyond same-adapter pinned original. Storage fixture observation stride and inverse swizzle errors corrected from original evidence, not by changing production output; failed logs retained.

Clean detached committed Release18.271s,54 locked deps/2283 compile inputs/ZERO primary-checkout paths. Manual18-file **H/artifacts-edb2d69**, EXE **089c9465b88253a01d0e8d0272279a68b16e68fa71082bf061416b8aa15b2b3f**, PDB **8d6b8adf67f9fa17946c4d3ccc7a23e8ee58405b0603d8d3d13e70a462f9d0cf**. Exact rebuilt core runtime/game/FPS NOT_RUN. Earlier745/fd8/e1eed cores retained; no auto app install/select. No game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved untracked.

Next actual CODE: H/NEXT_ORDERED_DOMAIN.md; ordered late cached source resource growth beyond prepared envelope in GuestTextureCache::PlanCachedImageBinding / BindPlannedImage, PrepareOrderedGuestStages / ExecuteGuestResources and Rasterizer::ValidatePreparedShaderBufferRange. Keep descriptor allocation stable by reserving only necessary physical capacity before final views, while original I/B/class/meta/ownership/residency publication stays at actual bind. Guard remains until exact proof and GPU fixture; no silent clamping/removal. Converted/multiview/shifted/clipped/MS/large remaining domains remain explicit. Do not rerun129 audit, original crash or unchanged broad matrices. Normal verified commits/clean builds/upstream push; manual-folder kernels. Final suite and minimal existing-XInput/image/input/stability/completed guest FPS versus saved Vulkan baseline excluding FG remain final.



## Current packed-float word and enhanced initial-layout checkpoint - 2026-10-18

Source **fd8e5f6196c669ca91af300a8c9be85b2b270cce**, fix/mixed-cmask; full port ACTIVE/INCOMPLETE. SESSION_HANDOFF FIRST section authoritative; prior sections historical. H=**F:/PS4Dev/handoff/native-packed6-20261018**.

Pinned ResolveDepthOverlap/CopyColorAndDepth -> IsDirectDepthWordClass now admits direct R11G11B10_FLOAT(data6/number7) cached depth word recreation/full resource normalization. Native word copy/target seed/source S exclusion/original flags/metadata/fences remain. Real renderer exposed PRODUCTION synchronization defect: PlacedTextureAllocator::Allocate CreatePlacedResource2 made enhanced COPY_DEST but Resource defaulted legacy. Exact old initial-layout-control reproduces validation interop rejection. Resource.enhanced_barriers=true at successful creation fixes tracking without changing initial state or bypassing barriers. Enhanced access/order/shared allocation lifetime retained.

Exact native **edda6b122f90cbba3fd8a5357876cb06e3532f0d9c00d7f514264c9a88ac330a**:2 tests/16 finite6 RT/UAV real GPU cases81.09s PASS. Exact GCN **c83e458a8071a5438be97e3223f7e71228480ea101a7035ca33d24b892bdf3bf**:8 CMASK6 cases5.050s +8 CMASK10 regression4.265s PASS;21 typed RGB UF11/UF10 probes(63 channel words) per6 case, all16 logical tiles/draw boundaries,full65536 physical B/S/meta/padding;clean/dirty,S/HTILE. Independent finite mini-float arithmetic/word oracles, not production decoder expectations. Exact allocator **4866df31991cc37afe66e3aba4c9af380764969cd54dd57c610b92a867723c9e**:COPY_DEST+COMMON upload/copy/readback0.395s and texture heap/budget/live-lease/GPU texel0.333s PASS. Unsupported-cast error subbranch skipped on supported adapter; actual positive test not skipped. No NaN/subnormal/rounding/comp_swap/full-group/game claim. Initial Native16 pass82.30s had another exact binary before allocator correction; earlier failed GCN/control retained, final checks recompiled. Previous128-case3/5/9/10 at e1eedf9 retains own identity, not transferred to new core.

Clean detached committed Release18.313s,54 pins/2283 compile inputs/ZERO primary paths;18-file manual **H/artifacts-fd8e5f6**, EXE **5a4d8cd37ff31cd9bb76a58f8d6fee295d51a5208400e25d0d91b9d0701261c7**, PDB **bd0175aeeed0cdd29462128ae44bb57da8b24608249f9c5137d767602855f6fe**. Exact rebuilt core runtime/game/FPS NOT_RUN. All9 source/build files including texture_initial_layout_fixture.h committed. H has exact source/exe/PDB/runtime/negative/positive manifests, logs, precommit patch/full snapshots and generated-shader equality. No game/UI/XInput/profiler/user-process/auto install-select/Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved untracked.

Next actual CODE **H/NEXT_NATIVE_PACKED16.md**: RetileImageVersion / GuestTiler::Prepare / guest_tiling.hlsl separate SOURCE native codec from TARGET addressing before admitting16..19. PackedFormat currently precedes Depth16: future D16+S -> packed target must not store low float bits or apply destination codec to source footprint. Current guards exclude this hazard. Keep original binding/word/ownership/envelope/fence contracts. Other known late-growth/converted/multiview/shifted/MS/large domains and nine broad gaps stay open;129 independent implementation/behavior rows preserved. No new general audit/repeated completed matrices; final suite and minimal existing-XInput game/completed frames remain final.


## Current native word-class and ownership transfer checkpoint - 2026-10-18

Source **e1eedf9b70cbd9eb1f3ee9e0c150f3eef58e7f6d**. Full port ACTIVE/INCOMPLETE;129 independent implementation/behavior records and nine broader gaps remain. SESSION_HANDOFF FIRST section authoritative; older entries historical. H=**F:/PS4Dev/handoff/depth-color-formats-20261018**.

Pinned ResolveDepthOverlap / CopyColorAndDepth -> IsDirectDepthWordClass / IsCachedDepthRecreationSource / CanMapIndependentDepthImage / ResolveDepthViewStorage now connect exact direct RG8(3),RG16(5),RGB10A2_UNORM/UINT(9),RGBA8(10)16/32-bit color words, including original lexicographic full resources/growth. Old native D words copied through native footprints, not numerically cast; old S excluded, B seed/tails/padding/history/meta/fence leases retained. Other admitted numeric modes are implemented but not all verified. Packed/expanded/permuted classes remain explicitly gated.

Confirmed production failure: new independent target inherited old exclusive depth NOACCESS without an explicit ownership transfer; later UnmapMemory rejected independent read-only watch retirement. Actual C++ throw/stack and old binary at H/cmask-watch-debug2 and cmask-tile-address. SetIndependentOwnershipTransfer passes the actual replaced Entry to OwnIndependentImage / AcquireGuestImageOwnership / PageWatch::TrackImageReadOnly. Transactional page cuts demote ONLY that source's transferred guards, preserving stronger B/unrelated guards and rejecting missing proof. No guard suppression or RW window. Native copy/ordering/lifetime contract retained.

FINAL native matrix:16 tests /128 real GPU cases PASS in 641.63s. Exact final native **bfcf069feff03b725b3ef49b01888ed6b0525c1c0671bef702b9fe2b53dfed3f** has independent typed/raw/guest/full B/capacity/padding/source-S/held-fence checks. Exact GCN **523e102e649403f30d0691b7c72255dbe9aacc3ee04a354bc098cc54f7df4470**:8 full renderer RGBA8_UNORM native CMASK clear/draw cases4.713s,8 single-stage4.406s,8 original Fragment-before-Vertex4.539s,4 old depth caller4.388s PASS. CMASK uses21 exact UINT word guest image probes covering all16 logical tiles plus draw boundaries; all65536 physical B/S/meta/padding bytes checked. This is not all native pixels/number/comp_swap/MSAA coverage. Nine related PageWatch/real GPU ownership/B versions/DMA/envelope/fence/lifetime/independent checks PASS with own exact binary identities (H/related-final initial2 and related-final2 remaining7). Negative missing-source/strong-B/outside-owner checks pass; old no-transfer CMASK binary fails at the preserved contract.

Clean detached committed Release22.900s,54 pins/2283 compile inputs/ZERO primary paths;18-file manual **H/artifacts-e1eedf9**, EXE **9ca2c5a98c9bbfc98ea75ddaced9f5361da9fa4b91feb2e6d961fd918fe92609**, PDB **a89f9a1df944fb972d3e020ad402e1c08be2a9e1bfc24c69bde49936122f9cdd**. Rebuilt core runtime/game/FPS NOT_RUN; fixture acceptance stays on its own binary. No automatic app install/select/game/UI/XInput/user-process/Vulkan/save changes. Unknown .serena/ and CLAUDE.md remain preserved untracked.

Next actual CODE: H/NEXT_NATIVE_FORMAT_CODE.md, exact source native-to-guest codec in RetileImageVersion / GuestTiler before admitting direct packed float6 / permuted16..19 where original capabilities permit. Finish full member CMASK numeric/component cases where implementation is already connected; missing tests alone are not code gaps. Late source growth beyond prepared envelopes and converted/shifted/multiview/MS/large domains stay open. No repeated129 audit/original crash/completed studies/unchanged large matrices. Narrow checks allowed; final suite/expanded game acceptance stays final. Normal verified upstream push and manual-folder versions.


## Current cached depth-to-storage checkpoint - 2026-10-18

Production/source **641f6b9c057d23c489f0bda2cc52d046d72fa3c9**, pushed to origin/fix/mixed-cmask. Direct fixed-base1x D16/D32 -> format2/4 writable color is CONNECTED through actual shader binding, legal writable snapshot/ownership and original word copy. CachedImageBindingPlan / PlanCachedImageBinding / BindPlannedImage replace earlier depth-only names. GPU-only input validation, early retention of only already current OWNED I, shared nonpublishing capacity plans and original-I witness for earlier formatted slots preserve original I/B order; future class/meta/ownership/residency commits at actual bind. H=F:/PS4Dev/handoff/depth-writable-20261018; SESSION_HANDOFF FIRST section authoritative.

FINAL exact native **fca02f5df87b3f6b6d8eaafdd7da17c78c998c388b3aea8d8f628e1ebe537c97**:16 float16/32 UAV dispatch cases77.10s +8 late-native42.37s +paired24/4 aspects/12 compact32.64s pass. FINAL exact GCN **b3da2b74779e0bcab9bb91a6e5e701c1b1548d119b3ee92faabb401c906c0de7**:8 single-stage PM4 storage4.143s +8 original Fragment-buffer/Vertex-image3.936s +4 prior depth caller4.654s pass. Clean I0.875 vs physicalB0.125; dirty B0.25; actual image write0.5; native tail0.875; optional S/HTILE/full65536 physical bytes. Intentionally early-bind control fails expected B observation; other authored GCN GetCsRegs helpers require proper queue use. Narrow exact binaries only; final/core/game/FPS NOT_RUN. Full final manifests include source equality, EXE/PDB/runtime identities; failures/dump/scope corrections preserved.

Clean committed Release25.788s:54 pins/2283 inputs/zero primary paths/18-file manual core **H/artifacts-641f6b9**, EXE **7dc8fa25071a839521d819698b6b94e24a152d2379f988301a9046215b6e25e4**, PDB **3d75dc45ae3479329ce503121e35020086351cebc64da0b8952c4b7361a0a28a**. No auto install/select/game/UI/XInput/user-process/Vulkan/save action. Source tracked tree preserved; .serena/ and unknown CLAUDE.md remain untracked.129 independent records/nine broader gaps still partial; goal ACTIVE. Next actual CODE: target data-format IDs beyond2/4 at IsCachedDepthRecreationSource / CanMapIndependentDepthImage / ResolveDepthViewStorage, and native-only pending CMASK decoder/component/clear integration. Follow H/NEXT_NATIVE_DEPTH_COLOR_FORMATS.md. Do not repeat original crash/129 audit/completed matrices; final suite/game acceptance stays final.

## Current private depth binding checkpoint - 2026-10-18

Production/source package **d52ac76798704617d12ab7d6e28ac6625799474e**, pushed to origin/fix/mixed-cmask. Capacity-only PlanCachedDepthBinding leaves old native class/I/HTILE/ownership/residency intact before shader bindings. BindPlannedDepth reselects CURRENT I/B at actual DB binding, preserves native S during D-only B refresh and consumed HTILE/history, and binds the actual initialized DSV. Real independent replacement now dirties its retired source. Headless fixture initializes ElfInfo through the same InitializeApp call as production; original assertions remain enabled. H=F:/PS4Dev/handoff/private-depth-binding-20261018; SESSION_HANDOFF FIRST section authoritative.

Exact native **5d121ea72296b6a73652f3a7f08aeefa8ea63de5576422cf38ec6bcb84e37dbf**: 8 late-native37.60s +8 late-B36.67s +16 dirty-depth72.26s +16 color-depth71.24s +16 RT71.30s +paired24/4 aspects/12 compact29.38s pass. Exact GCN **f9795ec2b10f1f09b7f9bcabaac63cf2dcd8fb34f740377d32d9b0fedeef6f40**: four actual shader/draw cases4.617s pass; PS sees B0.25 while DB consumes old native0.875. Separate intentionally early-bind control fails first pixel df vs40, as required. Seven related buffer-envelope/preserved-version/DMA/fence/lifetime/independent-conversion checks pass6.03s. All sources/native/PDB/runtime/LastTest/failures kept at H; no validation ERROR/CORRUPTION in accepted runs; nonfatal unaligned warmup ownership warnings retained.

Clean committed Release35.804s,54 pinned deps/2283 inputs/zero primary paths;18-file manual-folder **H/artifacts-d52ac76**, EXE **d03ed78f5bf95a993ed4ad14b6f35b912dd4af29c0df60be409541c7491955ba**, PDB **60c984c18dd8fb47fe364a882a14c7d88094d2e4dd15b9e09d4ac94000499709**. Narrow fixture checks do NOT accept this exact rebuilt core's runtime/game/FPS. No game/UI/XInput/auto-install/select/user-process/original Vulkan/save action. Preexisting .serena/ and unknown CLAUDE.md preserved uncommitted; separate clone metadata exclusion from prior checkpoint unchanged.129 independent records/nine broad gaps remain; full port ACTIVE/INCOMPLETE.

Next concrete code: cached native depth -> shader-writable color at IsCachedDepthRecreationSource / RecreateCachedDepthAtBind / ExecuteGuestResources original binding order. Follow H/NEXT_DEPTH_WRITABLE_BINDING.md; preserve this capacity-only strategy, not the superseded copied-private-entry/revision-token proposal. Actual writable/other native RT-CMASK formats/geometry/ordered-MS/64MiB/late-domain-growth guards remain explicit gaps. Full final suite/game/XInput/completed guest FPS vs saved Vulkan baseline excluding FG remains final.

## Current cached depth-to-RT checkpoint - 2026-10-18

Production **94ace23279c472f275c36d556afdf0313e1be444**; latest source/fixture package **01a1f6cea7a373b54b404f622ac93d6cf9cd3c26**. Direct1x depth->RT native copy/caller integration remains as94ace23. H=F:/PS4Dev/handoff/cached-depth-rt-20261017. Initial4 native12221e6e passes are separate; initial8 RT extra-mip helper cases do not prove original ColorBuffer call applicability. Pinned CB inherits one mip and changes layers; fixture-only01a1f6c now tests one-mip layer growth/full-parent reuse. Final16 cases73.53s at exact native ab7a2c334fa6829119ed9a119ef413fbcfc363151578e33840e4713bf6225d0a pass unchanged independent native/typed/guest/B/padding/source/fence controls. Final CB manifest and initial manifest/runtime identities separate. Full CMASK member/decoder/metadata/core/game runtime NOT_RUN; guards remain code gaps. Clean committed package Release10.46s on18Oct,54 deps/2283 inputs/zero primary paths;18-file manual core H/artifacts-01a1f6c EXEa9e588bccdad9aa30106b16aa4c1ee01803903b74d8697f14519486a9f613634. Context-only unknown CLAUDE.md preserved/excluded only in separate validation clone metadata; see local exclusion proof. Prior94 core preserved. SESSION_HANDOFF FIRST section authoritative. NEXT_PRIVATE_RECREATION_PUBLICATION.md: fix early DEPTH preparation class/meta/ownership publication and actual bound DSV producer receipt; then writable/CMASK/other RT formats/geometry/ordered/MS/size gaps.129 rows/nine broader gaps partial; goal ACTIVE. No game/install/select/user-session or original Vulkan/save action.


## Current cached color-to-depth checkpoint - 2026-10-17

Production **414094c4eef296b61aeaf6aad79382dc7726df05**: cached_native_current / RememberNativeImageStorage retain initialized color native class after CPU dirtying; direct fixed-base1x color->D16/D32 recreation preserves old native words versus new B, original GpuModified0/1, new S/padding/metadata/UID/prewrite/fences. H=F:/PS4Dev/handoff/dirty-cached-color-depth-20261017. Exact native 1525a2094a1b6722129bbf97e036b1f30136831d7d371511e088a00058fe8f30:16 color73.06s +16 dirty depth70.15s +24 paired/conversions/4 aspects/12 compact30.18s +8 subviews29.55s +ordinary color3.84s pass, no validation errors. All16 color targets have S; no fault-injected unwind or whole-core/game/FPS acceptance claimed. Source/runtime/log identities retained. Clean Release23.796s,54 deps/2283 inputs/zero primary paths;18-file manual core H/artifacts-414094c EXE727ba60e14f2c3374438fb790de67d0cf852e744cf91a94b5864978127cc328c; no auto install/select. SESSION_HANDOFF FIRST section authoritative. Next H/NEXT_DEPTH_WRITABLE_RT.md: depth->writable/RT admission/legal ownership/Upload flags and color/shader BEFORE-prewrite caller order. Other geometry/source/ordered/MS/size guards remain.129 rows/nine broader gaps partial; port ACTIVE. Original Vulkan/saves/user processes untouched; older sections history.


## Current dirty cached-depth recreation checkpoint - 2026-10-17

Production **1e2fae2cdf7bc5d2ea3b8c6d2c1bb325516a15fe** connects DISTINCT initialized cached native depth recreation after CPU dirtying, preserving original GpuModified0/1, old native D versus new B tail/S seed, lexicographic resources, HTILE0/new UID/source retirement, ordinary stale-I rejection, current-independent precedence and prewrite ordering. H=F:/PS4Dev/handoff/dirty-cached-depth-recreation-20261017. Final native ddb05761:16 actual original NoS->S/same-aspect-growth GPU cases68.72s + paired24 cases/conversions/4 aspects/12 compact30.01s. Source hashes/native/PDB/runtime/logs retained; prior5 related runs have DIFFERENT exact identity/scope. S-address-only helper cases are not original recreation applicability; final producer uses stencil presence/min-mips1 guards. No fault-injected exception-cleanup acceptance. Clean committed Release23.484s,54 deps/2283 inputs/zero primary paths;18-file manual core H/artifacts-1e2fae2 EXEa5ceec4a840215d614ee38b4ca62837d0eb992db8cdad38b18df994938e8cfc9. No automatic install/select. Whole-core/game/FPS remains unrun; fixture results do not transfer. SESSION_HANDOFF.md FIRST section authoritative. Next cached color->depth class/identity/producer+GPU regression per H/NEXT_CACHED_COLOR_DEPTH.md; writable/RT/geometry/ordered/size gates remain code gaps.129 rows/nine broad gaps not closed; full port ACTIVE. Older sections historical; no game/UI/user-session action.


## Current crossed-depth-resource checkpoint - 2026-10-17

Production **1a35c8b21e27fbdaa751ee276d55d11f9fa267ab** connects original min-mip/min-layer intersection with original source-native stride and target guest offsets/seeds/S/padding. Latest package **a09007a030dab4220ff79c6ce5db0d5a27fcb003** strengthens source readback timing only. H=F:/PS4Dev/handoff/depth-crossed-resources-20261017. Initial4 passes nativec0fa2cc2 have BEFORE-transfer source-readback scope; final2 targeted passes nativeec70028a verify actual source values AFTER transfer with old allocation pinned:16 crossed58.16s+16 growth58.39s. Manifests/identities kept separate. Original equal-MS overlap is unimplemented and differing-aspect buffer copy asserts min-mips1; broader native helper evidence is not original overlap parity. Clean package Release14.068s,54 deps/2283 inputs/zero primary paths;18-file manual core H/artifacts-a09007a EXEc22a33b0e84a2cd7b26518e887f3bdc678fa2750bd8659946ebd038ecf71a830. SESSION_HANDOFF.md FIRST section is authoritative. Next implement distinct DIRTY cached-native recreation source per H/NEXT_DIRTY_DEPTH_RECREATION.md without reviving current I ownership; preserve old native D versus new B seed/S/fences.129 rows/nine broad gaps not closed; full suite/game/FPS remains final. No game/install/select/user-session action; older sections are history.


## Current depth recreation extent checkpoint - 2026-10-17

Production **2ac536b80053e6a4dee6e6a94f1fd9c20625cb72** extends21d1e93: applicable depth/aspect/typed-color-MS recreation retains original lexicographic max resource extents BEFORE caller source/prewrite capture, preserving requested selectors, original source admission and one-mip MS constraints. Two exact GPU tests pass on native9ef33e8a5831f5f57f1a961d930bf4d596361bb9d0b21290ccf16f87d9e8621c:8 aspect/subview plus2 color/MS extent cases, independent native content/new-S/tail/padding/guard/fence checks. H=F:/PS4Dev/handoff/depth-recreation-extents-20261017; manifests/native logs/source snapshots preserve actual scope. Clean committed Release23.361s,54 deps/2283 inputs/zero primary paths;18-file manual core H/artifacts-2ac536b, EXE928cf1c2a081ea4692af5baee99fd7abcfde75a0db2e5e5e32383adb74609c58. SESSION_HANDOFF.md FIRST section is authoritative. Crossed resource counts, dirty-source and other recreation/geometry/source/ordered-MS/size code gaps remain;129 rows/nine broad gaps not closed. No whole-core/game/FPS or install/select/user-session action. Next check pinned copy intersection then adapt CanMapIndependentDepthImage/mapped RetileImageVersion for1mip/3layers ->2mips/1layer with real target-layout readback/seed/S/fences. Prior checkpoints/evidence remain separate; no repeated audits. Full suite/expanded existing-XInput acceptance remain final.


## Current depth-subview checkpoint - 2026-10-17

Production **21d1e93cd51a930e29b65d0ec7a7fd1831bba152** connects fixed-base same-word D16/D32 smaller SRV/DSV parent reuse, requested selectors, dirty allocation extent separate from I validity, full caller/prewrite envelopes and single-sample paired ordered domains. Current-B D-only refresh preserves native S in-place or through fence-retained replacement. Pinned GPU invalidation stays exact-base; interior negative retained.8 GPU subview cases plus four related scoped fixtures pass on native315efd6af2cd67f97e482b1400c5c4447045846dff34f8fdeaf454ee03fa459b; H/NATIVE_VERIFICATION.json gives exact identities/scope. Clean committed Release22.656s,54 deps/2283 inputs/zero primary paths;18-file manual runtime H/artifacts-21d1e93, EXE86538bde3bdf44c9b09e424cd68463fa6df66a485b0d0e234bf879ffb36a14ef. H=F:/PS4Dev/handoff/depth-subview-reuse-20261017. SESSION_HANDOFF.md FIRST section is authoritative; older sections are history. Remaining recreation max-resources, changed geometry/format/source/ordered-MS/64-MiB code gaps remain;129 rows/nine broad gaps not closed. No whole-core/game/FPS acceptance or app install/selection/user-session action. Next implement applicable ResolveDepthOverlap recreation max-resources across storage/caller/prewrite/UploadVersion, preserving lexicographic extents, original admission and D/S seed/lifetime. Full suite/expanded existing-XInput gameplay remain final.


## Current depth overlap admission checkpoint — 2026-10-17

Production9fa2125 restores pinned BlockDim(pitch/block,height/block) and num_bits*samples admission BEFORE depth overlap. Incompatible same-base classes retain old I and obtain a fresh supplied B image, not an implicit I blit. 92f085f corrects only cache-positive fixture mip counts; exact7-test and4-test rebuilt-binary evidence stay separate. Final32 color scenarios:10 eligible cache blits,22 pool controls plus explicit-helper pixel checks;16x hardware variants skipped. Prior e94f886/616fb45 cache-admission parity claim is superseded, not reused. Clean18-file manual core H/artifacts-92f085f, EXE19f74dbe989165609a544bc699c2bc1586d4cadcbedffd0f6596204f098f23ca;54 deps/zero primary paths. H=F:/PS4Dev/handoff/depth-subviews-20261017. SESSION_HANDOFF.md FIRST section and H/NEXT_SUBVIEW_DESIGN.md are authoritative. Smaller depth views are still NOT implemented; next connect parent reuse/descriptor selectors/ordered domains and prove actual GPU contents.129 rows/nine broader gap IDs remain; no whole-core/game/FPS or install/user-session action. Older sections are history.


## Current color -> MS depth checkpoint — 2026-10-17

Production e94f886 connects typed channel-to-sample depth draws, sRGB source views, native base-view/default component semantics, all original sample counts and fence-retained resources; 616fb45 corrects only the growth fixture's single-sample seed-residency policy. Six exact scoped tests on e94f886 pass, including28 new2x/4x/8x cases; four16x hardware cases are skipped, not verified. The rebuilt 616fb45 fixture has its own single corrected16-case growth pass. Spec-derived D16/sRGB precision bounds and immutable failed logs are documented explicitly. Clean committed18-file core H/artifacts-616fb45, EXEe97a7dee5f497655bd7969186f4e459f090b3f92d8e8c737485740c71b59eaf1,54 pinned deps/zero primary inputs. H=F:/PS4Dev/handoff/color-to-ms-depth-20261017. SESSION_HANDOFF.md FIRST section is authoritative.129 implementation/behavior rows/nine broader gap IDs remain; next is cached smaller-view reuse/rebinding and then remaining geometry/source/ordered guards. No whole-core/game/FPS proof or app install/selection/user-session action. Older sections are history.


## Current depth growth / upstream aspect correction — 2026-10-17

Production a4a51e8 CORRECTS the prior52add96 claim: pinned CopyImage AND CopyImageWithBuffer exclude stencil; explicit CopyDepthStencil is separate. Previous converted-S parity claim/oracle is superseded, not silently reused. Production a7e7ea5482e2e39a0ced692dd3780704805604e2 maps independent depth mip/layer growth through source native footprints into TARGET guest offsets, preserving target-only B1/S/padding and deferred seed residency.9 scoped fixtures pass, including16 independent typed/native growth cases and the corrected paired oracle. Clean committed18-file core F:/PS4Dev/handoff/depth-growth-aspect-contract-20261017/artifacts-a7e7ea5; EXE87b52fee29cc16e078e40af3091b47bf7610ff90756a8c61d30c2857cc9f4cb3;54 deps/2283 inputs/zero primary paths. Exact scopes/correction/remaining smaller-view, spatial/repack/size guards are authoritative in SESSION_HANDOFF.md first section.129 implementation/behavior rows/nine broad gaps remain; whole core/game acceptance not_run. No app installation/selection or user/game/session action. Older sections are history.


## Current paired independent ownership/conversion checkpoint — 2026-10-17

Production52add961b02029ee420fc76ef00847f015af64f8 connects both physical D/S ownership planes, shared production acquisition/retirement and native depth/color conversion for exact physical/sample domains.8 scoped fixtures pass:24 new paired conversions with actual Windows guards/CPU faults, strong B and deferred provenance,4 aspect cases,12 compact cases and prior24 captures. Clean committed Release18-file folder F:/PS4Dev/handoff/paired-independent-conversion-20261017/artifacts-52add96; EXE42dd0cc23fd429275ec77759422b24f4bc6c74017be1208aabfc9f485c8b1e9f;54 deps/2283 inputs/zero primary paths. Next production gaps: depth/S resource growth or mapped shifted/clipped geometry, pinned color-channel -> MSAA-depth repacking and remaining ordered alias gates. Full GuestRasterizer/priority-worker/core/game acceptance remains open. SESSION_HANDOFF.md first section is authoritative;129 implementation/verification rows/nine broader gap IDs remain. No app/kernel installation/selection or game/user-session action. Older sections are historical.


## Current transactional guards / compact paired-depth checkpoint — 2026-10-17

Production6f44e15711d052a33c6c2f2ce3c205ea9a29161e connects all-plane PageWatch acquisition/demotion with rollback planning, actual GPU B protection identity and preserved stronger guards;4 scoped fixtures pass. Production2be23d93d2cc1b5bd3b050db4a7364cde065d868 connects paired D16/S8+D32/S8 compact DEPTH-ONLY readback with actual native stride and existing independent resolve/pressure handlers;4 scoped GPU fixtures pass, including12 new compact cases plus unchanged24 native captures. Pinned DownloadImageMemory publishes depth only, NOT S: do not invent S pressure publication. Split physical ownership geometry/admission and applicable paired conversion remain code gaps; whole paired lifecycle/core/game acceptance not_run. Clean committed18-file Release F:/PS4Dev/handoff/compact-paired-depth-download-20261017/artifacts-2be23d9, EXE19c52c0700f9adb3b8804c8ff70174d3608ce011c54ce8249765fabc5a67025f,54 deps/2283 inputs/zero primary paths. SESSION_HANDOFF.md first section is authoritative;129 implementation/verification records/nine broader gaps remain. No app install/selection, game/UI or user-session action. Older sections are history.


## Current separate plane boundary checkpoint — 2026-10-17

Production e384ca5f8fcd78ee811559b0001be238e8276387 connects separate D/S boundary maps, plane enumeration for retained/released watches/faults, all-plane reprotection-before-read and failure-safe proof retention. Actual CPU PageWatch faults and6 related GPU fixtures pass, including24 I/private/I cases rejecting unvalidated S boundaries. This is bounded boundary infrastructure evidence; split independent ownership acquisition/rollback, paired independent conversion and retirement remain explicit code gaps. Clean committed Release18-file folder F:/PS4Dev/handoff/paired-plane-watches-20261017/artifacts-e384ca5, EXE597164fada941c9e38ffad9dd51e50004d0ae3650a8482a6941a40acc562324d;54 dependencies/2283 inputs/zero primary paths. Current whole-core/game acceptance not_run. Prior f0d047a/a2acbc3 captures and all exact evidence remain intact. SESSION_HANDOFF.md first section is authoritative. No automatic kernel installation/selection or user/game/session action. Full port remains active;129 implementation/verification records/nine broader gaps retained. Older sections are history.


## Current native depth/stencil I capture checkpoint — 2026-10-17

Production f0d047a7f48880a7cc6bcd6b08cef00710c2cc26 connects ordinary valid GPU-owned native depth/stencil I -> private D/S buffers -> native I, including MSAA/D16 conversion, prewrite DrawInternal source selection, nonpublication and real fence leases. Followup a2acbc3939d5b44664ddb2d64f66489534320362 exposes the complete private allocation capacity to residency and preserves available seed padding.9 scoped GPU fixtures pass at exact f0d047a binaries; the strengthened24-case full-capacity/held-fence fixture separately passes at exact a2acbc3 binary. Whole core/game/paired PageWatch ownership are not accepted. Primary and separate clean committed Release link; complete18-file manual-folder kernel is F:/PS4Dev/handoff/native-depth-capture-20261017/artifacts-a2acbc3 (EXE SHA256 5f68fb406981e74ab21778c247d5af8a315e14bbe65b969e2294e2cb11d1a609).54 pinned dependencies/2283 inputs/zero primary paths. No automatic app install/selection or game/user-process action. Authoritative continuation and limitations: SESSION_HANDOFF.md first section. Next production work is paired per-plane PageWatch/ownership/boundary proofs/retirement; retain existing split-independent rejection until the whole lifecycle is connected.129 registry rows and nine broader gap IDs remain distinct from verification. Older sections are historical.


## Current paired depth snapshot / portable stencil checkpoint — 2026-10-17

Production `99cb1f467b8af8d19ea95f21343c482247e4095a` connects separate CPU/GPU depth/stencil inputs to UploadVersion and DrawInternal prewrite snapshots, including MSAA. GPU B1 inputs use physical preparation without replaying shader-buffer binding effects; both native input capacities/lifetimes are accounted, and GPU planes never become CPU equality witnesses. GuestMultisample now uploads stencil without optional SV_StencilRef through eight fixed write-mask bit passes (plus rectangle-only zero pass for partial uploads), preserving depth and outside samples; supported hardware keeps the single-pass path. Nine scoped fixtures pass, including24 actual paired cases with no skips on device10de:2c02/PSStencilRef=0 and formerly skipped partial MSAA stencil controls. Whole GuestRasterizer prewrite acceptance, source I-to-private paired capture, independent multi-plane PageWatch/ownership/pressure and final gameplay remain open. Complete primary/clean committed Release built;54 dependencies,2283 inputs, zero primary paths. Runtime `F:/PS4Dev/handoff/paired-depth-snapshot-20261017/artifacts-99cb1f4`, EXE `612bfc73b93fbff7af48bb046a173b61c87c02ae89ad1dceeaade44965509dc1`, PDB `afa2720a0b21bfbfe8509cb03391c974eadb117ecf84f0f957681cf99d1bf055`; proofs H=`F:/PS4Dev/handoff/paired-depth-snapshot-20261017`.129 implementation/behavior rows and nine broader gap IDs remain. SESSION_HANDOFF is authoritative; continue I-plane capture and paired ownership, not another129-group audit. No game/UI/XInput/profiler or auto-install/selection/user-process restart.


## Current single-plane depth / mip-prefix checkpoint — 2026-10-17

Production `36f137329816f19ced7daa0069bca2f798234019` connects independent single-sample D16/D32 depth with resident B, readonly native depth views, ordered formatted retile and compact depth readback/linear retirement. Split stencil/MSAA depth and shader-writable shared depth/color remain explicit gaps. A regression also fixed missing valid smaller-mip I selection in FindIndependentUploadSource; all48 ordered-stage cases pass with unchanged expected values. Eight scoped fixtures pass, including six D16/D32 GPU configurations plus independent native-plane copy oracles, both I/B directions, B0/B1/padding and fence negatives; alias additionally passes with exact core runtime converter. Primary and clean committed Release linked7.58s/20.44s (primary code compilation14.04s beforehand);54 pinned dependencies,2283 inputs, zero primary paths. Complete external runtime `F:/PS4Dev/handoff/independent-depth-plane-20261017/artifacts-36f1373`, EXE `16a7f3050fdab7473cef2a41390e5de01ee49fa675ece2b6e71e0f16cb0b53b3`, PDB `63e65f281ad0db0603b0bb697a27d9394c98a392a4915237dc4d899568b2f49b`. Evidence H=`F:/PS4Dev/handoff/independent-depth-plane-20261017`. RHI proof does not establish GuestRasterizer/PageWatch depth end-to-end or current-core/game/FPS/final-suite acceptance.129 separate implementation/behavior records and nine broad gap IDs remain; SESSION_HANDOFF is authoritative. Next: connected split-stencil/MSAA-depth I/B planes, ownership and retirement; preserve pinned formatted depth-only aspect semantics. No repeated129-group audit, automatic kernel installation/selection or user-process restart.


## Latest readonly sharp / DMA alias proof — 2026-10-17

Test commit9fdd733 executes the pinned readonly SSBO + writable SSBO + ReadConst/DMA live-B contract in standalone and linked production conversion. The input's binding5 NonWritable decoration is checked; the native reflected binding is a live UAV, and readonly sharp and DMA observe the same-dispatch0xdbca0000+lane writes over prior GPU B1=0x22334455. Both fixture and exact core runtime converters pass the complete scoped DMA test. Evidence F:/PS4Dev/handoff/readonly-dma-alias-20261017/NATIVE_VERIFICATION_9fdd733.json. The prior queued actual reflected SRV/DMA conflict is covered for the production path: NativeOptions already enables native_storage_buffer_uavs, standalone/linked conversions apply it and shader cache keys include it. The incompatible custom SRV/UAV guard and original negative controls remain. No new fallback/snapshot is needed or permitted for true same-command aliases. Production core and16-case pressure evidence remain2e01aa6; the new commit changes only the fixture. Nine broader gap IDs/129 rows remain. **Next production mechanism: ordered opaque depth/stencil I/B transfers**, starting at guest_texture_cache.cpp::UploadVersion and guest_rasterizer.cpp::SetIndependentVersionTransfer callback. Keep existing exact-base/reference residency semantics, separate planes, masks, CPU B0 and ordering/lifetime. SESSION_HANDOFF remains authoritative.


## Current persistent buffer backing checkpoint — 2026-10-17

Production33a1e87/1ef3129/`2e01aa67acb768a1bfa58068a058e5912c4c05ae` retains mandatory GPU-derived B1 and selected native generations through cache pressure, matching pinned Vulkan persistent resident backing. Reconstructible entries alone are evicted; callback-safe victims are elected in one sorted pass. Soft cache targets admit mandatory guest contents; actual new native commitment still obeys freshly queried adapter availability and the existing reserve. Placed heap growth uses remaining budget and compacts against the current page target. Sixteen scoped GPU/readback cases plus alias/DMA with the exact core runtime converter pass. Primary and clean committed Release linked15.77s/23.09s;54 pinned dependencies,2283 compile inputs, zero primary-checkout paths. Manual-folder runtime `F:/PS4Dev/handoff/preserved-buffer-budget-20261017/artifacts-2e01aa6`; EXE `c235fad68b7cd572658514d3c154007daf7878729d68229e315f09d05e579811`, PDB `c4f20f688c4f13e092c1388eed8f99933f5806037b6db3ce6d9c59b92cac3aed`. Proofs: `F:/PS4Dev/handoff/preserved-buffer-budget-20261017/RELEASE_PROOF_2e01aa6.json` and `NATIVE_VERIFICATION_2e01aa6-final.json`. No current-core/game/FPS acceptance or auto-install/selection.129 independent implementation/behavior records and nine broader gap IDs remain; SESSION_HANDOFF is authoritative. Next: settle the actual reflected SRV/DMA-UAV production path (production converters already unify SSBOs), then ordered opaque depth/stencil I/B transfers. Do not repeat the global audit or completed ownership/DMA/disjoint/pressure work.


## Current disjoint native buffer-view checkpoint — 2026-10-17

Production `a39144a72db03df98836296d64d8c319afc6d01e` preserves one complete canonical B for disjoint native SRV/UAV byte ranges sharing preceding GPU-only/owned backing. Readonly native SRVs receive leased GPU copies refreshed at final binding; original UAV writes, CPU baselines, PageWatch, dirty ranges and semantic residency stay in canonical B. True overlapping native SRV/UAV ranges retain compiler-unification requirements; production standalone/linked guest SSBO conversion already unifies them. Eleven scoped GPU/readback cases plus alias/DMA with the exact core runtime converter pass. Complete primary and separate clean Release linked;54 pinned dependencies,2283 compile inputs, zero primary checkout inputs. Current external manual-folder runtime `F:/PS4Dev/handoff/disjoint-buffer-views-20261017/artifacts-a39144a`, EXE `d7f6dc266b0e30defb7d1bc4a8e71fb321d9208b6ed85ad50ab252a37f4d56c1`, PDB `e0478710fc8f00d52e3d1d9e5ee2fa9389fe3edf0ab12a6dff95bc4e2c6e92e6`. H=`F:/PS4Dev/handoff/disjoint-buffer-views-20261017`, RELEASE_PROOF_a39144a.json/NATIVE_VERIFICATION.json. No current-core/game/FPS acceptance or automatic installation/selection. Full goal remains unfinished:129 implementation/behavior rows, nine broader gap IDs. Next: inactive GPU-only B pressure backing in Find/PrepareShaderBuffersImpl; preserve B1, CPU B0/store proofs and actual fence leases. SESSION_HANDOFF.md is authoritative; no repeated global audit or completed alias/DMA investigation.


## Current owned-envelope / pending-DMA checkpoint — 2026-10-17

Authoritative continuation is the newest SESSION_HANDOFF.md section. Production `51ec0979800b23ae647c0d779714085bf251ea8c` fixes the formerly failing large-DMA no-submit regression: protected GPU-owned B grows through ordered GPU copies, inherited exact dirty ranges and acquired gap guards, retaining old guards/resources and separate CPU snapshots. Unprotected sources, actual CPU-store markers and rejected gap acquisition retain the necessary reconciliation path. No wait/exception/assertion is suppressed. RunLargePacket already had ownership callbacks; the earlier missing-callback hypothesis was disproved by source inspection and captured Finish stacks.

Eleven scoped GPU/readback cases pass, including the unchanged native_copy_dirty_ranges fence/data/source/padding/neighbor/GDS oracle. Extended actual PageWatch controls prove NOACCESS/pending writes, explicit-read visibility, joining two owned B versions, gap rejection, throwing acquisition/retry, new-guard rollback and a CPU winner in provisional backing. Original expected B0/B1/dirty-byte values remain; owned tests now request CPU visibility explicitly instead of requiring premature reconciliation. Related DMA shader, independent I/B, lifetime, graphics DMA/IA and final-view cases pass; native_gpu_page_ownership passes35.96s. Exact runtime converter DMA additionally passes; regular fixture/runtime converter identities remain distinct. NATIVE_VERIFICATION.json contains per-binary archives. No whole-core/game/FPS acceptance is inferred.

Primary Release and separate clean committed Release compiled/linked (16.77s/26.74s),54 pinned dependencies,2283 compile inputs, zero primary checkout paths. Complete manual-folder runtime `F:/PS4Dev/handoff/dma-copy-pending-20261017/artifacts-51ec097`, EXE `1dcac40a16056641481eb4aba137f4e9cce36dbd31b37c6dad4993099d275e16`, PDB `897981a79c38b9648e7c258a10ec3b8be5b0049508ea72c624d8efc192076970`; RELEASE_PROOF_51ec097.json. Evidence H=`F:/PS4Dev/handoff/dma-copy-pending-20261017`; all earlier builds/failures/baselines retained. No game/app/UI/XInput/profiler execution, automatic installation/selection or user-process restart.

Full goal remains unfinished:129 implementation/behavior rows, nine broader gap IDs. Next production source action: clipped GPU-only envelopes at RematerializeBufferEnvelope and incompatible simultaneous native SRV/UAV classes in PrepareShaderBuffersImpl, using the pinned one-arena contents/order contract. Inactive GPU-only B pressure storage, actual reflected SRV/DMA-UAV conflict, ordered opaque I/B, converted/shifted/clipped domains and other IDs remain. Do not repeat the now-fixed pending-DMA investigation, original crash replay or129-group audit. Final suite/expanded XInput gameplay and real-frame FPS baseline comparison remain final.


## Current initial-DMA / private-index / GPU-fill checkpoint — 2026-10-17

Authoritative continuation: newest SESSION_HANDOFF.md section. Production fcb67e1 separates raw DMA physical backing from real shader stage requests and adds one-shot/failure-safe ResourceBindings::Prepare before IA selection;2d5e107 connects original clean <=16KiB private physical CPU index streams and late resident auxiliary selection;7a59dd26410aa2ba29b38d8fd1845af8b90c44fa routes GPU FillBuffer through writable preserving preparation. Original B0/B1/I contents, exact writes, semantic residency, read states and fence leases remain distinct. No game/app/UI/XInput/profiler execution, automatic installation/selection or user-process restart occurred.

At2d5e107,12 scoped native GPU/readback cases passed; binaries/logs/compiler inputs are archived in `F:/PS4Dev/handoff/initial-physical-dma-20261017/native-fixtures-2d5e107`, NATIVE_VERIFICATION_2d5e107.json. At7a59dd2, the extended envelope/watch/fill case and DMA shader case pass; **native_copy_dirty_ranges fails** on `DMA submitted the pending guest list` in RunLargePacket. Its earlier exact-write/overlap/retry cases pass. Preserve this negative fence oracle. No FillBuffer call occurs in that failing scenario; fixture ownership/preparation and envelope-growth submission boundaries still need isolation. No claim that all checks pass. Both phases additionally pass DMA with their exact core runtime converter (different from the regular fixture converter); identities are separate. This is mechanism evidence, not whole-core/game/FPS acceptance.

Primary and separate clean committed Release compiled/linked at7a59dd2;54 pinned dependencies,2283 compile inputs, zero primary checkout inputs, clean source tree. Complete external runtime `F:/PS4Dev/handoff/initial-physical-dma-20261017/artifacts-7a59dd2`; EXE `d36c1bc70de83680065959280ff186234a633b6bb7d392c6516e2913fdc0ce5d`, PDB `b6e24ac5d5e8e7add719f4ad01ce5a0432dd2b942cef78beefa27f514f28d7b3`. RELEASE_PROOF_7a59dd2.json and NATIVE_VERIFICATION_7a59dd2.json preserve exact identities and the failure. Old fcb67e1/2d5e107 runtimes and all older artifacts remain. Never auto-install/select cores.

Full goal remains unfinished:129 implementation/behavior rows, nine broader gap IDs. Next production task is the large DMA pending-list regression in CopyBufferBatchImpl/PrepareShaderBuffersImpl/RematerializeBufferEnvelope; then remaining mixed native SRV/UAV splitting, inactive GPU-only B pressure backing, ordered opaque I/B and converted/shifted/clipped transfer domains. Do not restart the129-group audit, original crash replay, or completed raw-DMA/private-index mechanism. Full suite and expanded final XInput/game/real-frame baseline comparison remain final.


## Current final-buffer / physical-DMA checkpoint — 2026-10-17

This section supersedes older current/next statements; SESSION_HANDOFF.md remains authoritative. Source9791682 selects native buffer descriptors only after attachment preparation and validates original ranges against explicit full-copy allocation ancestry. Source0d5b4693e0784bdc7e01aaa9db2d6b2275851367 materializes missing legal64KiB/4KiB DMA backing from physical B before that final selection, preserving independent I and preceding GPU B1. Existing initialized backing is not rescanned here; BindResident retains its strict guard and single post-stage synchronization. Stage effects/private streams/bounds/CPU visibility/fence leases remain.

Nine related native fixtures passed (envelope/ownership/generations, DMA, two new graphics late-growth/private-snapshot controls, readonly and UAV indexed draw, independent I/B, buffer-view lifetime and scheduler lifetime). DMA additionally passed with the exact runtime Mesa converter; its normal fixture converter has a distinct hash despite identical pinned source/patch metadata, so identities/results are recorded separately. Complete primary and separate clean Release compiled/linked;54 dependencies,2283 compile inputs, zero primary-source inputs in clean build. Runtime `F:/PS4Dev/handoff/attachment-backing-20261017/artifacts-0d5b469`, EXE `abfa3efbf468e2e6ce24e001e172e4842e602e0f818da658ff07c9daa98a151e`, PDB `4a83d098e0106176d831a306c3bafb4879145e7b2a592a1901e57d08060fe4e0`; exact core runtime/game/FPS acceptance not_run. Evidence `F:/PS4Dev/handoff/attachment-backing-20261017`, RELEASE_PROOF_0d5b469.json and NATIVE_VERIFICATION_0d5b469.json. No game/app/UI/XInput/profiler run, core auto-install/selection or user-process restart this continuation.

Full port remains unfinished:129 implementation/behavior records, nine gap IDs. Next: initial opaque DMA request batches still call the legacy CPU image resolution path before ordinary shader preparation. Separate their physical B intent without dropping real shader-buffer BindBuffers effects, then adapt ordered opaque depth/stencil I/B domains. Direct Find, mixed SRV/UAV split, pressure B backing, private IA/auxiliary and other existing gap IDs remain. Do not redo the completed final-view or late physical-materialization mechanism, original crash replay or129-group audit.


## Current crash / GPU-contract checkpoint — 2026-10-17

Authoritative current state: SESSION_HANDOFF.md, crash evidence `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z`. This section supersedes older current/next claims and the old all-tests-deferred policy. Narrow regression and GPU/readback checks are now authorized; full suite and expanded gameplay acceptance remain final. No per-commit game launches, automatic core installation/selection, original Vulkan/save changes or user-app restart.

Production935fb69 restores existing prepared writable B ownership after prewrite reads/Finish, without replacing selected resources or bypassing the ownership assertion; fatal messages synchronously reach stderr/log and the launcher captures per-launch streams/manifest/hex exit/report. db0812c preserves signed integer clear bits through compatible UINT views and explicit UAV ordering. Production8fb9c79 reserves complete legal DMA backing for late image sources in ordered common native I/B domains before final views, preserving original semantic publication phases.

The original exactd01969e replay trapped in ExecuteGuestBufferCompute: `Independent image version lost its writable buffer ownership before execution`, followed by Liverpool coroutine UNREACHABLE -> int3. Original EXE/PDB/runtime, profile copy, logs, exception/context/stacks and dump are preserved. The exact935fb69 clean binary survived a120s bounded diagnostic startup exposure without this exception; its owned tree was stopped at deadline. No world/input/pixel/FPS acceptance, and that result is not transferred to8fb9c79.

Six exact native fixture binaries are preserved with PASS records in `narrow-regressions-8fb9c79.json`: envelope/semantic B, DMA (including late image full/partial backing), independent I/B, buffer view lifetime, scheduler lifetime and PageWatch/GPU ownership. The latter also covers typed/converted image clears and gated resource lifetime;34.08s, not the former20s budget. Normalized half ties have tightly specified alternatives; original expected bytes/negative checks/padding remain. Clean committed Release at8fb9c79ea55c8a71d5befebffa0797de98f59523,54 pinned dependencies,2283 compile inputs, zero primary checkout inputs. Full runtime `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z/artifacts-8fb9c79`; EXE SHA256 `3e822bf2a7c46fcc8c8c7dc242c36df4ba895f65ce8dbd7ae03be1032f5a0fab`, PDB `765be1db16d597c59bd40f376e511e8a763d13dd0fd4cdbe18893dfb8c6a8e09`. This exact core is not game-tested or automatically installed.

Full port is unfinished:129 independent implementation/behavior records, nine gap IDs retained. Next: extend pre-view source-footprint/backing planning beyond ordered common I/B domains to attachment-only/converted/BC/opaque source receipts; inspect GuestRasterizer::Draw submit_replay and ExecuteGuestResources before changing final views. Keep existing native identity/SRV-UAV/ownership guards. Direct Find/mixed-class/pressure B backing and private IA/auxiliary unification remain. App DLL was locked by the active user process: old DLL/PDB pair remains installed, new diagnostics pair staged; no process restart. Apply script/details in SESSION_HANDOFF.md.


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


Pinned reference: `e0a466753a1aacbc8db8253be685bf7ab4d9daae`. All 129 groups have committed implementation/integration sources.
Release core built from clean commit `72569a531e1c262fc828e80d56400f7f1507d75a`. Desktop C# and prepared fixtures were retained, not executed by this core build.

`connected` records integration, not exhaustive semantic closure. An empty change list means no specific remaining production change established for that group.
Historical review notes remain in contracts.json. Unrun behavior is never counted as missing production implementation.
The known backlog below retains real native guards; this registry does not certify the whole port complete.

## Concrete remaining production changes

| ID | Upstream symbol → native symbol | Missing semantics |
|---|---|---|
| color-attachment-feedback | Vulkan::Rasterizer::BindResources / BeginRendering → NativeDX12::GuestRasterizer::Draw / ExecuteGuestResources | Shared color attachment and shader feedback through the same physical storage; prewrite copies alone do not provide ordered per-fragment feedback. |
| formatted-alias-ordering | Vulkan::BufferCache::EnsureResident / SynchronizeMemoryFromImage / Rasterizer::BindResources / TextureCache::ResolveOverlap / ExpandImage -> NativeDX12::Rasterizer::PrepareShaderBuffers / RematerializeBufferEnvelope / Find / GuestDmaMemory::BindResident / ExecuteGuestResources | remaining depth views with changed base geometry/numeric storage and remaining recreation cases (dirty cached source representations outside connected direct fixed-base single-sample depth/color-to-depth mapping and non-direct16/32 render-target/storage depth-to-color); spatial/tile/rebased or clipped mappings and packed growth; color-to-MS-depth source representations needing raw/expanded/BC interpretation rather than an available native typed float SRV; remaining ordered depth/MSAA alias gates and independent snapshots above current64-MiB guards; converted or BC word-view stage domains without a common native image allocation, shifted bases, different physical geometries and clipped transfers outside the connected reader-arena proof remain. Shader-buffer residency publishes in original bind order and formatted I-to-B copies affect only already resident pages. Image prewarm now retains version-specific deferred B-source effects; actual image binding consumes them, including inherited independent conversion requirements and prewrite GPU/CPU/depth/stencil sources. Clean small shader streams retain private full DWORD envelopes; host/domain reservations and private I conversions do not publish. Compatible batches preserve preceding GPU B and CPU snapshots/store markers. Fully materialized late DMA publication and invalid I pressure discard remain connected. Valid GPU-modified tiled I pressure retention follows pinned GC and is not another port gap. Formatted readers retain the pinned exact-base assertion. Narrow GPU subcontracts have executed evidence; final broad/game acceptance remains deferred. Capacity-only private depth plans and actual-current-source DB binding are now connected/narrowly GPU-verified; actual GCN shader bindings prove old native D is not prematurely published into preceding formatted B reads. Late source growth beyond the reserved envelope remains a concrete code gap. Recreated RT native_only CMASK guards for unsupported pending plan/format/ownership domains remain concrete code gaps; full member/decoder runtime coverage is separate. Direct fixed-base1x format2/4 depth-to-storage is now connected through original actual bind order; narrow native float16/32 and real R32float GCN clean/dirty/ordered tests pass at641f6b9. Direct classes3/5/9(UNORM/UINT)/10 are now connected at e1eedf9, with separate native RT/UAV word proofs and full renderer RGBA8_UNORM CMASK clear/draw verification. Explicit old-exclusive-guard transfer fixes later independent retirement. Direct finite6 cached recreation and full native CMASK are connected/narrowly checked at fd8e5f6; permuted or expanded16..19 still need source-native codec adaptation before admission; other already connected numeric/component CMASK modes need checks rather than presumed missing code. Remaining unsupported native_only plan/format/ownership domains stay explicit. |
| depth-color-shared-storage | Vulkan::TextureCache::FindDepthTarget / FindRenderTarget / CopyColorAndDepth → NativeDX12::GuestRasterizer::Draw | Simultaneous overlapping depth/color attachments and buffer/HTILE depth-clear writers need shared storage/ownership; they are explicitly rejected. Opaque depth/stencil raw-DMA overlap retains the legacy physical-backing/exclusive-image path; ordered independent I/B transfer for this domain is unfinished. Same-plane depth write feedback is not newly required: the pinned renderer explicitly excludes depth feedback. |
| signed-packed10-attachment | Vulkan::SurfaceFormat / GraphicsPipeline / Rasterizer::UpdateDynamicState -> NativeDX12::LayoutGuestTexture / BuildGuestGraphicsState / EmitOrderedColor | Connected for sample1/2/4/8 packed-SNORM attachment quantization, blending, all represented logic operations and independent masks; focused GPU verified at e132726. Final game acceptance remains open; no concrete production-code gap remains in this group. |
| packed19-msaa-attachment | Vulkan::SurfaceFormat / GraphicsPipeline → NativeDX12::LayoutGuestTexture / GuestTextureCache::UploadVersion | Connected through the native B4 path and focused GPU-verified on an AMD adapter where the pinned Vulkan backend exposes B4 attachment support. The current NVIDIA adapter exposes it in neither API; no concrete pinned-contract production gap remains. Final game acceptance stays separate. |
| polygon-point-fill | Vulkan::Rasterizer::UpdateDynamicState → NativeDX12::BuildGuestGraphicsState | Polygon-mode point fill must preserve polygon clipping and point rasterization; ordinary VS/GS point primitives already use implemented expansion/capture. |
| mixed-blend-constants | Vulkan::GraphicsPipeline / Rasterizer::UpdateDynamicState → NativeDX12::BuildGuestGraphicsState | Simultaneous constant-color and constant-alpha RGB blend factors on adapters without native ALPHA_FACTOR require an exact fallback; native supporting adapters are handled. |
| other-vendor-clock | Shader::Backend::SPIRV::EmitMemtime → Shader::Backend::SPIRV::EmitMemtime / NativeDX12 shader clock selection | Hardware S_MEMTIME integration for adapters outside NVIDIA NVAPI and AMD AGS, when their pinned Vulkan path exposes the corresponding clock capability. Never substitute a constant or CPU time. |
| point-capture-stage-domain | Vulkan::PipelineCache::GetGraphicsPipeline / Shader::Backend::SPIRV::EmitEmitVertex → NativeDX12::GuestShaderFrontend::BuildPointCaptureGraphics | Connected prerequisites: owned internal SO layout, GPU count-to-indirect draw, buffered GS attribute reads, separate generated-primitive capture address and original patch ID (df9400e; CPU compilation verified, GPU NOT_RUN). dda2004 adds ordered uint4 DS/ES-input serialization, padded record ABI, reflected SO layout and prepass stage construction (CPU linkage verified). e3feab7 adds reusable bounded stream arenas, SO binding after ownership callbacks, state/lifetime retention and GPU arguments (compiled; GPU NOT_RUN). d78286a constructs an original-GS-only consumer without repeating original producer stages; fixed-tessellation end-to-end GPU fixtures are prepared/compiled, NOT_RUN. 864be3f adds complete primitive-aligned output-capture partitions and GPU-argument Replay scheduling (CPU boundaries verified, GPU NOT_RUN). 302aec1/835b047 connect fixed auxiliary capture to GuestRasterizer with checked bounds, live input payloads, one indirect GS/output arena and one PS replay (CPU automatic RectList linkage verified; GPU NOT_RUN). a2c0c71 shares normalized consumer topology and prepares native automatic RectList frontend-chain GPU fixtures (CPU verified, hardware NOT_RUN). a4771ef connects scoped attachment retention and first-only initialization; split-content fixtures compiled, GPU NOT_RUN. ca59465 freezes color feedback; 82cb259 adds opt-in prepared descriptor forks; 1460b0b fixes t1/space33 stream SRV guest-metadata rejection (CPU classification/shader checks PASS, GPU NOT_RUN). c60debe adds cross-layout descriptor projection and filters inactive dependencies (CPU checks PASS; GPU NOT_RUN). ab449e4 adds merged GS/PS logical preparation/completion, wired in prepared partition fixture (CPU checks PASS; GPU NOT_RUN). ad38ee6 adds cached full source/GS/PS context in the prepared fixture;1559575 preserves constant contents across arena retirement (compiled, GPU NOT_RUN). 0990d55 connects the fixed auxiliary partition graph to GuestRasterizer with common storage completion, first-only attachment/MSAA preparation, immutable feedback, one reusable output arena and native probe aggregation (implementation connected; compiled; GPU/game NOT_RUN). Prepared indexed/MSAA/alias/lifetime acceptance remains separate. b0e875b adds a conservative complete arbitrary-tessellation source bound (CPU boundaries PASS, not activated). 0252bc0 connects original LsHsEsGs point capture with decoded topology/control count;40d9138 uses GPU-sized clear/indirect replay (implementation connected; CPU metadata/auxiliary shaders checked; original guest HS/DS compile initially NOT_EXERCISED; b9f07e3 now verifies real GCN LS/HS/DS with live output-CP reads and signed/validated source/GS/PS DXIL, and dispatches clear groups from actual GPU counts. Clean Release PASS; prepared GPU guard/reuse/lifetime and CS-invocation oracle NOT_RUN; game NOT_RUN). fd7659b removes unconditional CPU indirect queue drain and verifies effective-count ABI/address/immutable snapshot bounds (CPU PASS, clean Release PASS); prepared GPU argument coherence NOT_RUN. adff2d5 connects low-level native indexed/nonindexed multi-command ExecuteIndirect with optional GPU count and stride/extent/lifetime contracts (CPU checks/clean Release PASS; prepared GPU cases NOT_RUN). bebcc25 adds opt-in root b0/space31, cross-layout descriptor fork/address preservation, per-command CBV-address prefixes and fallback restoration (CPU prefix/host shader checks and clean Release PASS; prepared GPU projection/content/lifetime/restoration NOT_RUN). 8dc9363/c1dfc32 implement actual-GPU-count dispatch and complete native command/runtime packing, preserving full viewport template and restoring prepared input states (CPU size checks/clean Release PASS; prepared exact GPU contents/order/lifetime NOT_RUN). 602bb65 connects root-runtime profile identity through guest cache keys, replacement/rollback records and persisted warmup version6 (compile/link and clean Release PASS; prepared four-profile GPU reload/preload oracle NOT_RUN). 316dffb connects eligible guest native batches with authoritative prepared B borrowing, GPU pack/root runtime, dynamic index envelope and one common attachment-feedback/resource scope (CPU reflection/bounds and clean Release PASS; prepared B-to-pack GPU bridge compiled NOT_RUN; actual guest-entry/game NOT_RUN). Remaining production: complete large-source SO capacity/streaming, active-part CPU scheduling, GPU restart-aware/native-multidraw index expansion (ordinary and capture no-restart indexed fan/polygon/loop/quad-strip connected825b5ec/f00a239/e8dbe93, GPU/actual capture NOT_RUN), fixed-marker list restart and large-batch coverage (swap and custom-marker restart are DX12 extras, not implemented upstream paths); complete CPU fallback retains those operations. Shared batch contents/order/lifetime and game acceptance remain unverified. Pinned exclusions confirmed: generalized dynamic GS ring-load offsets (original immediate-only .U32) and EsGs QuadList GS input (original unsupported topology); plain non-GS QuadList remains in scope. On-chip geometry and guest stream output remain pinned exclusions. |

## All 129 groups

Only concrete change IDs from the table above are listed. `—` means none established, not runtime PASS.

| # | Group | Implementation | Remaining production changes | Behavior |
|---|---|---|---|---|
| 1 | common-pm4-completion-and-backing-writes | connected | — | not_run |
| 2 | native-texture-binding-lifetime | connected | — | not_run |
| 3 | native-image-depth-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, depth-color-shared-storage | not_run |
| 4 | native-readonly-depth-buffer-prewrite-snapshots | connected_with_known_gaps | depth-color-shared-storage | not_run |
| 5 | native-gpu-buffer-image-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, formatted-alias-ordering | partial_gpu_verified |
| 6 | native-anisotropic-manual-storage-filtering | connected | — | not_run |
| 7 | native-buffer-image-prewrite-snapshots | connected_with_known_gaps | color-attachment-feedback, formatted-alias-ordering, depth-color-shared-storage | partial_gpu_verified |
| 8 | native-unit-anisotropy-storage-filtering | connected | — | not_run |
| 9 | native-r32-rgba8-shader-format-reinterpretation | connected_with_known_gaps | color-attachment-feedback | not_run |
| 10 | native-integer-nearest-image-sampling | connected | — | not_run |
| 11 | native-packed10-coherent-numeric-shader-views | connected_with_known_gaps | color-attachment-feedback | not_run |
| 12 | native-graphics-device-settings | connected | — | not_run |
| 13 | native-gpu-debug-markers | connected | — | not_run |
| 14 | native-signed-packed-sampled-textures | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 15 | native-packed10-storage | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 16 | native-signed-packed-integer-color | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 17 | native-signed-packed-backing-conversion | connected_with_known_gaps | color-attachment-feedback | not_run |
| 18 | native-packed10-disjoint-shader-views | connected_with_known_gaps | color-attachment-feedback | not_run |
| 19 | common-sysmodule-id-validation | connected | — | not_run |
| 20 | common-game-path-discovery | connected | — | not_run |
| 21 | common-kernel-network-time | connected | — | not_run |
| 22 | common-amdgpu-format-tables | connected | — | not_run |
| 23 | native-shared-color-attachment-layers | connected_with_known_gaps | color-attachment-feedback | not_run |
| 24 | native-cp-buffer-copy | connected | — | not_run |
| 25 | native-packed-float-bit-transfers | connected | — | not_run |
| 26 | common-original-videoout-cadence | connected | — | partial_gpu_verified |
| 27 | common-windows-mapping-protections | connected | — | not_run |
| 28 | common-gcn-arithmetic-and-subgroup-memory-ordering | connected | — | not_run |
| 29 | immutable-reference-gpu-completion-capture | connected | — | not_run |
| 30 | native-bc-volume-and-extent | connected | — | not_run |
| 31 | desktop-core-runtime-identity | connected | — | not_run |
| 32 | amd-layout | connected | — | not_run |
| 33 | guest-tiling-transfers | connected | — | not_run |
| 34 | guest-multisample | connected | — | partial_gpu_verified |
| 35 | native-render-state-and-rasterizer-discard | connected_with_known_gaps | polygon-point-fill, mixed-blend-constants | not_run |
| 36 | native-compute-wave-size | connected | — | not_run |
| 37 | guest-hardware-clock | connected_with_known_gaps | other-vendor-clock | not_run |
| 38 | guest-surface-metadata | connected_with_known_gaps | depth-color-shared-storage | not_run |
| 39 | host-fsr-and-post-process | connected | — | not_run |
| 40 | native-screenshot-capture | connected | — | not_run |
| 41 | native-rgb96-storage | connected | — | not_run |
| 42 | native-imgui-display-and-lifetime | connected | — | not_run |
| 43 | common-cpu-and-mp3 | connected | — | not_run |
| 44 | native-big-picture-launcher | connected | — | not_run |
| 45 | native-pipeline-preload | connected | — | not_run |
| 46 | native-packed16-images | connected_with_known_gaps | color-attachment-feedback | partial_gpu_verified |
| 47 | native-shader-debug-and-reload | connected | — | not_run |
| 48 | native-buffer-extents-and-index-bounds | connected | — | not_run |
| 49 | native-dma-page-table-growth | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 50 | native-depth-stencil-shader-plane-access | connected_with_known_gaps | depth-color-shared-storage | partial_gpu_verified |
| 51 | native-depth-shader-subresource-access | connected_with_known_gaps | color-attachment-feedback, depth-color-shared-storage | partial_gpu_verified |
| 52 | native-independent-buffer-image-versions | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 53 | native-independent-formatted-buffer-sync | connected_with_known_gaps | formatted-alias-ordering | partial_gpu_verified |
| 54 | native-formatted-metadata-arena-synthesis | connected | — | not_run |
| 55 | native-independent-r32-mip-chains | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 56 | native-independent-r32-array-layers | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 57 | native-independent-r32-vector-formats | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 58 | native-independent-r32-volume-slices | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 59 | native-independent-rgb32-native-expansion | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 60 | native-independent-rgba8-normalized-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 61 | native-independent-rg16-rgba16-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 62 | native-independent-formatted-buffer-larger-extent | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 63 | native-independent-formatted-buffer-short-views | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 64 | native-independent-small-integer-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 65 | native-independent-subdword-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 66 | native-independent-srgb-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 67 | native-independent-packed10-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 68 | native-independent-packed16-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 69 | native-independent-r11-packed-float-image-versions | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 70 | native-independent-logical-partial-buffer-writes | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 71 | native-mixed-page-buffer-ownership | connected | — | partial_gpu_verified |
| 72 | native-exact-subdword-buffer-write-ranges | connected_with_known_gaps | formatted-alias-ordering | not_run |
| 73 | native-actual-cpu-store-formatted-buffer-pages | connected | — | partial_gpu_verified |
| 74 | native-zero-clear-original-image-domain | connected | — | not_run |
| 75 | native-image-clear-complete-subresources-padding | connected | — | partial_gpu_verified |
| 76 | native-nonzero-wide-storage-image-clears | connected | — | partial_gpu_verified |
| 77 | native-nonzero-typed-narrow-image-clears | connected | — | partial_gpu_verified |
| 78 | native-raw-rgba8-converted-image-clears | connected | — | not_run |
| 79 | native-packed16-sampled-volumes | connected | — | not_run |
| 80 | native-packed16-nonzero-image-clears | connected | — | not_run |
| 81 | native-packed10-converted-image-clears | connected_with_known_gaps | signed-packed10-attachment | not_run |
| 82 | native-srgb-image-clears | connected | — | partial_gpu_verified |
| 83 | native-readonly-image-clear-promotion | connected | — | not_run |
| 84 | native-compact-linear-image-download | connected | — | not_run |
| 85 | native-queued-independent-image-downloads | connected | — | not_run |
| 86 | native-priority-retirement-drain | connected | — | not_run |
| 87 | native-priority-linear-pressure-commit | connected | — | not_run |
| 88 | native-rgb32-and-wide-readonly-clears | connected | — | not_run |
| 89 | native-expanded-packed-image-clears | connected | — | not_run |
| 90 | native-packed16-sampled-one-dimensional | connected | — | not_run |
| 91 | native-priority-inflight-backing-generation | connected | — | not_run |
| 92 | native-nonblocking-linear-pressure-source-residency | connected | — | partial_gpu_verified |
| 93 | native-queued-linear-color-attachment-domains | connected | — | not_run |
| 94 | native-compact-exact-expanded-packed-downloads | connected | — | not_run |
| 95 | native-source-residency-format-and-sample-admission | connected | — | not_run |
| 96 | native-image-source-gpu-modification-selection | connected | — | not_run |
| 97 | native-r32-float-multisample-image-clears | connected | — | not_run |
| 98 | native-image-clear-physical-channel-swizzles | connected | — | not_run |
| 99 | native-exact-expanded-pressure-source-residency | connected | — | not_run |
| 100 | native-multisample-storage-uav-image-clears | connected | — | not_run |
| 101 | native-exported-vs-point-size-geometry | connected_with_known_gaps | polygon-point-fill | not_run |
| 102 | native-sized-point-emulated-clip-forwarding | connected | — | not_run |
| 103 | native-sized-point-render-target-layer-forwarding | connected | — | not_run |
| 104 | native-sized-point-dynamic-viewport-extents | connected | — | not_run |
| 105 | native-guest-geometry-point-expansion | connected_with_known_gaps | polygon-point-fill, point-capture-stage-domain | not_run |
| 106 | native-geometry-cache-borrowed-copy-span-elimination | connected | — | not_run |
| 107 | native-point-misc-valid-varying-slot | connected | — | not_run |
| 108 | native-late-per-viewport-homogeneous-conversion | connected | — | not_run |
| 109 | native-point-window-transform-and-viewport-edge-footprint | connected_with_known_gaps | polygon-point-fill | not_run |
| 110 | native-point-parameter-liveness-and-sparse-clip-interface | connected | — | not_run |
| 111 | native-gcn-point-geometry-live-output-budget | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 112 | native-acyclic-point-gs-emission-bound | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 113 | native-unit-point-gs-hardware-emission | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 114 | native-point-gs-consumer-component-widths | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 115 | native-point-gs-sparse-component-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 116 | native-point-gs-aliased-fragment-input-declarations | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 117 | native-point-gs-qualified-parameter-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 118 | native-clip-disabled-viewport-transforms | connected | — | not_run |
| 119 | native-point-center-private-distances | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 120 | native-linked-clip-input-signatures | connected | — | not_run |
| 121 | native-point-dense-distance-interface | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 122 | native-unit-point-dense-distance-outputs | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 123 | native-point-vs-qualified-parameter-transport | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 124 | native-point-cross-parameter-varying-packing | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 125 | native-point-ordered-capture-producer | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 126 | native-point-captured-fragment-attribute-fetch | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 127 | native-point-capture-replay-submission | connected_with_known_gaps | point-capture-stage-domain | not_run |
| 128 | native-placed-color-transfer-initialization | connected | — | partial_gpu_verified |
| 129 | native-present-modes | connected | — | not_run |

## Prepared checks (independent of remaining code changes)

Descriptions retain prior evidence wording; this integration session ran no test suite or game.

### 1. common-pm4-completion-and-backing-writes

Split physical/VMA ranges, exact map end, unsupported/gapped/empty backing, arithmetic overflow and physical alias ordering fixtures prepared but not compiled/run.
`native-port/backing_write_plan_test.cpp`

### 2. native-texture-binding-lifetime

The existing exact tiled/mip/layer GPU texture fixture additionally rejects invalidation after the actual GPU fence has completed while either shared reference of a binding lease remains, then permits it after the final release. New full-core image/depth alias modes also exercise descriptor bindings across ownership and attachment uploads. Compile/link only; no execution.
`src/video_core/renderer_d3d12/guest_texture_test.cpp`

### 3. native-image-depth-prewrite-snapshots

Four additional full-core GCN modes, readonly-depth-image and mixed-depth-image with optional -owned, issue real vertex-stage R32 float image stores before Z32 depth tests. They use 32x32 aligned image/depth backing, 8x8 coverage, optional stencil increments, three warm CPU edits, consumed HTILE, exact store/attachment/color/padding oracles and rejection of competing depth writes/clears. Compile/link only; none executed.
`native-port/gcn_attachment_alias_test.cpp`

### 4. native-readonly-depth-buffer-prewrite-snapshots

The full-core GCN attachment fixture's readonly-depth-buffer and mixed-depth-buffer modes, plus -owned variants, prepare Z16/Z32, one/four samples, optional stencil, either plane's buffer writer, full/partial buffer descriptors, both read-only hints, three warm passes with CPU edits and consumed HTILE. Mixed cases additionally write depth or increment stencil while the other plane is buffer-written. Vertex stores precede attachment reads and write identical zero DWORDs. Independent expected bytes include color pass/fail results, attachment-plane results, exact shader stores and all remaining allocation/padding. Target-plane writes/clears and a fresh pending HTILE allocation overlapping the depth writer must reject before guest writes. Compile/link only; no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 5. native-gpu-buffer-image-prewrite-snapshots

The two existing full/partial sampled-image/buffer-writer fixtures additionally require a default-heap GPU tiler input with no CPU image snapshot. Warm passes include a GPU-to-CPU upload transition sampled through a separate output buffer, exact full guest allocation/padding comparison, a CPU edit and restoration of the GPU snapshot path. The remaining opposite-writer and two-writer rejection cases are retained. Compiled only; no cases executed.
`src/video_core/renderer_d3d12/guest_image_compute_test.cpp`

### 6. native-anisotropic-manual-storage-filtering

Thirty-six actual GCN cases prepare all four limits, both axes, point/linear, zero/degenerate/negative/skew gradients, a three-tap ratio, mip interpolation, sampler/resource LOD clamps, normalized/unnormalized coordinates, wrap/mirror addressing, min/max reduction, explicit LOD/LZ bias and bias cancellation beyond native sampler limits. Separate read-only outputs and seven cube cases cover shared/read-only faces, a second cube, edge clamping and mip interpolation. An independent double-precision CPU footprint/mip/tap oracle compares output within 1e-5 while all other allocation/padding bytes remain exact. Cache cases prepare changed/restored tap limits plus manual-to-hardware-to-new-manual transition and restoration. Compiled only; none executed.
`native-port/gcn_frontend_test.cpp`

### 7. native-buffer-image-prewrite-snapshots

Five metadata-to-DXIL/GPU cases prepare both directions, full/partial tiled overlap, a preceding GPU buffer writer, three warm submissions, a CPU edit, exact full allocation/padding, retirement of a preexisting sampled-cache witness and rejection of two simultaneous writers before execution. Compilation only; no cases executed.
`src/video_core/renderer_d3d12/guest_image_compute_test.cpp`, `native-port/buffer_reader_image_writer.comp`, `native-port/buffer_reader_image_writer_test.cpp`

### 8. native-unit-anisotropy-storage-filtering

Four actual GCN gradient cases prepare point and linear one-to-one anisotropy, zero/nonzero gradients, reflected shared resources, DXIL compilation and existing contents/padding/cache checks. Compilation only; no case executed.
`native-port/gcn_frontend_test.cpp`

### 9. native-r32-rgba8-shader-format-reinterpretation

120 additional native/reference classification cases prepare both format directions and external stage hints. Thirty actual GCN numeric pairs prepare shared/separate/restored resources, both stage representations/cache identities and DXIL compilation. Seven coherent GPU emitter modes prepare before/after reads, three warm mip/layer passes, reversed binding order, exact R32 float bits including a signaling-NaN word and full guest/output padding. Compilation/linking only; no fixture executed.
`native-port/image_access_classification_test.cpp`, `native-port/packed10_image_test.cpp`, `native-port/gcn_frontend_test.cpp`, `native-port/writable_image_alias_test.cpp`

### 10. native-integer-nearest-image-sampling

Eight GCN graphics cases prepare Uint/Sint sampling in ordinary R32 and packed10 formats, each with shared and separate stage resources; reflection, typed-load sampler classification and both DXIL stages are checked. Sixteen packed10 numeric cross-stage load/store pairs additionally prepare shared/separate/restored cache variants. These fixtures are compiled only and have not executed.
`native-port/gcn_frontend_test.cpp`

### 11. native-packed10-coherent-numeric-shader-views

Thirty-six additional resource-classification cases cover all read/write numeric pairs, native/reference isolation and signed reader hints for unsigned writers. Four actual SPIR-V/DXIL/GPU fixture modes prepare mixed reads before/after writes, opposing signed/unsigned writer types, three warm passes over different mips/layers, reversed descriptor order and full physical/output padding. Core/checks and refreshed full-core frontend/cache/reload/attachment fixtures compiled only; none executed.
`native-port/image_access_classification_test.cpp`, `native-port/packed10_image_test.cpp`

### 12. native-graphics-device-settings

Three native hardware fixture modes prepare default and configured core/sync/GPU/DRED device creation, explicit selected-adapter LUID identity, unavailable-index failure without fallback, actual GPU buffer-copy contents and long/color marker scopes spanning submissions. Fixture compilation/linking passed; no mode executed. Global/game/legacy persistence acceptance, actual device-removal report and original differential diagnostic review remain pending.
`native-port/device_configuration_test.cpp`

### 13. native-gpu-debug-markers

The new real native device fixture prepares nested host/guest scopes, a 1,400-character label, an actual ARGB color marker, GPU buffer copy, flush/finish/reopen/pop sequencing and debug-error checks. Compilation/linking passed; execution and native tool capture decoding remain unrun.
`native-port/device_configuration_test.cpp`

### 14. native-signed-packed-sampled-textures

Integration fixture reads actual expanded GPU texture buffers and validates signed 10-bit values, alpha endpoints, 1D/2D/3D mips/layers, distinct MSAA samples, cold/warm/changed source, typed SRV creation, retained original bytes/padding and debug messages. Multisample values are read through an actual GPU transfer buffer instead of unsupported multisample texture-to-buffer copying. Compile/link only; actual GCN sample/filter/gather/LOD/swizzle and adapter equivalence remain unrun.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`

### 15. native-packed10-storage

Twenty actual emitter/DXIL/native-runtime cases cover four numeric variants in tiled, linear, MSAA, 1D and 3D modes. They validate initial typed reads, normalized endpoints/rounding, signed integer endpoints, store-load-store sequencing, cold/warm reuse, every touched mip/layer/sample word, untouched texels/padding, integer bit patterns and debug messages. Existing unsigned typeless-clear contents/view-reuse checks remain unchanged. Compilation/linking only; no case has executed.
`native-port/packed10_image_test.cpp`, `native-port/typeless_texture_test.cpp`, `native-port/writable_image_alias_test.cpp`

### 16. native-signed-packed-integer-color

An additional signed-color mode of the existing full-core GCN fixture emits actual negative/positive signed endpoints and two-bit alpha to three simultaneous MRT array layers, testing one/four samples, original CB-slot permutations, cold/warm real GuestRasterizer::Draw, eager readback and all untouched bytes. The original unsigned fixture mode remains intact. Fixture compilation/linking succeeded; it has not executed.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 17. native-signed-packed-backing-conversion

Thirty-two additional packed10 modes of the existing real GPU/PageWatch fixture cover signed-only and mixed signed/unsigned transitions, linear/tiled, one/two samples and direct 1D/3D shapes. Six signed or nine mixed transitions include expanded reinterpretation and return to writable storage. Prepared assertions cover original negative-minimum words, actual expanded or packed single-sample GPU values, all array/mip/sample/depth bytes, untouched padding, zero intermediate ownership resolves or CPU faults, persistent-resource accounting after collection, final CPU visibility and debug messages. Existing sixteen packed16 cases remain intact. Compilation/linking passed; none executed.
`native-port/packed_backing_test.cpp`

### 18. native-packed10-disjoint-shader-views

Eight actual emitter/DXIL/runtime cases cover Unorm/Snorm/Uint/Sint sampled interpretations with signed writes, disjoint array layers or disjoint mips, intentionally writer-first image metadata, three warm dispatches, sampled numeric values, written packed words and all untouched texture/output bytes. Fixture compilation/linking passed; no case executed. Existing alias fixtures remain enabled.
`native-port/packed10_image_test.cpp`

### 19. common-sysmodule-id-validation

No separate prepared check recorded in this group; final acceptance not run.

### 20. common-game-path-discovery

No separate prepared check recorded in this group; final acceptance not run.

### 21. common-kernel-network-time

No separate prepared check recorded in this group; final acceptance not run.

### 22. common-amdgpu-format-tables

No separate prepared check recorded in this group; final acceptance not run.

### 23. native-shared-color-attachment-layers

Existing graphics-resource integration fixture shared-layers uses real MRT pixel exports, three differently ordered layer views, one/four samples, cold/warm cache identity, exact tiled contents/padding and required debug-layer messages. New full-core gcn_attachment_alias additionally invokes actual Liverpool registers, original GCN frontend, GuestRasterizer::Draw and eager guest readback for shared targets. Older graphics-resource branches remain intact. Compile/link only.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 24. native-cp-buffer-copy

`native-port/copy_dirty_ranges_test.cpp`, `native-port/guest_buffer_access_test.cpp`

### 25. native-packed-float-bit-transfers

Core and full-core fixture compiled/linked only. Prepared exact NaN/finite/shared-exponent physical words across linear/macro layouts, three layers/mips, one/four samples, warm uploads, CPU changes, sample-zero broadcast, untouched padding and retained backings. Active readback bytes are poisoned before comparison.
`native-port/packed_float_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 26. common-original-videoout-cadence

Core, standalone targets and desktop compiled only. Prepared presenter fixtures cover default coupled policy and lifetime-stable explicit independent mode; desktop fixtures reject altered capture controls.
`src/video_core/renderer_d3d12/guest_presenter_test.cpp`

### 27. common-windows-mapping-protections

Full-core fixture compiled and linked only. It prepares actual 64/16-KiB mapped splits and checks neighbouring PAGE_READONLY, PAGE_NOACCESS and PAGE_GUARD protections, exact physical contents/padding, remapped middle permissions and alias lookup.
`native-port/address_space_protections_test.cpp`

### 28. common-gcn-arithmetic-and-subgroup-memory-ordering

Full-core actual GCN fixture compiled/linked only, preparing dynamic positive/negative zero, infinities and multiple NaNs, finite arithmetic, 12 comparison ballots, dynamic neighbouring LDS visibility, 32/64-lane modes, cold/warm/CPU changes, metadata restoration and untouched guest padding.
`native-port/gcn_arithmetic_test.cpp`

### 29. immutable-reference-gpu-completion-capture

Original Release, explicit observer DLL, native core, actual-fence capture fixture and desktop compiled/linked only. Exact Flip RVA/extent/call target, preserved Request register/index offset, machine-code fingerprint and artifact hashes were derived offline from the actual original PE/MAP/PDB. Prepared delayed DIRECT fence interval, old/pending frame exclusion, duplicate/failure and desktop integrity fixtures have not run.
`native-port/guest_frame_trace_test.cpp`, `tools/vulkan_reference/completion_layer.cpp`, `tools/vulkan_reference/completion_layer.def`, `tools/vulkan_reference/completion-layer.json`, `tools/vulkan_reference/CMakeLists.txt`, `tools/vulkan_reference/resolve_completion_symbol.py`

### 30. native-bc-volume-and-extent

Compiled-only actual hardware fixture compares Texture3D Load and trilinear/mip SampleLevel with independent native BC array decoding across all BC families, sRGB/signed/float variants, linear/macro/thick layouts, partial blocks, five mips/depth slices and cold/warm/CPU-change. Its HLSL compiled offline with pinned DXC and strict warnings.
`native-port/bc_volume_test.cpp`

### 31. desktop-core-runtime-identity

Desktop compiled with zero warnings/errors. Prepared catalogue/profile fixtures cover both backends, legacy rollback, corruption, identity mismatches, untracked runtime, preserved graphics/source profile, per-version save copy/reuse/shared-save preservation and diagnostic cleanup. No fixture executed. At the user's explicit request one experimental current snapshot was packaged into the existing installed catalogue, with 76 hash-verified files, static PE dependency checks, matching PDB/MAP, dirty source patch and 20 copied save files; installed source saves/settings hashes were preserved.

### 32. amd-layout

No separate prepared check recorded in this group; final acceptance not run.

### 33. guest-tiling-transfers

No separate prepared check recorded in this group; final acceptance not run.

### 34. guest-multisample

No separate prepared check recorded in this group; final acceptance not run.

### 35. native-render-state-and-rasterizer-discard

Compiled-only GPU fixtures cover RGB/alpha MIN/MAX zero/one combinations, UNORM/SRGB, actual vertex UAV effects under discard and both-face culling, real bounded 16/32-bit IBV reads, cold/warm contents and untouched padding. Stencil preparation includes conflicting and unreachable ReplaceOp selection and post-clear write suppression.
`native-port/graphics_contract_test.cpp`, `native-port/stencil_lowering_test.cpp`, `native-port/gcn_winding_test.cpp`, `src/video_core/renderer_d3d12/shader_pipeline_test.cpp`

### 36. native-compute-wave-size

A compile-only full-core GCN fixture covers 128 threads, full-mask MBCNT, upper-half ballot, prefix count, fixed lane63/47, dynamic uniform lane47, divergent EXEC/firstlane, cold/warm dispatch, metadata restore, guest padding and D3D12 errors. The fixture has not executed.
`native-port/wave64_test.cpp`

### 37. guest-hardware-clock

No separate prepared check recorded in this group; final acceptance not run.

### 38. guest-surface-metadata

`src/video_core/renderer_d3d12/guest_cmask_test.cpp`

### 39. host-fsr-and-post-process

No separate prepared check recorded in this group; final acceptance not run.

### 40. native-screenshot-capture

No separate prepared check recorded in this group; final acceptance not run.

### 41. native-rgb96-storage

No separate prepared check recorded in this group; final acceptance not run.

### 42. native-imgui-display-and-lifetime

No separate prepared check recorded in this group; final acceptance not run.

### 43. common-cpu-and-mp3

No separate prepared check recorded in this group; final acceptance not run.

### 44. native-big-picture-launcher

No separate prepared check recorded in this group; final acceptance not run.

### 45. native-pipeline-preload

No separate prepared check recorded in this group; final acceptance not run.

### 46. native-packed16-images

No separate prepared check recorded in this group; final acceptance not run.

### 47. native-shader-debug-and-reload

No separate prepared check recorded in this group; final acceptance not run.

### 48. native-buffer-extents-and-index-bounds

No separate prepared check recorded in this group; final acceptance not run.

### 49. native-dma-page-table-growth

Resident-first-command, GPU-only B preservation, dirty CPU tail, late full64KiB/partial4KiB publication, initialized tiny-tail exclusion, selected-resource identity and SRV/UAV rejection controls are authored in native-port/dma_shader_test.cpp. Protected physical-backing and host-temporary rejection controls are in native-port/guest_buffer_access_test.cpp. All C++ typechecked only. Smaller late sharps, late arena replacement and SRV-only auxiliary unification remain production gaps; shader conversion/GPU acceptance not run.

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 50. native-depth-stencil-shader-plane-access

The plane-feedback mode compiles/linkably prepares real HLSL depth and stencil loads into RGBA32F while the other attachment plane writes. Twelve cases combine three warm versions with and without a beginning clear of the sampled plane. Independent full guest depth/stencil/padding and sampled color expectations include the cleared version; a sampled writable-plane variant must reject before recording. No tests or GPU execution.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`

### 51. native-depth-shader-subresource-access

Twelve depth-view-ranges cases prepare independent same-mip/different-layer and same-layer/different-mip shader loads versus attachment writes over three warm versions. The old descriptor is overwritten before draw. Exact complete guest contents/padding, sampled RGBA32F values and unrelated native states are checked; changing the DSV to the sampled subresource must reject before recording. Compile/link only, not executed.
`src/video_core/renderer_d3d12/guest_graphics_resources_test.cpp`

### 52. native-independent-buffer-image-versions

The full-core separate-buffer-image-owned GCN fixture compiles/links real simultaneous vertex buffer zero and image quarter stores, then pixel image loads. Twelve cases combine four warm bit patterns, including negative float bits, with R32 float/uint/sint views and compare the entire 64-KiB physical backing/padding. Pixel export supplies float-one alpha independently of integer image alpha. Actual protected CPU reads expose the buffer without destroying the image quarter; CPU writes and subsequent exact-base buffer writes invalidate it. The guest mapping and callbacks use two physical views. Fault-kind cases distinguish real reads and injected writes. Compile/link only.
`native-port/page_watch_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 53. native-independent-formatted-buffer-sync

The real GCN independent-version fixture includes BUFFER_LOAD_FORMAT_X before CPU reconciliation (image quarter must enter the GPU buffer, and later CPU visibility sees quarter) and after CPU reconciliation (small formatted reads see the raw zero CPU bytes while image loads still see quarter). Twelve float/uint/sint cases include negative bit patterns and independent complete physical allocation/padding oracles. All compile/link only, no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 54. native-formatted-metadata-arena-synthesis

The owned readonly-depth-buffer GCN fixture adds three warm HTILE cases: real depth metadata registration, raw GPU-modified 4-KiB arena writer, BUFFER_LOAD_FORMAT_X through RGBA8 UNORM, protected CPU read of 0x0f across the arena, then CPU zero write and small formatted stream. The formatted-cmask-owned GCN mode adds twelve cases over three warm versions and 1/2/4/8 samples, with real fast-clear-enabled color metadata registration. Large clean 32-KiB formatted arenas synthesize each sample pattern; small 4-KiB clean streams read physical 0xff instead. Exact complete 64-KiB contents/padding and per-sample color expectations prove arena-only writes. Compiled/linked only, not executed.
`native-port/gcn_attachment_alias_test.cpp`

### 55. native-independent-r32-mip-chains

The full-core GCN independent-version fixture now has twenty-four basic cases over two physical chains, three numeric views and four warm bit patterns. The two-mip chain writes/reads mip one at its actual physical byte offset using original sharp base_level and raw buffer offen instructions with distinct shader hashes. Twelve additional cases produce half in image mip zero and quarter in mip one, then require formatted synchronization and a real CPU buffer fault to expose both values while every other byte/padding remains exact. Original small versus large clean formatted behavior, per-mip sampling, CPU writes and exact-base GPU invalidation are prepared. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 56. native-independent-r32-array-layers

The actual full-core GCN fixture prepares sixty basic cases: one/two layers with one/two mip chains, a six-face cube base level, FLOAT/UINT/SINT and four warm values including negative bits. Array DA instructions write/read the last layer or cube face five, raw-buffer and formatted offen instructions use its actual physical mip/layer offset, and changed programs have distinct hashes. Forty-eight additional cases diverge layer-zero/mip-zero or cube face zero half from the selected layer/mip or cube face five quarter, require formatted transfer and a protected CPU buffer read to expose both, then sample the first layer/face separately. Every case independently checks the entire 64-KiB physical allocation/padding, including untouched cube faces. Compiled/linked only, not executed.
`native-port/gcn_attachment_alias_test.cpp`

### 57. native-independent-r32-vector-formats

The actual GCN fixture prepares 180 basic and 144 divergent mip/layer/face cases over three data formats, three numeric views, one/two layers/mips and six-face cubes, with four warm patterns. RG32 rectangular chains and RGBA32 reduced dimensions fit the same complete allocation. Every channel is initialized independently from the warm bits; formatted CPU visibility expects absent image-store channels zero while raw-store visibility preserves prior channels. Separate first/selected subresource results, all untouched texels/faces, full 64-KiB bytes/padding, clean streaming threshold, CPU writes and exact-base GPU invalidation remain independent oracles. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 58. native-independent-r32-volume-slices

The actual full-core GCN fixture adds seventy-two basic and seventy-two divergent volume cases over three data formats/numeric views and four warm patterns, bringing the fixture to 252 basic and 216 divergent cases. Base-only volumes have two Z slices; two-mip volumes shrink from four to two Z slices, write/read Z one at its actual guest offset, then diverge mip-zero/Z-zero half from selected mip/Z-one quarter. Every mip-specific Z slice/channel is initialized and expected independently. Complete 64-KiB bytes/padding, untouched Z slices, formatted full-volume transfer, clean streaming, CPU write and GPU exact-base invalidation are prepared. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 59. native-independent-rgb32-native-expansion

The actual full-core GCN fixture adds thirty-six basic and twenty-four divergent RGB32 cases over three numeric views and four warm values, one/two array layers and a two-slice volume base mip, totaling 288 basic/240 divergent cases. Every logical RGB channel is initialized. Complete physical bytes/padding compare exact twelve-byte output rather than the native sixteen-byte stride, including separate first/selected slices, raw first-DWORD versus full image-store component results and real protected CPU visibility after formatted transfer. Larger RGB cube/mip fixture shapes remain outside the fixed allocation. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 60. native-independent-rgba8-normalized-versions

The actual GCN fixture adds 56 basic/48 divergent normalized cases across two numeric views, four warm values and existing mip/layer/cube/volume shapes, totaling 344 basic/288 divergent cases. Warm values use independently quantized replicated bytes, including negative SNORM. Physical quarter/half/one bytes are 64/128/255 for UNORM and 32/64/127 for SNORM; the expected SNORM half export is 129. BUFFER_LOAD_FORMAT_X uses matching RGBA8 normalized sharp views. Full image/arena/CPU bytes, absent-channel zeroing, untouched subresources, complete 64-KiB padding, real faults and exact-base GPU invalidation remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 61. native-independent-rg16-rgba16-versions

The actual GCN fixture adds 168 basic/144 divergent cases across two data formats, three numeric views and four warm values over existing mip/layer/cube/volume shapes, totaling 512 basic/432 divergent cases. Warm half values are exact IEEE encodings for zero/one/positive-negative half; normalized values use independent sixteen-bit quantization including negative SNORM. Quarter/half/one are 3400/3800/3c00 FLOAT, 16384/32768/65535 UNORM and 8192/16384/32767 SNORM. Every channel, full physical/padding bytes, complete mip/layer/face/Z transfer, real protected CPU visibility, small streaming and exact-base GPU invalidation remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 62. native-independent-formatted-buffer-larger-extent

Every one of the 512 basic full-core GCN cases adds two larger sharp observations across all implemented data/numeric formats and mip/layer/cube/volume shapes. Extended views have at least twenty KiB and at least four KiB beyond the physical image. After CPU reconciliation, the large clean view must read the image version even when its image is below the small streaming threshold, while physical CPU buffer bytes remain separate. Before CPU reconciliation, the larger GPU-modified view must transfer the whole image and expose its exact component encodings to a real CPU fault. Whole 64-KiB expected bytes/padding include the untouched extended tail. Existing 432 divergent-subresource cases remain. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 63. native-independent-formatted-buffer-short-views

Every one of 512 basic full-core GCN cases adds two short-view observations alongside existing full/long views. The short sharp ends immediately after the selected texel, preserves matching format stride/record count and is smaller than the image. After CPU reconciliation it expects zero through a <=16-KiB clean stream, or image quarter through a larger clean arena, while image reads retain quarter. Before CPU reconciliation a GPU-modified short view requires complete image retile and exact protected CPU quarter/channel results. Whole 64-KiB physical/padding expectations cover all bytes beyond the view and untouched subresources. Existing 432 divergent mip/layer/face/Z cases remain. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 64. native-independent-small-integer-image-versions

The actual full-core GCN fixture adds 168 basic/144 divergent integer cases across three data formats, UINT/SINT and four warm values, totaling 680 basic/576 divergent cases with full/short/long formatted observations. Warm integers include negative sign-extension cases. Actual byte/halfword quarter/half/one stores, CPU edits, scaled numeric exports, every channel and full physical/padding/untouched subresources are independent expectations. The color oracle now explicitly clears G/B before export for every image-load case; this fixes earlier multi-channel warm-value exports while full physical comparisons continue to verify all guest channels. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 65. native-independent-subdword-image-versions

The actual GCN fixture adds 364 basic/312 divergent cases, totaling 1044 basic/888 divergent across all implemented data/numeric and mip/layer/cube/volume shapes. Geometry derives from actual format bits to keep complete image chains page-backed. Warm initialization and first-subresource image expectations write only each texel's actual one/two bytes, avoiding adjacent-texel/padding overwrites; raw/CPU DWORD operations still update four bytes. Negative normalized/integer and exact half values, scaled integer reads, full/short/long views, every neighboring texel and complete 64-KiB physical/padding bytes remain independent expectations. Compiled/linked only.
`native-port/gcn_attachment_alias_test.cpp`

### 66. native-independent-srgb-image-versions

The full-core GCN fixture adds 28 basic/24 divergent SRGB cases, totaling 1072 basic/912 divergent. Warm images use an independent piecewise SRGB decode oracle. Quarter/half UNORM image stores expect encoded bytes 64/128, separately sampled red 13/55, matching formatted-buffer red 64/128 and exact protected CPU encodings. Full/short/long views, all components, complete allocation/padding and untouched mip/layer/face/Z expectations remain. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 67. native-independent-packed10-image-versions

The full-core GCN fixture adds 119 basic/102 divergent cases across four packed numeric formats and all implemented shapes, totaling 1191 basic/1014 divergent. Actual ten-bit quarter/half/one literals and full RGB/alpha warm packing use independent encodings and CVT/MUL integer read scaling. Additional SNORM negative-minimum warm bytes use RGB -512 and alpha -2, distinguishing retained raw encodings from their decoded saturation. Full/short/long formatted views, actual protected CPU accesses and complete physical/padding/subresources are prepared. Core/checks/fixture and exact runtime SM5.1 host shader compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 68. native-independent-packed16-image-versions

The full-core GCN fixture adds 80 basic/64 divergent packed16 cases, totaling 1271 basic/1078 divergent before the later packed-float extension. Independent bit packing, component widths/remapped positions and normalized warm values expect quarter red 66/68 and half red 132/136 rather than unquantized values. Four warm values, full/short/long buffers, all mip/layer/face contents, neighbor texels, CPU faults and complete allocation/padding are prepared. Core/checks/fixture compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 69. native-independent-r11-packed-float-image-versions

The full-core fixture adds 56 basic/48 divergent packed-float cases across both raw descriptor orders, totaling 1327 basic/1126 divergent. Independent unsigned-float exponent encodings cover quarter/half/one and warm zero/one/half/negative-clamped-zero, all RGB components and both red positions. Actual formatted FLOAT loads, full/short/long views, all mip/layer/face/Z subresources, protected CPU values and full allocation/padding remain prepared. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 70. native-independent-logical-partial-buffer-writes

All 1327 basic/1126 divergent GCN cases now repeat with full, shortest valid DWORD-store and extended raw writer sharps, totaling 3981 basic/3378 divergent. Matching formatted full/short/long observations retain their original view extents independently of writer bounds. Each case additionally writes through an actual DWORD-shifted original base and requires its own CPU result while the preceding image quarter survives; a subsequent exact-base write requires image reload to zero. Full physical bytes, trailing ranges, neighbors and untouched subresources remain independent expectations. Compile/link only.
`native-port/gcn_attachment_alias_test.cpp`

### 71. native-mixed-page-buffer-ownership

The page-watch fixture prepares interior ownership splitting, unchanged outside read-only modes, retained range lookup, conversion back to read-only, partial release creating a writable hole, complete mixed-gap reacquisition, full original permission restoration, rejection of an originally read-only untracked page and rejection after watched OS permissions change with no partial mutation. The 3981 basic/3378 divergent full-core GCN partial/shifted cases compile against the same production API. Compiled only; no execution.
`native-port/page_watch_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

New preserving B envelope controls: native-port/buffer_envelope_fixture.h via guest_buffer_access_test.cpp. Eleven scenarios cover full GPU B/CPU/proof/store-marker transfer, tails/bridges, partial actual writes, clean ownership, callback CPU stores and fence lifetime; reentrant preparation is a negative control. C++ /Zs only, no runtime execution.

### 72. native-exact-subdword-buffer-write-ranges

Every one of 3981 basic/3378 divergent prepared GCN cases additionally issues real BUFFER_STORE_BYTE at original base image+selectedOffset+1/size1 and naturally aligned BUFFER_STORE_SHORT at +2/size2. Before these writes a formatted image copy intentionally makes resident GPU quarter bytes differ from clean physical buffer zero. Protected byte reads and full-allocation comparison require exact 0x35/0x7a35 results while untouched aligned DWORD bytes stay in the CPU domain and the sampled image remains quarter. Actual opcodes were checked against pinned frontend definitions. Core/checks/fixture compile only.
`native-port/gcn_attachment_alias_test.cpp`

### 73. native-actual-cpu-store-formatted-buffer-pages

Byte-component GCN image/buffer cases verify resident quarter through a large raw BUFFER_LOAD_DWORD plus byte mask/UINT conversion, perform a volatile same-zero CPU store and require raw zero before image refresh; raw reads cannot use small CPU streams or formatted synchronization. Subsequent refreshed image/formatted zero retains full physical oracles. Production GuestBufferAccess fixture fills a 16-KiB GPU-only arena, reconciles four declared GPU-written bytes on a real CPU read without a store marker, then stores an unchanged CPU byte in page two. Forced upload count is 4096 and full GPU readback retains three untouched GPU pages. A physical-alias callback additionally notifies page three during page-two upload; both CPU pages must reach this prepared view while first/last GPU pages persist. Current core/checks/all-ten full-core fixtures compile/link and static inventory/whitespace pass; no execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 74. native-zero-clear-original-image-domain

Pinned IsComputeImageClear has no separate zero CPU fill. Twelve prepared actual GCN two-buffer clear shapes use the old hash after independent R32 FLOAT buffer/image writes, require first/next sampled texels zero while warm physical buffer/padding bytes persist, then recreate the ordinary quarter scenario with an exact-base writer. Current core/checks/all-ten full-core fixtures compile/link, no non-deprecation diagnostics or whitespace errors; no execution.
`native-port/gcn_attachment_alias_test.cpp`

### 75. native-image-clear-complete-subresources-padding

A real padded RGBA8 RTV fixture has width17/pitch32/height9, two mips and three layers while its cached view selects mip1/layer1. Full GuestImageReadback must show exact yellow on every logical texel and unchanged 0xa6 padding. GCN zero-clear shapes expand to 84 R32 FLOAT warm/extent/mip/layer/cube/volume cases, requiring image zero with unchanged separate physical buffer bytes before restoring the quarter scenario. Core/checks/attachment fixture compile/link; final all-ten refresh/audit pending, no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 76. native-nonzero-wide-storage-image-clears

All 84 R32 FLOAT GCN zero-clear shapes additionally clear to half: first/next sampled red128, physical buffer zero/warm padding, large raw DWORD-to-float resident GPU buffer zero before formatted sync, then exact-base quarter restoration. A production API GPU fixture prepares 27 data-format/numeric/dimension combinations (R32/RG32/RGBA32; UINT/SINT/FLOAT; 2D array/1D array/volume), three mips and a restricted cached view. Full readback requires exact 1.5/-0/-3.25/0.25 union bits plus original 0xa6 padding. Nine R32 warm cases additionally clear to negative zero and require exact 0x80000000. Current core/checks compile; final fixtures/audit pending, no execution.
`native-port/gcn_attachment_alias_test.cpp`, `native-port/page_owned_gpu_test.cpp`

### 77. native-nonzero-typed-narrow-image-clears

81 prepared R8/RG8/RGBA8/R16/RG16/RGBA16 numeric/dimension combinations cover supported UNORM/SNORM/UINT/SINT/FLOAT views, arrays/1D/volumes, three mips and restricted cached views. Full GPU readback requires exact selected normalized/integer/sign/half-float/negative-zero texel encodings and unchanged 0xa6 guest padding. Latest compilation pending; no execution.
`native-port/page_owned_gpu_test.cpp`

### 78. native-raw-rgba8-converted-image-clears

Twelve additional API numeric/array/1D/volume cases require full exact RGBA8 output, unchanged 0xa6 padding, unchanged existing image resource identity/descriptor, temporary memory accounting and fence retirement. Actual GCN HLE zero/nonzero clear coverage adds 336 RGBA8 shapes to 84 R32 FLOAT shapes, requiring image observations and independent raw resident/physical buffer zero before formatted sync. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 79. native-packed16-sampled-volumes

Eight API format/tile volume cases prepare all four GPU channels, three mips/depth slices, raw-to-expanded-to-raw-to-expanded view changes, poisoned CPU upload inputs and exact original bits/padding retention. Actual GCN separate buffer/image coverage adds 96 basic and 96 divergent warm/writer/mip volume combinations (4077/3474 total). Core/checks and SM5.1 FXC /Ges /WX compile; no execution.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 80. native-packed16-nonzero-image-clears

Twelve added format/array/1D/volume API cases require explicit exact packed colors, three mips/restricted cached views, 0xa6 padding, resource identity and temporary allocation retirement. Actual GCN zero/nonzero HLE cases add 336 packed16 shape combinations using a correctly sized 16-bit formatted destination sharp (756 total clear combinations); sampled image output changes while raw resident/physical buffer stays zero before formatted sync. Core/checks and strict SM5.1 compiler pass; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 81. native-packed10-converted-image-clears

Twelve new API numeric/array/1D/volume cases prepare exact all-mip packed words/padding/identity/retirement. Twelve warm endpoint cases prepare normalized signed extremes, nearest-even alpha and unsigned/signed integer bounds, including two-bit alpha clamp. Actual GCN clear coverage adds 420 packed10 shape combinations (1176 total HLE clear combinations), with zero/half/integer image observations and unchanged resident/physical raw buffer before formatted sync. Core/checks and strict SM5.1 FXC compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 82. native-srgb-image-clears

Six typed/raw array/1D/volume API cases prepare exact encoded 137/188/0/255 RGB/alpha, three mips/full resource view, original image identity/descriptor/accounting and padding. Six warm cases prepare the transfer-curve boundary and linear half alpha (0x800d0a0a). Actual GCN adds 84 SRGB zero/nonzero HLE shapes (1260 total clear combinations), observing sampled linear half red128 with resident/physical buffer zero before formatted sync. Core/checks compile only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 83. native-readonly-image-clear-promotion

84 added read-only typed/SRGB numeric/dimension API cases require exact complete texels, padding, descriptor/resource identity and transfer retirement (234 API cases total; three added SRGB endpoint warm cases). 504 actual GCN cold read-only cache HLE cases prime sampled image/raw resident buffer before the first image writer, then require changed first/next image observations and unchanged original CPU/raw arena bits. Existing post-writer HLE coverage is 1260 combinations. The first zero-clear oracle now correctly preserves the preceding quarter color because compute does not draw into the color target. Core/checks compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 84. native-compact-linear-image-download

Twelve API cases cover R32 float, raw RGBA8, RGB32 and packed16 across restricted three-mip 2D/1D arrays and volumes. Independent compact CPU indexing requires every mip-zero texel, preserved row padding and untouched full-mip-chain tail. Real protected resident-buffer fixture prepares failed commit, changed physical image bytes with unchanged raw arena, no CPU-store upload and a subsequent same-value store requiring precisely one 4 KiB page upload. Production core/checks compile; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/guest_buffer_access_test.cpp`

### 85. native-queued-independent-image-downloads

New compile-only linear-download-owned actual GCN mode prepares eight R32-float single/array and base/two-mip-chain shapes with two initial CPU values. Both writers establish distinct versions; legacy OnFence/OnSubmit HLE clears require complete compact physical rows and unchanged padding/tail, unchanged raw arena, live cleared image, no repeat download after a read-only draw, and same-value CPU store transfer. No execution.
`native-port/gcn_attachment_alias_test.cpp`

### 86. native-priority-retirement-drain

GPU scheduler fixture prepares a priority map/physical-write callback retaining readback after caller release, a held memory lock released before drain, injected callback failure followed by successful retirement, and destructor draining a queued write. Compile-only; no execution.
`native-port/scheduler_lifetime_test.cpp`

### 87. native-priority-linear-pressure-commit

Protected production buffer fixture queues an actual GPU readback while arena/CPU are equal, holds the execution mutex to delay worker commit, performs a real same-value protected store, then requires physical bytes, precise 4 KiB GPU upload and zero retained budget after drain. Compact image cases release the source fence lease before mapping. Core/checks and fixtures compile only; no execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/page_owned_gpu_test.cpp`

New invalid-I pressure fixture: native-port/invalid_image_pressure_fixture.h via page_owned_gpu_test.cpp; actual LRU reclamation, B/CPU/padding/store-marker retention, caller/callback pins and pinned valid tiled exclusion. C++ /Zs only, not executed.

### 88. native-rgb32-and-wide-readonly-clears

45 added wide read-only/RGB32 API cases cover three numeric views and restricted three-mip 2D/1D arrays/volumes, exact physical bytes/padding, original identity and transfer retirement (279 storage-clear API cases total). Native RGB32 readback independently requires alpha one across the selected mip/layer/depth footprint; nine additional read-only negative-zero cases prepare exact bits (18 total). Actual GCN RGB32 clear cases use a correctly sized 96-bit destination sharp, cover cold read-only float promotion/repeated clears and post-writer clears with unchanged raw/physical buffer. Compile-only; no execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 89. native-expanded-packed-image-clears

20 additional read-only packed API cases include ten expanded signed-packed10/packed16-volume cases, all full mip/layer/depth texels and untouched physical padding (299 storage-clear API cases total). Twelve additional packed10 endpoint warm cases (24 total) exercise replacement/retirement of a second exact backing. Actual GCN cold read-only coverage adds packed10 and packed16 volumes, with three consecutive clear values, preserved initial raw/CPU bytes and subsequent writer/format sync (1056 cold cases total; 1368 post-writer HLE combinations after RGB32 coverage). No execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 90. native-packed16-sampled-one-dimensional

Eight new format/linear-or-tiled 1D-array GPU cases use three mip levels/layers and restricted selected mip/layer, compare every expanded component, poison CPU upload input, then require raw-to-expanded-to-raw-to-expanded identity/ownership/exact physical source and padding. Four new read-only packed16 1D clear API cases require all resource texels and preserved padding (303 API cases total). Compile-only; no execution.
`src/video_core/renderer_d3d12/guest_signed_packed_test.cpp`, `native-port/page_owned_gpu_test.cpp`

### 91. native-priority-inflight-backing-generation

Four actual GPU-copy fixtures hold an external fence unsignaled, require submitted-but-incomplete copy and retained priority budget, replace backing through register/map/unmap-remap/host mapping, signal the GPU gate, drain and require no read/resolve/write callback, unchanged replacement bytes and zero retained budget. Compile/link only; no execution.
`native-port/guest_buffer_access_test.cpp`

### 92. native-nonblocking-linear-pressure-source-residency

Actual GPU R32 3-layer/3-mip fixture clears the source, holds an external fence unsignaled, checks exact source accounting and shared lease copies, destroys the source cache/allocator/entries before signaling, then requires source-plus-readback priority budget, full compact physical contents, preserved padding/tail and zero priority budget after drain/collection. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`

New invalid-I pressure fixture: native-port/invalid_image_pressure_fixture.h via page_owned_gpu_test.cpp; actual LRU reclamation, B/CPU/padding/store-marker retention, caller/callback pins and pinned valid tiled exclusion. C++ /Zs only, not executed.

### 93. native-queued-linear-color-attachment-domains

Four actual GCN linear MRT cases use three shared array layers, two permutations/two warm runs, 56x16 logical pixels with padded pitch 64. They require one deduplicated fence download, unchanged old raw GPU bytes, complete physical allocation/padding, no stale empty-fence repeat, a second download after already-owned attachment rebinding on submit and actual same-value CPU store updating only its 4 KiB raw page. Existing storage/HLE fixtures compile against the shared ownership path. No execution.
`native-port/gcn_attachment_alias_test.cpp`

### 94. native-compact-exact-expanded-packed-downloads

Compact API coverage grows from 12 to 24 cases: signed packed10 arrays/1D/volumes and all four packed16 formats across raw 2D plus expanded 1D/volumes, with three mips and restricted selected mip/layer. Independent fixed packed-word expectations require every compact mip-zero texel, all padding/tail and source release before resolve. Existing RGB32/RGBA8/wide cases remain. Compile/link only; no execution.
`native-port/page_owned_gpu_test.cpp`

### 95. native-source-residency-format-and-sample-admission

All 24 compact real-GPU image cases additionally require the cache residency total to include a retained actual texture allocation and zero retired-source bytes after its copy fence/release. Existing shared source-copy/cache-destruction and sample-backing GPU fixtures compile against the unified admission path. No execution.
`native-port/page_owned_gpu_test.cpp`

### 96. native-image-source-gpu-modification-selection

Existing protected real-GPU arena fixture requires image physical download and read-only synthesis to leave GPU modification false. Actual GCN queued-image cases require buffer writer true and CPU readback false; one fresh alias case keeps quarter in physical CPU while raw arena is zero, retires the image for a buffer writer plus read-only image load, requires prewrite pixel quarter, raw zero, subsequent raw CPU download and full independent padding oracle. No execution.
`native-port/guest_buffer_access_test.cpp`, `native-port/gcn_attachment_alias_test.cpp`

### 97. native-r32-float-multisample-image-clears

18 actual GPU API cases cover 2/4/8 samples, linear/tiled physical layouts and three descriptor usages, restricted layer one of three. Two consecutive quarter/negative-zero clears require exact R32 bits in every sample/layer, unchanged native resource/descriptor and complete physical padding. Existing sample/typed-clear/HLE fixtures compile against the ownership/backing rule. No execution.
`native-port/page_owned_gpu_test.cpp`

### 98. native-image-clear-physical-channel-swizzles

Twelve actual GPU API cases cover R32 FLOAT, RGBA32 FLOAT and RGBA8 UNORM, two nonidentity selector mappings including constants, linear/tiled layout and restricted mip/layer. Fixed independent physical words, all texels and padding, original resource identity and descriptor are required. Compile-only; no execution.
`native-port/page_owned_gpu_test.cpp`

### 99. native-exact-expanded-pressure-source-residency

Six externally gated cache-destruction GPU cases cover R32 float, signed packed10 and four expanded packed16 volumes. They require actual summed source allocations, shared lease accounting, source/cache destruction before gate signal, exact compact packed words/padding and zero pending commit bytes after drain. Existing 24 compact source-accounting cases compile against the wider lease. No execution.
`native-port/page_owned_gpu_test.cpp`

### 100. native-multisample-storage-uav-image-clears

24 GPU API cases cover R32 uint/sint/float/raw-float, 2/4/8 samples and linear/tiled physical layouts, two consecutive quarter-bit/negative-zero clears, every sample/layer and padding, restricted layer one, unchanged resource/descriptor and post-fence clear-lease release. Existing full-core HLE fixtures compile only. No execution.
`native-port/page_owned_gpu_test.cpp`, `native-port/independent_version_transfer_test.cpp`

### 101. native-exported-vs-point-size-geometry

96 actual GPU GCN cases prepare exported one/four-pixel sizes at nonboundary centers, center outside XY clipping, four mixed viewport flip masks, three cull settings, cold/warm, front-face pixel and front/back stencil contents, auxiliary-stage serialization and exact full allocation/padding. Pipeline state fixtures require supported sized PointList with solid fill/no culling, while polygon point fill remains guarded. Compile-only; no execution.
`src/video_core/renderer_d3d12/shader_pipeline_test.cpp`, `native-port/gcn_winding_test.cpp`

### 102. native-sized-point-emulated-clip-forwarding

192 additional actual GPU GCN point cases cover native/emulated four-plane exports, positive/negative distances, one/four-pixel sizes, viewport flips, cull flags, front-facing pixel/stencil, cold/warm, auxiliary serialization and complete padding. All four emulated components are independently initialized in the fixture. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 103. native-sized-point-render-target-layer-forwarding

96 additional actual GPU GCN cases prepare one/four-pixel points routed to different layers of a three-layer color target, viewport flips, cull flags, clipped centers, cold/warm, untouched other layers/depth/stencil and full physical padding. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 104. native-sized-point-dynamic-viewport-extents

96 additional actual GPU GCN cases prepare one/four-pixel points routed to two viewports of different widths/heights, all four Y-sign masks, cull controls, front-face pixel/stencil, clipped centers, cold/warm, full guest allocation/padding and auxiliary serialization. Runtime offsets have compile-time assertions. The clip fixture also declares sufficient VGPR allocation for its last loaded component. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 105. native-guest-geometry-point-expansion

192 additional GPU winding cases prepare production GS EmitVertex with exported/default one-pixel size, mixed viewport flips, culling/front-face/stencil, clipped centers, cold/warm and full padding. These use real GCN VS/PS with an explicitly generated SPIR-V GS, not a guest GCN GS. State coverage prepares triangle input with point GS output and both cull flags. Existing auxiliary GS cases exercise the shared generator. Compile-only; no execution. Another 192 cases use actual guest GCN export/geometry/copy programs, coherent ESGS/GSVS ring stores, two GS emissions per invocation, independent ES/GS atomic counters, exported/default size, clipping and complete padding. Code metadata follows the actual token-directed offset and stays outside every program. Compile/link only; not executed.
`src/video_core/renderer_d3d12/shader_pipeline_test.cpp`, `native-port/gcn_winding_test.cpp`

### 106. native-geometry-cache-borrowed-copy-span-elimination

Actual guest GCN GS winding cases prepare metadata round trips requiring empty restored spans in runtime and specialization, preserved copy hash and nonempty unchanged live span, along with actual compiled GS/ring/atomic coverage. Existing archived/unarchived pipeline-reload fixtures remain available. Compile-only; no execution.
`native-port/gcn_winding_test.cpp`

### 107. native-point-misc-valid-varying-slot

A build-only compile-shaders path returns before hardware device creation and skips runtime metadata/content oracles. Offline production frontend, linked Mesa NIR/DXIL and DXIL signing validate ordinary point size, native/emulated clipping, layer/viewport, explicit GS and actual GCN ES/GS/copy variants. No GPU oracles, renderer/game launch or semantic acceptance are run. The separate direct VS viewport baseline currently fails NIR conversion and remains open.
`native-port/gcn_winding_test.cpp`

### 108. native-late-per-viewport-homogeneous-conversion

All ten existing VS/GS/PS chains compile offline through production frontend, linked NIR/DXIL and signing with the updated converter. Two additional 96-case GPU modes prepare late viewport exports, mixed Y signs, inverted viewport-one depth and w=2; fragment depth reaches an exact color-channel oracle. All preparation remains unexecuted. No GPU/device/game acceptance occurs in compile-shaders mode.
`native-port/gcn_winding_test.cpp`

### 109. native-point-window-transform-and-viewport-edge-footprint

Core/check builds, all ten full-core compile/link fixtures and offline signed VS/GS/PS chains are required. A 384-case mode prepares four viewport edges, size 1/4, two differently sized viewports, four Y-sign combinations, w=2, restricted normal/reversed depth, center rejection, cold/warm, front-facing stencil, ignored culling and full 64-KiB contents/padding. No hardware or runtime acceptance before full implementation boundary.
`native-port/gcn_winding_test.cpp`

### 110. native-point-parameter-liveness-and-sparse-clip-interface

Three additional 96-case real GCN modes export all 32 guest parameters: no fragment consumers, a live sparse flat parameter, and a live sparse parameter with emulated clip planes. Each guest VS performs one atomic increment, including center-clipped points, with full 64-KiB contents/padding oracles and cold/warm state. Shader binary metadata moves to word 192 in 256-word arrays to fit complete programs. Compilation, linking and offline signing are allowed; actual GPU oracles remain unexecuted.
`native-port/gcn_winding_test.cpp`

### 111. native-gcn-point-geometry-live-output-budget

Two new 96-case actual GCN ES/GS/copy modes export all 32 float4 parameters and emit two overlapping points, with zero or one live sparse fragment input. Original ES/GS atomics must increment once and stencil twice, including center rejection/cold/warm/full contents/padding. COPY offsets beyond twelve-bit MUBUF immediate use explicit scalar offsets handled by the original parser; 1024-word arrays put binary metadata at word 768. Core/checks, full-core links and offline signed linked chains are required; no GPU acceptance before full implementation boundary.
`native-port/gcn_winding_test.cpp`

### 112. native-acyclic-point-gs-emission-bound

Three additional 96-case actual GCN ES/GS/copy modes prepare large declarations with two actual emissions: maximum 128 with no live parameters, maximum 64 with a live float4 parameter, and a real scalar-controlled loop with original maximum 64 retained. Scalar COPY offsets accumulate signed increments instead of truncating high offsets. Static SPIR-V must distinguish eight allocated corner vertices for acyclic cases from 256 for the loop, preserving one ES/GS atomic instruction and MRT output. Core/checks, fixture linking and offline signing only; GPU contents/side effects oracles remain deferred.
`native-port/gcn_winding_test.cpp`

### 113. native-unit-point-gs-hardware-emission

Three additional 96-case actual GCN ES/GS/copy modes cover declared unit maxima 256, maximum 128 with one live parameter and a real loop declared 256. Existing explicit and GCN unit modes now compile with OutputPoints and single emissions; exact GPU color/stencil/counter/padding oracles are prepared but unrun. Offline linked shader compilation/signing, static topology/bound/atomic structure and C++ compile/link checks are permitted only.
`native-port/gcn_winding_test.cpp`

### 114. native-point-gs-consumer-component-widths

Three new 96-case real scalar-loop ES/GS/copy preparations read x/z/w of a parameter with distinct component values. Original maxima 48/36/32 expand to 192/144/128 corners with position-plus-parameter component budgets 960/1008/1024. Static SPIR-V/DXIL must show matching prefix widths 1/3/4 and retained one ES/GS atomic, one loop and four lexical emits. GPU counter/color/stencil/padding and metadata oracles remain unrun; only static builds, linking and offline shader signing are permitted.
`native-port/gcn_winding_test.cpp`

### 115. native-point-gs-sparse-component-transport

Existing x/z/w loop cases use scalar transport; the w case now declares 48 guest vertices instead of 32. A new 96-case x+w loop with maximum 42 transports two distinct channels and prepares green=32/blue=191, counter/stencil and full padding oracles. Static SPIR-V/DXIL must show matching widths 1/1/1/2, correct source values and remapped PS read columns, native budgets 960/720/960/1008, unchanged one ES/GS atomic and one loop/four lexical emits. Only C++ compile/link and offline signing/static artifacts are allowed; GPU oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 116. native-point-gs-aliased-fragment-input-declarations

Two new 96-case real-loop ES/GS/copy modes prepare inputs 0.x and 1.w from parameter 0, plus a default input 0 preceding input 1.w. Initial offline compilation signed but static SPIR-V had two Location 0 variables and DXIL expected TEXCOORD0/TEXCOORD1 while GS produced only TEXCOORD0. Final offline/static evidence must show one producer/consumer semantic and correct compact columns/values; GPU color/counter/stencil/padding and cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 117. native-point-gs-qualified-parameter-transport

One new 96-case actual GCN loop reads flat x and smooth w through two inputs from parameter 0. It declares 42 guest vertices, preserves FrontFace around barycentric input VGPRs and prepares green=32/blue=191 plus counters/stencil/padding. Initial compilation signed but retained duplicate Location 0 variables, requiring static signature review. Final SPIR-V/DXIL must show separate matched scalar locations/semantics with preserved interpolation/value stores, budget 1008 and unchanged one ES/GS atomic/one loop/four lexical emits. All earlier chains must compile/sign; GPU/runtime/cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 118. native-clip-disabled-viewport-transforms

Three new 96-case chains cover sized VS, actual sized GS and unit GS with exported viewport index and disabled clipping. Full color/stencil/padding/counter oracles include centers outside the original viewport. A source-only viewport case preserves differing signed raw transforms despite identical large hardware viewports. Offline compile/sign and static DXIL must prove byte 560 plus 16 times the actual loaded index, unchanged side effects and original arithmetic; all earlier point chains and ten full-core fixtures must compile/link. An initial sized-GS fixture reused v9 for its counter and viewport index; the counter is moved to v10 before final evidence.
`src/video_core/renderer_d3d12/guest_viewports_test.cpp`, `native-port/gcn_winding_test.cpp`

### 119. native-point-center-private-distances

Four new 96-case original GCN loop preparations cover sized clip-only, cull-only, combined clip/cull and unit clip control, with 64 declared guest vertices. Sized cases can emit 256 corners within a position-only budget of 1024; unit clipping retains 64 output points and native distance semantics. Guest distance buffers, signed values, counters, stencil and padding are prepared. Source compile/link and offline signing/static SPIR-V/DXIL must show retained distance loads/comparisons, no sized clipping output signature and unchanged guest side effects. Runtime oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 120. native-linked-clip-input-signatures

Existing native point clip chain previously signed despite VS SV_ClipDistance versus GS SV_CullDistance input mismatch. Final static DXIL must prove matching producer/consumer distance semantics. One new 96-case native cull preparation checks the zero producer clip length independently. All prior actual-GS private distance, unit clipping and emulated/generic chains must compile/sign with all ten full-core fixtures compiled/linked; no GPU/cache/game oracles run.
`native-port/gcn_winding_test.cpp`

### 121. native-point-dense-distance-interface

Two new 96-case native VS preparations cover four Clip plus four Cull distances and sparse Clip indices 0/2 plus Cull indices 5/7. Sparse positive inputs are distinct .125/.375/.75/1; reverse prepares rejection. Static SPIR-V/DXIL must show matched dense 4+4 or 2+2 producer/consumer input dimensions and preserved raw source values/comparisons, no GS distance output semantics and all previous chains signed. No runtime/game oracles run.
`native-port/gcn_winding_test.cpp`

### 122. native-unit-point-dense-distance-outputs

Two new 96-case original GCN loop preparations cover combined Clip4/Cull4 and sparse Clip0/2,Cull5/7 with 64 declared points. Budget is 768 or 512 components, unchanged one ES/GS atomic and one lexical point emit. Static signed DXIL must show the original loaded values in native Clip/Cull output semantics with correct dimensions and point topology, all prior chains signed and ten full-core fixtures compiled/linked. Hardware clipping/runtime/cache oracles remain unrun.
`native-port/gcn_winding_test.cpp`

### 123. native-point-vs-qualified-parameter-transport

Three new 96-case original GCN VS preparations export all 32 source parameters and one VS atomic, with distinct x=.125 and w=.75 from source 0. Two aliased flat inputs and mixed flat/smooth inputs cover sized points; unit mixed uses direct VS/PS. Initial signing succeeded but static PS declared two semantics while the producer had one. Final VS/GS/PS signatures and actual stores/reads must match compact vec2 or two scalar proxies, private misc separated and unit without auxiliary GS. Existing emulated clipping, actual GS and distance modes must sign with all ten fixtures compiled/linked. No GPU/runtime oracles run.
`native-port/gcn_winding_test.cpp`

### 124. native-point-cross-parameter-varying-packing

Three new 96-case original GCN preparations read 32 different scalar sources in alternating x/y/z/w channels or 31 sources plus synthetic four-component clipping. Initial sized modes fail free-slot allocation; initial unit signing terminates with 0xc0000409. Final expected interface is eight float4 locations plus separate point misc/clip data, one VS atomic and all source reads retained. Sized/unit VS, actual GS aliases/mixed/reserved and all distance chains require fresh CPU compile/link/signing and static value/interface proof. No GPU/runtime execution.
`native-port/gcn_winding_test.cpp`

### 125. native-point-ordered-capture-producer

CPU-only ES/GS compilation will cover 256-declared loop x/w and clip/cull cases plus full 32-source copy records. Capture fixture prefixes exclude PS from the producer chain and never permit runtime execution. Wide GCN ring stores use a literal scalar offset instead of overflowing the 12-bit immediate. Actual DXIL must preserve original ES/GS atomics and loops, raw stores and primitive/invocation addressing while containing no emit/storeOutput calls. Ordinary 53 chains and ten C++ fixtures require fresh compile/link evidence.
`native-port/gcn_winding_test.cpp`

### 126. native-point-captured-fragment-attribute-fetch

Six signed typed-reader and linked replay chains; nine GPU modes, 96 cold/warm/facing/flip/cull cases each, including two GS invocations and three draw instances. Full 64-KiB guest results and stencil checked; only known half-UNORM 127/128 rounding has bounded tolerance. Broad native suite failures remain separate.
`native-port/gcn_winding_test.cpp`

### 127. native-point-capture-replay-submission

Production pass and production shader generators run in gcn_winding fixture: 864 GPU cases across nine modes; original ES/GS atomic counters and entire guest memory/stencil checked. Three-instance case verifies cached default-path Required throws before fallback.
`native-port/gcn_winding_test.cpp`

### 128. native-placed-color-transfer-initialization

native_packed_backing_16_8_2 and native_guest_shared_color_layers pass with GPU validation. Packed10 SNORM/SINT/UINT MSAA roundtrips pass; broader backing results are saved externally. Full 64-bit/padding/sample comparisons retained.
`native-port/packed_backing_test.cpp`

### 129. native-present-modes

No separate prepared check recorded in this group; final acceptance not run.

## Pinned upstream limitations (not new port requirements)

Pinned vk_pipeline_cache.cpp skips on-chip geometry and stream output. Native guards for those paths are not counted as missing upstream implementation.
Pinned vk_rasterizer.cpp explicitly excludes depth attachment feedback from its color feedback mechanism. Same-plane depth write feedback is not added as a new requirement.
Raw FMASK decoder/TC-compatible compressed metadata interpretation is not supplied by the pinned fast-clear elimination path; do not expand scope solely because native extension guards exist.

## Post-integration BC continuation

Expanded BC1/2/3/7 CPU-backed RGBA8 now uses typeless storage and typed UNORM/sRGB views of one complete immutable compressed snapshot. Physical layout and all bytes/padding are checked; signed BC4/5/6H FLOAT decodes remain separate. bc_volume_test.cpp adds opposite numeric hardware-reference and same-resource checks, authored/unrun. This does not close the nine unrelated production gaps.

## Ordered interior buffer writers — 2026-10-17

ExecuteGuestResources now admits contained DWORD interior buffer writers in the single-stage, buffer-only, common-UAV image domain. Descriptor ordering uses the first original exact-base writer for I invalidation, while prior offset writers still affect buffer query pages. All clean small reads keep their private CPU snapshots, including those after a writer on another page. Full I retile occurs before logical invalidation; prepared resident aliases share B and preserve padding/exact dirty ranges. Broader layouts, unaligned/partial domains and mixed stages/images remain explicit code gaps.

Eight real-GCN ordered controls are authored/unrun. The full containing C++ fixture and the expanded BC volume fixture passed /Zs with real Release include/define/toolchain arguments. SDK small/far macro conflicts in two fixture headers were fixed by renaming local identifiers. Production core compiled/linked separately; no GPU/game acceptance or FPS evidence.
