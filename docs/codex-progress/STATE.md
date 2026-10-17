# Native DX12 state

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
