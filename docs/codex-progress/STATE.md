## Published clean boundary b1e8e5a — 2026-10-19

Clean source b1e8e5a (production3079ab7) Release PASS21.548s, command-20261019-200704-65376.log, pinned dependencies/no primary-checkout path leak. Sole app core C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current safely replaced, EXE601eb119d054edac9a00611c6f7d252e436e785e12672ab4814110c035f186f8/PDB86f4689d0fdf765e4a5950d2d10d122d0a1aab5fe6211db1858caf40d68f4bdf, isolated profile unchanged, no full archive. Source/fixture commits pushed normally. GPU5of5/GCN linear-MRT scoped verification only; EXACT NEW GAME_NOT_RUN, no36a33da result transferred. All own game/feeder processes stopped. Unknown untracked sources remain untouched.

Next source action: use existing native GCN linear-download-owned fixture (now actual PM4 queue dispatch/rasterizer binding) to inspect R32_FLOAT0.5 source raw GPU bits versus UNORM8 export127/expected128; same case reproduced on36 baseline (200503-64948.log) with only queued-clear fixture patch, restored afterward. Do not change oracle without establishing pinned export/precision contract. Keep SNORM129/128 and almost-black world separate open defects. Newly implemented unaligned page witnesses require the next relevant integrated game/FPS comparison; no rerun of already-passing5 scoped cases without new source change. Latest actual world36a33da26frames/77.18774s=0.336841FPS remains FAILED acceptance. Reports: F:/PS4Dev/handoff/physical-backing-proof-20261019 (unaligned-release.json, unaligned-final-tests.log, prior exact game directory).
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
## Native shifted depth SRV checkpoint — 2026-10-19

Connected production: ImageResource::is_depth now reaches GuestTextureDesc::sampled_depth. HasPossibleReferenceSubresourceBase follows pinned ImageInfo::IsCompatible/MipOf/SliceOf for normal 2D classes, including promoted depth role, format/sample class, logical dimensions/pitch, slice size and complete-child address alignment. A UINT view at an aligned D32 slice remains a separate class/current B, not copied depth I. A micro-tail pointer aligned only to its physical256-byte slice but not its standalone131072-byte child extent is also a reference chance overlap. This corrects the earlier assumption that every aligned depth/color slice needed a missing transfer.

Actual same-class sampled depth subresources use the existing native parent: ReadonlyDepthSubresource -> CompatibleGuestDepthSubview / FindDepthViewParent -> ResolveDepthViewStorage full-parent envelope -> Entry::Srv native mip/layer remap. No I->B transfer, new owner, private conversion or parent retirement. One-level view preserves native mip count1 and selected array slice. An existing valid independent child at the requested base wins over a broader cached parent. This is the pinned right-overlap view contract, not a blanket shifted-copy admission.

GPU verification PASS6/6,80.96s: independent_image_version_transfer3.89, current_independent_image_binding3.88, dirty_owned_depth_refresh7.31, chance_depth_overlap22.44, new shifted_depth_subresources14.67, existing independent_depth_subviews28.74. New test executes actual compute shader SRV loads and GetDimensions for D32 mip0/layer1 and micro-tail mip1/layer0; verifies parent GPU contents/ownership, zero capture/retirement, logical dimensions/mip count, independent-child priority and explicit unsupported writer rejection. Existing PageWatch/depth-subview suite passes separately. New fixture uses ownership adapters; shifted PageWatch/MSAA/D16/paired-S/writer/left-merge scope remains unverified or unimplemented as applicable. No game/FPS/image claim. Main full Release PASS19.262s (command-20261019-185019-50924.log), including guest_compute propagation. Reports/log/fixture hash: F:/PS4Dev/handoff/depth-subresources-20261019.

Do not repeat discarded private retile approach: standalone child macro envelope can exceed the parent's micro-tail domain; forcing guest reads there contradicts the parent-view contract. Small experimental source patch is retained only at build-migration-checks/private-depth-subresource-experiment.patch. Direct native parent view is the final implementation. Earlier private prototype failure and corrected mip-tail chance case were development failures, not accepted results. Normal .cmd diagnostic wrapper does not preserve regex pipes; the six CTests were invoked directly with their exact name regex and full CTest log preserved.

Next: committed clean Release build and safe single-core replacement with new hashes/status; do not transfer797aaeb startup capture to this source. Actual world/control/image/performance acceptance still requires a usable observation surface; the foreground-PID error is unresolved. Continue native shifted DSV writes/left-overlap parent merge and scoped MSAA/D16/paired-depth proofs without reimplementing existing same-base subviews or restarting the129-group audit.

## Installed AJM checkpoint — 2026-10-19, source797aaeb

Production797aaeb936af1db13817e58d9c2b66f3a13c3cc2 clean Release PASS16.201s (command-20261019-181925-44920.log), exact EXE2c7d6aca1ccf8a9f08dc0b22d7e005f1b943ea3ac1ac0e2b0a7fe2fb8f10d3e9/PDB175cba0476ddf666d7cf8510495a11fc7aee2b98c97a2eec1219a29986bef046. Installed sole dx12-current at C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current: all19 file hashes verified, same isolated profile external-7e8fca7a1bd828c19629fdd9, status Experimental, zero staging/replacing directories. Original Vulkan/saves remain untouched. The old user8068 had already ended; publication did not replace a running core. No full runtime archive was created.

Owned PID55400 ran13:20:16–13:22:57Z, explicit termination request without force, exit0. Virtual pad1000 connected, no buttons sent; feeder neutral/stopped and device_removed=true. Window snapshots AND activation failed with foreground window did not report a process id; console session was Active, but lock/foreground cause unknown. No blind input, no confirmed scene, no world/control/visual acceptance. One marker-bounded unidentified-startup window recorded1701 unique fence-complete new guest flips/56.841809s=29.925fps, p95 inter-completion50.041ms, zero invalid rows, pending0 at capture-stop; marker removed before shutdown. This excludes FG/repeated presents but is NOT a comparison to the user's world1FPS or Vulkan baseline and does NOT prove the stall fixed. All runtime/identity/frame/CPU diagnostic/exit/control evidence is in F:/PS4Dev/handoff/ajm-797aaeb; user8068 evidence separately remains F:/PS4Dev/handoff/live-slow-8068. No own core/feeder remains live.

Next: actual world/control/performance acceptance of the exact installed hash once window observation is available; capture canceled batch state if the same FMOD/AJM wait recurs. The cancellation defect has a failing-before/passing-after CPU regression, but causal connection to user8068 remains unproven. Continue the existing graphics port gaps without repeating same-base depth subviews or the chance-overlap fixtures. Shifted native-depth slice mapping remains a concrete unimplemented transfer domain, not closed by independent chance-overlap admission. Keep full suite and extended game acceptance open.

## AJM cancellation completion — 2026-10-19

Reliable paused snapshots (explicit thread resume before detach; postcheck suspended=0/Responding=True) replace stale-context interpretations from nonsuspending stack reads. User8068 Game:Main waits PthreadMutex::Lock (actual mutex60039bf9e0d0 recovered from frame3 RSI; owner800004c40 guesttid14). MOMainThread sleeps2ms at guest658e438 waiting bit0x40 on object210ef5ff8. Its outstanding list218 contains one node2074d1b20 named FMOD stream thread. That threadcc50 waits AjmContext::BatchWait while AjmWorker6908 waits empty work queue. Evidence: F:/PS4Dev/handoff/live-slow-8068/stable-lock-owner-context.txt, stable-mo-guest-context.txt, mo-pending-object.txt. Guest addresses are evidence only; no address-based changes. Exact batch cancellation status was NOT captured: user8068 ended before the follow-up attach (Unable to examine process / process missing). Exit reason unknown. Manifest and both logs preserved as *8068 files; no restart or termination was issued by this turn.

Confirmed source defect, also present in pinned original: WorkerThread skips canceled batches without releasing finished; BatchWait then cannot observe cancellation and destroy its slot. Production AjmBatch::Execute retains actual codec work, processed/canceled state and semaphore publication; WorkerThread now invokes it for every nonnull dequeued batch, including cancellation. Canceled work is skipped and completion is released, allowing the existing BatchWait return ORBIS_AJM_ERROR_CANCELLED. Exceptions are propagated without false completion; no timeout/success workaround and no graphics assertion suppression.

New native_ajm_batch_completion uses the production AjmBatch executor and actual MPSC queue. Baseline semantics-preserving extraction of the old worker behavior FAIL0.19s: Canceled queued AJM batch never completes (command-20261019-181704-54928.log). Fixed test PASS0.07s (command-20261019-181818-32404.log): cancellation without codec execution, normal work/result visibility and no completion on exception. This is CPU AJM batch lifecycle coverage, not full sceAjm API/codec or game causality acceptance. Main full Release PASS8.414s (command-20261019-181737-48144.log). The observed wait chain makes AJM relevant; the user stall is NOT yet proven to be this cancellation defect. No claim of corrected FPS/image or complete port.

Next: clean committed Release build of the AJM fix, then one controlled world/control/performance run using existing XInput-only setup plus completed-guest-frame capture at launch. User8068 is terminal; preserve its diagnostics already saved before reusing the isolated profile. Use exact binary identity; do not transfer old frame/fixture results or package a live core. Broader Vulkan/DX12 parity gaps remain active; no general audit restart.

## Live stall and consumed-EOP diagnostic overhead — 2026-10-19

Read-only live measurement of user8068 at13:06:33–13:06:53Z: VideoOutPort::flip_status.count remained0x17f8 (6136), pending=0, process CPU advanced, Responding=True, suspended threads=0. Exact counter address was derived from this installed3680578 executable's GetFlipStatus/GetPort machine code, not guessed guest offsets. This proves no processed flips during this20s window, not the cause, historical1FPS, or a GPU-fence frame-rate benchmark. Evidence: F:/PS4Dev/handoff/live-slow-8068/flip-count-window.json and flip-count-start/end.txt. All6 nonsuspending GPU-stack samples show Liverpool waiting for work; guest clock implementation matches pinned original. No clock/guest/kernel workaround was made. Diagnostic snapshots can be transient; root producer/wait contract remains OPEN.

Production Liverpool::ProcessGraphics now classifies EOP overlap only when SignalFence actually writes. A write wholly before the current packet keeps a debug record without the unconditional host-log flush; current/unread overlap retains error+flush. The pinned write/IRQ behavior remains unchanged: OnFence, actual num_bytes, TraceGuestWrite, TryWriteBacking assertion and interrupt callback all remain. The saved75532 stdout has1346 overlap reports, all targeting already-consumed storage; they previously forced1346 explicit sink flushes. This removes confirmed redundant host work, NOT a demonstrated fix for the game stall or an FPS gain. Main full Release PASS9.651s (command-20261019-181045-40036.log). Existing PM4 validation fixture PASS0.01s (command-20261019-181054-42676.log); it covers stream/writer validation, not live EOP/fence/GPU semantics. No new game launch.

Clean committed7c2461e Release PASS19.595s with pinned dependencies, zero primary-checkout paths in compile_commands/build.ninja/CMakeCache and required decoder/tile headers tracked. EXE7907af48d9ecab1ff5a2bce2f35842873665a0818d2c9d59962e9242378cc2a5; PDB65a2447e8d076b973591f26e307dfa89309b2433660bf9a17f7f79db7c2ceb11. Exact rebuilt runtime NOT_RUN. Report: F:/PS4Dev/handoff/chance-overlap-7f32403/clean-release.json. Before reusing the clean build, the unresolved835 EXE/PDB/identities were preserved with verified hashes at F:/PS4Dev/handoff/dirty-alias-835ad54/exact-crash. No successful runtime archive was made. Sole installed3680578 remains unchanged while user8068 uses it.

Next: rebuild this committed EOP change in the same clean tree; no acceptance transfer from either earlier binary. Diagnose the live guest scheduling/frame producer stall with actual context rather than assuming buffer cache or missing assets. Keep full-port gaps (including shifted depth slice mapping) explicit; do not duplicate already-implemented same-base depth subviews. Do not start another game while the user owns the active process/profile.

## Current blocker — 2026-10-19: game performance and image acceptance OPEN

Source 7f32403 preserves independent chance-overlapping images without inventing a mip/slice alias. Focused dirty-owned/chance GPU tests PASS27.68s, related independent/current-binding/paired-depth/dirty-recreation tests PASS116.03s. Main Release links; EXE SHA256607f2fc976e4fbfcdd6678bdf7d7a5f3ac72d2dde63624b78f37562c6100a7f6, PDB79f1e501bd3351bfe640b96d2ec0775d0a5f8f928fb3426f65275b04aebc4baa. Exact clean-checkout verification of this commit remains pending; preserve failed835 executable/PDB before overwriting clean build outputs. Evidence: F:/PS4Dev/handoff/chance-overlap-7f32403/verification.json.

One controlled same-binary XInput-only run PID75532 (12:45:05Z–12:55:33Z) reached actual 3D world, camera rotation, character movement and inventory menu; explicit close exit0 and virtual device released. Run environment, redirected output and exit identity: F:/PS4Dev/handoff/chance-overlap-7f32403/xinput-only. Screenshot CUSA03173_20261019_175312_401_game_000000.png under the isolated profile user/screenshots has SHA256ec2d7ffa58a012bfbb9b7c8b37df09e93f7f16d36900e72d44281fa7a048b357. This is scoped scene/input evidence ONLY. No completed-frame FPS capture was enabled; dark scene correctness is not established against Vulkan. User reports approximately1FPS and incorrect rendering: overall game/performance acceptance remains FAILED/OPEN, not a successful emulator acceptance.

