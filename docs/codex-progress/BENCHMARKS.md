# Benchmarks

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



## Integration builds — 2026-10-17

Clean committed Release compiled/linked at72569a5, ff8534f and latest `6cbaef71857b04275be11013e62c05c2b3c672b3`. Latest EXE SHA256 `7751a7b565c94920c3ac15271dba0088308506a1a1717406aa8c1eb2aaf450c8`; artifacts/proof: `F:/PS4Dev/handoff/integrated-release-20261016/artifacts-6cbaef7` and integration evidence root. No tests/game/XInput/profiling/FPS measured during integration. Prior saved Vulkan/DX12 measurements remain the baseline; build duration is not performance evidence.


## Current BC block source block - no new measurements, 2026-10-16

Authoritative current section; all earlier current/next sections are historical.
Source-only gate: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 groups remain partial/open. Source review is not runtime PASS, parity or FPS evidence.

No BC shader/GPU contents, debug-layer PASS, game launch, image acceptance, FPS/frame time,
CPU/GPU wait or memory performance result was obtained. fdb1e27 source bridge and2 authored
GCN cases are uncompiled/unrun. Earlier28 ordered cases and MSAA/heap modes remain separate
unrun acceptance. Historical326/346(native211/229), three targeted passes and Vulkan/DX12
measurements keep their original binary/scenario identities. Installed preview24 is unchanged.
See SESSION_HANDOFF.md and bc-block-expansion-20261016 exact source/proof/recovery identities.



## Current depth/stencil block - no new measurement, 2026-10-16

Authoritative current section; earlier current/next sections are historical.
Source-only: no configure/build/package/compiler/test/GPU/game/XInput/UI execution.
All129 contract groups remain partial/open; no runtime PASS, new FPS or game acceptance.

0682d65/9b5b356 source fixes and authored depth-init/stencil controls have no execution result.
No device-debug PASS, game launch, world acceptance, FPS/frame time, CPU/GPU wait or memory
throughput measurement was obtained. Historical326/346(native211/229), three targeted passes
and Vulkan/DX12 measurements remain tied to their original binaries/scenarios. Preview24
SHA70eeeff04e89e604f7f5f6fa33115706bad76550dbbe54eb0cd86f1852c7e8bf stays unchanged.
See SESSION_HANDOFF.md and msaa-depth-init-20261016 for exact source/proof/recovery identities.



## Texture heap pressure source continuation - 2026-10-16

No configure/build/package/compiler/test/fixture/GPU/game/UI run or new measurement for
c6e5429/50860ef/e00c72d. The new focused heap mode and prior28 ordered GCN scenarios are
uncompiled/unrun. No FPS/frame-time/CPU-GPU wait/image/controls/stability improvement is
attributed to these source changes. Historical326/346(native211/229) plus three targeted
passes remain old separate data. Installed preview24 was unchanged. Evidence root:
F:/PS4Dev/handoff/pressure-retirement-20261016; source/Git/hash/index review is not PASS.


## Readonly-history source continuation - 2026-10-16

No measurements, build/tests/compiler execution or GPU/game/UI runs were performed for
36b109c/01f916c/d55fb2d or their authored fixtures. No FPS, frame time, wait, memory-throughput,
image/control or stability result is attributed to them. Source review and whitespace/
hash/archive checks are not PASS evidence. Historical326/346(native211/229) and three
targeted passes remain old separate results. Installed preview24 was not replaced.
Use saved Vulkan/DX12 baseline only when authorized; count completed guest frames,
excluding FG. Evidence: F:/PS4Dev/handoff/readonly-revalidation-20261016.


## Ordered writer/source continuation - 2026-10-16

No new measurements, build, tests or GPU/game runs.24 authored GCN scenarios are not
PASS results. Source reviews for exact ranges, binding-order state, disjoint domains,
grouped annotations and image preparation do not prove frame correctness/performance.
Historical326/346(native211/229) plus three targeted passes remain old separate evidence.
No FPS/frame-time/wait/image/control result is attributed to current commits. Packages
were not rebuilt. Use saved Vulkan/DX12 baseline only after authorization and count
completed guest frames excluding FG. Evidence: ordered-writers-20261015/EVIDENCE_INDEX.md.


## Current ordered streams, native SSBO aliases and readonly extents - 2026-10-15

No measurements were performed for d4b70f9,4210e31 or4ef80fc.16 ordered GCN scenarios
are authored only; no PASS may be inferred from source/whitespace/archive review.
No new FPS, frame-time, CPU/GPU wait, validation, image or controls result exists.
Historical326/346(native211/229) plus three later targeted passes are unchanged old
results, not acceptance of these commits. Installed preview24 is unchanged. Use existing
Vulkan/DX12 baselines only after authorization to execute; count completed guest frames
and exclude frame generation. Evidence index: F:/PS4Dev/handoff/ordered-stream-20261015/EVIDENCE_INDEX.md.


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