User-owned app run PID8068 uses installed3680578 (EXE6093b4fc305a5df33e9086df8960d8cbb2b07295a2ce833f41acb840da896795). Do not stop/restart/control it or replace its loaded package. Read-only3s CPU sample found Game:Main TID52772 near full core. Its saved stack polls posix_gettimeofday; GPU command processor waits for work in that snapshot. Root cause remains UNPROVEN; neither missing assets nor CMASK nor timing implementation is established as cause. Stacks: F:/PS4Dev/handoff/live-slow-8068. Debugger -pv/qd left suspension counts behind: corrected using nonsuspending -pvr and two explicit thread resumes; process8068 subsequently Responding=True, CPU advanced120.19s. Do not mistake the diagnostic suspension for the original slowdown, and do not repeat this attach pattern. Use nonsuspending observation; explicitly verify process resumes.

Next: locate the producer/wait or frame-pacing contract behind the observed guest polling using saved evidence and pinned upstream. Do not change clock semantics or guest memory to bypass polling. The next owned performance run must enable SHADPS4_FRAME_CAPTURE and SHADPS4_FRAME_TRACE_LOG at launch and measure completed guest frames in world, excluding FG/repeated presents. No new uninstrumented game replay. Existing exact slice mapping limitations and unresolved835/d019 crash evidence remain protected. Current profile logs were overwritten by user8068; use own redirected output for75532, never relabel user logs as that test.

# Native DX12 state

## Latest continuation — 2026-10-19, source 7c794a8

Production835ad54 dirty-alias retirement/current-B refresh remains focused GPU verified (new depth/color fixture7.91s and four related tests115.37s). Clean835ad54 Release10.658s, EXE435d4700c53bd94195f493337fcb6cdbd01632f4a86affca03e7bd646561e3d1/PDB80c6cdb8a7016128102cb925146d39ca6fea7d08eec493ae448847f3df7e49b3 now has actual Continue FAIL: Independent depth/color overlap needs an exact physical transfer plan, exit0x80000003, before world. F:/PS4Dev/handoff/dirty-alias-835ad54 retains run/exit/stderr/profile logs and XInput evidence. Initial virtual pad1001 did not control title; neutral detach/reconnect registered1000, and actual intro skip/title/menu/Continue reaction was observed. Do not confuse earlier intro hunter imagery with world.

7c794a8 adds geometry output only at this unchanged overlap rejection. Main ReleasePASS5.364s after exact source commit, EXE72ad3609f0249620792c045725ca61ca7c69f040167e7d4aeb056bdd1dd92ada/PDB83a0d3fd4597ecfa4396057cb20cddf544a0bf07d54046e67e7f9ade05d7989e. One same-PID78660 run reached menu/Continue via virtual1000, then waited on loading until explicit close after9m17s, exit0; no overlap rejection/world/control acceptance. Non-invasive stacks and actual mutex-owner chain saved at F:/PS4Dev/handoff/depth-overlap-7c794a8/result.txt. GPU queue idle; Game:Main waits owner MOMainThread, which polls guest completion bit0x40 via2ms sleeps. Cause of missing completion and exact failed835 overlap descriptors remain unproven, separate blockers. No blanket mapping widening or kernel patch from these observations.

Next: inspect saved mo-caller/context and identify generic completion producer/wait contract using existing code/evidence before another unchanged game replay; obtain rejected overlap geometry on a causally justified controlled failure before adding its physical mapping fixture. Existing dirty-cache/ownership mechanisms must not be reimplemented. Debugger executable is C:/Program Files/WindowsApps/Microsoft.WinDbg_1.2610.1001.0_x64__8wekyb3d8bbwe/amd64/cdb.exe (not SDK path); local -y avoids symbol-server delay. Sole installed3680578 retained; reusable clean835 crash and main7c diagnostic remain, no new binary archives. Own game/debugger/feeder ended and virtual device removed. Unknown untracked untouched. SESSION_HANDOFF.md remains authoritative. Older sections below are historical checkpoints.

## BC prefix alias transactions - 2026-10-19

Production **51abbc717cab66032c60582c20755ffedf57998e** is connected and focused GPU verified. Readonly and writable block-word aliases now derive from one current BC parent transaction; only writer aliases commit back, dependent children cannot be reused across parent snapshots, and shader resource promotion is limited to overlapping mip ranges. All 27 writable BC variants pass sequentially, including shifted/rebased prefixes, duplicate same-mip slots, native BC readers and cross-transaction lifetime. The complete ordered-formatted related suite and focused ownership/PageWatch/binding/subresource checks pass.

The exact clean Release is the sole installed `dx12-current`: EXE SHA256 **acce7997e1ff08562daf05c3400ced4b84b4336032764f166f3d15a60c7d2b32**, PDB SHA256 **c05f51518aa744ad009b4a9da6138afe45bff3953c16f96059fe3cca786d55b8**. Game/runtime/full suite are not run. The broad `native_resources` fixture still has a separate `invalid map<K, T> key` failure and is not accepted or attributed to this mechanism. Next concrete BC gaps are MSAA, clipped and large block domains, each requiring its own contents/ordering/lifetime proof.


## Ordered multilevel BC/word terminal prefixes - 2026-10-19

Production **10e19f15c44dd6313b4d5afa6911c23abeb885e9** extends the verified BC/word converted domain to a terminal UINT block-word prefix of a multilevel BC parent. Admission reuses the cache's strict `MatchesReadonlyBlockMipPrefix` proof: identical base, codec block size, mip-major offsets, pitch, height, slice geometry and complete prefix envelope. Nonterminal and shifted descriptors remain rejected by this path.

Focused GPU verification passed for exact one-level and four-mip-parent/through-mip2 prefix cases in 7.93 s. The latter independently reads the complete early BC parent I and late current-B UINT prefix while preserving padding and the untouched parent tail. The complete ordered-stage matrix passed in 201.34 s; independent image transfer and BC hardware checks pass. Clean detached Release succeeded. The sole installed `dx12-current` is EXE SHA256 **be5b93e9f625041ea388bad028511f9741f5153445b54860ccfba932e9775457**, PDB SHA256 **fc411dd494ca961bcf7ce69b7870ba0acb72b477070de5b2905a7e0d506bcdda**. Runtime/game/final suite remain not run.

Next concrete gap: shifted/rebased BC block subresources and layer subsets from the existing `gcn_bc_writable_alias_fixture.h` variants. Prove the full frontend-to-ordered-stage descriptor path and parent commit with actual GPU writes before widening any shifted-domain guard.



## Ordered one-level BC/word stage domain - 2026-10-19

Production **4def6f26aca64357010b160cc512270acf0fef57** is connected and focused GPU verified. `PrepareOrderedGuestStages` now recognizes exact same-address, one-level BC parent and UINT block-word storage as a converted physical domain. The original stage order is preserved with separate BC3 typeless sampled and RGBA32 typeless writable allocations: the early sampled bind retains logical I, an exact-base buffer writer selects current B, and the late writable bind receives B without retiring the pinned early allocation. Shifted addresses and multilevel BC parents remain rejected.

`native_ordered_bc_word_stage_bindings` passed in 3.95 s with independent GPU readback of early logical BC blocks, late UINT words and current guest padding. The complete ordered-stage matrix passed in 197.42 s; independent image transfer and BC hardware checks also pass. A clean detached Release build of the exact commit succeeded. The sole installed `dx12-current` is EXE SHA256 **480282eac32fd2f3d5235309fad387aec5f6bda0103b9f902fd95e1db065ebf4**, PDB SHA256 **d2f519b111702f1c5c6791ecdc9f28209488232423e32c3cce32bbf2338fdff5**. Runtime/game/final suite remain not run for this identity.

The next concrete production gap is the multilevel BC-parent/word-prefix ordered domain. It must keep untouched parent mips authoritative while materializing the selected word view in original stage order; do not weaken the one-level, shifted, MSAA or large-domain guards without a focused contents/lifetime fixture.



## Ordered converted packed image domains - 2026-10-19

Production **a082b64cfd67f52b692163d9762ece23d67b025c** is connected and focused GPU verified. `PrepareOrderedGuestStages` now preserves original per-stage binding order when one PS4 packed image needs distinct native sampled and writable representations. Compatible `CompatibleGuestPackedStorage` views bind at their actual `BindTextures` position; early conversion no longer retires the preceding I. A later exact-base B writer selects a separate current-B representation, while pins retain earlier descriptors through the submission fence. Compatible single-allocation views keep the previous refresh path. BC, shifted, MSAA, clipped and different-geometry domains remain explicit gaps.

The ordered-stage fixture passed its existing matrix plus a packed10 SNORM converted-domain case with deliberately different early I and late B contents. Both native allocations were read back independently, including the complete tiled guest envelope and padding. Independent image transfer and packed10 SNORM/SINT writer/tiled regressions pass. A clean detached Release build at the exact commit succeeded. The sole installed `dx12-current` is EXE SHA256 **7dac81c4a8f235eb6136f1c75a148d154c8422d073185c55b07819aad794989f**, PDB SHA256 **415a9a739b0f02c9b8797a031d5ae9fad41362d5332a205f300f5306e637713f**. Runtime/game/final suite remain not run for this binary.

The previously recorded late same-address source-envelope hypothesis was not reproduced: depth subview, planned depth native change and current-buffer fixtures already cover the examined growth cases. The next concrete production guard is a BC/word-view ordered stage domain whose physical geometry cannot share one native allocation. Preserve exact guest words, stage order, current I/B choice, descriptor lifetime and ownership; do not weaken shifted/MSAA/large guards without their own fixture.

## Mixed constant MRT blending - 2026-10-19

Production **2b03a4d0dfe07882984e5b8f36babc0a80302e5b** preserves simultaneous constant-color and constant-alpha RGB blending on devices without D3D12 OPTIONS13 `AlphaBlendFactorSupported`. Supported RGBA8 UNORM and packed10 SNORM targets use the existing per-sample raster-ordered destination read/modify/write path; devices with native support keep fixed-function blending. The exact GCN fixture passed eight sample1/4 draws with three MRT equations, both disjoint layer orders, full readback and padding. The state/pipeline, graphics-resource, 136-draw RGBA8 logic, 104-case SNORM and signed shared-MRT regressions pass. Implementation is connected and focused GPU verified for the ordered formats; unsupported mixed-format combinations still reject explicitly, and game/final-suite behavior remains unverified.

The clean committed Release is the sole installed `dx12-current`: EXE SHA256 **44ad271ebea9dfaa59c2a02ea4ef89ea351df23cd103e038c91c011dd114896c**, PDB SHA256 **92f1f80ae80a265442a724d06ea012a02089875cc72f13229515538bcceacf0a**. The application catalogue contains one core and selects it. The previous launch manifest remains factual history for the last launched binary; this core was not launched.

Current storage measurement after publication: C: has 33.450 GiB free, F: has 689.780 GiB free; `F:/PS4Dev/handoff` is 15.097 GiB, `C:/Users/skei/Documents/Codex/handoff` is 0.737 GiB, and the single installed core directory is 0.180 GiB. No new per-test runtime archive was created.

Next boundary is the recorded late formatted-alias source-envelope item. First prove whether the current same-address domain union already reserves every original level/layer and depth/stencil extent before final descriptor selection. If it does, record that scope and advance to the next existing converted/multiview/shifted/MS/large guard; do not invent a production change.


## Per-sample ordered color - 2026-10-19

Production **e132726c97fa794e9e53e4cc8c9d80866b47ebf2** connects ordered color to independent MSAA sample planes and writes them back into the authoritative attachment. Packed-SNORM sample1/2/4/8 ordered blend/logic passed 104 actual GCN draws; RGBA8 sample1/4 independent logic passed 136 draws. Both verify complete contents and padding, with signed, graphics-resource and transfer regressions passing. Clean Release reproduced both focused GPU checks. The sole installed dx12-current has EXE **96b0ef3657ea9fcbd05e27387d57d4de18d70ab80a6c0761bcd88dbd6371a06e** and PDB **1106f8206b62c846ce1c946773191bd2f5465bbdc518bfbdb230c227abdf5f2d**. Game/final-suite behavior is not verified. Packed format 19 MSAA remains connected and focused-verified on the pinned-original-capable AMD adapter; no NVIDIA-only fallback is required by the pinned original. Next gap: late formatted-alias source growth beyond the buffer envelope reserved before descriptor creation.


## RGBA8 independent MRT logic - 2026-10-19

Production **48c9795b6901e94087b1b795ccfeb43bb08e6563** connects single-sample `RGBA8_UNORM` independent MRT logic through ordered `R32_UINT` read/modify/write. All 17 operations and masks `0x1/0xA/0xF` passed 68 actual GCN draws with disjoint array layers and full 64 KiB comparison. The fix also makes ordered UAV descriptors use each actual attachment layer view. Packed-SNORM, shared signed logic and graphics-resource regressions pass. Clean Release reproduced the focused GPU result. The sole installed `dx12-current` has EXE **47a3eb7b55311362e42302e075296271b5398647cb018c3529a04654733af3dd** and PDB **340c1ab9d8d316277023c42283e3c170a901842b8e6883f4bfba0fc95d9820fc**. Game/final-suite behavior is not verified for it. Next production gap: packed-SNORM MSAA per-sample ordered blend/logic; other non-castable independent logic formats remain explicit.


## Independent packed-SNORM MRT logic - 2026-10-19

Commit **bcc852b** is connected and narrowly verified. Packed 10:10:10:2 SNORM now executes all represented logic operations through ordered raw-word read/modify/write when active MRTs require different scattered write masks. The real GCN/GPU fixture passed 44 cases over three MRTs and complete 64 KiB readback; shared SINT logic/MSAA and graphics-resource regressions also passed. Ordinary/non-castable independent logic masks and packed-SNORM MSAA remain concrete gaps. Clean source **88c9326** is installed as the sole validated `dx12-current` core: EXE **47ea2051e7550be9213390c87f6577cc1ad14c6a77a7fc0b93ecb8a098155d46**, PDB **d001421e7987d2abab3d31108a23109a1ccf777813a979a66dded0c3d94daa2f**; game/final suite not run.

## Current production ordered packed-SNORM checkpoint - 2026-10-19