No new performance result exists. Source admission is not a measured speedup. Historical executable/scenario/FPS evidence remains unchanged.



## Current packed-float/RGB32 checkpoint - 2026-10-15

Scheduler batch6f55ea1 has no new build/test/GPU/performance measurements. Source review cannot establish FPS or device-failure acceptance; historical baselines retain original identities.

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

92af292 only changes authored fixture observation. No build, test, runtime, game or performance
measurement was performed. Fixture source review for the newly added output baseline is pending.

## Selected BC view continuation - no new measurements, 2026-10-16

923d637 contains a source-only readonly mip0 BC parent view change. No build, tests, GPU
execution, game run, FPS, frame-time, image, stability, or performance measurements were
obtained. Fixture ac3ca48 is authored but unreviewed/unrun. Earlier benchmark identities remain unchanged.


SESSION_HANDOFF.md's top section is authoritative; older next-task entries are historical.
06536f2 and6ded358 are pushed: exact packed-float2/4/8-sample transfers with actual cast lists,
readonly RGB9E5 physical ownership, and readonly RGB32 guest12/host16 transfer with separate
independent-image retention. Page/backing/fence/physical-domain guards remain. Source-reviewed
only, NOT compiled/installed/run. All129 groups remain partial/open. Preview24 stays installed
unchanged; no new FPS or game result. User execution gate remains active.
Evidence and final HEAD/remote/residual recovery: F:/PS4Dev/handoff/packed-float-msaa-20261015/CHECKPOINT_FINAL.json.
Next source task: CMASK6/7 selected +INF conversion in DecodeGuestColorClear; see
rgb32/NEXT_SOURCE_TASK.md. Finite RTZ is done; NaN encoding and format34 clear stay open.

No measurements were collected. New callbacks and transfer admission are not measured speedups.



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

No new performance data was collected. Additional raw capture work is not a measured speedup.


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

No new benchmark, wait time, memory-operation count or performance improvement is claimed.
Prepared fixtures and source review are not measurements. Historical results below remain unchanged.


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

Diagnostic-only core3c26133c ran155.76s before/requested stop, exit0; offline title observed, no gameplay input, world/FPS not accepted. EOP rejection not reproduced before stop. No new performance measurement.


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

| Run | Seconds | Exit | Paired completions |
|---|---:|---:|---:|
| diagnostic-game | 91.08 | 0 | 1805 |
| fixed-game | 87.65 | -2147483645 | 1446 |
| ownership-fixed-game | 79.54 | -2147483645 | 1200 |
| domain-fixed-game | 602.36 | 0 | 2456 |
| gbuffer-capture-game | 362.43 | 0 | 3667 |
| lighting-input-game | 362.61 | 0 | 1658 |
| producer-graph-game | 361.52 | 0 | 3109 |
| rop-graph-game | 143.12 | -1073741819 | 3358 |
| avplayer-fixed-game | 152.87 | -2147483645 | 3643 |

These are mixed incomplete-scene counts, not gameplay FPS. Exact SHA/session/config/provenance in RUNS.json.


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

## Emergency provenance checkpoint — 2026-10-13

See [SESSION_HANDOFF.md](SESSION_HANDOFF.md) for the current unfinished full-port
boundary. This checkpoint adds no measurements. Source core237/checks190 compile/
link only; typed-reader signed proof and replay/game/GPU acceptance remain pending.
Historical FPS and suites below do not validate the current executable.

`F:/PS4Dev/handoff/full-port-emergency-20261013/DIAGNOSTIC_PROVENANCE.json`
catalogues 25 existing diagnostic sets with absolute paths, session/run IDs,
source/executable identities, dates/scenarios/results/limits; missing metadata is
explicitly unknown. `evidence-files.json` indexes the existing local data, and
`important-evidence-index.json` lists crash/XInput/monitor/run metadata.
Current saved experimental core237 SHA256:
`a639d91da0561b213cbfa7075fbbf6959b1ba560c15c12e67e92cfcfbc4b6d4a`.
Latest installed preview18 SHA256:
`519d8356217d04c9b8c12251bbd1efc014ac4f86d8325e08d3eca7614a180f87`.
It includes through124; current source additionally125/126. No fresh baseline.
Preserve A/A2 failures, all old traces/dumps and limits; do not rerun for the report.

## Covered envelopes and version catalogue — 2026-10-12

Production `ba3892f0c93b5f6fe29fa0c363000b398bb234d6`; the core was built with
desktop-only dirty changes in the checkout, which is recorded in its manifest.
Core SHA256 `866e800e5f7f97fb7c3613697869883178091888ca82290d1aeec4567a26aa77`.
Baseline SHA256 `9438103c6eca8b5453971a55fc20159ce19091a75fc3784120d6cee50ad4c1c0`.
All three launches use the application's validated version launch path, isolated
copies of the immutable coherence seed, normalized config SHA256
`c897ac3e9cf8cc3212d4ff0b89bdc6e72de83b7e2ec5592c3d9933294dac7d55`, identical
game/patch/cache policy, 1280x800, partial render 0, 2048 MiB, no FG. B input is
neutral during 30-s explicit warmup and 90-s capture; no concurrent builds/tests.
A failed in loading; A2 is the one allowed failure retry and fails identically.
Both exit 0x80000003 with the pre-existing SRV/UAV overlap exception. There is no
valid matched before measurement; historical passive 1.587158 FPS is not its substitute.

| Run | Completed | Duration s | FPS | Median ms | P95 ms | Dropped |
|---|---:|---:|---:|---:|---:|---:|
| A/A2 (0c340b4) | N/A: loading failure | N/A | N/A | N/A | N/A | N/A |
| B (ba3892f-dirty), PID 107616 | 124 | 90.015605 | 1.377539 | 727.008 | 765.871 | 0 |

B materialized/upload/readback bytes per completed frame:
266,769,064 / 278,011,416 / 14,048,893 (decimal bytes, rounded).
Capture diagnostics: 48,740 envelope and 122 overlap evictions; 52,650 direct
submissions; finish reasons batch-limit 363 and CPU-visible dirty 162. Inclusive
submit/wait 11.924 s, overlap retirement 8.102 s, materialization 11.572 s,
image lookup 7.954 s. These nested scopes and counters have slightly different
window boundaries; do not sum them or call lower raw work totals a speedup.
The bounded churn sample still reports 7.115 GB at 0x2d1ce3000. One small
coverage fix does not solve GPU-owned/role-related envelope churn.

Deterministic regression: an exact writable 128-KiB envelope at base+4 KiB is
rebound through two disjoint read-only UAV views. Before, arena rounding replaces
it with a 192-KiB allocation/upload at the first transition; after, six read/write
cycles preserve its identity and exact view offsets. Full GPU readback equals
CPU contents after each cycle including CPU updates. Existing alias, dirty-owner,
in-flight index/descriptor, PRT/remap and failure/retry regressions remain green.
Native suite 221/221, 61.58 s; desktop catalogue tests and selected/rollback hardware
probes pass. No new Finish-to-Flush substitution was made: actual dirty writeback,
fault, image alias and guest ordering waits remain necessary in the existing model.

B confirms movement, camera, menu open/close, visible attack and short B backstep;
combat, hit accuracy, full roll, death/respawn and long-run stability are not claimed.
Existing rendering artifacts remain and native speed is unplayable. B exits 0;
all three feeders plus the short backstep feeder remove their owned device.
Both saved cores are visible in the installed application's «Версии ядра»;
selection survives app restart and rollback verifies the exact original package.
The old configured default remains. Packages with all runtime hashes are under
%LOCALAPPDATA%/PS4Desktop/cores/dx12-20261012-{0c340b4-baseline,ba3892f-covered}.
Only B is marked «Проверено в игре»; its notes explicitly deny a speedup claim.
Evidence and exact manifests: F:/PS4Dev/handoff/covered-envelopes-20261012.

## Vulkan mechanisms adapted to DX12 — 2026-10-12

Same immutable coherence seed, CUSA03173 1.09 game SHA256
`6764938b23539d29c936bca9880fc4a774e7b0099ce31c7e8c4b0f8bd0befb80`,
1280x800, partial render 0, 2048 MiB budget, no frame generation, 30 FPS guest cap.
Normalized profile SHA256 `c897ac3e9cf8cc3212d4ff0b89bdc6e72de83b7e2ec5592c3d9933294dac7d55`.
Baseline `dx12-before-1`: 90.368935 s, 78 new GPU-completed frames, 0.863128 FPS.
Intermediate protected-only build: 89.802757 s, 80 frames, 0.890841 FPS.
Final `dx12-protected-1`, PID 91108, executable SHA256
`9438103c6eca8b5453971a55fc20159ce19091a75fc3784120d6cee50ad4c1c0`:
UTC 2026-10-11 19:13:19.291489–19:14:49.306971, 90.098142 measured seconds,
143 new GPU-completed frames, **1.587158 FPS**, median 624.685 ms, P95 707.203 ms,
no dropped capture rows. Observed +83.89% over baseline is not a significance claim.

User restricted further tests to launches and forbade Computer Use during this
run. The world/camera was observed before that restriction; capture was passive,
virtual pad removed, no automated input, focus/external input not controlled.
The isolated cache was retained, native pipeline cache disabled; final capture
had no additional scripted warm-up. A strictly matched stationary comparison and
fresh movement/camera/attack acceptance are not established. Present/FG are not FPS.