Commit **3b886393c6e7d345bbe298763ed4a9e045f60d91** connects single-sample packed 10:10:10:2 SNORM blending end to end through a rasterizer-ordered `R32_UINT` UAV. Blend constants/state, destination read/modify/write, masks, transitions, barriers, ownership and resource lifetime are connected. The production GCN fixture passed 27 cases; ROV metadata, hardware ordering and guest graphics-resource tests also passed. The implementation is connected and mechanism-verified, while game/final-suite behavior is not yet verified. Independent MRT logic/write masks and packed-SNORM MSAA remain concrete gaps. The one installed core is the clean **825905c** build: EXE SHA256 **337da2b09a7dd5ac247b8e8a25d77e9b466438f29ba81731c42de9c6223eefb0**, PDB SHA256 **c211ed28351b14913694385e877630f9cbaa021c1ca2c764b376ca651eb36227**; catalogue and file-identity validation passed.

## Current ordered-render compiler checkpoint - 2026-10-19

The prepared ROV path passed real D3D12 execution: 64 overlapping pixel invocations produced ordered value 64 in all 16 UAV texels after barriers and readback. Guest color-export lowering and blend/logic selection remain open.

The ROV compiler prerequisite is connected: an explicit pixel UAV binding range is emitted as DXIL rasterizer-ordered metadata, sets the required module feature, is scoped per thread/compile, participates in shader-cache identity and is rejected for invalid stages/ranges. `native_dxil_raster_ordered_metadata` passed, including normal/mismatch/exact/reset/invalid-stage cases, and the Release core links. Renderer behavior is not yet verified or implemented end to end: fragment export lowering, UAV binding/state, blend/logic semantics and GPU ordering/readback remain the next production work.

Installed core 378bda3 still remains the only application core. An exact window capture shows a rendered Bloodborne scene and the controlled run exited 0, but the window could not be foregrounded; active-world input, XInput and completed-frame FPS remain unverified. No controlled process remains running.

## Current packed SNORM and one-core integration checkpoint - 2026-10-19

This section supersedes the 2026-10-18 current-state section below. Branch `fix/mixed-cmask` is at production commit **378bda3aba1771026b9a2160bfca28a3867433b8** plus desktop catalogue-path fix **54ddffbae5a112027ffb074aa98feb85a9203904**. The full pinned shadPS4 port remains **ACTIVE / INCOMPLETE**; original reference revision is **e0a466753a1aacbc8db8253be685bf7ab4d9daae**.

Packed 10:10:10:2 SNORM render-target storage now retains exact four-byte UINT lanes and encodes fragment exports with deterministic NaN handling, clamp and nearest-even signed quantization. Compatible shared-MRT logic operations use valid D3D12 state with independent blend disabled; incompatible independent target masks are rejected explicitly. Shader cache identity is v8. Narrow verification passed: 24 SNORM draws, shared three-MRT XOR, existing SINT and 16 mixed-sample cases, four related native GPU/readback regressions in 16.887 s, and shader reload in 4.849 s. Evidence is under `F:/PS4Dev/handoff/storage-cleanup-20261018` in `SNORM_FUNCTION_VERIFICATION.json`, `snorm-final-gcn.json`, `snorm-related-native.json`, and `snorm-shader-reload.json`. Whole-group and final-suite acceptance remain open.

A clean detached Release build at 378bda3 used 54 pinned dependencies, 2283 compile inputs and no primary-checkout source paths. The application has exactly one package at **C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current**: EXE SHA256 **38b517c62b0a0dd04fa41489aa0a305b4c7ab0d59194ecf3462cd5a7b1f45350**, PDB SHA256 **03359c8ac054bd3127e5701cbf5322b0502719509bc2d03c8aabf4e98b5db057**, 19 manifest-tracked files, and preserved profile `external-7e8fca7a1bd828c19629fdd9`. Application catalogue and selected-core validation return success.

One controlled integration run of that exact EXE on a copied profile lasted 84.632 s, loaded verified DX12 runtime identities, produced 549 new valid GPU-completed guest frames after the marker, and exited 0 after graceful close. No unhandled application exception occurred and original profile save/config hashes stayed unchanged. This is **not** a gameplay or performance acceptance result: the window helper captured another application and could not activate the game window, so world/image/XInput were not verified and no FPS or speedup is claimed. See `INTEGRATION_RESULT_378bda3.json` and `integration-visual-limitation.json` in the same evidence directory. Do not repeat blind input; finish visual/XInput acceptance only when the actual target window can be observed.

Storage policy remains one current DX12 core and reusable build trees, with no per-test runtime archives. The completed cleanup removed 3310 obsolete outputs and 26 old installed packages; measured cleanup gains were C: 23.028 GiB, F: 42.155 GiB, D: 6.649 GiB. Current free space is C: 33.616 GiB, F: 690.124 GiB, D: 165.040 GiB; later unrelated cleanup is not credited to this project checkpoint. Protected unresolved d019 evidence remains. No launcher/core/debugger process is running.

Next production boundary is exact ordered post-blend SNORM quantization, followed by an ordered implementation for independent MRT logic write masks. Existing guards are not parity closure. Keep polygon point fill, mixed blend constants, attachment feedback and other recorded gaps explicit. Reuse completed checks and existing build trees; after changed code, run only the focused contents/ordering/lifetime checks and safely replace `dx12-current` after a clean committed build.


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

Focused large-copy diagnostic: `before=0 prepared=0 copied=1`; source preparation keeps the list pending, CopyBuffer/CopyBufferBatchImpl submits it. Only the test's failure message changed; the original oracle remains. Production/runtime is still7a59dd2; H/copy-diagnostic-followup.json records this separate fixture identity. Root cause inside copy remains open.

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


## Manual core folders — current user instruction, 2026-10-17

The user now requires manual folder selection. Do not automatically copy new core builds into the application catalogue, install them, or change the selected core. This supersedes older automatic core installation instructions, including the final-port installation step. Leave future builds in named artifact folders and report their paths.

Folder selection is implemented and installed in C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/PS4Desktop.exe. UI: «Версии ядра» -> «Выбрать папку с ядром…» -> «Выбрать ядро». Supports existing manifests or raw full native Release folders. Files stay external; persisted hashes reject changed/missing/additional runtime, including between launch preparation and actual process start. Raw builds use isolated profiles and are labelled unverified in game. Existing settings/core selection/catalogue and original saves were not changed.

Source commits48396fd and a545b9f; installed source `a545b9f0b8dc322484230f10ff92db803394a8bf`. Application DLL SHA256 `ddc452b62fa86196661e3d2774ff45aa9e260104cc34f959bb81e78cdf7f748c`. Release publish succeeded; pure filesystem/settings tests passed in both published and installed host. Latest test log: F:/Codex-Shared/devtools/logs/command-20261017-010304-62100.log. These checks exit before WinForms initialization, create temporary dummy files, prepare launch parameters without executing them, and never start a core/probe/game or operate user UI. Renderer/game suite remains deferred. Installed GUI appearance was not interactively inspected.

Deployment backup, payloads, hash manifest: F:/PS4Dev/handoff/desktop-folder-core-20261017/. Only application payload files were updated; published .NET10.0.12 runtime files already matched the installed runtime. No user application was stopped and no automatic UI launch was performed.

Continue the renderer source task in NEXT_ORDERED_BUFFER_WRITERS.md; the prior turn established its exact existing guard and fixture locations. No production offset-writer patch has been made yet. The reproducible core remains6cbaef7 in F:/PS4Dev/handoff/integrated-release-20261016/artifacts-6cbaef7.


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

## Current pointer - one-level BC / word views, 2026-10-16

SESSION_HANDOFF.md and bc-single-level-20261016/CHECKPOINT_FINAL.json are authoritative.
Exact sole-mip word views now preserve the BC parent for readonly and writable paths.
Twenty-seven GCN scenarios plus macro address controls authored/unrun; all129 groups partial.
No builds/tests/game/UI; preview24 and user session unchanged. Previous sections historical.

## Current BC block image bridge - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

fdb1e27 is pushed: retained UINT64/128 I -> BC, reverse UINT block views and samecodec
numeric BC views via exact physical layout, canonical readonly BC ownership and block-aware
retile. Independent review's generic fallback bypass was corrected before commit.
Two real-GCN BC1/BC3 cases extend the older28 authored cases, all uncompiled/unrun. MSAA
source fixes/focused mode remain as saved earlier. No new core was built/installed/selected.
Next exact production task is per-mip BC block correspondence and ownership. Read
SESSION_HANDOFF.md / NEXT_BC_MIP_CONTRACT.md; preserve scope, residual/index and execution gate.



## Current MSAA depth/stencil source corrections - 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

0682d65/9b5b356 are pushed: full selected-plane placed MSAA depth initialization before
upload draw and unsigned shader type for actual stencil SRV extraction. Other planes/layers,
partial preserved pixels, guest metadata/ownership and fence leases stay protected.
One focused depth-init/stencil extraction mode is authored/unrun; older28 ordered cases and
heap mode remain separate unrun coverage. No new core is built/installed/selected.
See SESSION_HANDOFF.md and msaa-depth-init-20261016 exact proof/recovery. Partial fresh-resource
lifecycle and broader depth/MSAA/alias/runtime acceptance remain open. Continue the bounded
compressed-copy producer/ownership source contract; BC identity-alone expansion lacks a
reachable producer. Do not redo saved BC/geometry/heap/readonly proofs.



## Current actual heap pressure - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
No configure/build/package/compiler/test/GPU/game/XInput/UI execution. All129 groups
remain partial/open; source/Git/hash reviews do not establish runtime correctness/FPS.

c6e5429/50860ef are pushed: typed actual-heap admission, adaptive remaining-budget pages,
independent MSAA heap alignment, shared retired-pool/LRU recovery, transitive caller/victim
pins across recursive resolution and eager unleased-heap reclamation after budget shrink.
e00c72d adds one authored/unrun focused heap mode; ordered28 cases remain separately unrun.
No core was rebuilt/installed. Inherited casting/large-mode integration remains residual.
Next confirmed source gap: first MSAA depth upload draw needs legal metadata initialization
with layer/plane preservation. Read SESSION_HANDOFF.md and the new pressure evidence.


## Current readonly history and source selection - 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only continuation: no configure/build/package, fixture/test/compiler execution,
GPU/game/profiling/XInput/UI operation. All129 contract groups remain partial/open.
No current runtime PASS, image acceptance, FPS or performance result is claimed.

36b109c/01f916c/d55fb2d are pushed: sticky modified/dirty history, current readonly refresh,
original-slot write classification, failure guards and clean-I preservation for direct
and existing packed conversion/reference-compatible uncompressed color paths. Four additional GCN scenarios (28 total in this mode) are authored/unrun.
Current source is not built/installed; preview24/selection remain unchanged. See the
new authoritative SESSION_HANDOFF.md top and readonly-revalidation-20261016 evidence.
Remaining work includes readonly identity controls, the bounded reinterpretation source
boundary, pressure/retirement and existing registry contracts; no broad semantic closure.


## Current ordered ranges and original-slot state - 2026-10-16

Authoritative current section; earlier current/next statements are historical.
Source work only: no build/configure/package, test execution, GPU/game, profiling,
XInput or UI operation. All129 groups remain partial/open. No runtime PASS or new FPS.

Pushed ccacee9/e877a3f/4f66433/f9ec214/792f3fe add varying exact writer extents, ordinal
query-page facts, unreflected Guest/image preparation, multiple disjoint domains and
valid grouped SSBO annotations.24 GCN scenarios are authored only. Installed packages
and selection were not modified. Next concrete source gap is sticky image modified
history across buffer invalidation and readonly refresh; reflection-gate removal alone
does not restore it. SESSION_HANDOFF.md is authoritative; evidence/final source/index
manifest: F:/PS4Dev/handoff/ordered-writers-20261015/CHECKPOINT_FINAL.json.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

This section is authoritative; earlier current/next statements are historical.
Source commits d4b70f9,4210e31 and4ef80fc are pushed on fix/mixed-cmask.
No build/configure/package, test/fixture execution, GPU/game, XInput, profiling or UI
operation occurred. All129 registry groups remain partial/open. Source reviews and
Git/archive checks do not establish runtime correctness or performance.

Early clean reads now keep immutable private CPU snapshots. Native compiler SSBO alias
permissions and cache version are corrected. Ordered readonly short/long extents keep
original shader bounds; writers still cover a full image.16 GCN cases are authored only.
Same-arena/same-submit sparse backing is conditional; no runtime closure. Installed
preview24 and selected preview23 are unchanged. Next source task: exact-base short writer,
with original dirty ranges and a readonly retile envelope only when needed.
Authoritative recovery: SESSION_HANDOFF.md; F:/PS4Dev/handoff/ordered-stream-20261015/CHECKPOINT_FINAL.json.


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

Current source6f55ea1 owns callback/resource leases before queue execution and latches submit failures to prevent replay. Reviewed source only; no compilation/runtime. See SESSION_HANDOFF.md.

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

92af292 baselines the full fixture output region via fenced GPU readback before chunk writes.
Earlier source review validates the readback lifetime but not this addition; fixture remains
unrun and lacks special-operand/sampler-mask and reverse-view assertions. Writable selected
reverse aliases fail closed; writable parity, nonzero mips, and dirty-parent refresh remain open.

## Current selected BC mip view - 2026-10-16

923d637 pushed: readonly mip0 UINT view over multi-mip BC parent snapshots privately and
keeps the parent authoritative; no child ownership/parent retirement. Prefix validation
uses allocation mip0 independent of the parent's shader view selector. Source-only; fixture
oracle and review remain pending, and no runtime acceptance is claimed. Writable and nonzero
selected aliases remain open. See SESSION_HANDOFF.md and bc-mip-bridge-20261016.


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

## Emergency handoff — 2026-10-13 (authoritative current boundary)