Dirty-page proof and shared resident indices passed 220/220 regressions (57.38 s)
and incremental core linking. Earlier full-port attempts exposed an index SRV/UAV
alias rejection and a rearm scan regression (~200 ms per 94,306,304-byte mapping);
both were fixed before this final launch. PID 14184's 15-second **loading** diagnostic
is archived under `previous-14184` and is excluded from world FPS comparison.
Evidence is under `F:/PS4Dev/handoff/performance-recovery-20261011`.
Final diagnostic totals: snapshot compare 46.95 GB, upload 38.03 GB, readback 2.00 GB,
24,212 partial-proof comparisons, 1.72 GB index CPU copies avoided. Remaining
inclusive scopes: submit/wait 11.40 s, backing materialization 9.02 s, overlap
retirement 7.58 s, CPU detile 6.43 s. Do not add nested scopes or infer safe smaller
GPU-written ranges from these totals. Playable native performance is not achieved.

## Final high-memory experiment — 2026-10-11

Supersedes the intermediate and historical measurements below.
Initial source c31111c; high-memory implementation 7a550c9; final tested source
9527c901a50256016072c4b67464946f78061ab1, clean Release build. Same executable in A/B/C/D:
SHA256 db7e55e2e8f070bf3739a2e8aa50bb8b0f67cd2b393cc34a19cb25ccabcc44df.
RTX 5080, driver 32.0.16.1692, 64 GB RAM; Bloodborne CUSA03173 v1.09,
Hunter's Dream, stationary same saved scene/camera, internal 1280x800, partial render 0.
Every run uses a fresh copy of the same immutable coherence-20261011/seed, including
shader cache and patches (the assertion NOP was removed from that seed previously).
Thirty-second warm-up after observing the world, then a 90-second requested capture.
Validation/API tracing variables are absent in child processes; no builds/tests
overlap gameplay captures. Control input is neutral during capture, checked afterward.

| Mode | Budget MiB | Seconds | Completed frames | FPS | Median ms | P95 ms |
|---|---:|---:|---:|---:|---:|---:|
| A | 512 | 91.615 | 59 | 0.6440 | 1560.733 | 1684.461 |
| B | 2048 | 91.664 | 77 | 0.8400 | 1180.479 | 1209.101 |
| C | 4096 | 91.942 | 69 | 0.7505 | 1321.244 | 1373.113 |
| D | 8192 | 91.605 | 74 | 0.8078 | 1218.814 | 1390.598 |

Best observed sample: mode B, +30.44% FPS relative to A and
97.21% fewer materialized bytes per completed frame. The 2-GiB
configuration is a reasonable dev-launch choice here; 8 GiB did not improve on it.
Observed C/A FPS difference: +16.53%. Materialized bytes per completed frame:
96.91% lower. One final run per configuration: these are observations,
not statistical significance or a guarantee. Repeated Present is not counted;
GPU-complete timestamps identify new guest frames. Timestamp polling is about 2 ms.

| Mode | Effective cache GiB | Local GPU usage peak GiB | Buffer heaps peak GiB | Working set peak GiB | Private bytes peak GiB |
|---|---:|---:|---:|---:|---:|
| A | 0.500 | 2.544 | 0.750 | 3.951 | 4.948 |
| B | 2.000 | 3.622 | 1.750 | 4.502 | 6.451 |
| C | 4.000 | 3.622 | 1.750 | 4.457 | 6.484 |
| D | 8.000 | 3.461 | 1.500 | 4.381 | 6.184 |

DXGI budget/usage refers to this process; separate memory-samples.json records
whole-adapter nvidia-smi usage, not interchangeable with process local usage.
Peak DXGI observations include startup budget-change records and capture records.
Configured budgets are caps, not allocations. All final modes retained their
requested caps without a reported GPU pressure event. The 8-GiB trial was permitted by the
observed DXGI budget after other-resource usage and the 2-GiB safety reserve.

| Mode | Materializations | Materialized GB | MB/completed frame | Cache hits | New cached GPU buffers | Evictions | Upload GB | Readback GB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| A | 74831 | 24.450 | 414.410 | 950038 | 5219 | 74606 | 24.488 | 1.570 |
| B | 3704 | 0.889 | 11.550 | 1376203 | 349 | 3352 | 1.771 | 1.084 |
| C | 4807 | 0.885 | 12.826 | 1213682 | 699 | 4108 | 1.663 | 0.965 |
| D | 3773 | 1.082 | 14.625 | 1302619 | 703 | 3079 | 1.913 | 1.033 |

| Mode | Snapshot spans GB | Watch-establishment GB | Snapshot compare CPU s | Materialization CPU s | Validation CPU s | Fence wait CPU s |
|---|---:|---:|---:|---:|---:|---:|
| A | 99.434 | 72.698 | 17.824 | 18.506 | 0.704 | 3.380 |
| B | 160.091 | 124.645 | 29.511 | 0.601 | 0.791 | 3.048 |
| C | 140.512 | 106.806 | 26.991 | 0.650 | 0.506 | 8.048 |
| D | 152.142 | 116.600 | 29.301 | 0.708 | 0.637 | 2.897 |

| Mode | Budget evictions | Envelope evictions | Other overlaps | DMA whole-cache clears / evicted mappings | GPU fence wait calls |
|---|---:|---:|---:|---:|---:|
| A | 0 | 44701 | 1309 | 57 / 28596 | 19799 |
| B | 0 | 3276 | 76 | 0 / 0 | 20527 |
| C | 0 | 4040 | 68 | 0 / 0 | 19589 |
| D | 0 | 3006 | 73 | 0 / 0 | 19842 |

| Mode | GPU pool reuse | Snapshot new / reused | Large staging new / reused | Retained snapshot peak MiB | Retained large staging peak MiB |
|---|---:|---:|---:|---:|---:|
| A | 69612 | 74831 / 0 | 1742 / 0 | 0.00 | 0.00 |
| B | 3355 | 349 / 3355 | 0 / 89 | 247.55 | 124.73 |
| C | 4108 | 699 / 4108 | 0 / 86 | 255.64 | 122.79 |
| D | 3070 | 703 / 3070 | 2 / 73 | 253.46 | 122.66 |

GB/MB above are decimal; memory-capacity GiB/MiB are binary. Snapshot spans count
validation ranges, not hardware bytes read by memcmp. CPU stage times are inclusive
and must not be summed. Counters use 2-second windows with slightly different edges
from the frame marker, so normalized work is approximate. Cache hits count lookups,
not unique draw requests. Misses correspond to materializations in these runs without observed buffer allocation/upload failures.
Budget eviction behavior is proven by the production regression; the table shows
whether it was exercised in each game window. No frame drops or diagnostic truncation
were observed. Background desktop activity was not controlled, adding run-to-run
variance; the measured differences do not isolate every component or establish
that larger requested budgets monotonically improve performance.

Changed functions: Rasterizer::RefreshBufferBudget, Find, PrepareShaderBuffers, DropCached, AcquireBuffer, RetireBuffer, Upload, Finish, CopyBuffer, ReleaseCpuBufferWatches and NotifyCpuWrite; PlacedBufferAllocator::CreatePage, SetBudget and Trim. New helpers manage bounded intersections, snapshots and pools. Allocation counters cover resident GPU-buffer backing, not every texture/index/descriptor allocation in the process.

Confirmed mechanisms and changes:
- A: address-ordered budget eviction discarded recently reused buffers. Mode uses
  last-use order among inactive candidates, while preserving active batch mappings.
- B/C: overlapping/incompatible envelopes still require safe retirement; no new
  GPU-owned coalescing or expansion across PRT holes. The tiny legacy DMA path also
  cleared the entire cache above 64 MiB. Mode routes aligned copies through the
  existing exact-footprint batch path, retaining unrelated hot backing.
- D/H: CPU changes continue to invalidate proof or upload changed content. They are
  not classified as avoidable stale-data reuse. Dirty ranges and CPU reads still reconcile.
- E: GPU ownership and PageWatch transfers remain authoritative; retained mappings
  exposed full-map overlap scans, now replaced by ordered intersection queries.
- F/G: 160-MiB allocations wasted 256-MiB heap pages. Compact large pages eliminate
  that measured fragmentation; incompatible/in-flight/pinned pool entries remain ineligible.
  Idle leases can be released and allocation retried once on a budget-specific failure.
- Reusable snapshots and large mapped upload arenas keep capacity separate from
  valid contents. Snapshot/staging content is verified with actual GPU readbacks.
  SHADPS4_DX12_CPU_POOL_MB (default 512) partitions retained storage: half snapshots,
  quarter large staging, quarter small upload pages. Required active/in-flight data
  and the pre-existing constant/descriptor/stream pools are separate working memory.

Diagnostics: exhaustive aggregate counters plus top-16 rows from a bounded first-4096
guest-address sample, not global top-N. Each row records request/mapping extent,
backing allocation identity, reuse/recreation, last retirement reason, ownership,
dirty state, completed fence, materialized/uploaded bytes. Excluded events are reported.
The initial A diagnostic used materialization-generation IDs and omitted global-clear
counts; final A/B/C/D use persistent pool allocation IDs and explicit clear counters.

Validation: original tests retained. 215/215 ordinary suite (69.36 s); 215/215
high-memory after fragmentation fix (70.88 s); final high-memory 216/216 (70.85 s).
Fail-before/pass-after regressions cover hot eviction, tiny-DMA cache destruction,
and watch lookup complexity (10.24M -> 10k candidates; 27.958 -> 0.353 ms).
Added tests cover real GPU-gated eviction, live leases under zero budget, compact
heaps, snapshots/staging without stale bytes, changed envelopes and PRT holes.
Existing production F1–F3, retry/recursion and concurrent/queued PageWatch tests pass.
An earlier high-memory suite failed on heap fragmentation; it is preserved in
suite-high-memory.log and was fixed, not bypassed.

Game limitation: still below playable speed. Old color/edge artifacts remain;
no new visible regression was identified. This is not proof of pixel correctness,
absence of every possible memory fault, or measured input-to-photon latency.
Optional index-buffer fast path is not included: the bounded session prioritized
cache correctness and four same-build game comparisons. Next concrete entry point:
GuestRasterizer::DrawInternal index conversion in guest_rasterizer.cpp around line 480.
Any direct path must join index residency with the shader request batch, preserve
the authoritative resource state, pin its allocation through the draw fence, and
fall back for endian conversion, primitive expansion/restart rewriting or incompatible aliases.