Read [SESSION_HANDOFF.md](SESSION_HANDOFF.md) first; older current-state sections
below retain history. Development stopped at the user's emergency checkpoint.
Source WIP `c0aa3e4a6fb2de072b29ffa4d0682d5accd9c292` preserves 19 known point/capture
files. Remaining broader changes, original index and all new files are retained
in the tree and externally at `F:/PS4Dev/handoff/full-port-emergency-20261013`.
Source group126: typed captured-FS SRV reader, Stage21/pipeline4/metadata30;
core237/checks190 compile/link passed. Fresh typed-reader signing/reflection and
runtime replay remain unverified/unimplemented. Production capture flag stays
false; 126 partial groups, last audit 1,501 associations/13 mappings/zero closures.
Latest installed preview18 includes only through124/core232. No current game,
GPU or FPS acceptance; no new tests/builds were started for emergency handoff.
Final checkpoint SHA/status/push result are in external CHECKPOINT_FINAL.json.

## Current task — full original e0a46675 port, implementation in progress

See [full-port status](full-port-e0a46675/STATUS.md) for the new reference,
common-core and GPU tiling integration, pinned-dependency Release build and open
contracts. No tests or game launches have run in this task; prior acceptance
below belongs to the previous core. The full implementation boundary is open.
The user-requested current snapshot is available in the existing installed
desktop catalogue as `dx12-20261013-full-port-preview-519d8356217d`, named
`DX12 — перенос №18 — 13.10.2026`, experimental,
with a separate copy of saves. Static package hashes/imports passed; game readiness
is unverified. The previous selected default is preserved and migration continues.
Preview ten includes groups through 100/core 201: in-flight physical backing
cancellation, nonblocking linear/exact-expanded pressure source residency,
queued color targets, CPU/GPU image source selection, physical channel clears
and R32 float/uint/sint/raw-float MSAA clears. All ten full-core fixtures compile
and link only; 100 groups, 1,483 associations and zero semantic closures.
Current source additionally includes groups 101–109: exported VS/actual GS
point size, native/emulated clip/cull, layer/viewport transport, safe geometry
metadata, late per-viewport homogeneous Mesa conversion and viewport-edge point
footprints. Core 215/checks 169 and all ten full-core fixtures compile/link;
thirteen shader chains compile and sign offline before device creation. The
109-group audit has 1,492 associations/13 mappings/zero semantic closures.
Prepared GPU/runtime oracles remain unexecuted. This later code is included
in immutable preview eleven. Its 76 manifested files/imports were checked
statically; 20 saves were copied from the still-selected first preview profile.
Installed application/settings and previous packages were preserved. No game
launch or hardware acceptance was performed; full migration continues.
Current source additionally includes group 110, ordinary-point VS interface
liveness, safe sparse clip output mapping and stage v8/metadata v14. Core
216/checks 170 and all ten full-core fixtures compile/link. Winding preparation
now binds color before frontend translation; sixteen offline chains sign with
actual MRT0 outputs, including all-32-export/live-parameter/VS-atomic modes.
The audit has 1,493 associations/13 mappings/zero semantic closures; GPU oracles
remain unexecuted. These later changes are excluded from immutable preview eleven.
Source group 111 extends liveness to actual guest point GS and its output budget,
including 32 float4 copy exports with two real GS point emissions. Stage v9 /
metadata v15, core 217/checks 171, all ten full-core compile/link fixtures and
eighteen offline signed chains pass. Static ES/GS atomics/emits/live interfaces
are inspected; hardware oracles remain unexecuted. The 111-group audit has
1,493 associations/13 mappings/zero closures. Immutable preview twelve includes
these groups through 111/core 217, with 76 manifested files checked and 20 saves
copied from the selected first preview profile. Previous packages/application/
settings/default remain intact; no GPU/game acceptance. Genuinely high live GS
output budgets/capture fallback remain open; full migration continues.
Source group 112 adds a conservative acyclic EmitVertex bound for expanded
point GS while preserving original bounds for any loop and original copy
strides. Metadata v16, core 218/checks 172, all ten compile/link fixtures and
21 offline signed MRT chains pass. Static artifacts show eight declared corner
vertices for the two acyclic large-declaration modes, versus 256 with a real
loop. The corrected new-fixture S_MOV_B32 encoding and earlier failed compiler
attempt are recorded in full-port status. The 112-group audit has 1,494
associations/13 mappings/zero closures; runtime/GPU acceptance remains unrun.
These changes are excluded from immutable preview twelve. Full migration
continues, including capture fallback for genuinely high live GS budgets.
Immutable preview thirteen includes group 112/core 218. Its 76 manifested
files/imports and exact source/compiler provenance were checked statically;
20 saves were copied from the still-selected first preview into a separate
profile. Previous packages/application/settings/default are preserved. No
runtime or hardware acceptance; full migration continues.
Source group 113 emits hardware unit points for actual point GS with no size
export, retaining native point liveness/clip conversion, output state and real
viewports/Mesa flips. The inherited operandless OutputVertices topology typo
is corrected to OutputPoints. Metadata v17, core 220/checks 173, all ten final
compile/link fixtures and 24 offline signed MRT chains pass. Static SPIR-V and
DXC disassembly confirm real point topology and loop maximum 256; ES/GS atomic
instructions are retained. Audit 113 has 1,495 associations/13 mappings/zero
closures. No runtime/GPU oracle ran. These changes are excluded from immutable
preview thirteen; large variable-size GS budgets/capture remain open.
Source group 114 narrows actual point GS/point fragment parameter prefixes
through the highest consumed component, preserving logical indices, guest
computations and side effects. Stage v10/metadata v18, core 221/checks 174,
all ten final compile/link fixtures and 27 offline signed chains pass. Three
real-loop x/z/w cases show GS/PS widths 1/3/4 and native component budgets
960/1008/1024; DXC instructions retain PS columns 0/2/3. Audit 114 has 1,496
associations/13 mappings/zero closures. No GPU/runtime oracle ran. These
changes are excluded from immutable preview thirteen; sparse packing/full
live interfaces/genuinely excessive output capture and acceptance remain open.
Source group 115 compacts sparse actual point GS channels with matching GS
store/PS read index maps, preserving guest values and effects. Stage v11 /
metadata v19, core 222/checks 175, all ten final compile/link fixtures and 28
offline signed chains pass. Static x/z/w/x+w widths are 1/1/1/2 with budgets
960/720/960/1008; DXC retains original values and compact PS load columns.
Audit 115 has 1,499 associations/13 mappings/zero semantic closures. Runtime
and GPU oracles remain unrun; aliased consumers/full interfaces/genuinely
excessive output capture and acceptance remain open. These later changes are
excluded from immutable preview thirteen; full migration continues.
Immutable preview fourteen includes groups through 115/core 222. Its 76
manifested files/imports and exact source/compiler provenance were checked
statically; 20 saves were copied from the selected first preview into a
separate profile. Previous packages/application/settings/default preserved.
No runtime or hardware acceptance; full migration continues.
Source group 116 reuses identical-interpolation fragment declarations for
multiple inputs from one actual point GS parameter. Metadata v20/stage v11,
core 223/checks 176, all ten final compile/link fixtures and 30 offline signed
chains pass. Initial duplicate Location declarations produced an unmatched
TEXCOORD1; final GS/PS signatures match and retain compact x/w reads. The
default-input fixture is corrected from unsupported original MOV/default
handling to the original P2 default path. Audit 116 has 1,499 associations /
13 mappings/zero semantic closures. No GPU/runtime oracle ran. These later
changes are excluded from immutable preview fourteen; mixed-interpolation
aliases/full interfaces/genuinely excessive output capture remain open.
Source group 117 derives matched qualified point GS/PS transport plans:
equal interpolators share masks; other qualifiers use free proxy locations
without replaying guest code. Stage v12/metadata v21, core 224/checks 177,
all ten final compile/link fixtures and 31 offline signed chains pass. Static
SPIR-V/DXIL confirms matched scalar flat/perspective locations and values,
budget 1008 and unchanged GS/ES atomic instructions. Audit 117 has 1,499
associations/13 mappings/zero closures. GPU/runtime/cache acceptance remains
unrun; host-slot order/reservation and full capture/signature limits stay open.
These changes are excluded from immutable preview fourteen; migration continues.
Source group 118 provides selected original signed viewport transforms for
native clip-disabled VS/point GS conversion. Runtime adds 16 float4 records
at byte 560 (816 bytes/1024 CBV), retaining Mesa's prefix and point fields;
stage v13/metadata v22, pipeline v4. Core 225/checks 178, all ten final fixtures
compile/link, and 35 chains compile/sign offline. Static DXIL uses the actual
raw viewport input at byte 20 and original transform arithmetic; a fixture
counter overwrite of v9 was corrected to v10. The reserved-slot mixed case
also has matched flat/smooth channels and .125/.5/.75 values with budget 1008.
Audit 118 passes with 1,500 associations/13 mappings/zero closures; 68 final
commands, 323 existing deprecated checks warnings, no unexpected diagnostics,
clean whitespace. All runtime/game/cache/W/depth/window-offset and full-port
acceptance remains unrun/open. Snapshot fifteen is installed with all changes
through group 118/core 225, SHA256
`499eb21d6c50132c80368002c706d5987226a46fab792b29f8c46c3f2c5ecbea`.
All 76 manifested files/imports/dependencies pass static verification and 20
save files were copied/hash-checked in its isolated profile; the selected
default and other packages remain intact. Packaging passed
(`command-20261013-012912-43340.log`, 36.460 s). The first packaging attempt
stopped before staging because a summary log matched the offline JSON glob;
summary logs are now excluded. No runtime/game acceptance; migration continues.
Source group 119 keeps sized actual GS clip/cull distances private for point
center checks, reducing its corner output budget to actual raster outputs.
Four new GCN loop modes (384 prepared cases) compile/sign with 256 sized
corner vertices/budget 1024 or unchanged 64 unit points/ClipDistance. Raw
loads, all eight combined comparisons, one loop and GS/ES atomics remain.
Stage v14/metadata v23, core 227/checks 180, all ten final fixtures and 39
offline chains pass. Audit 119: 1,500 associations/13 mappings/zero closures;
72 final commands without diagnostics, initial checks only 45 existing
deprecated warnings, whitespace clean. Auxiliary outputs were retained;
static review found an existing VS ClipDistance / GS CullDistance input
mismatch because linked Mesa leaves input_clip_size unset. The next compiler
fix will supply the producer clip length. Full signatures/capture and all
hardware/runtime acceptance remain open; immutable preview fifteen excludes
119 and migration continues.
Source group 120 fixes linked Mesa input_clip_size using the actual preceding
graphics stage's clip length, captured before compilation. Pinned b628375f
patch SHA256 `d4d16e7054beb94acdf45beeb7e68b5380ac391f2aa189f01970d2ebeaf18b2b`
(48,144 bytes), generated source restored, linked header/ABI unchanged.
Stage v15/metadata v24, core 228/checks 181, all ten final compile/link fixtures
and 40 signed offline chains pass. Static VS/GS ClipDistance0/1 and
CullDistance0/1 signatures match exactly; new cull mode prepares 96 cases.
Audit 120: 1,500 associations/13 mappings/zero closures. Seventy-three final
commands retain 20 reviewed Meson CMake/configuration warnings and two
capability probes; no C++/link errors, whitespace clean. Runtime/cache/game,
combined clipping/tessellation and full acceptance stay open. Changes through
120 are excluded from immutable preview fifteen; migration continues.
Source group 121 compacts active sized-VS point clip/cull values in original
component order with matched auxiliary GS inputs. The GS still checks all
original center distances and now omits redundant distance outputs; emulated
generic clipping, units/nonpoints and actual-GS private arrays are unchanged.
Two new modes prepare Clip4/Cull4 or sparse Clip0/2,Cull5/7 (192 cases).
Static signatures match 4+4/2+2 values; sparse loads 20/28/40/48 and all four
comparisons remain, with shared xy/zw physical row and no GS distance output.
Stage v16/metadata v25, core 229/checks 182, all ten final compile/link fixtures
and 42 signed chains pass. Audit 121: 1,500 associations/13 mappings/zero
closures; 75 final commands, 45 existing deprecated warnings, zero unexpected,
whitespace clean. Runtime/game/cache/MSAA/NaN/boundary and full capture
acceptance stay open. Snapshot sixteen is installed with all changes through
121/core 229, SHA256
`b423844202bf4b5fd77e0826f3282290763480dfb19d4342e0180e15d765ff15`.
All 76 manifested files/imports/dependencies pass static checks and 20 save
files are copied/hash-checked in its isolated profile; the default and prior
packages remain intact. Packaging passed
(`command-20261013-015112-75952.log`, 34.845 s); no tests/game launches, migration
continues.
Source group 122 densely emits active unit-point GS native clip/cull outputs,
with matched declarations/store indices/snapshots/restoration and budget;
hardware clipping and point topology remain. Two new GCN loop modes prepare
Clip4/Cull4 or sparse Clip0/2,Cull5/7 (192 cases), 64 declared points/budgets
768/512; clip-only budget is now 512. Static native output signatures, original
20/28/40/48 loads, one loop/point emit and ES/GS atomics remain. Stage v17 /
metadata v26, core 230/checks 183, all ten final compile/link fixtures and 44
offline chains pass. Audit 122: 1,500 associations/13 mappings/zero closures;
77 final commands with no diagnostics, whitespace clean. All hardware/runtime/
cache/MSAA/NaN/boundary and large capture acceptance stay open. Immutable
preview sixteen excludes 122; migration continues.
Source group 123 shares qualified point transport with ordinary sized/unit VS
points. Actual host component masks drive VS declarations/stores, auxiliary
GS forwarding and private misc allocation; flat aliased x/w use one vec2,
flat-x/linear-w use separate scalars. Synthetic clip inputs keep original
indexing. Three new modes prepare 288 cases, preserving .125/.75 and one VS
atomic. Static VS/GS/PS interfaces match, sized GS emits four corners, native
unit VS needs no GS, and emulated clip retains all four input columns.
Stage v18/metadata v27, core 231/checks 184, all ten final compile/link fixtures
and 47 signed chains pass. Audit 123: 1,501 associations/13 mappings/zero
closures; 89 final commands retain 294 existing deprecated warnings and zero
unexpected diagnostics, whitespace clean. Full varying packing/capture,
qualifiers, runtime/cache/MSAA/precision and game acceptance stay open.
Preview seventeen is being packaged; full migration continues.
Snapshot seventeen is installed with all changes through 123/core 231 as
`dx12-20261013-full-port-preview-95d73764dd3d`, SHA256
`95d73764dd3d3e277539a8a11376457f78a775a242584d386e2a68abe071da93`.
All 76 manifested files/imports/dependencies pass static checks; 20 saves are
copied/hash-checked in an isolated profile, retaining default/older packages.
Packaging passes (`command-20261013-021204-56080.log`, 35.117 s); no tests/game
launches, zero closures, full migration continues.
Source group 124 packs different point sources with identical primary/auxiliary
qualifiers into shared float4 rows, retaining source mask/offset mapping and
matched VS/GS/PS declarations, stores, snapshots and auxiliary forwarding.
Initial 32-scalar sized / 31-scalar-plus-clip modes fail free-slot allocation;
unit signing terminates with 0xc0000409. Three new modes prepare 288 cases,
using distinct source/channel values. All 32/31 source stores and 32/35 PS
reads match in eight rows; clip columns and four auxiliary forwards per value
remain, one VS atomic, green 64/62. Actual GS reserved/mixed packs two flat
sources into vec2, retains separate smooth and one GS atomic. Stage v19 /
metadata v28, core 232/checks 185, all ten compile/link fixtures and 50 signed
chains pass. Audit 124: 1,501 associations/13 mappings/zero closures; 94 final
commands retain 294 existing deprecated warnings/zero unexpected diagnostics,
whitespace clean. Full signature/large-GS capture and qualifiers/runtime/cache/
MSAA/precision/game acceptance remain open; immutable preview seventeen
excludes 124, full migration continues.
Supplemental 124 adds three 32-source packed modes with flat alternating with
smooth center/centroid/sample (288 more cases; 576 packed cases total).
Distinct values, all 32 PS reads, eight host rows and four auxiliary forwards
match; one VS atomic and original front face remain. DXIL qualifier rows
match; only sample enables SampleFrequency=1. All 53 chains compile/sign
offline; latest winding compiles/links. Combined final evidence has 109
successful commands, 294 existing deprecated warnings/zero unexpected.
Preview eighteen is being packaged; full migration continues.
Snapshot eighteen is installed through 124/core 232 as
`dx12-20261013-full-port-preview-519d8356217d`, SHA256
`519d8356217d04c9b8c12251bbd1efc014ac4f86d8325e08d3eca7614a180f87`.
All 76 manifested files/imports/dependencies pass static checks; 20 saves
are copied/hash-checked in an isolated profile; default/older versions remain.
Fresh audit 124: 1,501 associations/13 mappings/zero closures. Packaging
passes (`command-20261013-022324-38672.log`, 34.982 s); no tests/game launches,
full migration continues.
Source group 125 adds opt-in point-GS capture, production off. Private outputs
write fixed primitive/invocation/emission slots to raw u0 space33; shared
152-word/608-byte records retain raw center/size/layer/viewport, original eight
clip/cull indices and all 32 source float4 records. Ordinal saturates; no append/
guest replay/hardware emits. CPU planning caps DXIL byte addressing; valid
bits require producer completion/barrier because Mesa merges payload stores.
Wide fixture ring stores use literal scalar offsets instead of overflowing
12-bit offsets. Five producer chains sign, including 256-declared loops, full
32 sources and two GS invocations. Static proof preserves ES/GS atomics,
loops/addressing and zero raster outputs/emits. Stage v20/metadata v29, final
core 235/checks 188, all ten compile/link fixtures, 53 ordinary + five producer
chains pass; 104 final commands have zero diagnostics, whitespace clean.
Audit 125: 1,501 associations/13 mappings/zero closures. Replay, allocation/
clear/barriers/fences/pressure, instances/indirect/tessellation/streams and all
runtime/game equivalence remain open. Next: flat record id to replay PS, raw
attribute fetch avoids full 128-component signatures. Immutable preview
eighteen excludes 125; full migration continues.
The previous second snapshot `7e7092ff4b78` includes coherent packed10 numeric views, integer
nearest sampling, R32/RGBA8 shader reinterpretation and the removed scene depth
bypass. Core/checks and all ten full-core fixtures compiled only; metadata v6 /
stage-pipeline record v4, unit PointList, anisotropy-one filtering and DRED UTF-8.
Its 76 manifested files and 15 PE import tables were checked statically. Twenty
save files were copied from the selected previous preview's isolated profile;
installed settings, the installed desktop executable and the old packages stayed
unchanged. The first snapshot `95d72e06d1ac` remains available.
After publishing snapshot two, production additionally gained separate prewrite
buffer/image read views for either single writer. Sampled-image captures precede
writable buffer preparation; image writers retain prepared read-only buffer leases.
Two writers and writable buffer/attachment overlaps still fail explicitly. Core,
checks and all ten full-core fixtures compiled only. Five new GPU cases are
prepared but unexecuted. Registry now has 44 groups, zero semantic closures;
the later buffer/image work is not included in the immutable second snapshot.
Production then gained manual AnisoPoint/AnisoLinear ratios 2/4/8/16 with
major/minor gradient footprints, centered multi-tap filtering and existing
mip/address/reduction behavior. Explicit LOD/LZ now includes sampler LOD bias.
The core and checks compile; 24 additional GCN content/cache cases and their
independent CPU oracle compile only. No tests were run. Hardware differential
semantics, extreme gradients and inactive-tap cost remain open. Shared native
bias bounds are now asserted against the SDK; read-only point/mip anisotropy
uses manual filtering, and shader specialization tracks hardware/manual changes.
Cube coordinates now retain the original 2D-array face lowering, including
multiple cubes. The prepared GCN set has grown to 36 cases; compilation only.
This later source work is also absent from immutable preview two. Continue the
main migration; a requested experimental package does not end this task.
The later single-writer image snapshot now also has a GPU path: resident buffer
copy before current writes, followed by direct GPU detile from an immutable
default-heap input. Source allocations and transfer resources remain leased.
Aligned supported layouts use at most 64 MiB of unique transient captures per
batch; decoded/odd-byte/stencil/MSAA/larger cases keep coherent CPU capture.
Transfer-pool growth is reserved without evicting the current entry, and switching
back to CPU capture replaces default heaps before mapping. Core and checks
compile only; the two alias fixtures now prepare GPU/CPU/GPU transition contents.
Registry has 46 groups, zero semantic closures. These changes also postdate
preview two; continue all remaining implementation before runtime acceptance.
The current user-requested immutable snapshot is now preview three,
`dx12-20261012-full-port-preview-8797e98ab807`, named
`DX12 — перенос №3 — 12.10.2026`, SHA256
`8797e98ab8074a07b1675a06a0ed22a0fef06800e530a7511f592ccc951426c2`.
It includes the later anisotropy/cube and CPU/GPU single-writer snapshots.
Its 76 manifested files and 15 PE import/delay tables passed static verification;
20 save files were copied from the selected first preview's isolated profile.
Settings, selected core, installed desktop and previous selected package remain
unchanged. Instructions are in `outputs/DX12-preview-3-20261012` of the task.
No runtime verification occurred. Continue the full port after this package.
Production after preview three accepts shader buffer writers overlapping
read-only depth/stencil planes, including a nonoverlapping writable other plane.
It captures complete prewrite contents, preserves consumed HTILE masks and
rejects actual competing plane writes/clears. Mixed DSVs use per-plane barriers;
partial readbacks commit only the written plane and remove whole-image equality
proofs. Shader UAV draws use the existing OM path. Core/checks and all ten
full-core fixtures compile/link only. Four new attachment fixture modes prepare
read-only/mixed planes with eager and GPU-owned backing. Registry has 47 groups,
1,446 associated functions and zero semantic closures; no runtime checks ran.
These latest changes are absent from the immutable third preview. Continue the
remaining full implementation; packaging a requested snapshot does not end it.
The subsequent production extends prewrite depth/stencil versions to image UAV
writers. Snapshot uploads keep current image owners acquired after capture and
allocate separately from GPU-owned or bound cache candidates. Cache binding pins
now protect identities/pools independently of fence retirement; bindings retain
actual placed texture/sample allocations, and early depth feedback holds a pin.
Pin counters outlive cache destruction safely instead of dereferencing old entries.
Four image/depth GCN modes and a shared-lease post-fence invalidation fixture are
prepared, compiled only. Core/checks and all ten full-core fixtures compile/link;
registry has 49 groups, 1,447 associated functions, zero semantic closures.
Subsequent production distinguishes depth and stencil SRVs on an attached
resource. A shader may read one read-only plane while the other plane writes;
the sampled writable-plane guard remains. DSV flags follow actual draw writes,
and beginning clears of read-only planes record before shader/depth read states
using a separate writable view. Ownership still covers cleared planes. Render
passes require Options18 RenderPassesValid and matching read-only flags; absent
stencil uses NO_ACCESS. Twelve HLSL content/clear/rejection cases compile only.
Core/checks compile (`command-20261012-142645-17052.log`, 7.274 s;
`command-20261012-142645-22304.log`, 4.066 s). All ten full-core fixtures were
refreshed after the ResourceBindings layout change in compile-only mode; a later
attachment relink includes clear handling. Registry has 50 groups and zero
semantic closures. The immutable third preview excludes this subsequent work.
Depth/stencil access now also tracks actual SRV mip/layer/plane ranges and the
selected DSV mip/layers. A writable attachment can coexist with a disjoint
sampled range of the same resource; guards and barriers leave unrelated native
subresources intact. Replacing a descriptor removes its previous range while
COM lifetime retention stays independent. Twelve additional complete-content,
padding, sampled-value, state and overlap-rejection cases compile only.
Core/checks passed (`command-20261012-143912-14692.log`, 9.642 s;
`command-20261012-143912-41276.log`, 13.565 s). All ten full-core fixtures were
refreshed in compile-only mode: task `work/depth-range-final-<fixture>-compile.log`.
Registry group 51 remains partial/review pending; the current inventory has
1,447 associated functions, 13 explicit mappings and zero semantic closures.
No games/tests ran. Continue the complete migration; preview three is immutable.
Original root remains clean; preview-three executable still matches its hash.
Continue mixed-plane shader feedback and simultaneous writers; no tests or games
have run, and this later source is not included in preview three.