Tested dev launcher: F:/PS4Dev/handoff/high-memory-20261011/C-final/Launch-Bloodborne.cmd.
Best-sample 2-GiB launcher: F:/PS4Dev/handoff/high-memory-20261011/B-final/Launch-Bloodborne.cmd.
Exact core: F:/PS4Dev/handoff/high-memory-20261011/B-final/core/shadPS4-DX12-Core.exe.
Original installed core and all 20 save files match their preserved hashes.
Evidence: F:/PS4Dev/handoff/high-memory-20261011 (per-run analysis.json, CSV,
memory samples, PID/executable identity, screenshots, source archives and test logs).


## High-memory intermediate B and range-scan attribution

Clean 7a550c9, high-memory 2048 MiB: 54 completed frames / 91.348 s = 0.59115 FPS,
median 1685.006 ms, P95 1796.985 ms. Materialized bytes fell from A's 21.329 GB
to 0.673 GB (394.99 -> 12.47 MB/completed frame), misses 63,958 -> 3,641.
No meaningful average FPS improvement in this first comparison. Retaining the
cache exposed whole-map scans: validate-ranges CPU stage 0.201 -> 6.614 s/window.
The bounded lookup regression visited 10,240,000 -> 10,000 entries for 10,000
interior watch releases (1024 mappings), 27.958 -> 0.353 ms, Debug Layer enabled.
This component timing is not an FPS claim. New final game comparisons are required
after replacing full-map GPU-owned/watch/snapshot overlap scans with ordered lookups.
Evidence: ranges-before.log, ranges-after.log, B/analysis.json. B camera/movement
responded after capture; existing edge/color artifacts remain.

## Active — high-memory baseline (comparison pending)

2026-10-11: diagnostic A, c31111c plus archived instrumentation and explicit
512-MiB cap, high-memory disabled. Same prior seed and stationary Hunter's Dream;
30 seconds warm-up after visible world, then 91.267 seconds capture.
54 new completed frames, 0.59167 FPS; median 1734.169 ms, P95 1834.414 ms.
No dropped frame events. 63,958 materializations, 21,329,256,844 materialized bytes,
852,849 cache hits; 4,912 new GPU backing allocations, 59,046 pool reuses.
35,837 envelope retirements and 1,318 other overlap retirements were captured;
this initial instrumentation did not count whole-cache DMA clears. The final
control build adds their separate counter. 1,518 large upload allocations confirm
reusable staging work. Top sampled span rematerialized 159 times / 3.04 GB.
Counters use the diagnostic window, whose boundaries differ slightly from frames.
This is a new baseline, not the historical F1–F3 aggregate. No high-memory result yet.



## Active — final F1–F3 comparison, 2026-10-11

RTX 5080, driver 32.0.16.1692; Release /O2; Bloodborne CUSA03173 1.09, Hunter's Dream.
Four successful measured runs use fresh copies of the same full profile, original
saved character and shader cache seed; internal 1280x800, same patches, stationary
original camera, partial render 0. Only the assertion NOP at 024B56D2 was removed
from test copies. General.home_dir points into each isolated copy.
SHADPS4_NATIVE_D3D12_VALIDATION and SHADPS4_API_TRACE are absent in child environments;
startup/page-fault traces are absent too. Bounded diagnostics/frame capture arm after
visible world loading and warm-up. No build/suite overlapped any game benchmark.
There was no experimental marker in the isolated run roots; installed roots were untouched.

| Run | Seconds | New completed frames | FPS | Median ms | P95 ms |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 98.844 | 60 | 0.6070 | 1610.690 | 1703.886 |
| baseline-3 | 93.528 | 60 | 0.6415 | 1584.199 | 1650.093 |
| after-1 | 99.318 | 64 | 0.6444 | 1522.374 | 1589.832 |
| after-2 | 94.715 | 63 | 0.6652 | 1491.489 | 1580.313 |


Pooled completed-frame rate: 0.6238 -> 0.6545 FPS (+4.93%).
This is a modest observed improvement in two runs per version, with no statistical
significance claim. Neither build is playable. No 1% low claim from these small samples.
An additional baseline-2 attempt crashed before measurement (c0000096,
eboot+0x1196254); all successful windows and both fixed runs had no native exception
markers. Zero dropped trace events. Created/Present-queued counts are recorded separately
in each measurement.json and are not substituted for completed FPS.

| Run | Snapshot spans GB | Watch establishment GB | Readback GB | Upload GB | Materialized GB |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 220.238 | 0.000 | 7.454 | 26.403 | 26.391 |
| baseline-3 | 210.789 | 0.000 | 7.043 | 26.636 | 26.621 |
| after-1 | 106.981 | 75.509 | 1.727 | 26.722 | 26.699 |
| after-2 | 107.824 | 79.147 | 1.732 | 26.415 | 26.395 |


Readback per completed frame: 120.808 -> 27.238 MB
(-77.45%). Fixed commit bytes equal fixed readback bytes in each window.
The original snapshot counter records validation spans, including absent snapshots;
it is not a hardware count of bytes touched by libc memcmp. Include the new watch
establishment column when assessing total comparison work; do not call the first
column alone a 50% reduction. Fresh uploads now retain their exact staged version,
so the old/new changed-vs-unchanged call counts also have different meanings.
New fixed counters: requested copy 75.238/75.134 MB, destination union same in these
world windows; new total dirty union 1713.877/1732.287 MB includes conservative shader writes.
Watch upgrades 1159/1205, watch hits 26812/26714; establish-changed fallbacks 9/10.
GPU validations 1747736/1751272 are distinct from CPU validation/reconcile calls
344761/346824. Zero A/B commits before CPU access is proved by the production fixture,
not by claiming zero commits for the entire game. Unsupported image aliases still reconcile.
Finish reasons include batch-limit 186/189 and CPU-visible dirty 143/143; other actual
CPU consumers retain their existing waits. Full counters are in measurement.json.

| Run | Snapshot compare s | Materialization s | Overlap retirement s | CPU writeback s | Fence wait s |
|---|---:|---:|---:|---:|---:|
| baseline-1 | 29.949 | 10.289 | 5.457 | 8.989 | 4.019 |
| baseline-3 | 27.882 | 10.210 | 5.238 | 8.622 | 3.832 |
| after-1 | 19.042 | 21.090 | 6.610 | 7.675 | 3.622 |
| after-2 | 19.246 | 20.894 | 6.874 | 7.702 | 3.694 |


CPU wall stage totals are inclusive/nested; do not sum them into sequential frame time.
Snapshot capture now occurs during materialization, so that stage is not directly
comparable in isolation. Diagnostics use 2-second windows; their start/stop boundaries
differ slightly from the frame marker (sampled ~500 ms). Logs are selected by the
actual child PID, not historical logs copied with the profile; capture remained below
the 8192-record budget. Fence polling timestamps have approximately 2 ms granularity.

Camera and movement response observed outside the measurement windows; input-to-photon
latency was not measured. Same geometry retained, original color/edge artifacts remain.
No new visible regression identified; no pixel-correctness guarantee. The remaining
~26 GB materialized/uploaded per window and snapshot/watch work warrant bounded
attribution of request-range churn, not an automatic F4/F5 redesign.

Evidence: F:/PS4Dev/handoff/coherence-20261011 (CSV, PID/process paths, per-run JSON,
screenshots, test/build logs, seed manifest, originals-verification.json, final-verification.json).
Original ab1bf1a exe archived: 4d5c6944e6fd0594ace84a45abe37ade9365510edc37f5caad46321c0f8d8097.
Instrumented baseline: 3d14e00 plus archived instrumentation-only diff/header;
exe a0e9a6f92bcd5f3e5cb9cc7c7f489b132c0967a48b6a19902ff5d45b0f2e44db.
Fixed clean source 67d0ddc2df0294c9839f769821e7414681d43661;
exe c921b7e819a9c0d557ee23dae5a7f99075776f9a8b7e4f881c0d7e4705b1ccc5.
Tested launcher: F:/PS4Dev/handoff/coherence-20261011/after-2/Launch-Bloodborne.cmd;
core: F:/PS4Dev/handoff/coherence-20261011/after-2/core/shadPS4-DX12-Core.exe.
Diagnostics=1 with capture marker absent on normal launch; flags are set only in
the launcher child process. The final game was closed deliberately after control checks.
Native suite 211/211, 67.40 s. No push; original installed core and 20 saves unchanged.

## 2026-10-11 reconciliation recursion checkpoint

Baseline `ee765f4`, forced rebuilt core; fixed core uses request-local reconciliation.
Regression fails before and passes after; native suite 208/208 in 96.62s with Debug Layer.
This duration is validation, not performance evidence; the game overlapped part of it.
Baseline loading window at t=153.615 (~2s): 523 draws, 233 dispatches, 634 direct and
29 copy submissions; 1,279 buffer misses, 692,477,504 materialized bytes,
2,559,965,256 snapshot bytes, 389,699,720 readback bytes; fence waits 114.473ms/1,267 calls.
Fixed final window: snapshot comparison 605.094ms, materialization 182.217ms,
overlap retirement 108.483ms, snapshot upload 162.979ms. These are nested stage totals,
not identical-scene comparisons or frame times. No before/after full-frame result exists.
Exclude debugger pauses; compute timestamps do not cover complete GPU frames.
See FULL_FRAME_PROFILE.md for evidence and limitations.

## Integration checkpoint 2026-10-11