Independent raw-buffer/R32 image versions now preserve the original separate
post-write domains for exact, fully protected single-subresource allocations.
CPU reads reconcile the buffer and keep the image; actual CPU write faults and
exact-base GPU buffer writes invalidate the image version. PageWatch forwards
the read/write fault kind. The real GCN `separate-buffer-image-owned` fixture
prepares three warm full-allocation/padding oracles using physical guest aliases,
CPU read/write faults and later image loads. Compile/link only. Core/checks
passed (`command-20261012-151334-55980.log`, 13.753 s;
`command-20261012-151334-56208.log`, 19.876 s). All ten full-core fixtures were
refreshed in `work/independent-r32-final-<fixture>-compile.log`.
Registry group 52 is partial/review pending. Formatted buffer reads, pressure
eviction/incompatible views, legacy linear readback and other alias geometries
remain open or explicitly rejected. No tests or GPU execution.
Latest immutable user snapshot is preview four, `dx12-20261012-full-port-preview-c5683c4914ea`,
SHA256 `c5683c4914eaf1ec9a3c2848a7337fd4b678b0a54341a0b7a4313e2e13632157`.
It includes groups 47–52 and is installed in the existing desktop catalogue with
20 independently copied save files. Seventy-six file hashes and static PE imports
passed; no runtime execution. Task `outputs/DX12-preview-4-20261012` contains the
instructions/provenance. Desktop/settings/current selection are unchanged.
Continue formatted synchronization and domain-preserving pressure retirement;
preview four remains immutable.
Subsequent production now handles exact R32 formatted GPU arena synchronization
versus original <=16-KiB clean CPU streaming, including actual GPU retile/padding,
original sharp size/base and fence/accounted device allocations. Active tiled
independent versions are excluded from GC victims. Group 53 compiled and ten
full-core fixtures refreshed (`work/formatted-pressure-final-<fixture>-compile.log`).
Audit `command-20261012-154214-102576.log`, 21.896 s: 1,457 associated functions,
13 explicit mappings, zero semantic closures. Linear pressure download, broader
formats/geometries and simultaneous formatted writers remain open/guarded.
HTILE/CMASK formatted metadata now uses exact native arena-only UAV fills with
original sample patterns and CPU-stream condition. Group 54 prepares real GCN
HTILE registration, GPU writer/formatted loads, protected CPU read, CPU write and
full-allocation/padding/color/depth expectations. Core/checks compile in
`command-20261012-154953-58364.log` / `command-20261012-154953-105784.log`
(13.045/18.018 s); full-core attachment fixture links, not executed.
Preview four excludes this work. No tests/games ran; continue all implementation.
Registry audit 54 passed (`command-20261012-155504-65040.log`, 22.54 s), with
1,458 associated functions and zero semantic closures. Ten fresh production-linked
fixtures compiled in `work/formatted-metadata-final-<fixture>-compile.log`.
Actual `formatted-cmask-owned` GCN mode additionally prepares twelve complete
physical/padding/per-sample cases for 1/2/4/8 samples, actual metadata registration,
large synthetic arena versus small CPU stream. Compiled/linked, not executed.
R32 independent versions now also permit SINT. Twelve FLOAT/UINT/SINT GCN cases
cover four warm bit patterns including negative values, independent export alpha,
formatted synchronization and CPU/GPU domains. Core/checks compile in
`command-20261012-160225-68744.log` / `command-20261012-160225-11436.log`
(6.841/3.833 s). Preview four is immutable and excludes these changes.
Independent R32 versions now also retain valid full mip chains (one layer/sample)
and formatted GPU retile covers every mip while preserving physical padding.
Twenty-four GCN cases cover one/two mips, three numeric views and four warm values;
twelve additional cases diverge both image mips from the buffer before complete
retile and protected CPU visibility. Shader byte offsets and distinct hashes
preserve original sharp view selection. Core/checks compile in
`command-20261012-161142-43208.log` / `command-20261012-161142-64312.log`
(7.446/4.74 s); attachment fixture links in
`command-20261012-161521-87616.log` (0.441 s), compile-only. Group 55 is partial.
Arrays/cubes/volume/MSAA, broader aliases, partial/concurrent formatted writes,
linear pressure download and final contents/ordering/lifetime acceptance remain
open. No tests or games ran. Continue the same complete migration.
The 55-group inventory passed (`command-20261012-161913-111540.log`, 22.429 s),
with 1,458 associations, 13 explicit mappings, zero semantic closures; all ten
fixtures refreshed in `work/r32-mips-final-<fixture>-compile.log`.
Independent R32 physical array layers now retain the same full-range ownership
and formatted mip/layer retile. Forty-eight basic actual GCN cases and thirty-six
divergent first/selected mip/layer cases are prepared, with explicit DA/layer
coordinates, actual physical offsets and full 64-KiB padding oracles. Core/checks
compile (`command-20261012-162758-57872.log`, 6.819 s;
`command-20261012-162758-68220.log`, 3.823 s); attachment links
(`command-20261012-162822-27516.log`, 0.438 s), compile-only. Group 56 is partial.
Cube shader acceptance, volume/MSAA, broader aliases/writers, linear pressure
and final actual contents/protection/fence acceptance remain open. Preview four
is unchanged and excludes these following sources. Continue the complete port.
All ten fixtures refreshed in `work/r32-arrays-final-<fixture>-compile.log`.
The 56-group inventory passed (`command-20261012-162930-42984.log`, 21.844 s),
retaining 1,458 associations, 13 mappings and zero semantic closures. Following
fixture preparation adds actual six-face Cube sharp/integer DA coordinates,
different face-zero/face-five values and untouched-face/padding expectations:
sixty basic and forty-eight divergent mip/layer/face cases, all unexecuted.
Independent R32G32/R32G32B32A32 FLOAT/UINT/SINT now use actual 8/16-byte texels
in the formatted footprint/tiler transfer, retaining the same domain rules.
The GCN fixture prepares 180 basic/144 divergent cases, all-channel warm data,
distinct buffer-DWORD versus absent IMAGE_STORE channel expectations and
separate operation hash namespaces. Core/checks compile in
`command-20261012-163437-50260.log` / `command-20261012-163437-3052.log`
(7.502/4.797 s), zero non-deprecation diagnostics. The final rectangular RG32
geometry fixture relink and group-57 audit follow. No runtime acceptance.
All ten vector fixtures refreshed in `work/r32-vectors-final-<fixture>-compile.log`;
57-group audit passed (`command-20261012-163633-5092.log`, 21.792 s), 1,459
associations, 13 mappings, zero semantic closures. Following volume support
retains complete 3D footprints/tiler Z depth and actual XYZ (no DA) coordinates.
GCN cases total 252 basic/216 divergent after adding two-slice and four-to-two
mip volume shapes, every Z/channel warm data and full physical/padding oracles.
Core/checks compile (`command-20261012-163930-50596.log`, 6.902 s;
`command-20261012-163930-109056.log`, 3.846 s); volume fixture links
(`command-20261012-164006-111036.log`, 0.44 s), compile-only. Group 58 partial;
actual GPU/fault/fence acceptance, MSAA/formats/partial/pressure stay open.
All ten volume fixtures refreshed in `work/r32-volumes-final-<fixture>-compile.log`.
58-group audit passed (`command-20261012-164102-68524.log`, 21.859 s), 1,460
associations/13 mappings/zero closures. RGB32 now permits independent domains:
native sixteen-byte footprint, existing twelve-byte guest retile/alpha skip.
GCN totals 288 basic/240 divergent after RGB32 arrays/two-slice volume cases,
all RGB channels and full physical/padding expectations. Core/checks compile
(`command-20261012-164245-40860.log`, 6.866 s;
`command-20261012-164245-42988.log`, 3.805 s); RGB fixture links
(`command-20261012-164432-97228.log`, 0.439 s), compile-only. Group 59 partial.
All ten RGB fixtures refreshed in `work/rgb32-final-<fixture>-compile.log`;
59-group audit passed (`command-20261012-164517-45828.log`, 21.736 s), retaining
1,460 associations/13 mappings/zero closures. Independent RGBA8 UNORM/SNORM
adds native normalized encodings and matching formatted views: 344 basic/288
divergent GCN cases, negative SNORM, exact image/arena/CPU bytes and padding.
Core/checks compile (`command-20261012-164726-10864.log`, 6.853 s;
`command-20261012-164726-96812.log`, 3.801 s); attachment links
(`command-20261012-164812-61620.log`, 0.445 s), compile-only. Group 60 partial;
numeric/capability/GPU/fault/fence and full migration acceptance remain open.
All ten normalized fixtures refreshed in
`work/rgba8-normalized-final-<fixture>-compile.log`; 60-group audit passed
(`command-20261012-164959-57644.log`, 21.69 s), 1,460 associations/13 mappings/
zero closures. RG16/RGBA16 UNORM/SNORM/FLOAT versions now retain actual half/
normalized physical data and matching formatted sharp stride/record count.
512 basic/432 divergent GCN cases cover exact half/negative normalized channels,
full mip/layer/face/Z and physical/padding visibility. Core/checks compile
(`command-20261012-165209-62792.log`, 6.845 s;
`command-20261012-165209-102156.log`, 3.82 s); fixture links
(`command-20261012-165250-105424.log`, 0.442 s), compile-only. Group 61 partial.
All ten half fixtures refreshed in `work/half-versions-final-<fixture>-compile.log`;
61-group audit passed (`command-20261012-165548-58280.log`, 21.707 s), 1,460
associations/13 mappings/zero closures. Larger readonly formatted sharp extents
at the exact image base now choose unique/exact-original-size candidates,
capture/retile/copy only complete image bytes and preserve the prepared buffer
tail. Every 512 basic GCN case adds large-clean/GPU-owned extended sharp
observations, at least twenty KiB and four KiB beyond image, full physical tail
oracles and protected CPU visibility. Core/checks compile
(`command-20261012-165905-80156.log`, 7.045 s;
`command-20261012-165905-107136.log`, 5.372 s); fixture links
(`command-20261012-170009-59700.log`, 0.444 s), compile-only. Group 62 partial;
short/non-base/ambiguous/concurrent/pressure and final runtime acceptance open.
All ten extended fixtures refreshed in
`work/formatted-extended-final-<fixture>-compile.log`; 62-group inventory passed
(`command-20261012-170150-66528.log`, 21.749 s), 1,460 associations/13 mappings/
zero closures. Short formatted views now use deduplicated readonly full-image
residency helpers appended after ordinary/DMA/auxiliary requests, preserving
original shader bounds and access class. Each 512 basic GCN case adds clean/
GPU-modified short-sharp observations; full physical/padding oracle includes
bytes beyond view. Core/checks compile
(`command-20261012-170611-76348.log`, 6.801 s;
`command-20261012-170611-61248.log`, 5.3 s); fixture links
(`command-20261012-170749-62816.log`, 0.452 s), compile-only. Group 63 partial;
real bounds/residency/streaming/dirty/GPU/CPU/fault/fence/auxiliary acceptance open.
All ten short fixtures refreshed in `work/formatted-short-final-<fixture>-compile.log`;
63-group audit passed (`command-20261012-170950-55584.log`, 21.802 s), 1,460
associations/13 mappings/zero closures. RGBA8/RG16/RGBA16 integer versions now
add UINT/SINT retention, actual literal stores and real CVT/MUL-scaled GCN reads.
680 basic/576 divergent cases include negative integer warm data and full/short/
long views. Metadata hashes now include numeric view; R-only image-load color
exports explicitly zero G/B, correcting earlier multi-channel warm expectations
while full physical checks still cover every channel. Core/checks compile
(`command-20261012-171538-49380.log`, 6.86 s;
`command-20261012-171538-35628.log`, 3.844 s); fixture links
(`command-20261012-172136-48624.log`, 0.45 s), compile-only. Group 64 partial.
All ten integer fixtures refreshed in `work/small-integer-final-<fixture>-compile.log`;
64-group audit passed (`command-20261012-172408-46264.log`, 22.506 s), 1,463
associations/13 mappings/zero closures. R8/RG8 normalized/integer and R16
normalized/integer/FLOAT now retain independent one/two-byte image versions.
Linear footprint buffers/accounting round to DWORDs; existing CAS retile and
actual-byte warm/first-subresource oracles preserve neighboring texels/padding.
GCN totals 1044 basic/888 divergent with full/short/long observations. Core/checks
compile (`command-20261012-172741-15388.log`, 7.602 s;
`command-20261012-172741-39372.log`, 4.893 s); attachment links
(`command-20261012-173035-82012.log`, 0.438 s), compile-only. Group 65 partial;
actual bounds/CAS/neighbor/capability/GPU/CPU/fault/fence acceptance stays open.