2026-10-11: native-buffer-range-lookup-test, 1,024 resident disjoint mappings,
10,000 interior lookups, Debug Layer on: CPU wall total 26.789 -> 0.366 ms.
One sample each; no percentiles or FPS claim. Finish/barriers/ownership unchanged.
Full native suite 208/208 (60.06 s), core build passed. Integration profiles/stages
differed and still failed: no controlled frame-time or transfer/submission delta.

## Parity correctness checkpoints

PARITY-E01: core build succeeds; native suite 206/206, 64.75 seconds.
PARITY-A01: core build succeeds; native suite 207/207, 54.09 seconds.
These are validation durations, not game performance benchmarks. No new CPU/GPU frame
measurements or FPS comparison were performed for these correctness changes.

## Correction: GPU timing validity at 774d095

The parity audit found that CollectTimestamps could consume a recording context using its
old completed fence (or zero on first use). A regression reproduced this before the fix.
The historical 0.0064 ms GPU number below is therefore UNVERIFIED and must not be used to
rule out a GPU bottleneck. CPU timing and submission counters have a separate mechanism.
No replacement performance measurement is claimed in this correctness cycle.

## 2026-10-10 compute dispatch cycle

Both runs used Bloodborne `CUSA03173`, the same profile seed cloned into separate directories,
the same executable location and arguments, `SHADPS4_NATIVE_DIAGNOSTICS=1`, and a 66-second
duration. Steady-state aggregation excludes reports before 6 seconds. Percentiles below are
the arithmetic mean of bounded per-window percentiles; total/average/max use all reported
steady-state calls.

| Dispatch stage | Baseline calls | Baseline avg ms | After calls | After avg ms | Change | Baseline/after mean P95 ms |
|---|---:|---:|---:|---:|---:|---:|
| copy-shader-hle | 6,422 | 4.8972 | 6,090 | 4.6275 | -5.5% | 38.197 / 36.305 |
| resource-upload-dispatch-writeback | 3,695 | 3.8074 | 3,503 | 4.0438 | +6.2% | 11.054 / 11.980 |
| shader-translation | 6,423 | 0.0031 | 6,091 | 0.0030 | -3.2% | 0.005 / 0.005 |
| pipeline-creation | 3,695 | 0.0005 | 3,503 | 0.0004 | within timer noise | 0.001 / 0.001 |
| image-operation-recognition | 6,423 | 0.0056 | 6,091 | 0.0054 | -3.6% | 0.040 / 0.038 |
| metadata-clear-recognition | 4,604 | 0.0002 | 4,366 | 0.0002 | within timer noise | 0.001 / 0.000 |

Copy-HLE counters after the fix: 1,727 recognized dispatches, 1,726 deferred, zero reported
eager reconciliations, and 4,315 validated page envelopes. Compute caches recorded 11 PSO
misses backed by 11 disk DXIL hits during startup, 3,559 PSO hits, and zero DXIL compilations.

No FPS result is reported because scene identity was not independently verified. Raw logs are
kept outside Git under `F:\PS4Dev\handoff\compute-dispatch-cycle`.

Validation: targeted metadata/compute tests 2/2 passed; full native suite 205/205 passed in
59.70 seconds. Tests enable the D3D12 Debug Layer, and the game-log scan found no D3D12 error,
corruption, device removal, native exception or dispatch failure.

## 2026-10-10 resource preparation and copy submission cycle

Both Bloodborne runs used cloned copies of one profile seed, identical arguments, a 46-second
duration, and diagnostics enabled. Aggregation excludes the first six seconds. This is a
controlled startup/load sample, not a fixed-camera FPS benchmark.

| Metric | Baseline | Batched copy uploads | Change |
|---|---:|---:|---:|
| Copy queue submissions | 4,046 | 2,678 | -33.8% |
| Direct queue submissions | 11,953 | 11,980 | +0.2% |
| Upload calls | 16,914 | 16,993 | +0.5% |
| Upload bytes | 5.254 GB | 5.204 GB | -1.0% |
| Resource bindings avg CPU wall | 2.1775 ms | 2.0052 ms | -7.9% |
| Buffer views avg CPU wall | 0.9203 ms | 0.8700 ms | -5.5% |
| Buffer reconcile avg CPU wall | 0.6725 ms | 0.6373 ms | -5.2% |
| Fence wait avg CPU wall | 0.5619 ms | 0.5174 ms | -7.9% |
| Whole dispatch avg CPU wall | 5.8671 ms | 5.8551 ms | -0.2% |
| Compute GPU execution avg | 0.0064 ms | 0.0064 ms | unchanged |

The stable upload volume and 4,056 recorded copy operations after batching show that work was
coalesced rather than omitted. No FPS result is reported. Raw logs remain outside Git under
`F:\PS4Dev\handoff\resource-cycle`.

Validation: targeted resource/metadata/wave32 tests 3/3 passed; full native suite 205/205
passed in 60.03 seconds. No D3D12 debug, device-removal, native-exception or dispatch-failure
marker occurred in the controlled game run.