All ten sub-DWORD fixtures refreshed in `work/subdword-image-final-<fixture>-compile.log`.
The 65-group inventory passed (`command-20261012-173155-30232.log`, 21.942 s),
1,463 associations/13 mappings/zero closures. RGBA8 SRGB independent versions
now retain UNORM storage bytes with separately sampled SRGB decoding; formatted
buffers use their actual three-bit UNORM numeric field. GCN totals are 1072
basic/912 divergent. The independent oracle expects encoded quarter/half bytes
64/128 and sampled red 13/55, complete physical bytes/padding and piecewise
warm decoding. Core/checks compile (`command-20261012-173935-102820.log`, 7.175 s;
`command-20261012-173935-20456.log`, 3.988 s); attachment links
(`command-20261012-174141-99240.log`, 0.442 s), compile-only. Group 66 is partial;
runtime and cross-stage storage-read classification remain open. Preview four
is immutable and excludes all production work after group 52.
All ten SRGB fixtures refreshed in `work/srgb-image-final-<fixture>-compile.log`.
The 66-group audit passed (`command-20261012-174654-24764.log`, 21.758 s),
1,467 associations/13 mappings/zero closures. No runtime acceptance is implied.
Packed 2:10:10:10 UNORM/SNORM/UINT/SINT now retain independent versions. Expanded
signed sampled images keep exact raw backing/masks; formatted synchronization
merges written bytes into the buffer snapshot, preserving padding and SNORM
negative-minimum bits without requantization. Dispatch is bounded to 256 X groups
and <=1024 rows for the existing 64-MiB scope. GCN totals 1191 basic/1014 divergent
include RGB -512/alpha -2 warm encodings. Core/checks compile
(`command-20261012-175343-72452.log`, 13.99 s;
`command-20261012-175343-21884.log`, 17.01 s); attachment links
(`command-20261012-175447-30884.log`, 0.455 s). Exact SM5.1 host shader compile
passes (`command-20261012-175407-67284.log`, 2.641 s), generated source includes
the merge. No execution. Group 67 is partial; actual GPU/fault/fence/casts and
other packed/MSAA/partial/pressure/complete acceptance remain open.
All ten packed fixtures refreshed in `work/packed10-image-final-<fixture>-compile.log`.
The 67-group audit passed (`command-20261012-175557-41584.log`, 21.718 s),
1,467 associations/13 mappings/zero semantic closures.
Packed16 UNORM versions now support 2D/mip/array/cube storage/sample conversion;
volume stays guarded. The fixture uses supported R16_UINT formatted reads and
real GCN shift/mask/CVT/MUL, preserving the original unsupported packed16-buffer
format contract. Quantized quarter red is 66/68, half 132/136, with exact
two-byte physical component/remapping and neighbor/padding expectations.
Core/checks compile (`command-20261012-180122-111508.log`, 7.109 s;
`command-20261012-180122-107672.log`, 3.945 s); fixture links
(`command-20261012-180136-104324.log`, 0.456 s), compile-only. Group 68 partial.
Canonical R11G11B10 float retention also accepts both original descriptor orders;
typed load/store capability checks and original remapping remain mandatory.
Independent unsigned-float exponent warm/store oracles bring actual GCN totals
to 1327 basic/1126 divergent. Core/checks compile
(`command-20261012-180328-56916.log`, 7.007 s;
`command-20261012-180328-112488.log`, 3.896 s); fixture links
(`command-20261012-180341-107100.log`, 0.449 s), no execution. Group 69 partial;
actual capability/quantization/casts/retile/shader/GPU/CPU/fault/fence, R9/MSAA/
volume/partial/pressure and full acceptance remain open.
All ten current fixtures refreshed in `work/packed-formats-final-<fixture>-compile.log`.
The 69-group audit passed (`command-20261012-180530-40752.log`, 21.89 s):
982 files, 13,358 functions, 12,186 dispatch entries, 1,470 associations, 13
explicit mappings, zero semantic closures. Remaining review counts are 765
mapping/adaptation functions, 210 changed functions and 567 dispatch entries;
unchanged source is still semantically unverified. No tests/games ran.
User-requested current core snapshot five is installed as
`dx12-20261012-full-port-preview-8849f997245d`, named
`DX12 — перенос №5 — 12.10.2026`, SHA256
`8849f997245d940de270fafe4a81265bd07c59207671464bbae99a1fea6590c2`.
It includes groups 53–69 and all earlier source through core154. Packaging passed
(`command-20261012-180651-36908.log`, 36.509 s): 76 manifested files, static PE
imports/dependency checks, matching PDB/MAP and exact dirty/untracked source patch.
Twenty save files were copied from still-selected preview one, hashes verified.
Installed desktop/settings/default selection and older packages are unchanged.
User deliverables: `outputs/DX12-preview-5-20261012`. No runtime was launched;
snapshot five is immutable and full migration continues.
After preview five, original-address GPU buffer/copy notifications now invalidate
independent image identities even when protected native buffer preparation is
skipped. Native aligned/page envelopes cannot choose the invalidation base.
Exact-base simultaneous writers allow short/long extents with complete prewrite
snapshots; DWORD-shifted buffer-only writers retain preceding image contents.
Stale image read-only watches retire with logical identities, other active image
watches rearm, and retained-image buffer writes require actual deferred ownership
before guest execution. All prepared GCN cases repeat full/short/extended writer
sharps: 3981 basic/3378 divergent, each with an additional shifted-write/CPU read
and image-preservation observation before exact-base invalidation. Core/checks
compile (`command-20261012-181459-99576.log`, 12.806 s;
`command-20261012-181459-49036.log`, 19.485 s); attachment links
(`command-20261012-181520-52856.log`, 0.454 s), zero non-deprecation diagnostics
and whitespace passes. No execution. Group 70 partial; sub-DWORD/non-base
concurrent/formatted writers, broader ownership/DMA/watch/pressure and complete
acceptance remain open. This source is absent from immutable preview five.
All ten logical partial-write fixtures refreshed in
`work/logical-partial-final-<fixture>-compile.log`, compile-only. The 70-group
audit passed (`command-20261012-181739-86832.log`, 21.549 s), 1,470 associations,
13 mappings and zero semantic closures.
Mixed buffer ownership now uses PageWatch::TrackOwned. Exact fast paths remain;
crossing image RO watches split while outside protection/generations remain.
Writable gaps and old watched OS modes validate before preallocated replacement
nodes are published. All owner pages become NOACCESS without releasing existing
protection; failure rolls back preceding watched modes. Prepared page-watch
cases cover interior/outside boundaries, RO conversion, holes/reacquisition,
original rights restoration and unchanged rejection after lost/readonly rights.
Core/checks compile (`command-20261012-182151-62812.log`, 7.697 s;
`command-20261012-182151-79908.log`, 4.345 s), including page-watch executable;
attachment links (`command-20261012-182205-78304.log`, 0.451 s), no non-deprecation
diagnostics/whitespace errors. No execution. Group 71 partial; actual Windows
rollback/races/owner lifetimes/remaps/GPU/CPU/fault/fence/pressure/full acceptance
remain open. Immutable preview five excludes this later source.
All ten mixed-page fixtures refreshed in `work/mixed-page-final-<fixture>-compile.log`,
compile-only. The 71-group audit passed (`command-20261012-182449-88864.log`,
21.674 s), 1,470 associations/13 mappings/zero semantic closures.
Original sub-DWORD shader write ranges now remain distinct from aligned native
resource/descriptor envelopes. ShaderBufferRequest::WriteRange validates bounds/
overflow and drives logical invalidation plus preparation/bind-time dirty marking.
Byte-aligned buffer-only writes can retain independent images; exact-base
concurrent image legality uses the original base. Pinned DownloadMemory intersects
page-tracker ranges with exact gpu_modified_ranges, so physical commits must not
widen to pages/envelopes. All 3981 basic/3378 divergent GCN cases now add actual
STORE_BYTE at +1/size1 and STORE_SHORT at +2/size2 after formatted synchronization
makes GPU quarter bytes differ from clean physical CPU zeros. Exact CPU 0x35/
0x7a35, unchanged unused DWORD bytes, sampled quarter and full padding expectations
are prepared. Core/checks compile (`command-20261012-182936-44456.log`, 12.542 s;
`command-20261012-182936-30488.log`, 19.239 s); final attachment links
(`command-20261012-183301-104996.log`, 0.449 s), no non-deprecation diagnostics/
whitespace errors. No execution. Group 72 partial; actual lowering/dirty/fault/
fence and broader CPU-upload granularity, ownership/pressure/full acceptance stay
open. Immutable preview five excludes this source.

All ten exact-subDWORD fixtures refreshed in `work/exact-subdword-final-<fixture>-compile.log`,
compile-only. The 72-group inventory passed (`command-20261012-183432-21896.log`,
21.824 s), 1,471 associations/13 mappings/zero semantic closures.

Actual CPU dirty pages now have separate upload markers when readonly image/
metadata synthesis has changed a resident GPU arena independently of CPU bytes.
Markers survive image-watch retirement and loss of snapshot proofs. Preparation
uploads touched 4-KiB CPU pages even for same-value stores; untouched GPU-only
pages remain resident and exact GPU physical dirty spans are unchanged. Divergence
is conservatively retained for each mapping's lifetime. Byte-component GCN cases
prepare a volatile zero store over an unchanged physical zero after formatted
quarter synthesis. A large actual raw GCN DWORD load, byte mask and UINT conversion
first witness quarter, then require zero immediately after the store before any
image refresh. The image is subsequently refreshed before formatted zero checks.
Core/checks compile (`command-20261012-184626-111628.log`, 12.922 s;
`command-20261012-184626-97696.log`, 19.41 s), zero non-deprecation diagnostics.
Group 73 partial; final fixture compilation pending, no execution.
Generic GuestBufferAccess read faults no longer create CPU-store markers. Its
prepared production fixture synthesizes a 16-KiB GPU arena, reads/reconciles only
four GPU-written bytes without upload, then stores the unchanged byte in page
two. Full GPU readback requires exactly that CPU page and three untouched GPU
pages; diagnostic forced upload count is exactly 4096. Latest compile pending.
Upload generation changes cause another pass over actual-store markers before
publishing the prepared view. A physical-alias callback prepares a same-value
third-page notification during second-page upload; the readback requires both
CPU pages while first/last GPU-only pages persist. Current production/checks
compile (`command-20261012-185347-79404.log`, 12.688 s;
`command-20261012-185347-84384.log`, 19.5 s). Final all-ten refresh/audit pending.
All ten final full-core fixtures compiled/linked in
`work/actual-cpu-store-final-160-<fixture>-compile.log`; attachment link
`command-20261012-185539-72572.log`, 0.452 s. Current builds have zero non-
deprecation diagnostics; whitespace validation passed. The 73-group inventory
passed (`command-20261012-185451-44760.log`, 21.904 s), 1,473 associations,
13 mappings/zero semantic closures. No execution; preview five excludes group 73.
Removed the legacy 0x8b355b5a all-zero CPU-fill special case. Pinned
IsComputeImageClear clears a compatible cached image before ordinary buffer
binding; absent/incompatible image candidates fall back to actual shader
dispatch. Zero values now use the same native candidate/format/ownership checks
and GPU image clear as nonzero values, preserving independent buffer domains
and image-clear padding semantics. Group 74 partial, compile pending.
Zero clears can additionally target canonical writable UAV images with a whole
single-sample allocation and all mips/layers selected. Both clear descriptor
heaps and the image resource are retained; typed integer/raw versus float clears
match native storage. Exact positive-zero bit patterns are required (negative
float zero is not silently rewritten). Twelve prepared real GCN clear-shape
cases use the old hash and a preceding separate R32 FLOAT image/buffer version,
require whole sampled image zero while physical warm/padding bytes persist,
then recreate the ordinary quarter scenario. Latest compile pending.
Current core/checks compile (`command-20261012-190806-68764.log`, 7.23 s;
`command-20261012-190806-82856.log`, 4.117 s), no non-deprecation diagnostics.
All ten full-core fixtures refreshed in `work/zero-clear-storage-final-<fixture>-compile.log`,
compile/link only. Whitespace validation passed; current inventory pending.
The 74-group inventory passed (`command-20261012-191046-84596.log`, 20.269 s),
1,474 associations/13 mappings/zero semantic closures. Current attachment link
`command-20261012-190948-113212.log`, 0.447 s. No execution. Preparing a new
immutable sixth snapshot from core162/groups 70–74 plus all prior changes.
Sixth immutable package installed as `dx12-20261012-full-port-preview-0e76d14b1685`,
`DX12 — перенос №6 — 12.10.2026`, SHA256
`0e76d14b1685ad3afd1af17ab7b8e65064f167035be85ffb80a2e8b2277c414e`.
Packaging/static manifest/import checks passed (`command-20261012-191301-38156.log`,
36.232 s), 76 files with dirty/untracked source patch and matching PDB/MAP/pinned
runtime/licenses. Twenty save files copied from still-selected `95d72e06d1ac`
into the isolated sixth profile; all hashes verified. Existing desktop, selection,
settings and older packages stay unchanged. Outputs README/launcher/provenance
are in `outputs/DX12-preview-6-20261012`. No runtime launch. Full port continues.
After preview six, image-clear compatibility no longer requires padding-free
guest storage. Temporary clear views select all resource mips/layers regardless
of the view which populated the cache, matching pinned IsComputeImageClear.
Zero UAV clears additionally keep complete native 1D/3D extents. Prepared padded
RGBA8 clear uses width17/pitch32/height9, two mips/three layers and a cached
mip1/layer1 view; full readback requires yellow texels and unchanged 0xa6 padding.
R32 FLOAT GCN zero-clear shapes expand to 84 warm/extent/mip/layer/cube/volume
cases with separate physical buffer oracles. Group 75 partial; compile pending.
Immutable sixth preview excludes this later implementation.
Core compiles (`command-20261012-191708-29380.log`, 7.3 s). Checks initially
exposed the existing fixture Require(bool,source_location) signature, corrected
without production changes; final checks compile
(`command-20261012-191734-43504.log`, 3.115 s). Attachment fixture links
(`command-20261012-191744-45036.log`, 0.456 s), compile-only; final all-ten/audit pending.
All ten complete-image-clear fixtures refreshed compile/link only. The 75-group
inventory passed (`command-20261012-191833-27700.log`, 21.68 s), 1,475 associations/
13 mappings/zero semantic closures; no non-deprecation diagnostics or whitespace
errors. Runtime remains unverified.
Wide R32/RG32/RGBA32 writable image clears now accept nonzero vectors through
the same complete resource views. Integer/raw views use exact input union bits;
typed float views use FLOAT clear. Narrow/packed storage keeps its positive-zero
restriction. All 84 R32 FLOAT prepared GCN clear shapes additionally clear to
half, require first/next sampled red128, retain physical buffer zero/warm bytes,
then restore quarter. Group 76 partial; compile pending; preview six excludes it.
Large actual raw DWORD-to-float GCN reads additionally require the resident GPU
buffer's zero after both zero/half image clears, before any formatted sync.
Core/checks compile (`command-20261012-192225-73596.log`, 7.287 s;
`command-20261012-192225-8012.log`, 4.141 s). Final fixture/all-ten/audit pending.
Prepared API fixture covers 27 R32/RG32/RGBA32 × UINT/SINT/FLOAT × array/1D/volume
cases, three mips/restricted cached views, exact finite/negative-zero union bits
and full 0xa6 padding. Nine warm R32 clears additionally require 0x80000000 on
all texels. Final checks/refresh/audit pending; no execution.
Final checks compile (`command-20261012-192650-71056.log`, 3.186 s), attachment
links (`command-20261012-192405-102084.log`, 0.476 s), all ten refreshed compile/
link only. The 76-group inventory passed (`command-20261012-192728-73288.log`,
22.386 s), 1,475 associations/13 mappings/zero semantic closures. Whitespace and
non-deprecation diagnostics clean. No execution.
Typed writable narrow images now accept nonzero native numeric clears; integer
views keep input union bits, normalized/half-float views use FLOAT clear. Raw
RGBA8/packed16/signed packed10 and sRGB nonzero paths remain guarded. Prepared
81 R8/RG8/RGBA8/R16/RG16/RGBA16 numeric/array/1D/volume cases cover exact norm,
integer/sign and half-float/negative-zero texels, three mips/full views and 0xa6
padding. Group 77 partial; compile pending; preview six excludes this source.
Core/checks compile (`command-20261012-193324-68732.log`, 7.079 s;
`command-20261012-193324-22944.log`, 4.034 s); no execution.
Raw RGBA8 nonzero clears now use temporary typed native numeric clear followed
by GPU retile/detile into the existing R32 image. Its descriptor/resource identity,
CPU snapshot and independent buffer version remain unchanged. Actual dimensional
UAV store support and texture/scratch memory budget are checked before commands;
all heaps/textures/transfers are retained before recording and retired by fence.
Twelve extra API cases prepare exact full RGBA8 texels/padding/identity/accounting;
336 extra actual GCN HLE shapes prepare separate image/resident/physical buffer
observations (420 total with R32 FLOAT). Group 78 partial; no execution.
Core/checks compile (`command-20261012-194730-10376.log`, 7.500 s;
`command-20261012-194808-92876.log`, 13.893 s). Final refresh/audit pending.
All ten full-core fixtures refreshed in compile/link-only mode. The 78-group
inventory passed (`command-20261012-194919-32020.log`, 21.846 s): 1,475 function
associations, 13 explicit mappings, zero semantic closures. Whitespace and
non-deprecation diagnostics clean. No test, game or application execution.
Snapshot seven includes production through group 78/core166. Installed as
`dx12-20261012-full-port-preview-92043d412a67`; SHA256
`92043d412a672e720e225bf3da9234e211d2d2365a584ea9f3eedb6cb2185049`.
Packaging/static hashes/imports passed (`command-20261012-195040-12232.log`,
34.882 s): 76 files, 20 save files copied into a separate profile from selected
preview `95d72e06d1ac`. Old cores/settings/installed desktop remain unchanged.
Task outputs: `outputs/DX12-preview-7-20261012`. Full migration continues;
no tests/games/apps executed, no commits or pushes.
Post-snapshot-seven: packed16 UNORM sampled volumes now expand to RGBA32 FLOAT
on GPU. Physical two-byte texels and written masks remain exact retained backing;
inverse writable/raw conversion and formatted resident-buffer synthesis reuse it
without quantization. GPU decode covers native packed channel ordering, all mip
depths and the existing layout. Added eight direct API round-trip cases with
poisoned CPU sources/exact padding and 96 actual GCN volume combinations in each
basic/divergent category (4,077/3,474 total). Group 79 partial; no execution.
Core/checks compile (`command-20261012-200005-36300.log`, 8.103 s;
`command-20261012-200005-33112.log`, 15.014 s). Strict SM5.1 FXC compilation passed
(`command-20261012-200051-66460.log`, 2.699 s). Final fixtures/audit pending.
All ten full-core fixtures refresh compile/link only. The 79-group inventory
passed (`command-20261012-200152-104300.log`, 21.123 s), 1,476 associations,
13 mappings and zero semantic closures. Whitespace/build diagnostics clean.
Packed16 nonzero image clears now use temporary GPU RGBA32 clear, then GPU
UNORM clamp/nearest-even packing and native/guest field conversion into the same
R32 image. Existing buffer version, resource identity/descriptor and padding stay
independent. Twelve API array/1D/volume exact packed-color/accounting cases and
336 actual GCN HLE combinations are prepared (756 clear shapes total), using a
16-bit destination buffer sharp so the pinned physical-bits check is exercised.
Core/checks compile (`command-20261012-200418-79748.log`, 11.175 s;
`command-20261012-200418-13836.log`, 5.535 s). Strict SM5.1 FXC /Ges /WX passed
(`command-20261012-200506-29112.log`, 3.069 s); saved assembly has `round_ne`.
Group 80 partial; final refresh/audit pending, no execution. Preview seven
predates groups 79/80 and remains immutable.
All ten full-core fixtures refreshed compile/link only. The 80-group inventory
passed (`command-20261012-200557-103228.log`, 21.146 s), 1,476 associations,
13 mappings, zero semantic closures. Build/fixture diagnostics and whitespace
passed. No execution.
Raw packed10 nonzero clears use native GPU RGBA32 FLOAT/UINT/SINT clear followed
by packed image writer clamp/nearest-even GPU packing into the same R32 image.
Ordinary expanded sampled inverse conversions still reuse exact inherited bits.
Twelve API numeric/array/1D/volume cases, twelve warm normalized/integer/alpha
endpoint cases and 420 actual GCN clear combinations are prepared (1,176 HLE
clear combinations total), preserving raw resident/physical buffer observations.
Core/checks compile (`command-20261012-200830-92084.log`, 10.733 s;
`command-20261012-200830-25020.log`, 5.285 s). Strict SM5.1 FXC passed
(`command-20261012-200916-110500.log`, 3.654 s). Group 81 partial; final endpoint
fixture/checks/refresh/audit pending; no execution. Preview seven predates it.
Final checks compile (`command-20261012-201006-62920.log`, 3.112 s), all ten
fixtures refreshed compile/link only. The 81-group inventory passed
(`command-20261012-201019-103304.log`, 21.034 s), 1,476 associations/13 mappings/
zero semantic closures. Final diagnostics/whitespace verification pending.
SRGB nonzero clears now use temporary native SRGB RTV conversion then GPU byte
transfer into the existing typed/raw image. Direct SRGB attachment clears are
enabled. Complete RTV descriptors are prepared/retained before commands; 1D,
arrays and volumes use corresponding native RTV ranges. Temporary SRGB clear
does not change ordinary guest attachment geometry restrictions. Six API
typed/raw/dimension cases plus six warm curve-boundary/linear-alpha cases and
84 actual GCN HLE SRGB shapes prepare numeric/padding/identity/accounting and
independent raw buffer observations (1,260 HLE clear shapes total).
Initial core/checks compile (`command-20261012-201329-97060.log`, 7.387 s;
`command-20261012-201329-28592.log`, 4.897 s). Group 82 partial; final build/
fixtures/audit pending, no execution. Preview seven predates groups 79–82.
Final core/checks compile (`command-20261012-201443-106308.log`, 4.564 s;
`command-20261012-201443-66256.log`, 3.355 s), all ten fixtures refresh compile/
link only. The 82-group inventory passed (`command-20261012-201640-48540.log`,
21.278 s), 1,476 associations/13 mappings/zero semantic closures. Final core/
checks/fixture non-deprecation diagnostics and whitespace clean; no execution.
Read-only typed cached images without UAV now clear through a temporary native
typed image then GPU transfer into the existing resource/descriptor. HLE clear
obtains independent image ownership before buffer binding; an image equality
witness ends without dropping CPU buffer proofs. Existing no-access image watch
converts directly to read-only with ChangeMode, preserving protection against
stores. Copy HLE ownership stays unchanged; compatible direct RTV clears remain
direct. The existing retention/legacy readback/geometry envelope still applies.
84 new read-only API cases plus three SRGB warm endpoints are prepared (234 API
cases total), and 504 actual cold read-only GCN HLE cases preserve original CPU/
raw resident buffer values after image clear. Post-writer HLE count stays 1,260.
Corrected first zero-clear color oracle: compute retains preceding quarter color.
Core/checks compile (`command-20261012-202134-103036.log`, 7.488 s;
`command-20261012-202134-51156.log`, 4.388 s); final ownership core compile
(`command-20261012-202340-76568.log`, 6.791 s). Final fixture/checks compile
(`command-20261012-202536-26676.log`, 1.446 s;
`command-20261012-202536-52676.log`, 3.898 s). Group 83 partial; final all-ten/
audit/diagnostics pending, no execution. Preview seven predates groups 79–83.
All ten full-core fixtures refreshed compile/link only; attachment link
`command-20261012-202759-96980.log`, 0.447 s. The 83-group inventory passed
(`command-20261012-202723-49364.log`, 20.986 s), 1,476 associations/13 mappings/
zero semantic closures. Core/checks/fixture diagnostics and whitespace clean.
No test/game/application execution, no commits or pushes. Full migration continues.
Snapshot eight includes production through group 83/core175. Installed as
`dx12-20261012-full-port-preview-ae5276ef6874`; SHA256
`ae5276ef687461d96ee2a99e70654e5a06a8648406c95bddcc0b5b2a3b6aac47`.
Packaging/static hashes/imports passed (`command-20261012-202956-64540.log`,
34.258 s): 76 files, 20 save files copied into a separate profile from selected
preview `95d72e06d1ac`. Old cores/settings/installed desktop remain unchanged.
Task outputs: `outputs/DX12-preview-8-20261012`. Main port continues; runtime
tests/games/apps unexecuted, no commits/pushes.

## Latest — covered envelopes and selectable cores, 2026-10-12

`ba3892f` retains an already covering, successfully initialized mapping before
optional 64-KiB arena rounding in `PrepareShaderBuffers`. Validation, batch-wide
SRV/UAV separation, ownership and failed-upload retry remain mandatory. The new
content regression fails before and passes after; the native suite passes 221/221
(61.58 s). Incremental Release core and desktop publish pass. Version catalogue,
runtime hashes, persistent UI choice, missing/corrupt-package rejection and rollback
are implemented and installed. Prior default core is retained.

The current request explicitly restored isolated XInput/focus/gameplay authorization,
superseding the old restriction below. Three game runs used one immutable seed:
A/A2 on exact 0c340b4 both fail during world loading with SRV/UAV alias exception;
B on ba3892f-dirty loads Hunter's Dream, completes 124 new GPU frames / 90.015605 s
(1.377539 FPS), and closes normally (0). Movement, camera, menu, attack and short
B backstep are observed; all owned devices removed. No matched baseline or speedup
is established. Remaining envelope churn is substantial; see BENCHMARKS/NEXT_SESSION.
Evidence: F:/PS4Dev/handoff/covered-envelopes-20261012.

## Latest — Vulkan model adapted to native core, 2026-10-12

Read [CORE_CONTRACT.md](vulkan-emulation-20261011/CORE_CONTRACT.md) before core
changes. It records guest execution, CPU/GPU ownership, aliases, shader variants,
queue domains and VideoOut; root AGENTS.md persists this work order and automatic
push. [PINNED_PROBE.json](vulkan-emulation-20261011/PINNED_PROBE.json) adds a
separate c7e065d1 compatible-source Vulkan probe with actual transfer-site bytes,
PM4 counts, scheduler/tick joins and existing-fence completion observations.
Three stationary windows produced 29.99 completed new flips/s at a 30 FPS cap.
Trace raised mean CPU 2.5615 to 3.1924 cores (~24.6%); unchanged capped FPS does
not imply zero profiler overhead. Shader identities and three missing source
hooks remain explicit gaps. Two probe games closed with exit 0; all owned devices
were removed. XInput confirmed movement, camera, chalice prompt and Workshop
entry; enemy combat and death/respawn remain unvalidated.

Production changes preserve clean CPU pages after stores, rearm only dirty gaps,
and protect before comparing without publishing an unfinished equality proof.
Ordinary 16/32-bit indices share the shader resource preparation batch and retain
their allocation through command retirement. UAV aliases use a separate pooled
GPU index snapshot; special index conversions retain the existing fallback.
220/220 native regressions passed in 57.38 s; the incremental core linked.
The final isolated launch loaded Hunter's Dream and completed 143 new guest
frames in 90.098142 s (1.587158 FPS), versus the earlier 0.863128 baseline.
See [BENCHMARKS.md](BENCHMARKS.md) for identities, failures and comparison limits.
Latest user restriction: tests limited to game launches; no Computer Use,
window activation or automated gameplay input while the user uses the computer.
The final capture was passive, with focus/external input uncontrolled, so the
observed +83.89% does not establish a matched stationary speedup. No fresh
movement/camera/attack acceptance is claimed for this final build. Original
17 home-file hashes match the installed-source manifest; owned pad was removed.
Earlier installed-runtime evidence and parked experiments below retain their
own revision/coverage limitations; they are not this probe or an accepted fix.

## Active — Vulkan emulation first, 2026-10-11

The user accepted the complete autonomous engineering cycle, then corrected its
order: understand the working Vulkan emulator before testing further DX12
changes. See [execution and ownership research](vulkan-emulation-20261011/RESEARCH.md)
and compact RESULTS.json. Existing XInput verified movement/camera, attack/roll
inputs and Central Yharnam → Hunter's Dream lamp travel on byte-identical copies
of the installed 974f8c21-dirty executable. Separate CPU/queue and detailed GPU
diagnostic runs exercised recording/cache/copy/detile paths. Actual EOP/VideoOut
tags linked 5,049 stationary flip submissions; 60.0029 submissions/s is **not**
GPU-completed guest FPS. Full PM4/resource/fence causal attribution remains open.

Three owned Vulkan runs ended with 0xC0000409 / 0xC0000005 / 0xC0000409 during
ordinary closing; causes unresolved. Collectors stopped and virtual devices were
removed. Original copied 17 home-file hashes still match. Enemy combat and
death/respawn are not validated. Producer's dirty patch is unavailable; compatible
pinned-source Vulkan instrumentation is authorized but must remain separate from
installed-runtime evidence. Earlier unverified DX12 and profiler edits are parked
under F:/PS4Dev/handoff/performance-recovery-20261011, outside the clean checkout.
No new DX12 fix or performance improvement is accepted here. Prior notes asking
for game-run permission are superseded by the user's explicit authorization.

## Installed Vulkan HUD and XInput validation — 2026-10-11

See [live companion report](vulkan-live-monitor-20261011/VULKAN_LIVE_MONITOR_REPORT.md)
and its session manifest. Existing Bloodborne shortcut launches the original
974f8c21-dirty DLSS core with a real external HUD/collector. Virtual Xbox input
entered Central Yharnam and verified walking, camera and menus after the user
switched the physical pad off; ordinary launches do not start the feeder.
Seven local recording segments, failures and exit-code disagreement are retained.
Final wait-signaled lifecycle check: collector/supervisor both code 0; virtual
device removed, all owned session processes finished. Two earlier game exits
0xC0000409 remain unexplained. Guest/display FPS, stacks and internal GPU/PM4/
cache costs are unavailable; exact dirty source is missing. Production renderer
unchanged. Resource-cost observations are not DX12 FPS/latency improvements.

## Vulkan reference profiler prepared — 2026-10-11

See [actual results and coverage](vulkan-reference-profiler-20261011/REPORT.md).
Production backend is unchanged. User's original Vulkan PID 18920 exited;
collector finalized 1254 samples / 1252.77s with process_exited, zero errors and
skipped intervals. Named threads and NVIDIA available; intermediate 426-sample
snapshot retained separately. Checking automation deleted after finalization.
Results: F:/PS4Dev/handoff/vulkan-reference-profiler-20261011.
The socket-named thread consumes ~1 CPU core; function/cause/critical path unknown.
Old PID 14740 segments and collector failures are explicitly separate.
Exact c7e065d1 instrumented Vulkan Release build compiled with 100 hook sites;
non-game sensor/trace tests pass. MODE B Bloodborne launch is **not authorized**.
Full PM4-to-frame causal attribution, graphics/copy GPU timestamps and matched
Relaxed/Precise/DX12 costs remain pending. No FPS inferred from GPU utilization.

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

## 2026-10-19 PM4 crash boundary

HEAD/remote `7335a30`; single installed core EXE SHA256 `2add101f20f300412b0f5933992650982bfc00fb7547a6d5ccc9922eaebf40a5`. Owned-copy lifetime is fixed/tested. The current Bloodborne `0x80000003` is not fixed: malformed PM4 is present before `SubmitGfx` in a first-submit DCB and was not produced by retained PM4 writes, renderer backing commits, or physical aliases. Next code change is producer-side GNM writer-range attribution before `PerformSubmit`; no more game reruns until that instrumentation exists. Exact retained evidence: `F:/PS4Dev/handoff/gameplay-physical-trace-7335a30`.