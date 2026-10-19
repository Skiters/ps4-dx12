# SESSION HANDOFF — active full-port checkpoint, 2026-10-17

## Current production ordered packed-SNORM checkpoint - 2026-10-19

Branch `fix/mixed-cmask` is at production commit **3b886393c6e7d345bbe298763ed4a9e045f60d91**. Single-sample packed 10:10:10:2 SNORM destination blending is now connected end to end through a rasterizer-ordered `R32_UINT` UAV: fragment exports preserve their source until the ordered read/modify/write, blend constants and per-target state reach the shader, non-dual-source blend factors and Add/Subtract/ReverseSubtract/Min/Max are implemented, masked lanes are merged, and quantization uses deterministic nearest-even SNORM encoding. Texture creation, UAV descriptors, transitions, barriers, ownership and lifetime are connected to this path. Invalid MSAA and dual-source cases remain explicit rejections.

Focused production verification passed. `native-port/run-gcn-frontend-test.ps1 build-migration-core snorm-color-owned gcn_attachment_alias` reported 27 passes, including real destination-plus-source ordered blending and explicit MSAA rejection. `native_dxil_raster_ordered_metadata`, `native_dxil_raster_ordered_gpu` and `native_guest_graphics_resources` passed; the hardware ordering fixture still reads exact value 64 from all 16 texels after 64 overlapping fragments. Incremental Release core compilation and linking passed. No game was launched for this identity, so image, gameplay, FPS and final-suite acceptance remain open.

The next production gap is independent MRT logic operations and write masks through the ordered path. Per-sample packed-SNORM MSAA is a separate open gap. Do not repeat the compiler/ROV or single-sample SNORM audit unless new evidence invalidates these checks. The installed application core is still the earlier `378bda3` build until a post-commit rebuild supplies an exact executable/PDB identity and replaces `dx12-current` safely.

### NEXT CODEX SESSION — START HERE

Read this file and verify the current Git HEAD. Rebuild the committed `fix/mixed-cmask` state, run the focused ordered packed-SNORM fixture, record exact executable/PDB hashes, and safely replace the single application core at `dx12-current`. Then continue production code with independent MRT logic operations and per-target write masks using the existing ordered UAV path; preserve contents, visibility, ordering, ownership and lifetime. Keep packed-SNORM MSAA explicit until implemented. Reuse the pinned Vulkan reference and existing measurements, avoid completed audits, run only mechanism-specific GPU/readback checks, and defer the full game suite until the integration boundary.

## Current ordered-render compiler checkpoint - 2026-10-19

Hardware D3D12 execution is now verified for the prepared ROV path: a 4x4 `R32_UINT` UAV received 64 overlapping pixel increments per texel, explicit UAV barriers preserved visibility, and readback returned exact value 64 in all 16 texels. `native_dxil_raster_ordered_gpu` passed in 0.41 s; the combined metadata/GPU pair passed in 0.36 s. Guest color-export lowering and ordered blend/logic selection remain open.

Branch `fix/mixed-cmask` starts this checkpoint at pushed commit **e40ad79**; compiler implementation commit is **53a8d45**. The DXIL compiler path now accepts one explicit pixel-shader UAV range and emits that exact range as rasterizer-ordered metadata. The Mesa backend writes `is ROV` into UAV metadata and sets the DXIL ROV feature bit; standalone and linked conversion use a thread-local, scoped selector that restores the previous selection after every compile. Non-pixel use and overflowing ranges are rejected. The public pinned Mesa runtime configuration ABI is unchanged. Shader cache option identity includes the selector.

Implementation is **connected and compiler-verified**, while the ordered post-blend renderer remains **open**. `native_dxil_raster_ordered_metadata` compiles an authored pixel SPIR-V fixture four ways and proves through DXIL reflection that the normal and mismatched variants do not require ROVs, the exact UAV does, the selector does not leak, and non-pixel use is rejected. The focused test passed in 0.20 s. Incremental Release core compilation and linking also passed. A detached clean checkout of committed **2dbca4f** produced `shadPS4-DX12-Core.exe` SHA256 **549cfbaf0aca86adb6a072e8447e3d89af494216f12fcbb75d22b75156e981db** and PDB SHA256 **456aea34b83355923974308f1fb0f5996948dfe0e9965caad43ae86987f24a4b**; CMake cache, Ninja graph and compilation database contain zero primary-checkout source paths. Runtime work still must lower the selected fragment export to an actual UAV operation, bind the matching UAV/resource state, preserve blend/logic/write-mask semantics and prove GPU contents/order/lifetime; the existing packed-SNORM blend and independent-MRT logic guards therefore remain.

A separate controlled run of installed core **378bda3** used the exact catalogue executable and copied profile. Exact-HWND capture now shows a rendered Bloodborne gothic scene in `F:/PS4Dev/handoff/gameplay-current-378bda3-20261019/window-initial.bmp`; the image was unchanged after five seconds because the game window never became foreground. The process exited normally with code 0 and verified runtime identities. No virtual controller was created, so entering an active world and XInput remain unverified. `guest-frames.csv` has only its header in this run; no FPS claim is made. Repeated `DX12 buffer ownership rejected` diagnostics were observed but have no established causal or performance meaning. The installed `dx12-current` identity remains unchanged, and no game/launcher/controller process from this check remains running.

Next exact code boundary: transform the applicable fragment export into a rasterizer-ordered UAV path, add the matching descriptor/root binding and transitions, then implement ordered packed-SNORM post-blend quantization and independent MRT logic masks. Verify authored destination/source blend values, per-target masks, ordering and readback before replacing the one application core. Do not treat the compiler metadata test as renderer parity or repeat the foreground-blocked input attempt without a new way to target the real window.

## Current packed SNORM and one-core integration checkpoint - 2026-10-19

This section supersedes the 2026-10-18 current-state section below. Branch `fix/mixed-cmask` is at production commit **378bda3aba1771026b9a2160bfca28a3867433b8** plus desktop catalogue-path fix **54ddffbae5a112027ffb074aa98feb85a9203904**. The full pinned shadPS4 port remains **ACTIVE / INCOMPLETE**; original reference revision is **e0a466753a1aacbc8db8253be685bf7ab4d9daae**.

Packed 10:10:10:2 SNORM render-target storage now retains exact four-byte UINT lanes and encodes fragment exports with deterministic NaN handling, clamp and nearest-even signed quantization. Compatible shared-MRT logic operations use valid D3D12 state with independent blend disabled; incompatible independent target masks are rejected explicitly. Shader cache identity is v8. Narrow verification passed: 24 SNORM draws, shared three-MRT XOR, existing SINT and 16 mixed-sample cases, four related native GPU/readback regressions in 16.887 s, and shader reload in 4.849 s. Evidence is under `F:/PS4Dev/handoff/storage-cleanup-20261018` in `SNORM_FUNCTION_VERIFICATION.json`, `snorm-final-gcn.json`, `snorm-related-native.json`, and `snorm-shader-reload.json`. Whole-group and final-suite acceptance remain open.

A clean detached Release build at 378bda3 used 54 pinned dependencies, 2283 compile inputs and no primary-checkout source paths. The application has exactly one package at **C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current**: EXE SHA256 **38b517c62b0a0dd04fa41489aa0a305b4c7ab0d59194ecf3462cd5a7b1f45350**, PDB SHA256 **03359c8ac054bd3127e5701cbf5322b0502719509bc2d03c8aabf4e98b5db057**, 19 manifest-tracked files, and preserved profile `external-7e8fca7a1bd828c19629fdd9`. Application catalogue and selected-core validation return success.

One controlled integration run of that exact EXE on a copied profile lasted 84.632 s, loaded verified DX12 runtime identities, produced 549 new valid GPU-completed guest frames after the marker, and exited 0 after graceful close. No unhandled application exception occurred and original profile save/config hashes stayed unchanged. This is **not** a gameplay or performance acceptance result: the window helper captured another application and could not activate the game window, so world/image/XInput were not verified and no FPS or speedup is claimed. See `INTEGRATION_RESULT_378bda3.json` and `integration-visual-limitation.json` in the same evidence directory. Do not repeat blind input; finish visual/XInput acceptance only when the actual target window can be observed.

Storage policy remains one current DX12 core and reusable build trees, with no per-test runtime archives. The completed cleanup removed 3310 obsolete outputs and 26 old installed packages; measured cleanup gains were C: 23.028 GiB, F: 42.155 GiB, D: 6.649 GiB. Current free space is C: 33.616 GiB, F: 690.124 GiB, D: 165.040 GiB; later unrelated cleanup is not credited to this project checkpoint. Protected unresolved d019 evidence remains. No launcher/core/debugger process is running.

Next production boundary is exact ordered post-blend SNORM quantization, followed by an ordered implementation for independent MRT logic write masks. Existing guards are not parity closure. Keep polygon point fill, mixed blend constants, attachment feedback and other recorded gaps explicit. Reuse completed checks and existing build trees; after changed code, run only the focused contents/ordering/lifetime checks and safely replace `dx12-current` after a clean committed build.

### NEXT CODEX SESSION - START HERE

Read this first section, verify `fix/mixed-cmask` HEAD/upstream and the single `dx12-current` identity. Preserve unknown `.serena/` and `CLAUDE.md`, the pinned original, d019 evidence, profiles and saves. Do not repeat the 129-group audit, accepted SNORM/logic fixtures, clean build or failed window-helper attempt without a changed reason. Continue exact ordered post-blend SNORM quantization in production C++/shader code, then independent MRT logic masks. Reuse existing build trees, run focused GPU/readback checks, build from the clean committed checkout, replace the one current core, and push normally. When reliable target-window access is available, complete one short world/image/XInput check and compare only valid completed guest frames excluding FG. Keep unverified limits explicit.


## Current storage correction / one current core - 2026-10-18

This FIRST section supersedes every older instruction to keep intermediate DX12 versions/manual artifact folders. Read [STORAGE_POLICY.md](STORAGE_POLICY.md) and current root AGENTS.md. Full port remains ACTIVE / INCOMPLETE; production core aa503d0, source history preserved in Git. Application selection explicitly migrated to **C:/Users/skei/AppData/Local/PS4Desktop/cores/dx12-current**, same EXE SHA256 **7e8fca7a1bd828c19629fdd9299155aead41bc13fb285792532905e5b1c1f5a0**, matching PDB/runtime/licenses and v2 source/build manifest. ProfileId external-7e8fca7a1bd828c19629fdd9 preserves the existing isolated profile. Installed app finds exactly ONE core and validates19 files. No core/game started in this work; previous launch-manifest remains factual historical evidence. Exact core runtime/game/full suite NOT_RUN; prior414GCN/18native/reload acceptance remains scoped to original fixture identities.

Cleanup completed:3310 obsolete loose PE/PDB/MAP/control outputs and26 legacy installed packages removed with hashes, scope/reparse/active-process checks and retained unique source inputs. H=F:/PS4Dev/handoff/storage-cleanup-20261018 contains deletion manifests/journals, FINAL_STORAGE_PROOF.json and inventory-after-final.json. Net disk free increase C23.03GiB/F42.16GiB/D6.65GiB; checkpoint free C27.99GiB/F42.63GiB/D164.93GiB. Handoff logical data now about16.98GiB across F/C/D, primarily protected diagnostics/reference/profile/source/reports. Known physical D archive binaries retired directly; old C junctions were not followed. Games, saves/profiles, pinned original/toolchain/dependencies, current active trees and unknown .serena/CLAUDE.md preserved. d019 exact-runtime/original exception/dump/logs/input remain because the original individual rejected range is still not established; fixed development runtime copies retired.

Storage implementation: desktop/package-core.ps1 publishes only stable dx12-current, stages/validates replacement, refuses active/locked/unknown old files, shares a publication/launch mutex and removes temporary/previous runtime on success. CoreVersions.ProfileId preserves profile identity across replacement; no stale acceptance is copied. tools/porting/publish-current-core.ps1 packages from the existing Release build, validates identities, includes licenses and can explicitly select the current core. Reuse native-port/run-gcn-frontend-test.ps1 / native CTest in existing build trees with codex-dev logs and small reports. DO NOT reuse archived run-gcn.py/save-release.py full-copy helpers. Packaging safety fixture and desktop core/profile/gate/external/diagnostic regressions PASS; desktop Release0 warnings/errors; updated installed app metadata checks PASS.

Next: continue existing signed-packed10 SNORM attachment gap, not a129 audit. Pinned SurfaceFormat maps to A2B10G10R10_SNORM; GetSupportedFormat has NO packed10 expanded fallback. Native SINT number5 is already connected. Candidate exact unblended/logic SNORM path uses raw R10G10B10A2_UINT and fragment encode to preserve untouched negative raw codes; blending still requires a genuine ordered quantization path. This is the next design boundary, not accepted behavior. Native host robust sample copies/mixed1/2/4/8 are already narrowly verified; guest sampled MS UMod is separate. Preserve B/I, PageWatch, ownership/UID/history, actual flags/casts and fences/leases. Final448 suite/extended existing-XInput game/image/input/stability/completed FPS excluding FG remain final. Build clean committed changes, replace the ONE current package after necessary checks and ordinary push.

### NEXT CODEX SESSION — START HERE (current)

Read FIRST SESSION_HANDOFF.md, STORAGE_POLICY.md and root AGENTS.md; verify HEAD/upstream. Preserve current sources/unknown work, pinned original and current application profile. Reuse existing primary/native/clean Release trees; no per-test runtime archives. Current app package cores/dx12-current is aa503d0 with manifest/hash/PDB; runtime/game NOT_RUN. Continue signed packed10 SNORM attachment from the exact existing guard and pinned fallback/export contract, keeping blending explicit until exact. Implement real production code, narrow GPU/readback/negative regressions, clean committed Release, safely replace one current core via publish-current-core.ps1 and normal push. Use saved129 registry/measurements; don't reimplement already accepted mechanisms or replay fixed d019. Full suite and expanded XInput game acceptance remain final. H/storage-cleanup journals explain retired historical binary paths; exact unresolved diagnostics stay protected.


## Current robust sample copy and mixed attachment checkpoint - 2026-10-18

This FIRST section is authoritative over historical current/next sections. Production **aa503d01ffde04363465eff368edb3865e022ec3**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 records and8 broad gaps remain. H=**D:/Codex-Handoff/ps4-dx12/mixed-samples-20261018**. H/VERIFICATION_FINAL.json preserves immutable run identities and confirms committed sources match final fixture snapshots after canonical line endings. User permits compilation and narrow native/GPU regressions; full suite and extended game remain final.

### A. Current state

Mixed color/depth1/2/4/8 queued draws now have real per-sample contents/depth/padding acceptance, not merely PSO creation. Host sample blits explicitly preserve pinned robust2 out-of-source component defaults, including RGB32 physical alpha. No new Bloodborne/image/input/FPS acceptance; original T#16 payload/view/transfer and broad remaining cases stay open. Existing buffer-envelope, semantic residency, independent B/I, DMA and inactive-history fixes remain preserved.

### B. Contract -> implementation -> check

Pinned vk_instance requires/enables robustImageAccess2; vk_runtime BlitHelper uses destination sample index. Excess source samples require zero existing components and format defaults for absent channels; 1x source broadcasts. D3D ld2dms leaves excessive sampleIndex undefined. GuestMultisample::PrepareCopy and guest_msaa_copy.hlsl now query actual source samples and return explicit defaults outside the source. RGB32 expanded physical RGBA uses alpha1 for integer and float-one raw bits for raw float. ROBUST_ALPHA is included in CopyKey/PSO specialization, so RGB/RGBA and numeric classes sharing a physical format cannot reuse an incompatible program. Valid samples/order/state/lifetime remain intact. Guest sampled MS fetch UMod is a separate original contract and unchanged. Prepare rejects readback into a buffer without actual ALLOW_UAV before GPU/descriptor mutation; upload remains allowed.

Commit aa503d0 includes production C++/HLSL, two new fixture headers and native CMake integration. Existing Guest-Tiling.cmake tracks host HLSL and generates guarded shader text in all3 build trees. No guest IR/cache namespace change; native cache v7 retained.

### C. Remaining work

Next existing gap: signed-packed10-attachment, specifically SNORM number1 (SINT number5 already connected). H/NEXT_MECHANISM.md identifies pinned SurfaceFormat/GetSupportedFormat/GraphicsPipeline -> LayoutGuestTexture/BuildGuestGraphicsState/GuestRasterizer. Establish original fallback/export contract then implement exact applicable per-fragment quantization/masks/blend/logic/MSAA/alias behavior. Do not merely remove the explicit guard or assume expanded float is exact. Remaining broad queue: color attachment feedback, formatted alias ordering, depth/color shared storage, signed packed10 attachment, polygon point fill, mixed blend constants, other-vendor clock, point capture stage domain. No129 re-audit.

### D. Confirmed failures, corrections and limits

Same functional robust fixture against committed fb738a6 fails RGB32 UINT sample2: physical alpha0 vs independently required1. H/old-control-build preserves exact EXE/PDB/rsp/runtime/source identity; this is actual GPU content failure. Final12 cases cover RGB/RGBA x UINT/SINT/FLOAT x both cache construction orders,2 layers, all valid sample bits and physical missing-alpha readback before guest RGB stripping, plus real no-UAV rejection. H/FIXTURE_CORRECTIONS.json records PRE-GPU depth-slice geometry fix, correction of old mixed red-tail expectation from pinned robust2 contract, and a fixture missing-UAV device-validation failure. No expectations masked, old logs retained.

Original d019 game int3 was captured/localized and fixed935fb69 (writable-B ownership after prewrite/Finish); original rejected individual guest range remains unknown. Evidence F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. No repeated game or assertion/exit-code/warning-only diagnosis.

### E. Measurements and verification

**414 GCN cases PASS**, exact EXE8126f3c025f07e729f7c239963a671fb48f5fef8d77f177ca57f7085dee14ec0. Includes16 new mixed color/depth combinations, warm blue, red Less and cached blue LessEqual, independent per-sample depth/color and whole64KiB guest padding; other398 are related existing regressions. **18 native fixtures PASS160.812s**, including buffer envelope/view/lifetime/GPU chain, semantic residency/preserved versions/DMA/pending fence, scheduler, independent MSAA pressure/requested/history and12 robust-copy content cases plus permission negative. **Shader reload PASS5.952s** with two PSOs, changed contents, failed replacement rollback/canonical restore, metadata negatives and same-version preload. H/PRECOMMIT_VERIFICATION.json retains original HEAD+patch identities verbatim; committing does not rewrite run identity. No full-suite, cross-version cache-hit, gameplay or FPS claim.

### F. Reproducible manual kernel

**H/artifacts-aa503d0**,18 files, clean detached checkout ataa503d01ffde04363465eff368edb3865e022ec3;54 pinned dependencies,2283 compile inputs and zero primary-source paths in compile DB/build.ninja/CMakeCache. Incremental Release build21.937s. EXE **7e8fca7a1bd828c19629fdd9299155aead41bc13fb285792532905e5b1c1f5a0**; PDB **0cf8895b7dae53a59e7a934f0f91878ebd17f4ea99026aaf8f5d90766fe5cc1c**. H/RELEASE_PROOF_aa503d0.json/release-identity.json preserve package identity. Exact rebuilt core runtime/game/FPS **NOT_RUN**; fixture checks are separate executables. Registry448 prepared tests (447->448); full448 and extended existing-XInput/image/input/stability/completed guest FPS excluding FG against saved Vulkan baseline remain final. Folder selection only, no automatic app install/select.

### G. Recovery and resource preservation

H includes start-state/patches, precommit source closure, PRECOMMIT_VERIFICATION/VERIFICATION_FINAL, original contract/spec excerpts, all failed and passing logs, old control and exact binary identities. Five CLOSED owned C handoffs (shared-exp-word, native-sample-fallback, msaa-word-writer, requested-sample-backings, sample-backing-pressure dated20261018) now physically reside under D:/Codex-Handoff/ps4-dx12/preserved-c-archives. All original C logical paths are NTFS junctions to byte-verified files; hardlinks retained. H/ARCHIVE_RELOCATION.json and per-folder proofs preserve before/after hashes. Redundant copies removed only after full through-junction verification; no old results lost. C freed~6.3GB; F remains~0.5GB, new bulk archives onD. No original Vulkan/saves/user process/HUD/controller changes. Unknown .serena/ and CLAUDE.md untouched.

### H. Exact continuation

From F:/PS4Dev/work/shadps4-dx12-src run `git status --short; git rev-parse HEAD; git log -3 --oneline`, then H/NEXT_MECHANISM.md. Continue specific signed SNORM attachment source contract and production path. Do not repeat already bounded guest-MS sample lookup audit or newly accepted mixed1/2/4/8 cases without a relevant change.

### I. Forbidden repetitions

No per-inactive-backing eviction that loses copy=false history, silent sample drop, invented16 stored CB/DB sample requirement, replacement of guest UMod by host robust-zero, acceptance transfer to rebuilt cores, changed expected data just to match implementation, fake budget success, suppressed assertions, skipped draw/dispatch, game/address/hash workaround,129 re-audit, repeated adapter queries/fixed d019 replay, unchanged full suite, automatic installation/selection or destructive cleanup of unknown work.

### NEXT CODEX SESSION — START HERE

Read FIRST docs/codex-progress/SESSION_HANDOFF.md and D:/Codex-Handoff/ps4-dx12/mixed-samples-20261018/VERIFICATION_FINAL.json; verify actual HEAD/upstream. Continue full original-shadPS4 native DX12 port from aa503d0, preserving unknown work. H/NEXT_MECHANISM.md gives next existing signed-packed10 SNORM attachment gap and exact symbols; establish pinned fallback/export/blend contract, implement production path, narrow real GPU/readback/negative checks, clean committed Release/manual kernel and ordinary push. SINT attachment and guest-MS UMod already connected; mixed1/2/4/8 and host robust defaults are narrowly verified, do not re-audit129 or replay fixed d019. Preserve independent B/I, PageWatch, ownership/class/UID/history, flags/casts, fences/leases and honest residency. Use saved measurements/identities; full448 suite and extended existing-XInput game image/input/stability/completed guest FPS excluding FG against saved Vulkan baseline remain final. Keep whole goal incomplete until all agreed port work and acceptance finish.


## Current inactive sample history pressure checkpoint - 2026-10-18

This FIRST section supersedes older current-state/next-task sections. Production **fb738a62d96a1b0364422a2c0dc8a3be580b9d72**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. H=**C:/Users/skei/Documents/Codex/handoff/sample-backing-pressure-20261018**. H/VERIFICATION_FINAL.json preserves exact source/runtime/EXE/PDB identity; all4 committed paths match final native/GCN/reload source snapshots. Previous goal turn was progress (8f2338e/032e337), not a wait/blocker. Latest human authorization permits compilation/narrow native+GPU checks; full suite/extended game remain final.

### A. Current state

Inactive sample backing contents now survive memory pressure until the owning image can be correctly evicted. Exact requested/native backing identity, original class/UID/history and independent B/I remain. This fixes a concrete correctness gap after the previous sample transition work. It does not close generic MSAA/renderer parity or claim performance/game acceptance. Full port and8 broad work items remain active.

### B. Upstream contract -> implementation -> checks

Pinned Runtime::UploadImage/CopyImage/CopyImageWithBuffer/ResolveImage use SetBackingSamples(copy_backing=false) for the destination; CopyImage can overwrite only a source-sized region, leaving older destination pixels observable. Pinned FreeImage retains all backing_images until whole-image deferred deletion, and GC skips tiled SafeToDownload images. Exact paths/hashes/excerpts are in H/REFERENCE_PARTIAL_BACKING_CONTRACT.json.

Native GuestTextureCache::ReleaseRetiredStorage previously erased inactive sample_backings when only its own allocation lease remained. The current active count cannot reconstruct a different backing's old uncovered pixels. fb738a6 retains that history as image residency; upload/readback pools, reconstructible per-sample UAV scratch and valid whole-image eviction retain their existing paths. No fictional budget success, skipped GPU operation, assertion suppression, changed oracle or game/address/hash workaround. Native cache namespacev7 unchanged: no shader IR/layout change.

### C. Next unfinished mechanism / source-scope correction

Continue real mixed color/depth backing behavior for supported1/2/4/8, keeping the independent T#16 boundary open. Pinned CB::NumSamples uses2-bit num_fragments_log2; DB uses2-bit z_info.num_samples. The normal graphics pipeline key is max(CB/DB counts), with capability-dependent normalization to depth count. AA coverage/exposed count can16 independently and feeds fragment runtime metadata. Prior handoffs conflated these with a normal16-sample attachment/PSO requirement; that was too broad for the pinned register path. T# last_level still encodes16 (and other raw values), so its actual valid shader-image payload/upload/SRV/readback contract remains UNACCEPTED. Do not close it by dropping samples or declaring all16 inputs invalid. No overall goal reduction or full MSAA closure.

Next concrete action: add one actual GCN Draw fixture mode to gcn_attachment_alias_test.cpp for the real common-count path. Reuse formatted-cmask's known VS/constant-color Draw setup (lines roughly156-201) without CMASK; reuse readonly-depth register setup (roughly737-759). Warm the same original color I with depth disabled, then draw a different constant color with depth enabled and color/depth sample logs0/1/2/3 independently. Use color base+4096, depth base+8192 (verify extents fit existing64KiB allocation), tile5,8x8 logical extent, no stencil/HTILE, depth Always/write true with initial0.75 and drawn0.5. Verify full independent expected color/depth logical words and whole physical padding after real completion/ReadMemory, not just PSO creation. At least cover color1/depth4 and reverse4/1 before broadening the small matrix. Keep real UID/history and B/I rules; don't manufacture CPU reconciliation if the pinned contract doesn't provide it. Existing formatted/depth fixtures provide the valid queue/register conventions; do not repeat the fixed GetCsRegs-outside-ASC harness mistake.

GuestGraphicsState::BuildGuestGraphicsState already normalizes counts; GuestRasterizer::Draw creates the PSO then SetBackingSamples for color; GuestImageReadback explicitly handles an active count differing from original desc near lines17-45. Use these exact symbols, no129 audit. If the queued draw exposes a real mismatch, fix production backing/view/PSO/synchronization and keep an independent negative/oracle. Then narrow related regressions and clean committed Release/manual folder/push. Keep original coverage semantics and the native physical-count rejection guard. No generic lowering of Upload's primary sample count until original T# semantics are established.

The copy=false pressure defect is now fixed atfb738a6: inactive sample backing history remains resident with its image until valid whole-image eviction, matching pinned Runtime/GC. Do not restore per-backing eviction as scratch. A future lossless spill is optional optimization, not accepted by this fix; it must preserve uncovered texels across partial writes and all lifetime/accounting constraints.


The broad queue remains color attachment feedback, formatted alias ordering, depth/color shared storage, signed packed10 attachment, polygon point fill, mixed blend constants, other-vendor clock and point capture stage domain. Converted/shifted/multiview/large aliases and other recorded gaps are not closed by this unit.

### D. Confirmed failure and limits

The identical NEW fixture header linked against clean committed8f2338e fails original1/alternate4 and original8/alternate1 with `Inactive sample backing retired despite observable copy=false contents`. H/old-control preserves EXE/PDB/rsp/runtime/release identity, exact source closure and per-case results/timestamps. The old failure checks premature retirement BEFORE the partial-copy oracle; do not claim observed old undefined GPU pixel values. New production proves retained history and actual post-partial-copy data. These headless expected failures are distinct from original d019 game int3, captured/fixed935fb69 (writable-B ownership after prewrite/Finish), with original context/dump/manifest at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Original individual rejected guest range remains unestablished; no replay or attribution to warnings/exit code.

### E. Measurements / accepted scopes

**native17 PASS147.598s** includes earlier buffer envelope/view lifetime/GPU chain, semantic residency/DMA/preserved/pending/scheduler, actual access/cast/packed/raw writers, independent image/MSAA pressure and requested backing transitions. New native_sample_backing_pressure_history runs2 cases: original1/alternate4 and original8/alternate1,2 layers, independent authored words per sample/pixel, active B different from inactive A, pending and completed pressure, exact allocation.use_count==1 (no hidden retention lease), honest zero-budget failure, same original allocation/class/UID/history, copy=false, real one-pixel shader copy changing every selected sample, all outside texels/full guest padding preserved and separate real GPU B readback unchanged. Native cache ownership callback is synthetic for isolated retention; combined real PageWatch+pressure/game acceptance is not newly claimed.

**398 GCN PASS**, EXE **d393f5711e73f76b8dc08e0063802f27fbb10bed5a71d5d6658ce463a991f1ff**:60 R9 MSAA writer,112 readonly R9 MSAA,64 signed packed MSAA and162 prior R9/R32/R11/RG16/mutable/ordered regressions with real queued GPU readback and independent oracles. **Shader reload PASS5.715s**:two PSOs, changed contents, failed replacement rollback/canonical restore, metadata negatives and same-version persistent preload. No guest frame-time/FPS or cross-version cache-hit claim. All4 final source paths match archives after canonical LF normalization; tested HEAD+worktree patches remain verbatim, not rewritten as a different run identity.

### F. Reproducible manual kernel

**H/artifacts-fb738a6**,18 files;54 pinned dependencies/2283 compile inputs/ZERO primary checkout paths in compile DB/build.ninja/CMakeCache. Clean clone F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 atfb738a6;18.718s build. EXE **74f0336d950be398312cfd3c0135f596c94f458ed8609233d81cc7740649638b**; PDB **a71db632df25aaa5b0e26d33c1aac6b725d8731b8c0f9ef1ece8de37648fc040**. H/RELEASE_PROOF_fb738a6.json/release-identity.json preserve the package. Exact rebuilt CORE runtime/game/FPS **NOT_RUN**; fixture acceptance is not transferred to that EXE. New focused CTest raises446->447. Full447 and extended existing-XInput game image/input/stability/completed guest FPS excluding FG against saved Vulkan baseline remain final. Manual folder only; no automatic app install/select.

### G. Recovery and resources

H/start-state.json, staged/unstaged-start and precommit patches/sources, PRECOMMIT_VERIFICATION.json, VERIFICATION_FINAL.json, reference contract and scope-correction JSON, native17-final, first-pressure-history, old-control, reg-* and shader-reload preserve recovery and exact identities. New fixture header and CMake integration are committed. Older kernels/results remain at old paths. Unknown .serena/ and CLAUDE.md remain untouched; original Vulkan, user saves/processes/controller/HUD/monitor unchanged.

Low free space: only known owned CLOSED outputs/compiler objects and immutable archive binaries were losslessly compressed. H/CLOSED_OBJECT_COMPRESSION.json (900 objects), CLOSED_OUTPUT_COMPRESSION.json, CLOSED_ARCHIVE_COMPRESSION.json and IMMUTABLE_ARCHIVE_LZX.json (130 unlinked immutable files in five explicit handoffs) preserve SHA256 before/after and free-space records. No old data deleted or moved. Prefer small/deduplicated archives and observe free space before large work; do not clean unknown files.

### H. Next command

From **F:/PS4Dev/work/shadps4-dx12-src** run `git status --short; git rev-parse HEAD; git log -3 --oneline`; read H/NEXT_MECHANISM.md and SAMPLE_COUNT_SCOPE_CORRECTION.json, then implement the queued mixed color/depth scenario at the exact existing fixture boundary. No new overall audit needed.

### I. Forbidden repetitions

Do not erase inactive backings as scratch merely because fences/leases retired. Do not fake a successful budget reclaim or use active contents in place of old copy=false history. Do not equate16 AA coverage with16 stored CB/DB fragments/normal PSO, nor delete the separate unaccepted T#16 boundary. No silent guest sample drop, relaxed resource flags/casts, B/I reconciliation, rewritten oracle, acceptance transfer to new binaries,129 re-audit, repeated adapter query, fixed d019 replay, false PageWatch crash, unchanged full suite or automatic core installation/selection.

### NEXT CODEX SESSION — START HERE

Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and C:/Users/skei/Documents/Codex/handoff/sample-backing-pressure-20261018/VERIFICATION_FINAL.json; verify HEAD/upstream and preserve unknown .serena/ and CLAUDE.md. Continue the full original-shadPS4 port from production fb738a62d96a1b0364422a2c0dc8a3be580b9d72. H/NEXT_MECHANISM.md gives the real mixed color/depth1/2/4/8 queued Draw scenario and exact symbols; add the narrow GPU/readback case, fix any proven production backing/view/PSO/ordering violation, then continue the existing functional queue. SAMPLE_COUNT_SCOPE_CORRECTION.json distinguishes stored CB/DB fragments from AA coverage/T#16; keep the unaccepted T#16 shader-image boundary open without inventing16 attachment requirements or dropping samples. Preserve inactive copy=false history, original class/UID/GpuModified, independent B/I, PageWatch, actual flags/casts, bind order, fences/leases and bounded honest residency. Use saved source/measurements; no129 re-audit, repeated capability query or fixed d019 replay. Build clean committed Release/manual kernel and normal push. Full447 suite and extended existing-XInput game/image/input/stability/completed guest FPS excluding FG against saved Vulkan baseline remain final. Keep the full goal ACTIVE until actually complete.


## Current requested/native sample backing checkpoint - 2026-10-18

This FIRST section supersedes older current-state/next-task sections. Production **8f2338e1aa2a2fcaafe5386e01ee71e443f9363e**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. H=**C:/Users/skei/Documents/Codex/handoff/requested-sample-backings-20261018**. H/VERIFICATION_FINAL.json retains exact source/EXE/PDB/runtime identities; committed9 source files match archived final code. User permits compilation and narrow native/GPU fixtures; full suite/extended game remain final. Previous goal turn was progress (bcc4bac/90ae162 and GPU proofs), not a wait or blocker.

### A. Current state

Alternate requested sample backing identities now differ from actual resource count, matching pinned Runtime::BackingImage semantics. Exact original1/8 images can transition to requested16/native8 and back without replacing the original allocation/class/UID/history or losing required original sample words. Original primary guest16 upload/storage/SRV/readback and generic graphics PSO fallback remain open; no new game/FPS acceptance. Prior readonly R9 MSAA writer/independent-B and physical buffer-envelope fixes remain regressions.

### B. Contract -> implementation -> checks

Pinned LiverpoolToVK::NumSamples/Runtime::SetBackingSamples -> GuestMultisample::SelectBackingSamples chooses supported physical count by halving; Entry::RequestedBackingSamples and requested-keyed sample_backings retain logical identity separately. GuestTextureCache::SetBackingSamples pins the real owning Entry, validates actual geometry/count, stages map/resource publication, prepares real per-index shader copy leases, reacquires RecordingList after possible reserve/wait/Finish, then publishes resources/requested identity/accounting. Copy=false restores the cached backing with no copy. Original payload fallback remains guarded: an original requested count cannot silently acquire fewer native samples. PromoteWritableStorage resets the new backing identity when replacing the physical domain.

Pinned non-block image attachment usage -> suitable supported2D single-mip color allocations retain actual ALLOW_RENDER_TARGET even from readonly T#, so MSAA->1x blit can restore the SAME original allocation. This grants physical capability without changing logical write access/history. GuestRasterizer::Draw explicitly rejects an attachment's physical count differing from the compiled PSO; common fallback mapping must be completed before that branch can proceed. Existing host HLSL already copies by sample index. No shader IR/layout/cache namespace change; native v7 remains. No assertion suppression, fake success, skipped draw/dispatch or game/address/hash workaround.

### C. Unfinished work / exact next boundary

Continue the ORIGINAL guest16 sample payload boundary and common graphics sample mapping, not another129 audit. GuestTextureCache::Upload still requires an exact native resource for desc.samples (including initial GPU/CPU tiling admission). GuestMultisample::Prepare/PrepareStorage require actual native count matching guest desc; Entry::Srv and shader sample-index/query semantics need a lossless original sample representation before generic primary16 can be admitted. GuestGraphicsState/GuestRasterizer::Draw must select mutually compatible physical PSO/color/depth counts; the new explicit Draw guard must remain until this is complete. Do not just apply SelectBackingSamples to Upload, relabel guest16 as8, discard sample words or weaken transfer/view checks. The helper now correctly handles ALTERNATE requested16/native8 backings of an exact original1/8 image; that is not full primary16/graphics fallback acceptance.

Also settle inactive backing semantics for copy=false under pressure before closing backing lifetime parity: ReleaseRetiredStorage currently erases inactive sample_backings after fences/leases allow. Current roundtrip proves retained cached copy=false contents only, not contents after inactive backing eviction. Compare real Runtime::SetBackingSamples(copy_backing=false) callers and original GC contract with the saved exact source symbols; preserve required contents/lifetime, do not invent success on reconstruction. Existing queued copy/current-list/pin behavior and true-copy reconstruction must remain.

Saved REFERENCE_BACKING_CONTRACT.json contains pinned Runtime::SetBackingSamples, BlitHelper and ms_image_blit.frag, including requested backing identity, explicit copy flag and per-index sample copy. Previous C:/Users/skei/Documents/Codex/handoff/native-sample-fallback-20261018/REFERENCE_SAMPLE_CONTRACT.json has original Image/RefreshImage and requested/native allocation evidence. BlitHelper::CreateMsCopyPipeline uses RAW requested ToSampleCount, not the image fallback helper; successful original unsupported16 blit is NOT established. Do not infer it from image creation or API masks. No repeated adapter query or fixed d019 game replay.


Eight recorded broad gaps remain: color attachment feedback, formatted alias ordering, depth/color shared storage, signed packed10 attachment, polygon point fill, mixed blend constants, other-vendor clock and point capture stage domain. Converted/shifted/multiview/large aliases are not accepted by this unit. No whole-group closure from source/build presence.

### D. Confirmed failures / limits

H/old-control and H/new-control archive IDENTICAL minimal source using only established APIs, EXE/PDB/rsp/runtime identities and individual runs: old cleanbcc4bac fails1x->4x->1x with `Invalid native sample backing copy resources`; old requested16 fails `Native sample backing count is unsupported by the adapter`. New source passes both. Full new fixture additionally proves GPU contents/padding; the minimal control alone does not. The two-stage cached RTV/capability failures are distinct from original d019 game int3, already diagnosed/fixed935fb69 (writable-B ownership after prewrite/Finish), with actual context/dump/manifest at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Original rejected individual guest range remains unknown; no new cause inferred from warnings/exit code or new game replay.

### E. Measurements

**398 GCN PASS**, EXE **101294c55653576d87cb67d8a98483b3136a254728a21417cfe213cc1b902ba9**:60 R9 MSAA writer,112 readonly R9 MSAA loads,64 signed packed MSAA and162 prior R9/R32/R11/RG16/mutable/ordered regressions. Real queued load/store/barrier, independent B and full65536 physical/padding oracles remain unchanged. Current tests do not transfer the previous CDB trace's binary identity; original class/UID evidence has its own archived runtime scope.

**native16 PASS136.628s**: earlier buffer envelope/view lifetime/GPU chain, semantic residency/DMA/preserved/pending/scheduler, independent image versions/storage pressure, access/cast/packed/raw writers plus native_requested_sample_backings. New fixture runs original1 and8,2 layers, independently authored distinct per-sample/pixel words; actual GPU donor CopyResource changes alternate backing, explicit no-copy preserves old original, true-copy returns new data. Real Flush/Finish/Collect inside current-list provider verifies resumed pending receipt. Invalid count, foreign cache, null current list and relabeled original16/native8 fail without map/accounting/identity publication. No inactive backing eviction acceptance here. Native16 archive matches all8 changed component sources at execution; later ninth source change is only Draw's mismatched-physical-count guard, compiled in final GCN/core but that Draw branch is NOT newly runtime exercised. **Shader reload PASS6.121s**:two PSOs, changed GPU contents, failed replacement rollback/canonical restore, metadata negatives and same-version persistent preload. No cross-version cache hit or guest FPS claim.

### F. Reproducible kernel / deferred acceptance

**H/artifacts-8f2338e**,18 manual files;54 pinned dependencies/2283 compile inputs/ZERO primary paths in compile DB/build.ninja/CMakeCache. Clean clone F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 at8f2338e,22.044s build. EXE **d9588b41b6742040a9c5a6343cc5d3891d3f16e194259a0bc14ec9abe9287c57**; PDB **5ece06de1285e368310852c5f87cb39bc3c9c073b360198e387229bb9e900024**. H/RELEASE_PROOF_8f2338e.json and release-identity.json contain the complete package identity. Exact rebuilt CORE runtime/game/FPS **NOT_RUN**; narrow fixture acceptance belongs only to its archived EXEs. New focused CTest raises registry445->446; full446 and extended existing-XInput game image/input/stability/completed guest FPS excluding FG against saved Vulkan baseline remain final.

### G. Recovery / preservation

H/start-state.json, staged/unstaged-start and precommit patches, precommit-sources, PRECOMMIT_VERIFICATION.json, VERIFICATION_FINAL.json, REFERENCE_BACKING_CONTRACT.json, old-control-source, old/new-control, native16-final, roundtrip-backings, first-backings, reg-* and shader-reload preserve progress and exact identities. First-backings predates the original1 fixture extension and has its own EXE. All9 sources and new include/CMake entry are in Git. Prior kernels/results remain at their original paths. Unknown .serena/ and CLAUDE.md remain untouched. No automatic core install/selection; original Vulkan, saves, game/controller/HUD/monitor and user processes untouched. Owned CLOSED build outputs were losslessly NTFS compressed, SHA256 checked before/after (H/CLOSED_OUTPUT_COMPRESSION.json); no data deleted. Disk space remains constrained; prefer C for archives and compress only known immutable evidence/closed owned outputs.

### H. Next command

From **F:/PS4Dev/work/shadps4-dx12-src** run `git status --short; git rev-parse HEAD; git log -3 --oneline`; then read H/NEXT_MECHANISM.md. Continue the explicit original payload/PSO/retirement boundary above using saved pinned source evidence. No new overall plan/audit needed.

### I. Forbidden repetitions

Do not equate alternate requested16/native8 success with original16 payload or rendering acceptance. Do not simply apply count halving to Upload, relax transfer/SRV guards, drop guest samples, mutate readonly original metadata, conflate B/I, fabricate actual RTV/UAV/cast capability or transfer fixture acceptance to rebuilt cores. No129 re-audit, repeated adapter query, fixed d019 replay, PageWatch first-chance false crash, unchanged full suite or automatic installation/selection.

### NEXT CODEX SESSION — START HERE

Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and C:/Users/skei/Documents/Codex/handoff/requested-sample-backings-20261018/VERIFICATION_FINAL.json; verify HEAD/upstream and preserve unknown .serena/ and CLAUDE.md. Continue full original-shadPS4 port from production 8f2338e1aa2a2fcaafe5386e01ee71e443f9363e, using H/NEXT_MECHANISM.md: original guest16 payload/upload/SRV/readback plus common physical graphics PSO/backing mapping; inactive copy=false backing contents under pressure remain unaccepted. Alternate requested/native identities and exact original1/8 return copies are connected/narrowly verified, not generic fallback closure. Preserve all guest sample contents, original class/UID/history, independent B/I, PageWatch, actual flags/casts, bind order, fences/leases and lifetime. Use saved reference and GPU evidence; no129 re-audit, repeated capability query or fixed d019 replay. Make production C++/HLSL changes, focused real-GPU/readback regressions with independent oracles/negatives, clean committed Release/manual version folder and normal push. Full446 suite and final existing-XInput game/image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. Keep the full goal ACTIVE until all required work is actually complete.


## Current readonly MSAA writable-plane checkpoint - 2026-10-18

This FIRST section supersedes older current-state/next-task sections. Production **bcc4bace8e85f31bc0aa921a80008496101d8f40**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. H=**C:/Users/skei/Documents/Codex/handoff/msaa-word-writer-20261018**. H/VERIFICATION_FINAL.json preserves exact runtime/source/EXE/PDB identities and canonical committed-source matches. Latest user authorization permits compilation and narrow native/GPU fixtures; full suite/extended gameplay remain final.

### A. Current project state

Original readonly raw-R9 MSAA I now survives actual writable R32/packed aliases through genuine per-sample UAV planes; original desc/class/UID/native allocation and history stay intact. All2/4/8 sample contents and independent B are retained. Readonly MSAA sampling at15b1c88 and physical-envelope/R9 mutable fixes at02a04f2 remain regressions. No new Bloodborne/FPS acceptance. Full renderer parity remains unfinished.

### B. Upstream contract -> production implementation -> verification

Pinned e0a46675 mutable _32BIT image backing and independent image/buffer semantics -> GuestTextureCache::MatchesGuestUploadStorage/CanRetainReadonlyWordStorage retain the original readonly MSAA class; CanUseMultisampleStorageView validates the WRITER view and actual native geometry/count/cast. EnsureMultisampleStorage(entry,view) transactionally allocates lazy single-sample planes/linear backing under the owning cache; PrepareMultisampleStorage retains original I descriptors. Entry::StorageUav checks the ACTUAL plane's UAV flag, single-sample geometry, cast and view bounds. guest_compute binds those plane UAVs and uses existing StorageLease read/writeback/barriers; the MSAA base UAV guard remains. RetireMultisampleStorage admits readonly word owners while preserving pending/fence/pin/lease/allocation guards and accounting. CompatibleGuestReadonlyWordStorage also permits ordinary R32 MSAA SRV reuse without rewriting original metadata. Real typed R9 IMAGE_WRITE/atomics are not newly admitted. No shader IR/layout change in this unit; v7 cache namespace remains.

Production commit **bcc4bac** changes3 production files and3 existing fixture sources; no missing CMake/include dependency. No assertion suppression, skipped draw/dispatch, fake success or game/address/hash workaround. Existing pressure CTest extended; total445 unchanged.

### C. Current unfinished work

Generic original unsupported-count fallback is the next unfinished MSAA mechanism. Use the SAVED REFERENCE_SAMPLE_CONTRACT.json at C:/Users/skei/Documents/Codex/handoff/native-sample-fallback-20261018 and same-LUID query at F:/PS4Dev/handoff/cached-r11-word-20261018/reference-format-query. Original Image::Image halves unsupported physical counts, BackingImage retains requested count, FindView/SetBackingSamples use requested identity, RefreshImage skips MSAA. Native GuestTextureCache allocation/SetBackingSamples, GuestMultisample transfer/StorageLease and shader/PSO sample mapping must preserve the actual upstream contract (including requested16 on the saved NVIDIA adapter). Do not just lower SampleDesc.Count or drop guest samples. First finish the exact requested/native backing, view, sample-index and transition mapping using these known symbols; then implement the necessary production separation. API masks alone do not establish semantics. No new129 audit or repeated capability query. Current readonly R9/R32/packed2/4/8 writer-plane mechanism is already connected and narrowly GPU verified; do not redo it without a new failure.

Other recorded broad gaps remain: color attachment feedback, formatted alias ordering, depth/color shared storage, signed packed10 attachment, polygon point fill, mixed blend constants, other-vendor clock and point capture stage domain. Converted/multiview/shifted/large aliases and requested16 are not accepted by this narrow mechanism. Graphics-stage R9 MSAA paths are connected/compiled, not newly GPU/game accepted.

### D. Proven failures and hypotheses

Identical FINAL queued fixture linked against clean15b1c88 fails with `Image writer beside a physical CPU stream needs independent ownership`, followed by Liverpool unhandled-exception assertion. H/old-writer-negative saves EXE/PDB/rsp/runtime/release identity; H/old-writer-final-fixture saves exact source closure. New production passes. This is an expected headless regression control, distinct from the user's earlier d01969e game0x80000003.

The original d019 game crash was already captured at **F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z**: ExecuteGuestBufferCompute threw `Independent image version lost its writable buffer ownership before execution`, then Liverpool coroutine assertion int3.935fb69 restores writable-B ownership after prewrite/Finish;4828eae fixes early-fatal cleanup. Actual context/dump/manifest preserved; original individual rejected guest range remains unestablished. No cause inferred from exit code, old overlap warnings or missing files; no replay now. Generic native count fallback mapping remains unproven, not diagnosed by capability masks alone.

### E. Measurements and GPU evidence

**398 GCN cases**, EXE **a30f855a56b4318986637e57831e15d107ae6c467869b1c5ca04f799b35fbc32**:60 new writer cases (R32 UINT/SINT/FLOAT and packed SNORM/SINT x2 descriptor orders xGpuModified0/1 x2/4/8 samples),112 readonly R9 MSAA,64 signed packed MSAA aliases and162 R9/R32/R11/RG16/mutable/ordered regressions. Actual PM4 seed/load/store/S_BARRIER/after-load; all-sample ordinary R32 and decoded R9 probes prove one changed sample/untouched others, independent B and full65536 physical/padding oracle. Independent expected constants retained. CDB H/ownership-msaa-writer and OWNERSHIP_METADATA.json:740 OwnIndependentImage entries,60 original UIDs, class34/7, readonly/raw flags00/00/01, sample counts2/4/8,0 ResolveImage.

**native15 PASS128.013s** covers buffer envelope/view lifetime/GPU chain, semantic residency/DMA/preserved/pending/scheduler, format/access/cast negatives, GPU packed/raw writers and independent image transfer/pressure. Extended pressure fixture: real per-sample/per-layer UAV clears, two layers, actual missing UAV/cast/sample/range negatives, preserved separate GPU B, exact accounting and I reconstruction; pending/fence-1/live-transfer/pin/allocation leases prevent retirement. Its synthetic cache ownership callback proves isolated-cache retention only; actual PageWatch ownership is exercised by the GCN fixture. **Shader reload PASS5.760s**:two native PSOs, changed contents, failed replacement rollback/canonical restore, metadata negatives and same-version persistent preload. No guest FPS/frame-time measurement and no new cross-version cache-hit acceptance.

### F. Clean Release / exact binary limits

**H/artifacts-bcc4bac**,18 manual files;54 pinned dependencies,2283 compile inputs,ZERO primary checkout paths in compile DB/build.ninja/CMakeCache. Clean clone F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 atbcc4bac;22.718s build. EXE **48c2806f4551cf29f7d966c82a73b4fa7ac1f3d045aa33cd2f91a8de8b97f7bd**; PDB **2064693cd71c21265209e40cf52b57d5705e05eb17e5baa71aef04b01778caee**. H/RELEASE_PROOF_bcc4bac.json and release-identity.json retain complete identity. Exact rebuilt CORE runtime/game/FPS **NOT_RUN**. Fixture acceptance belongs only to archived fixture EXEs with tested HEAD+patch identities retained verbatim; canonical matching does not transfer runtime acceptance to the core. Full445 suite and extended existing-XInput game image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final.

### G. Recovery and preserved state

H/start-state.json, staged/unstaged-start patches, precommit-staged/unstaged patches, precommit-sources, PRECOMMIT_VERIFICATION.json, VERIFICATION_FINAL.json, reg-* archives, native15-final, first-pressure, shader-reload, old-writer-negative and ownership-msaa-writer preserve source and diagnostic identities. Initial first-writer has its own EXE; accepted final fixture is writer-word-probes/reg-* with all-sample R32 probes. Prior working15b/02a cores and old evidence remain at prior handoffs. Unknown .serena/ and CLAUDE.md remain untouched. Manual core folder only; original Vulkan, saves, user processes, HUD/monitor/controller and app selection unchanged. New archives use C because F remains constrained; do not delete old results.

### H. Next command

From **F:/PS4Dev/work/shadps4-dx12-src** run `git status --short; git rev-parse HEAD; git log -3 --oneline`, then read H/NEXT_MECHANISM.md and the already saved reference sample contract. Continue production logical/native sample backing mapping with exact pinned semantics; do not restart the overall audit.

### I. Forbidden repetitions

No129 re-audit, repeated adapter masks, fixed d019 replay, first-chance PageWatch false crash or unchanged full suite. Do not mutate original readonly I to satisfy a writable guard, give the MSAA base a fictional UAV, reconcile independent I into B, silently halve guest samples, relax real native cast/flags or transfer acceptance to rebuilt binaries. No automatic core installation/selection.

### NEXT CODEX SESSION — START HERE

Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and C:/Users/skei/Documents/Codex/handoff/msaa-word-writer-20261018/VERIFICATION_FINAL.json; verify current HEAD/upstream and preserve unknown .serena/ and CLAUDE.md. Continue the full original-shadPS4 port from production bcc4bace8e85f31bc0aa921a80008496101d8f40, using H/NEXT_MECHANISM.md for generic unsupported-count MSAA fallback and exact requested/native backing/view/shader/PSO mapping. Preserve every required sample/content, original readonly class/UID/history, independent B/I, PageWatch, actual resource permissions/casts, ordered publication, fences/leases and retirement. Use saved Vulkan/DX12 evidence; no129 re-audit, repeated adapter query or fixed d019 replay. Implement production C++, use focused real-GPU/readback regressions with independent oracles and negatives, clean committed Release/manual version folder and normal push. Full445 suite and extended existing-XInput game/image/input/stability/completed guest FPS excluding FG versus saved baseline remain final. Do not transfer fixture acceptance to rebuilt cores or mark the overall port complete.


## Current readonly R9 MSAA word-sampling checkpoint - 2026-10-18

This FIRST section supersedes older current-state/next-task sections. Production **15b1c8829e00f35a1d750da98e39b9a7cf628322**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad work items remain. H=**C:/Users/skei/Documents/Codex/handoff/native-sample-fallback-20261018** (metadata, GCN/native/old-negative archives, manual kernel); P=**F:/PS4Dev/handoff/native-sample-fallback-20261018/packed-float8** (transport8). H/VERIFICATION_FINAL.json retains exact EXE/PDB/runtime/source identities. Previous goal turn made verified production/checkpoint progress.

### A. Current project state

Readonly R9G9B9E5 MSAA shader loads now preserve every requested2/4/8 sample through an R32 word resource; there is no typed R9 MSAA RTV/SRV assumption. Ordinary1x mutable R9/R32 and physical buffer-envelope fixes from02a04f2 remain as regressions. Readonly MSAA -> writable alias ownership, generic unsupported-count fallback, other recorded renderer gaps and final gameplay remain unfinished. No new Bloodborne run/FPS measurement; the exact previously diagnosed d019 user crash and early cleanup fixes remain separate from the expected headless old-production negative here.

### B. Contract -> implementation -> proof

Original _32BIT mutable storage and shared-exponent decoding -> resource_patching_pass.cpp selects native_raw32 for READ/query of an MSAA R9 T#; real R9 IMAGE_WRITE/atomics do not gain this read representation. spirv_emit_context.cpp and specialization.h choose UINT texels/packed34 decoding for raw readonly resources, record forced channel selection and apply shader swizzle after per-sample float decode. GuestTextureDesc::UsesRaw32 admits readonly data34 ONLY with explicit native flag and samples>1. GuestTextureCache allocates native R32 typeless MSAA with UINT views and validates actual R32 sample support. GuestMultisample validates the raw layout and moves exact words; logical format, readonly access, requested sample count and guest tiling/addresses remain intact. No assertion suppression, fake success, skipped draw/dispatch, game/address/hash workaround or guest sample loss.

The raw representation uses the existing per-sample transport and real native MSAA resources. It removes the typed-R9 format capability gate for these READs without reducing sample count. Generic per-adapter count halving is NOT implemented by this change. Native cache namespace **native-dxil-cache-20261018-msaa-shared-exp-word-v7** prevents old representation reuse.

### C. Unfinished production work

Preserve an ORIGINAL readonly raw-R9 MSAA I when an actual R32 MSAA writer view arrives. Adapt MatchesGuestUploadStorage, PrepareMultisampleStorage/EnsureMultisampleStorage and Entry::Uav/guest_compute per-plane bindings to validate the writer VIEW and the actual single-sample UAV plane, without setting entry.desc.writable. Retain original class/UID/GpuModified, independent B and pressure/lifetime behavior. Generic original unsupported-count halving/backing/view mapping remains a separate open contract. The current MSAA sampling unit is integrated/narrowly checked, not complete MSAA renderer parity. Graphics-stage R9 MSAA rendering is compiled/connected through shared frontend paths but not GPU accepted. No group is closed from code or build alone.

### D. Evidence and unresolved source mapping

H/REFERENCE_SAMPLE_CONTRACT.json saves pinned e0a46675 absolute paths/hashes/excerpts: Image::Image physically halves unsupported samples; BackingImage retains REQUESTED num_samples; FindView and Runtime::SetBackingSamples use requested backing identity. TextureCache::RefreshImage explicitly skips guest MSAA. Thus adapter sample masks do not establish a complete logical/native upload/view/shader mapping. Use the saved same-LUID query at F:/PS4Dev/handoff/cached-r11-word-20261018/reference-format-query (NVIDIA format122 mask0xf, format123 mask1; AMD both0xf). Do not invent native typed R9/4x capability or silently drop samples. Actual fallback behavior remains unverified; intended mapping needs completion in production.

The IDENTICAL final112-case GCN fixture against clean committed02a04f2 fails with the explicit 'Native guest color format lacks render-target support' exception; H/old-msaa-negative contains exact EXE/PDB/runtimes/release identity and output. This is an expected headless capability regression control, not a new diagnosis of the user's old game breakpoint.

### E. Measurements and accepted scopes

GCN EXE **f8a539891f7c4c5223283d78a10407d81453cd994330aa6bcc0d5b3361c36e7a** passes274 queued cases:112 NEW R9 MSAA loads (2/4/8 independently distinguishable sample words, tiles5/13, ordinary/forced channels on identical guest code/hash, changed CPU backing, minimum/maximum shared exponent and full65536 physical/padding oracle) plus162 prior R9/R32/R11/RG16/mutable32/ordered-buffer regressions. Independently authored expected words/colors remain unchanged by observed output. Debug layer errors are checked; real GPU readback proves contents.

P/packed-float8 PASS28.785s: data6/34, tiles8/13, samples1/4, mips/layers, changed backing, active single/multisample swaps/broadcast, all original bit patterns and padding. The fixture now explicitly selects the production raw32 representation for R9 MSAA; its actual resource assertion uses the R32 typeless family rather than a cross-family cast list. Numeric/bit/readback oracles are unchanged. This is NOT proof of an unmodified typed R9 MSAA resource path. Its archived unrelated GCN header is the earlier32-case version; required7 production paths plus packed_float_test.cpp match final code, not the uncompiled extra header. Final GCN/native/reload snapshots match all10 changed sources after LF normalization.

Native14 PASS105.480s: buffer envelope/view lifetime/GPU chain, independent image versions, semantic residency/DMA, preserved versions/pending fence/scheduler, actual format/access/cast negatives and packed/raw GPU writers. Shader reload PASS6.197s:2 native PSOs, changed GPU contents, failed-replacement rollback/canonical restore, metadata negatives, same-version persistent preload. No cross-version cache-hit acceptance and no guest FPS claim. Initial32-case success belongs to its own EXE; final112/274 acceptance belongs to the exact EXE above.

### F. Reproducible kernel / deferred checks

Clean committed Release **H/artifacts-15b1c88**,18 manual files;54 pinned dependencies,2283 compile inputs, ZERO primary checkout paths in compile commands/build.ninja/CMakeCache. Build22.657s. EXE **910a15112274a53713dd8de306b4acb4b82c2937dbf9b10cb4403e751d17079c**; PDB **31cc3750176d1006b69c9eda2b8a4aeb97d2788d21eb3de9816cc6fdeae94f01**. This exact rebuilt CORE runtime/game/FPS **NOT_RUN**. Standalone tested HEAD plus archived worktree patches are retained verbatim; source matches are not automatic runtime acceptance of a rebuilt binary. Full445 suite and extended existing-XInput game image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline remain final. No automatic app installation/selection; original Vulkan, user saves, HUD/monitor/controller setup and user processes were untouched.

### G. Recovery and storage

H/start-state.json classifies previous turn as progress; staged/unstaged-start/precommit patches, PRECOMMIT_VERIFICATION.json, VERIFICATION_FINAL.json and LOCATIONS.json preserve recovery and scopes. Unknown .serena/ and CLAUDE.md remain untouched. H/native14-final, H/shader-reload, H/reg-*, H/old-msaa-negative and P preserve runtime identities/logs. Previous working kernels and prior CDB ownership/envelope/crash evidence remain under the earlier handoffs. No old result/build was deleted.

Low free space was resolved by LOSSLESS compression of139 owned immutable archive files and34 CLOSED build outputs, SHA256 checked before/after. Source/old results remain at the same paths; hot build outputs used ordinary NTFS compression, immutable archives LZX. Exact free-space records are in H/ARCHIVE_COMPRESSION_LZX.json, CLOSED_BUILD_COMPRESSION.json and FINAL_STATE.json. Prefer C for new bulk archives until F has headroom. Do not clean/remove unknown files or old evidence.

### H. Next command / implementation boundary

From **F:/PS4Dev/work/shadps4-dx12-src** run `git status --short; git rev-parse HEAD; git log -3 --oneline`, then read H/NEXT_MECHANISM.md. Start the actual readonly MSAA writer-view/plane integration described there, not another capability audit. Keep the generic NumSamples fallback on the queue with exact logical/native sample mapping still open.

### I. Forbidden repetitions

No129 re-audit, repeated same adapter query, unchanged full suite, fixed d019 game replay or fake first-chance PageWatch crash. Do not equate capability-query/build success with GPU contents or gameplay. Do not use typed R9 MSAA failure to justify dropping guest samples. Do not mutate original readonly I merely to satisfy EnsureMultisampleStorage's writable-entry guard; validate the actual writer view and plane resource. No acceptance transfer to newly linked EXEs.

### NEXT CODEX SESSION — START HERE

Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and H/VERIFICATION_FINAL.json; verify HEAD/upstream. Continue readonly R9 MSAA to actual R32 writer alias ownership using H/NEXT_MECHANISM.md. R9 readonly2/4/8 sample loads are connected and narrowly GPU verified at15b1c88; ordinary1x R9/envelope fixes remain verified. Preserve original readonly desc/class/UID/GpuModified, all guest sample bits, independent B, PageWatch, bind order, actual UAV plane permission, barriers/fences, leases and pressure retirement. Use saved reference source/API evidence; do not repeat129 audit, capability queries or fixed crash/game replay. Implement production C++, use one queued before/store/barrier/after regression with distinct I/B and independent full-padding oracle, then related native checks, clean committed Release/manual version folder and normal push. Generic unsupported-count sample fallback remains open; full445 suite and final existing-XInput game/FPS excluding FG versus saved baseline remain final. Do not mark the overall port complete.


## Current shared-exponent mutable word / physical buffer envelope checkpoint - 2026-10-18

This FIRST section supersedes older current-state and next-task sections. Source **02a04f2ebc8630d83bcbc5864de137eae82483b2**, WIP **60bfc338de5960d513d3d60508dd9e0758c14b83**, branch fix/mixed-cmask. Full original-shadPS4 port **ACTIVE / INCOMPLETE**;129 independent implementation/behavior records and8 broad production work items remain. H=**C:/Users/skei/Documents/Codex/handoff/shared-exp-word-20261018** (metadata, GCN archives, manual kernel); B=**F:/PS4Dev/handoff/shared-exp-word-20261018** (native checks, old negatives, CDB). H/VERIFICATION_FINAL.json verifies all15 changed sources and exact executable/PDB/runtime identities. Main restoration source is this file.

### A. Current project state

Direct fixed-base single-sample mutable32 R9G9B9E5 reads/samples now share native storage with actual R32/packed writers. Existing R11, RG16 and original stage-order mechanisms remain. Only the narrow behavior below is accepted; complete renderer compatibility, graphics inter-stage data34, converted/shifted/MS aliases and final game behavior remain open. No new Bloodborne run or FPS measurement occurred. Previously saved user-crash diagnosis/fix935fb69 and pre-title cleanup4828eae remain distinct from headless regression negatives; their manifests/dumps and prior results remain in the earlier handoffs.

### B. Implemented contracts -> production -> checks

Pinned host_compatibility.cpp _32BIT mutable class -> NativeImageStorageAliases data34, resource_patching_pass.cpp raw32 read classification, guest_shader_frontend.cpp graphics context, GuestTextureDesc::UsesRaw32 and SPIR-V UnpackNativeSharedExponent. Exact mantissa *2^(exponent-24), alpha1; each texel decodes BEFORE filtering. Real guest R9 IMAGE_WRITE/atomic is not newly admitted or replaced with a fake operation. Physical SRV cast support does not imply a typed R9 UAV.

Pinned ImageUsageFlags non-block storage usage -> GuestTextureCache::UploadVersion creates direct readonly mutable-word allocations with physical R32_UINT/UAV capability and real cast lists while leaving the original readonly description intact. Writable allocations include the R9 SRV cast. Entry::Srv/Uav check actual format lists/UAV permission. CanRetainReadonlyWordStorage and CanRevalidateReadonly preserve current unmodified I without inventing original GpuModified. SetBackingAccess retention and actual ordered binding use this capability; preparation still does not publish a future readonly I's ownership. Typed packed10/RGBA8 join the same physical word comparison domain; expanded/depth/geometry exclusions remain.

Pinned SafeToDownload=GpuModified&&!Dirty -> guest_compute.cpp and guest_stage_bindings.cpp only transfer a genuinely written I into formatted B. Merely keeping readonly I current is not download permission. Original resource numeric class, UID and history remain unchanged.

Pinned CopySparseMemory physical B -> Rasterizer::PrepareBufferContents now reads physical backing when a CPU snapshot/upload envelope overlaps GPU I. The original protected guest-VA memcpy caused a PageWatch CPU fault, ResolveImage and I-to-B reconciliation outside the shader's original sharp. The strengthened queued regression writes Seed to exclusive R32 I while physical B keeps Old. IDENTICAL final fixture on clean committed60bfc33 fails (B=92004100, expected89020080); fixed code preserves B and original R32 I. No assertion suppression, draw/dispatch skip, game/address/hash workaround or artificial FPS claim.

Native cache namespace: native-dxil-cache-20261018-mutable-shared-exp-uav-v6. Allocator enhanced barriers, real GPU commands, existing ownership transfer, fences and allocation leases remain.

### C. Current unfinished work

The shared-exponent/physical-envelope unit is integrated and narrowly verified at 02a04f2. Port original per-adapter native sample fallback and its logical guest/native backing/view mapping, starting at GuestTextureCache::UploadVersion and GuestMultisample/Entry views. Use the saved same-LUID Vulkan query and pinned Image::Image / LiverpoolToVK::NumSamples; do not invent native4x RGB9E5 support or discard guest samples. Source and API capability evidence do not yet prove the original fallback's complete upload/bind/readback behavior. Broader converted/shifted/MS/large transfer and8 recorded renderer work items remain explicit; no129 re-audit needed.

### D. Confirmed problems versus hypotheses

Confirmed and fixed here: readonly retention incorrectly used a different-view sharing predicate; unmodified current I could be downloaded into formatted B; a protected-VA buffer-envelope CPU snapshot resolved exclusive I into B. CDB/GPU negative and positive evidence is archived. Old native abstract rejection of typed packed10 was beyond original _32BIT semantics: it became a positive logical-class check plus an ACTUAL typeless-packed-resource/missing-cast negative;8 geometry/access negatives, missing-list and missing-UAV negatives remain. Numeric/readback oracles were not adjusted to match production.

Confirmed remaining: saved default packed_float8 / explicit data34 tile8 samples4 native RT rejection. Same-LUID pinned Vulkan query on RTX5080: format122 samples0xf, format123 samples1; AMD integrated both0xf. Original NumSamples halves unsupported counts to1. Native4x R9 is not proved required on NVIDIA, while native fallback plus unchanged logical guest mapping still needs implementation/acceptance. Further fallback image/shader behavior is unresolved, not a verified performance or game cause. FPS impact of today's fixes is unmeasured.

### E. Measurements and exact scope

GCN EXE **19ca20dfbd17c56c73795ec85fcda081eff78dd49f3626d893f4b720d4ddbd4a** passes162 actual queued cases:80 shared-exponent (ordinary readonly R9 donor / original written R32 donor, both descriptor orders, optional seed stores, bilinear decode-before-filter, zero/minimum/maximum, later standalone typed SRV),8 R11,8 RG16 FLOAT,20 readonly mutable32 and46 ordered-buffer regressions. Real image_load/store/S_BARRIER, independent colors/bit words and full65536 physical bytes/padding checked. R9 donor70 OwnIndependentImage calls/20 original UIDs, flags000; R32 donor20 OwnImage+90 OwnIndependentImage calls/20 original UIDs, flags100; BOTH zero ResolveImage. CDB exit0, original class/UID retained. Exact timestamps/durations/commands in identities; fixture wall time is not guest FPS.

Final14 related native checks PASS105.270s: buffer envelope, view allocation lifetime, GPU chain, semantic residency/DMA cache, preserved versions/pending fence/scheduler, independent image transfer, mutable/word readonly GPU views, writable word class and packed/raw writers. The readonly view fixture now performs actual R32 UAV clear followed by typed GPU readback and asserts original description/UID/native allocation. Shader reload PASS5.658s:2 PSOs, changed GPU contents, replacement rollback/restore, metadata negatives, same-version persistent preload.

Standalone tested HEAD/worktree patches and EXE identities are retained verbatim; their15 archived source paths match final committed sources after LF normalization. This is not a claim that independently rebuilt binaries inherited runtime acceptance.

### F. Reproducible build / final tests

Clean detached committed Release **H/artifacts-02a04f2**,18 files, source 02a04f2ebc8630d83bcbc5864de137eae82483b2;54 pinned dependencies,2283 compile inputs, ZERO primary checkout inputs; build.ninja and CMakeCache also have no primary-checkout paths. Incremental build18.334s. EXE **f3ba36c5375354da7c0e29f071236c1c1f7a225a7ec071aeb96e813af5a94af7**; PDB **b8f004fbe7c5df4395a536f8eb5056fc5de3e88cebaa25c0ed4fd8cf798a339b**. This exact rebuilt CORE runtime/game/FPS **NOT_RUN**. Full445 suite and extended final existing-XInput acceptance remain final; new inter-graphics-stage data34 context is connected/compiled, not GPU accepted. No tests/game/UI/XInput/profiler processes remain from these completed runs; no user process, original Vulkan or save was changed. Manual folder only; no application installation/selection.

### G. Evidence / retained failures / build paths

H/LOCATIONS.json lists storage split and latest/previous kernels. Final GCN archives: envelope-positive, envelope-final-*. B/native14-envelope-final, B/shader-reload-envelope-final, B/ownership-r9-envelope-final, B/ownership-envelope-fixed; exact old controls B/old-production-complete-negative (4828eae) and B/old-envelope-negative (60bfc33). Earlier first-shared-exponent failure, contents-only phases, metadata failures, native expected-rejection failure and initial include/compiler mistakes remain; they are superseded only within their recorded scope, not erased. H/start-state.json, staged/unstaged-start patches, frozen/final source snapshots and the WIP commit preserve recovery. Unknown .serena/ and CLAUDE.md are untouched.

H/NEXT_MECHANISM.md identifies next symbols. Saved Vulkan query is **F:/PS4Dev/handoff/cached-r11-word-20261018/reference-format-query**; previous14 native/R11 proofs, old original C++/secondary cleanup dumps, working kernels and original reference paths remain under the earlier checkpoint. Existing HUD/live monitor/virtual XInput setup was not altered. Do not delete older builds/evidence or modify original saves. Lossless compression of owned immutable archives and closed outputs preserved byte hashes; free space is in FINAL_STATE.json. Check space before another large build/archive; redundant new MAP copies were omitted while PDBs were kept.

### H. Next action

In **F:/PS4Dev/work/shadps4-dx12-src**, run `git status --short; git rev-parse HEAD; git log -3 --oneline`, read H/NEXT_MECHANISM.md, then continue the specific native sample fallback contract in C++. Use existing native sample/backing code and saved adapter evidence. Preserve logical guest samples/geometry; determine the pinned original upload/view mapping before lowering native samples. Do not simply disable MSAA.

### I. Do not repeat

No broad129 audit, unchanged445 suite, fixed d019 game replay, previous fake first-chance PageWatch crash diagnosis, old missing-Elf fixture, unsupported typed R9 UAV attempt or arbitrary native4x R9 expectation. Do not accept contents-only results as class/ownership proof; initial R32 donor did resolve I and was not accepted until the different-I/B negative and corrected CDB trace. Do not equate build/link with GPU/game acceptance or transfer evidence to a rebuilt executable.

### NEXT CODEX SESSION — START HERE

Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and H/VERIFICATION_FINAL.json; verify HEAD/upstream. Continue original native sample fallback plus guest/native backing/view mapping in GuestTextureCache::UploadVersion, GuestMultisample and Entry views, using the saved Vulkan/DX12 same-LUID measurements and pinned Image::Image / LiverpoolToVK::NumSamples. R9 word codecs/readonly retention and physical buffer-envelope bug are now fixed/narrowly verified; do not repeat129 audit, old crash replay or completed experiments. Preserve contents, original class/GpuModified, PageWatch, original binding order, barriers/fences and allocation lifetime. Build/test each finished mechanism with exact binary identities, clean-checkout Release/checkpoint/normal push and manual kernel folders. Keep full445 suite and existing-XInput game image/input/stability/completed guest FPS excluding FG versus saved Vulkan baseline for final acceptance. Goal remains ACTIVE / INCOMPLETE.


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

### A/B/C. Original contract, implementation and unfinished work

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae creates mutable-format images and FindView builds typed views without replacing I for ordinary numeric interpretation. Existing DX12 typeless allocation already supports those families; an old pure-readonly numeric mismatch guard blocked it. New sharing does not reinterpret expanded packed values or depth aspects and does not grant a guest writer. MatchesGuestUploadStorage admits readonly reuse while still preventing a readonly allocation from satisfying a writer lacking UAV permission. Entry::Srv already maps RGB32 synthetic alpha to typed constant1;44-case proof checks it across numeric views.

Protected CPU reads in UploadVersion can reenter PageWatch::Fault ->GuestRasterizer::ResolveCpuFault ->Finish. Scheduler contexts are leased by fences; reopening can select a different list.21445a2 resolves current context at each subsequent GPU upload recording step instead of using the stale caller pointer.62c0d3b also selects the actual entry and restores pending receipt after current-context acquisition, since a nested submission consumed the previous one. Original GPU work stays submitted,resource states/access/barriers and source data stay intact; no CPU/B copy is silently dropped. Known original d01969e user crash ownership fix935fb69 remains separate and is not re-inferred from this SDK notification87a.

b19feb3 additionally preserves a named manual known-failing exclusive-I regression and detailed9-word output; no WILL_FAIL or fake green test. Native44 timeout is now300s for future runs because measured177.46s leaves too little ambient-load margin at180. Accepted44 run remains on its exact214 binary/configuration; metadata change is not new binary acceptance.

### D/E/F. Evidence, failed runs, oracle corrections and scope

H/closed-list-cpu-fault-stack and closed-list-debug-87a preserve exact EXE/PDB/map/debugger command/context/stacks. First debug attempt only caught bpe while SDK raises87a;87a-specific follow-up captured the actual first validation stack. H/readonly-pair-recording-only proves original numeric alias guard after current-list fix made legal seed GPU data work. No cause inferred from an exit code alone. Failure/oracle bookkeeping in FAILURE_AND_ORACLE_ANALYSIS.json and original excerpts/hashes in UPSTREAM_READONLY_AND_FORMATTED.json.

Keep authoring failures distinct: missing PM4 header fixed;4-byte writer required a real full raw GPU arena; CPU upload does not create original GpuModified (native fixture now performs actual GPU CopyResource before Own); formatted I->B requires original SafeToDownload GpuModified&&!Dirty; rawSNORM8 FF is signed-1/127, not -1 endpoint. These are documented independent input/oracle corrections; original failures remain failures and production output was not changed to fit them. Native ownership witness is a standalone physical-geometry model; full GCN uses real PageWatch. GCN modified1 positive uses a JOINT independent B/I seed. Exclusive standalone seed still fails latest source and is not hidden by that distinction.

Latest negative: H/exclusive-final-after-space/identity.json binds b19feb3 exact binary and original operands. B output13579bdf matches expected; typed image UINT givesdf/9b/57/13 instead offf/0/0/ff, UNORM returns the matching normalized marker. This proves contents divergence; precise ownership/dirty/retirement root transition remains unobserved. Preserve expected old I, do not change it to B or force the fixture into joint seeding for acceptance. Extended SNORM/sRGB/all numeric values/NaN/rounding/attachment feedback/inter-stage/game/full-suite acceptance remains unrun; no guest FPS claim from fixture durations.

Clean source/build proofs for21445a2,62c0d3b,b19feb3 and old immutable versions all retained. NATIVE_RUNTIME_IDENTITY.json and _PENDING.json distinguish native binaries; source-equal GCN manifests distinguish214/62 positives and currentb19 failed diagnostic. Generated host shaders match clean build after LF normalization; guest_bc_decoder.h/.cpp,guest_tile_address.h and both new fixture headers are tracked/integrated. WinError112 occurred copying exclusive-final-diagnostic archive, before launching that attempt; incomplete directory retained, not accepted as evidence. Only this newly authored H compressed without changing logical bytes; subsequent exact replay succeeded in RUNNING and reproduced its data FAILURE. Final checked manifest hashes and storage facts in STORAGE_NOTE.json.

### G/H/I. Exact continuation and recovery

First: git -C F:/PS4Dev/work/shadps4-dx12-src status --short then log -5 --oneline. Read H/NEXT_EXCLUSIVE_I_TRANSFER.md. Continue concrete exclusive-I ->later interior B writer problem at GuestRasterizer::OwnImage / OwnIndependentImage / PrepareGpuBacking, GuestBufferAccess ownership reconciliation and GuestTextureCache native-I history. Observe actual UID/GpuModified/Dirty/gpu_owned/buffer_alias_version transitions before choosing the production fix. Reuse original exactbase invalidation and SafeToDownload evidence; preserve old typed I,distinct B, CPU visibility,strong PageWatch guards,metadata,real submission order and resource lifetime. Keep current pending/context fix, readonly typeless sharing, previous source NativeFormat, original Fragment-before-Vertex/buffers-before-images order, replaced-I guard transfer and enhanced initial-layout tracking.

H contains complete precommit source copies/patches, all local commits, executable/PDB/runtime identities, debugger evidence, failures and exact next command. END_STATE.json records final docs HEAD/upstream/process state after checkpoint. Unknown context files preserved, no user process stopped. No repeated129 audit, original game crash replay, unchanged177-second numeric matrix or finished DMA/disjoint/budget work just for metadata. Eight broad source items and final behavior remain open; full port ACTIVE. Full suite and minimal existing virtualXInput image/input/stability/completed guest FPS vs saved Vulkan baseline excluding FG remain final. New kernels manual folder only.

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. Read FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/readonly-numeric-20261018/NEXT_EXCLUSIVE_I_TRANSFER.md, verify HEAD/status/upstream and preserve all work.21445a2 fixes reentrant closed command lists and readonly numeric typeless sharing;62c0d3b restores pending receipts; scoped GPU proofs have separate exact identities. b19feb3 provides current known-failing readonly-numeric-exclusive-owned: preserve oldff0000ff I after standalone image_store +interior B writer; actual typed views read B13579bdf. Localize ownership/dirty/history transition and fix production C++ without weakening guards,changing expectations or replacing the standalone case with joint seed. Reuse pinned source/metrics; no repeated audit/crash/unchanged full matrices. Build/narrow actual GPU negative/readback checks per mechanism, clean committed Release, manual kernel folders and normal push. Keep129 implementation/behavior records separate;8 broad gaps remain. F had disk-full; only new H was safely compressed and hashes checked; use C for new large evidence if needed, never delete old builds/saves. Full suite and minimal existing XInput game/image/input/stability/completed guest FPS comparison excludingFG remain final. Continue from latest checkpoint; full port incomplete.

---


## Current native packed19 MSAA checkpoint - 2026-10-18

Source **b5029e12430db8594cbb8d475e080c2d08b0ef5a**, fix/mixed-cmask; full port **ACTIVE / INCOMPLETE**. SESSION_HANDOFF FIRST section authoritative; historical sections retained. H=**F:/PS4Dev/handoff/packed19-msaa-20261018**. Original format19 B4 UNORM preserves all four4-bit channels/sample order. Production GuestMsaaUploadSource PACKED_MODE3 now normalizes alpha by15 (old GPU got000f vs expected0004). GuestTextureCache::CanTransferIndependentMultisample admits exact native B4/2-byte geometry, retaining format/sample device gates, RAW32/expanded separation and original retile/padding/order/lifetime. No float attachment approximation or unsupported-adapter bypass.

Exact native **85e0939c5afcec4eba18aa4175671a2b403d472d93899f940c9ce513885b8a99**, PDB **5f21aa3b54fcb198208c3465db887342a11fcffef93e1360df938957d8da9ddf**:6 actual AMD native19 cases(samples2/4/8 xlinear/macro,two layers),seed+ADD-blend phases,all16 alpha codes,RG-only/clipped/alternating sample mask,16016 independent typed sample-word probes plus wholeguest/B/padding readback PASS23.77s;2 existing packed16 raw/sampled MSAA tests PASS97.89s. Two separate old-source exact binaries reproduce alpha corruption and transfer-domain rejection. Own EXE/PDB/source/runtime/generated shader/LastTest manifests at H/VERIFICATION_FINAL.json. Initial narrower-alpha positive6-cases23.69s has separate identity. No PageWatch/full ownership,all blend/logic/MRT/CMASK/feedback/game/FPS claim; whole-group behavior acceptance unchanged.

Clean detached committed Release19.543s,54 pinned deps/2283 compile inputs/ZERO primary paths;18-file manual **H/artifacts-b5029e1**, EXE **a45cadb788600db50b0b2d3fee89ef0c49395b630bb2494e8b492c1574954562**, PDB **f2b74f597c102eff2ba7b6c0f2723ead9f60a9b94facc17cd0f685f725b2711c**. Exact core runtime/game/FPS NOT_RUN; no result transfer from fixtures. No auto app install/select,game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md retained. Prior core versions and test evidence untouched.

129 independent implementation/behavior rows retained. Registered native19 MSAA source gap is now resolved by native supported-adapter path; hypothetical fallback on an adapter unsupported by pinned original is not a requirement. **8 existing broad production work items remain**, not a full-port closure. Existing late-envelope guard is not itself proof of a missing code path: current capacity-union covers examined planned descriptors, unseen-growth hypothesis unconfirmed. Next actual guard: guest_stage_bindings.cpp PrepareOrderedGuestStages converted physical-domain validation / RefreshBoundImage common-allocation constraint, existing converted/multiview/shifted/MS/large alias queue. Follow H/NEXT_EXISTING_ALIAS_DOMAIN.md; no129 audit, crash replay or unchanged full matrix. Narrow GPU checks allowed; full suite/expanded existing-XInput acceptance remains final; normal verified commits/push/manual versions.

### A/B/C. Current implementation and unfinished production work

This checkpoint extends source-native codec74519e5/e277da5/edb2d69, rather than replacing it. Original pinned e0a466753a1aacbc8db8253be685bf7ab4d9daae SurfaceFormat maps Format4_4_4_4 to B4G4R4A4_UNORM_PACK16; original native image copies preserve each channel/sample. H/UPSTREAM_NATIVE19_CONTRACT.json saves original file/range/hash. Pinned Vulkan same AMD1002/13c0 supports B4 attachment/blend samples1/2/4/8; actual fixture D3D12B4 checks2/4/8. NVIDIA5080 B4 RT unsupported in BOTH APIs, so no parity requirement to silently synthesize a float RT. This is source/capability evidence and an independent bitfield oracle, not a measured full original-Vulkan render comparison.

Production C++ permits native19 exact sample transfer alongside16..18; actual device/sample/typed-buffer checks in UploadVersion / GuestMultisample::Prepare remain. Existing PACKED_MODE3 normalized RGB but returned alpha as integer0..15; native UNORM output clamped1..15. Alpha now uses the same /15 normalization. GuestTiler reverses native B4 nibble order to original guest B12/G8/R4/A0, preserving physical B-only padding and resource states/fences. No assertion disabled,draw/dispatch skipped,success fabricated,shader/game/address exception or alternate render precision introduced. New packed19_msaa_attachment_fixture.h and its CMake/main integration are committed with production changes.

### D/E/F. Proven failures, accepted evidence and limits

Two distinct baseline failures, both exact binaries/PDB/input snapshots/logs retained: native19-before-fix executed upload+independent typed GPU probe, observed000f instead of original0004; native19-alpha-only normalized alpha, passed typed seed samples, then RetileBufferVersion rejected native19 with Independent image sample backing has no exact transfer. Final positive uses all16 alpha codes, unchanged first0004 expectation, two layers, clipped RG-only additive draw and alternating sample mask. Every sample/pixel typed read is independently packed and checked against original bit fields, alongside full private retile/B/padding. Mismatched typed format/guest-word-size negatives remain. Native ownership callback supplies physical geometry for this transfer fixture; this does NOT verify PageWatch/complete ownership. Source-equality proof binds final native and related2 runs to b5029e1; earlier negative binaries have deliberately earlier source states, not that commit.

H/PRODUCTION_DEFECTS_AND_CONTROL_PROOF.json, VERIFICATION_FINAL.json, NATIVE_RUNTIME_IDENTITY.json, RELEASE_PROOF_b5029e1.json save identities/dates/session paths/results. Native generated guest_msaa_shaders.h and guest_tiling_shader.h match clean core after canonical LF. Prior native-packed6/depth-color/memory/GCN proofs remain attached to their original binaries; not transferred. LastTestsFailed.log dated05:11:49 retains deliberate alpha-only failure287; copied as H/historical_LastTestsFailed.log. Later native19-final LastTest explicitly passes287, related run passes286/288; stale aggregate file is not evidence of a remaining final failure. No rerun to erase history.

Fixture timings23.77s and97.89s are not guest frame times/FPS. Full original parity, all blend equations/rounding/logic/MRT/CMASK, attachment feedback, expanded acceptance, exact rebuilt-core game/image/input/stability/FPS NOT_RUN. Original user d01969e0x80000003 writable-B ownership crash was separately captured/fixed935fb69; evidence remains F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. No new root-cause inference or game replay here.

### G/H/I. Recovery, exact next code and prohibited repeats

Start with git -C F:/PS4Dev/work/shadps4-dx12-src status --short and log -4 --oneline; read H/NEXT_EXISTING_ALIAS_DOMAIN.md. Keep full original scope and8 broad production items. Investigate the EXISTING explicit converted-domain guard at PrepareOrderedGuestStages before editing it; map same-adapter original mutable-format/native contents/order/lifetime to a concrete real fixture. Do not turn the late-envelope hypothesis into a new requirement: all examined declared storage descriptors participate in physical preparation before final B views, and actual cached image binding retains ValidatePreparedShaderBufferRange. If a valid missing extent appears, preserve original PS-before-VS/buffers-before-images semantics, reserve capacity only, publish class/I/meta/ownership/residency at actual bind and keep stronger PageWatch watches. Preserve source NativeFormat, explicit replaced-I transfer, enhanced initial-layout tracking and all partial algorithms.

H retains start staged/unstaged patches, complete precommit sources including new header, old/final EXE/PDB/runtime/generated shaders, failed/positive logs, original capability/source contract and18-file immutable core. Clean clone detached b5029e1; no primary checkout source participates. Unknown context files remain untracked and unchanged; final docs HEAD/upstream/process state in H/END_STATE.json after checkpoint. No user processes need stopping. Prior working DX12/Vulkan/DLSS/HUD/live monitor/virtual XInput/profile/save/debug paths remain historical/untouched; kernels manual folder only.

Do not repeat129 audit, original d019 replay, finished memory/DMA/disjoint/budget studies, native19 before/after matrix or unchanged broad source-codec matrices merely for newer EXE metadata. Compile/link proves build only; fixture scopes remain exact. Final suite and minimal existing-XInput game acceptance/completed guest FPS versus saved Vulkan baseline excluding FG remain final. Goal ACTIVE/INCOMPLETE.

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/packed19-msaa-20261018/NEXT_EXISTING_ALIAS_DOMAIN.md; verify actual HEAD/status/upstream, preserve all current work. Native packed19 alpha/transfer production fix b5029e1 is compiled and narrowly GPU-proven with separate fixture/core identities; reuse saved Vulkan/DX12 metrics and source/capability evidence, do not repeat completed audits/crash/tests. Continue existing C++ converted-domain guard in PrepareOrderedGuestStages / RefreshBoundImage with original I/B publication, binding order, resource contents/states/visibility/lifetime and stronger PageWatch protections. Build/actual GPU negative-readback test each finished mechanism; preserve129 separate implementation/behavior records, clean committed Release and manual-folder kernels; normal upstream push verified commits. Only at final run full suite plus minimum existing virtual XInput image/input/stability/game checks and completed guest FPS comparison excluding FG. Full port incomplete; continue from latest checkpoint.

---


## Authoritative source-native packed codec checkpoint - 2026-10-18

### A/B/C. Current project, completed implementation and unfinished work

Source package **edb2d6945639ea09a0db381bbee4902a90d153a1**, branch fix/mixed-cmask; full original Vulkan-to-DX12 port **ACTIVE / INCOMPLETE**. SESSION_HANDOFF FIRST section is authoritative; prior sections are historical. H=**F:/PS4Dev/handoff/packed16-codec-20261018**. Narrow compile/regression/real GPU readback allowed; full suite/expanded game acceptance remain final.129 implementation/behavior records remain independent.

Pinned ResolveDepthOverlap / CopyColorAndDepth -> RetileImageVersion now separates SOURCE native codec/stride/depth from TARGET addressing via GuestTiler::NativeFormat / DescribeNativeFormat / Prepare. Direct nonexpanded16..19 word classes connect cached D16/D32 -> RT/RAW32 storage and color -> depth, keeping original min-mip/layer/lexicographic resources, old D despite B dirty, S exclusion, B tail/padding/meta/guard/fence leases.74519e5 production+fixtures; e277da5 original inverse image_store swizzle fixtures; edb2d69 capability-selected isolated test device. No production shader workaround or changed app adapter.

Exact identities/source-equality in H/VERIFICATION_FINAL.json. Native745:80 packed RT/UAV/reverse-copy cases PASS; two19 RT attempts fail on NVIDIA capability. Nativee277:8 sampled19->depth, paired depth and2 MSAA packed16 tests PASS. Nativeedb:16 previously inapplicable19 RT/reverse cases PASS on AMD. GCN745:24 CMASK16/17/18 cases PASS; GCNe277:32 packed16..19 actual image_store cases plus8 CMASK18 regression PASS; GCNedb:8 CMASK19 cases PASS on AMD. Source-specific binaries/results are NOT transferred to each other or rebuilt core. Two deliberately wrong SOURCE codec controls fail actual GPU bytes; original expected data unchanged.8 related envelope/preserved-version/DMA/pending-fence/allocation-failure/view-lifetime/scheduler/independent-image tests PASS11.97s. Independent whole65536 physical B/S/meta/padding checks and real PM4/GCN probes retained; no every-pixel/every-number/comp_swap/MRT/MSAA attachment/game claim.

**Scope correction:** NVIDIA5080 supports neither Vulkan B4 color attachment nor D3D12B4 RTV/blend; original has no B4 fallback. AMD supports both; isolated19 RT/CMASK passes there. Earlier universal19 RT fallback claim was an unproven inference, superseded by H/APPLICABILITY_SCOPE_CORRECTION.json; failures remain recorded. Conditional packed19 MSAA cross-API capability gap is not established/closed by single-sample results. Nine registered broad work items remain (eight known domains plus this conditional item); do not invent requirements beyond same-adapter pinned original. Storage fixture observation stride and inverse swizzle errors corrected from original evidence, not by changing production output; failed logs retained.

Clean detached committed Release18.271s,54 locked deps/2283 compile inputs/ZERO primary-checkout paths. Manual18-file **H/artifacts-edb2d69**, EXE **089c9465b88253a01d0e8d0272279a68b16e68fa71082bf061416b8aa15b2b3f**, PDB **8d6b8adf67f9fa17946c4d3ccc7a23e8ee58405b0603d8d3d13e70a462f9d0cf**. Exact rebuilt core runtime/game/FPS NOT_RUN. Earlier745/fd8/e1eed cores retained; no auto app install/select. No game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved untracked.

Next actual CODE: H/NEXT_ORDERED_DOMAIN.md; ordered late cached source resource growth beyond prepared envelope in GuestTextureCache::PlanCachedImageBinding / BindPlannedImage, PrepareOrderedGuestStages / ExecuteGuestResources and Rasterizer::ValidatePreparedShaderBufferRange. Keep descriptor allocation stable by reserving only necessary physical capacity before final views, while original I/B/class/meta/ownership/residency publication stays at actual bind. Guard remains until exact proof and GPU fixture; no silent clamping/removal. Converted/multiview/shifted/clipped/MS/large remaining domains remain explicit. Do not rerun129 audit, original crash or unchanged broad matrices. Normal verified commits/clean builds/upstream push; manual-folder kernels. Final suite and minimal existing-XInput/image/input/stability/completed guest FPS versus saved Vulkan baseline excluding FG remain final.


### D. Confirmed failures and hypotheses

Actual source codec defect: target packed flags were applied to source native footprint. D16+S native float32 must narrow depth before target placement; source packed color must undo its own component permutation. Production now supplies explicit SOURCE NativeFormat while retaining TARGET geometry. Alternate-TU target-codec/source-depth controls reproduce actual wrong bytes18 cc/e6 and16 fa/e6; unchanged independent expectations. Full controls/map/PDB/output preserved. Pure nullptr-codec16 control hits strict conversion guard rather than data mismatch; do not mislabel it.

Original user d01969e 0x80000003: already captured in **F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z**.935fb69 restored writable prepared-B ownership and preserved launcher fatal diagnostics. Exception/stack showed writable ownership failure followed by Liverpool coroutine int3. Do not infer a new cause from SRV/UAV warnings or missing files, and do not repeat that game launch. This continuation did not perform new game acceptance of that fix.

Historical failures in H/FAILURE_ANALYSIS.json remain failures: initial19 unsupported-RT runs, newly authored storage output stride2048 instead of4096, initial image_store expectation omitting ORIGINAL inverse DstSelect swizzle, control harness/cwd/u16 authoring errors. Original resource_patching_pass/resource/pixel-format snippets and hashes in H/UPSTREAM_STORAGE_COMPONENTS.json. Original CB draw19 f00f and image_store19 0ff0 are distinct contracts. No production expectations adjusted to match observed output. Native19 MSAA capability mismatch on same original-capable device remains a conditional question, not proven missing software fallback. No disabled asserts/skipped work/game-address-shader hacks.

### E/F. Measurements, accepted tests and limits

H/VERIFICATION_FINAL.json binds7 native run records and9 GCN run records to three exact source packages and their EXE/PDB/runtime/source/log identities. Accepted original packed cached copies total104 cases across exact binaries, including8 sampled19; GCN72 cases include32 CMASK16..19,32 image_store16..19 and8 CMASK18 regression. Related8 tests11.97s; paired depth31.98s; MSAA2 tests97.15s. These durations measure fixtures, **not guest FPS**; some independent GCN runs overlapped. NVIDIA capability failures were not renamed passes. Native tests use linear/tiled,GpuModified0/1,original one-mip layer growth/optional S; GCN uses69 UINT word probes across64 tile centers/boundaries and all65536 physical bytes. PDB/runtime/archive evidence retained; generated host shaders match clean build after LF normalization. Native runtime archive taken at final checkpoint; historical runs keep exact EXE/PDB/source, without inventing per-run OS/driver identity.

Full original functional coverage, every numeric mode/rounding/NaN/subnormal/comp_swap, MSAA attachment blending/feedback, full suite and rebuilt-core game/FPS are not accepted. Earlier measured Bloodborne/Vulkan/DX12 outcomes remain historical with original identities. Do not infer a working game or FPS improvement from these proofs.

### G/H/I. Exact continuation, recovery and prohibited repeats

Start: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` then `git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline`. Read H/NEXT_ORDERED_DOMAIN.md. Continue the existing late-source growth contract at the named functions, using capacity-only plans and original Fragment-before-Vertex buffers-then-images bind order. HasCurrentNativeContents requires OWNED I; HasValidLogicalImage alone is insufficient. Never publish future class/meta/I/ownership/residency during prewarm. Preserve explicit replaced-exclusive-I PageWatch transfer and enhanced initial layout flag from fd8e5f6. Keep min-mip1 differing-aspect original restriction and same-adapter feature applicability. Add a narrow actual GPU negative/readback fixture for the next completed mechanism; do not remove the prepared-envelope guard to make it pass.

All meaningful sources committed, including GuestTiler changes, dirty_cached_depth_fixture.h, GCN fixture, new required_format_device.h and CMake tests; guest_bc_decoder.h/.cpp and guest_tile_address.h remain tracked/integrated. H has precommit patches/full source copies, all prior core versions, failures, negative controls, PDB/runtime identities and clean Release proofs. Separate clean clone detached exact edb2d69. Current docs HEAD/upstream/status/process state in H/END_STATE.json after final checkpoint. Unknown context files preserved; no own running tests/builds are required for recovery. Previously recorded installed DX12/Vulkan/DLSS/HUD/live monitor/virtual XInput/profile/save/PDB paths remain historical and untouched.

Do not repeat finished129 audit, original d019 crash, readonly/DMA/disjoint/budget studies or unchanged80-case packed matrix merely to move acceptance to a newer EXE. Compile/link only proves build; GPU proofs remain exact-fixture scoped. Original Vulkan and user saves preserved. Goal remains ACTIVE until all applicable code gaps plus final suite/game acceptance complete.

### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. Read the FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/packed16-codec-20261018/NEXT_ORDERED_DOMAIN.md, check actual Git HEAD/status/upstream and restore from the latest checkpoint without resetting. Source-native packed codec is implemented and narrowly GPU-proven at74519e5/e277da5/edb2d69 with separate binary identities; reuse saved Vulkan/DX12 metrics and capability evidence. Do not repeat completed audits/crash/matrices. Continue C++ late ordered cached-resource growth at PlanCachedImageBinding/BindPlannedImage/PrepareOrderedGuestStages/ValidatePreparedShaderBufferRange while preserving original I/B ownership/residency/binding order, barriers and lifetime. Build and run minimal actual GPU negative/readback regressions per finished mechanism. Maintain129 independent implementation/behavior records, clean committed Release and manual-folder core versions; normal push verified commits. Only at final acceptance run full suite and minimal existing virtual XInput game/image/input/stability checks, comparing completed guest frames with saved Vulkan baseline excluding FG. Full port remains incomplete; continue from last checkpoint.

---



## Current packed-float word and enhanced initial-layout checkpoint - 2026-10-18

Source **fd8e5f6196c669ca91af300a8c9be85b2b270cce**, fix/mixed-cmask; full port ACTIVE/INCOMPLETE. SESSION_HANDOFF FIRST section authoritative; prior sections historical. H=**F:/PS4Dev/handoff/native-packed6-20261018**.

Pinned ResolveDepthOverlap/CopyColorAndDepth -> IsDirectDepthWordClass now admits direct R11G11B10_FLOAT(data6/number7) cached depth word recreation/full resource normalization. Native word copy/target seed/source S exclusion/original flags/metadata/fences remain. Real renderer exposed PRODUCTION synchronization defect: PlacedTextureAllocator::Allocate CreatePlacedResource2 made enhanced COPY_DEST but Resource defaulted legacy. Exact old initial-layout-control reproduces validation interop rejection. Resource.enhanced_barriers=true at successful creation fixes tracking without changing initial state or bypassing barriers. Enhanced access/order/shared allocation lifetime retained.

Exact native **edda6b122f90cbba3fd8a5357876cb06e3532f0d9c00d7f514264c9a88ac330a**:2 tests/16 finite6 RT/UAV real GPU cases81.09s PASS. Exact GCN **c83e458a8071a5438be97e3223f7e71228480ea101a7035ca33d24b892bdf3bf**:8 CMASK6 cases5.050s +8 CMASK10 regression4.265s PASS;21 typed RGB UF11/UF10 probes(63 channel words) per6 case, all16 logical tiles/draw boundaries,full65536 physical B/S/meta/padding;clean/dirty,S/HTILE. Independent finite mini-float arithmetic/word oracles, not production decoder expectations. Exact allocator **4866df31991cc37afe66e3aba4c9af380764969cd54dd57c610b92a867723c9e**:COPY_DEST+COMMON upload/copy/readback0.395s and texture heap/budget/live-lease/GPU texel0.333s PASS. Unsupported-cast error subbranch skipped on supported adapter; actual positive test not skipped. No NaN/subnormal/rounding/comp_swap/full-group/game claim. Initial Native16 pass82.30s had another exact binary before allocator correction; earlier failed GCN/control retained, final checks recompiled. Previous128-case3/5/9/10 at e1eedf9 retains own identity, not transferred to new core.

Clean detached committed Release18.313s,54 pins/2283 compile inputs/ZERO primary paths;18-file manual **H/artifacts-fd8e5f6**, EXE **5a4d8cd37ff31cd9bb76a58f8d6fee295d51a5208400e25d0d91b9d0701261c7**, PDB **bd0175aeeed0cdd29462128ae44bb57da8b24608249f9c5137d767602855f6fe**. Exact rebuilt core runtime/game/FPS NOT_RUN. All9 source/build files including texture_initial_layout_fixture.h committed. H has exact source/exe/PDB/runtime/negative/positive manifests, logs, precommit patch/full snapshots and generated-shader equality. No game/UI/XInput/profiler/user-process/auto install-select/Vulkan/save action. Unknown .serena/ and CLAUDE.md preserved untracked.

Next actual CODE **H/NEXT_NATIVE_PACKED16.md**: RetileImageVersion / GuestTiler::Prepare / guest_tiling.hlsl separate SOURCE native codec from TARGET addressing before admitting16..19. PackedFormat currently precedes Depth16: future D16+S -> packed target must not store low float bits or apply destination codec to source footprint. Current guards exclude this hazard. Keep original binding/word/ownership/envelope/fence contracts. Other known late-growth/converted/multiview/shifted/MS/large domains and nine broad gaps stay open;129 independent implementation/behavior rows preserved. No new general audit/repeated completed matrices; final suite and minimal existing-XInput game/completed frames remain final.

### A/B/C. Contract and unfinished work

Source-native D words remain distinct from current B. R11G11B10 recreation uses raw footprints/GPU retile, not numeric D32->RGB conversion. UF11/UF10 source values in new tests are finite and exactly representable through an independent F16 channel reconstruction. Full renderer uses original CB format6 clear word, real pending CMASK nibble, clipped guest draw and real queued PM4 image_load to observe native I; ReadMemory(B) is not an I publication request. Standard component selection only. Existing direct2/3/4/5/9/10 implementation remains; previous e1eedf9 evidence is separately archived and was not rerun/automatically relabeled.

Next16..19 CODE is explicit in NEXT_NATIVE_PACKED16.md. Retile must decode SOURCE native representation while TARGET layout supplies guest offsets/strides. PackedFormat-before-Depth16 ordering is a guarded hazard for future D16+S -> packed target, not an observed failure on an admitted path. Source codec/expanded/raw/CB remap semantics need real production adaptation and targeted GPU proof before loosening guards. Late source growth beyond capacity plans and other existing nine gap IDs stay unfinished. Missing full member tests for already connected numbers/comp_swaps are verification gaps, not presumed absent code.

### D/E/F. Confirmed defect, negative control and scope

The first8 CMASK6 data comparisons completed correctly but debug validation rejected enhanced COPY_DEST/legacy barrier interoperability. memory_allocator.cpp CreatePlacedResource2 accepted EnhancedLayout(initial) while Resource.enhanced_barriers stayed false. Minimal texture_initial_layout_fixture.h reproduced SAME validation rejection on exact initial-layout-control binary, without game or complex shaders. Fix preserves declared COPY_DEST/COMMON and uses enhanced transitions/access. Actual GPU readback and debug validation pass in initial-layout-fixed. Control/source/PDB/runtime/logs and first failed GCN retained; no exception/validation suppression or switch to COMMON to hide error.

Exact final manifests: H/NATIVE_VERIFICATION_FINAL.json (16 cases/2 tests81.09s), H/GCN_VERIFICATION_FINAL.json (8 finite6 +8 RGBA8), H/ALLOCATOR_VERIFICATION_FINAL.json (2 checks +negative control), H/FAILURE_ANALYSIS.json. Accepted binaries/source snapshots canonical-LF equal fd8e5f6. Generated clean host shaders equal fixture shaders. Native previous82.30s proof belongs to pre-allocator-fix EXE; final81.09s proves its own rebuilt fixture. H/native-fixtures12 files; GCN full runtime under cmask6-layout-fixed/cmask10-related. This is narrow fixture acceptance; exact clean core runtime/game/FPS remains NOT_RUN, and no duration here is guest frame time.

Original d01969e0x80000003 remains independently localized/fixed935fb69 at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z (manifest/exe/PDB/context/dump): ExecuteGuestBufferCompute writable-B ownership contract -> Liverpool coroutine int3. No replay, no inferred SRV/UAV or missing-file cause. Previous e1eedf9 explicit old-exclusive-I guard transfer fixes separate confirmed UnmapMemory retirement error; its old context/negative/positive controls at F:/PS4Dev/handoff/depth-color-formats-20261018 remain valid for their exact binaries. Original PS-before-VS and current-owned-I/capacity-only semantics preserved.

### G/H/I. Recovery and first continuation action

Separate F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 detached EXACT fd8e5f6, clean status,Release18.313s. RELEASE_PROOF_fd8e5f6.json / raw command-20261018-042003-47240.log bind54 pinned dependencies/2283 inputs/zero primary paths. Manual core path and exact EXE/PDB hashes above; no automatic app install/select. Earlier e1eedf9/641f6b9/working/Vulkan/DLSS/HUD/monitor/XInput/profile/save paths remain historical and untouched. Unknown .serena/ and CLAUDE.md preserved. Check END_STATE.json for final docs HEAD/upstream/status; no owned test/build command remains after checkpoint.

First command: git -C F:/PS4Dev/work/shadps4-dx12-src status --short; git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline. Read this FIRST section and H/NEXT_NATIVE_PACKED16.md, then edit named source codec/tiler gates directly. Reuse saved original contract/metrics; no129 audit, original crash replay or unchanged broad matrices. Narrow mechanism regressions/GPU negative+readback checks are allowed. Keep actual stronger B guards, explicit replaced I transfer, enhanced initial model, complete guest bytes/PS4 ordering and fence leases. Normal verified commits/clean builds/upstream push, manual folders. Full suite and minimal existing-XInput world/image/input/stability/completed guest FPS versus saved Vulkan baseline excluding FG remain final; full port NOT COMPLETE.


## Authoritative native depth/color words and guard transfer checkpoint - 2026-10-18

Full original shadPS4 -> native DX12 goal **ACTIVE / INCOMPLETE**. Source package **e1eedf9b70cbd9eb1f3ee9e0c150f3eef58e7f6d**, fix/mixed-cmask; this FIRST section supersedes historical sections. H=**F:/PS4Dev/handoff/depth-color-formats-20261018**. Narrow compile/regression/GPU/readback authorized. Full suite and expanded game/XInput/image/input/stability/completed guest FPS comparison excluding FG remain final. No game/UI/user-process/profiler/automatic kernel install/select action here; original Vulkan/saves preserved.

### A/B. Original contract and connected production changes

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae ResolveDepthOverlap -> CopyColorAndDepth preserves OLD initialized native D despite CPU/B dirtiness, excludes S, maps original mip/layer intersection and target strides, keeps target-only B seed/padding, original GpuModified0/1 and allocation lifetime. Existing UPSTREAM_DIRTY_RECREATION / stage-order evidence reused. Original resource binding is Fragment BEFORE Vertex, buffers then images; not hardware execution order. Capacity-only plans do not publish future I/class/meta/ownership/residency.

Production IsDirectDepthWordClass replaces format2/4-only gates in IsCachedDepthRecreationSource / CanMapIndependentDepthImage / ResolveDepthViewStorage with exact physical direct16/32 classes2,3,4,5,9(UNORM/UINT),10. New RG8/RG16/RGB10A2/RGBA8 RT/UAV can copy cached old depth WORDS via native texture footprints and GPU retile, without float numeric color casts. Resource normalization retains original lexicographic extent and source-native stride/min-mips1 rules. Expanded/packed-permuted/BC/native stride guards remain. New code/color-source growth for these classes is CONNECTED; extra tests for every numeric mode/source direction are not implied.

SetIndependentOwnershipTransfer conveys the actual replaced Entry at RecreateCachedDepthAtBind and independent UploadVersion conversion. OwnIndependentImage -> AcquireGuestImageOwnership -> TrackImageReadOnly carries old exclusive image plane ranges. PageWatch transaction splits at explicit source/target/strong-B boundaries; only transferred source guards demote to READONLY. Stronger B and unrelated exclusive pages remain NOACCESS; missing/invalid old proofs reject without partial mutation. No RW store window, disabled assertion, skipped operation, address/game/hash workaround or fake success. Native source/copy/descriptors remain pinned through fences.

### C/D. Confirmed new failure and unfinished work

Initial real renderer RGBA8 CMASK clear/draw produced correct I pixels, but next allocation unmap threw Cannot retire an image-only read-only watch tail. H/cmask-watch-debug2 captured actual first-chance C++ e06d7363 and stack UnmapMemory -> DiscardBufferImageVersions -> ReleaseIndependentImageWatches -> ReleaseUnusedReadOnlyWatches. Separate bounded owner trace proved source uid2 exclusive NOACCESS, new uid3 independent but still NOACCESS. The old source was retired while its strong guard was never explicitly handed to the new Entry. The new transfer operation fixes this; strict untrack protection remains. Old binary/source/exception log retained under cmask-tile-address / cmask-watch-* / cmask-owner-trace. Initial debugger -g run did not configure the event command and is NOT cited as capture; debug2 explicitly configured at loader stop and continued to actual throw.

Original user d01969e0x80000003 remains independently localized/fixed935fb69: ExecuteGuestBufferCompute writable-B ownership contract -> Liverpool coroutine int3. Manifest/exe/PDB/actual context/dump F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z; rejected individual guest range unknown. No replay and no inference from SRV/UAV or missing-file warnings.

Next CODE: **H/NEXT_NATIVE_FORMAT_CODE.md**. Remaining actual gates include native packed float6, source-versus-target native codec for permuted/expanded16..19, late source extent exceeding prepared envelope and existing shifted/converted/multiview/MS/large snapshot domains. Missing full member CMASK tests for other already implemented numeric/comp_swap modes are verification, not automatically absent implementation. Signed packed10 attachment/MSAA19/shared depth-color feedback and other existing nine gap IDs remain open. No broad129 audit or new requirements beyond pinned original.

### E/F. Exact verification and limits

FINAL native matrix:16 tests /128 real GPU cases PASS in 641.63s. Exact native EXE **bfcf069feff03b725b3ef49b01888ed6b0525c1c0671bef702b9fe2b53dfed3f**, PDB/runtime/sources H/native-fixtures and color-final*. New classes3/5/9/10 x UNORM/UINT x RT/UAV x linear/tiled x GpuModified0/1 x same/growing original one-mip layers. Native source after copy, source S exclusion, target before/after actual clipped DrawInstanced/Dispatch and clear, independent typed channel packing versus raw/guest retile, all B inputs/capacities/padding/HTILE retirement and held-fence leases checked. Other numeric modes in admitted classes are CONNECTED but not established by this matrix. No timing here is gameplay FPS/frame time.

Exact GCN EXE **523e102e649403f30d0691b7c72255dbe9aacc3ee04a354bc098cc54f7df4470** / own PDB/full runtime/source/compile+link responses:8 full renderer RGBA8_UNORM CMASK4.713s +8 single-stage PM4 storage4.406s +8 original PS-before-VS ordered4.539s +4 previous depth caller4.388s PASS. CMASK native I0.875 stays separate from physical B0.125(clean)/0.25(dirty); partial clear and disjoint clipped draw alter I only.21 real guest image_load UINT word observations cover all16 logical tiles and draw boundaries;65536 physical bytes include B/S/meta/padding. Not every pixel/number/comp_swap/MSAA acceptance. PM4 dispatch uses real active Liverpool queue; old external GetCsRegs patterns remain unready.

H/RELATED_VERIFICATION_FINAL.json:9 exact-binary checks PASS: transactional PageWatch CPU negative/ownership tests, actual GPU page ownership/readback46.167s, scheduler0.543s, preserved versions0.511s, DMA0.428s, pending fence0.495s, covered envelope0.311s, descriptor lifetime0.300s, independent image versions5.090s. PageWatch includes explicit missing-source transaction rejection and preservation of strong B/outside exclusive guard. Scheduler's first direct invocation omitted required diagnostics env and failed timestamp availability; corrected declared env, only scheduler and not-yet-run checks rerun. Its failure retained. No broad suite repeated.

H/FAILURE_ANALYSIS.json preserves all earlier attempts and scope corrections. New UNORM0.5 command hit assumed tie rounding127vs128; NEW commands use exact0/1 endpoints, arbitrary copied words and prior oracles unchanged, no tie parity claim. New CMASK descriptor needed original P8 slice2048; independent intended tile8,0 uses pipe1 byte256 (initial byte512 selected tile0,16). Corrected INPUT descriptors/nibble, not expected desired tile. ReadMemory(B) correctly retained separate I; actual guest image_load probes replaced wrong observation method. First128 GPU assertions passed but final old counter expected16 instead of filtered8; final matrix rerun after counter fix/new production guard transfer. Failures remain failures; no expected data weakened to actual output. Accepted validation has no ERROR/CORRUPTION; informational1328/820/821 and known nonfatal ownership warnings stay in logs. No fault-injected unwind/concurrent remap/full game claim.

### Recovery: committed clean source and immutable core

Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached EXACT e1eedf9, clean source, Release22.900s,54 locked dependencies,2283 compile inputs,ZERO primary-checkout paths. H/RELEASE_PROOF_e1eedf9.json / raw command-20261018-040626-56248.log bind compiler/dependencies/generated shaders/runtime. All12 production/build/fixture files committed; guest_bc_decoder.h/.cpp and guest_tile_address.h remain tracked and included in clean build. Clean generated host shaders match fixture shaders after canonical LF.

Manual18-file **H/artifacts-e1eedf9**, EXE **9ca2c5a98c9bbfc98ea75ddaced9f5361da9fa4b91feb2e6d961fd918fe92609**, PDB **a89f9a1df944fb972d3e020ad402e1c08be2a9e1bfc24c69bde49936122f9cdd**. Runtime/game/FPS of THIS core NOT_RUN; compile/link is not GPU/core correctness. New kernels manual-folder only, no automatic app install/select. Native12-file runtime/PDB, GCN4 source-equal manifests, related executables/PDB/logs, precommit patch/full source, failed attempts/debug context/control identities all preserved in H. Earlier working DX12/Vulkan/DLSS/HUD/monitor/XInput/profile/save/PDB paths remain in historical sections and unchanged. Unknown .serena/ and CLAUDE.md preserved untracked. No user processes stopped. Check END_STATE.json for final docs HEAD/upstream/process status.

### G/H/I. Exact continuation and prohibited repeats

First: git -C F:/PS4Dev/work/shadps4-dx12-src status --short; git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline. Read this FIRST section and H/NEXT_NATIVE_FORMAT_CODE.md. Continue exact source native packing in RetileImageVersion / GuestTiler::Prepare / src/video_core/renderer_d3d12/guest_tiling.hlsl before admitting other original-capable packed classes. Keep target geometry/strides but SOURCE native decode/packing semantics. Preserve new explicit old-guard transfer and original PS-before-VS binding/current-I ownership constraints. Use saved Vulkan/DX12 measurements; do not rerun original crash/129 audit/completed readonly/DMA/disjoint/budget studies or unchanged broad matrices without new cause. Build/test each completed mechanism with real GPU/readback and negative oracles, commit/clean Release/normal push/manual versions. Final suite and minimal game/XInput/image/input/stability/completed guest FPS versus saved Vulkan baseline excluding FG stay final; full port NOT COMPLETE.


## Authoritative cached depth-to-storage checkpoint - 2026-10-18

Production/source package **641f6b9c057d23c489f0bda2cc52d046d72fa3c9**, fix/mixed-cmask, pushed normally. Full original Vulkan-to-DX12 goal ACTIVE/INCOMPLETE. This FIRST section is authoritative; earlier sections historical. Narrow compile/regression/real GPU/readback allowed. Full suite/expanded game stays final. No game/UI/XInput/profiler/automatic app install/select/user-process/HUD action; original Vulkan/saves unchanged.

### A/B/C. Original contract -> production implementation -> current work

Pinned e0a46675 FindTexture(Storage) -> ResolveDepthOverlap / CopyColorAndDepth preserves OLD initialized native D even after CPU/B Dirty, excludes S, copies original min mip/layer intersection with original native strides, retains target-tail B seed and original GpuModified0/1/meta/lifetime. Original BindResources enumerates GetStages in SwStage order: FRAGMENT BEFORE VERTEX, each buffers then images. This is NOT GPU execution order. Exact original source/range/hashes in H/UPSTREAM_STAGE_BIND_ORDER.json and prior overlap/copy JSON. Do not reorder stages or repeat129 audit.

H=**F:/PS4Dev/handoff/depth-writable-20261018**. IsCachedDepthRecreationSource now admits legal direct fixed-base1x format2/4 writable color destinations; CaptureCachedDepthRecreationVersion / RecreateCachedDepthAtBind explicitly supply writable snapshot semantics, still validating real independent ownership/format. ResolveDepthViewStorage and shader storage planning retain original full lexicographic resource extent for writable views. Old native words are copied rather than numerically cast or replaced by B; source S remains separate.

Generic CachedImageBindingPlan / PlanCachedImageBinding / BindPlannedImage replace depth-only names. Source selection includes current independent I and cached native history with correct priority. Reserve raw D/S capacity only, pin allocation, retain only already CURRENT OWNED valid I before buffer classification; HasCurrentNativeContents checks live UID/handle/ownership/logical/boundary state. HasValidLogicalImage alone is insufficient after CPU reconciliation. Actual bind reselects CURRENT I/B and initializes/binds the real target, retiring original class/meta/ownership and consuming B receipts only then. Current-B refresh/native S/consumed HTILE/history semantics from d52 remain.

ExecuteGuestResources validates cached recreation ranges as GPU backing without treating planning as a CPU write, shares capacity plans with ordered stages and defers actual upload to image binding. PrepareOrderedGuestStages keeps the future domain distinct from original cached depth; a private original-I witness is consumed at the ORIGINAL earlier formatted buffer bind. It is captured only when a formatted descriptor precedes first image use, not for raw-only/later readers. Result resources/plans are retained through submitted fences. No skipped draw/dispatch, fake success, disabled assertion or game/address/hash workaround.

Headless fixture uses real PM4 SetShReg(offset0x200, complete ComputeProgram)/DispatchDirect through Liverpool Gfx queue +WaitGpuIdle. GetCsRegs now asserts active queue index instead of silently indexing -1. Production packet call sites use active queues. Other authored, unrun direct-compute fixture helpers that call GetCsRegs externally need the same queue integration before acceptance. Native fixture uses actual clipped compute Dispatch, not clear/CPU substitution. New gcn_cached_depth_storage_fixture.h is COMMITTED with all using source/build integration.

### D/E/F. Exact verification, failed experiments and scope

NATIVE_VERIFICATION_FINAL.json: exact native **fca02f5df87b3f6b6d8eaafdd7da17c78c998c388b3aea8d8f628e1ebe537c97**, own PDB/runtime, committed source equality:16 D16/D32 -> float16/32 UAV cases77.10s;8 late-native source cases42.37s; paired24 conversions/4 aspects/12 compact32.64s ALL PASS. Linear/tiled, original GpuModified0/1, original one-mip layer growth/full-parent view, optional source S, old native versus distinct CPU/GPU B, source-after-copy and target-before/after actual clipped Dispatch, independent typed/guest/capacity/padding, HTILE retirement/unchanged physical B and held-fence allocation/descriptor leases remain checked. Other numeric formats inside admitted data-format IDs2/4 are CONNECTED but not proven by float-only native tests; no whole-group claim.

GCN_VERIFICATION_FINAL.json: exact GCN **b3da2b74779e0bcab9bb91a6e5e701c1b1548d119b3ee92faabb401c906c0de7**, own PDB/runtime/source/compile+link responses:8 single-stage PM4 storage4.143s +8 original PS-buffer-before-VS-image graphics3.936s +4 prior depth caller4.654s ALL PASS. Real native clear makes I0.875 over physical B0.125. Clean-I formatted read obtains0.875 without publishing I into physical B; after real CPU dirtiness it obtains B0.25. Actual guest image_store writes I0.5 while next I texel remains0.875; a later RAW B/image probe distinguishes both. Optional source S/HTILE, full65536 physical bytes/padding and allocation retirement across eight cases are checked. Prior depth caller retains PS B0.25 versus old native D0.875. No validation ERROR/CORRUPTION; informational1328/821 and two known unaligned disjoint warmup ownership warnings remain. These elapsed times are fixture latency, NOT gameplay frame times/FPS.

Latest ordered-early-publication-control-final EXE/PDB/source deliberately binds future I in ONE alternate TU before original stage buffers: independent expected B0.25 instead observes0.875 at observation byte57346 (actual60 expected80), required failure. Primary sources/objects untouched. Final source and runtime identities archived. Seven B-envelope/preserved-version/DMA/lifetime checks at previous d52 retain THEIR own identities; not rerun/transferred as acceptance of this new core. Current related paired and GCN scopes are explicit; no unchanged huge matrix merely for metadata.

FAILURE_ANALYSIS.json preserves ALL earlier attempts/binaries/inputs/logs. uav-first exposed missing writable extent normalization (production fixed); uav-normalized fixture omitted required writable-alias retention callback (added existing geometry predicate plus real PageWatch acquisition; guard not disabled). storage-first used GetCsRegs outside active queue, overwriting prior Liverpool structures; actual second-chance c0000005 was captured in SlotVector<AscQueueInfo> destructor with stack/registers/dump at H/storage-debug-first. Loader breakpoint skipped; expected PageWatch first-chance AVs separated. Proper PM4 queue execution and active-index guard fix the harness, not an inferred GPU-warning cause.

The FIRST ordered test wrongly put VS read before PS image despite original PS-first binding. Value0.875 there was correct for original order, NOT a renderer bug. Exact pinned source confirms correction; revised test puts PS buffer before VS image and keeps independent0.25/0.5/0.875 expectations. Early clean tests had B==I0.875 and were too weak for clean separation; stronger INPUT physicalB0.125 (same expected formatted0.875) exposed loss of original I, fixed by GPU-only validation/early current-I retention and original witness. Related depth-caller-final then caught revival of unowned stale native under logical flags alone; HasCurrentNativeContents precondition fixes it, and final4+8+8 GCN/3 native passes apply only to corrected source. No expected output was changed merely to pass. No fault-injected exception-cleanup or broad concurrent remap/alias claim.

Original d01969e int3 remains independently localized/fixed935fb69: actual ExecuteGuestBufferCompute writable-B ownership contract failure -> Liverpool coroutine assertion. Original manifest/EXE/PDB/context/dump at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z; individual rejected guest range still unknown. No replay here and no automatic attribution to old overlap/missing-file warnings.

### Reproducible source/core, artifacts and recovery

Source commit includes all12 production/build/fixture files. Clean **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached EXACT 641f6b9c057d23c489f0bda2cc52d046d72fa3c9, Release25.788s:54 locked dependencies,2283 compile inputs, ZERO primary-checkout inputs, empty status. H/RELEASE_PROOF_641f6b9.json / raw command-20261018-033420-23656.log bind toolchain/runtime/source; generated host shaders match tested native after canonical LF.

Manual18-file **H/artifacts-641f6b9**, EXE **7dc8fa25071a839521d819698b6b94e24a152d2379f988301a9046215b6e25e4**, PDB **3d75dc45ae3479329ce503121e35020086351cebc64da0b8952c4b7361a0a28a**. Runtime/game/FPS for THIS rebuilt core NOT_RUN; build/syntax/link proof cannot establish GPU correctness. Native12-file runtime/PDB at H/native-fixtures; exact accepted GCN snapshots/runtime/PDB at storage-owned-fixed, ordered-owned-fixed, depth-caller-owned-fixed. NATIVE/GCN final manifests record session IDs/time/source equality/own hashes/absolute paths/scenario/limitations. Precommit patches/full source snapshots, source-641f6b9.patch, failures/debug dump/control sources preserved. Source commit pushed to origin/fix/mixed-cmask. Earlier manual cores/Vulkan/DLSS/HUD/monitor/XInput/PDB/profile/save locations remain in historical sections, unchanged. Unknown .serena/ and CLAUDE.md preserved untracked; prior separate-clone-only context exclusion unchanged. All owned build/test/debug commands terminal; user processes not stopped.

### G/H/I. Next CODE and exact continuation

129 implementation/behavior records remain independent; nine broad gap IDs remain partial. Direct16/32 cached depth->writable admission/caller integration is CONNECTED and narrowly checked; absence of final game test is not missing implementation. Broader converted/multi-view/shifted/geometry/MS/size/late-growth guards remain. NEXT **H/NEXT_NATIVE_DEPTH_COLOR_FORMATS.md** names real production gates: IsCachedDepthRecreationSource target formats only2/4, CanMapIndependentDepthImage growth format restrictions, ResolveDepthViewStorage early return for other16/32-bit classes. Extend exact original bit-compatible words/native capability mapping for other RG16/RGBA8/packed classes, not numerical casting. Then MaterializeColorClear(native_only) -> MaterializeOwnedColorClear pending CMASK decoder/component/ownership path, preserving copied I outside rectangles and current B. Full member CMASK runtime remains separate from native RT rectangle tests.

First command: git -C F:/PS4Dev/work/shadps4-dx12-src status --short and log -4 --oneline; read this FIRST section, H/NEXT_NATIVE_DEPTH_COLOR_FORMATS.md and named functions. Reuse saved Vulkan/DX12 metrics. No repeated129 audit/original crash/completed readonly-DMA/disjoint/budget studies/unchanged broad matrices without new reason. Preserve generic capacity plans, original PS-before-VS binding, strict old-I ownership, exact source/native/clean-core identities; normal verified commits/push/manual-folder versions. Full suite and minimal existing-XInput world/image/input/stability/completed guest FPS vs saved Vulkan baseline excluding FG remain final; full port NOT complete.

## Authoritative private depth binding checkpoint - 2026-10-18

Production/source package **d52ac76798704617d12ab7d6e28ac6625799474e**, branch fix/mixed-cmask, pushed normally. Full original Vulkan-to-DX12 goal ACTIVE/INCOMPLETE. This FIRST section is current; earlier sections historical. Narrow compile/GPU/readback regressions authorized; final suite and expanded game acceptance stay final. No game/UI/XInput/profiler/automatic kernel install or select/user-session/HUD action. Original Vulkan/saves untouched.

### A/B/C. Contract, production change and work state

Pinned e0a46675 ResolveDepthOverlap copies OLD initialized native D even when CPU/B made it Dirty. BindResources selects buffers/images in original order; early capacity reservation must not make a future I visible to preceding formatted buffer readers, publish new class/meta/ownership or semantic B residency. Original GpuModified0/1, S exclusion, target seed, min-mips1 aspect rule, lexicographic resource extent, metadata state and submitted allocation/descriptor lifetime remain separate contracts. Reuse saved UPSTREAM_DIRTY_RECREATION.json / UPSTREAM_COPY_INTERSECTION.json; no129 audit repeated.

GuestTextureCache::PlanCachedDepthBinding reserves raw D/S envelopes and pins source lifetime only. It copies NO native pixels and creates NO early Entry. BindPlannedDepth reselects CURRENT I/B at actual binding, including earlier native source writes and shader source-class replacement. Actual cached recreation is private RecreateCachedDepthAtBind. Original ordinary Upload preparation cannot accidentally publish cached recreation; current binding handles it. During same-class dirty refresh D comes from current B, valid native S is retained, consumed HTILE metadata ownership/mask and pending receipts move to the actual bound I, and B residency is consumed once. Replaced independent source is MarkLogicalDirty after retirement; its RHI/pins/fences remain alive.

GuestRasterizer::SetBackingAccess supplies nonpublishing capacity/current-B callbacks. Rasterizer::ValidatePreparedShaderBufferRange checks actual mapped extent before refreshing current CPU/store-marker data and capturing B inside the reserved envelope. DrawInternal skips early depth/meta/feedback publication when a plan exists, then binds the actual initialized DSV through BindPlannedDepth. Only this actual target receives later depth/stencil write history. Earlier proposal to pre-copy a private destination plus revision token is SUPERSEDED: there are no early copied pixels to validate. Larger source geometry arriving after reservation can still exceed the prepared envelope; do not relax the guard or claim complete late-growth coverage.

Headless actual-GCN fixture now establishes required ElfInfo metadata through production InitializeApp, also extracted into Emulator::Run without disabling assertions. Existing script links the real core objects with test-only main and now emits its own PDB. Only new planned-depth-recreation-owned mode is runtime accepted; other authored GCN modes remain separately prepared/unverified, including their own geometry problems.

### D/E/F. Proof, failures, measurements and limits

H=**F:/PS4Dev/handoff/private-depth-binding-20261018**. NATIVE_VERIFICATION_FINAL.json binds exact native **5d121ea72296b6a73652f3a7f08aeefa8ea63de5576422cf38ec6bcb84e37dbf** to committed linked sources and six GPU passes:8 late-native37.60s;8 late-B36.67s;16 dirty-depth72.26s;16 color-depth71.24s;16 original one-mip RT71.30s; paired24 conversions/4 aspects/12 compact29.38s. Typed/native/source/guest/full capacity/padding/S/HTILE/read receipts and held-fence leases pass. Late native mutation is real DSV clear1.0; late B source is actual GPU buffer data after intervening image-class replacement. Same-plan repeated bind performs no extra source read/publication. No expected-copy/negative oracle changed to make transfer pass.

**gcn-final/identity.json**, exact GCN **f9795ec2b10f1f09b7f9bcabaac63cf2dcd8fb34f740377d32d9b0fedeef6f40**, own PDB/runtime/compile+link responses/source snapshots: FOUR actual GCN draw cases4.617s. An old no-S native DSV contains0.875; CPU B contains0.25; adding S forces recreation. Interior VS buffer writer cannot mask DWORD0 through exact-base invalidation. Large formatted PS reader must see B0.25/red0x40, while DB tests consume old native0.875. Both small/full writer extents and HTILE off/on verify complete65536 bytes/padding. Separate gcn-early-publication-control intentionally binds early in ONE isolated alternate translation unit, leaving primary sources/objects untouched: fails first pixel at4096 actual0xdf expected0x40. Its EXE/PDB/source/log retained. This demonstrates actual wrong bytes under premature publication, not a game/FPS measurement.

buffer-related-final contains seven exact executable identities/CTest LastTest: scheduler lifetime, preserved B versions, DMA cache, pending fence, covered envelope, buffer view lifetime and ordinary independent image conversion ALL pass6.03s. The latter includes inherited deferred-residency once-only control. Relevant native targets were rebuilt4.545s; warnings retained. First wrapper attempt with pipe regex never ran tests; corrected with CTest --tests-from-file, not a changed expected result.

Failure history retained: late-native-first exposed genuinely clean retired source, fixed by MarkLogicalDirty. Later new D16 clear0.5 test assumed0x8000, but source and copied target both0x7fff; changed the NEW COMMAND to exact clear1.0 with mathematical endpoint0xffff/D32 bits3f800000. No normative half-rounding parity claim. FAILED_LATE_NATIVE_ANALYSIS.json and old binaries/oracles retained. Initial full-core GCN main lacked ElfInfo init and asserted; fixed required process metadata. Next fixture DB had pitch16,height16,slice128 pixels (invalid), corrected ONLY new mode height to8 matching declared slice, no production allocation guard removed. Other GCN prepared modes are not claimed runtime accepted. Negative-control compile first misplaced -I after --; response/log retained; isolated corrected build follows same compiler/ABI. No validation ERROR/CORRUPTION in accepted runs; informational1328/821 and disjoint unaligned warmup ownership warnings remain, not hidden or called original crash causes. Fault-injected cleanup and broad concurrent alias/remap domains not newly accepted.

Original d01969e crash remains independently localized by actual exception/context/dump: ExecuteGuestBufferCompute writable-B ownership failure -> Liverpool unhandled coroutine assertion int3, fixed935fb69. Evidence F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Not inferred from old SRV/UAV warning, missing files or exit code; original rejected individual guest range still unknown. No replay here.

### Reproducible code/core and recovery

Primary full Release compile/link18.295s. Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached EXACT d52ac76798704617d12ab7d6e28ac6625799474e, clean source, pinned54 dependencies: Release35.804s,2283 compile inputs, ZERO primary checkout paths. H/RELEASE_PROOF_d52ac76.json / command-20261018-025113-33356.log bind source/build/runtime/compiler and generated shaders equal tested native after canonical LF. Source-only/compile proof is not GPU/core/game correctness proof.

Manual-folder18-file **H/artifacts-d52ac76**, EXE **d03ed78f5bf95a993ed4ad14b6f35b912dd4af29c0df60be409541c7491955ba**, PDB **60c984c18dd8fb47fe364a882a14c7d88094d2e4dd15b9e09d4ac94000499709**. Exact rebuilt core runtime/game/FPS NOT_RUN; fixture results belong to their own binaries. Native12-file runtime/PDB/compiler/shaders at H/native-fixtures; GCN exact PDB/runtime under H/gcn-final. Precommit patches/full changed source snapshots, source-d52ac76.patch and per-run identities/logs preserved. Earlier01a1f6c/94ace23/working/Vulkan/DLSS/HUD/monitor/XInput/PDB/profile/save locations remain recorded in earlier sections and unchanged. Unknown preexisting .serena/ and CLAUDE.md preserved untracked. Context-only CLAUDE exclusion exists only in separate clone Git metadata, as already evidenced at prior H; no primary source ignored or overwritten. Owned commands exited; no user process stopped. Source commit pushed to origin/fix/mixed-cmask.

### G/H/I. Next exact mechanism, command and forbidden repeats

129 groups retain independent implementation-connected / behavior states, nine broad gap IDs remain partial. This depth mechanism is CONNECTED and narrowly GPU-checked, including actual shader bindings; absence of final game acceptance is not an absent implementation. Remaining code: cached native depth->shader-writable direct color, additional representations/native-only CMASK clear guards/geometry/spatial/rebased/clipped/ordered-MS/size and late prepared-envelope growth domains.

Next **H/NEXT_DEPTH_WRITABLE_BINDING.md** names IsCachedDepthRecreationSource color_target requiring render_target&&!writable; RecreateCachedDepthAtBind CanUploadGpuSnapshot without writable_snapshot and UploadVersion writable_snapshot=false; ExecuteGuestResources independent-prefix early Upload(Preparation) and actual descriptor-order image bind. Add legal UAV allocation/word-preserving old D transfer/source retirement and actual shader caller integration; do not simply relax guards. Add real clipped UAV dispatch/readback, old I versus B, S/padding/meta/GpuModified0/1/source replacement/fence negatives. Preserve current readonly parent reuse and private capacity plan.

First action: git -C F:/PS4Dev/work/shadps4-dx12-src status --short and log -4 --oneline, then named H note/functions. Use saved Vulkan/DX12 baseline; no repeated129 audit/d01969e replay/completed disjoint/readonly-DMA/budget studies/unchanged large matrices without new reason. Normal verified commits/push and clean manual-folder releases. Full final suite and minimal existing-XInput world/image/input/stability/completed guest FPS vs baseline excluding FG remain required. Goal stays ACTIVE.

## Authoritative cached depth-to-render-target checkpoint - 2026-10-18

Production **94ace23279c472f275c36d556afdf0313e1be444**; latest source/fixture package **01a1f6cea7a373b54b404f622ac93d6cf9cd3c26**, branch fix/mixed-cmask. Prior414094c/a1fb3d3 and exact runtime evidence preserved. Full original Vulkan-to-DX12 goal remains ACTIVE/INCOMPLETE. This FIRST section is current; older sections historical. Narrow builds/regression/GPU checks authorized; full suite/expanded gameplay final. No game/UI/XInput/profiler, automatic kernel install/select, original Vulkan/save or user-session/HUD action.

### A/B. Original contract -> connected production C++

Pinned e0a46675 ResolveDepthOverlap RenderTarget recreates old native depth as color even if old image is Dirty. Resources use lexicographic max(levels,layers). CopyColorAndDepth preserves logical depth WORDS/native contents rather than refreshing from new B; copy excludes S and inherits original GpuModified. Differing-aspect buffer copy asserts min-mips1. H=**F:/PS4Dev/handoff/cached-depth-rt-20261017** preserves UPSTREAM_DIRTY_RECREATION.json / UPSTREAM_COPY_INTERSECTION.json exact source ranges/hashes, not a repeated129 audit.

IsCachedDepthRecreationSource now admits direct fixed-base1x D16/D32 -> format2/4 native render targets with existing exact mapping/UID/class/native allocation guards; writable targets remain explicitly unsupported. ResolveDepthViewStorage now promotes RT storage to original full resource extent while retaining requested view selectors. HasCurrentNativeContents validates cache UID/initialized or logical class/texture/ownership/Dirty/boundaries; HasCurrentDepthContents additionally checks depth storage. Ordinary stale independent-I reads remain guarded.

DrawInternal normalizes RT storage after MergeGuestColorAttachmentLayers and BEFORE memory envelopes/prewrite inputs. It does NOT prepare/publish the future RT image before shader resources. Actual ordinary Upload in submit_replay performs the cached native copy at original RT binding. Pending CMASK for a depth-source RT is deferred until after actual native copy; initialized independent RT ownership is required for native clear, preserving old I outside clear rectangles rather than replacing it with B. MaterializeOwnedColorClear admits a valid independent native color owner through HasCurrentNativeContents and marks real GPU writes. DrawInternal marks actual recorded RT writers (and ordinary non-private depth writers) GpuModified, including a copied original GpuModified0 source. No skipped draw/dispatch, assertion suppression, fake success or game/address/hash workaround.

### D/E/F. Exact GPU evidence / limits

NATIVE_VERIFICATION_FINAL.json binds FOUR passes and canonical-LF source equality to exact native EXE **12221e6ee0e384c7d7c33fcf2035a4dd482b9bd776a6c5c8afbf63018acf53da**: rt-first69.44s; depth-related71.69s; color-related71.71s; paired-related29.53s.16 NEW RT cases cover D16/D32, linear/tiled, original GpuModified0/1, optional source S, same-size and lexicographically crossed resource growth/smaller request. Actual PageWatch CPU store makes B distinct from old native I. They bind through ordinary Cache.Upload (no early Prepare), independently read back the source AFTER native transfer and target BEFORE draw, then issue a real clipped graphics DrawInstanced and clipped ClearRenderTargetView on layer0. Independent typed R16_FLOAT/R32_FLOAT pixels and complete guest bytes verify old native words outside writes, target-only B mip seeds, native source/S immutability, padding/full input capacities, HTILE source-owner retirement, true post-draw GpuModified0->1, unchanged physical B and externally held-fence leases. No native validation ERROR/CORRUPTION; informational warnings remain.

FINAL ColorBuffer-shaped fixture correction: pinned ImageInfo(ColorBuffer) inherits resources.levels1 and sets only resources.layers=NumSlices. H/UPSTREAM_CB_RESOURCE_SHAPE.json saves exact constructor/default sources/hashes. The initial rt-first16 includes8 extra-target-mip helper cases: valid native copy/draw/clear evidence, NOT demonstrated original ColorBuffer call applicability. No prior result was relabeled. Fixture-only01a1f6c replaces RT growth with one-mip layer growth1->3 and full-parent2-layer reuse from smaller request; all other depth/color pixel expectations remain unchanged. NATIVE_VERIFICATION_CB_LAYERS_FINAL.json binds16 final RT cases73.53s (measured2026-10-17) to DISTINCT native **ab7a2c334fa6829119ed9a119ef413fbcfc363151578e33840e4713bf6225d0a**, with source snapshots equal committed01a1f6c after canonical LF. Source/depth/S/target-only B layers, actual clipped RT draw/clear, typed/native/guest/padding/capacity/metadata-owner/held-fence oracles pass. Initial four checks at94ace23/native12221e6e stay separate; they are not transferred to this binary or clean whole core.

IMPORTANT SCOPE: clipped ClearRenderTargetView is a real native-clear CONTROL, not execution of GuestRasterizer::MaterializeColorClear/PlanGuestColorClear/decoder/metadata-consumption. Core caller integration and native-only CMASK path are connected and compile/link checked, but their runtime acceptance is NOT_RUN. The native_only failure guard still exposes unsupported pending plan/format/ownership combinations requiring real code completion. Do NOT close CMASK or renderer groups from this fixture. No whole GuestRasterizer/core/game/world/image/input/stability/FPS acceptance is claimed. Related16 dirty depth +16 color->depth +24 paired captures/conversions/4 aspects/12 compact checks retain independent values. Prior other fixtures keep their own exact identities, not transferred here.

### New explicit ordering issue: depth preparation is not yet private

Current PrepareCachedDepthRecreation(UploadPurpose::Preparation) still acquires new ownership, unregisters source metadata/class and publishes a clean new logical image. DrawInternal calls it for DEPTH before original shader buffer/image binding. Existing dirty-depth fixture confirms that early metadata/ownership mutation; for old GpuModified1+Dirty it can expose a future clean independent source to earlier formatted buffers. This is confirmed CODE/fixture timing; exact resulting ordered-buffer wrong bytes/game/FPS impact has not been measured. RT avoids this early publication by copying at actual bind. Do not claim the old depth preparation has correct original bind-order semantics just because its copied pixels pass. Full details/precise next implementation are H/NEXT_PRIVATE_RECREATION_PUBLICATION.md.

Previous S-address-only helper applicability correction, paired priority/validation921 exception cleanup, unproven resource attribution and no fault-injected unwind acceptance remain at prior handoff paths. Original d01969e actual exception/context/dump remains F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z: writable buffer ownership violation -> Liverpool unhandled coroutine assertion int3;935fb69 restores ownership after prewrite/Finish. No replay or new cause inferred from warnings/exit code. Original individual rejected guest range remains unestablished from that dump.

### Reproducible core / preservation

Primary full core compile/link10.756s. Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached exactly01a1f6cea7a373b54b404f622ac93d6cf9cd3c26, compiled/linked Release10.46s on2026-10-18:54 pinned deps/2283 compile inputs/zero primary-checkout source paths/empty status. H/RELEASE_PROOF_01a1f6c.json and raw command-20261018-021424-22452.log bind real inputs/compiler/runtime. Generated host shaders equal the tested native headers after canonical LF.

Manual-folder18-file core **H/artifacts-01a1f6c**, EXE **a9e588bccdad9aa30106b16aa4c1ee01803903b74d8697f14519486a9f613634**, PDB **0fd819e8d1afc1f8f4d308413eb3400e597dab7a6d9de685ddb8cc67c96adde6**. Core runtime/game NOT_RUN; no automatic app install/select.Final12-file tested native runtime at H/native-fixtures-cb-layers / NATIVE_RUNTIME_IDENTITY_CB_LAYERS.json; initial12-file native runtime remains H/native-fixtures-rt / NATIVE_RUNTIME_IDENTITY.json, initial clean24.535s core remains H/artifacts-94ace23 with its own RELEASE_PROOF_94ace23.json. Unknown untracked CLAUDE.md appeared overnight in both checkouts; originals preserved. Separate validation clone excludes only /CLAUDE.md through its own Git metadata, with copied file/hash/previous exclude in LOCAL_VALIDATION_CONTEXT_EXCLUSION.json; it is not a build input. A stale94ace23 rebuild after the initial checkout assertion failed is not counted as01a1f6c verification (raw command-20261018-021318-52396.log). Start/post-source staged/unstaged patches, exact per-test sources/EXE/PDB/LastTest/wrapper logs preserved. All seven production/build/fixture files committed; preexisting .serena/ and externally created CLAUDE.md excluded from task commits. Previous working cores/Vulkan/DLSS/HUD/live monitor/XInput/debug/test profiles/saves remain at recorded paths untouched. Owned build/test commands exited; no user process stopped.

### C/G/H/I. Next concrete implementation / avoid repeats

129 independent implementation/behavior records and nine broader gap IDs remain partial. Next FIRST fix private cached DEPTH preparation per H/NEXT_PRIVATE_RECREATION_PUBLICATION.md, not another129 audit. guest_texture_cache.h/.cpp needs an initialized private recreation receipt retaining old source UID/class/pin/native revision and target seeds, without changing ordinary I validity/ownership/class/metadata until actual bind. Explicit private prewrite capture may read the initialized preparation; ordinary stale I may not. Actual bind must consume receipt/deferred B effects exactly once and publish ownership/metadata/class on the ACTUAL bound allocation. DrawInternal depth_attachment_snapshot may bind a different DSV from captured D/S planes; publishing the untouched preparation while that DSV is written would leave stale native contents. Attach producer-proven captured inputs to actual bound I, preserve opposite-plane writes/flags, validate source replacement/native writes and abandonment/error/fence lifetime. HTILE prewrite lookup also must not RegisterMeta early. New before-bind buffer/metadata and after-bind target native GPU oracles are required.

Then continue explicit writable destination legality/ownership/Upload flags and actual shader binding order in guest_compute.cpp / guest_stage_bindings.cpp, remaining RT/source formats beyond direct16/32 mapping and guarded CMASK combinations, geometry/spatial/tile/rebased/clipped/ordered/MS/64-MiB paths. H/NEXT_DEPTH_WRITABLE_RT.md retains earlier concrete gates; RT subset is now implemented, writable is not. No generic RHI-only test substitutes for actual stage/member runtime coverage.

First commands: git -C F:/PS4Dev/work/shadps4-dx12-src status --short and log -4 --oneline; then this FIRST section, H/NEXT_PRIVATE_RECREATION_PUBLICATION.md and named functions. Reuse exact current/prior Vulkan/DX12 evidence. No repeat d01969e,129/completed readonly-DMA/disjoint/budget studies or unchanged large matrices without code/evidence cause. Narrow-check completed mechanisms, clean committed Release/manual-folder versions/exact identities/ordinary push. Full final suite and minimal existing-XInput world/image/input/stability/completed guest FPS versus saved baseline excluding FG remain required.


## Authoritative cached color-to-depth checkpoint - 2026-10-17

Production/source **414094c4eef296b61aeaf6aad79382dc7726df05**, branch fix/mixed-cmask. Previous depth recreation1e2fae2/checkpoint e698d97 preserved. Full original Vulkan-to-DX12 goal remains ACTIVE/INCOMPLETE. This FIRST section is current; older sections are historical. Builds/narrow regression/GPU checks authorized; full suite/expanded game acceptance remains final. No game/UI/XInput/profiler, automatic core install/select, original Vulkan/save or user-session/HUD action.

### A/B. Original contract -> connected C++ implementation

Pinned e0a46675 ResolveDepthOverlap DepthTarget/Texture binding recreates initialized cached color as depth despite old CPU/B Dirty. CopyColorAndDepth reads OLD native source, optionally initializes growing destination, excludes S and inherits original GpuModified. Differing-aspect buffer copy asserts min-mips1. H=**F:/PS4Dev/handoff/dirty-cached-color-depth-20261017** contains original UPSTREAM_DIRTY_RECREATION.json and UPSTREAM_COPY_INTERSECTION.json exact source/range/hash snapshots; no new129 audit.

Entry::cached_native_current / RememberNativeImageStorage replace depth-only depth_storage_current. Every actually refreshed/owned native allocation, including CPU-initialized color, has a cached class/identity; true replacement clears the old class, CPU dirtying does not. Private snapshots cannot replace it. It NEVER certifies logical current-I validity. IsCachedDepthRecreationSource now admits existing direct fixed-base1x format2/4 color->depth mapping, alongside prior depth->depth. Actual target remains supported D16/D32; BC/expanded/stencil-color source representations stay rejected. Color->depth or S-presence changes require original min-mips1. S-address-only rebasing alone remains insufficient.

CaptureCachedDepthRecreationVersion / RetileImageVersion read old color-native footprints ONLY under explicit cached recreation purpose plus live UID/class/texture/mapping/size/pin checks. Ordinary native color CaptureDepthPlane and stale independent-I guards remain strict. PrepareCachedDepthRecreation preserves original GpuModified0/1, lexicographic destination extent/current B seed/new S, HTILE0/new UID/source metadata retirement, current-independent source priority and fences. Existing GuestRasterizer BEFORE-prewrite depth preparation covers the new source. No fake success, skipped operation, assertion suppression or address/game/hash workaround.

### D/E/F. Exact executed GPU verification / limits

NATIVE_VERIFICATION_FINAL.json: FIVE passed checks on exact native **1525a2094a1b6722129bbf97e036b1f30136831d7d371511e088a00058fe8f30**, with per-test precommit/committed source equality verified after canonical LF: color-first73.06s; depth-related70.15s; paired-related30.18s; subviews-related29.55s; ordinary-related3.84s. Sixteen NEW cases use R16_UINT/R32_UINT native render-target sources, linear/tiled, original GpuModified0/1 and aspect/resource growth. ALL16 color destinations have stencil; optional NoS color->depth cases are not claimed verified. Real PageWatch CPU stores make current B different from old native source. Independent typed target depth pixels, post-transfer source color values, guest/capacity/padding/new S, target-only B mips, private prewrite after later GPU B mutation, CMask source unregister/new HTILE0, stale/private-source/input negatives and held-fence leases pass. GPU fixture source/upload buffers now have submission-owned lifetime on unwind; no fault-injected cleanup acceptance is claimed.

Related checks retain16 dirty depth cases,24 paired captures/conversions/4 depth-color-depth aspects/12 compact downloads,8 full-parent subviews and ordinary GPU-written color->different sampled-format conversion with unchanged physical B/padding. Independent expected data was not changed to make color conversion pass. No native validation ERROR/CORRUPTION; informational warnings remain. Whole-core runtime/GuestRasterizer/game/image/input/stability/FPS/priority acceptance remains NOT_RUN for this exact core. Prior envelope/residency/DMA/lifetime tests retain their own recorded binaries; results are not transferred to this rebuilt core.

Previous S-address-only helper applicability correction and paired cached-vs-independent priority failure/validation921 cleanup/device removal remain at previous H/SCOPE_CORRECTIONS.json / PAIRED_FAILURE_CAUSE.json; exact offending resource attribution and fault-injected unwind acceptance remain unproven. No replay now. Original d01969e crash has actual exception/context/dump: ExecuteGuestBufferCompute writable-buffer ownership failure -> Liverpool unhandled coroutine assertion int3.935fb69 restores B ownership after prewrite/Finish. Evidence F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z, not inferred from SRV/UAV warnings/missing files/exit code. Original rejected individual guest range was not established from that dump.

### Reproducible core / preservation

Primary full core compile/link14.428s. Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached exactly414094c4eef296b61aeaf6aad79382dc7726df05, compiled/linked Release23.796s:54 pinned deps/2283 inputs/zero primary-checkout source paths/empty status. H/RELEASE_PROOF_414094c.json and raw command-20261017-114330-45936.log bind actual inputs/runtime/compiler; generated shaders equal tested native headers after canonical LF.

Manual-folder18-file core **H/artifacts-414094c**, EXE **727ba60e14f2c3374438fb790de67d0cf852e744cf91a94b5864978127cc328c**, PDB **aded9c7ca2ad269c62e4b1a9e5df02d1fd4258bc37478ce441980bc12159a985**. Runtime/game acceptance NOT transferred from fixture; no auto install/select.12-file tested native runtime/PDB/compiler/shaders at H/native-fixtures-color / NATIVE_RUNTIME_IDENTITY.json. Start/post-source patches, per-test source/EXE/PDB/LastTest/wrapper logs preserved; all five production/build/fixture files committed. Preexisting .serena/ excluded. Earlier1e2fae2/working cores/Vulkan/DLSS/HUD/monitor/XInput/PDB/test profiles/saves remain untouched at prior recorded paths. Owned build/test commands exited; no user process stopped.

### C/G/H/I. Next concrete production gap / continuation

129 independent implementation/behavior records and nine broader gap IDs remain partial. Dirty direct fixed-base1x color/depth -> depth is CONNECTED and narrowly GPU-checked; absence of final game acceptance does not make this code absent. Remaining CODE: cached depth->shader-writable/render-target recreation, source representations beyond direct16/32 words, geometry/spatial/tile/rebased/clipped mappings, remaining ordered/MS alias gates and64-MiB guards.

Next follow **H/NEXT_DEPTH_WRITABLE_RT.md**. guest_texture_cache.cpp/.h: IsCachedDepthRecreationSource requires a DEPTH destination; PrepareCachedDepthRecreation uses CanUploadGpuSnapshot without writable_snapshot and private UploadVersion with writable_snapshot=false. These are actual gates for original Storage/RenderTarget conversion. Keep readonly sampled color using depth-parent reuse rather than forcing recreation. Destination needs legal UAV/RT format, actual ownership, old native D versus current B, original flags/meta/source allocation/fence lifetime. guest_rasterizer.cpp BEFORE-prewrite preparation currently covers descriptions[8] depth ONLY; color-attachment/shader-writable integration must preserve original bind/read/write order. Cache-only changes are not complete caller integration.

Add actual writable dispatch and RT draw/native readback, real CPU dirtying, independent old I/B expectations, S exclusion/padding, stale/private/mapping/min-mip negatives, held-fence and ordered prewrite checks. Use captured original binding/copy contracts; no broad audit or new requirements from unsupported branches. First commands: git -C F:/PS4Dev/work/shadps4-dx12-src status --short and log -4 --oneline, then this FIRST section, H/NEXT_DEPTH_WRITABLE_RT.md and named functions. Reuse Vulkan/DX12 baselines; no d01969e replay/129 audit/completed readonly-DMA/disjoint/budget studies/unchanged large matrices without reason. Narrow-check each finished mechanism, clean committed Release/manual-folder versions/exact identities/normal push. Full final suite and minimal existing-XInput world/image/input/stability/completed guest FPS-vs-baseline excluding FG remain required.


## Authoritative dirty cached-depth recreation checkpoint - 2026-10-17

Production/source package **1e2fae2cdf7bc5d2ea3b8c6d2c1bb325516a15fe**, branch fix/mixed-cmask. Full original Vulkan-to-DX12 goal remains ACTIVE/INCOMPLETE. Builds and narrow regression/GPU checks authorized; full suite/expanded gameplay remain final. This FIRST section is current; older sections are history. No game/UI/XInput/profiler, automatic kernel install/select, original Vulkan/save or user-session/HUD action this continuation.

### A/B. Original contract -> connected production implementation

Pinned original e0a46675 FindImage -> ResolveDepthOverlap copies OLD initialized native contents without refreshing the old dirty image. New single-sample growing storage may be refreshed first; new Dirty is cleared, HTILE initial mask0, native copy follows, then old image is freed. CopyImage excludes S; differing-aspect CopyImageWithBuffer requires min-mips1. H=**F:/PS4Dev/handoff/dirty-cached-depth-recreation-20261017** preserves UPSTREAM_DIRTY_RECREATION.json and UPSTREAM_COPY_INTERSECTION.json with original source hashes/ranges.

GuestTextureCache::IsCachedDepthRecreationSource / FindCachedDepthRecreationSource now identify retained initialized fixed-base D16/D32 single-sample native allocations separately from valid independent I. PrepareCachedDepthRecreation / CaptureCachedDepthRecreationVersion preserve old native D over the copied intersection, current target B tail/padding and separate new target S seed. Original GpuModified0/1 is preserved; old dirty I is never made ordinarily readable. Private snapshots cannot overwrite cached native class identity; fresh recreation has a fresh image UID/HTILE0 and unregisters old metadata, retaining actual native allocation/descriptor lifetime through cache pins/fences. Current independent I has precedence over older cached history. Upload validates GPU snapshot flags/states and full CPU spans BEFORE the cached fast path. HasCurrentDepthContents admits an actually initialized owned clean result even with original GpuModified0; ordinary stale independent lookup/capture guards are unchanged.

ResolveDepthViewStorage preserves original lexicographic max mip/layer resources for this cached source. GuestRasterizer::DrawInternal performs recreation before shader-buffer/prewrite captures and pins the prepared result. Real PreparePhysicalDmaBuffers/CapturePreparedShaderBuffer seeds retain the original additional growing-B residency effect for actual binding only. No assertion suppression, skipped draw/dispatch, fake success or game/address/hash exception.

### D/E/F. Evidence, failures and precise limits

NATIVE_VERIFICATION_FINAL.json: exact native EXE **ddb0576152e5656eb3ac4eb0f7112936b291092fe3a9475f9542b538c65041b6**, committed source equality verified after canonical LF. native_dirty_cached_depth_recreation passed **68.72s**:16 D16/D32, linear/tiled, original GpuModified0/1 cases split between TRUE single-mip NoS->S recreation and same-aspect lexicographic resource growth. Real PageWatch CPU stores make B differ from old native I. Independent typed target pixels, post-copy old-native readback, full guest data/padding/buffer capacities, new S, HTILE0/new owner, current-B mutation after prewrite capture, physical B nonpublication and held-fence leases all pass. Stale independent I/private native source/incompatible geometry/same-resource refresh/malformed Upload are negative cases. native_paired_depth_gpu_snapshot passed **30.01s**:24 captures/24 conversions/4 depth-color-depth transitions/12 compact readbacks, unchanged expected values. Debug validation has no ERROR/CORRUPTION on these final runs; informational warnings remain. Twelve native runtime/compiler/shader/PDB files and each exact source snapshot/log/binary are preserved.

Earlier distinct native27e78f4b passed dirty65.11s, paired29.51s, crossed59.10s, subviews29.31s, color-MS7.49s; NATIVE_VERIFICATION_RELATED.json retains their OWN identities. Earlier dirty non-growth tests used same-presence S-address rebasing: they prove native helper behavior, NOT original recreation applicability. SCOPE_CORRECTIONS.json preserves that limitation; final fixture uses a true aspect addition. Original differing-aspect min-mips>1 and equal-MS overlap-unimplemented branches remain limitations, not newly invented port requirements.

During paired-related, cached-source fallback incorrectly preceded a current independent source and threw. Fixed source precedence; preserved failure logs and PAIRED_FAILURE_CAUSE.json. Cleanup then reported validation921/resource deletion and device removal887a0006. Paired color_native/native_return/compact readbacks now have submission-owned shared lifetimes. Exact offending resource addresses were NOT conclusively mapped; no fault-injected exception-unwind GPU acceptance is claimed. One attempted CTest name matched no tests and returned8; paired-lifetimes is NOT a pass. Correct exact test name subsequently ran and passed. No game or user process/driver-reset action was performed.

Original d01969e game0x80000003 cause was already captured: ExecuteGuestBufferCompute threw "Independent image version lost its writable buffer ownership before execution", then Liverpool unhandled coroutine reached assertion int3. Evidence/actual context/dump at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z;935fb69 restores writable B ownership after prewrite/Finish. This was not inferred from exit code, old SRV/UAV warning or missing file. No replay now. Exact individual rejected guest range was not established by the original dump. Full GuestRasterizer runtime, Bloodborne world/image/input/stability/FPS/priority acceptance remain not_run for this core. Narrow fixture proof is separate from whole-core compiler proof.

### Reproducible core / saved work

Primary incremental full core compile/link8.139s. Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached exactly1e2fae2cdf7bc5d2ea3b8c6d2c1bb325516a15fe, compiled/linked Release **23.484s**,54 pinned dependencies/2283 compile inputs/zero primary-checkout paths/empty status. H/RELEASE_PROOF_1e2fae2.json and raw command-20261017-113710-68200.log bind exact build inputs/runtime identities. Generated host shaders equal tested native shaders after canonical LF.

Manual-folder18-file core **H/artifacts-1e2fae2**, EXE **a5ceec4a840215d614ee38b4ca62837d0eb992db8cdad38b18df994938e8cfc9**, PDB **5c3dbe58a1fd3cb9d894863cf92ffd0abba3af41b52f7589de290d36f39bea34**. Runtime/game acceptance NOT transferred from the different fixture exe. Existing catalog versions remain intact; no automatic app installation/selection. H/native-fixtures-applicable and NATIVE_RUNTIME_IDENTITY.json retain the12-file tested runtime. Start/post-source patches and intermediate source snapshots/executables/logs preserved. All seven source/build/fixture files committed; .serena/ is excluded preexisting navigation state. Previous Vulkan/DLSS/HUD/live monitor/virtual XInput/PDB/test profiles/saves remain at the older recorded paths, untouched.

### C/G/H/I. Next concrete code / commands / avoid repetition

Connected and narrowly verified: cached DIRTY fixed-base single-sample D16/D32 -> depth recreation with real source class, original aspect/mip restrictions, full target seeds and source/fence lifetime. Remaining CODE: initialized cached COLOR -> depth and depth -> shader-writable/render-target recreation (current depth_storage_current tracks only depth; RetileImageVersion cached producer currently requires depth source/target); changed geometry/spatial/tile/rebased/clipped mappings; broader source representations, remaining ordered/MS gates and64-MiB guards.129 implementation/behavior records and nine broader gap IDs remain partial; no whole-group closure and no new129 audit.

Continue next with initialized cached color -> depth in src/video_core/renderer_d3d12/guest_texture_cache.h/.cpp: replace depth-only historical witness with an explicit initialized cached native class/identity, invalidate on true cache replacement, retain dirty native contents without claiming current ownership, generalize the distinct recreation producer only for existing CanMapIndependentDepthImage supported fixed-base direct word domain. Preserve current-independent precedence and BEFORE-prewrite GuestRasterizer integration; extend native-port/dirty_cached_depth_fixture.h with actual old-native color versus new B, typed native depth readback and source/metadata/pin/fence negatives. Use the already saved original ResolveDepthOverlap binding branches and CopyColorAndDepth contracts; do not redo broad audits. Depth -> writable/RT follows with explicit original binding semantics; do not silently broaden guards.

First commands: git -C F:/PS4Dev/work/shadps4-dx12-src status --short; git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline. Then read this section, H/NEXT_CACHED_COLOR_DEPTH.md, H/UPSTREAM_DIRTY_RECREATION.json and the named functions. Reuse already captured Vulkan/DX12 baselines. Do not replay d01969e, repeat129 audit, completed readonly-DMA/disjoint/budget studies or unchanged matrices without a code/evidence reason. Full suite and minimum necessary final existing-XInput world/image/input/stability/completed-frame FPS versus baseline, excluding FG, remain required at the final stage. Keep exact identities and normal verified pushes/manual-folder cores.


## Authoritative crossed-depth-resource checkpoint - 2026-10-17

Production **1a35c8b21e27fbdaa751ee276d55d11f9fa267ab**; latest source package **a09007a030dab4220ff79c6ce5db0d5a27fcb003** strengthens ONLY the source-readback fixture. Branch fix/mixed-cmask. Full original Vulkan-to-DX12 port remains ACTIVE/INCOMPLETE. Narrow compiler/regression/GPU/readback checks authorized; full suite and expanded gameplay remain final. No game/UI/controller/profiler, app kernel install/select, original Vulkan/save, HUD/monitor or user-session action. This FIRST section is current; older sections are historical.

### A/B. Original contract -> production implementation

Pinned e0a46675 Runtime::CopyImage copies min mip counts and min2D-array layer counts, using source native dimensions and excluding S. CopyImageWithBuffer also clips layer counts but asserts min-mips1. Original ResolveDepthOverlap selects lexicographic max resources before this copy. H/UPSTREAM_COPY_INTERSECTION.json saves exact source ranges/hash/branch limits.

CanMapIndependentDepthImage now admits nonempty compatible fixed-base mip/layer intersections instead of requiring both target counts >= source. Original geometry/word/sample/format mapping, boundary, independent ownership and live-resource guards remain. RetileImageVersion bounds source-native footprint allocation/copies/tiler dispatches by min(source,target) mips/layers, retaining ORIGINAL source subresource stride and TARGET guest offsets/layout. Target-only mips and padding retain their initialization seed; S is not imported by overlap copy. No skipped operation, fake success, assertion suppression or game/address/hash bypass.

### E/F. Executed GPU checks / scope corrections

H=**F:/PS4Dev/handoff/depth-crossed-resources-20261017**. Initial NATIVE_VERIFICATION.json records four passes on native **c0fa2cc263a4cc8d5e5a607f6fad94eeaf9a736e80d35577de88890231a89b5e**: crossed58.63s; growth58.27s; subviews29.43s; color/MS7.48s. CORRECTION: that fixture recorded source-native readback BEFORE the transfer; those runs verify initial source values, target results, raw GPU input immutability/receipts/guards/fences but do NOT independently prove source-texture immutability after transfer. Original metadata/raw logs/source snapshots/binaries remain preserved, with an explicit scope annotation and untouched original metadata copy.

Fixture-only a09007a pins the actual old allocation and records its full native D/S readback AFTER conversion/upload. No expected pixel, seed, S, padding, ownership or fence data changed. NATIVE_VERIFICATION_FINAL.json records two targeted repeated passes on DIFFERENT native **ec70028a12786a11d3c87e65f4f0513aaabc74a9d55613738f4b73882cc703ac**: crossed58.16s and growth58.39s. Each has16 cases, independent typed target-pixel oracle, full source-native values after transfer, dropped source layers, target-only B1/S/padding, input buffer capacity, real PageWatch and held-fence leases. Eight reverse requests prove lexicographic parent reuse without independently maximizing axes. All16 NEW crossed cases are single-sample paired SAME-ASPECT CopyImage paths. Width mismatch is rejected before capture. Native validation has no error/corruption; informational warnings remain. No earlier pass is transferred to a rebuilt executable or the clean whole core.

ORIGINAL APPLICABILITY LIMIT: pinned CopyColorAndDepth equal-MS source/destination branch logs unimplemented and performs no overlap copy; multi-mip NoS->S would hit CopyImageWithBuffer min-mips1 assertion. Existing native MS growth and prior2ac536b multi-mip aspect/helper cases prove actual native content/lifetime/seed behavior, NOT an implemented original Vulkan overlap path. Preserve useful native helper evidence but do not claim upstream overlap parity there or invent new requirements from original unimplemented branches. Whole-core/game/image/control/stability/FPS/priority-worker acceptance remains not_run.

### Reproducible core / preservation

Primary full core compile/link11.834s. Clean committed1a35c8b Release18.047s preserved at H/artifacts-1a35c8b with its own identity. Latest packagea09007a separately compiled/linked clean Release14.068s; same production, strengthened fixture package. Separate checkout **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016** detached exactlya09007a:54 pinned deps/2283 inputs/zero primary paths/empty status. H/RELEASE_PROOF_a09007a.json and raw command-20261017-105807-36696.log bind actual source/compiler/generated-header/runtime identities.

Latest complete18-file manual-folder core **H/artifacts-a09007a**, EXE **c22a33b0e84a2cd7b26518e887f3bdc678fa2750bd8659946ebd038ecf71a830**, PDB **5007ab336d000e8321d533c3ae33d944929af4f5660621020e6f554aa99b6556**. Whole-core runtime/game not_run for this exact exe. Distinct12-file native runtimes are saved in native-fixtures-1a35c8b and native-fixtures-a09007a, each with EXE/PDB/compiler/host shaders and its own NATIVE_RUNTIME_IDENTITY*.json. Nothing installed or selected automatically; previous working cores/Vulkan/saves remain at previously recorded paths. Start/current/post-source staged/unstaged patches and source snapshots saved; sources committed; .serena/ excluded navigation state. All owned build/fixture commands exited; no user process was stopped.

### C/D/G/H. Next production gap: dirty cached-native recreation

Crossed same-word/fixed-base mip/layer mapping is connected and tested; other geometry/source/ordered/writable/size gaps remain.129 implementation/behavior rows and nine broader gap IDs remain; no whole-group closure. Next use H/NEXT_DIRTY_DEPTH_RECREATION.md and UPSTREAM_DIRTY_RECREATION.json. Focused original trace: FindImage -> ResolveDepthOverlap does NOT refresh the old cached dirty image before native copy; only NEW single-sample growing storage is refreshed. New Dirty is cleared, then cached native I is copied; caller UpdateImage(new) happens after FindImage. Thus old initialized native allocation can be a recreation copy source after CPU dirtying, even though it is no longer a valid current independent I.

Current DX12 recreation selection/CaptureIndependentImageVersion reject that dirty source. Implement a DISTINCT retained cached-native recreation source with initialized class/allocation identity, source selection BEFORE GuestRasterizer prewrite capture, target extent/seed/aspect provenance, binding/resource lifetime and fence ordering. Do NOT clear logical_dirty, fake current ownership, relax ordinary independent reads or publish old I into B. Keep same-resource D refresh semantics separate. Start with the established fixed-base D16/D32 domain; other source domains remain explicit gaps.

Required GPU regression: initialize old native D/S, make an actual PageWatch CPU dirty store with distinctly different B data, force aspect/resource recreation. Copied intersection must use OLD native D; grown target-only mip/layers use current B seed; S follows separate initialization. Ordinary readonly stale-I lookup must still reject. Check physical B changes only for intended stores, post-transfer old native values, target/source descriptor lifetime and actual fence. Do not choose refreshed CPU B as copied-native expected data.

First commands: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` and `git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline`, then the two H next-contract files and ResolveDepthViewStorage/CaptureIndependentImageVersion/RetileImageVersion plus caller prewrite selection. Reuse existing Vulkan/DX12 baselines; do not repeat129/crash/readonly-DMA/disjoint/pressure audits or unchanged matrices. Full suite and minimum necessary final existing-XInput game/image/control/stability/completed-frame FPS-vs-baseline acceptance, excluding FG, remain required after implementation.


## Authoritative depth recreation extent checkpoint - 2026-10-17

Current production **2ac536b80053e6a4dee6e6a94f1fd9c20625cb72**, branch fix/mixed-cmask; previous verified production21d1e93 and its checkpointfdfe354 remain immutable. Full original Vulkan-to-DX12 goal is ACTIVE/INCOMPLETE. Narrow builds/regression/GPU/readback checks are authorized; full suite/expanded gameplay remain final. No game, UI/controller/profiler, app kernel installation/selection, original Vulkan/save, user-session/HUD/monitor action occurred. This FIRST section is current; previous sections retain historical scope.

### A/B. Contract -> additional implementation

In addition to21d1e93 full-parent subview/dirty-geometry/native-S-preserving D refresh and ordered single-sample integration, ResolveDepthViewStorage now handles applicable depth/aspect and typed color-to-MS-depth RECREATION with a smaller request. Pinned ResolveDepthOverlap uses new_info.resources=std::max(requested.resources,cached.resources), lexicographic levels then layers. The resolver selects that maximum while preserving requested format/aspect/selectors, BEFORE GuestRasterizer source/alias/prewrite capture. Selection requires a valid owned independent source, existing CanMapIndependentDepthImage/original root BlockDim and bits*samples admission, boundary validation and valid one-mip MS allocation. Ambiguous native sources fail explicitly. Dirty geometry alone cannot become a recreation source. Original copy S exclusion/exact-base GPU invalidation remain unchanged; assertions/operations/ownership are not bypassed.

The full promoted descriptor reaches existing CaptureIndependentImageVersion/UploadVersion initialization and native transfers. A smaller request must not retile a request-sized byte prefix or shrink the old image's later mip/layer data. Changed S addresses receive their new target initialization seed; old native S is not implicitly copied. Requested view_layers remains independent of the promoted storage layers.

### E/F. Executed checks / exact identity / limits

H=**F:/PS4Dev/handoff/depth-recreation-extents-20261017**. NATIVE_VERIFICATION.json records two scoped passes on DISTINCT native executable **9ef33e8a5831f5f57f1a961d930bf4d596361bb9d0b21290ccf16f87d9e8621c**: native_depth_recreation_extents7.54s (two RGBA32F-to-D32 MS4x optional-S cases) and native_independent_depth_subviews29.22s (eight D16/D32 optional-S1x/4x cases extended with aspect recreation). Each source snapshot/hash, binary/PDB, native LastTest and raw wrapper log is preserved. Source packages were tested before commit and equal2ac536b after canonical line endings. No result is transferred to the separate core exe. Prior21d1e93 five-test evidence is separate, not rerun or relabeled.

Eight new aspect requests start with one mip/layer but must retain the native parent's full3/1 mips and3 layers. Real readback checks every copied depth mip/layer/sample against the independently tracked source values, new S seed0x93 and different target padding0x83. It checks ownership replacement and absence of invented growth residency when promoted resources equal the source. Incompatible pitch is a negative. Existing typed SRV/DSV/CPU-store/current-B/ordered/fence/input-capacity oracles remain active. Two typed-color/MS cases promote a one-layer request back to two layers, then exercise the existing native draw and full-layer/sample/S/padding/input/guard/fence oracles with the resolved descriptor; untouched target layer keeps its initialization seed. Native validation has no error/corruption; informational warnings remain.

The first new color mode completed two GPU cases but its old final count guard expected16; corrected only this new mode's required count to2, preserving every independent pixel/S/padding/fence oracle. Its failed output/executable and initial eight-case pass on another binary remain under H. No FPS/image/control/game/world or priority-worker claim. Earlier d01969e0x80000003 ownership diagnosis/fix935fb69 remains documented in the previous authoritative section; no new game replay was performed.

### Reproducible core / saved state

Primary complete core compile/link13.893s. Separate **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016** detached exactly2ac536b: pinned Release compile/link23.361s,54 deps/2283 inputs/zero primary paths/empty status. H/RELEASE_PROOF_2ac536b.json and raw command-20261017-104356-67264.log bind source/runtime/compiler/generated-host-shader identity. Complete18-file manual runtime **H/artifacts-2ac536b**, EXE **928cf1c2a081ea4692af5baee99fd7abcfde75a0db2e5e5e32383adb74609c58**, PDB **30b0ca17b6687cddbb782b93dc91a05b8f77b1e1e9f1de3d593682955e3a9c19**. Whole-core runtime/game not_run for this exact exe.12-file native runtime/PDB/compiler/shaders retained separately in H/native-fixtures-2ac536b and NATIVE_RUNTIME_IDENTITY.json. Older21d1e93/92f085f/Vulkan builds remain at previously recorded paths; nothing installed/selected automatically. Start/current staged+unstaged patches and per-run source snapshots are saved. Source committed; .serena/ excluded navigation state. All owned build/fixture processes ended.

### C/D/G/H. Remaining code / exact next continuation

This closes applicable smaller-request maximum-resource selection for a VALID owned source and the already-supported depth target/aspect/typed color-MS transfer domain. It does NOT close all recreation. Remaining: lexicographically crossed resources (e.g. source1mip/3layers -> requested2mips/1layer), dirty-source recreation, shader-writable/render-target depth-to-color, other geometry/numeric/packed/raw/BC sources, ordered-MS/shifted domains and actual64-MiB guards.129 implementation/behavior rows and nine broader gap IDs remain; implementation and final acceptance are distinct.

Next production change: inspect pinned CopyColorAndDepth/CopyImage intersection bounds for the crossed-resource case, then adapt CanMapIndependentDepthImage (currently target.layers<source.layers rejects) and mapped RetileImageVersion (currently loops ALL source mip/layers at2594-2642). Do not merely remove the guard or independently maximize both axes: copy only the original supported intersection into the target's own guest mip/layer layout, keep target-only B seed/S/padding, descriptors/ownership and actual fence lifetime. Add a precise native case for1mip/3layers ->2mips/1layer, and its reverse where applicable. No new129 audit or repeated completed crash/DMA/disjoint/pressure research; don't rerun unchanged matrices to fill metadata.

First commands: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` and `git -C F:/PS4Dev/work/shadps4-dx12-src log -4 --oneline`; then H/UPSTREAM_DEPTH_SUBVIEW_CONTRACT.json and those two current functions. Use saved Vulkan/DX12 baselines. Full suite and minimum necessary final existing-XInput game/image/control/stability/completed-frame FPS acceptance, excluding FG, remain required after implementation. Full port is not complete.


## Authoritative depth-subview checkpoint - 2026-10-17

Production **21d1e93cd51a930e29b65d0ec7a7fd1831bba152**, branch fix/mixed-cmask. Full original Vulkan-to-DX12 port remains ACTIVE/INCOMPLETE. This section supersedes older statements that fixed-base smaller D16/D32 views are entirely unimplemented. Historical sections retain their original scope. Narrow compiler/regression/GPU/readback checks are authorized; full suite and expanded gameplay remain final. No game/UI/controller/profiler, kernel installation/selection, original Vulkan/save, HUD/monitor or user-process action occurred.

### A/B. Original contract -> production implementation

Pinned e0a46675 ResolveDepthOverlap returns the cached allocation for smaller bindings without recreation. Requested selectors must not shrink its mip-major physical guest layout or ownership. RefreshImage764-836 skips MS images and uploads DEPTH ONLY to existing single-sample D/S images. InvalidateMemoryFromGPU171-180 considers exact image base addresses; CPU dirtying is range based. Exact source ranges/hash: H/UPSTREAM_DEPTH_SUBVIEW_CONTRACT.json. Original BlockDim and bits*samples admission9fa2125 and overlap-copy S exclusiona4a51e8 remain authoritative.

GuestTextureCache::CompatibleGuestDepthSubview, MatchesGuestUploadStorage, FindIndependentUploadSource/FindValidIndependentImage/HasOwnedShaderStorage and Entry::Srv(view)/Dsv(view) retain full parents for fixed-base D16 UNORM/D32 FLOAT, equal width/height/pitch/sample count, requested mip/layer counts <= parent and valid selectors. Sampled color views can omit S while using the native depth format. GuestRasterizer::DrawInternal and ExecuteGuestResources retain full parent envelopes before source capture/alias/prewrite preparation; requested shader dimensions/selectors stay separate. PrepareOrderedGuestStages connects single-sample paired-depth admission, grouping, actual stencil geometry, validation, prewarm, pinning and same-bound-allocation refresh. Interior GPU writes still do not refresh I.

Entry::depth_storage_current and RememberDepthViewStorage/FindDepthViewParent/ResolveDepthViewStorage retain allocation extent after CPU/B invalidation without reviving dirty I; private prewrite snapshots do not replace that identity. UploadVersion refreshes all parent D mips/layers from current CPU/GPU B while preserving native S. When held bindings require a replacement, it copies full native S subresources, restores the source tracked state and retains native resources/binding pins through the submission fence. In-place refresh does not upload S or memoize unrelated B stencil equality. No assertion/draw/dispatch/ownership/guard is disabled; no game/address/hash bypass.

### C/D/G/H. Remaining production work / exact next action

Implemented scope is fixed base geometry and identical D numeric storage. Clean MS subview reuse is covered; MS dirty refresh and ordered MS alias lifecycle are NOT newly implemented/verified. Remaining: maximum-resource retention on other depth/color/aspect/MS recreation; changed numeric/packed/raw/BC source representations; spatial/tile/rebased/clipped mappings; remaining ordered domains and actual64-MiB capture/retention guards. These are code gaps, not absent final tests.129 implementation/behavior rows and nine broader gap IDs remain; no whole-group closure.

Next continue production C++ recreation: original ResolveDepthOverlap new_info.resources=std::max(requested.resources,cached.resources) uses lexicographic SubresourceExtent comparison, not independent maxima for levels/layers. Extend physical storage resolution and GuestRasterizer/ExecuteGuestResources source/prewrite captures to applicable depth/color/aspect recreation; connect UploadVersion/CaptureIndependentImageVersion and requested descriptors. Preserve original overlap admission and valid MS one-mip allocation constraints. Require full parent D/optional-S seed/ownership/fence footprints before relaxing a guard. Add an independent native pixel/readback case with distinct source/request extents and unchanged tail/S/padding.

First commands: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` and `git -C F:/PS4Dev/work/shadps4-dx12-src log -3 --oneline`; then read H/UPSTREAM_DEPTH_SUBVIEW_CONTRACT.json and current ResolveDepthViewStorage/CanMapIndependentDepthImage/UploadVersion plus the two caller prewrite paths. Do not repeat129 audit, completed crash replay, descriptor-only probe, unchanged175s ordered matrix or readonly-DMA/disjoint/pressure work without a new source reason. Do not revive older wrong S-copy/cache-admission claims or range-based GPU invalidation.

### E/F. Executed evidence / limits

H=**F:/PS4Dev/handoff/depth-subview-reuse-20261017**. NATIVE_VERIFICATION.json records five scoped passes on exact optimized native executable **315efd6af2cd67f97e482b1400c5c4447045846dff34f8fdeaf454ee03fa459b**: depth-subviews29.29s; paired D/S29.54s; growth58.54s; single-plane depth29.45s; ordinary independent image3.90s. Native logs, source snapshots/hashes, executable/PDB and compiler/shaders are preserved. These runs used precommit files equivalent to21d1e93 after canonical line endings. No fixture result is transferred to the separate clean core. /Z7 and /DEBUG keep optimized fixture failures symbolizable without changing compiler ABI or Release optimization.

New8-case D16/D32 x optionalS x1x/4x GPU fixture checks independently generated typed scalar/array SRV pixels/samples, selected DSV writes, full parent readback/tails/S/padding, receipts consumed once, real Windows PageWatch stores, dirty extent without stale-I reuse, CPU D refresh, pinned current-GPU-B replacement/fence leases and immutable original GPU input capacity. Four single-sample cases additionally run real B GPU fills through ordered stages, exact-base refresh and interior-write negative controls. Related paired coverage retains24 captures+24 conversions+4 aspect+12 compact cases; growth16; single-plane six format/tile cases. No native validation error/corruption; preexisting informational/performance warnings remain. No whole-core/game/world/image/control/stability/FPS/priority-worker claim.

Failed setup/ordered outputs remain immutable under H. Ordered extension exposed omitted paired production domain admission; the fixture then dereferenced null (0xc0000005, address0x14). ORDERED_FAILURE_CAUSE.json, ordered-symbolized-cdb.log and ordered-symbolized.dmp preserve actual exception/stack and llvm-symbolizer line214 proof. Paired admission/stencil geometry and the safe null assertion were fixed; independent expected pixels did not change. This is DISTINCT from d01969e game0x80000003: proven writable-B ownership failure/fix935fb69 and evidence remain F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Do not replay it for metadata.

### Reproducible core / preserved state

Primary full core compiles/links including GuestRasterizer/ExecuteGuestResources. Separate clean checkout **F:/PS4Dev/work/shadps4-dx12-clean-release-20261016**, detached exactly21d1e93, Release compile/link22.656s;54 pinned deps/2283 inputs/zero primary paths/empty status. Accidental preceding92f085f build was identified and is NOT current proof. Correct raw log command-20261017-103528-14340.log and RELEASE_PROOF_21d1e93.json bind commit/runtime/compiler/generated-header identities.

Manual-folder **H/artifacts-21d1e93** has18 runtime files, EXE **86538bde3bdf44c9b09e424cd68463fa6df66a485b0d0e234bf879ffb36a14ef**, PDB **a5207ea59e673b7be979b3040fa2a0364e977dbc0692c7f17ff21f995b2ce9ec**. Runtime/game not_run for this exact core. NATIVE_RUNTIME_IDENTITY.json/native-fixtures-21d1e93 preserve the separate native fixture runtime. No installed or previous working core was replaced. Staged/unstaged patches/new fixture snapshots saved; production is committed; .serena/ is excluded navigation state. Original saves/baselines remain at previously recorded paths. All owned fixture/debugger/build handles ended; no user session stopped.


## Authoritative original overlap-admission correction — 2026-10-17

Branch fix/mixed-cmask. Production **9fa21251bcaf72b9106d50dfd1e6e20ef5017f51** fixes a caller contract omitted by e94f886. Current source **92f085f4e285258ea9491f7c3db81cc5d1a21f01** changes only the color/MS fixture's cache-positive source mip count; production is identical to9fa2125. Full port remains active/incomplete. Narrow compiler/regression/GPU/readback checks are authorized; full suite/expanded gameplay remain final. No game/UI/controller/app install/selection, original Vulkan/save or user-process actions occurred.

CORRECTION TO OLDER CACHE PARITY CLAIM: pinned ResolveOverlap276-290 checks ImageInfo::BlockDim() and num_bits*samples BEFORE ResolveDepthOverlap/CopyColorAndDepth. BlockDim is pitch/block and height/block, not simply block extent. Earlier e94f886/616fb45 tests proved the broad explicit blit helper's real GPU behavior, but omitted this caller admission and used a two-mip source even for implicit MS cache recreation (original max-resources would create an invalid multi-mip MS image). Their cache-admission parity claim is superseded; exact historical results remain immutable. Do not discard the valid primitive pixel/S/guard/fence evidence or revive the old cache claim. H/UPSTREAM_OVERLAP_CLASS.json and H/NEXT_SUBVIEW_DESIGN.md preserve pinned source/hashes and the distinction. Original CopyImage/CopyImageWithBuffer still exclude S; old52add96 S-copy parity claim also remains superseded.

### Production contract / executed verification

GuestTextureCache::MatchesReferenceOverlapClass now checks normalized block pitch/height and bytes_per_block*samples. CanMapIndependentDepthImage applies it to color->MS depth source admission. UploadVersion treats incompatible SAME-BASE independent depth/color classes as distinct recycled image allocations: it leaves preceding I valid/owned and initializes the requested I from its supplied B source; no implicit I copy/retirement, inherited I provenance or invented residency effect. This matches the pinned pool-overlap branch. Shifted-base/other ordered domains retain existing guards. The public GuestMultisample helper keeps its broader explicit rendering contract; an explicit copy command is distinct from choosing a cache-overlap source. No draw/assertion is suppressed or replaced with success.

The revised fixture preserves every independent blit pixel/default/sRGB/S/padding/guard/fence oracle. Incompatible classes first assert no independent source/capture, no source retirement/modified-history inheritance or residency, and GPU-read back the fresh supplied CPU B upload payload before explicitly invoking the production helper. This verifies the Cache API's handling of provided B data; it does NOT prove whole GuestRasterizer CPU/GPU B-source selection. Physical B0 and original GPU seed/input buffers remain unchanged. Root-class controls include matching geometry with incompatible bit*sample sizes, plus equal bit*sample sizes with different pitch or height. Final eligible cache sources have one mip; four geometry-negative explicit-helper controls retain source mip1/two-mip allocations to verify private base-mip selection without claiming original implicit-cache applicability.

H=F:/PS4Dev/handoff/depth-subviews-20261017. NATIVE_VERIFICATION_9fa2125.json preserves exact executable **4285b20901db92b47b58a1dde2589d41a57aa28304ec1c914b60b762a55af882**, seven scoped passes: overlap-class14.79s, color58.17s, sRGB29.10s, wide14.65s, paired29.56s, corrected growth57.88s and ordinary independent-image3.82s. NATIVE_VERIFICATION_92f085f.json preserves a DISTINCT fixture executable and four targeted repeated color tests after only the mip-input correction: overlap-class14.73s, color57.68s, sRGB29.52s, wide15.09s. No previous pass is transferred to the rebuilt fixture binary. The final32 executed color scenarios comprise10 eligible implicit-cache blits and22 pool-class controls followed by explicit-helper checks. Four16x variants remain hardware SKIPs (no quality levels), not GPU verified. D16/sRGB mathematical precision bounds remain unchanged and documented in the earlier PRECISION_CONTRACT.json; no observed GPU values become expected data. Native content/S/input-capacity/padding, deferred receipts, actual Windows CPU guards and real held-fence lifetimes remain checked. Old failed outputs and all older builds remain untouched. No full suite, unchanged175s matrix or unrelated DMA/pressure/crash audit was repeated.

### Reproducible core / preserved state

9fa2125 clean Release18.268s is preserved separately at H/artifacts-9fa2125, EXEd53609d85a9731a0962f29c9bd2a5f06db69c142fe0f48d66246daeb8987a07d. Latest committed 92f085f primary and separate clean Release builds completed without source overlays. H/artifacts-92f085f is a complete18-file manual-folder runtime, EXE **19f74dbe989165609a544bc699c2bc1586d4cadcbedffd0f6596204f098f23ca**, PDB **16603d1e194dcd80c202f40193bf2eafa33a08d8134d4e3b6344c057b7e29bd5**. RELEASE_PROOF_92f085f.json/release-identity.json preserve54 pinned deps/2283 inputs/zero primary paths/clean status and exact runtime/compiler/generated-header identities. Whole-core/game/world/image/control/FPS/priority-worker acceptance is not_run; fixture evidence and compiler proof are separate. Nothing was installed/selected in the app. Staged/unstaged snapshots, source patches,7+4 exact run identities, all old evidence and targeted probe sources/binaries/logs remain under H. All owned build/fixture/probe handles ended; .serena/ is excluded navigation state.

### Next concrete production work — smaller depth views, NOT implemented yet

Continue cached smaller-view reuse/rebinding over the grown depth I, retaining maximum resources on color/MS recreation as required by the original. H/NEXT_SUBVIEW_DESIGN.md contains the focused source findings and exact next read. Required wiring: new supported depth-subview compatibility independent of full MatchesGuestMipLayout; MatchesGuestUploadStorage needs an early OR OUTSIDE that equality; FindIndependentUploadSource/FindValidIndependentImage/HasOwnedShaderStorage must retain the parent before any B snapshot; Entry::Srv(view)/Dsv(view) use requested selectors but keep the full parent allocation/owner/guest layout. Never shrink or retile a request-byte prefix when parent layer count changes later mip offsets.

Targeted API probe (NOT GPU content proof): actual D3D12 debug device accepted 2D SRV(mip1), array-one-slice SRV and2D DSV(mip1) over an R32_TYPELESS depth array3/mip3 with zero info-queue messages. Sources/exe and raw logs command-20261017-094658-56880.log / command-20261017-094700-54516.log are saved. Thus do not invent a requirement to rewrite Texture2D shaders just because the parent allocation has multiple slices; verify actual GPU pixels next. No need to repeat this descriptor probe absent new evidence.

PrepareOrderedGuestStages also has independent Domain grouping/validation/prewarm/rebinding gates. Read add_domain74-87 and sequencing170-215, then main image binds near1095/1309/1330/1371. A Cache-only predicate is insufficient to claim stage integration. Preserve full-parent refreshed allocations so existing RefreshBoundImage can update earlier bound descriptors. Check prepared B envelope coverage. Paired D/S dirty refresh currently has calls with only depth B snapshots/no S; do NOT just relax an earlier guard and call it implemented. Keep explicit ordered depth/MSAA gaps until source/S/seed/barrier ownership is implemented and checked. Required next GPU fixture: stable parent identity/extents and actual SRV reads/DSV writes through smaller mips/layers, optionalS,1x/4x, two retained views, unchanged other mip/layer/S/padding, CPU invalidation and held-fence lifetime; descriptor creation/linking alone is insufficient.

129 implementation/behavior records and nine broader gap IDs remain; no whole-group closure. Remaining smaller-view/maximum-resource recreation, shifted/spatial/packed/typed-source/ordered/actual64-MiB guards are production gaps, not merely absent tests. Reuse saved Vulkan/DX12 baselines and finish the established queue. Do not restart129 audit, crash replay or earlier readonly-DMA/disjoint/pressure experiments. Full suite and expanded existing-XInput gameplay/real completed-frame FPS vs baseline remain final.


## Authoritative typed color to MS depth checkpoint — 2026-10-17

Branch fix/mixed-cmask. Production **e94f886bad466a0d169f4626812c5c82e50af68d** connects the pinned color-channel -> multisample depth branch. Current source/checkpoint **616fb45873af98a0eda0ea294f633473f866ce5f** additionally corrects only the existing growth fixture's single-sample residency oracle; production code is identical to e94f886. Full port remains active/incomplete. Narrow compiler/regression/GPU checks are authorized; full suite/expanded gameplay remain final. No game, UI/controller/profiler, app install/selection, original Vulkan/save or user-session action occurred.

### Contract -> implementation -> evidence

Pinned e0a46675 Runtime::CopyColorAndDepth -> BlitHelper::ReinterpretColorAsMsDepth -> color_to_ms_depth.frag performs a real draw: typed source channels indexed by SampleID become depth, source textureSize scales UV, and native views select mip/layer zero with default component mapping. Stencil is not written. SampleId enables per-sample invocations; undecorated original UV position is implementation-defined. The native HLSL explicitly chooses sample-position interpolation, which the fixture independently verifies; no single mandatory Vulkan location or new Vulkan runtime comparison is claimed. The helper accepts 2/4/8/16 samples. Original vec4 indices >=4 have undefined values; native still draws/writes every sample and chooses zero for those undefined channels. H/UPSTREAM_CONTRACT.json saves pinned sources, hashes and exact scope. Earlier a4a51e8 correction still applies: CopyImage and CopyImageWithBuffer exclude S; the old52add96 S-copy parity claim is superseded, explicit CopyDepthStencil is separate.

Production src/video_core/renderer_d3d12/guest_color_to_ms_depth.hlsl is tracked and embedded through native-port/Guest-Tiling.cmake / guest_msaa_shaders.h.in. GuestMultisample::CanReinterpretColorAsMsDepth / PrepareColorAsMsDepth / ColorDepthLease::Record use typed native SRVs and a DSV draw with ALWAYS depth writes and stencil disabled. PSO keys include depth format, sample count AND native quality. RGB32's absent alpha maps to one. Resource geometry/flags/sample domain are validated. Writable sRGB allocations retain a linear UAV layout, but the pinned helper samples ImageInfo::pixel_format: source data10/number9 now restores the sRGB SRV. A real fixture failure (0.043137 instead of ideal0.0033465) exposed and fixed that production gamma loss without changing the independent source/oracle.

GuestTextureCache::CanMapIndependentDepthImage / FindIndependentUploadSource / UploadVersion now admit this specific differing-sample typed path without opening arbitrary resolves. CaptureIndependentImageVersion captures complete target initialization seeds; UploadVersion records the native draw DIRECTLY into the initialized target, retaining S and other slices. It inherits source I and deferred source residency, acquires the target guards before retiring source ownership, and publishes once at the actual bind. Source and destination are marked pending and the ColorDepthLease retains native resources, placed allocations, descriptors and PSO through the actual submission fence; Collect releases completed leases. No I->B0 publication, suppressed draw/assertion or game/hash/address workaround. Main GuestRasterizer now limits additional growth RefreshImage B residency to desc.samples==1, exactly as pinned ResolveDepthOverlap247-250; MSAA growth must not invent that effect. This callback rule is compiler/source evidence, not whole-main-runtime proof.

### Exact native verification / failures retained

H=F:/PS4Dev/handoff/color-to-ms-depth-20261017. NATIVE_VERIFICATION_e94f886.json archives the exact executable **aeedbb70fddd2e90083f91d9be52ba1837358ea4a08fe144af8f2b46ad5d232f**, six scoped passes: native_color_to_ms_depth59.12s (16 float/RG default-channel cases), native_color_to_ms_depth_srgb29.28s (8 typed sRGB cases), native_color_to_ms_depth_wide14.68s (4 actual8x cases); corrected paired29.59s, depth growth57.85s and ordinary independent-image3.83s. Four16x variants were explicitly SKIPPED because this device reports no quality levels; they are connected/compiled, NOT GPU verified. The28 new executed cases cover D16/D32, with/without S,2x/4x/8x, scaled13x11->7x5, default missing components, guest-swizzle/mip/layer selectors ignored by the private base view, target-only layers/S, independently computed native SRV pixels and full guest readback/padding. They use actual Windows dual physical aliases and CPU write faults, held real GPU fences, unchanged complete-capacity source/seed GPU readback, physical B0 immutability and deferred ancestor receipts exactly once. Zero-sample, integer-sampler and rebased-overlap negatives reject before capture. Generic shared PASS labels do not expand each mode's scope.

D16 precision failure was an ORACLE assumption: source0.5 became valid0x7fff (native0.499992371); D16/S8's D32 native pixel stayed0.5 while its typed R16 download also chose0x7fff. D3D FLOAT->UNORM permits0.6 integer ULP, while Vulkan allows adjacent integer conversion. The independent mathematical expected values remain; D16 checks that exact0.6001-unit bound, not observed native values. sRGB initially had an arbitrary too-tight float epsilon; the final independent interval maps encoded-byte +/-0.5 through the ideal decoder, per D3D3.2.3.7, plus the depth conversion bound. All S/padding/input bytes remain exact. H/PRECISION_CONTRACT.json links primary specs and preserves the reason; failed outputs/source snapshots remain, not erased or relabelled production defects. No broad epsilon or changed expected pixels solely to match the implementation.

The original e94f886 growth fixture injected an extra seed receipt even in its MSAA variant; it tested receipt carrying, not the main callback's policy. The later fixture-only correction 616fb45 conditions that injected effect on single-sample growth. NATIVE_VERIFICATION_616fb45.json archives its DISTINCT rebuilt executable and one repeated16-case growth pass. No previous passes are transferred to that rebuilt fixture binary automatically. All production inputs are identical between these commits. Whole GuestRasterizer/priority worker/HTILE/GCN/core/game/image/control/FPS acceptance remains not_run. Unchanged DMA/PageWatch suites and the old175s matrix were not repeated without cause.

### Reproducible source/core / current remaining work

Initial staged/unstaged snapshots and all11 new/changed sources (including originally untracked HLSL/fixture) were copied before commits under H. Both core builds completed from committed sources, including a separate clean checkout without overlays or primary inputs. RELEASE_PROOF_616fb45.json and release-identity.json preserve54 pinned deps, 2283 compile inputs, zero primary paths, clean status and exact compiler/generated-header identities. Complete18-file manual runtime: **F:/PS4Dev/handoff/color-to-ms-depth-20261017/artifacts-616fb45**, EXE **e97a7dee5f497655bd7969186f4e459f090b3f92d8e8c737485740c71b59eaf1**, PDB **4e3718d9dc4d35abba2327f8792184b5ca5306dfe55e7fd7fa7a51bc7020357d**. This exact whole core was NOT executed in game; fixture acceptance is separate. No automatic app/kernel installation or selection. Previous builds/profiles/saves/Vulkan/HUD/monitor/XInput paths and baseline measurements remain below. All owned build/fixture sessions ended; .serena/ is excluded navigation state.

CONNECTED: native typed float-compatible color->MS depth draw and direct-word fixed-base depth mip/layer growth. GPU VERIFIED: exact scoped2x/4x/8x modes above. OPEN:16x hardware behavior; raw/expanded/BC source representations without a native typed float SRV; cached smaller-view reuse/rebinding over a grown depth I; spatial/tile/rebased/clipped or packed growth; actual64-MiB snapshot/retention guards; remaining ordered depth/MSAA alias gates and other existing nine broader gap IDs.129 implementation/behavior records remain independent; no whole-group closure from these subcontracts.

NEXT: continue cached smaller-view reuse/rebinding. Inspect ONLY existing guest_texture_cache.cpp::CanMapIndependentDepthImage / UploadVersion / existing binding-view constructors and pinned ResolveDepthOverlap/ResolveOverlap. A smaller requested view must retain the grown backing allocation and other mip/layer I contents, with fresh correct SRV/DSV selectors and resource lifetime across already-bound stages; do not destructively shrink or refresh from stale B. Then continue spatial/packed/ordered gates in the established queue. Reuse saved Vulkan/DX12 baselines. Do not redo129 audit, crash replay, earlier readonly-DMA/disjoint/pressure audit or unrelated long matrices. Full suite and expanded existing-XInput game/FPS acceptance remain final.


## Authoritative depth mip/layer growth AND upstream aspect correction — 2026-10-17

### Current state / correction that supersedes older notes

Branch fix/mixed-cmask. Production a4a51e84739e501f066d114082ae0d7553665db2 fixes overlap aspects; a7e7ea5482e2e39a0ced692dd3780704805604e2 connects depth mip/layer growth afterad2259a/52add96. Full port remains active/incomplete. Latest specific human permits narrow compiler/regression/GPU/readback now; full suite/expanded gameplay remain final. No game/UI/app/controller/profiler, user-process restart, automatic kernel installation/selection or original Vulkan/save mutation occurred.

IMPORTANT CORRECTION: prior52add96/ad2259a implementation/handoff wrongly assumed Runtime::CopyImage preserves S. Pinned e0a466753a1aacbc8db8253be685bf7ab4d9daae vk_runtime.cpp248-249 explicitly removes eStencil from BOTH aspect masks in CopyImage. CopyImageWithBuffer also excludes S. Explicit Runtime::CopyDepthStencil471+ intersects aspects and is a DIFFERENT operation. a4a51e8 therefore keeps target S initialization for ALL cache-overlap conversions; direct CaptureDepthPlane1/explicit stencil operations remain intact. Only the converted-S pixel expectation changed to the independently supplied target seed per actual source contract. Original source/restored/private-S pixel, input/padding/fence/negative oracles are unchanged. Earlier exact results remain immutable and demonstrate their actual binary behavior, but their old S-copy assertion is NOT upstream parity evidence. H/UPSTREAM_ASPECT_CORRECTION.json preserves pinned source/hash/lines and explicitly supersedes that claim. Do not repeat or revive the erroneous statement. Original DownloadImageMemory/formatted synchronization remain depth-only.

### Production implementation / precise contract

Pinned ResolveDepthOverlap/ExpandImage initialize a new allocation then copy source mip/layer contents; target-only resources remain initialized contents. a7e7ea5 CanMapIndependentDepthImage recognizes growth in direct16/32-bit depth/color word families with fixed base spatial/tile geometry and matching samples. Its physical prefix comparison canonicalizes VIEW selectors before reducing temporary mip/layer counts. This does not change the actual target view or allocation. Source lookup and inherited GPU-modified history use the growth predicate, retaining I authority/logical validity rather than refreshing from stale B.

GuestTextureCache::CaptureIndependentImageVersion/RetileImageVersion now distinguish SOURCE plane layout/native footprints from TARGET guest layout. Each source mip/layer is copied/read on GPU and retiled with target mip offsets, target array strides and source-native linear stride. Adding array layers relocates later guest mip offsets, so a byte-prefix copy would be wrong. Native D32/S8 -> guest D16 conversion remains explicit; source1x/4x samples stay independent, without resolve/broadcast. Output starts with the complete target B seed; target-only mips/layers, all guest/native padding and S initialization remain intact. All source/input/output/mask/tiler/multisample objects and allocation accounts survive the actual submission fence. No guest B write or early semantic residency is added.

IndependentImageVersion::seed_residency carries additional source effects. Main GuestRasterizer queries IsGpuBufferModified before target capture for a growing image and records the target D EnsureResident range when pinned RefreshImage would obtain B. UploadVersion appends those effects after ancestor receipts and publishes only at the actual bind. S initialization does not invent a stencil EnsureResident effect. Existing shared per-plane PageWatch acquisition/retirement and stronger B/survivor rules are reused.

### Executed evidence / actual defects found

H=F:/PS4Dev/handoff/depth-growth-aspect-contract-20261017. NATIVE_VERIFICATION_a4a51e8.json archives the exact corrected-aspect executable (SHA256e781c9722e1d257daa100e845ff24624084c968c0fdc73a76efd780000b574bc),29.81s paired pass:24 conversions,4 depth/color cases,12 compact and24 original captures. It is separate from the later rebuilt growth executable.

NATIVE_VERIFICATION_a7e7ea5.json archives9 exact scoped fixture passes. New native-port/independent_depth_growth_fixture.h is INCLUDED/TRACKED with independent_version_transfer_test.cpp --depth-growth and native-port/CMakeLists.txt test integration. native_independent_depth_growth59.00s runs16 configurations: D16/D32, tile0/8; mip1->3 with two layers, layer1->3 with two mips, BOTH mip/layer2/2->3/3, and4x MSAA layer2->3. Odd13x11/pitch16, distinct GPU I, target B1 and physical CPU B0, source/target padding and every sample/layer/mip are covered. Independent typed native mip SRVs check each pixel; full GPU private output readback uses target CPU tiling oracle; source and seed full-capacity readbacks are unchanged. Target-only mip/layer pixels come from target B1; S stays target S seed. Deferred ancestor+target receipts publish once at bind. Actual Windows dual aliases/guards and a CPU S write retire the owner without I->B0 publication. An intentionally held real GPU fence retains source copies/leases. A nonmapped spatial-extent negative rejects before target seed capture. No new game/FPS/whole-renderer claim.

growth-first7.51s found a REAL production comparison bug: temporary prefix.layers was reduced while view_layers still described the larger target, so LayoutGuestTexture rejected it. The temporary physical-prefix view is now canonicalized before layout; actual descriptors are unchanged. The final matrix additionally strengthens simultaneous mip/layer and MSAA source-layer coverage; no expected pixel/byte result was relaxed. The original failed log and first correct/expanded source snapshots remain. Corrected paired fixture on the final executable passes30.44s; independent depth28.79s; independent image3.88s; PageWatch/cache0.03s; buffer watch1.39s; GPU ownership35.61s; attachment retirement3.93s; unified aliases0.50s. Growth was not rerun after unchanged source just to fill the nine-record manifest. No SKIP in16-case growth or paired evidence on this host. Prior175s matrix, unchanged DMA and full suite were not repeated without cause. Fixture/core compiler and executable identities remain separate; no pass is transferred to a rebuilt core.

### Reproducible Release / preserved work

Start staged/unstaged snapshots, aspect-correction JSON, complete source patches, failed/successful outputs, exact native EXEs/available PDBs and compiler/generated inputs remain under H. Primary precommit Release14.262s is compilation proof only. Exact committed a7e7ea5 primary identity Release7.563s, command-20261017-091510-71864.log. Clean separate checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached a7e7ea5 with no overlay; Release20.956s, command-20261017-091510-63916.log.54 pinned deps/2283 compile inputs/zero primary paths/empty clean status. Complete18-file runtime **F:/PS4Dev/handoff/depth-growth-aspect-contract-20261017/artifacts-a7e7ea5**, EXE SHA256 **87b52fee29cc16e078e40af3091b47bf7610ff90756a8c61d30c2857cc9f4cb3**, PDB **417940e5f89f61576959727eb8241ef617f49d99ea26ac192755f6a851e35494**. RELEASE_PROOF_a7e7ea5.json/release-identity.json preserve runtime/compiler/dependency/generated-header raw and LF-normalized identities. All owned build/fixture sessions ended. Whole GuestRasterizer/HTILE/GCN/priority-worker/core/world/image/control/FPS/full-suite acceptance is not_run. Previous kernels/reference/Vulkan/DLSS/FG/HUD/monitor/XInput/profiles/saves remain at their recorded paths below. Tracked source is committed; .serena/ is excluded navigation state.

### Next concrete production work / remaining guards

CONNECTED/GPU VERIFIED: paired ownership and depth-only overlap conversion, plus mip/layer allocation growth for fixed base geometry in the connected direct16/32-bit snapshot domain. Do not redo these mechanisms or129 audit. Remaining production: smaller requested view reuse/rebinding over a grown cached I (pinned ResolveOverlap returns views instead of destructively shrinking I), spatial/pitch/tile changes and mapped shifted/clipped transfers, packed/converted growth outside direct word domains, and snapshot/retention guards above64MiB (existing CanUploadGpuSnapshot/retention caps are still real limits). Do not label these merely missing tests. Remaining ordered depth/MSAA alias gates are also code gaps.

Next action: continue the pinned single-color channels -> MSAA depth repacking branch. Inspect ONLY existing vk_runtime.cpp::CopyColorAndDepth/ReinterpretColorAsMsDepth and its pinned helper/shader, plus native CaptureIndependentImageVersion/UploadVersion same-sample guards and GuestMultisample. Implement component-to-sample semantics and exact source formats/strides/initialization/aspects, not an ordinary resolve/broadcast or success return. Then continue cached smaller-view/rebinding and spatial mapping in the existing queue. Do not assume unsupported original MSAA branches are mandatory new requirements. Keep original aspects: cache overlap/compact/formatted copies exclude S; explicit CopyDepthStencil is separate.

Reuse saved Vulkan/DX12 baseline metrics and exact manifests; no crash replay (d01969e lost writable B ownership after prewrite Finish was proven/fixed935fb69), readonly-DMA/disjoint/B-pressure audit or unchanged175s matrix without cause. Full suite and expanded existing-XInput gameplay/real completed-frame FPS comparison remain final. Preserve all old evidence/builds, ordinary push verified checkpoints and continue the active full-port goal without confirmation, automatic app selection or original save/Vulkan/user-session actions.


## Authoritative paired independent ownership/conversion continuation — 2026-10-17

### Current production / pinned contracts

Branch fix/mixed-cmask; production52add961b02029ee420fc76ef00847f015af64f8 afterb170101/2be23d9/6f44e15. Full port remains active/incomplete. Latest specific human authorizes narrow compilation/regression/GPU/readback now; full suite/expanded gameplay remain final. No game/UI/app/controller/profiler, automatic kernel install/selection, original Vulkan/save mutation or user-process restart occurred.

Pinned e0a466753a1aacbc8db8253be685bf7ab4d9daae ResolveDepthOverlap/Runtime::CopyColorAndDepth has distinct aspect contracts: same depth/stencil aspects use CopyImage and preserve S; CopyImageWithBuffer excludes S at BOTH endpoints. Texture sampled color views can retain the depth image; a color render-target binding forces recreation. Compact DownloadImageMemory and original formatted BufferCache synchronization remain DEPTH ONLY even when S exists. H/upstream-depth-copy-contract.json preserves exact runtime source/hash and branches; previous compact-upstream evidence remains intact. Do not turn the explicitly unimplemented original MSAA overlap branch into a new mandatory requirement or success/no-op. The original single-color channels -> MSAA depth branch IS supported and remains a concrete native repacking gap.

### Implemented symbols / semantics

- guest_image_ownership.h::GuestImageOwnershipGeometry now carries optional independently validated stencil descriptor/layout. DescribeGuestImageOwnership derives color-shaped physical D and S descriptions, checks every physical mip/byte stride against the cached planes and native snapshot capability, while leaving actual native DSV/SRV/readonly usage untouched. CanRetainGuestImageOwnership validates BOTH planes through the existing backing/retention predicates. IsIndependentGuestDepthStorage remains the legacy single-plane helper; it is no longer the whole ownership admission gate.
- AcquireGuestImageOwnership/ReleaseGuestImageOwnership/GuestImageProtectionRanges are shared production operations, used by GuestRasterizer and the native fixture. They preserve cache identity, pending boundary guards, both backing ranges, actual strong B guards and flags-after-success transaction ordering; release retains other I guards and stronger B. Main OwnImage also detects a DMA B overlapping S, not just D. Existing PageWatch transaction, separate boundary maps and lazy full-byte witness validation are reused, not duplicated. Main compute/stage physical-retention checks use the complete owner helper; remaining ordered native UAV/alias gates are still explicit.
- GuestTextureCache::MatchesReferenceDepthImage recognizes exact physical/sample depth/color transfer domains separately from actual native access permission. FindIndependentUploadSource and inherited logical GPU-modified history use it, avoiding a stale B fallback or lost validity after conversion. Nonmatching geometry stays guarded rather than using a byte-prefix assumption.
- SetIndependentImageTransfer/CaptureIndependentImageVersion provide immutable D/S inputs. The main callback captures raw B with PreparePhysicalDmaBuffers/CapturePreparedShaderBuffer and no semantic residency publication or formatted bind replay. Depth uses RetileBufferVersion; native D16/S8 requires the existing D32-to-guest-D16 private plane conversion. Matching D/S aspects retile S from valid native I into the target's own padding seed, including a changed S address. Aspect-changing depth/color transfers keep the target's explicit S initialization seed and do not copy the old S; that seed is the native initialization contract, not a claim that upstream promises a particular undefined S value. The original formatted transfer still copies D only.
- UploadVersion acquires the new owner before retiring the old source, pins/retains real copies through fences and carries the exact pending B-residency provenance until actual binding. Logical GPU-modified validity survives replacement. Exact1x/4x native matching-sample planes are GPU-proved; no resolve/broadcast replaces samples. Full geometry/sample-repacking support is not inferred from that predicate.

### Executed evidence / preserved failures

H=F:/PS4Dev/handoff/paired-independent-conversion-20261017. NATIVE_VERIFICATION_52add96.json archives8 exact fixture passes, binaries/available PDBs, compiler/generated shader/SPV inputs and CTest/native/raw outputs. native_paired_depth_gpu_snapshot30.20s now retains the original24 ordinary-I capture/input/padding/held-fence and12 compact cases, and adds24 real paired independent conversions: D16/D32, tile0/8,1x/4x, two layers, odd13x11/pitch16 and three mixed CPU/GPU source modes. Six independent native typed shader outputs preserve the four old source/restored oracles and add converted D/S. Actual dual Windows aliases carry distinct CPU B0 and GPU B1 seeds. Negative S geometry/backing admission cannot partially guard D; source S retirement retains destination D/S guards; strong NOACCESS B survives demotion and CPU-store retirement. Preparation does not publish residency; actual bind publishes exactly once. All original/new GPU seeds, full native capacity/padding and physical CPU bytes remain unchanged before the intentional CPU store. A real S write fault invalidates/releases the final I without overwriting B from I.

Four single-sample D -> color RENDER TARGET -> D cases additionally check native intermediate color words via independent compact copy and final D/S guest output. The new S initialization seed differs from old I and CPU B0, proving that aspect-changing conversion does not blindly carry S. This is private readback, not a claim that the guest cache's priority worker or a whole GuestRasterizer/GCN frame was accepted. Other PASS: native_independent_depth_version_transfer28.73s; native_independent_image_version_transfer3.84s; native_page_ownership_and_cache0.03s; native_guest_buffer_watch_transfer1.41s; native_gpu_page_ownership36.02s; native_guest_attachment_retirement3.94s; native_guest_unified_buffer_aliases0.51s. The paired pass was preserved and composed with seven subsequent related passes without rerunning it after unchanged source. No SKIP in the24-case paired evidence; PSStencilRef=0 exercises fixed-mask fallback. No new FPS figure or whole-group/core/game acceptance.

Preserved failures: paired-first4.16s rejected the fixture's guessed128-KiB plane spacing before a copy; actual macro/MSAA guest layouts can exceed it. Backing slots now derive from measured layouts and use real disjoint aliases; no pixel/padding expectation changed. paired-backing29.57s passed24 independent cases. paired-aspects3.95s incorrectly expected readonly color sampling to recreate depth; pinned Texture binding correctly reuses it. The fixture now requests a color render target, the actual upstream recreation trigger. paired-aspect-target30.20s passes all4 aspect controls; renderer behavior was not changed to force a needless copy. Failed patch context matching made no partial source edit. All intermediate logs/source snapshots/patches remain; none relabeled as game failures. Source52add96.patch is complete committed production/test code. .serena/ is excluded navigation state.

### Reproducible core / retained paths

Primary precommit linked14.080s (compiler proof only). Exact committed52add96 primary identity Release13.925s, raw command-20261017-085236-51548.log. Separate clean checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached52add96 without overlay; Release21.567s, command-20261017-085236-54072.log.54 pinned deps/2283 compile inputs/zero primary paths/empty clean status. Complete18-file manual folder **F:/PS4Dev/handoff/paired-independent-conversion-20261017/artifacts-52add96**, EXE SHA256 **42dd0cc23fd429275ec77759422b24f4bc6c74017be1208aabfc9f485c8b1e9f**, PDB **51b81f1a8a73912712ae87f8d60a0e35b1a508559a8356f03bf9bf1aebf360b9**. RELEASE_PROOF_52add96.json/release-identity.json retain exact runtime/compiler/dependency hashes, immutable clean/fixture generated headers and LF-normalized equality. Fixture and core compiler/runtime identities are separate; passes are not reassigned to a rebuilt whole-core EXE. All owned fixture/build sessions ended. Core/world/image/control/FPS/full suite and whole GuestRasterizer/priority-worker paired lifecycle remain not_run. Prior kernels, installed Vulkan/DLSS/FG/HUD/monitor/XInput/profiles/saves and original crash dump paths below remain untouched.

### Next concrete production task

Next: lift the exact-geometry restriction for original depth/S resource growth/overlap, starting MatchesReferenceDepthImage/CaptureIndependentImageVersion/UploadVersion and pinned ResolveDepthOverlap/ExpandImage. Preserve target-only mips/layers/B-padding and source I authority with an explicit per-plane geometry mapping; use existing GuestMultisample partial/sample transfers and native copy capabilities only where their actual depth/subresource restrictions allow them. Do not copy an old guest-byte prefix into a new tiling or simply remove the guard. Then implement the pinned single-color-component -> MSAA-depth sample repacking branch in CopyColorAndDepth/ReinterpretColorAsMsDepth (currently source/target sample equality is required here), and address remaining guest_compute/ordered-stage depth/MSAA alias gates. Converted/BC shifted/clipped domains and other existing broader gaps remain in contracts.json; no new129 audit.

Paired exact-domain ownership and conversion are now CONNECTED/GPU VERIFIED at the shared production operations. Do not list them as wholly absent again or reimplement completed guards/captures/compact endpoints. Keep verification independent: whole GuestRasterizer/HTILE/GCN/priority-worker lifecycle, other geometry, final core/game and real completed-frame FPS versus saved baseline are still open. Do not repeat completed original crash replay (d01969e lost writable B ownership after prewrite Finish, fixed935fb69), readonly-DMA/disjoint/B-pressure audits or unchanged175s matrix without new cause. Preserve all exact evidence/old builds and ordinary push verified checkpoints; no auto-install/selection or original Vulkan/save/user-session mutation. Continue the active full-port goal without confirmation.


## Authoritative transactional image guards / compact paired-depth continuation — 2026-10-17

### Current state and precise upstream contract correction

Branch fix/mixed-cmask; production6f44e15711d052a33c6c2f2ce3c205ea9a29161e and2be23d93d2cc1b5bd3b050db4a7364cde065d868 after39d4d7b/e384ca5. This turn is production progress plus bounded real CPU fault/GPU evidence. Full-port goal remains active/incomplete. Latest specific human permits narrow compiler/regression/GPU/readback now; full suite/expanded gameplay remain final. No game/UI/app/controller/profiler, automatic kernel installation/selection, original Vulkan/save mutation or user-process restart occurred.

IMPORTANT refinement from pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae: TextureCache::DownloadImageMemory uses compact pitch*height*depth*layers*guest depth-byte size, mip0 and DEPTH aspect for depth images even when S exists. It does NOT download stencil. BufferCache formatted synchronization also retains its depth-only aspect. Therefore prior generic notes requiring paired CPU visibility/pressure must not be interpreted as publishing S. Both ranges need independent write guards and private I preservation; original compact downloads write D only. F:/PS4Dev/handoff/compact-paired-depth-download-20261017/upstream-contract.json preserves exact pinned source/hash and GC policy. ResolveDepthOverlap/CopyColorAndDepth retain original supported aspect/sample/overlap cases; upstream's explicitly unimplemented MSAA overlap branch is not permission to invent success, broadcast samples, or expand requirements beyond that original. No129-group re-audit occurred.

### Production implementation and verification chain

6f44e15: PageWatch::TrackImageReadOnly accepts all physical plane page ranges in one transaction. It normalizes shared/adjacent ranges, plans exact existing watch fragments and writable gaps, validates every OS protection before mutation, preallocates map/vector state, then changes permissions and commits watch generations. A failure rolls applied permissions back to their preceding watched modes; existing strong B and outside-owner fragments never become writable. Transferring exclusive I requires complete existing coverage, so a lost guard cannot be accepted as a new RW gap. Actual B guards are preserved explicitly; a claimed strong B in an unprotected gap rejects. Rasterizer::GpuOwnedBufferProtectionRanges returns actual gpu_owned mapping envelopes (excluding virtual GDS), distinct from semantic residency/GPU-modified pages. GuestRasterizer::OwnIndependentImage now validates backing and uses this transaction before changing its flags. Existing single-plane owners therefore preserve a newly overlapping strong B guard; multi-plane transaction support is ready but paired physical-geometry admission remains guarded.

6f44e15 exact4 fixture PASS in F:/PS4Dev/handoff/paired-ownership-acquisition-20261017/NATIVE_VERIFICATION_6f44e15.json: native_page_ownership_and_cache0.03s includes real CPU write faults, second-plane mismatched native protection rejection with unchanged first plane/metadata, rejected untracked readonly S, shared union, disjoint planes, outside exclusive-owner preservation, stronger B and missing-B/unaligned negatives. native_gpu_page_ownership37.27s checks a real GPU-derived B query, retains NOACCESS through image demotion request, verifies full GPU readback and eventual CPU fault commit, then confirms strong ownership ended. Existing buffer-watch1.47s and independent depth28.82s pass. Actual failure AFTER successful OS preflight was not forcibly injected; the rollback branch is implemented, while the executed negative proves preflight transaction atomicity. Do not call unexercised OS failure injection verified.

2be23d9: guest_image_ownership.h::IsGuestDepthPlaneStorage separates physical native D16/D32 plane capability from independent ownership permission. GuestLinearImageReadback now accepts a single-sample depth image with optional S, captures only plane0, computes native bytes per texel from actual GetCopyableFootprints row bytes/width, validates stride, and emits original compact guest bytes. D16/S8 uses native D32/S8, so float32 is explicitly quantized to UNORM16; finite values are tested and a NaN-safe saturate matches existing guest_tiling.hlsl before integer conversion (NaN output was not separately GPU-injected). D32 bits and other native/packed strides retain their exact paths. Pitch padding remains initial CPU bytes. Fence/extent checks precede mapping/mutation. AllocationBytes and source lifetime retain actual native allocations/fences. GuestRasterizer::RetireImageForPressure/ResolveImage route structurally supported paired single-sample depth to this class; their existing linear/sample/admission conditions remain. No stencil span/write is introduced. Actual paired end-to-end priority retirement still awaits ownership/conversion admission and its dedicated fixture; endpoint availability is distinct from that unrun behavior.

2be23d9 exact4 GPU fixture PASS in F:/PS4Dev/handoff/compact-paired-depth-download-20261017/NATIVE_VERIFICATION_2be23d9.json: native_paired_depth_gpu_snapshot30.05s retains24 original native I/private D+S/I cases and adds12 compact D16/S8+D32/S8 cases (two physical tilings, two layers, odd13x11/pitch16 and all mixed CPU/GPU source modes). Independently generated original guest depth codes/dyadic bits and complete compact pitch padding are checked; source native/CPU/GPU S and original seed readbacks remain unchanged. Short compact spans and a deliberately unsignaled GPU fence reject without changing CPU destination; unsupported compact MSAA rejects rather than resolving/skipping samples. native_independent_depth_version_transfer28.78s, native_independent_image_version_transfer3.82s and native_guest_attachment_retirement3.86s also pass; the latter proves actual native draw -> tiled writeback with64 checked pixels, not a whole paired priority-worker test. Initial paired-first29.29s and final results/logs remain separate. Existing pixel/negative/fence expectations were not weakened. Generated shaders/compiler identities/native stdout/CTest/raw logs/EXEs/available symbols and source patches are immutable in each phase's native-fixtures folder. Results do not transfer across rebuilt binaries or to a whole core/game.

### Reproducible Release / saved work

Both phases' start snapshots/patches and complete committed source patches are local; tracked source is committed. .serena/ remains excluded navigation state. Production primary compiled/linked15.737s at initial atomic work and13.921s at compact work (compiler evidence only). Exact final2be23d9 primary identity Release7.504s, raw command-20261017-083053-44348.log. Separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached exact2be23d9, no overlay, Release23.059s, command-20261017-083053-56732.log;54 pinned deps/2283 inputs/zero primary paths/empty clean status. Complete18-file runtime **F:/PS4Dev/handoff/compact-paired-depth-download-20261017/artifacts-2be23d9**, EXE SHA256 **19c52c0700f9adb3b8804c8ff70174d3608ce011c54ce8249765fabc5a67025f**, PDB **2cd4f97b04a4d47e6d1813be5d12e528b8b55945df3ad95ab5e8bb73e3e991dd**. RELEASE_PROOF_2be23d9.json/release-identity.json preserve exact compiler/dependency/runtime/generated shader hashes. This source includes both production commits; no separate whole-core runtime proof is inferred from fixtures. Whole core/world/image/control/FPS/full-suite acceptance remains not_run. Previous e384ca5/a2acbc3/99cb1f4/36f1373 and installed Vulkan/DLSS/FG/HUD/monitor/XInput/profile/save paths below are unchanged. All owned builds/fixtures ended; no user process touched.

### Next concrete missing production work

Next action: finish paired physical ownership geometry/admission and applicable independent conversion in guest_image_ownership.h::GuestImageOwnershipGeometry/DescribeGuestImageOwnership, GuestRasterizer retention predicates/OwnIndependentImage, and GuestTextureCache::UploadVersion/read_independent_ callback. Currently DescribeGuestImageOwnership still yields a single color-shaped owner and rejects split S/MSAA depth; native transfer support and transaction support do not remove that rejection. Validate separate D/S derived physical layouts against both cached layouts/mips, backing and original access restrictions; do not authorize depth UAVs through ownership descriptions. Reuse TrackImageReadOnly and both Entry::ProtectionPlanes maps; do NOT reimplement acquisition/boundary helpers. Extend private conversion inputs from one Resource to the necessary per-plane captures while preserving source authority, padding, residency provenance, original copy-aspect semantics, binding order and real source/destination fence lifetime. Use only pinned supported depth/color/sample overlap behavior; retain explicit guards for genuinely unported paths rather than returning success. Once admission is connected, add actual paired PageWatch/GuestRasterizer/priority-retirement GPU cases; the compact endpoint and pressure/resolve plumbing are already connected, but that behavior is unrun. Never publish stencil just to close a generic checklist item.

Use existing native paired capture12/24, single-depth and transaction fixtures plus saved baseline metrics. Do not repeat129 review, original crash/debugger (d01969e lost writable B ownership after prewrite Finish was proven/fixed935fb69), readonly-DMA/disjoint/persistent-B audits or unchanged175s matrix without new source cause. Original crash evidence remains F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Full suite/expanded XInput gameplay and real completed-frame FPS comparison remain final. Other existing broader gaps remain in contracts.json. Preserve exact clean builds/evidence, ordinary push verified checkpoints, and continue full port without approval requests or automatic kernel installation/selection.


## Authoritative separate depth/stencil boundary proof continuation — 2026-10-17

### Current production / upstream contract

Branch fix/mixed-cmask; production e384ca5f8fcd78ee811559b0001be238e8276387 after a8504e1/f0d047a/a2acbc3. Full port remains active/incomplete. Narrow compile/regression/GPU/readback are authorized now; full suite/expanded gameplay remain final. No game/UI/app/controller/profiler, user-process restart, original Vulkan/save mutation or automatic kernel installation/selection occurred.

Pinned Vulkan TextureCache MaybeCpuDirty (texture_cache.cpp772+) retains I when a shared-page outside CPU store did not change its content witness; it is independent of GPU B contents. The existing DX12 full-byte boundary oracle is stricter than upstream's limited hash. e384ca5 preserves that contract separately for D and S. GuestTextureCache::Entry now keeps stencil_unprotected_cpu_pages beside the original map, exposes both absolute ProtectionPlanes and checks HasUnprotectedCpuPages at every independent lookup/private retile/native prewrite source. One plane's witness cannot certify the other. Native formatted BufferCache synchronization remains depth-only; no new stencil B publication.

guest_page_range.h::GuestImageBoundaryPlane/VisitGuestImageProtectedRanges enumerate each physical page envelope minus that plane's pending holes. ValidateGuestImageBoundaries first rearms the union of ALL saved pages (deduplicating shared pages), then reads/compares each plane's exact saved segment, and only clears maps after all reads succeed. Failed protection or a late S read keeps both D/S proofs for retry. Changed-page output is deduplicated. Legacy single-plane helper delegates to that implementation and retains original saved segment sizes.

GuestRasterizer::RememberImageBoundary/ValidateIndependentImageWatches/ImageProtectionRanges/ReleaseImageProtection/WatchBufferImageVersions/DiscardBufferImageVersions/ResolveCpuFault now use both planes, preserve hole/survivor/stronger-B guard rules and release CPU buffer proofs for both ranges. Full-byte CPU changes invalidate I; same-value/outside stores do not fabricate a content change. Existing independent split-stencil/MSAA admission guards remain deliberately in force: these production callbacks are ready for two-plane records but the full independent acquisition/conversion/pressure lifecycle has not been implemented or accepted. Do not call this whole paired ownership closure or simply remove DescribeGuestImageOwnership/CanRetainIndependentImage/UploadVersion guards.

### Executed checks / exact results and limitations

H=F:/PS4Dev/handoff/paired-plane-watches-20261017. NATIVE_VERIFICATION_e384ca5.json archives7 exact fixture passes and EXEs/available symbols, compiler/generated source/SPV identities, CTest/native output and raw logs. native_page_ownership_and_cache PASS0.03s includes two new actual Windows CPU fault scenarios over dual physical aliases: D/S on one host page and on separate pages, independent512/128-byte witnesses, same-value store, all guards present before first read, protection failure before any comparison, late S read failure retaining D proof, a later unguarded changed S store, deduplicated changed pages, correct hole enumeration and preservation of a stronger NOACCESS B guard. These are production helper/PageWatch tests, not whole GuestRasterizer paired lifecycle proof. Existing multithreaded faults/legacy boundary/cache-corruption controls remain unchanged.

native_paired_depth_gpu_snapshot PASS30.51s: existing24 native I/private D+S/I cases keep all independent source/restored pixel, original seed/full capacity/padding and held-fence oracles; a new negative injects an unvalidated S witness and requires BOTH depth and stencil captures to reject before transfer. The injected test witness is then removed; no production guard is disabled. Remaining scoped GPU PASS: native_independent_depth_version_transfer28.89s, native_independent_image_version_transfer3.83s, native_guest_buffer_watch_transfer1.49s, native_gpu_page_ownership35.92s (GPU pixels, deferred physical CPU commit, depth/stencil and padding), native_guest_unified_buffer_aliases0.55s. Native output is preserved, including benign D3D12 optimization/initial-state warnings. No failing test or pixel expectation was relaxed. Initial/final runs are retained. prepare-archive command had a shell quoting SyntaxError before writing its local helper; corrected file-based script is preserved and no production source was affected. No GPU/test failure was observed for this phase.

before-commit.patch/source-snapshot.json preserve precommit tracked work; source-e384ca5.patch preserves the complete committed change. .serena/ remains excluded navigation state. Prior9 f0d047a passes,24 a2acbc3 capacity pass, earlier48-case175s matrix and all crash/profiling/baseline evidence remain tied to their archived identities below. Unchanged DMA/lifetime/pressure/full-suite cases were not rerun without cause. Standalone fixture converter and core runtime identities remain distinct; no fixture pass is assigned to a rebuilt whole core or different compiler. Current core/world/image/control/FPS/whole-group acceptance is not_run.

### Reproducible Release and saved kernels

Primary precommit Release linked14.173s (compilation only). Exact committed e384ca5 primary identity Release7.794s, command-20261017-081859-60448.log; separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached e384ca5, no overlay, Release21.436s, command-20261017-081859-66744.log.54 pinned dependencies/2283 compile inputs/zero primary paths/empty clean status. Complete18-file folder **F:/PS4Dev/handoff/paired-plane-watches-20261017/artifacts-e384ca5**, EXE SHA256 **597164fada941c9e38ffad9dd51e50004d0ae3650a8482a6941a40acc562324d**, PDB **b16884711a10ce056c83d731a4cdbb8a3661b7f76988802a6f9ad95468476999**. RELEASE_PROOF_e384ca5.json/release-identity.json preserve exact runtime/compiler/dependency/generated shader identities. Compilation is not GPU/game proof. Prior artifacts-a2acbc3/99cb1f4/36f1373 and installed Vulkan/DLSS/FG/HUD/monitor/XInput/profile/save paths below remain intact. No installed kernel replaced or selected. All owned builds/fixtures ended.

### Next concrete production task / prohibitions

Next action: extend guest_image_ownership.h::DescribeGuestImageOwnership and GuestRasterizer::OwnIndependentImage plus retention admission for validated paired D/S physical geometry. Implement atomic acquisition/demotion of both protection ranges, with rollback if second acquisition fails and shared pages/stronger B guards preserved. Then extend the single-Resource read_independent_/UploadVersion callback to consume existing CaptureDepthPlane D/S private inputs, and implement paired independent CPU visibility/pressure retirement before enabling admission. Reuse now-connected boundary maps/plane callback paths; do not reimplement them or restart129-group audit. Current guard still rejects split-stencil/MSAA independent owners; prewrite ordinary-I capture remains connected/tested at the primitive level. D16/S8 native D32/S8 must retain per-plane guest/native strides, sample/layer order and source/padding semantics. Formatted synchronization must stay original depth-only. Whole GuestRasterizer/GCN/HTILE and paired end-to-end ownership/retirement need their own targeted GPU proofs once enabled; no game launch after each commit.

Do not repeat original d01969e crash replay: lost writable B ownership after prewrite Finish was proven and fixed935fb69; original dump/manifest/exe/PDB/runtime evidence remains F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. No new crash explanation based on warnings. Do not repeat readonly-DMA/disjoint/persistent-B audits,129 review or unchanged175s stage matrix without a new source reason. Continue other existing production gaps after this lifecycle, preserve exact committed clean builds/fixtures and ordinary push verified checkpoints. Full suite, expanded XInput gameplay and real completed-frame FPS versus saved baseline remain final; no automatic app install/selection, original save/Vulkan mutation or user session/process actions.


## Authoritative native depth/stencil I capture continuation — 2026-10-17

### Current state and completed production contract

Branch fix/mixed-cmask; source f0d047a7f48880a7cc6bcd6b08cef00710c2cc26 plus capacity followup a2acbc3939d5b44664ddb2d64f66489534320362. Full port remains active/incomplete. Latest human authorizes narrow compilation/regression/GPU/readback now; full suite and expanded gameplay remain final. No game/UI/app/controller/profiler, original Vulkan/save mutation, kernel installation/selection or user-process restart occurred.

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae preserves native image contents independently of resident B and only publishes formatted depth through its original aspect. NativeDX12::GuestTextureCache::CaptureDepthPlane/RetileImageVersion now capture a valid ordinary GPU-owned I into immutable private D/S Resources without publishing or overwriting guest B. RetileBufferVersion keeps its original default depth-only aspect and strict native/guest stride guard. The explicit private D16/S8 exception converts native32-bit depth to guest16-bit codes; stencil uses its own DescribeGuestStencil layout and native plane subresource, MSAA uses GuestMultisample per-sample GPU loads. Invalid plane/source/extent/state and boundary witnesses still reject. Source entries, footprints, masks, upload/tiler/multisample leases and reservation bytes survive actual completion fences. Ordinary exclusive ownership does not become independent merely because capture is permitted.

guest_rasterizer.cpp::DrawInternal selects the unique valid cached GPU-owned depth I matching storage and mip layout, pins it before metadata preparation, retains native source authority through HTILE preparation, and captures each plane before current writes. Raw B supplies only immutable padding seeds; private I capture does not publish new B residency or replay shader binding effects. Ambiguous cached native resources reject. Existing modified-B/CPU-source paths remain for their valid domains. Native prewrite primitives are GPU-tested; the whole GuestRasterizer/GCN/HTILE/PageWatch execution path has not been exercised by these fixtures.

a2acbc3 aligns only new private output capacity to native committed buffer granularity so retained input-pool residency cannot count a tiny logical stencil length for a64-KiB allocation. Full available initial capacity seeds padding; tiling still writes only physical guest bytes. Ordinary formatted retile ranges are unchanged. The strengthened fixture asks GetResourceAllocationInfo independently and checks full native-capacity readbacks, so allocation and padding expectations are not simply copied from production.

### Executed evidence and exact identities

H=F:/PS4Dev/handoff/native-depth-capture-20261017. NATIVE_VERIFICATION_f0d047a.json archives9 exact fixture passes: paired depth snapshot, MSAA depth initialization, independent depth, independent image, guest buffer watch, unified aliases, DMA shader, buffer view lifetime and scheduler lifetime. Paired fixture executes24 cases, D16/D32, tile0/8,1x/4x samples, two layers, odd13x11/pitch16 and three mixed CPU/GPU source modes. Original I and restored I have independent native shader probes; private retile bytes use a CPU tiling oracle with original seeds and padding, D16 quantization and fixed D32 dyadic values. Original GPU/CPU inputs remain unchanged. Invalid plane/logical source negatives reject. A deliberately unsignaled real GPU gate fence verifies pending resources survive Collect; local capture references are dropped before signal/Finish. This device is10de:2c02 with PSStencilRef=0; existing fixed-mask stencil passes execute, no paired SKIPs.

NATIVE_VERIFICATION_a2acbc3.json separately archives the strengthened24-case paired fixture PASS29.35s, including full native capacity/outer padding and held-fence lease checks (EXE SHA25667c80e656d500f6e8533463134aca1dd2631c335790dd50ead44198da0c1b3dd). No unrelated9-fixture rerun was needed for the capacity-only private branch; the previous9 passes remain tied to their exact f0d047a executable/compiler inputs and are not automatically assigned to this rebuilt executable. Actual native output, CTest output, raw logs, available symbols/compiler/generated shader inputs and source patches are immutable in native-fixtures-f0d047a and native-fixtures-a2acbc3. Earlier intermediate runs remain intact. The previous175s ordered-stage matrix is retained at36f1373 rather than rerun without a relevant source reason. Adapter capability2x/8x is historical query evidence; new paired pixels execute1x/4x. Fast optional PS stencil-ref path was not executed on this feature0 adapter. No new FPS or whole-group acceptance claim.

Primary source compiled/linked before f0d047a; exact final a2acbc3 primary Release links11.287s, raw command-20261017-081201-28624.log. Clean separate checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached committed a2acbc3 without overlays and links20.323s, command-20261017-081212-36376.log.54 pinned dependencies,2283 compile inputs, zero primary source paths, empty clean Git status. Complete18-file runtime F:/PS4Dev/handoff/native-depth-capture-20261017/artifacts-a2acbc3; EXE SHA256 **5f68fb406981e74ab21778c247d5af8a315e14bbe65b969e2294e2cb11d1a609**, PDB **193f03cbe0e82b5ed7ae20929d22837c54e6504dac81655f58a650e3aafd1667**. RELEASE_PROOF_a2acbc3.json and release-identity.json retain exact runtime/compiler/dependency hashes and generated source raw/LF-normalized hashes. Compilation proves compilation only. Current core/game/world/image/control/FPS/full-suite and complete paired independent ownership/pressure are not_run. Older builds, installed Vulkan/DLSS/FG/HUD/live monitor/XInput/profiles/saves and original crash evidence stay at paths preserved below; none replaced. .serena/ is excluded navigation state.

### Next concrete unfinished production work

Continue paired independent depth/stencil ownership and PageWatch, not the now-connected source capture. IsIndependentGuestDepthStorage/DescribeGuestImageOwnership still reject stencil/MSAA; independent UploadVersion callback is single-Resource and keeps its stencil guard. Do not remove these guards ahead of lifecycle support. Extend GuestRasterizer::OwnIndependentImage/WatchBufferImageVersions/RememberImageBoundary/ValidateIndependentImageWatches/ImageProtectionRanges/ReleaseImageProtection across both absolute depth/S ranges. Add rollback on second acquisition, separate boundary store proofs for each plane, shared-page survivor/stronger B guards and real fence retirement. Then extend read_independent_/UploadVersion to paired private captures and pressure/CPU visibility resolution without guest B publication or CPU-roundtrip substitution. RetileImageVersion must check both plane boundary witnesses once introduced. Ordinary formatted synchronization remains depth-only. Paired capture tests do not justify ownership/retirement closure. Other existing broader gaps (converted/BC/shifted/clipped domains, shared depth/color and feedback, etc.) remain in contracts.json; no129-group re-audit.

Original d01969e0x80000003 root was previously proven lost writable B ownership after prewrite Finish and fixed935fb69, not the last warning or exit-code speculation. Dump/stack/manifest/exe/PDB/runtime evidence remains F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z. Its120s startup did not prove world/input/FPS. Do not repeat crash/game/debugger, persistent-B pressure, readonly-DMA/disjoint audits or full stage matrix without a new cause. Full suite/expanded XInput acceptance remain final. Continue production, preserve checkpoints and ordinary push verified commits; never auto-install/select a kernel.


## Authoritative paired depth snapshot / portable MSAA stencil continuation — 2026-10-17

### Current state / exact contracts

Branch fix/mixed-cmask; production `99cb1f467b8af8d19ea95f21343c482247e4095a`, after09dceb7/36f1373. This goal turn is progress: production C++/HLSL,24 actual paired GPU cases, formerly skipped stencil/partial controls,9 scoped regressions and committed clean Release. Full goal remains active and unfinished:129 independent implementation/behavior records, nine broader gap IDs. Latest specific human authorizes narrow compiler/regression/GPU/readback; generic older all-tests-deferred goal text is superseded. Full suite and expanded gameplay remain final. No game/UI/app/profiler/XInput, automatic kernel install/selection, original Vulkan/save mutation or user-process restart occurred.

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae preserves depth/stencil contents and samples while buffers and cached I may have distinct authority. Formatted BufferCache::SynchronizeMemoryFromImage excludes stencil: this phase does **not** retile stencil through an original depth-only formatted bind. It connects the native plane-upload/prewrite-input endpoint. Source-I capture and full independent paired ownership remain separate, explicit implementation gaps.

### Production implementation

- guest_texture_cache.h/.cpp::Upload/UploadVersion has an optional stencil_gpu_snapshot in addition to the existing depth/color Resource input. Either/both planes can be immutable GPU buffers, with CPU spans for the other plane. Each native extent/state/class is validated before cache mutation; per-plane pool growth, actual source capacities and readback reservations are counted. gpu_snapshot_any governs equality/reuse/CPU snapshots, and both old CPU snapshot vectors are cleared before refresh. A GPU stencil alone cannot leave a false CPU-equality witness. CPU-only and ordinary color callers retain the existing defaults.
- CanUploadGpuSnapshot structurally admits native D16/D32 depth/stencil and MSAA; actual D3D12 format/sample capability checks remain at preparation. This is **not** permission for independent multi-plane ownership or a writable color UAV over depth.
- guest_rasterizer.cpp::DrawInternal captures each modified B plane independently before current shader writes; raw image sources use PreparePhysicalDmaBuffers/CapturePreparedShaderBuffer, with no early semantic residency publication or replayed formatted shader effects. Modified depth and stencil can both remain native GPU inputs. The unmodified-plane legacy CPU source path remains; whole prewrite source-I/visibility behavior has not been accepted by a GuestRasterizer game/GCN test. Do not declare it closed merely because RHI uploads passed.
- guest_multisample.cpp/.h and guest_msaa_upload.hlsl retain the SV_StencilRef single-pass path when the device supports it. Without that optional feature, a sample-frequency shader discards samples whose selected input bit is zero; eight PSOs use fixed StencilWriteMask=1<<bit with OM stencil ref255. Full-plane initialization supplies zeros first. Partial uploads additionally draw a rectangle-only zero pass with mask255/ref0, then set bits, preserving outside pixels/layers and the full depth plane. The shader's extra bit constant/root layout and all8 PSOs stay in the fenced Program/Lease. No guest draw/dispatch/assertion/exception is suppressed; this performs the complete operation through native D3D12 capability adaptation.
- Native host-shader generated headers remain driven by Guest-Tiling.cmake's existing configure dependencies. Clean/native generated source hashes (including explicit LF-normalized hashes) are recorded; binary identities are separate.

### Executed evidence / discovered capability gap

H=`F:/PS4Dev/handoff/paired-depth-snapshot-20261017`. feature-query-fixed.txt directly records hardware vendor10de/device2c02, PSStencilRef=0, and supported2x/4x/8x quality levels for D16, D32 and D32/S8. The first paired test passed12 single-sample cases but skipped MSAA because the old helper required PS stencil reference. That was **not** full MSAA evidence. Earlier green msaa_depth_initialization also permitted skipping stencil uploads on this device. Its old expected values are unchanged; those controls now execute unconditionally through the new fallback when required.

New native-port/paired_depth_snapshot_fixture.h runs24 actual configurations: D16/D32, tile0/8, single-sample/4x MSAA, two layers, odd13x11 extent/pitch16, CPU seed followed by GPU/GPU, CPU/GPU and GPU/CPU reuse. GPU B1 values differ from CPU B0 and change each reuse. Independent HLSL depth/stencil SRV probes check every pixel/sample/layer; D16 is quantized back to its original16-bit code, D32 uses exact dyadic float bits, stencil includes the unsigned8-bit range. Full GPU input readbacks verify original bytes and both logical/outer padding; CPU input vectors remain unchanged. Wrong-state/short stencil and missing depth inputs reject before cache entries appear. No stale CPU equality witness is allowed. LastTest/native stdout is preserved, so no-skips/case counts are inspectable.

Final9 PASS fixtures: native_paired_depth_gpu_snapshot29.07s; native_msaa_depth_initialization0.43s, now including actual fixed-mask depth/stencil order, layers and partial rectangle; native_independent_depth_version_transfer28.55s; native_independent_image_version_transfer3.81s; native_guest_buffer_watch_transfer1.39s; native_guest_unified_buffer_aliases0.49s; native_dma_shader_addressing10.30s; native_buffer_view_lifetime0.32s; native_scheduler_lifetime0.37s. Manifest `F:/PS4Dev/handoff/paired-depth-snapshot-20261017/NATIVE_VERIFICATION_99cb1f4.json` archives exact EXEs/available symbols, CTest and actual native output, raw logs, compiler/SPV/generated host shader inputs. A separate alias run0.723s uses exact core runtime converter/DXC. Regular fixture Mesa18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf differs from runtimefedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab; do not transfer results between binaries/compilers or to whole-core/game acceptance. Fast SV_StencilRef path is retained but was not GPU-executed on this feature0 device.2x/8x capability was queried; the new paired pixel matrix executes1x/4x only. The unchanged175s ordered-stage matrix from36f1373 was not repeated without a new source reason. No new FPS figure is claimed.

Preserved setup failures: build-fixture used Descriptors::Get instead of Heap, fixed before execution. paired-first failed debug validation because the short-input negative recorded a transition and freed that test resource before command-list closure; its creation now needs no recorded transition, while the same invalid extent/cache-mutation oracle remains. No production lifetime check was disabled. The standalone feature query first lacked initializer_list; corrected source/compile output remains local. Initial12-case skips, failures and every later output remain; before-commit.patch/source-snapshot.json/untracked header preserve working development, source-99cb1f4.patch is complete committed code. These setup failures are not new game-crash explanations.

### Reproducible core / remaining verification

Primary source compiled/linked13.483s before commit (compiler evidence only), then exact source identity Release7.598s at99cb1f4, command-20261017-075530-18068.log. Clean separate checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached exact committed99cb1f4 with no overlay or local-source inputs; Release20.659s, command-20261017-075530-69948.log.54 pinned dependencies,2283 compile inputs, zero primary paths, empty clean status. RELEASE_PROOF_99cb1f4.json/release-identity.json preserve commands/toolchain/dependencies/18-file runtime, generated host shader source and file hashes. Runtime `F:/PS4Dev/handoff/paired-depth-snapshot-20261017/artifacts-99cb1f4`, EXE **612bfc73b93fbff7af48bb046a173b61c87c02ae89ad1dceeaade44965509dc1**, PDB **afa2720a0b21bfbfe8509cb03391c974eadb117ecf84f0f957681cf99d1bf055**. Previous kernels/reference builds and installed Vulkan/DLSS/FG/HUD/monitor/XInput/saves remain untouched; older sections retain their paths. Current core/game/world/image/control/FPS/final suite and complete paired GuestRasterizer/PageWatch lifecycle are **not_run**. All owned builds/fixtures ended; .serena/ is excluded navigation state.

### Next concrete production mechanism — not yet implemented

Continue **I-to-private paired depth/stencil GPU capture and independent paired ownership/retirement**, starting at RetileImageVersion/RetileBufferVersion, the read_independent callback and existing ordinary owned depth source selection. Reuse the new per-plane Upload inputs and executed GuestMultisample transfers; preserve the native I logical version and B0/B1 padding/source intent, without a CPU roundtrip or premature guest writes. D16/S8 uses native D32/S8, so plane footprint/quantization must be explicit; do not assume depth guest bytes equal native plane bytes. Keep original formatted depth-only aspect. Then extend DescribeGuestImageOwnership/retention and OwnIndependentImage/WatchBufferImageVersions/RememberImageBoundary/ValidateIndependentImageWatches/ImageProtectionRanges/ReleaseImageProtection to cover both absolute ranges and separate CPU-store boundary proofs, including shared pages, rollback and real fences. Current IsIndependentGuestDepthStorage still rejects stencil/MSAA and UploadVersion still guards conversion of an independent I through its single-Resource read callback; do not remove these until both planes and lifetime are connected and GPU-verified. Whole GuestRasterizer prewrite/visibility acceptance remains necessary; API tests alone do not prove it. Other8 broad gap IDs and converted/BC/shifted/clipped domains remain in the existing queue. Do not repeat129-group audit, completed crash/B-pressure/disjoint/readonly-DMA work or hardware-feature investigation. Full suite/expanded XInput and real completed-frame baseline comparison remain final; no automatic kernel install/selection.


## Authoritative single-plane depth / mip-prefix continuation — 2026-10-17

### Current state / reference contracts

Branch fix/mixed-cmask, production `36f137329816f19ced7daa0069bca2f798234019`, following c4efa4b. This continuation made production and actual GPU verification progress. Full port goal stays active, unfinished:129 implementation/behavior records, nine broader gap IDs. Latest human instruction authorizes narrow build/regression/GPU/readback; full suite and expanded gameplay remain final. No game/UI/app/XInput/profiler, auto-install/selection, original Vulkan/save modification or user-process restart occurred. Existing crash root cause/context/fix, benchmarks and old binaries remain in earlier sections.

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae: BufferCache::SynchronizeMemoryFromImage copies the **depth aspect excluding stencil** and retains exact-base formatted-reader requirements. ObtainBufferForImage chooses modified B or physical staging; separate I must not be resolved over unrelated B. TextureCache::ExpandImage refreshes the new image, then CopyImage preserves preceding I before freeing/rebinding the old identity. These contracts map to the implementations below. Do not invent formatted stencil retile, shared depth/color UAV permission, or new beyond-original requirements.

### Production changes

- guest_image_ownership.h::IsIndependentGuestDepthStorage/DescribeGuestImageOwnership admits exact single-sample, single-plane D16 UNORM/D32 FLOAT. Only physical ownership uses matching writable color-shaped geometry; the actual depth attachment remains typed/readonly to shaders. Other formats, split stencil, MSAA, expanded or different-dimensional domains retain their guards.
- guest_rasterizer.cpp::SetBackingAccess retention connects that geometry to existing PageWatch/backing/size predicates. OwnImage reconsiders a newly published DMA B before the already-owned fast path, and uses OwnIndependentImage to renew independent ownership. The existing strong I guard can become a readonly independent witness without replacing B's contents. RetireImageForPressure/ResolveImage admit the supported depth kind into existing linear/small-image paths; tiled valid I retention remains governed by pinned GC. QueueLinearImageDownload policy is unchanged.
- guest_texture_cache.cpp::CompatibleGuestShaderStorage/CompatibleGuestImageMipGeometry permits the exact readonly depth/sample storage domain. A depth attachment cannot become a shader-writable color allocation; existing geometry/type/format and shifted/clipped guards still apply. Existing RetileBufferVersion and GPU UploadVersion perform the actual depth-plane copies and tiling; no assertion/draw/dispatch was removed.
- guest_image_readback.cpp::GuestLinearImageReadback now accepts supported depth planes, preserving native row/pitch bytes, typed source state, fence checks and placed-resource retention.
- FindIndependentUploadSource now finds valid smaller mip chains through CompatibleGuestShaderMipPrefixes. Previously the ordered preparation lookup required full layout equality; BindTextures could therefore use B0 instead of expanding preceding I. The authored regression first failed at I byte0 (got0xb4b4b4b4, expected0x13570000; prefix8192/target10240 bytes) while B already passed. The production lookup fix preserves the old prefix/new physical tail through the existing private expansion path. No expected values were changed.

### Actual verification and its limits

native-port/independent_depth_fixture.h, integrated in CMake and independent_version_transfer_test --depth, runs D16/D32 at tile modes0(Depth2DThin64),8(linear),13(Thin1DThin), two layers, odd logical width13/height11 with pitch16. Native DSV clears produce I over distinct CPU B0=0xa5 and GPU B1=0x5a. Independent direct D3D12 plane copies supply the exact I bits. Actual GPU retile, sampled native identity, production PrepareOrderedGuestStages formatted I-to-B and reverse GPU B-to-depth upload are compared to that independent source. All logical texels, complete buffer/pitch padding, original B1 and actual guest B0 are checked. Compact depth readback and source release reject unfinished fences. Negative controls reject split stencil, MSAA and shared shader-writable depth/color. New single-plane RHI case passes29.74s; it does **not** execute the whole GuestRasterizer/PageWatch depth lifecycle or game acceptance.

Final8 scoped PASS cases: native_independent_depth_version_transfer; native_independent_image_version_transfer; native_msaa_depth_initialization; native_ordered_stage_image_buffer_bindings; native_guest_buffer_watch_transfer; native_guest_unified_buffer_aliases; native_buffer_view_lifetime; native_scheduler_lifetime. The complete48-case ordered-stage matrix passes175.38s with unchanged data/negative oracles (interior/exact writers, optional refresh/retile, sub-DWORD private streams, old-prefix growth, I/B/padding). Stage fixture now reports line and case on failure; original expectations remain. Its timeout is420s because48 independent caches compile tilers; the old90s process timeout is retained as an infrastructure timeout, not a functional pass/failure. No unrelated full suite was rerun.

`F:/PS4Dev/handoff/independent-depth-plane-20261017/NATIVE_VERIFICATION_36f1373.json` archives8 exact fixture EXEs/available symbols, raw logs, compiler/SPV inputs and loaded system D3D12/D3DCompiler identities. One additional alias run uses exact core runtime converter/DXC and passes0.715s. Regular fixture converter18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf and runtimefedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab remain distinct. No fixture pass transfers to another EXE, whole group or current core gameplay. No new FPS/performance claim; saved Vulkan/DX12 completed-frame baselines remain authoritative.

Failures/setup evidence remains in H. First compile lacked Descriptors' shader-visible argument; first ordered integration link lacked guest_stage_bindings.cpp in this fixture target; both fixed in test build integration before execution. Initial D16 test inferred the0.5 clear as0x8000, while the first differing native-source byte was0xff; that CPU conversion assumption was not a valid bit oracle. It now independently copies the native source plane before retile rather than guessing CPU float quantization or reading the implementation's output as its oracle; D32 clear bits additionally have fixed CPU expectations. Full padding/B0/B1 and negative checks remain. stage-diag/stage-values record the genuine smaller-mip lookup failure; stage-prefix-fix records90s timeout; stage-final is the complete175.38s PASS. Earlier failed evidence is never deleted or relabeled. source-snapshot.json/before-commit.patch/new header preserve the earlier working state; source-36f1373.patch is complete committed production/test change.

### Reproducible Release / preserved paths

Primary source compilation linked14.038s before commit (compiler evidence only), then source-identity Release linked7.578s at36f1373, command-20261017-073450-13840.log. Separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached exact source36f1373 without overlay; tracked/untracked status empty; Release linked20.443s, command-20261017-073450-51976.log.54 pinned dependencies,2283 compilation inputs, zero primary paths. Exact commands/toolchains/dependencies/runtime hashes are in RELEASE_PROOF_36f1373.json/release-identity.json. Complete18-file manual-folder runtime `F:/PS4Dev/handoff/independent-depth-plane-20261017/artifacts-36f1373`, EXE **16a7f3050fdab7473cef2a41390e5de01ee49fa675ece2b6e71e0f16cb0b53b3**, PDB **63e65f281ad0db0603b0bb697a27d9394c98a392a4915237dc4d899568b2f49b**. Current-core/game/world/image/control/FPS/final suite are not_run. Old external kernels, installed Vulkan/DLSS/FG/HUD/monitor/XInput and saves remain intact; paths are preserved in older sections. All owned builds/fixtures have ended; .serena/ is excluded navigation state.

### Next concrete production action

Extend **split-stencil/MSAA-depth independent I/B** rather than reopening the proven single-plane source lookup. Start at guest_texture_cache.cpp::CanUploadGpuSnapshot/UploadVersion (one gpu_snapshot and plane1 guard), RetileImageVersion, GuestRasterizer::SetIndependentVersionTransfer callback, OwnIndependentImage/WatchBufferImageVersions/ImageProtectionRanges/RememberImageBoundary and existing GuestMultisample depth-plane transfers. Split S must have its own seed, snapshot, mask, exact address/layout and protection/lifetime; native D32/S8 backing can differ from guest D16/S8 physical planes. Retain the pinned formatted depth-only copy; do not publish stencil through that operation. Keep GPU B1 separate from physical CPU B0 and current depth/stencil I, original bind order and real fence leases. Reuse the executed MSAA depth-init fixture and new native-plane oracle, add only missing actual GPU plane cases. A whole depth/ownership/pressure closure is still not established; actual GuestRasterizer/PageWatch end-to-end proof remains necessary. Other8 broad gap IDs and converted/BC/shifted/clipped domains remain in the existing queue. Do not repeat129-group audit, completed crash replay, memory-pressure/disjoint/readonly-DMA work or175s matrix without a new source reason. Full suite/expanded XInput/game/real-frame comparison remain final; no auto-install/selection.


## Latest readonly sharp / DMA alias proof — 2026-10-17

Test commit9fdd733 executes the pinned readonly SSBO + writable SSBO + ReadConst/DMA live-B contract in standalone and linked production conversion. The input's binding5 NonWritable decoration is checked; the native reflected binding is a live UAV, and readonly sharp and DMA observe the same-dispatch0xdbca0000+lane writes over prior GPU B1=0x22334455. Both fixture and exact core runtime converters pass the complete scoped DMA test. Evidence F:/PS4Dev/handoff/readonly-dma-alias-20261017/NATIVE_VERIFICATION_9fdd733.json. The prior queued actual reflected SRV/DMA conflict is covered for the production path: NativeOptions already enables native_storage_buffer_uavs, standalone/linked conversions apply it and shader cache keys include it. The incompatible custom SRV/UAV guard and original negative controls remain. No new fallback/snapshot is needed or permitted for true same-command aliases. Production core and16-case pressure evidence remain2e01aa6; the new commit changes only the fixture. Nine broader gap IDs/129 rows remain. **Next production mechanism: ordered opaque depth/stencil I/B transfers**, starting at guest_texture_cache.cpp::UploadVersion and guest_rasterizer.cpp::SetIndependentVersionTransfer callback. Keep existing exact-base/reference residency semantics, separate planes, masks, CPU B0 and ordering/lifetime. SESSION_HANDOFF remains authoritative.

### New proof, preserved failures and exact continuation

Source9fdd733acc7eda270c3f9b1234806d0df4774489 changes native-port/dma_shader_test.cpp only. The generated shader has writable guest sharp4 and readonly sharp5 over the same B, a workgroup/device acquire-release barrier, dynamic/immediate ReadConst and direct readonly loads. Before dispatch the complete native backing is GPU-only B1=0x22334455, distinct from old CPU values and the shader's0xdbca0000+lane writes. The original production ExecuteGuestBufferCompute path handles actual reflected UAVs and final DMA bank binding. Both standalone/linked DXIL reflect the readonly sharp as UAV while logical is_written stays false. GPU outputs test64 direct readonly lanes,62 valid DMA lanes and the existing invalid/fallback controls. Original scattered/hole/CPU-refresh/fence/feedback oracles remain unchanged.

The extended full scoped native_dma_shader_addressing test passes10.35s with regular fixture converter (command-20261017-071503-58880.log). The archived exact EXE49c5af540f0c11657a8905e26651db7fdb05da85cd9441f184d58b10c575e49b also passes10.47s with exact current core runtime converter/DXC (command-20261017-071538-60580.log). Manifest records both identities; native-fixtures-9fdd733 contains EXE/available symbols and raw logs. This adds scoped proof to already implemented production unification; it does not claim a newly implemented renderer subsystem, full-group closure or current-core/game/FPS acceptance. Current reproducible complete Release stays F:/PS4Dev/handoff/preserved-buffer-budget-20261017/artifacts-2e01aa6 with its prior exact EXE/PDB identities and54 pinned dependencies/2283 inputs/zero primary paths. No new core build was needed for a fixture-only commit; no previous result is assigned to a rebuilt EXE.

Setup failures are preserved in the new H: dma-default.txt attempted an ordinary non-unified DMA conversion as an SRV witness, but that conversion independently failed DXIL atomic-destination validation. It is not evidence of a production failure. Instead the fixture now directly proves original readonly decoration tied to binding5, then validates native reflection and GPU behavior after production conversion. dma-final.txt passed the new alias checks but failed the old scattered oracle because the new block wrote the shared guest fixture before the old scenarios. The new block now runs after the unchanged original scenarios; no expected values were relaxed. Final dma-isolated.txt and dma-exact-runtime.txt pass. The existing true SRV/DMA-bank negative at BindResident is retained. Do not rerun these setup errors or call them new port gaps.

Next command/action: inspect only guest_texture_cache.cpp::UploadVersion's independent buffer_alias_version guard (currently rejects split-stencil snapshot domains), RetileBufferVersion/RetileImageVersion, guest_rasterizer.cpp independent-version callback and the existing independent_version_transfer_test/msaa_depth_initialization_fixture cases. Extend the production GPU transfer representation for the required opaque depth/stencil planes while preserving original formatted exact-base bind effects, prewrite B1 vs physical B0, padding/store proofs, publication order and fence ownership. Keep a rejecting guard until a complete transfer is connected; do not add shader/game/address exceptions. Do not repeat129-group review, completed crash/debugger, B pressure/disjoint or reflected-class investigation. Full suite/expanded XInput gameplay and real completed-frame FPS comparison remain final. No game/UI/controller/user process was touched. Continue the active full-port goal after this checkpoint.


## Authoritative persistent buffer backing continuation — 2026-10-17

### Current state and upstream contract

Branch fix/mixed-cmask. Source checkpoint `2e01aa67acb768a1bfa58068a058e5912c4c05ae` follows89f009f/a39144a. Full port goal remains active and unfinished. The latest specific human instruction permits build/link and narrow regression/GPU/readback; full suite and expanded gameplay remain final. No game/app/UI/profiler/XInput, auto-install/selection, Vulkan/save mutation or user-process restart occurred here. Existing0x80000003 causal evidence/fix and game-startup limitations remain in earlier authoritative sections; no new crash claim or game/FPS evidence is inferred.

Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae, BufferCache::resident_ranges/GetArena retains guest resident backing until explicit invalidation/unmap; GetArena migration does not replace independent B1 with physical CPU B0. The previous proposed CPU spill cache was a hypothesis, not an original requirement. This phase implements persistent required backing rather than adding synthetic guest writes or deleting protective guards.

### Production implementations and commits

- `33a1e87e59a65f172e791fe43d2364e426941bdf`: rasterizer.cpp/.h::PreserveBufferBacking/AdmitBufferCache is shared by Find and PrepareShaderBuffersImpl. GPU/CPU-diverged B, actively resolving/preparing mappings, selected backing_version and retained preserved-version ancestry cannot be pressure victims. Required active ranges and mandatory bytes outside them are counted without overlap. Only reconstructible inactive mappings are reconciled, rechecked after callbacks and retired. Replacement savings are retained. Mixed native SRV/UAV preparation also protects selected generations. Soft cache admission is separate from hard native allocation admission.
- BufferBudgetObserver::Query(force), MemoryPerfConfig::AvailableBacking and AcquireBuffer use fresh DXGI availability on mandatory admission after reclaiming only idle/unleased pool/heap resources. Current commitment plus aligned new backing must fit availability after original reserve/other allocations. Zero/failed/insufficient actual budget still fails. No draw/dispatch, assertion, ownership or CPU visibility is suppressed.
- `1ef31296ced157ebca95c462cada7ee7de73afac`: pressure pass elects/sorts eligible victims once rather than rescanning all mappings per victim; ownership/generation/active predicates are rechecked around reconciliation callbacks.
- `2e01aa67acb768a1bfa58068a058e5912c4c05ae`: memory_allocator.cpp::PlacedBufferAllocator::CreatePage trims idle pages first and uses remaining commitment budget. Compact-large-page decisions follow the current page target, preserving scarce future admission instead of stranding slack based on the old256-MiB target. Live leases survive reduced/zero limits, while new pages must fit.

### Executed narrow verification

`native-port/preserved_buffer_budget_fixture.h` plus memory_perf_test.cpp and CMake registration add actual GPU/readback/PageWatch pressure tests. A fixed valid40-bit guest alias and independent physical alias retain4-MiB B1/CPU B0 under repeated2-MiB hot requests. Selected CPU-equal generations survive until the last observer is released, then become evictable. Unselected GPU-derived B1 and semantic DMA residency survive pressure; direct Find/GDS pressure is covered. A real same-value CPU store wins its page only; later GPU writes publish exactly4 dirty bytes on an explicit read. Whole GPU data, physical B0, untouched bytes, native identities and exact callback counts are independently checked. These are not microbenchmark-only assertions.

The two-page12-MiB placed-heap regression verifies three live GPU resources and readback, live retention at zero budget, new-page rejection and final lease retirement. The compact regression starts with a6-MiB admitted limit, allocates4 MiB, then admits8 MiB and allocates another4 MiB. It fails on the old implementation with BufferBudgetExhausted (compact-before-fix-2.txt; command070906-65564), and passes after the current-page compaction fix with independent0x71727374/0x81828384 GPU data. The earlier4+2-MiB case passed because it fit one heap; it was not a demonstrated failure and was corrected to the actual gradual-admission case. Existing expected data/negative checks were not changed to match production.

Final16 cases pass: native_memory_perf_preserved_versions, hot_eviction, pools, pending_fence, native_placed_buffer_alias_barriers, native_guest_unified_buffer_aliases, native_copy_dirty_ranges, native_guest_buffer_watch_transfer, native_dma_shader_addressing, native_independent_image_version_transfer, native_buffer_view_lifetime, native_scheduler_lifetime, native_guest_dma_indices_uav32/16 and native_guest_late_buffer_growth/private. Final native binaries, available symbols, logs, compiler inputs and SPV are archived under native-fixtures-2e01aa6-final. Alias and DMA additionally pass using the exact core runtime converter and DXC (runtime-alias.txt/runtime-dma.txt). Fixture converter18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf remains distinct from runtimefedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab. No results transfer to another EXE or whole-core/game acceptance.

Evidence root H=`F:/PS4Dev/handoff/preserved-buffer-budget-20261017`. NATIVE_VERIFICATION_33a1e87.json and _1ef3129.json preserve intermediate exact binaries/results. NATIVE_VERIFICATION_2e01aa6.json is explicitly INVALID_EARLY_ARCHIVE: it copied before all final cases completed; use **NATIVE_VERIFICATION_2e01aa6-final.json** only. The archiver now requires completed16-case results and checks each log is newer than its executable. All old data/failures remain. Build/test raw logs are also retained at F:/Codex-Shared/devtools/logs.

### Reproducible Release and limitations

Primary Release linked15.768s (command-20261017-071012-16932.log). Separate committed clean checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 fetched/detached exact source2e01aa67acb768a1bfa58068a058e5912c4c05ae, linked23.09s (command-20261017-071012-69024.log), source status empty.54 pinned dependencies,2283 compile inputs, zero primary paths; exact toolchain/dependency/runtime identities in RELEASE_PROOF_2e01aa6.json and release-identity.json. Complete18-file external runtime `F:/PS4Dev/handoff/preserved-buffer-budget-20261017/artifacts-2e01aa6`; EXE **c235fad68b7cd572658514d3c154007daf7878729d68229e315f09d05e579811**, PDB **c4f20f688c4f13e092c1388eed8f99933f5806037b6db3ce6d9c59b92cac3aed**. Build/link is compilation evidence only. Current core/game/world/image/control/FPS and final suite are not_run; previous reference metrics remain authoritative. Original Vulkan/saves and older working builds are retained. No owned live game/controller/test remains after commands complete; .serena/ is excluded navigation state.

### Next concrete production action / forbidden repeats

Pressure retention is now connected and narrowly verified; do not reintroduce a pressure spill requirement without new evidence from pinned original. Check only the queued actual reflected SRV/DMA-UAV contract at guest_compute.cpp final BeforeBind and guest_dma.cpp::BindResident against guest_rasterizer.cpp::NativeOptions and standalone/linked NativeStorageBufferUavs. Production converters set native_storage_buffer_uavs=true and include it in shader cache keys; ordinary native raw SSBOs therefore share UAV with DMA. Establish whether the queued conflict is an actual reachable original path or a guarded nonproduction conversion caller; do not invent required support for arbitrary external HLSL. Same-command true aliases must share live storage, not a prewrite snapshot. Reuse existing DMA/alias GPU controls; add the missing production-path proof if needed. Next remaining production implementation is ordered opaque depth/stencil I/B transfer in GuestTextureCache::UploadVersion/ExecuteGuestResources, followed by converted/BC word-view, shifted/different geometry and clipped domains already in the queue. Eight other broad gap IDs remain. Do not rerun129-group architecture audit, completed d019 debugger investigation, pending-DMA ownership, disjoint-view or full pressure tests without a new code reason. Full suite and expanded gameplay remain final; manual folder selection only.


## Authoritative disjoint native buffer-view continuation — 2026-10-17

### Current state / preserved evidence

Branch fix/mixed-cmask; production `a39144a72db03df98836296d64d8c319afc6d01e`, after f5c210a/51ec097. This turn made production C++ and real GPU verification progress. Full goal remains active/uncompleted:129 independent implementation/behavior records, nine broader gap IDs. The latest specific human instruction authorizes narrow compiler/regression/GPU checks; the generic goal's older all-tests-deferred text is superseded. Full suite and expanded gameplay remain final. No game, app/UI/XInput, profiler, auto-install/selection or user-process restart occurred. Original Vulkan/profile/saves and all previous builds/baselines remain untouched.

H=`F:/PS4Dev/handoff/disjoint-buffer-views-20261017` contains before-head/status, source-working.patch, committed source patch, build logs/proofs and per-binary native archives. Initial tracked state was cleanf5c210a; untracked .serena/ is excluded local navigation state. All four changed source/fixture files are explicitly committed. No reset, clean, unknown-file overwrite, binary/save commit or previous artifact deletion. Earlier exactd01969e ownership exception/int3 evidence and935fb69 fix remain in F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z; it was not replayed. The large-DMA growth regression remains fixed/scoped-verified by51ec097, retained under F:/PS4Dev/handoff/dma-copy-pending-20261017.

### Upstream contract -> implementation

Pinned BufferCache::ObtainBuffer/EnsureResident/GetArena keep one B arena version, including formatted GPU-only bytes outside current sharps. Native D3D12 cannot bind one resource simultaneously as SRV and UAV even for byte-disjoint views. PrepareShaderBuffersImpl previously split an old larger B, and RematerializeBufferEnvelope rejected clipped GPU-only contents. Recreating slices from physical CPU B0 would lose independent B1 and untouched tails. Conversely, a frozen SRV copy is invalid when an actual shader UAV can write the same bytes in that command.

**Current implementation:** for valid mixed-class requests requiring an existing GPU-only/owned version, validate that all native SRV envelopes are disjoint from all UAV envelopes. Keep the complete B through existing canonical UAV preparation, with original request addresses/sizes/written ranges/resident/publication flags. Only native descriptor class changes internally. Source original guest writes, CPU baselines/store markers, PageWatch and complete allocation-generation ancestry remain canonical. Each SRV binds a private DEFAULT GPU read copy. SnapshotShaderBufferRead creates that storage before final binding; ShaderReadSnapshot holds its original canonical view/version and placed leases, separately from immutable CPU UploadArena streams.

RefreshPreparedShaderBuffers refreshes existing GPU-read storage after late image/attachment/buffer updates, without allocating another GPU resource or replaying guest binding effects. It validates the original logical range, immutable selected native source identity, full-copy generation continuity, snapshot resource/offset/size and readonly role; unrelated recreated B and substituted views fail. Both actual current source and destination placements are retained before recording CopyBufferRegion through the real copy/use fence. Descriptors receive the copy's exact new offset/byte bounds; a source at canonical offset12 is checked in the fixtures. CPU snapshots remain immutable and take their existing separate lease path. SynchronizePreparedDmaBuffers validates private-read origin identity, synchronizes canonical physical B once, then refreshes those views; it never treats a read copy as canonical residency.

The read-copy pool reuses only resources with no live snapshot/copy/table lease, is bounded256 resources/128MiB, obeys the existing placed allocator budget, and releases idle copies under memory pressure. Pool metadata limits do not reclaim active allocations. Pure UAV production batches avoid the extra mixed-class VM checks. No SPIR-V/HLSL/guest ABI/shader metadata change was needed. True SRV/UAV envelope overlap still fails instead of using a stale snapshot. Production guest SSBOs already pass NativeStorageBufferUavs in standalone and linked conversion; their same-command read/write alias semantics remain on shared UAV B and are executed in the alias fixture. Unadmitted/clipped-limit guards remain protective; no hypothetical beyond-pinned-original requirement was added to the registry.

### Executed behavior evidence

`native_guest_unified_buffer_aliases` now additionally compiles an actual HLSL t0 ByteAddressBuffer/u1 RWByteAddressBuffer shader with disjoint sharps sharing preceding formatted B. A20KiB read at byte12 and a4-byte write at32768 must read late B1 value0x31323334 and write0x3132333b. The non-growth case executes the complete production ExecuteGuestBufferCompute metadata/binding path. The other forces an overlapping late envelope extension before final BeforeBind and uses the same production RHI refresh. Full GPU readback verifies all old B1 words, changed source word, exact output, B0-initialized new tail and padding. Explicit CPU visibility commits only the4-byte actual writer; physical B0 stays0x01010101 elsewhere. Offset substitution, true overlap and unrelated same-address recreation are negative controls. Existing standalone/linked SPIR-V alias results/CPU-write oracles remain unchanged.

`native_guest_buffer_watch_transfer` adds actual GuestBufferAccess/PageWatch cases for both prior readonly-formatted and real-written B. Disjoint SRV at byte12 retains the whole canonical version, refreshes after sync and preserving growth under PAGE_NOACCESS with no premature submission/commit, and reads exact current B1 on the GPU. Private snapshot state survives clearing logical views until the recorded fence, then retires; its GPU storage is reused only after completion. Full canonical B/padding and physical B0 are checked independently; explicit CPU reads commit exactly4/8 actual write bytes. No expected value was changed to fit an implementation.

Current NATIVE_VERIFICATION.json records11 exact PASS cases, archived EXEs/available symbols, unique raw logs/compiler inputs and generated buffer_alias_fixture.spv with hash: native_guest_unified_buffer_aliases(.60s), native_copy_dirty_ranges(.73s), native_guest_buffer_watch_transfer(1.35s), native_dma_shader_addressing(9.80s), native_independent_image_version_transfer(3.86s), native_buffer_view_lifetime(.29s), native_scheduler_lifetime(.40s), native_guest_dma_indices_uav32/16(3.92/3.89s), native_guest_late_buffer_growth/private(3.85/3.79s). No broad suite/unchanged page-ownership rerun. Two additional archived runs use the **exact core runtime converter/DXC**: alias(.727s) and DMA(9.995s). The regular fixture Mesa DLL18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf differs from runtimefedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab; identities/results remain separate despite matching pinned metadata. No fixture result automatically transfers to another EXE, a whole group, current core gameplay or FPS. Existing Vulkan/DX12 real-frame baselines remain authoritative; no new performance figure is claimed.

Setup failures are retained in H build logs and F:/Codex-Shared/devtools/logs: initial fixture used nonexistent Stage::Compute (fixed to CompileComputeDxil); one combined shell command incorrectly ran an old EXE after failed compilation, so command063010-9148 PASS is **discarded**, not current-source acceptance. Later commands gate tests on actual build exit. A late-growth fixture first requested merely adjacent backing, so one-resource capture correctly failed; it now overlaps the old final DWORD to exercise actual preserving growth with the same data oracle. Exceptional fixture cleanup now Finish-es while its descriptor table lives, eliminating its unwind-only deleted-heap validation error. The production metadata fixture now allocates a valid40-bit guest address because AmdGpu::Buffer encodes40 bits; a random host allocation was truncated. This is test address-space setup, not a production address/game workaround. Final current binaries pass; old failure/diagnostic outputs are not erased.

### Reproducible current core / limitations

Primary Release compiled/linked16.18s, command-20261017-063904-22964.log. Separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 compiled/linked23.84s, command-20261017-063904-16036.log. Sourcea39144a was fetched and detached without overlaying local files; clean status empty.54 pinned dependencies and2283 compilation inputs include zero primary-checkout paths. Exact commands/toolchains/dependencies are in RELEASE_PROOF_a39144a.json/release-identity.json. Complete manual-folder runtime `F:/PS4Dev/handoff/disjoint-buffer-views-20261017/artifacts-a39144a` has18 EXE/PDB/runtime/compiler/license/identity files; EXE **d7f6dc266b0e30defb7d1bc4a8e71fb321d9208b6ed85ad50ab252a37f4d56c1**, PDB **e0478710fc8f00d52e3d1d9e5ee2fa9389fe3edf0ab12a6dff95bc4e2c6e92e6**. Source patch and older immutable artifacts remain. Build/link is compilation evidence only. Current core/game/world/image/control/FPS/final-suite acceptance is not_run. Installed Vulkan/DLSS/FG/HUD/live monitor/XInput/save paths and previous staged launcher diagnostics remain in older sections; no application changes now. All owned builds/fixtures terminated; no user process/controller device was touched.

### Exact next production action

Continue **inactive GPU-only B pressure backing** at rasterizer.cpp::Find's budget guard and PrepareShaderBuffersImpl's reduced-budget/eviction guard. Reclaim native resident allocations without replacing formatted B1 by CPU B0 or publishing GPU-only bytes as guest writes. Preserve CPU baselines and same-value-store proofs, exact actual dirty visibility, semantic residency, active/source generation identity, and leases until the real GPU fence. Reuse existing memory-perf/buffer-envelope/ownership fixtures and pinned original residency algorithm; do not delete the guards without a complete storage/restore/validation implementation. Current disjoint-class path retains whole canonical B, so it deliberately does not solve pressure eviction of that version. Actual reflected SRV/DMA-UAV collision and ordered opaque depth/stencil/converted/BC/shifted/clipped I/B transfers remain in the existing queue, along with the other eight gap IDs. The true overlapping native SRV/UAV guard remains correct when compiler unification is absent; do not label that as same-command alias support through snapshots.

Next command: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short`, verify HEAD/upstream, then inspect only those pressure guard functions and existing fixture source. No129-group re-audit, original crash replay, repeated solved pending-DMA/class-splitting investigation, unchanged whole suite, per-commit game launches or expected-byte/fence relaxation. Use current per-binary evidence and saved Vulkan/DX12 baselines. Build/check related mechanisms, reproduce committed Release in the separate checkout, archive exact identities, checkpoint and ordinary push. Never auto-install/select cores or restart the user's session. Full suite and minimal final XInput/game/image/control/stability/real-completed-frame baseline comparison remain final. Goal stays active/uncompleted.


## Authoritative owned-envelope / pending-DMA continuation — 2026-10-17

### Current state, preservation and established cause

Branch fix/mixed-cmask; production `51ec0979800b23ae647c0d779714085bf251ea8c` follows5370017/ce30883 and the earlier fcb67e1/2d5e107/7a59dd2 mechanisms. This section supersedes older current/next statements without deleting history. The previous goal turn made code/evidence progress; this turn fixes the concrete large-DMA regression, with real GPU/PageWatch/negative controls. Full port remains active/uncompleted. Narrow compiler/regression/GPU checks are authorized by the latest specific human instruction; the generic goal's older all-tests-deferred text is superseded. Broad suite/extended game acceptance remains final. No game, app/UI/XInput, profiler, automatic installation/selection or user-process stop/restart occurred; original Vulkan/profile/saves remain untouched.

H=`F:/PS4Dev/handoff/dma-copy-pending-20261017` holds before-head/status, staged/unstaged source patches, complete source commit patch, diagnostic link/command/PDB/stacks and exact current native/core archives. Initial tracked state was clean5370017; preexisting untracked .serena/ remains excluded navigation state. No reset, clean, unknown-source overwrite, save/binary commit or old artifact deletion. The original d01969e crash ownership exception/int3 evidence remains at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z, fixed by935fb69; no original replay now. Prior startup exposure is not transferred to current binaries.

The large DMA failed with fences before=0/prepared=0/copied=1. **Established source/stack cause:** CopyBufferBatchImpl prepares page-expanded source B; RematerializeBufferEnvelope calls ResolveGpuBuffers -> Finish (and then Finish again) before copying the old allocation. Thus an already owned, dirty source is unnecessarily committed/submitted while only growing storage. H/copy-finish-armed.log captures both stacks from RunLargePacket through CopyBuffer/CopyBufferBatchImpl/PrepareShaderBuffersImpl/RematerializeBufferEnvelope. Pinned BufferCache::ObtainBuffer/EnsureResident/GetArena retain pending arena GPU contents; allocation growth need not make protected actual writes CPU-visible.

**Correction to the prior handoff hypothesis:** RunLargePacket already calls SetBufferOwnership with own/commit/release callbacks. Missing callbacks are not the cause. Plain Rasterizer here models ownership synthetically; the separate new GuestBufferAccess controls exercise actual PageWatch protection. The first debug attempt's -g bypassed initial command arming; it did not provide a Finish stack. Relinking the same Release objects using the same lld-link/toolchain with /DEBUG:FULL plus original /OPT:REF/ICF provided a separate diagnostic PDB. Its public Finish symbol was verified via llvm-pdbutil before a new run without -g armed the command at the loader stop and continued. Initial loader breakpoint is not an application failure. The owned diagnostic fixture exited normally with its original assertion failure; debugger command/result/EXE/PDB hashes are retained. This diagnostic link is not represented as the original exact Release EXE.

### Production implementation and verified contracts

Rasterizer::RematerializeBufferEnvelope now admits deferred growth only when all complete valid sources remain GPU-owned, have full CPU baselines and no actual CPU-upload markers. Sources with failed initial contents, CPU-visible dirty data or store markers keep the previous explicit reconciliation boundary. Physical read callbacks are rechecked for lost ownership/new store markers before target creation. Source-to-target copies remain on the same ordered GPU timeline; full-copy allocation-generation ancestry and source/destination leases are retained through the actual submission fence.

The target inherits every source's exact dirty spans at the rebased offset, separate CPU snapshot, formatted-divergence state and original CPU-store proof metadata. Dirty-map nodes, address-index nodes and rollback storage exist before protection callbacks. Existing owned source guards are borrowed unchanged. Only disjoint new physical gaps acquire ownership; GDS keeps its internal-host-array rule. Callback reentry remains guarded by preparing_buffer_envelope_/preparing_buffer_contents_. Lost source ownership, new source CPU markers or a CPU store recorded against provisional target backing rejects deferred publication. Newly acquired gap guards are rolled back without releasing source guards; ordinary reconciliation rebuilds a valid result. A throwing acquisition releases the new guards, preserves original map/generations and propagates the exception; retry works. No success is fabricated and no operation/draw/dispatch is skipped. Successful publication inherits dirty state and ownership atomically under the execution lock, with no allocating node insertion or owner callback left after guard transfer.

The earlier RO-growth tests expected the old eager reconciliation phase. They now distinguish owned growth: require unchanged submission fence, zero premature commits and actual PAGE_NOACCESS after growth, then explicitly ReadMemory before checking the **same original** CPU dirty-byte/data expectations. Same-value CPU-store injection for owned sources happens at the physical witness phase (there is no RO-downgrade callback on a successful owned route); source markers force fallback. Existing B0/B1/padding/negative oracles are unchanged. Four additional actual PageWatch joining cases cover two pending dirty B sources plus three physical gaps: success; rejected middle gap with prefix rollback/fallback; throwing middle gap with preserved old generations/protection and retry; CPU-store value0x61626364 into provisional prefix during gap acquisition, which must beat the earlier target upload. GPU readback checks both distinct B1 pages and all B0 gaps; CPU visibility commits exactly two original4-byte GPU writes, preserving the CPU winner and all outside padding. These are mechanism fixtures, not whole-game memory ownership acceptance.

### Exact tests, builds and limitations

Eleven exact scoped PASS cases are archived under H/native-fixtures-51ec097 with NATIVE_VERIFICATION.json: native_copy_dirty_ranges(.77s), native_guest_buffer_watch_transfer(1.27s), native_dma_shader_addressing(9.75s), native_independent_image_version_transfer(3.89s), native_buffer_view_lifetime(.29s), native_scheduler_lifetime(.40s), native_guest_dma_indices_uav32/16(3.94/3.88s), native_guest_late_buffer_growth/private(3.93/3.84s), native_gpu_page_ownership(35.96s). The original large DMA no-submit, exact dirty union/commit retry, source/gap/neighbor and odd-size GDS expectations all pass unchanged. Initial failures remain in old immutable7a59dd2/5370017 archives and unique raw logs; the intermediate RO-phase expectation failure is H/envelope-old-phase-expectation.txt. Regular logs still remain in F:/Codex-Shared/devtools/logs; per-binary copies prevent later overwrite confusion.

The DMA fixture additionally passed with the **exact current core runtime converter/DXC**, archived with its own command and hash inputs; regular fixture Mesa18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf differs from runtimefedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab. Identical pinned metadata is not binary equality. No fixture result automatically transfers to a rebuilt EXE, whole group, current core gameplay or FPS. No new performance baseline was measured; the proven timing property is no pending-list submission during admitted owned growth. Existing Vulkan/DX12 real-frame measurements remain authoritative.

Primary Release compiled/linked16.77s (command-20261017-061917-8596.log). Separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 compiled/linked26.74s (command-20261017-061917-31188.log), source51ec097, empty status,54 pinned dependencies and2283 compile inputs with zero primary-source paths. Source was fetched/checked out as a commit without overlaying local files. Full current runtime `F:/PS4Dev/handoff/dma-copy-pending-20261017/artifacts-51ec097` contains18 EXE/PDB/runtime/compiler/license/identity files. EXE **1dcac40a16056641481eb4aba137f4e9cce36dbd31b37c6dad4993099d275e16**, PDB **897981a79c38b9648e7c258a10ec3b8be5b0049508ea72c624d8efc192076970**. H/RELEASE_PROOF_51ec097.json and release-identity.json retain complete identities/toolchains/dependencies/build commands. Build/link proves only compilation; all behavior claims above name the actual separate fixture EXEs. Current core/game/world/input/image/FPS/final-suite acceptance is not_run. Installed Vulkan/DLSS/FG/HUD/live monitor/XInput/save/symbol locations remain in earlier handoff sections; no change/installation here. Old builds and baselines remain. All owned builds/fixtures/debugger commands terminated normally.

### Exact next production action / forbidden repeats

Next: src/video_core/renderer_d3d12/rasterizer.cpp::PrepareShaderBuffersImpl's simultaneous SRV/UAV class splitting and RematerializeBufferEnvelope's `clipped && diverged` guard. Pinned original uses one arena B version; preserve complete old B, original sharp byte limits, shader alias visibility/order, PageWatch/CPU baselines and resource/fence leases when native access classes require another representation. A stale SRV snapshot is not an acceptable replacement for same-command shader alias visibility. Keep the current guard until a complete implementation/verification exists. Reuse the existing buffer-envelope, physical-DMA and actual GCN alias fixtures; do not restart129-group review.

Other existing gaps remain: inactive GPU-only B under pressure, actual reflected shader SRV/DMA-UAV conflicts, ordered opaque depth/stencil I/B, converted/BC stages without common native geometry, shifted/clipped transfers and the other eight broader IDs.129 implementation/behavior rows and nine gap IDs remain; this owned-growth submechanism and failing DMA regression are connected/scoped-verified, not a whole-group/full-port closure. Start with `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` and current HEAD/upstream, then only the named functions/pinned contract. Do not repeat the resolved pending-DMA stack investigation, original crash replay, broad schema audit, completed private-index/bounds/mip/identity audits, unchanged whole suite or per-commit game launches. Reuse current exact binaries/measurements; archive new identities after code changes. Ordinary commit/checkpoint/push remains authorized. Never auto-install/select a core, touch original saves/Vulkan or restart the user's app/session. Full suite plus minimal final XInput gameplay/control/image/stability/real-completed-frame FPS comparison remains at the implementation boundary. Goal remains active/uncompleted.


## Authoritative initial physical DMA / private IA / GPU-fill continuation — 2026-10-17

Focused copy diagnostic after the main checkpoint: the unchanged no-submit oracle now reports `before=0 prepared=0 copied=1` (command-20261017-060916-4476.log). The source preparation does not submit; the first submission occurs inside CopyBuffer/CopyBufferBatchImpl. Only the failure message was extended in native-port/copy_dirty_ranges_test.cpp, not its expected fences/data or any production path. Current production core remains7a59dd2 with its preserved identities. Continue by isolating CPU-visible-dirty reconciliation vs preserving source growth **inside copy**. Evidence and exact diagnostic fixture identity: H/copy-diagnostic-followup.json. This narrows the failure boundary; it does not establish whether production or the plain-Rasterizer ownership precondition is wrong.

### A. Current state and preserved crash evidence

Branch fix/mixed-cmask; production `7a59dd26410aa2ba29b38d8fd1845af8b90c44fa`, following fcb67e1839e10a895d4849c148f074ae388b41ab and2d5e107b269b258b29a9fc1d797544591cf01a77. This section supersedes older current/next entries without deleting their evidence. Narrow compilation/regression/GPU checks are authorized; full suite/expanded gameplay remains final. No game, app/UI/XInput, profiler, core installation/selection or user-process restart this continuation. Original Vulkan/profile/saves remain untouched. Full port is not completed or accepted in Bloodborne.

The earlier exactd01969e crash was captured as `Independent image version lost its writable buffer ownership before execution`, followed by Liverpool coroutine UNREACHABLE -> assertion int3. The actual exception context, faulting stack, exact EXE/PDB/runtime, copied profile and dump remain at F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z.935fb69 restores writable B ownership after prewrite reads/Finish rather than suppressing the assertion. Its earlier120s startup diagnostic is not world/input/image/FPS acceptance and does not transfer to current binaries. Original SRV/UAV warnings/missing files are not asserted to be the crash cause. Launcher fatal stream/hex-report implementation and staged locked-DLL notes remain in the previous handoff sections; no launcher replacement was attempted now.

Current evidence H=`F:/PS4Dev/handoff/initial-physical-dma-20261017`. Before-status/head/staged/unstaged snapshots retain the initial tracked-clean8d8705f state. Three explicit source commits preserve all changes, including tracked physical_dma_backing_fixture.h. No reset/clean/unknown-change overwrite, binary/profile commit or original-save write. Preexisting .serena/ is excluded local navigation state. All previous handoff roots/artifacts/baselines remain.

### B. Implemented production contracts

**fcb67e1 — initial raw DMA.** Pinned BufferCache::SynchronizeDmaBuffers synchronizes resident physical B after actual shader/image stage effects; DMA range predictions must not resolve independent I into B0 or publish future sharp residency. ExecuteGuestResources now keeps dma_reads separate from ordinary requests, removes the initial opaque_dma ReadMemory branch and legacy DMA prefix in the ordinary preparation batch, and materializes existing/faulted resident backing through PreparePhysicalDmaBuffers. PrepareOrderedGuestStages no longer reserves predicted DMA ranges as shader buffers. Actual shader-buffer effects retain their original order/guards. BeforeBind materializes final actual DMA backing, refreshes true shader views, then binds DMA banks; auxiliary IA ranges are not shader SRVs. Actual reflected shader SRV/UAV conflicts remain guarded.

ResourceBindings::Prepare is an explicit one-shot phase before native IA pointers are selected. Bind reuses it. Reentry and preparation after failure/recycling are rejected; a callback which throws after writing every descriptor cannot later bind a partial table. GuestRasterizer normal/point producer and ExecuteGraphics run this phase before selecting native input storage. No assertion is bypassed.

**2d5e107 — original private index streams.** Pinned vk_rasterizer.cpp BindIndexBuffer -> BufferCache::ObtainBuffer(address,bytes,false), buffer_cache.cpp:194-211: clean reads <=16KiB use CopySparseMemory into a private stream without EnsureResident; larger/GPU-modified reads use resident arena storage. Rasterizer::ObtainIndexBuffer implements this distinction after binding effects. The mandatory physical reader copies directly into a leased, append-only64KiB UploadArena page. A held binding/submission prevents resetting its arena; released pages are reused, the retained pool is bounded256 pages and overflow pages retire by their actual leases. Upload heaps retain GENERIC_READ while IA read dependencies preserve existing read unions. Execution retains the arena lease through the real GPU fence.

Auxiliary index requests retain original byte-adjust/logical-byte fields but are not appended to ordinary shader buffers. BeforeBind marks actual shader writes, classifies larger/GPU-modified auxiliary spans at the late phase, prepares only their physical resident backing and publishes the original logical sharp. It then finalizes DMA, restores exact write reservations and refreshes selected shader generations. Small clean indices get no DEFAULT B allocation or residency effect; a preceding formatted B1 or protected independent I cannot replace physical B0 as their source. Resident UAV sources still use the existing private DEFAULT GPU index snapshot for D3D12 UAV/IA state compatibility. Both actual 16/32-bit draw controls passed.

**7a59dd2 — GPU fill.** Pinned Vulkan FillBuffer uses ObtainBuffer(...,true) for a GPU-modified range. Native GPU FillBuffer now calls PrepareShaderBuffers with one writable/resident range and its exact BufferWrite before upload. Existing preserving envelope logic retains complete old GPU B/snapshots/generations, while ownership/dirty publication uses only actual guest writes. The CPU fill branch and GDS path keep their existing behavior. New real GPU envelope control extends an8192-byte B to12288 bytes, checks an untouched2048-byte GPU-only prefix, exact fill contents, exact committed old/new dirty bytes and full physical B0/padding. Its oracle passes unchanged.

### C–F. Scoped results, failures and limits

At2d5e107, all12 bounded cases passed: native_guest_private_cpu_indices32/16; native_dma_shader_addressing; native_guest_dma_indices_uav32/16; native_guest_late_buffer_growth/private; native_guest_buffer_watch_transfer; native_reflected_resource_bindings; native_independent_image_version_transfer; native_buffer_view_lifetime; native_scheduler_lifetime. Actual hardware draws/copies/dispatches/readback and original negative/expected-data checks execute. New physical fixture uses real PAGE_NOACCESS guest alias, distinct B0/B1/I and full/partial logical mappings; engine ownership callbacks for that image are synthetic, not whole-core proof. Index fixture makes B1 indices degenerate while B0 still draws the expected triangle. No relaxed pixel/data expected values were introduced. An initial compile failure was a missing guest_dma.h include in the fixture, fixed before these passes; build log055823-37836 retains it.

NATIVE_VERIFICATION_2d5e107.json and native-fixtures-2d5e107 preserve exact EXEs, available symbols, compiler inputs and per-case output/raw logs. They belong only to those binaries. At7a59dd2, NATIVE_VERIFICATION_7a59dd2.json/native-fixtures-7a59dd2 record two scoped PASS cases (extended envelope/watch/fill and DMA shader) and one FAIL: native_copy_dirty_ranges, `DMA submitted the pending guest list` in RunLargePacket. Exact-write disjoint/overlap/commit-retry controls preceding it pass. The copy fixture never calls FillBuffer, so the new fill code is not on that failing path. Production vs fixture-precondition cause is **not yet proven**. Do not edit the fence oracle or claim large-copy asynchronous parity.

A bounded CDB attempt ran only the archived failing fixture, not a game/user process. It exited normally with the same test failure; the Release fixture has no standalone PDB, so the symbolic Finish breakpoint did not resolve and **no Finish call stack was captured**. copy-debugger-command/result.json, copy-finish-debugger.log and console preserve this limitation. Do not mistake this attempt for a localized root cause or repeat it without symbols/another targeted instrumentation strategy.

Both2d5e107 and7a59dd2 additionally pass the DMA fixture using their exact core-runtime converter/DXC, with separate command/hash records. Regular fixture Mesa DLL is18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf; runtime converter isfedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab. Identical source/patch metadata is not binary equality. No narrow result transfers automatically to a rebuilt EXE, full group, game or FPS. No new performance measurements; existing Vulkan/DX12 baseline evidence remains authoritative. Broad suite/game/XInput are not_run.

### Working builds and reproduction

Primary Release7a59dd2 compiled/linked in11.66s, command-20261017-060556-41636.log. Separate clean committed Release at F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 compiled/linked in18.68s, command-20261017-060556-38188.log. Pinned original e0a466753a1aacbc8db8253be685bf7ab4d9daae is read-only;54 dependency revisions, LLVM22.1.3/MSVC SDK environment and DXC identities remain pinned.2283 compile inputs include zero primary checkout paths; clean source status is empty. No primary source overlay was used. C++ build proves compile/link, not GPU or game behavior.

Current complete external runtime `F:/PS4Dev/handoff/initial-physical-dma-20261017/artifacts-7a59dd2` (18 EXE/PDB/runtime/compiler/license/identity files), EXE **d36c1bc70de83680065959280ff186234a633b6bb7d392c6516e2913fdc0ce5d**, PDB **b6e24ac5d5e8e7add719f4ad01ce5a0432dd2b942cef78beefa27f514f28d7b3**. RELEASE_PROOF_7a59dd2.json records per-file hashes/toolchain/commands/dependencies. Earlier fcb67e1 and2d5e107 clean committed runtimes/proofs also remain in H; old handoff folders are not replaced. All cores remain manual folder choices, never auto-installed/selected. Installed Vulkan/DLSS/FG/HUD/live-monitor/virtual-XInput/debug/save locations and identities are retained in the earlier sections; no changes here. All newly launched build/fixture/debugger processes terminated; no user process/controller device was manipulated.

### G–I. Exact continuation and forbidden repeats

First fix/isolate the concrete **large DMA pending-list** failure with the existing fixture: CopyBufferBatchImpl -> PrepareShaderBuffersImpl's cpu_visible_dirty/retire-overlaps -> RematerializeBufferEnvelope's ResolveGpuBuffers/Finish. Establish the source fence immediately before/after shader preparation and copy, and the actual ownership/protection precondition. RunLargePacket currently constructs plain Rasterizer without the SetBufferOwnership callbacks used in its earlier Run controls; page-envelope expansion can also require preserving growth. Those are explicit hypotheses, not established causes. Preserve the original no-submit assertion, source/destination/padding/neighbor/GDS expectations. Use symbols or narrowly scoped diagnostic instrumentation; don't retry the unresolved CDB breakpoint unchanged. A production change must preserve actual GPU contents, exact CPU visibility, ordering and leases; do not suppress Finish if it is required for unprotected CPU visibility. Once the mechanism is valid, rerun only this fixture and directly affected ownership/DMA cases, archive new identities, rebuild clean committed Release.

Then continue existing formatted-alias queue: mixed native SRV/UAV envelope splits, inactive GPU-only B pressure storage, actual reflected shader SRV/DMA-UAV conflict, ordered opaque depth/stencil I/B, converted/BC stage domains without common native geometry, shifted/clipped transfers and the remaining eight gap IDs. Direct Find still retains its protective overlap guard; GPU FillBuffer now avoids it via writable preparation, and CopyBufferBatch already prepares operands. Don't remove the guard generically.129 implementation/behavior rows and nine IDs remain; only these proven subcontracts move from missing to connected/scoped GPU evidence. No whole-group/game closure.

Next command: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short`, verify HEAD/upstream, then inspect only native-port/copy_dirty_ranges_test.cpp::RunLargePacket and those three production functions. Reuse H/native-fixtures-7a59dd2 failure and manifests, pinned build trees/clean reproduction. No129-group re-audit, original crash replay, completed DMA separation/private CPU index/bounds/mip/identity audits, repeated unchanged full suite/game, or expected-byte/fence relaxation. Preserve checkpoints and ordinary-push verified source with explicit known failures; no force push. Full suite plus minimal final game/XInput/real-completed-frame FPS comparison remain at the full implementation boundary. Goal stays active/uncompleted.


## Authoritative final-buffer and physical-DMA continuation — 2026-10-17

### Current code and policy

Branch fix/mixed-cmask, production `0d5b4693e0784bdc7e01aaa9db2d6b2275851367` following9791682c77671bbde6bb31aa78664707e6d289c2 and6e5eab3. Narrow compilation/regression/GPU/readback checks remain authorized by the latest specific user instruction; full suite/expanded gameplay remains final. No game, app/UI automation, XInput, profiler, installation/selection or user-process stop/restart occurred in this continuation. Original Vulkan/profile/saves remain untouched. The previous goal turn made production/evidence progress; this continuation adds two further production mechanisms. Full goal remains active/uncompleted.

Evidence root `F:/PS4Dev/handoff/attachment-backing-20261017` (H below). before-status/head/staged/unstaged snapshots preserve the initial tracked-clean state; preexisting untracked .serena/ is excluded navigation state. Source patches/commits and tracked new physical_dma_backing_fixture.h preserve all code; no reset, unknown-file overwrite or binary/profile commit. Previous crash root F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z and all older artifacts/baselines are retained. Runtime9791682 and its six-test evidence remain separately archived; current results are not transferred between EXEs.

### 9791682: final native views after attachments

Pinned original vk_rasterizer.cpp:200-214 calls BindResources (per-stage BindBuffers -> BindTextures and DMA sync), then BeginRendering/IA, then pipeline BindResources. BufferCache::GetArena supplies stable arena storage. Native ExecuteGuestResources previously selected raw SRV/UAV descriptors before image/attachment uploads could replace a preserving buffer envelope. Production now keeps logical stage effects and immutable snapshots at their original phase, and selects actual native buffer descriptors/push offsets/inclusive bounds in BeforeBind after attachment preparation.

Rasterizer::Mapping and ShaderBufferView carry BufferBackingVersion (original native resource, base and extent). Only RematerializeBufferEnvelope's complete GPU copies inherit source generations, including transitive ancestors. Weak ancestry expires with outstanding old views and does not itself pin old GPU allocations. Actual source/destination resources and placed leases still survive the real copy/use fence. Failed sources never authorize continuity. RefreshPreparedShaderBuffers validates the original range/offset/native identity, accepts only the current generation or a fully copied ancestor, rejects retired/recreated mappings even at an identical address, and never allocates/replays stage effects. Immutable UploadArena snapshots keep their original resource/offset/lease; writable use is rejected. Final states preserve existing IA/indirect read bits and the existing separate-UAV index snapshot requirement.

BeforeBind completes restoration/materialization before final selection, retains final allocations/stream leases, rebuilds raw views and original push byte adjustments/inclusive limits from current offsets, and supplies same-phase selected views to GuestDmaMemory::BindResident. Its strict native-identity, initialized-coverage and SRV/UAV checks remain; no assertion or operation is suppressed. No shader metadata/guest ABI change.

New real graphics controls late-buffer/late-buffer-private force attachment-side preserving growth after initial resource preparation. Earlier B is changed to .25, grown B to .75 while physical CPU B0 remains .5. Resident shader must read .75 (R96); private CPU stream must retain .5 (R64). Actual draw/readback uses original PushData packed byte offsets. The initial fixture required offset8, but native16-byte alignment produced offset20 from old offset4; corrected the independently derived envelope offset, unchanged GPU pixel oracle. Initial failed runs are retained. Envelope fixtures additionally cover two-generation ancestry, substituted-resource/offset negatives, same-address recreation rejection and leased immutable CPU data under a separate B1 allocation. Old complete B/padding/CPU-store/fence oracles remain.

### 0d5b469: late physical B backing, without image resolution

Pinned BufferCache::ObtainBufferForImage chooses resident B only for GPU-modified pages; SynchronizeDmaBuffers uploads CPU-dirty B, not an independent I. A late binding receipt can publish a tiny B source after the initial DMA plan. Rounding it through ordinary GuestBufferAccess::PrepareGpuBacking would call ResolveImages and make I an unintended physical CPU winner. Production adds explicit readonly physical preparation, rather than bypassing a rejected reader or disabling ownership.

Rasterizer::PreparePhysicalDmaBuffers admits only resident, readonly, nonpublishing UAV ranges. It shares PrepareShaderBuffersImpl's range validation, allocation budgeting, active-range admission and preserving growth. Find's physical initialization and RematerializeBufferEnvelope use the mandatory ReadDmaBacking alias reader; failure throws, never falls back to guest CPU access or exposes initial_upload_failed contents. The physical mode does not call ordinary PrepareGpuBacking/CheckBacking for new image-overlapping B. Existing SynchronizePreparedDmaBuffers preserves B1 when CPU bytes match saved B0, uploads actual CPU-store pages, and reads physical alias chunks beneath owned I. Actual GPU-write reconciliation/waits, PageWatch/image protection, memory indices, original dirty spans and fence retirement remain. Normal shader requests continue through their existing path.

GuestDmaMemory::MaterializeResidentBacking plans actual published/faulted legal pages before final native selection. It reserves only missing complete4KiB subpages, coalescing adjacent holes; already initialized full ranges/subpages keep their allocations and receive no extra physical scan. Pending fault publication follows actual materialization. BindResident then performs the existing post-stage CPU-content synchronization and final table/bank binding. No allocation occurs after that final selection; final generation validation checks any preserving growth. This covers late attachment/converted/BC/opaque B footprints that have been published and have legal physical backing. It does **not** close earlier opaque shader-stage preparation or image interpretation/ordered transfer guards.

Tracked native-port/physical_dma_backing_fixture.h is included by dma_shader_test.cpp, whose existing CMake target already links guest texture operations. Actual shared guest/physical aliases use PAGE_NOACCESS, a distinct GPU I image and a tiny preceding GPU B1. Full/partial logical mappings check complete B0 tails, B1 preservation, unchanged I1 texels/native identity/ownership and physical B0, retained NOACCESS protection, inaccessible-subpage negatives and generation continuity. A rejected physical reader must fail without CPU/image fallback or exposing a failed mapping. A repeat with the reader forced to reject proves initialized backing is not read/materialized again. Image ownership callbacks are synthetic here; actual protection, GPU image/B copies and readbacks are real. This is not whole-core opaque pipeline acceptance.

### Exact verification/build identities

H/NATIVE_VERIFICATION_0d5b469.json records nine PASS fixtures with EXE/compiler/log hashes and immutable per-binary archives H/native-fixtures-0d5b469. Envelope1.26s, DMA9.07s, late-growth3.87s, late-private3.83s, readonly indices3.86s, UAV-write indices3.88s, independent I/B3.88s, buffer-view lifetime.30s, scheduler lifetime.42s. New physical controls run inside the DMA fixture alongside prior real ReadConst/address/fault/table/unmap/CPU/ring tests. No full suite or unchanged broad page-ownership rerun. Relevant logs are archived beside their own EXEs; unique complete raw logs also remain in F:/Codex-Shared/devtools/logs. Wrapper command-20261017-053517-57696 is the current DMA CTest PASS;052901-40940 and earlier979 logs are preserved as earlier binary evidence, not current acceptance.

The regular fixture Mesa DLL has hash18bd24c137cc8e4bcadf15dc83b0f71806f0bf4c73be129776e638220dd6cbbf; the core runtime's converter has hashfedbdf3662ef81f26cd3c73cb0d85cc9420fffeb92dede1256440d7cef48eeab. Both metadata records specify pinned Mesa b628375f and patch180e873a...; identical metadata is not proof of identical binary behavior. DXC/validator DLL hashes match. A separate9.292s run of the preserved current DMA EXE with the **exact runtime converter/DXC** passed (command-20261017-054003-50808.log, H/dma-exact-runtime-converter.txt; archived additional_runs record). Expected tiny1024-byte ownership-rejection notices are fixture fallback/negative diagnostics, not an application crash. Compiler DLL inputs are archived separately; no result was silently transferred between compiler identities.

Primary Release compiled/linked at0d5b469, log command-20261017-053707-50668.log exit0. Separate clean committed checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016, log command-20261017-053707-21836.log exit0 in23.35s.54 pinned dependencies,2283 compile inputs, zero primary-source inputs and empty clean checkout status. H/RELEASE_PROOF_0d5b469.json plus release-identity.json record commands/toolchains/dependencies/files. Complete current external runtime **F:/PS4Dev/handoff/attachment-backing-20261017/artifacts-0d5b469**, EXE **abfa3efbf468e2e6ce24e001e172e4842e602e0f818da658ff07c9daa98a151e**, PDB **4a83d098e0106176d831a306c3bafb4879145e7b2a592a1901e57d08060fe4e0**, native-compiler/licenses included. No current core/game/world/input/FPS/whole-renderer acceptance; fixtures prove only their listed mechanisms. Earlier935fb69 startup exposure is historical and is not transferred to this EXE. Do not auto-install/select a core. No new performance metric; retained Vulkan/DX12 baselines remain authoritative. Previous launcher diagnostics staging is unchanged by this continuation; inspect current app/files before any future application, never stop the user's app/session.

### Exact remaining work / next command

129 implementation/behavior rows and nine gap IDs remain; no whole group is closed by this work. Final-view stability after **preserving** growth and late physical DMA materialization are connected/tested subcontracts. Direct Find can still reject GPU-only overlap; incompatible native SRV/UAV splits and inactive GPU-only B pressure backing remain. Private original IA/index streams and SRV-only auxiliary DMA unification remain. Full ordered opaque depth/stencil I/B transfers, converted/BC stage domains without shared native geometry, shifted/clipped ranges and the other eight gap IDs remain.

**Next C++ task:** ExecuteGuestResources' initial opaque_dma branch still calls rasterizer.ReadMemory on planned DMA ranges, and the following ordinary PrepareShaderBuffers batch calls PrepareGpuBacking for them. Separate those raw DMA requests' physical-B intent from ordinary stage buffers; use the new physical preparation without losing stage BindBuffers/BindTextures ordering, whole-page initialized coverage, original CPU streams, selected generations or actual writes. Keep the ordered opaque-domain guard until its transfer/ownership adaptation is implemented. Exact entry points: guest_compute.cpp opaque_dma/initial dma_count spans, guest_stage_bindings.cpp dma_ranges/retention, rasterizer.cpp PrepareShaderBuffersImpl and SynchronizePreparedDmaBuffers, guest_buffer_access.h::PrepareGpuBacking (ResolveImages), GuestDmaMemory::AppendRequests/MaterializeResidentBacking. Extend the existing real NOACCESS/independent-I fixture and guest shader control, do not weaken image-resolve/alias guards speculatively.

Start with `git -C F:/PS4Dev/work/shadps4-dx12-src status --short` and current HEAD/upstream, then only those symbols. Reuse existing pinned build trees, clean committed Release and exact per-binary regression identities. No repeated129-group audit, original crash replay, completed full-copy/bounds/mip/identity audits, extra game launch or blanket tolerance/expected-byte edits. Full suite and minimal final XInput/game/real-frame FPS comparison remain at the implementation boundary. Goal stays active until all applicable gaps and verification are resolved.


## Authoritative crash and narrow-GPU continuation — 2026-10-17

### Policy, source and preservation

Latest user authorization replaces the old prohibition on all runtime tests: build/link, narrow fixtures and real GPU/readback checks are allowed now. Full suite/expanded gameplay remains final. Exactly one original-binary diagnostic reproduction and one bounded post-fix startup check were performed. No additional game launch is needed for this checkpoint. Never stop/restart the user's app, game, HUD, monitor or remote session. No XInput feeder/device was acquired in these checks.

Branch `fix/mixed-cmask`, production HEAD `8fb9c79ea55c8a71d5befebffa0797de98f59523`. Local source commits:935fb69cad8f9b0afb695ef6b414a8f15b6cc50f (writer ownership/fatal capture), db0812c (integer clears),8fb9c79 (late image DMA backing). Pinned readonly original remains e0a466753a1aacbc8db8253be685bf7ab4d9daae at F:/PS4Dev/work/shadps4-original-e0a46675. Full port remains unfinished, not an accepted Bloodborne build. Check actual Git HEAD/upstream because the documentation checkpoint follows these source commits. Preexisting untracked `.serena/` is navigation state, excluded from commits.

Evidence root `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z`; pointer F:/PS4Dev/handoff/active-breakpoint-crash-evidence.txt. Original before-Git snapshots, current-development.patch/new source backups, later checkpoint-before-docs and source-commit files preserve development independently of the replay. No resets, discarded sources, original-profile modifications or binary/profile/dump commits. Original Vulkan and all earlier runtime/baseline directories are retained.

### Proven crash and scope of correction

Original user launch manifest was preserved before reproduction: local PS4Desktop launch-manifest.json, PID51784, start2026-10-16T23:39:05.3274144Z; source d01969e9e7c5cb5336b3937f4f2c9aa85b35f768; original EXE F:/PS4Dev/handoff/semantic-buffer-residency-20261017/artifacts-d01969e/shadPS4-DX12-Core.exe; SHA256 a7a8e296a17ea9cc96b3a338a87ebf968a5c87b15b8f689461a481f6b1fa87c9. H/exact-runtime includes matching PDB/compiler/licenses and file identities. H/launch-manifest.json and original-logs preserve the original profile identity/logs. H means the evidence root above. H/replay-profile is a copy; original user/log and saves were not changed.

Installed CDB1.2610.1001.0, hidden console, `-g -G` skips debugger initialization. H/debugger-command.json records argv/cwd, H/debugger-process/result.json records the owned run. Original replay PID70680, fault thread0xbce0, runtime_error thrown by NativeDX12::ExecuteGuestBufferCompute+0x1331e. The actual application breakpoint at2026-10-17 04:46:33.899 UTC+5 was `assert_fail_impl+0xcc`, int3 at0x70000007718c, through Liverpool::Task::promise_type::unhandled_exception -> UnreachableFail. This is distinct from the initial debugger break.

Recovered original exception object's message from H/application-exception.dmp: **Independent image version lost its writable buffer ownership before execution**. It is the BuffersMayDefer contract after prewrite image snapshots, not a presumed SRV/UAV overlap or missing-game-file warning. H/cdb.log, application-exception-context.txt, cpp-first-chance-contexts.txt, dump-exception-analysis.log retain the actual stack/registers/exception; dump `/mip` is5,619,957bytes and supports offline `.ecxr`. Earlier caught out_of_range envelope checks are not the fatal exception.

Original dump retains shader metadata (hash42f2a521, seven buffers including one written formatted Guest buffer and written GDS), but the unwound request vector and actual flattened sharp words are not captured. The exact individual rejected Guest range is therefore **not established from the original dump**. GDS alone is not proven causal: MarkDirty explicitly preserves GDS ownership. Do not rerun failed heap recovery or invent a shader/address/game workaround.

Production935fb69: Rasterizer::MarkShaderBufferWrites validates only existing selected mappings and exact WriteRange; pins all written mappings if restoration is needed, calls PrepareBufferContents before declaring writes, and reestablishes current B0/watches on the same allocation after prewrite RO/Finish downgrade. Original contents/visibility/exact dirty bytes, native view identity, resource states and fence lifetimes remain. No assert suppression, draw/dispatch skip, fake success or removal of real CPU visibility waits. BuffersMayDefer logs exact rejected range/watch/ownership and flushes if any other case remains. New envelope fixture actually forces the ownership loss, checks negative rejection, restores the same resource, writes GPU bytes/readbacks full8KiB, retains neighbor B1 and defers physical B until an explicit4-byte CPU request. The earlier harness prematurely expected a CPU commit after GPU-only Finish; corrected call sequencing, unchanged data oracle, failure log retained.

Postfix935fb69: exact clean EXE887fdbb239302be901c7df6a90752390e3204db5eca16e6182c9fd82767f6506, PDB4af0b89b0af669658fce12d20746818659ca28b52bcc4da2bba1edb6b6df8398, H/artifacts-935fb69. H/postfix-935fb69 contains fresh copied profile, command/process/result, flushed redirected streams, preserved-final-logs and INTERPRETATION.json. No observed application breakpoint/unhandled exception for120s; debugger tree stopped at deadline (`timeout:true`, debugger exit1 is the controller stop, not a spontaneous game crash/success). Normal handled PageWatch AVs continue; no world/pixel/control/FPS claim. This evidence narrows the crash, does not prove whole-game correctness or apply to later rebuilt EXEs.

### Further production fixes and actual GPU checks

db0812c: RecordGuestStorageImageClear/GuestStorageImageClearLease now clear SINT through the UINT view of the same typeless family and explicitly cast integer union bits to native component width; raw guest R32 bits remain intact. Real readback previously returned R8_SINT -1 for representable -15; original expected -15 is unchanged. A UAV barrier precedes clear as well as the existing following barrier, preserving ordering even when the UAV state does not change. Pinned upstream Runtime::ClearImage at vk_runtime.cpp:578-600 uses typed vkCmdClearColorImage over the complete subresource range, retaining its transfer-write ordering. No CPU/download/operation bypass.

The initial normalized mismatch127/128 is **a fixture oracle issue**, not evidence of broken GPU contents. [Direct3D functional spec3.2.3.4/6](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#3.2.3.6%20FLOAT%20-%3E%20UNORM) allows0.6 integer ULP, and [Vulkan fixed-point conversion](https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-fixedconv) does not mandate a single half-tie value. The nearest-up expected vectors remain; only the corresponding logical half-tie components admit the other nearest encoding. Complete16-bit components are compared together, no bytewise carry mixtures or blanket +/-1 tolerance, padding/RGB/other texels remain exact.126/129 negative controls remain rejected. [Vulkan clear values](https://docs.vulkan.org/spec/latest/chapters/clears.html#clears-values) defines integer casts/negative preservation; out-of-range SINT is undefined, not a new emulator requirement.

8fb9c79: PrepareOrderedGuestStages receives explicit dma_active, predicts domains' possible future B residency with GuestDmaMemory::AppendRequests, adds nonpublishing complete legal64KiB/4KiB physical reservations before final views, and includes rounded ranges in independent-I retention. Existing backing is grown through the already preserving B algorithm. Original stage BindBuffers/BindTextures still determines residency and masked I->B transfer; prediction/private CPU streams do not publish. Final BindResident still validates initialized coverage/native identities and retains actual allocations. This closes late tiny B sources in **ordered common native I/B domains**, not every attachment/converted/BC/opaque path or pressure-eviction case.

New tracked native-port/late_image_dma_fixture.h is compiled into native-dma-shader-test. Real production ReadConst -> DXIL -> GPU dispatch checks full/partial legal backing, old tiny GPU B1 preservation, complete physical B0 tails, original private CPU stream, inaccessible neighboring subpage, actual binding publication and unchanged selected resources. The two initial fixture failures lacked synthetic cache retention callbacks; harness callbacks now match its documented content/table-only scope. It is not a substitute for real PageWatch fixture coverage. Existing missing/tiny coverage, incorrect native identity and SRV/UAV rejection controls are retained.

Six PASS identities are authoritative in H/narrow-regressions-8fb9c79.json; their exact EXEs/PDBs retained in H/narrow-artifacts-8fb9c79. Native guest_buffer_watch_transfer1.21s (envelope, semantic B, image-source receipt, restored writer), native_dma_shader_addressing9.41s (new late sources plus existing address/fault/table/CPU/unmap/ring controls), native_independent_image_version_transfer3.85s; exact unchanged earlier buffer_view_lifetime and scheduler_lifetime binary/results retained. native_gpu_page_ownership34.08s covers actual fault/write watches, typed/converted clears/mips/layers/volumes/padding, independent versions, invalid-I pressure and gated allocation lifetime. Its20s timeout cut off later controls; changed to90s, passed. Raw logs in F:/Codex-Shared/devtools/logs and named H/*.txt, earlier failures retained. These are GPU checks, not /Zs. Full suite not run. No new FPS/CPU-wait/frame-time/performance baseline exists; reuse saved prior Vulkan/DX12 metrics below.

### Reproducible runtime and launcher

Source `8fb9c79ea55c8a71d5befebffa0797de98f59523` primary Release log command-20261017-051557-58796.log exit0. Separate clean checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016, clean committed detached source, Release log command-20261017-051558-7708.log exit0.54 pinned dependencies,2283 compile inputs, no primary source input. H/RELEASE_PROOF_8fb9c79.json, native-port/vendor/release-dependencies.json and release-identity.json record toolchains/commands/dependencies/files. **Current external complete runtime `F:/PS4Dev/handoff/breakpoint-crash-d01969e-20261016T234433Z/artifacts-8fb9c79`; EXE `3e822bf2a7c46fcc8c8c7dc242c36df4ba895f65ce8dbd7ae03be1032f5a0fab`; PDB `765be1db16d597c59bd40f376e511e8a763d13dd0fd4cdbe18893dfb8c6a8e09`.** Native compiler/licenses included. Exact core runtime/game verification not_run: only the earlier935fb69 binary received the120s startup check. Never transfer that result or native fixture acceptance onto this rebuilt core. Do not auto-install/select any core; user manual folder selection remains authoritative.

Fatal Critical messages now synchronously flush to stderr and file before int3. desktop/LaunchDiagnostics.cs captures raw stdout/stderr including unterminated text, immutable per-launch manifest/profile logs, signed/hex exit and exit-report.json, with post-exit pipe deadline/error records. Program observes exit and displays understandable error plus report path. Normal captures live in Settings.DataRoot/launch-diagnostics/<UTC>-PID-GUID. Production/core and C#clean Release built; both primary/clean `--launch-diagnostics-test` passed. H/desktop-935fb69 is the staged complete app output.

The active installed PS4Desktop DLL denied atomic replacement (WinError5); user process was not stopped. Both original DLL/PDB hashes remain installed as a pair (DLLddc452b62fa86196661e3d2774ff45aa9e260104cc34f959bb81e78cdf7f748c); old backup H/desktop-before-diagnostics. New DLL/PDB are staged beside installed files as .codex-diagnostics-pending, new DLL52eaeded686b22036a880ded9ecb498aa400c05889eff8efb47ff8edb16ba60a. H/desktop-diagnostics-install.json records actual hashes and limitation. H/APPLY_LAUNCHER_DIAGNOSTICS.ps1 applies after the user normally closes that app; it refuses to stop an active process. No installed GUI acceptance claim and no core replacement.

### Exact continuation / do not repeat

129 implementation/behavior rows and nine gap IDs remain. Executed subcontracts have partial_gpu_verified evidence bound to exact native test hashes; final group acceptance is not implied. Existing original clock/blend adapter conditions, pinned original exclusions and baseline history remain unchanged. No new129-group audit, retile/envelope/mip/bounds audit, duplicate original-binary replay, failed freed-heap recovery, per-commit game runs, broad test reruns after unchanged EXEs, or changed expected GPU bytes to match implementation. Valid tiled-I pressure exclusion is upstream behavior, not a new port requirement.

Next source work: **pre-view source-footprint/backing preparation outside the connected ordered common native I/B domains**, specifically GuestRasterizer::Draw `submit_replay` (guest_rasterizer.cpp:1254 onward), texture B-source publisher (line122) and ExecuteGuestResources' final BeforeBind. Handle attachment-only source reservations before shader views, then converted/BC/opaque receipt footprints, preserving distinct I/B, original semantic publication and selected views. Keep identity guards until this is connected. Direct Find growth, mixed SRV/UAV split, inactive B pressure backing and original private IA/SRV-only auxiliary unification remain subsequent concrete changes in the formatted-alias-ordering gap; other eight gap IDs unchanged.

Start command: `git -C F:/PS4Dev/work/shadps4-dx12-src status --short`, then read this section and only those source symbols. Use existing build-release.ps1/toolchains/incremental trees, meaningful narrow GPU readback controls, clean committed Release and per-binary identities. Continue production C++/HLSL; final broad suite and extended XInput acceptance only after the remaining implementation boundary. New app diagnostics remain staged until safe normal app exit.


## Image-source bind publication checkpoint - 2026-10-17

Current source checkpoint `2498c6fafe567e13a6b8fe71937aa967a1632b9b` on fix/mixed-cmask, following b48fcc9/d01969e. This section supersedes older current/next sections below; all previous implementation/runtime evidence is preserved.

Production `2498c6fafe567e13a6b8fe71937aa967a1632b9b` connects version-specific image B-source residency: prewarm/capture reserves physical backing and retains deferred effects; actual image binding consumes them, including inherited independent I conversions and prewrite shader/depth/stencil sources. Publication preserves retry state, discards overwritten-version effects, checks existing source backing and does not replace native views. Primary and separate clean committed Release linked; six fixture TUs passed C++ /Zs in each checkout. All new runtime/shader/GPU/game/XInput/UI/FPS acceptance remains not_run.129 independent implementation/behavior records and nine gap IDs remain. Latest complete external runtime: F:/PS4Dev/handoff/image-source-publication-20261017/artifacts-2498c6f. Do not auto-install/select. Next: complete legal tiny DMA backing before final selected views, preserve GPU B/witnesses/fence leases, then the existing source-gap queue. SESSION_HANDOFF.md is authoritative; older sections preserve history.

Production changes: GuestTextureCache::Upload/UploadVersion now distinguish UploadPurpose::Preparation from Binding. Entry.pending_buffer_residency belongs to the precise prepared I version; SetIndependentBlockExpansion returns BlockExpansion containing its private GPU snapshot and the target B footprint only when original IsGpuBufferModified selects B. The callback uses CaptureShaderBuffer(...,false). UploadVersion propagates inherited pending effects through private I conversions before retiring the source, without a recursive bind consuming them early. Preparation reuse retains effects; actual Binding and explicit PublishBufferResidency consume them. Publisher failure leaves receipts intact for retry; actual content replacement clears stale prior-version receipts. BeginWritableBlockAlias consumes an actual bound parent's pending effect before its private I child. RefreshBoundImage transfers pending provenance to the shared native allocation.

GuestRasterizer::SetBackingAccess configures the publisher to validate complete current PreparedDmaBuffer coverage for each receipt, then only ObserveResidentSharps; no Find/allocation/resource replacement occurs at publication. Ordered stage/nonordered storage prewarm now use Preparation explicitly. ExecuteGuestResources freezes prewrite B-source selection before CPU resolution, captures GPU sources without publication, adds missing complete physical CPU fallback source envelopes before final buffer views and publishes at the actual shader image use. Masked ordered-transfer image refreshes also publish there. Depth/stencil prewrite capture records both source planes, reserves nonpublishing full source footprints before shader views, and publishes at attachment binding. Metadata/depth feedback early uploads are Preparation. Existing original memory ownership, RO/NOACCESS proofs, actual-write markers, cache pins, copied resource leases, selected-view identity guards and shader ABI remain.

Prepared verification: native-port/image_source_residency_fixture.h, included by semantic_buffer_residency_fixture.h / guest_buffer_access_test.cpp, prepares actual cache uploads and repeated reuse with a deferred source receipt. It checks no early publication, one-shot binding, failure/retry preservation, and dropping a prior version receipt on actual overwrite. independent_version_transfer_test.cpp adds inherited receipt assertions around its existing native private I conversion and retains its pixels/physical B padding checks. An included preexisting MSAA fixture variable named small collided with a Windows macro; only that local name changed to smaller. No fixture was run. Primary and clean alias, GCN, DMA, buffer-access, page-owned and independent TUs passed C++ /Zs only. The initial independent /Zs failure and corrected result remain in the local logs.

Build evidence: primary Release final log F:/Codex-Shared/devtools/logs/command-20261017-044124-44180.log, exit0. Separate checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 is detached at2498c6f and full porcelain status empty before/after. Its pinned configure/Release log command-20261017-044159-8492.log exited0 in21.153s.54 pinned dependencies,2283 compile-command inputs, zero primary source inputs. Six clean syntax logs command-20261017-044246-60080/49736/8560/14024/45936/49756.log all exit0. Primary syntax logs are in RELEASE_PROOF.json. No test process, shader conversion, GPU workload, emulator/game/XInput/UI launch, FPS measurement or acceptance occurred.

Complete immutable external runtime: F:/PS4Dev/handoff/image-source-publication-20261017/artifacts-2498c6f. Contains EXE/PDB/native-compiler/licenses/release-identity.json. EXE SHA256 `084a44512749842780bef446bbaa5052a2d97f86e6e5521be023fa2788ba3c45`; PDB `6c9bc076c87d8caa00cbfed4de032b5696b63e4955c3fb2dd90606c9f4b764b8`. RELEASE_PROOF.json records all18 artifact file hashes, source commit and compiler logs. The handoff root preserves before HEAD/status/staged/unstaged patches and all nine changed source files with hashes and production.patch. All previous artifacts/evidence remain. Do not auto-install/select; existing app folder selection remains manual. Original Vulkan/saves/profiles/desktop/app selection were untouched. Preexisting untracked .serena/ navigation files are excluded from Git.

Concrete remaining work: tiny real late sharps can publish a64KiB semantic page while only a few bytes of native B exist, so final BindResident correctly withholds incomplete subpages. Image receipts likewise require complete current backing through intervening pressure/reservation; the new publisher rejects missing backing rather than allocating after views. Complete legal4KiB/64KiB backing earlier, after prewarm has disclosed actual possible B sources and before shader/DMA/IA final views. Use existing IsDmaReadable legality, preserving compatible RematerializeBufferEnvelope growth and nonpublishing requests. Retain old B1 tails, CPU B0 snapshots, actual-store pages, borrowed RO/NOACCESS witnesses and real source/use fence leases. Handle page-sharing native SRV/UAV compatibility through actual binding intent, not stale unused envelopes. Do not simply round every host temporary or change cached logical accounting/stream behavior. Keep original sharp/view/write limits; inaccessible VM subpages stay absent. Final selected identity/SRV/initialized-coverage guards must remain; no late partial re-Prepare after descriptors.

Next source action: inspect the existing PrepareOrderedGuestStages pre-view request batch and ExecuteGuestResources final buffer-views boundary, plus GuestDmaMemory::AppendRequests/PlanRanges, and add a reusable nonpublishing backing plan for actual/potential guest residency before their final PrepareShaderBuffers. Direct Find overlap, incompatible native S/U splitting and inactive B pressure backing still require data-preserving residency; original private IA stream semantics, auxiliary unification, opaque depth/stencil and converted/BC/shifted/clipped domains remain in the same nine-gap queue. No full-port, complete sparse-arena or runtime parity claim. Valid tiled I GC exclusion remains the pinned rule.

Do not repeat the129-group/general Vulkan audit, completed semantic-publication split, invalid-I pressure election or complete old B envelope preservation work without a new failure. Reuse pinned e0a46675 and saved Vulkan/DX12 metrics. Continue source work/builds, reproduce committed Release, update separate implementation/behavior records and normal-push. Runtime/shader/GPU/game/XInput/UI remains deferred until the full agreed implementation boundary.

## Semantic B residency and resident-only retile checkpoint - 2026-10-17

This section supersedes older current/next entries below. Production source commit: `d01969e9e7c5cb5336b3937f4f2c9aa85b35f768` on `fix/mixed-cmask`. The prior checkpoint is98090af; all earlier evidence remains preserved.

Production `d01969e9e7c5cb5336b3937f4f2c9aa85b35f768` separates shader-buffer semantic residency from physical DEFAULT preallocation, publishes original buffers in bind order, and masks formatted I-to-B copies to the pages resident at that point. Clean small shader reads use private full DWORD envelopes; host/domain reservations and private I conversion captures do not publish pages. Final refreshed images consume masked B, with selected native states restored. Primary and clean committed Release linked; five fixture TUs passed C++ /Zs in both checkouts. All new runtime/GPU/game/XInput/UI/FPS results remain not_run. Full port unfinished;129 implementation/behavior records and nine gap IDs retained. Latest full external runtime: F:/PS4Dev/handoff/semantic-buffer-residency-20261017/artifacts-d01969e. Do not auto-install/select cores. SESSION_HANDOFF.md is authoritative; older sections preserve history.

Implemented: ShaderBufferRequest gains independent publish_dma_residency intent. GuestDmaMemory collects predicted original sharps and completed feedback for physical planning without prematurely committing future shader pages. Fault pages publish after legal backing is prepared. ObserveResidentSharps honors the intent;64-bit native fixture page numbers remain separate from the40-bit shader table domain. ResidentIntersections clips/merges only currently semantic pages with overflow-safe native addresses. CopyResidentImageToPreparedShaderBuffer preserves source offsets and unresident B tails, retains source/destination leases before recording and does not mark formatted copies as new guest writes.

PrepareOrderedGuestStages reserves physical buffers/private streams/domain envelopes without publication, then publishes original nonstream buffers immediately before their ordered metadata/retile operation. Image-source B snapshots in its explicit refresh branch publish at that image bind. ExecuteGuestResources freezes the original clean-small classification before later host prewarm side effects; all clean small shader buffers bind private complete DWORD envelopes retaining byte adjustment and separate native SRV/UAV classes. Null/special temporaries do not publish. The single-stage independent formatted transfer records all reader indices and applies each resident-only retile in original buffer order. Exact-base refresh/mip-growth snapshots use final masked B instead of an unmasked private I result. States are restored by selected native resource before descriptors. BindResident guards actual bound SRVs/private replacements rather than unused original envelopes. Private independent-I transfer captures are nonpublishing; clean small IA requests carry nonpublishing intent but private IA data binding is still unfinished.

Limits/next concrete C++ work: GuestRasterizer's actual B-source seed callback and ExecuteGuestResources prewrite GPU snapshot paths can still run during prewarm before the original image bind; separate physical capture from semantic publication for those paths without dropping actual ObtainBufferForImage residency. Then plan complete legal4KiB/64KiB DMA backing for tiny late real sharps before final views exist, using the preserving compatible envelope growth from558baa3/f16630d. Retain the final selected identity/coverage/SRV guards: do not re-Prepare buffers after descriptors. Direct Find overlap, incompatible SRV/UAV native split and inactive B pressure backing still need GPU-preserving solutions. IA clean-small inputs need original private data semantics as well as nonpublishing intent. Opaque depth/stencil, converted/BC/shifted/clipped domain paths and the existing nine-gap queue remain. No full sparse-arena, image callback phase or runtime parity is claimed. Valid tiled I GC exclusion is pinned behavior, not an extra feature request.

Prepared checks: native-port/semantic_buffer_residency_fixture.h is included/instantiated by guest_buffer_access_test.cpp. It reserves two native pages and captures B without semantic publication, then checks first-page-only retile and untouched B0 tail before later second-page residency, subsequent full retile, retention under private requests, merged/clipped original spans, top64-bit addresses and overflow rejection. dma_shader_test.cpp now checks prediction and physical preparation/private capture do not publish before the actual sharp, followed by its existing full/partial/tiny GPU/identity controls. These fixture bodies are only compiled/typechecked; none executed. Existing alias/GCN/DMA/buffer-access/page-owned source controls passed /Zs in primary and clean checkouts. No shader conversion or emulator/game launched.

Build proof: primary incremental Release final log F:/Codex-Shared/devtools/logs/command-20261017-043355-24148.log, exit0. Separate checkout F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 is detached atd01969e and full porcelain status empty before/after; pinned Release configure/build log command-20261017-043421-53428.log, exit0,26.735s. It used54 pinned dependencies; compile_commands.json contains2283 inputs, zero source inputs from the primary checkout. Five clean /Zs logs command-20261017-043439-55264/54180/52404/32492/66256.log all exit0. Primary syntax logs and the corrected initial undeclared-member error are retained locally; the error was fixed before the source commit. None of these are runtime verdicts.

Full external artifact: F:/PS4Dev/handoff/semantic-buffer-residency-20261017/artifacts-d01969e contains EXE/PDB, native-compiler, licenses and release-identity.json at the source commit. EXE SHA256 `a7a8e296a17ea9cc96b3a338a87ebf968a5c87b15b8f689461a481f6b1fa87c9`; PDB `aeda385989c7fecf550e96b9044cd85d32da9850ca3f8e437ede47a9a57c447e`. RELEASE_PROOF.json records all18 artifact files, hashes and logs. The handoff root preserves before-HEAD/status/staged/unstaged patches, all ten source files with SHA manifests and production.patch. Earlier runtimes remain. Core is uninstalled/unselected/unrun; the existing manual folder selector remains the only installation route. Original Vulkan/saves/profile/desktop and current app selection were not touched. Only preexisting `.serena/` navigation state is excluded from Git.

Next exact action: open src/video_core/renderer_d3d12/guest_rasterizer.cpp around the B-source upload seed callback and the callers in guest_stage_bindings.cpp prewarm / guest_compute.cpp prewrite snapshots. Carry capture/publication phase explicitly so private preparation is nonpublishing and actual image binding publishes the full original footprint. Reuse this checkpoint's pinned upstream facts, masks, compiler commands and saved metrics. Then finish tiny late backing before final views, build/reproduce from source commits, update registry and normal-push. Do not repeat129-group/general Vulkan audits, valid tiled GC eligibility experiments or existing envelope-growth work without a new failure. No runtime/UI/GPU/game/XInput/shader conversion until the full implementation boundary is ready.

## Preserving B envelope growth checkpoint - 2026-10-17

This is the current implementation/build/continuation section; older current/next entries below retain history.

Production558baa3/f16630d preserves complete preceding GPU B ranges, old CPU snapshots, forced CPU-store pages and RO witnesses across compatible batch envelope growth. Only new holes initialize from physical B; old allocations survive the copy fence. Existing actual guest-write reconciliation remains. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only, no new runtime/GPU/game/XInput/UI or FPS result. Latest full external runtime: F:/PS4Dev/handoff/buffer-envelope-preservation-20261017/artifacts-f16630d. Never auto-install/select it. Full port remains unfinished: semantic DMA residency publication, tiny full-subpage materialization and stable final views, direct/split/pressure B residency, auxiliary SRV unification, opaque depth/stencil and the existing nine-gap queue remain.

Production:558baa3912eb52bf19f7336ad3be5065615fd487 andf16630df085e70bedb9e3c299bdc9a4727c27301. PrepareShaderBuffers expands compatible active envelopes to full old allocations, uses mapped aligned prefixes where legal, reserves net new logical bytes and protects all replacement sources during budget admission. RematerializeBufferEnvelope retains the existing ResolveGpuBuffers/Finish boundary for actual dirty guest writes; it does not label readonly formatted B as new guest writes. It initializes new holes from physical B, GPU-copies full old B including tails, separately inherits CPU B0 snapshots, forced-store page markers and proven/unknown RO spans, and restores each source's recorded resource state after copying. Old/new placed allocation leases are registered before commands and survive the real copy/use fence. Only then are index/map nodes replaced without RetireBuffer/release_buffer_ opening an RW gap. The new target is prepared as a preserved version (fresh=false), so a changed CPU page cannot cause a whole-buffer B0 overwrite of unrelated B1.

Provisional target tracking records actual same-value CPU stores during the watch callback, even outside the old tiny sharp. Current source mappings are pinned against reentrant drop/pressure clear during content preparation and witness acquisition; Find/PrepareShaderBuffers cannot reenter materialization from that witness phase. Clean owned reservations with no dirty readback now relinquish ownership during CPU resolution; complete RO downgrades clear old partial-proof holes. ReadDmaBacking has a legality-only host fallback for standalone callers; the guest core retains its physical alias override and no qualified bypass is used after that reader rejects a GPU-only version.

Limits remain explicit: direct Find overlap, incompatible native SRV/UAV splitting and inactive GPU-only B budget discard cannot recreate B1 from CPU B0. Those paths now preserve data or reject; bounded pressure backing/stable residency is unfinished. The placed allocator still enforces physical memory budget including retained source leases. No sparse-bank equivalence, no removal of actual-write CPU waits, no full late-descriptor stability or FPS acceptance is claimed.

New tracked native-port/buffer_envelope_fixture.h is instantiated by existing guest_buffer_access_test.cpp: eleven source-authored scenarios cover RO B1, partial actual guest writes, clean owned reservations; full old prefix/tail and bridged B versions; fresh physical gaps; same-value CPU stores before/during guard acquisition, including outside a tiny old mapping; exact CPU commit sizes/bytes, native view bounds, RO continuity and allocation-fence lifetime. A negative control rejects reentrant resource preparation. These are C++ /Zs checks only, not linked/executed GPU tests. All existing alias/GCN/DMA/page-owned fixture sources remain retained.

Build proof: primary Release command-20261017-041346-43584.log exit0; separate clean checkout detachedf16630d, full git status empty, pinned54 dependencies,2283 compilation records and zero primary source inputs. Final clean Release command-20261017-041513-45204.log exit0 in17.901s. Clean buffer-access /Zs041523-33580 passed atf166. Alias/GCN/DMA/page-owned typecheck logs040601-49828 /040601-43944 /040601-36976 /040601-52144 passed at558; their TU/header inputs are unchanged byf166's rasterizer.cpp and buffer-access-only fixture correction. Raw logs: F:/Codex-Shared/devtools/logs. No fixture link, conversion, GPU validation, game, desktop, XInput or profiler run. Earlier runtime evidence and Vulkan/DX12 baselines remain authoritative.

Full artifact F:/PS4Dev/handoff/buffer-envelope-preservation-20261017/artifacts-f16630d: EXE SHA2563b5b4417831e53045003ffa8f8af42a72bb3dca997700c01203b48716f0c7808; PDB27d2a1b90065c9ff77bff4ff59b566087896cc4550dcab81ad39e343700c9555; native-compiler/licenses/release-identity included. The handoff root has original staged/unstaged/status/HEAD snapshots, new-header backup, precommit source patch, correction patch, complete manifests and RELEASE_PROOF.json. Full558 artifact/proof and all earlier runtimes remain preserved. No core installation/selection, user save/profile or original Vulkan change. Primary .serena/ is preexisting untracked navigation state.

Next concrete source task: distinguish semantic BDA residency from host materialization before completing tiny DMA pages. Pinned ObtainBuffer194-211 bypasses EnsureResident for clean small CPU streams. OrderedGuestStages currently retains original CPU-stream requests and adds private stream/domain reservation requests to the batch; PrepareShaderBuffers observes all DEFAULT requests at emit-buffer-views. A preallocated physical B range alone is not proof the original called EnsureResident. Introduce explicit publication intent/phase for original resident guest sharps versus CPU streams and host domain reservations; retain already resident pages and actual fault feedback, do not hide a genuinely resident page. Then complete legal64KiB/4KiB backing for newly materialized guest sharps before final views, using preserving growth above. Table queries must never expose an uninitialized subpage. Reserve all possible image/attachment B geometry before selecting descriptors or refactor final native descriptor selection without replaying semantic I/B stage effects. BindResident's identity guard must remain until this is implemented.

Exact next files/actions: verify HEAD in F:/PS4Dev/work/shadps4-dx12-src; inspect only guest_stage_bindings.cpp private CPU-stream and domain request construction, rasterizer.cpp emit-buffer-views/ObserveResidentSharps, guest_dma.cpp::AppendRequests/PlanRanges/BindResident and ExecuteGuestResources/Draw materialization boundary. Add prepared cold-first-command DMA controls distinguishing private CPU-stream fallback from actual EnsureResident publication. Aux IA inputs need classification under the same original rule. Do not repeat the completed B copying/proof, bounds, physical image identity, mip-prefix or per-stage ordering audits. Do not add DMA guest stores: ReadConst is read-only; native UAV bank descriptors do not authorize guest DMA writes. Outstanding direct/split/pressure and opaque/converted/shifted/clipped gaps stay in the registry; valid tiled I pressure retention follows pinned exclusion and must not be reopened as a new feature requirement. Runtime tests remain deferred until the full implementation boundary.


## Invalid independent-image pressure checkpoint - 2026-10-17

This current section supersedes older current/next statements below; earlier sections retain historical source/build/runtime evidence.

Production `284be47d1bfacd32bdc8a871759bf6953a1edcf4` connects pressure eviction of invalid independent I without downloading stale pixels into physical B. Actual cache admission shares ReclaimResidentMemory with ReserveMemory; preserve pins, completion, callback identity and budget refresh are retained. Primary and separate clean committed Release linked; five fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution or FPS result. Latest full external runtime: F:/PS4Dev/handoff/invalid-image-pressure-20261017/artifacts-284be47. Never auto-install/select it. Valid GPU-modified tiled I remains excluded from pressure download by the pinned Vulkan rule; older broad statements calling that exclusion a gap are superseded. Late tiny DMA materialization/stable B residency, SRV auxiliary unification, opaque depth/stencil and existing nine-gap queue remain unfinished.129 implementation/behavior records and prior behavior evidence are retained.

Pinned evidence inspected only for this mechanism: e0a46675 src/video_core/texture_cache/image.h104-105 SafeToDownload requires GpuModified && !Dirty; texture_cache.cpp990-1008 skips valid tiled downloads but frees Dirty images without download. Thus valid tiled pressure retention is deliberate pinned scope, not an extra missing feature. Native EvictPressureVictim previously excluded every dirty independent image. It now admits invalid independent I and calls the already registered retire_independent_ owner callback (GuestRasterizer::ReleaseIndependentImageWatches) to release only that I's witnesses/history. It never calls the download/resolve path for this invalid version, and verifies ownership was actually dropped before erase. Existing real B/sibling I protections and CPU-store markers remain owned by their original subsystems. Callback reentry/re-pin and UID/vector-iterator revalidation remain enforced.

ReclaimResidentMemory(target, completed, preserve) is the production reclamation entry shared by ReserveMemory. It releases eligible inactive transfer storage, then uses the same LRU victim election. Normal under-budget admission still scans residency once. Recursive resolver budget changes are checked against the current budget; waits/retries still require changed budget, reclaimed bytes or completed source release. No tight retry or synthetic FPS claim.

New tracked native-port/invalid_image_pressure_fixture.h is included/called by the existing page_owned_gpu_test.cpp TU (no new standalone target or external dependency). Four authored linear/tiled plus callback-repin cases deliberately keep CPU B0, GPU B1 and stale I2 distinct. They check pending/in-flight/pinned/preserve exclusion, pinned valid tiled retention, discard without any download callback, complete B mip/layer/padding retention, unchanged CPU bytes and preservation of an actual same-value CPU-store page marker through later B synchronization. This is a cache/owner-callback contract fixture, not an executed PageWatch/game acceptance claim. All five relevant TUs are C++ typechecked only; no runtime/link/conversion/GPU test.

Build proof: primary Release command-20261017-033258-22452.log exit0; separate clean checkout detached284be47, full git status empty, pinned54 dependencies,2283 compilation records and zero primary-checkout inputs. Clean Release command-20261017-033323-72524.log exit0 in20.4s. Clean alias/GCN/DMA/buffer-access/page-owned /Zs logs033344-16608 /033344-44372 /033344-64296 /033344-60380 /033344-28580, all exit0. Raw logs: F:/Codex-Shared/devtools/logs. Saved Vulkan/DX12 baseline remains authoritative; no new measurement or runtime verdict.

Artifact F:/PS4Dev/handoff/invalid-image-pressure-20261017/artifacts-284be47 contains full EXE/PDB/native-compiler/licenses/release-identity.json. EXE SHA25660c54330fc1e453126469900a3f0661ca47f9204ee9cc0e8ca587a08ee915e10; PDB619d885f5e5a18ed49014814feedd66a515bd2a2f85f8f98c4b8a8245a5824f1. Handoff root retains before-HEAD/status/staged/unstaged patches, new-source backup, complete source manifest/patch, compiler commands and RELEASE_PROOF.json. Previous full runtimes/evidence preserved; no installation/selection, original Vulkan/save/profile changes or desktop control. Primary .serena/ remains preexisting untracked navigation state, excluded from commits.

Next concrete task remains full late DMA residency. Newly confirmed prerequisite: PrepareShaderBuffers retires a partial overlap through DropCached, and Find seeds a recreated mapping from CPU. Existing GPU-only formatted B (gpu_cpu_diverged, not necessarily dirty guest data) and old mapping tails can be lost. Before widening tiny late sharps to complete legal subpages, implement stable physical B residency or equivalent GPU-preserving envelope expansion. Pinned BufferCache::EnsureResident uses sparse arena residency and writes BDA entries for new blocks; do not invent nonexistent upstream Join/CreateBuffer semantics. Reuse that algorithm/identity where D3D12 residency permits, or preserve GPU bytes by queued copies with exact old CPU snapshots, forced-store page markers, dirty ownership and guard transfer. Complete old mapped tails must survive, not just the requested overlap. Do not release RO/NOACCESS proof into an RW window: same-value CPU stores must still fault/be recorded. Old GPU allocations must survive their copy/use fence, and current selected views must stay stable. Do not remove BindResident's identity/initialized-coverage guards or rerun partial PrepareShaderBuffers under final descriptors.

Exact source continuation: src/video_core/renderer_d3d12/rasterizer.cpp::PrepareShaderBuffers (retire-overlaps/materialize-resident-backing), Find/DropCached/PrepareBufferContents; guest_dma.cpp::ObserveResidentSharps/PlanRanges/BindResident; guest_stage_bindings.cpp prewarm/reservation. Pressure discard above is connected; do not repeat it or broaden valid tiled GC eligibility beyond pinned rules. After stable/tiny residency, complete SRV auxiliary unification, opaque depth/stencil independent I/B, converted/BC/shifted/clipped domains and the nine-gap queue. Do not repeat general Vulkan or129-group audit. Build incremental Release, reproduce source commits in the clean checkout, preserve full external runtime. Runtime acceptance remains deferred until full implementation boundary.


## Late fully resident DMA publication checkpoint - 2026-10-17

This current section supersedes older current/next entries below. Those entries preserve source/build history and earlier runtime evidence.

Production `7cc493f72c96a0492cc165166987ad4c7f38fa88` publishes fully materialized late DMA pages at BeforeBind: complete64KiB pages or4KiB subpages, exact existing mapping/offset coverage, original selected-view identity guards, no reservation/retile, single CPU-dirty B synchronization and placed-allocation retention. Primary and clean committed Release linked; four fixture TUs passed C++ /Zs only. No new runtime/GPU/game/XInput/UI execution. Latest external full runtime: F:/PS4Dev/handoff/late-dma-publication-20261017/artifacts-7cc493f. Never auto-install/select it. Remaining: complete-page materialization for tiny late sharps, late arena replacement, auxiliary SRV/UAV unification, opaque depth/stencil and tiled independent pressure/retirement, then existing converted/shifted/clipped/nine-gap backlog.129 groups remain separate implementation/behavior records:59 with concrete gaps,70 without a separately established production change. Full port remains unfinished.

Implementation: guest_dma.cpp::PlanRanges is shared by early requested residency and final resident-only selection. AppendRequests retains the existing full-page/rights/backing planning. BindResident runs after all image/attachment preparation, checks that every early DMA view still matches the selected mapping, then admits current complete64KiB pages or each initialized4KiB secondary subpage via Rasterizer::PreparedDmaBuffer. The selector only reads mappings: no Find, allocation, ownership transfer or upload. Consecutive guest spans merge only when the native resource and native offset are also consecutive. Readable tiny candidates remain queued but do not expose uninitialized tails. Simultaneous SRV/UAV backing is explicitly rejected. Actual selected placed memory is retained through ResourceBindings. CPU-dirty B is synchronized once before the existing bank/translation table update; metadata descriptors are written after possible table growth. Original table initialization/zeroing, live-prefix copy, actual capacity, entries patching, fault bitmap/compactor and four-slot fence feedback remain unchanged. Non-DMA binding does no new work. guest_compute.cpp moves DMA banks/table/fault descriptors to the existing BeforeBind hook; buffer-write declarations/constants stay after this preparation. No shader ABI/metadata version change.

Production files: guest_dma.cpp/.h, guest_compute.cpp, rasterizer.cpp/.h. Authored native-port/dma_shader_test.cpp controls plan first then materialize one full page, one tail4KiB and one four-byte-only sharp. They verify first-command full/partial results and both fallback forms for the tiny page, untouched GPU-only B, complete output lanes, rejection of a mismatched old view and SRV conflict, and exact resource/offset retention. Existing adapter controls remain. These checks are only C++ typechecked, not executed or GPU-accepted.

Build evidence: primary Release exit0 (F:/Codex-Shared/devtools/logs/command-20261017-032054-41828.log); separate clean checkout detached7cc493f, full git status empty, pinned54 dependencies and2283 compilation records with zero primary-checkout inputs. Clean Release exit0 in22.45s (command-20261017-032125-8172.log). Clean alias/GCN/DMA/buffer-access /Zs exit0 logs032141-55656 /032141-71592 /032141-25264 /032141-14576. No fixture link, shader conversion, runtime, GPU validation, game, XInput, UI or profiling run. No new FPS/frame-time/correctness/stability result; saved Vulkan/DX12 baselines remain authoritative.

Complete external artifact F:/PS4Dev/handoff/late-dma-publication-20261017/artifacts-7cc493f: EXE SHA25658e5e55591439bf5ecb9194815caabcfb99977d802d6ab0b99c4e2a13b1763bb; PDB725e8477fe01474dee8dad57b411b72e55877aa540619b81e4966df44f1e0709; native-compiler, licenses and release-identity.json included. Handoff root has before-HEAD/status/staged/unstaged patches, source patch/manifest, exact /Zs commands and RELEASE_PROOF.json. Previous342/a564/167 artifacts and all runtime evidence remain untouched. No installed core/selection/settings/saves/Vulkan change. Manual folder selector already installed in PS4Desktop; no new app/UI execution.

Next concrete C++ task: late sharps smaller than4KiB currently remain absent from that command's translation table, then use existing feedback/materialization next time. Complete their legal physical subpage while still in the buffer materialization phase, before final views; preserve independent I, private clean streams, real guest backing and every GPU-only B byte/padding. Also make late arena replacement occur before selected descriptors rather than deleting BindResident's identity guard. Inspect only rasterizer.cpp::PrepareShaderBuffers emit-buffer-views boundary, GuestDmaMemory::ObserveResidentSharps/PlanRanges and guest_stage_bindings.cpp prewarm/reservation order. Unifying SRV-only auxiliary snapshots with DMA must retain actual IA/native resource states. Then finish opaque depth/stencil independent I/B and tiled pressure retirement, followed by the existing nine-gap queue. Fully resident publication is connected; do not redo it or count unrun tests as absent implementation. No broad audit, duplicate opcode research, game run or automatic install.


## Planned resident DMA synchronization checkpoint - 2026-10-17

This is the authoritative current implementation/build/continuation section; older current/next sections below preserve history.

Production50bdc81/a564b6c/34232f8 connect synchronization of already planned resident DMA B after image binding, preserve CPU streams and physical guest backing, and safely borrow/release PageWatch image/buffer witnesses. Opaque depth/stencil retains the legacy exclusive-image/physical-backing path; it is not completed independent transfer. Primary and separate clean committed Release linked at34232f8. For these changes runtime/GPU/game/XInput/UI tests are not_run; fixture TUs passed C++ /Zs only. Earlier behavior evidence is retained. Latest full external runtime: F:/PS4Dev/handoff/ordered-dma-sync-20261017/artifacts-34232f8. Never auto-install/select cores. Next: stable late DMA residency/table publication before final native descriptors; then remaining opaque/tiled-pressure/converted/shifted transfer guards. Registry:129 groups,59 connected_with_known_gaps,70 with no separately established change, nine broader gap IDs. The DMA group now explicitly records the proven late-page implementation gap; unrun behavior is separate. Full port remains unfinished.

Production commits:50bdc81d832142af3a35c2fe29408b598c53e1af, a564b6c8a9ff26edbefa76ab37f71a8aa98b3572 and34232f80bbbeaf46ded52e4ae853ccf310c32dfd.50 alone is intermediate WIP: its overbroad opaque-image rejection was corrected bya564.342 restricts captured refresh views to actual DMA ranges and avoids additional non-DMA work. The original PrepareShaderBuffers content loop is extracted unchanged as PrepareBufferContents; BUFFER_CONTENT_EXTRACTION.json records normalized source equivalence, not behavioral acceptance.

ExecuteGuestResources now permits the known one/multiple-stage DMA image domain and retains overlapping I before B binding. ObserveResidentSharps records actual DEFAULT sharps (private CPU streams excluded); AppendRequests checks physical guest rights/backing and avoids an eager formatted ReadMemory in independent domains. BeforeBind runs SynchronizePreparedDmaBuffers after image/attachment preparation, with selected resource identity guards, no future guest-write declarations, no formatted I retile and no B replacement. For protected opaque legacy ranges, ReadDmaBacking compares bounded physical alias chunks, preserves GPU-only B against unchanged CPU snapshots and uploads real CPU dirtiness. GPU ReadConst remains read-only; atomic miss bitmap, four-slot fence feedback, bank encoding and shader metadata27 are unchanged.

PageWatch: borrowed RO witnesses coexist with independent I; release preserves sibling B, pending CPU watches, image protection and NOACCESS->RO ordering. CPU-visible unaligned output ranges demote and release exact rounded RO gaps. GPU/CPU ownership, padding, queues and lifetime remain required; no function is disabled for FPS.

Verification: primary Release linked (command-20261017-031023-40856.log); clean checkout detached34232f8, full git status empty, pinned54 dependencies,2283 compile commands, zero primary source inputs; clean Release linked exit0 in18.719s (command-20261017-031231-65004.log). Raw logs: F:/Codex-Shared/devtools/logs. Existing alias/GCN/DMA/buffer-access TUs passed /Zs in clean a564b6c (030541-44000/61400/52984/59756); their source/header content is unchanged by342's guest_compute.cpp-only refinement. These are compile/typecheck results, not fixture execution, conversion, GPU validation or game acceptance. No new FPS/frame-time/stability claim. Saved Vulkan/DX12 baselines remain authoritative.

Artifact: F:/PS4Dev/handoff/ordered-dma-sync-20261017/artifacts-34232f8 contains EXE, PDB, native-compiler, licenses and release-identity.json. EXE SHA256577accb89992962596c5595efa1607774aa829afecb5378e81e23a6e12f8f512; PDB083cd6499118738fd305313312e9f826a1be147faa6aa1888b4f02c82d050218. Handoff root retains staged/unstaged snapshots, source patches/manifests, intermediate50/a564 identities, complete a564 artifact and RELEASE_PROOF.json. This build is not installed/selected/launched. Manual app workflow remains Versions of core -> Select folder with core -> Select core; settings/saves/reference Vulkan untouched. Primary .serena/ is preexisting untracked navigation state, excluded from commits.

Remaining production work: ObserveResidentSharps can discover real native pages while image prewarm/attachment preparation runs after AppendRequests has frozen ranges_/translations_. Bind publishes only the earlier plan. Newly resident mip-tail/late image bytes can therefore remain absent on the first command. Admit and materialize late pages at a stable boundary before selecting final B views/descriptors, respecting full4KiB rights/backing/initialization. Do not rerun partial PrepareShaderBuffers under selected views or delete identity guards: arena growth can retire mappings and lose GPU-only B/padding. Precise files: guest_dma.cpp::ObserveResidentSharps/AppendRequests/Bind, guest_compute.cpp materialization/binding order, guest_stage_bindings.cpp prewarm/final views. After that, complete opaque depth/stencil independent I/B and tiled pressure retirement, then the remaining nine-gap queue. No new broad upstream audit, DMA-opcode research, page-identity/mip-prefix audit, duplicate test runs or automatic installation.

Next action: in F:/PS4Dev/work/shadps4-dx12-src, verify HEAD and inspect only those existing late-residency/materialization symbols, preserving this checkpoint. Continue real C++ integration; build incrementally, reproduce commits in the separate clean checkout, save a distinct external runtime. Runtime acceptance remains deferred until the full implementation boundary. A newer docs checkpoint may follow the production SHA above without changing the artifact identity.


## Ordered per-stage I/B binding checkpoint - 2026-10-17

This is the current implementation/build/continuation section. Older current/next entries below retain historical evidence; do not restart completed source work.

Production `167863896f1d92196c74c17a4440a2a800541284` adds guest_stage_bindings.cpp/.h and integrates PrepareOrderedGuestStages into ExecuteGuestResources. Pinned e0a46675 evidence reused: vk_rasterizer.cpp410-441 loops each original stage with BindBuffers then BindTextures, then SynchronizeDmaBuffers; BindBuffers757+ invalidates I at each original exact written sharp; texture_cache.cpp628-639 marks storage GpuModified then updates that image; buffer_cache.cpp194-211 takes clean small CPU streams before residency,357-378 only synchronizes formatted readonly buffers,381+ retiles I with the exact-base assertion. No broad audit was repeated.

Implementation: GuestStageBindingSpan preserves original stage resource intervals before descriptor/classification reordering. For common native image domains, the new executor pins/promotes initial I, validates transfers and natural narrow-writer bounds, reserves full B geometry in one batch with known_writes empty (future writes are not marked early), captures original clean CPU streams including their DWORD prefix/byte adjustment, then applies each stage's buffer operations before its image operations. Resident aliases share B; private early streams remain immutable. Exact buffer writes invalidate I in place and advance the domain generation. Image binds reuse valid I or refresh from the current private B snapshot. RefreshBoundImage copies refreshed storage into the same bound allocation: earlier views observe the later refresh, while earlier recorded I->B copies remain earlier GPU operations. Intermediate image ownership/history becomes available to subsequent formatted buffers. Final shader preparation does not revive an I invalidated by a later stage, or download its independent pixels into B. Actual buffers/images are transitioned, pinned and submitted through the existing fences. Original view counts, descriptor sharp bounds and metadata synthesis are retained; stage metadata remains27 because the serialized shader ABI did not change.

Host preallocation must not advance the semantic mip expansion. If cached I needs growth/reinterpretation before stable B views, an initial full-byte retile of its old prefix is frozen; buffers before the first BindTextures use that original prefix, not a future tail. After the image event, compatible views use the common maximum storage. Unmodified fresh readonly inputs keep the existing snapshot path; they do not acquire speculative GpuModified history. DMA, converted/BC view domains without a common native allocation, shifted geometry and transfers beyond the original reader VA arena stay explicit guards. This is connected implementation for the proven domain, not whole-stage or full-port acceptance.

Build integration: native-port/Native-Core.cmake includes the new production translation unit; all nine standalone source lists that already compile guest_compute.cpp include it too. Initial primary link failed because the core's explicit list omitted the new module (command-20261017-022721-26480.log); the actual list was fixed, then full links passed. New native_ordered_stage_image_buffer_bindings CTest registration calls the existing alias host with ordered-stages. Standalone suite configuration/link/execution remains deferred, so the new registration is authored, not a claimed executed test.

Prepared validation: ordered_stage_bindings_fixture.h adds48 transfer/cache controls across exact/interior writers, refresh/no-refresh, early/late retile, DWORD/byte/halfword-adjusted private reads and old mip-prefix/full-chain seeds. They call the production executor with real GPU uploads/retile/copy/readback when eventually executed, compare complete tiled allocation/mip/layer/padding bytes, shared bound resource identity and final logical dirty state, and ensure early CPU streams stay B0. The fixture supplies the exact-base notification/ownership adapter; it does NOT establish GuestRasterizer PageWatch behavior, actual graphics shader execution or full acceptance. Alias and existing real-GCN TUs passed C++ /Zs only, with actual Release includes/defines in primary and clean checkouts. No shader conversion, fixture link or test execution was performed in this turn.

Primary final Release linked: command-20261017-023704-34592.log. Separate clean committed Release linked at1678638:023731-30028. Clean F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 has no tracked dirt,54 pinned dependencies and2283 compiler commands, including guest_stage_bindings.cpp, with zero primary checkout inputs. Primary alias/GCN /Zs023619-57464 /023117-61176; clean023809-33988 /023809-30108. Logs are command-20261017-*.log in F:/Codex-Shared/devtools/logs. No runtime/GPU/game/XInput/UI/profiler execution, new FPS/image/stability result or final acceptance claim.

Full immutable EXE/PDB/compiler/runtime/licenses folder: `F:/PS4Dev/handoff/ordered-graphics-stages-20261017/artifacts-1678638`. EXE SHA256 `239d3350d0bac4abf6d0a270c47ad44e65bf8e1ab35fdb05ab5348e5ba23c9b9`; PDB `1b67bb002b36cc52ff68035073d594fb22c9eebf32233de52d28d1794069e362`. Evidence folder has before staged/unstaged/status/HEAD, source-before-commit.patch, new-file backups, nine-file source hashes, primary/clean response/command files, release-identity.json and RELEASE_PROOF.json with all runtime hashes. The artifact was not installed/selected. Older artifacts, original Vulkan, saves, app selection and user processes remain unchanged.

Exact continuation: adapt the explicit ordered_stages&&uses_dma guard in guest_compute.cpp. Start with GuestDmaMemory::AppendRequests / Bind / RecordFaults / ConsumeFaults and pinned vk_rasterizer.cpp437-439 plus BufferCache::SynchronizeDmaBuffers233-240. Preserve the same per-stage buffer/image event order, then perform the pinned post-stage resident-buffer synchronization before submit; keep fault feedback, original dirty ranges, PageWatch, independent I/B authority and lifetime. Do not remove the guard by treating native GPU-address banks as ordinary known sharp writers. Next queued source gaps are converted/BC view domains and shifted/clipped physical transfers in guest_stage_bindings.cpp, then the other existing gap IDs. Reuse the saved measured baselines and existing fixtures; do not repeat completed audits or narrow-bounds/image-identity/mip-prefix/partial-page work. Builds are allowed; final renderer/game/XInput acceptance is still deferred. Nine explicit gap IDs and129 unchanged implementation/behavior group records remain. Normal push is authorized; never auto-install/select cores. Full-port goal remains active.


## Partial-page independent image checkpoint - 2026-10-17

This is the current source/build/continuation section. Older current/next sections below are historical; do not restart completed mip-prefix or ownership work.

Production `7420e327594dd6b2f3087719bd600aac09fdc812` connects partial-page independent I ownership without collapsing its physical/resident B version. Reference reused: pinned e0a46675 TextureCache::InvalidateMemory/MarkAsMaybeDirty (texture_cache.cpp123-169), RefreshImage's MaybeCpuDirty handling, and page_manager.cpp434-443. An actual CPU write inside I dirties/retires it; a CPU write outside I but on its boundary page must preserve I until B equality is checked. Conservatively dirtying every image sharing that page would not preserve pinned semantics. No broad audit was repeated.

Production details: new guest_page_range.h separates overflow-checked logical bytes from their 4KiB protection envelope. Entry retains at most two complete B/image boundary intersections, <=4KiB each; repeated outside stores keep the earliest snapshot. GuestRasterizer::RememberImageBoundary, WatchBufferImageVersions and ResolveCpuFault match protection pages, skip temporarily writable boundary pages, and preserve stronger NOACCESS buffer guards. ValidateIndependentImageWatches restores protection before comparing all saved bytes through the physical alias; unchanged B retains I, changed B marks logical dirtiness while retaining GpuModified history, cancels stale linear downloads, invalidates CPU buffer proofs before Finish, then retires ownership. Root draw/dispatch, CPU/GPU backing preparation, memory resolution, display, download and pressure paths validate before reuse. Nested pre-store fault/CheckBacking/InvalidateMemory resolution defers validation until the CPU instruction can finish, avoiding an immediate rearm/fault loop. Cache lookup/upload/retile rejects uncertain I. ReleaseImageProtection and Rasterizer::ReleaseUnusedReadOnlyWatches preserve sibling pages and buffer proofs while retiring rounded image-only gaps. Existing format/size/layout guards remain; ordinary exclusive image ownership still requires complete pages. Exact-base GPU invalidation is unchanged.

Prepared checks: existing native-port/page_watch_test.cpp now uses the actual production boundary helpers with real dual physical mappings, two I ranges on one page, a stronger B owner on the tail page, outside stores followed by unguarded inside stores, first/tail byte ranges, preserved distinct logical-I values, retained earliest snapshots, failed reprotection and full-byte comparisons. Existing native_page_ownership_and_cache CTest registration covers it. PageWatch, writable_image_alias and GCN attachment TUs passed C++ /Zs with actual Release toolchain/includes in primary and clean checkouts. No fixture execution/link, shader conversion, GPU/game/XInput/UI/profiler run or acceptance is claimed. The newly authored helper controls do not establish whole-renderer runtime correctness.

Primary Release linked: command-20261017-021650-23600.log. Separate clean committed Release linked at7420e32: command-20261017-021720-34568.log. Clean F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 has no tracked dirt,54 pinned dependencies and2282 compiler commands with zero primary-checkout inputs. Primary PageWatch/alias/GCN /Zs logs021512-49684 /021537-47632 /021537-70680; clean021825-70668 /021825-21280 /021825-13728, all command-20261017-*.log in F:/Codex-Shared/devtools/logs. Initial clean fixture-only response files failed to replace forward-slash include paths and could not find fmt; saved failed-clean-*.rsp and logs021808-37164 /021808-68480 /021808-51008 preserve the failure. Both path forms were corrected and all three passed using clean inputs. The independent production clean build was not affected.

Full immutable EXE/PDB/runtime/compiler/licenses folder: `F:/PS4Dev/handoff/partial-image-watch-20261017/artifacts-7420e32`. EXE SHA256 `bf939c21df644bd6cf43cada9a5a0ef48222864944bce05705e746706cb7fe57`; PDB `336233b82f5d31e289de32e304ededfcc82820f0807d3b33732abac2dc864a0d`. Evidence folder includes before staged/unstaged/status/HEAD snapshots, source-before-commit.patch, new header backup, seven-file source hash manifest, original/corrected/failed compiler response files, release-identity.json and RELEASE_PROOF.json. New runtime was not installed/selected. Original Vulkan, saves, old artifacts and user processes remain untouched. No new FPS/stability/image/control result exists.

Next source action: continue ExecuteGuestResources's explicit multi-stage writable buffer/image guard at guest_compute.cpp444 and ordered formatted domain guard618. Use original GuestStageResources order to preserve each stage's BindBuffers then BindTextures effects, with per-stage I/B snapshots and immutable earlier views, before admitting cross-stage ordered writers. Keep DMA, independent geometry and attachment feedback guards until their transfer/storage adaptation exists. Reuse the existing GCN and alias fixtures; author controls without executing them before the full implementation boundary. Do not redo shared mip-prefix planning, physical image identity, narrow bounds or partial-page MaybeCpuDirty mechanisms. The129-group implementation/behavior split and nine broader concrete gap IDs remain; lack of execution is not an implementation gap. Core build is reproducible, full port is still unfinished. Normal push is authorized; never auto-install/select new cores.


## Shared shader mip-prefix backing — 2026-10-17

Production `e780a6a43332023f71b7c224adfd7bd5dffe0e2c` connects same-base uncompressed single-sample mip-prefix views. NativeImageStorageAliases promotes compatible prefixes together while preserving MSAA last_level as sample count; original ImageResource NumBindings and specialization native_storage_mip_levels retain each view's range. Metadata StageVersion27. Reference used: pinned TextureCache::ResolveOverlap equal-address size/resource reuse/ExpandImage and FindImage (texture_cache.cpp266+ /517+), plus the saved mip-major layout. No global audit was repeated.

Production changes: guest_compute.cpp::ExecuteGuestResources plans ShaderImageStorage groups before validation/snapshots/uploads, chooses maximum declared and compatible GPU-owned cached geometry, pins old I, retains original per-image desc/layout/mip/count, and allocates a common UAV-capable backing. CompatibleGuestShaderMipPrefixes validates native typed sharing; CompatibleGuestImageMipGeometry separately validates physical transfer/growth, including exact packed expansion. Every common mip must have identical pitch/height/offset/slice/macro/thickness; arrays and volumes retain their complete mip-major slice layout. Entry::Srv/Uav accepts only a bounded prefix of the actual backing and constructs the original view. Upload refresh history now retains logical GPU-modified provenance across a compatible prefix growth.

Independent cached I growth/reinterpretation is prepared before final buffer views: existing private I/B transfer seeds the enlarged tail from B and preserves old I/padding, then acquires the replacement guard before retiring the source. A buffer overlapping a cached parent's extra mips uses the full storage domain and preserves valid I before buffer preparation. Ordered buffer/image binding can now use compatible prefix image descriptors; when an earlier formatted R retiled only short I and later exact-base W invalidated I, CaptureShaderBuffer captures the full prewrite B0 and the saved short tiled B1 is merged into it before final views. Actual resource state transitions and RetainUntilFinish leases protect this immutable snapshot; original buffer dirty ranges/sharp bounds are unchanged. Existing complete-range retention checks still apply; ordered partial-page growth explicitly remains guarded.

Prepared validation: classified-prefix-read/write CTests are registered in native-port/CMakeLists.txt. writable_image_alias_test.cpp runs real resource patching into dynamic mip arrays with original counts; shared backing, per-view mip-count queries, zero OOB reads, discarded poison OOB stores, and full allocation/mip/layer/padding oracle are authored. Identity controls now require10 storage-field differences to split aliases, a mip-count change to retain the alias but change its original-count key, and an MSAA count change to split storage. Fixed the pre-existing authored classifier helper's array flag: full arrays retain their slice/mip coordinates; single-slice T# still exercises non-array zero-slice lowering. The C++ undeclared-zero error in the new probe was corrected; failed log command-20261017-015626-28580.log is retained, then /Zs passed015645-28016. No shader conversion, fixture link or test execution is claimed.

Primary full Release linked (F:/Codex-Shared/devtools/logs/command-20261017-015835-56024.log). Clean committed Release linked ate780a6a (015919-21256); clean alias/classification /Zs passed020001-55364 /020005-32112. All names are command-20261017-*.log in that directory. Clean source F:/PS4Dev/work/shadps4-dx12-clean-release-20261016 has no tracked dirt and2282 compiler commands with zero primary-checkout inputs;54 dependencies remain pinned. Immutable full EXE/PDB/runtime/compiler/licenses: `F:/PS4Dev/handoff/shader-mip-prefix-20261017/artifacts-e780a6a`. EXE SHA256 `4441471b27da1050c54b9b6e26835c4be09c1fcc12a4f2f861f0316fed46df91`; PDB `9be0ee61ceddf262462a05267f77003f3f3a3c650a51aabc97af9d6083907d92`. Evidence folder contains before/source patches, compiler response/command files, release-identity.json, RELEASE_PROOF.json with all hashes. No GPU/game/XInput/UI/profiler test, new FPS, acceptance, app/core selection or installation. Old artifacts/original Vulkan/saves/user processes remain intact.

Exact next source task: independent partial-page image-range ownership, starting at guest_rasterizer.cpp retain_image's desc.address%4096/layout.guest_bytes%4096 restrictions, OwnIndependentImage, WatchBufferImageVersions, ReleaseUnusedReadOnlyWatches and the existing PageWatch::TrackReadOnly/fault flow. Connect logical guest image bytes to the physical protection envelope and stronger buffer/sibling guards before relaxing retention. In particular a CPU write outside the image but inside its protected page must not silently remove that image's future write witness; alias/unmap/remap and retirement must stay coherent. Extend existing page/range fixtures for that contract, with execution still deferred. Keep multi-stage BindBuffers/BindTextures ordering, shifted image-base/subresource geometry and other nine-gap queue items explicit. Do not redo mip-prefix implementation or invent that invalidated independent I still owns memory: the inspected DiscardBufferImageVersions already marks it dirty and calls ReleaseIndependentImageWatches. Full-port goal remains active; builds/compiler/link allowed, final renderer/game/XInput verification deferred to the agreed full-implementation boundary. Never auto-install/select cores.


## Physical image identity checkpoint — 2026-10-17

Production `b85f076842a80f9745291b3265d42a7401f4f6d0`: resource.h::NativeImageStorageAliases compares the physical storage inputs of pinned ImageInfo::ImageInfo(sharp, desc) (image_info.cpp129–160), replacing a whole-T# memcmp. Retains nonzero/equal base, format family/numeric storage compatibility, width/height/depth/pitch, last-level/sample field, type, tiling, pow2 padding and alt tiling. View-specific ranges/mapping and non-storage cache/performance/counter/reserved bits do not create separate allocations. Reads whose different control bits formerly hid a physical alias now enter the existing coherent common-UAV, manual sampling/LOD, swizzle and numeric conversion paths. Those paths were already implemented; do not reimplement them. No guard was deleted, reference/Vulkan profile behavior is isolated, and shader metadata StageVersion26 rejects older unpromoted persistent records.

Prepared validation extends native-port/writable_image_alias_test.cpp with classified-identity, registered as native_classified_image_storage_identity in native-port/CMakeLists.txt. It exercises real resource patching, shared UAV read/write and the complete physical allocation oracle; stable specialization under non-storage controls and11 individual physical-field changes that must currently split alias groups. image_access_classification_test.cpp additionally covers local loaded aliases and external writer hints with control/reserved differences, both native/reference profiles, constant/dynamic mips, queries and sRGB. Both TUs passed actual C++ /Zs in primary and clean checkouts; no fixture link, resource-patching execution, shader conversion, tests or GPU/game execution occurred. The new CTest registration is authored; standalone final-suite configuration/execution is deferred.

Primary Release linked (F:/Codex-Shared/devtools/logs/command-20261017-013955-23704.log). Clean committed Release linked atb85f076 (command-20261017-014126-32792.log), pinned54 dependencies, clean tracked source,2282 compiler commands with zero primary-checkout inputs. Primary alias/classification syntax logs:014030-56836 /014101-57440; clean014200-29900 /014203-40244 (all command-20261017-* in that log directory). Full immutable runtime/PDB/compiler/licenses: `F:/PS4Dev/handoff/image-storage-identity-20261017/artifacts-b85f076`. EXE SHA256 `29ce090e1a8b8beb87aedc42bd88666a17c54163432a1f8edc0e78f7597ce053`; PDB `868cee8c2e1700ae1f5375d8e5172045626174eb9357322411b01f0fd06d85f9`. The evidence folder has before/source patches, compiler response/command files, release-identity.json and RELEASE_PROOF.json with all hashes. No core installation/selection, UI manipulation or game launch; previous artifacts/original Vulkan/saves/user processes remain intact. No new FPS, image or stability acceptance claim.

Next actual production work: adapt same-base compatible uncompressed mip-prefix image views across shader physical alias classification, native image collection/group allocation and original descriptor ranges. Entry points: resource.h::NativeImageStorageAliases / NativeImageViewAliases; resource_patching_pass.cpp::reads_written_image; specialization.h alias keys; guest_compute.cpp guard `Native image aliases with different storage layouts unsupported` and uploaded_views grouping; guest_texture_cache.cpp::CompatibleGuestShaderStorage / MatchesGuestMipLayout / Upload. Distinct last_level currently fails the alias proof and native levels equality; do not merely ignore last_level or remove guards. Connect maximum backing geometry, prefix correspondence, original per-view NumBindings/LOD/quantization, ownership and fence lifetimes together, using pinned ImageInfo/FindImage and saved BC prefix grouping as bounded references. Current negative mip-key control intentionally protects the present boundary and must be updated only with connected prefix semantics. Other geometry/overlap and multi-stage I/B binding order remain explicit. Nine broader backlog IDs remain active; formatted-alias-ordering now distinguishes existing compatible-UAV implementation from these actual gaps. Continue production work without a global re-audit. Final renderer/game/XInput suite remains deferred until the agreed implementation boundary; builds/link allowed. Never auto-install/select core builds.


## Mixed ordered image/buffer versions — 2026-10-17

Current production `c238eef6d068c5dae31ad70387b55f423526af61` builds on6a7dff0/7304bb2. Pinned Vulkan BindBuffers then BindTextures preserves distinct native image I and buffer B, even with a shared guest address. Do not replace that with one shader-visible resource or make one writer win physically. Source facts reused: vk_rasterizer.cpp757–849/853–969 and TextureCache::InvalidateMemoryFromGPU exact-original-base rule. The prior full audit was not repeated.

Implemented in guest_compute.cpp::ExecuteGuestResources: interior buffer writers can accompany retained writable images in one stage; valid independent I is now pinned/reused for writable as well as readonly descriptors. Ordered formatted buffers can coexist with shader image descriptors when CompatibleGuestShaderStorage and the existing complete MatchesGuestMipLayout proof establish one interpretation. The existing layout predicate is exported from guest_texture_cache.cpp/.h rather than duplicated. Multi-stage offset writers, ambiguous image versions/layouts, DMA and auxiliary buffer domains remain guarded.

When an earlier resident formatted R has retiled I into B1 and a later exact-base W invalidates logical I, sampled/storage image initialization now consumes the immutable B1 transfer rather than an older B0 capture. ImagePending holds the transfer index through buffer preparation. Only after the final prepared-buffer copy does the actual tiled resource transition COPY_SOURCE -> NON_PIXEL_SHADER_RESOURCE and publish lightweight immutable views; Resource copies have independent state fields, so copying earlier would produce stale barriers. Transfer leases, original physical padding, descriptor/sharp bounds, dirty byte ranges and fence publication are retained. Private small clean CPU R still reads B0; interior writers preserve valid I. These are connected source semantics, not runtime acceptance.

Prepared controls: native-port/gcn_mixed_formatted_fixture.h is committed and called from the existing ordered GCN fixture.32 cases cover R/W and W/R, small/large, clean/dirty B, exact/interior W, sampled/storage I. Seeded I!=B, same-invocation raw/formatted/image observations, next-element OOB store, two I probes around B CPU readback, and full64KiB physical/padding oracle. Primary /Zs passed (command-20261017-013436-53048.log); clean /Zs passed atc238eef (command-20261017-013530-22748.log). No fixture link, shader conversion or GPU execution. Primary full Release linked (command-20261017-013441-6136.log); clean pinned Release linked (command-20261017-013514-34824.log). Logs: F:/Codex-Shared/devtools/logs. No tests/game/XInput/UI/profiler were launched and no FPS or image-correctness result is claimed.

Clean source: F:/PS4Dev/work/shadps4-dx12-clean-release-20261016, detachedc238eef, clean tracked tree,2282 compiler commands with zero primary-checkout paths. Immutable complete EXE/PDB/native-compiler/licenses: `F:/PS4Dev/handoff/mixed-image-buffer-ordering-20261017/artifacts-c238eef`. EXE SHA256 `5c5f5cb485f70c1aec99e5e61c85146f674719f2b338d774d7e512b0113cbfd3`; PDB `69ae387850ad3a3ccedff4878d4df0b24967b4f18ec8648a97813524e36be3e6`. The enclosing evidence folder has before-state/source patches, response/command files, release-identity.json and RELEASE_PROOF.json with all file hashes. Original Vulkan, saves, old artifacts, installed app and selected core are unchanged. Never automatically install/select new cores.

Next concrete source entry: guest_compute.cpp's `Native overlapping SRV/UAV image subresources require shader access unification` guard, plus its descriptor collection and UAV/SRV binding phases. Pinned BindTextures905–947 force_general keeps compatible sampled/storage views on one actual image. Connect shader resource access/metadata/cache specialization and native UAV-compatible image reads before relaxing that guard; preserve sampling/LOD/filter/derivative semantics for any sampled paths, or keep their unresolved path explicit while proceeding through the established backlog. This source guard is now explicit in formatted-alias-ordering. Remaining multiple-stage BindBuffers/BindTextures ordering and incompatible image geometries also stay in that gap. Read the saved registry/queue, not a new global audit. Nine broader gap IDs remain; the full-port goal is active. Build/compiler/link are authorized; final renderer/game/XInput verification stays deferred until the agreed full-implementation boundary. No user confirmation is needed for continuing production work.


## Native exact buffer bounds and narrow writers — 2026-10-17

Current production commits: `6a7dff087b39e539cfd799c4147cd30f72b65396` and alignment follow-up `7304bb203bd363ad84ebd49587f3e64d66bfaf45`. These supersede the DWORD-only continuation below. Full-port goal remains active; the nine broader gap IDs in IMPLEMENTATION_REGISTRY.md are not closed by this subdomain.

Implemented: Profile::native_buffer_bounds, EmitContext::BufferAccess and DefineBuffers, guest_compute.cpp::ExecuteGuestResources. A readonly packed constant block at b0 space34 carries each original clamped sharp's inclusive byte limit, including the native 16-byte descriptor prefix and both halves of >4GiB ranges. This space is distinct from DMA32/point capture33. Scalar and vector U8/U16/U32/U64/F32 loads/stores and 32/64-bit atomics are guarded; invalid reads/atomic results return zero and writes/atomics do not execute. Native prefix/component overflow is checked. Float sign-atomic phi inputs follow actual nested predecessor labels. Special buffers and null-sharp 16-byte temporaries retain prior semantics; native metadata StageVersion is25. Reference profiles keep the flag off.

Contained byte/halfword writer DWORD envelopes now share the prewrite B arena around I, under the existing compute-only, buffer-only, single-image/layout/ownership proof. New padded admissions require reflected exact bounds and alignment to every used scalar type; no new odd-base wider-alias rounding semantics is invented. Original sharp base determines I invalidation; exact write bytes determine dirty tracking. DMA, multiple stages, mixed shader images, ambiguous owners/layouts and offset formatted readers remain guarded. No function is disabled for speed.

Prepared verification: sixteen narrow cases extend the existing eight DWORD interior controls in native-port/gcn_ordered_formatted_fixture.h. Real GCN BYTE/SHORT loads and stores keep the writer U8/U16-only, test next-element OOB load/store, and read the live full GPU far word after the write so dirty-byte readback cannot conceal GPU-only padding corruption. They retain R/W and W/R, both image sizes, clean/dirty B, I!=B, retained I probes and full64KiB physical oracle. The containing fixture now enables the same native bounds/int8/int16 profile. Actual C++ /Zs passed in primary and clean checkouts; this is not shader conversion, fixture linking, GPU or game acceptance.

Primary Release compiled/linked (latest log command-20261017-012719-19580.log). Separate clean committed Release passed at7304bb2, with2282 compile commands and zero primary-checkout inputs, pinned54 dependencies and clean tracked source. Log: F:/Codex-Shared/devtools/logs/command-20261017-012742-57960.log. Fixture syntax log: command-20261017-012611-48632.log (at6a7dff0; no later fixture/compiler-header change). Latest immutable full runtime/PDB/compiler/licenses: `F:/PS4Dev/handoff/native-buffer-bounds-20261017/artifacts-7304bb2`. EXE SHA256 `f478eac8d5fe0cd51affc1e79a869305100d94c0a59038e54322320a18473880`; PDB `a068d9b4709654224bb5f8f8084f4e9168b4b9f66cb739b40bc420086b6610a3`. RELEASE_PROOF.json has every file hash; before-state and source patches are in the same evidence folder. Earlier6a7dff0/daa76fc/6cbaef7 artifacts remain intact. No core installation or selection occurred.

Next concrete source work: continue the remaining ordered formatted domain in ExecuteGuestResources, starting from its still-live mixed image/buffer guard and the image binding/commit phase, adapting shared storage/lifetimes before removing any guard. Read NEXT_ORDERED_BUFFER_WRITERS.md and reuse pinned BufferCache::SynchronizeMemoryFromImage/TextureCache::InvalidateMemoryFromGPU evidence; do not restart the global audit, redo the connected DWORD/subword source work, or treat offset formatted readers as a requirement (pinned exact-base assertion). Extend existing controls for first-word narrow invalidation and remaining shared domains when changing them. Final shader/GPU/game/XInput suite remains deferred until the agreed implementation boundary. The installed app folder selector is already verified/pushed; future cores stay external and user-selected.


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

## Active one-level BC / word views checkpoint - 2026-10-16

This section supersedes earlier current/next instructions. Full-port goal remains active;
all129 groups partial/open. No configure/build/compiler/tests/GPU/game/XInput/UI.
Preview24, original Vulkan, saves and running user session remain unchanged.

Through82320ea: thin shifted UINT and native BC prefix views preserve actual parent
ownership/descriptor spans. Macro/thick admission now proves rounded physical storage,
aligned groups, bulk stride and pipe/bank corrections before normalizing parent swizzle.
Nine positive/eighteen negative CPU address controls are authored/unrun; real-GCN
macro/thick end-to-end coverage remains absent. Prior proofs are not runtime acceptance.

Current production block extends MatchesReadonlyBlockMipPrefix to exact mip0 of a one-
level native BC parent by removing source.levels<=1. Physical prefix, block-word shape,
readonly role and complete layer matching remain. This common predicate consistently
routes readonly Upload through a dirty/nonowner private child and writable grouping
through the existing live parent UID/pin/watch transaction. BC remains the sole owner;
replacement still acquires before old parent retirement. A writable-only exception would
leave readonly-only BC+UINT binding able to retire an already-bound BC parent. Source
review found no concrete blocker; ordinary snapshot/ownership and format gates stay.

Twenty-seven GCN scenarios are prepared, unrun. New24-26 use word12x12/pitch16/layers2,
BC48x48/pitch64/one mip: BC3=8192B, BC1=4096B, with physical padding. Each includes a
same-dispatch readonly BC+UINT pair before writing; the last also checks disjoint native
BC layer1 read/UINT layer0 write. Source I differs from B/P and full physical/output
oracles remain. Two image sharps use all16 user registers; output buffer constants are
materialized in scratchSGPRs16..19 and exact GetSharp/output bounds are asserted. No
out-of-bounds user_data copies or production shader workaround. Prior24 GCN cases and
the macro/thick address controls remain intact. No compilation, fixture/address execution,
descriptor/PageWatch/fence/graphics/game or performance acceptance; no current FPS/PASS.

Evidence/current recovery: F:/PS4Dev/handoff/bc-single-level-20261016/{SOURCE_CONTRACT.md,
REVIEW.md,fixtures,checkpoint/COMMIT.json,CHECKPOINT_FINAL.json,final-residual}. Previous
macro and shifted/native evidence/measurements remain. The pinned SliceOf/FindImage
multi-local-layer/base_array quotient/overwrite discrepancy is still open; full Vulkan
parity is unproved. Broad inherited integration/22 staged deletions stay separate. Clean
commits alone do not reproduce the core; restore only in a separate checkout of manifest.head.

Next production task: shifted readonly BC views whose local base dimensions are odd or
smaller than4 pixels, while the actual parent already has a valid native BC mip. Inspect
RebaseBlockSubresource's native_bc detection/expanded rejection and LayoutGuestTexture's
BC fallback. Prove physical block/slice correspondence, then build the parent-relative
native descriptor, without requiring CPU RGBA expansion solely for that binding. Preserve
original shader type/dimensions, typed codec/number format, SRV span, parent I and ownership.
Do not relax generic odd BC allocation/upload rules. Reuse saved actual-owner/partial-copy
fixtures for tail provenance. Nonarray-over-array, unaligned thick fragments, numeric
variants and shared seed accounting remain open. Keep execution deferred; no general audit.

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

## NEXT CODEX SESSION — START HERE (current 7304bb2 continuation)

Continue Skiters/ps4-dx12 on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section, STATE.md and NEXT_ORDERED_BUFFER_WRITERS.md; verify actual HEAD (production7304bb2 plus documentation checkpoint). Exact sharp bounds and naturally aligned contained subword writer domains are connected and Release reproduced; do not redo their source audit. Resume C++ ExecuteGuestResources mixed image/buffer shared-storage ownership/commit adaptation using pinned e0a46675 evidence and the saved Vulkan/DX12 measurements. Preserve exact sharp invalidation, PageWatch bytes, physical padding, barriers and fence leases. Finish the existing concrete implementation backlog, compile/link incrementally and reproduce committed sources in the clean checkout. Renderer/game/XInput execution remains deferred to the agreed full-implementation boundary; then run the prepared minimal suite and isolated gameplay, compare completed guest FPS against the saved baseline without FG, and fix defects. Never auto-install/select a new core: the app folder selector is installed and the user selects external artifact folders. Preserve original Vulkan/saves/user session; ordinary push after appropriate verification, no force. Latest complete runtime: F:/PS4Dev/handoff/native-buffer-bounds-20261017/artifacts-7304bb2; no new FPS measured.

## NEXT CODEX SESSION — START HERE (current c238eef continuation)

Continue Skiters/ps4-dx12 fix/mixed-cmask from the latest SESSION_HANDOFF.md, STATE.md and NEXT_ORDERED_BUFFER_WRITERS.md. Verify current Git HEAD (productionc238eef plus checkpoint). Mixed ordered compatible image/buffer I/B versions and exact native buffer bounds are connected and clean Release reproduced; reuse that evidence and the saved Vulkan/DX12 metrics. Continue production C++/shader access unification for overlapping image SRV/UAV subresources using pinned BindTextures force_general semantics. Preserve physical image sharing among image views, independent B/I versions, exact writes/PageWatch, barriers, lifetimes and fence leases; keep unresolved filtering/stage/layout paths explicit. Then continue the existing nine-gap implementation queue without a repeat global audit. Build/link incrementally, commit coherent sources, and reproduce them using the pinned clean build. Tests/game/XInput remain deferred until the agreed full-implementation boundary; only then run necessary prepared controls and isolated gameplay, verify completed guest FPS without FG against saved baseline, and fix defects. Latest full runtime F:/PS4Dev/handoff/mixed-image-buffer-ordering-20261017/artifacts-c238eef is game-unverified. Do not auto-install/select cores; manual folder selection is installed. Preserve original Vulkan/saves/user session and old results, ordinary push verified commits, no force. Keep the full-port goal active until all implementation and required verification are actually complete.

## NEXT CODEX SESSION — START HERE (current b85f076 continuation)

Continue fix/mixed-cmask from the latest SESSION_HANDOFF.md and STATE.md; verify actual HEAD (productionb85f076 plus checkpoint). Reuse pinned Vulkan/DX12 evidence and the connected exact-buffer/mixed-I/B/physical-image-identity work. Do not redo the global audit or already implemented coherent UAV/manual sampling/LOD paths. Continue production same-base uncompressed mip-prefix image aliases through shader alias metadata/specialization, native maximum backing geometry and original per-view mip ranges; preserve dirty bytes, independent I/B, padding, barriers, ownership and fences, and keep guards until the whole transfer is connected. Continue the established nine-gap queue. Compile/link incrementally, commit coherent sources and reproduce committed Release in the pinned clean checkout. Final tests/game/XInput stay deferred to the agreed full-implementation boundary; then run necessary prepared controls and isolated gameplay, compare completed guest FPS without FG with the saved baseline, and fix defects. Latest complete runtime is F:/PS4Dev/handoff/image-storage-identity-20261017/artifacts-b85f076, game-unverified. Never auto-install/select cores; the user selects folders. Preserve original Vulkan/saves/old results/user session; push appropriately verified commits normally. Keep the full-port goal active until all applicable implementation and required verification are complete.

## NEXT CODEX SESSION — START HERE (current e780a6a continuation)

Continue Skiters/ps4-dx12 fix/mixed-cmask. Read the newest SESSION_HANDOFF.md and STATE/NEXT_SESSION, verify actual HEAD (productione780a6a plus documentation checkpoint), and reuse saved Vulkan/DX12 measurements and connected mip-prefix/alias/I-B contracts without a global re-audit. Continue production partial-page independent image-range ownership in guest_rasterizer.cpp retain_image/OwnIndependentImage/WatchBufferImageVersions and PageWatch: preserve byte-level logical I, physical protection envelopes, buffer/sibling guards, CPU faults outside I but on the same page, aliases, remap/unmap and fence retirement before relaxing current alignment guards. Continue the existing concrete nine-gap queue, including stage ordering and shifted geometries. Compile/link incrementally, commit coherent sources, reproduce committed Release from the pinned clean checkout. Final renderer/game/XInput execution remains deferred until the agreed full-implementation boundary; then run prepared necessary controls and isolated gameplay, compare completed guest FPS without FG with the saved baseline, and fix defects. Latest complete runtime F:/PS4Dev/handoff/shader-mip-prefix-20261017/artifacts-e780a6a is game-unverified. Never auto-install/select cores; user selects external folders. Preserve original Vulkan/saves/old results/user session, push appropriately verified commits normally, and keep the full-port goal active until all applicable implementation and verification are complete.


## NEXT CODEX SESSION - START HERE (partial-page checkpoint, 2026-10-17)

Continue Skiters/ps4-dx12 from the latest checkpoint on fix/mixed-cmask. Read the current top section of SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md and IMPLEMENTATION_REGISTRY.md; verify Git HEAD/upstream and preserve all dirt. Production7420e32 has primary/clean Release and C++ typechecking, no runtime acceptance. Resume production C++ at guest_compute.cpp ExecuteGuestResources multi-stage guards444/618: preserve original per-stage BindBuffers/BindTextures I/B ordering and immutable prior views before widening admission. Reuse pinned Vulkan and saved measurements; do not repeat completed audits or partial-page/mip-prefix work. Build incrementally, reproduce committed sources in the clean checkout, and save complete external artifacts. Keep renderer/GPU/game/XInput/UI execution deferred until the agreed full implementation boundary; then run the minimal existing suite/game controls, compare real guest FPS with the saved baseline, and retain exact executable identity. Never auto-install/select cores, alter original Vulkan/saves, or interrupt user processes. Commit/checkpoint and normally push verified changes; continue the remaining explicit nine-gap backlog without claiming full parity prematurely.


## NEXT CODEX SESSION - START HERE (ordered-stage checkpoint, 2026-10-17)

Continue Skiters/ps4-dx12 on fix/mixed-cmask from the latest checkpoint. Read the current top of SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md and IMPLEMENTATION_REGISTRY.md; verify HEAD/upstream and preserve all dirty sources. Production1678638 has primary/clean Release and C++ typechecking only; full port and runtime acceptance are unfinished. Resume the explicit ordered_stages&&uses_dma guard: adapt GuestDmaMemory stage binding and the pinned post-stage SynchronizeDmaBuffers contract without losing fault feedback, original dirty ranges, PageWatch, independent I/B domains or fence lifetime. Then continue the existing converted/BC/shifted transfer and nine-gap queue. Reuse saved Vulkan/DX12 measurements and fixtures; do not repeat completed audits or per-stage/native-bounds/mip-prefix/partial-page work. Build incrementally, reproduce committed sources from the clean checkout, save complete external core folders and normally push verified commits. Keep tests/game/XInput/UI execution deferred to the agreed full implementation boundary; then run the necessary suite/game controls, compare actual guest FPS with saved baselines and preserve binary identity. Never auto-install/select cores or alter original Vulkan/saves/user processes.


## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read the top current section of SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md and VULKAN_DX12_PARITY.md, verify current HEAD (production34232f8 plus docs checkpoint). Resume the concrete late DMA resident-page/table publication gap in GuestDmaMemory/ExecuteGuestResources/PrepareOrderedGuestStages without moving selected B views or losing GPU-only data, then opaque depth/stencil and tiled independent-image pressure/retirement, then the remaining pinned nine-gap queue. Use existing Vulkan/DX12 evidence; do not repeat completed audits or alter pinned scope. Production implementation and behavior acceptance are independent registry states. Incremental Release and clean-checkout reproduction are allowed; runtime suite, Bloodborne and virtual XInput remain deferred until the full implementation boundary. Then perform only necessary acceptance with isolated profiles, real completed guest frames (no FG), compare saved baseline, release controller. Preserve exact verified binary identity. Save new builds to distinct external full-runtime folders; never automatically install/select them. Commit and normally push verified changes; keep SESSION_HANDOFF.md authoritative. Preserve original Vulkan/saves and existing evidence.


## NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask, from production7cc493f plus its documentation checkpoint. Read the top current SESSION_HANDOFF.md section, STATE.md, NEXT_SESSION.md and VULKAN_DX12_PARITY.md; verify actual Git HEAD. Resume tiny late DMA sharp/subpage materialization before stable final views in PrepareShaderBuffers/GuestDmaMemory/PrepareOrderedGuestStages. Preserve GPU-only B, distinct I, clean private streams, real backing, native states, fences and selected-resource identity. Then complete opaque depth/stencil and tiled independent-image pressure retirement and the existing pinned nine-gap queue. Use saved Vulkan/DX12 measurements, do not repeat completed research or expand pinned upstream scope. Build incrementally and reproduce committed inputs in the clean checkout, retain exact binary/PDB/runtime identity in a new external folder. Do not auto-install/select cores. Runtime suite/Bloodborne/XInput are deferred until the full implementation boundary; then use isolated saves/profiles, minimal acceptance, actual completed guest frames excluding FG, saved baseline comparison and neutral/released controller. Keep implementation and behavior statuses independent, checkpoint and normally push verified work. SESSION_HANDOFF.md is authoritative; preserve all previous artifacts/evidence.


## NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask, from production284be47 plus its docs checkpoint. Read the top current SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md and VULKAN_DX12_PARITY.md; verify actual HEAD. Resume stable physical B residency/GPU-preserving envelope growth before complete tiny late DMA materialization; preserve old GPU B, CPU snapshots/store markers, independent I, PageWatch guard transfer, complete old tails, native states, selected-view identities and fences. Pinned EnsureResident uses sparse arenas; DropCached/Find currently lose GPU-only formatted B on CPU rematerialization. Invalid-I pressure discard is now connected; valid tiled GC exclusion is pinned scope, do not reopen it. Continue SRV auxiliary/opaque depth/converted/shifted/clipped and the existing nine-gap queue using saved Vulkan/DX12 metrics without repeating completed research. Incremental Release and separate clean committed source reproduction are allowed. Save exact EXE/PDB/full-runtime identity in a new external folder; never auto-install/select cores. Runtime suite/Bloodborne/XInput remain deferred until full implementation boundary, then use isolated saves/profiles and minimal acceptance, actual guest frames excluding FG, saved baseline comparison and released/neutral controller. Keep implementation/behavior states independent; checkpoint and normally push verified changes. Preserve all earlier evidence and keep SESSION_HANDOFF.md authoritative.


## NEXT CODEX SESSION — START HERE

Continue F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask, from558baa3/f16630d plus its docs checkpoint. Read the top current SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md and VULKAN_DX12_PARITY.md; verify actual HEAD. Resume semantic DMA residency publication distinct from ordered CPU-stream/domain preallocation in GuestStageBindings/PrepareShaderBuffers/GuestDmaMemory, then full tiny subpage materialization and resource reservation before final descriptor selection. Preserve the new complete GPU B copying, old B0 snapshots, forced CPU-store pages, RO guard transfer, source/copy fences and view identity checks; do not rerun this research. Use pinned ObtainBuffer/EnsureResident and saved Vulkan/DX12 metrics. Continue direct/split/pressure B residency and opaque/converted/shifted/clipped plus existing nine-gap queue; do not expand valid tiled GC beyond pinned rules or invent DMA guest stores. Incremental Release and separate clean committed-source reproduction are allowed. Save full EXE/PDB/compiler/license identity externally; never auto-install/select cores. Runtime suite/Bloodborne/XInput are deferred until the full implementation boundary; then use isolated saves/profiles, minimal acceptance, completed guest frames excluding FG, saved baseline comparison and neutral/released controller. Keep implementation and behavior states independent, checkpoint and normally push verified work, and keep SESSION_HANDOFF.md authoritative.

## NEXT CODEX SESSION — START HERE (semantic residency checkpoint)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section first and verify current HEAD/upstream; source checkpointd01969e has a clean reproduced Release. Restore the exact unfinished image-source publication phase in GuestRasterizer's B-source seed callback / guest_compute prewrite captures before completing tiny late DMA backing with stable final views. Use existing Vulkan/DX12 metrics and the pinned original e0a46675; do not repeat completed audits, invalid-I pressure or preserving envelope work. Preserve masks, original CPU stream semantics, witnesses, actual-store markers and fence leases. Then continue the nine concrete production gaps, build and reproduce committed sources in the separate clean checkout, and update independent implementation/behavior records. Runtime/shader/GPU/game/XInput/UI remains deferred until the full agreed implementation boundary, then run the minimum final suite and XInput game acceptance and compare completed guest frames to saved baseline excluding FG. Save immutable complete runtimes to external artifact folders; never auto-install/select a core. Normal push is authorized. Continue from the last checkpoint without reconstructing old chat history.

## NEXT CODEX SESSION — START HERE (image-source bind checkpoint)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section first, verify HEAD/upstream and restore source checkpoint2498c6f (clean reproduced Release). Image-source prewarm/binding publication is connected; continue the exact missing legal4KiB/64KiB backing plan for tiny late residency before final PrepareShaderBuffers views. Preserve masks, original private streams, old GPU B1 tails/CPU B0 snapshots/store markers, watched-page proofs and source/use fences. Do not remove final selected identity, initialized-coverage or SRV/UAV guards, or allocate under final descriptors. Use existing saved Vulkan/DX12 measurements and pinned original e0a46675; do not repeat completed audits/publication/invalid-I pressure/envelope work. Continue the nine concrete source gaps, then build and reproduce committed Release from a clean checkout, maintaining independent implementation/behavior status. Runtime/shader/GPU/game/XInput/UI stays deferred until the full implementation boundary; at that point run minimum final suite and existing virtual XInput gameplay acceptance, compare actual completed guest frames to saved baseline excluding FG. Save complete immutable runtimes externally, never auto-install/select cores. Normal push is authorized. Preserve an exact actionable checkpoint and continue from it.

## NEXT CODEX SESSION — START HERE (current crash checkpoint)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src from the latest checkpoint. Read the authoritative newest SESSION_HANDOFF.md section, STATE.md/NEXT_SESSION.md and current Git HEAD/upstream. Preserve all current changes; use existing Vulkan/DX12 evidence and exact fixture/runtime hashes, no repeated129-group audit or original-crash replay. Originald01969e failure was the pre-dispatch writable B ownership assertion;935fb69 survived one120s startup exposure, not world acceptance. db0812c fixes signed storage clear bits;8fb9c79 reserves late image DMA backing in ordered common I/B domains, six narrow GPU checks pass. Continue production C++ at GuestRasterizer::Draw submit_replay / texture B-source publisher / ExecuteGuestResources BeforeBind: pre-view legal backing for attachment-only/converted/BC/opaque source receipts outside the connected domain. Preserve contents, PageWatch/alias ownership, publication phase, selected view identity, states/fences/lifetimes. Compile and run only related narrow real GPU/readback regressions while implementing. Use pinned dependencies and clean committed Release, preserve exact binary identities, checkpoint and ordinary push. Final full suite and minimal necessary XInput/game/FPS comparison (real frames, no FG) remain at the full implementation boundary. Never auto-install/select cores or stop/restart the user's app/session; new launcher diagnostics are staged because the active DLL was locked. Full port remains unfinished; continue from the last checkpoint.

## NEXT CODEX SESSION — START HERE (current final-buffer/physical-DMA checkpoint)

Continue the full Skiters/ps4-dx12 port in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section, STATE.md/NEXT_SESSION.md and current Git HEAD/upstream. Preserve all code and existing metrics, no129-group re-audit or original-crash replay.9791682 defers final buffer descriptors until after attachments with full-copy generation proofs;0d5b469 fills late legal DMA backing via physical B without resolving independent I. Nine narrow native fixtures plus exact-runtime-converter DMA run passed; clean committed Release/complete runtime/EXE-PDB identities are in H/RELEASE_PROOF_0d5b469.json, H=F:/PS4Dev/handoff/attachment-backing-20261017. These are mechanism checks, not current-core gameplay acceptance. Next production C++: separate initial raw DMA request physical-B intent in ExecuteGuestResources opaque_dma/ordinary preparation and PrepareOrderedGuestStages. The legacy ReadMemory/PrepareGpuBacking calls still resolve opaque I. Reuse PreparePhysicalDmaBuffers and existing NOACCESS/I-B/guest shader controls; preserve original binding order, CPU streams, contents, ownership, states/fences/lifetime and final generations. Keep unsupported ordered opaque/SRV-UAV/pressure guards until adapted. Build/check related mechanisms with real GPU readback, commit/checkpoint and ordinary push. Full suite and minimal XInput/game/real-frame FPS comparison remain final. Never auto-install/select cores or restart the user's app/session. Full goal remains unfinished; continue from the checkpoint.

## NEXT CODEX SESSION — START HERE (initial-DMA/private-index/GPU-fill checkpoint)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section plus STATE/NEXT_SESSION and verify Git HEAD/upstream. Preserve source/metrics; no129-group re-audit or original crash replay. Production fcb67e1/2d5e107/7a59dd26410aa2ba29b38d8fd1845af8b90c44fa separates raw DMA physical backing from shader stages, prepares final IA after bindings, preserves original <=16KiB clean CPU index streams and writable GPU-fill envelopes. Clean committed Release/current exact runtime is H/artifacts-7a59dd2, H=F:/PS4Dev/handoff/initial-physical-dma-20261017; RELEASE_PROOF/NATIVE_VERIFICATION manifests retain source/EXE/PDB/compiler/test identities.2d5e107 has12 scoped GPU passes;7a59dd2 has fill/envelope and DMA passes but native_copy_dirty_ranges fails on large DMA pending-list submission. Keep that oracle. Next isolate RunLargePacket ownership/preparation vs growth Finish boundaries in CopyBufferBatchImpl/PrepareShaderBuffersImpl/RematerializeBufferEnvelope; no production cause is yet proven. Plain Rasterizer has no ownership callbacks there; CDB lacked fixture PDB, so don't repeat unresolved symbolic breakpoints. Fix the real contract, rerun related GPU/readback cases, build clean committed Release and archive exact identities. Then resume mixed-class/pressure/ordered opaque/converted/shifted gaps; nine IDs remain. Use saved Vulkan/DX12 measurements; broad suite/minimal final XInput gameplay/image/control/stability/real-frame FPS comparison remains final. Don't auto-install/select cores, touch original Vulkan/saves or restart the user's app/session. Commit/checkpoint and ordinary push; full goal remains open.

## NEXT CODEX SESSION — START HERE (owned-envelope/pending-DMA checkpoint)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section plus STATE/NEXT_SESSION and verify Git HEAD/upstream. Preserve source/metrics, no129-group re-audit or original-crash replay. Production51ec0979800b23ae647c0d779714085bf251ea8c fixes pending-list DMA growth: all-owned B versions copy on the GPU timeline, inherit exact dirty spans/CPU baselines/generations and borrow old guards; only new gaps acquire protection. CPU stores/rejected acquisition reconcile, exceptions roll back new guards and preserve old maps. Eleven scoped GPU/readback cases plus exact-runtime-converter DMA pass; clean committed Release/runtime/EXE-PDB identities are in H/RELEASE_PROOF_51ec097.json and NATIVE_VERIFICATION.json, H=F:/PS4Dev/handoff/dma-copy-pending-20261017. The old large-DMA failure and missing-callback hypothesis are superseded: callbacks existed; stacks proved eager growth ResolveGpuBuffers/Finish. No current core/game/FPS acceptance. Next production work: incompatible native SRV/UAV envelope splitting and clipped GPU-only B in PrepareShaderBuffersImpl/RematerializeBufferEnvelope. Preserve pinned one-arena alias visibility/order, full contents/CPU winners/PageWatch/bounds/lifetime; don't hide it with a stale SRV snapshot or remove guards prematurely. Then remaining pressure/ordered opaque/converted/shifted/nine-ID backlog. Use saved Vulkan/DX12 measurements, build/check only affected mechanisms with real readback, clean committed Release and exact binary identities. Full suite/minimal final XInput gameplay/image/control/stability/real-frame FPS baseline comparison remains final. No auto-install/select, original Vulkan/save changes or user-process restart. Commit/checkpoint and ordinary push; full goal stays open.

## NEXT CODEX SESSION — START HERE (disjoint native buffer views)

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read newest SESSION_HANDOFF.md, STATE/NEXT_SESSION and verify current HEAD/upstream. Preserve source/metrics; no129-group re-audit or original-crash replay. Productiona39144a72db03df98836296d64d8c319afc6d01e keeps canonical B for byte-disjoint native SRV/UAV ranges; leased GPU read copies refresh at final binding across preserving generations, leaving true same-command aliases on existing compiler-unified UAV storage. Eleven scoped GPU passes plus alias/DMA with exact runtime converter are archived at H=F:/PS4Dev/handoff/disjoint-buffer-views-20261017, NATIVE_VERIFICATION.json/RELEASE_PROOF_a39144a.json. Complete manual-folder runtime is H/artifacts-a39144a; no current-core/game/FPS acceptance. Next real C++ work: inactive GPU-only B pressure backing in Find/PrepareShaderBuffersImpl. Preserve B1, CPU B0/store proofs, exact dirty visibility, semantic residency, active generations and actual fence leases; don't remove guards before a complete spill/restore/verification path. Then remaining SRV/DMA, ordered opaque/converted/shifted I/B and other nine-ID backlog. Use saved Vulkan/DX12 measurements and authored fixtures, build/check only affected GPU/readback cases, reproduce clean committed Release and archive exact binary/compiler inputs. Full suite/minimal final XInput gameplay/image/control/stability/real-frame FPS comparison remains final. No auto-install/select, original Vulkan/save changes or user-process restart. Checkpoint and ordinary push; full goal remains open.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section and verify actual HEAD/upstream; restore from the local checkpoint, not chat history. The persistent B pressure/allocator phase is implemented and narrowly verified at2e01aa6; retain exact native/core identities and use NATIVE_VERIFICATION_2e01aa6-final.json, not the invalid early archive. Reuse existing Vulkan/DX12 measurements and all completed diagnostics. Next settle actual reflected SRV/DMA-UAV reachability against production native_storage_buffer_uavs conversion and existing alias/DMA fixtures, then implement queued ordered opaque depth/stencil I/B transfer. Preserve contents, original guest visibility/order, CPU-store proofs, semantic residency and real fence leases. Add meaningful narrow GPU/readback regressions, build committed source from a clean checkout, retain exact EXE/PDB identities. Do not repeat the129-group audit. Finish applicable remaining port mechanisms before final suite/minimal necessary XInput gameplay acceptance and real completed-frame baseline comparison. No automatic kernel installation/selection or user-process restart. Commit/push verified progress and update this handoff; full goal stays active until implementation and required acceptance finish.

## NEXT CODEX SESSION — START HERE

Continue the active full Skiters/ps4-dx12 port at F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section, verify HEAD/upstream, retain the latest checkpoint and immutable native/core identities. Production pressure backing is complete/narrowly verified at2e01aa6; readonly sharp/DMA production unification has new standalone/linked GPU proof at9fdd733. Use existing Vulkan/DX12 baselines; do not repeat completed audits/diagnostics. Next implement ordered opaque depth/stencil independent I/B transfers at UploadVersion/RetileImageVersion and the GuestRasterizer independent-version callback, preserving separate planes, contents/CPU-store proofs, exact publication/order/resource state/lifetime. Reuse prepared independent/depth fixtures, add focused GPU/readback regressions and fix failures. Build production changes from committed clean source, archive exact binaries/results. Finish remaining agreed port mechanisms, then final suite and minimal XInput gameplay/image/control/stability/real completed-frame baseline comparison. No automatic kernel installation/selection or user-process restart. Commit/push verified progress, update handoff and continue from this checkpoint; do not mark the full goal complete prematurely.

## NEXT CODEX SESSION — START HERE

Continue the active full Skiters/ps4-dx12 port in F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section and verify actual HEAD/upstream; recover current committed code, not old chat history. Single-plane D16/D32 ordered I/B and valid smaller-mip source lookup are implemented/narrowly GPU verified at36f1373:8 exact fixtures, six depth configurations, complete48-case matrix, clean Release and immutable identities. Reuse saved Vulkan/DX12 measurements and completed diagnostics; do not rerun the129-group audit or175s matrix without a new reason. Next implement connected split-stencil/MSAA-depth I/B planes at UploadVersion/RetileImageVersion and GuestRasterizer independent transfer/ownership/protection/retirement callbacks. Preserve separate B0/B1/I/S, original depth-only formatted aspect, exact ordering/residency/CPU-store proofs/native state and lifetime. Use existing MSAA-depth/native-plane fixtures plus focused new GPU/readback controls; fix violations, compile/link committed clean source and preserve exact binaries/results. Complete all remaining agreed mechanisms, then final suite and minimal necessary existing-XInput gameplay/image/control/stability/real completed-frame baseline comparison. Do not auto-install/select kernels or restart user processes. Commit/push verified progress and update authoritative handoff; full goal stays active until implementation and required acceptance complete.

## NEXT CODEX SESSION — START HERE

Continue the active full Skiters/ps4-dx12 port at F:/PS4Dev/work/shadps4-dx12-src, fix/mixed-cmask. Read the newest SESSION_HANDOFF.md section and verify actual HEAD/upstream; recover committed current work, not old chat.99cb1f4 connects paired CPU/GPU depth/stencil uploads and portable MSAA stencil write-mask passes;9 exact fixtures,24 actual paired cases and partial controls pass on PSStencilRef=0, with a clean committed Release and immutable identities. Full paired I ownership and game acceptance are unfinished. Next implement native I-to-private depth/stencil plane capture at RetileImageVersion/read_independent callback, then paired PageWatch/ownership/boundary proofs/retirement in GuestRasterizer. Preserve D16/S8 native-vs-guest plane conversion, I/B0/B1/S contents, original formatted depth-only aspect, exact visibility/order/residency/state/lifetime. Reuse existing Vulkan/DX12 measurements, new per-plane inputs/native probes and executed MSAA stencil controls; do not repeat129-group audit or capability investigation. Add meaningful narrow GPU/readback/ownership regressions, fix failures, build committed clean source and preserve exact artifacts. Complete remaining agreed port mechanisms, then final suite and minimal necessary existing-XInput gameplay/image/control/stability/real completed-frame baseline comparison. No automatic kernel install/selection or user-process restart. Commit/push verified progress, update authoritative handoff and continue from the latest checkpoint; keep full goal active until implementation and required acceptance finish.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 from docs/codex-progress/SESSION_HANDOFF.md first authoritative native-I capture section. Verify actual HEAD/upstream and source a2acbc3 after f0d047a; preserve all work. Use saved Vulkan/DX12 and exact native-fixture manifests, not old-chat archaeology. Next C++ task: paired per-plane PageWatch/ownership/boundary proofs and safe retirement, then connect independent UploadVersion callback to existing private CaptureDepthPlane D/S inputs. Keep rejecting guards until contents/visibility/order/state/lifetime are implemented and narrowly GPU/readback verified. Do not repeat129-group audit, proven crash replay, readonly DMA/pressure or unchanged175s matrix without cause. Narrow tests/compilation are authorized; full suite and expanded XInput/game/real completed-frame FPS comparison are final. Build a committed clean Release, preserve exact identities/evidence and previous builds/saves/Vulkan. Manual folder kernel only; no auto-install/selection. Ordinary push verified checkpoints and continue full port from latest checkpoint without waiting for approval.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 full port from SESSION_HANDOFF.md first authoritative separate-plane-boundary section. Verify actual HEAD/upstream and e384ca5 source after a8504e1/a2acbc3/f0d047a; preserve all work and exact saved Vulkan/DX12/GPU evidence. Next C++ work: paired physical ownership geometry and atomic acquisition/demotion/rollback in DescribeGuestImageOwnership/OwnIndependentImage, then paired independent UploadVersion inputs and CPU visibility/pressure retirement. Reuse completed per-plane boundary infrastructure and native CaptureDepthPlane transfers; retain rejecting admission guards until all contracts are connected. Use narrow real GPU/readback/fault fixtures, then committed clean Release and exact identities. Full suite/expanded game through existing XInput/real completed-frame FPS comparison versus baseline remain final. No129 audit/completed crash replay/unchanged175s matrix without cause; no auto-install/selection or original Vulkan/save mutation. Ordinary push verified checkpoints; keep full-port goal active and continue from this checkpoint without confirmation.

## NEXT CODEX SESSION — START HERE

Continue the active full Skiters/ps4-dx12 port from SESSION_HANDOFF.md first authoritative transactional-guards/compact-depth section. Verify HEAD/upstream and source2be23d9 after6f44e15; preserve all work and exact Vulkan/DX12/native evidence. Next C++ task: paired physical ownership geometry/admission in DescribeGuestImageOwnership/retention predicates, then applicable per-plane independent UploadVersion/read callback conversion, source authority/provenance and real fences. Reuse completed transactional acquisition, separate boundary maps, private native plane captures and compact readback. Pinned DownloadImageMemory is depth-only even with stencil; do not publish S or demand unsupported upstream MSAA copies. Keep remaining admission/transfer guards until correctly connected, then run targeted real GPU/PageWatch/priority-retirement cases and committed clean Release with exact identities. No129 re-audit/completed crash replay/unchanged175s matrix without cause. Full suite/expanded existing-XInput gameplay and real-frame FPS versus saved baseline remain final. No auto-install/selection or original Vulkan/save/user-session mutation. Ordinary push verified checkpoints; continue from latest checkpoint without confirmation, keeping full-port goal active.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 from SESSION_HANDOFF.md first authoritative paired-independent section. Verify actual HEAD/upstream and52add96 source; preserve all work and exact saved Vulkan/DX12/native evidence. Exact-domain D/S ownership/conversion is connected and8 scoped fixtures passed: do not redo it or129 audit. Next C++ work: explicit per-plane mapping for depth/S resource growth, source/target mips/layers/clipped geometry in CaptureIndependentImageVersion/UploadVersion, preserving target-only B seed/padding, source I and real fences. Then pinned color channels -> MSAA depth repacking and remaining ordered aliases. Match CopyImage versus depth-only CopyImageWithBuffer/DownloadImageMemory aspects; do not invent S publication or unsupported-original success. Narrow real GPU/PageWatch/readback tests and a committed clean Release are authorized. Full suite/expanded existing-XInput gameplay/real-frame FPS versus baseline stay final. No completed crash replay/unchanged175s matrix without cause, auto-install/selection, original Vulkan/save or user-session actions. Ordinary push verified checkpoints and continue full port from the latest checkpoint, keeping the goal active.

## NEXT CODEX SESSION — START HERE

Continue the active full Skiters/ps4-dx12 port from SESSION_HANDOFF.md first depth-growth/ASPECT CORRECTION section. Verify HEAD/upstream and a7e7ea5 after a4a51e8; preserve all work and saved Vulkan/DX12/GPU evidence. IMPORTANT: both pinned CopyImage and CopyImageWithBuffer EXCLUDE S; prior52add96 S-copy parity claim was wrong and is explicitly superseded. Direct explicit CopyDepthStencil is separate.16 typed/native mip/layer growth cases plus8 related fixtures passed at exact archived binaries; do not redo growth/129 audit. Next C++ task: pinned single-color components -> MSAA-depth repacking in CopyColorAndDepth/ReinterpretColorAsMsDepth, using actual pinned helper/shader semantics and native GuestMultisample/CaptureIndependentImageVersion/UploadVersion. Preserve format bits, sample order, target init/aspects, source authority, deferred residency and real fences. Then smaller-view/rebinding over grown I, spatial/shifted/clipped mappings and existing64-MiB guards/other queue. Narrow real GPU/PageWatch/readback checks and committed clean Release are authorized; full suite/expanded existing-XInput gameplay/real-frame FPS versus baseline remain final. No completed crash replay/unchanged175s matrix without cause, auto-install/selection, original Vulkan/save or user-session changes. Ordinary push verified checkpoints and keep full-port goal active.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 on fix/mixed-cmask from 616fb45873af98a0eda0ea294f633473f866ce5f. Read SESSION_HANDOFF.md FIRST section, STATE.md, NEXT_SESSION.md and VULKAN_DX12_PARITY.md; verify actual HEAD/upstream and dirty sources before edits. e94f886 connects typed color->MS depth; 616fb45 changes only the growth fixture's single-sample B-effect oracle. Reuse H=F:/PS4Dev/handoff/color-to-ms-depth-20261017 exact6-test/one-test identities,28 new2x/4x/8x cases and explicit16x hardware skips. Do not transfer native fixture passes to rebuilt executables or the unexecuted whole core. Continue production C++ cached smaller-view reuse/rebinding over grown depth I in guest_texture_cache.cpp with correct views/ownership/fences and preserved other mips/layers; then remaining spatial/packed/source/ordered gates. Reuse Vulkan/DX12 baselines, avoid129 re-audit/crash replay/finished pressure/DMA experiments. Narrow compiler/regression/GPU checks are authorized; full suite/expanded existing-XInput game/image/control/stability/real completed-frame FPS vs baseline remain final. Build reproducibly from clean committed checkout with pinned deps, keep immutable runtime identities/manual-folder kernels, ordinary push verified checkpoints, preserve original Vulkan/saves/user processes and keep the full-port goal active until actual completion.

## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from 92f085f4e285258ea9491f7c3db81cc5d1a21f01. Read SESSION_HANDOFF.md FIRST section, STATE/NEXT_SESSION/VULKAN_DX12_PARITY and verify actual HEAD/upstream/dirty sources. Production9fa2125 restores original overlap class admission; 92f085f changes only cache-positive fixture mip inputs. Reuse H=F:/PS4Dev/handoff/depth-subviews-20261017 exact7+4 manifests;32 final scenarios include10 eligible cache blits and22 pool controls/explicit primitive checks. Old e94f886 cache-admission claim is superseded;16x remains hardware unverified. Continue production C++ smaller-depth-view reuse over grown parent I: use H/NEXT_SUBVIEW_DESIGN.md focused findings/next reads, keep full allocation/ownership/guest layout and correct requested SRV/DSV selectors, connect ordered Domain/caller gates and actual dirty-I rebinding safely. No blanket guard removal, prefix retile or new129 audit. Descriptor probe already passed creation only; implement independent GPU pixel/DSV-write/guard/fence regression next. Use saved Vulkan/DX12 baselines; narrow builds/GPU fixtures authorized; full suite/expanded existing-XInput game/image/control/stability/real FPS acceptance remain final. Preserve exact tested identities, build clean pinned Release, manual-folder kernels only, ordinary push verified checkpoints and keep full-port goal active. Do not touch original Vulkan/saves/user sessions or repeat completed crash/DMA/pressure audits.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from production 21d1e93cd51a930e29b65d0ec7a7fd1831bba152 and latest docs checkpoint. Read SESSION_HANDOFF.md FIRST section, verify actual HEAD/upstream/dirty sources and restore saved WIP. Reuse H=F:/PS4Dev/handoff/depth-subview-reuse-20261017 exact five-test identities and prior Vulkan/DX12 baselines; do not reconstruct old chat or repeat completed129/crash/DMA/pressure work. Fixed-base D16/D32 smaller-view cache/caller/ordered single-sample reuse and native-S-preserving D refresh are implemented with8 real GPU cases. Continue production C++ ResolveDepthOverlap recreation with lexicographic max resources, full physical source/prewrite capture, optional-S initialization/lifetime, then remaining guarded geometry/source/ordered-MS/64-MiB paths. Keep implementation/verification separate. Build clean committed Release; run relevant independent GPU/readback/fault/fence fixtures for each completed mechanism; preserve exact binary identities. Full suite and minimum necessary final existing-XInput game/image/control/stability/completed-frame FPS-vs-baseline acceptance follow completed port, excluding generated frames. Manual-folder cores only; no automatic install/select, original Vulkan/save/user-session changes. Ordinary push verified checkpoints and continue the active full-port goal.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from production 2ac536b80053e6a4dee6e6a94f1fd9c20625cb72 and latest docs checkpoint. Read SESSION_HANDOFF.md FIRST section; verify actual HEAD/upstream/dirty files and restore saved WIP. Reuse H=F:/PS4Dev/handoff/depth-recreation-extents-20261017 exact two-test identities and prior21d1e93 five-test/Vulkan-DX12 baselines; do not reconstruct chat or repeat completed129/crash/DMA/pressure audits. Subview/full-parent/native-S-preserving refresh and applicable recreation max-resource selection are connected. Next verify pinned CopyColorAndDepth/CopyImage intersection then implement crossed mip/layer mappings in CanMapIndependentDepthImage/mapped RetileImageVersion, beginning source1mip/3layers ->target2mips/1layer, with independent native target-layout/seed/S/padding/ownership/fence regression. Preserve original lexicographic resources/root admission/exact-base GPU invalidation. Continue other guarded source/geometry/ordered-MS/64-MiB gaps. Build clean pinned committed Release and relevant narrow GPU fixtures; save exact binary identities and implementation-versus-behavior states. Full suite and minimum necessary final existing-XInput game/image/control/stability/real completed-frame FPS-vs-baseline acceptance remain required, excluding generated frames. Manual-folder cores only; no automatic install/select or original Vulkan/save/user-session action. Push verified checkpoints normally and continue active full-port goal.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from production 1a35c8b21e27fbdaa751ee276d55d11f9fa267ab, package a09007a030dab4220ff79c6ce5db0d5a27fcb003 and latest docs checkpoint. Read SESSION_HANDOFF.md FIRST section, verify actual HEAD/upstream/dirty files, restore any WIP. Reuse H=F:/PS4Dev/handoff/depth-crossed-resources-20261017 exact initial4/final2 native identities and saved Vulkan/DX12 baselines. Source readback before/after transfer and native-helper/original-branch scopes are distinct; do not repeat tests to fill metadata or infer requirements from original unimplemented equal-MS/aspect branches. Crossed fixed-base D16/D32 mapping is implemented. Next read H/NEXT_DIRTY_DEPTH_RECREATION.md and UPSTREAM_DIRTY_RECREATION.json; implement distinct cached-native DIRTY recreation source with initialized class identity/old native D contents, full target extent/B seed/S, caller prewrite integration and real resource/pin/fence lifetime. Keep ordinary stale-I ownership/readonly guards; never fake success/current validity or publish old I to physical B. Add genuine PageWatch CPU-store/native GPU readback regression with independent old-native versus new-B data, then continue remaining guarded geometry/source/ordered/size paths. Build clean pinned committed Release; narrow GPU checks per completed mechanism. Full suite and minimum necessary final existing-XInput game/image/control/stability/real completed-frame FPS-vs-baseline acceptance remain required, excluding FG. Manual-folder cores only; no automatic install/select or original Vulkan/save/user-session action. Push verified checkpoints normally and continue the active full-port goal.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from production/source 1e2fae2cdf7bc5d2ea3b8c6d2c1bb325516a15fe and latest docs checkpoint. Read SESSION_HANDOFF.md FIRST section; verify current HEAD/upstream/dirty sources and restore WIP. Reuse H=F:/PS4Dev/handoff/dirty-cached-depth-recreation-20261017 exact final2 native checks, separately scoped prior5 related results and existing Vulkan/DX12 baselines. Do not repeat129/crash/completed studies. Dirty fixed-base1x depth->depth recreation is connected and GPU-checked, with original stencil PRESENCE and min-mips restrictions. Next implement initialized dirty cached COLOR->depth native class/identity and producer from H/NEXT_CACHED_COLOR_DEPTH.md; preserve stale ordinary I rejection, old native contents versus current B seeds/S, current-independent precedence, BEFORE-prewrite order, metadata and real allocation/descriptor/fence lifetime. Then continue explicit depth->writable/RT and existing geometry/source/ordered/size gaps, without expanding requirements beyond pinned original e0a46675. Add narrow GPU readback regression per completed mechanism, build clean pinned committed Release and preserve distinct runtime identities. Full suite and minimal final gameplay through existing virtual XInput must verify image/input/stability and actual completed guest FPS versus saved baseline, excluding FG. Manual-folder versions only; do not auto install/select or alter original Vulkan/saves/user sessions. Push verified checkpoints normally and continue the active full-port goal.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from source 414094c4eef296b61aeaf6aad79382dc7726df05 and latest docs checkpoint. Read SESSION_HANDOFF FIRST section, verify current HEAD/upstream/dirty code and restore WIP. Reuse H=F:/PS4Dev/handoff/dirty-cached-color-depth-20261017 five exact GPU checks and saved Vulkan/DX12 baselines; do not repeat129/crash/completed studies. Dirty direct fixed-base1x color/depth->depth is connected and narrowly verified. Next implement original cached DEPTH->STORAGE/RENDER_TARGET recreation per H/NEXT_DEPTH_WRITABLE_RT.md: legal UAV/RT format/ownership/Upload flags, old I versus B, original flags/metadata/source lifetime and BEFORE-prewrite color/shader binding order. Preserve readonly depth-parent reuse and strict ordinary stale-I/private guards. Add real writable dispatch/RT draw/native readback/PageWatch/fence regression with independent expected data, then continue existing geometry/source/ordered/MS/size gaps. Build clean pinned committed Release, retain distinct binary/PDB/runtime identities and manual-folder versions. Final suite and minimal game through existing virtual XInput must verify image/input/stability and completed guest FPS versus saved baseline excluding FG. No auto install/select, original Vulkan/save edits or user-session interruption. Push verified checkpoints normally and continue ACTIVE full original-port goal.

## NEXT CODEX SESSION - START HERE

Continue Skiters/ps4-dx12 fix/mixed-cmask from source 94ace23279c472f275c36d556afdf0313e1be444 and latest docs checkpoint. Read SESSION_HANDOFF FIRST section; verify actual HEAD/upstream/dirty sources and restore WIP. Reuse H=F:/PS4Dev/handoff/cached-depth-rt-20261017 four exact native/source checks and prior Vulkan/DX12 baselines; do not repeat129/crash/completed studies. Direct1x cached depth->RT is connected and GPU-checked, but clipped native clear is NOT full GuestRasterizer CMASK runtime proof. Next FIRST implement PRIVATE cached-depth recreation preparation per H/NEXT_PRIVATE_RECREATION_PUBLICATION.md: no early cache class/metadata/ownership/ordinary-I publication, explicit initialized prewrite source, actual-bind receipt consumption and actual bound DSV producer provenance, source UID/native revision, opposite-plane content/flags, rollback/abandonment/fence lifetime. Add actual ordered-buffer B-before-bind versus old-native I-after-bind readback/metadata/ownership regressions with unchanged independent pixel expectations. Then complete writable destination/shader bind integration and remaining guarded CMASK/RT formats/geometry/source/ordered/MS/size paths. Build clean pinned committed Release, preserve exact EXE/PDB/runtime evidence/manual-folder versions and normal verified push. Full suite and minimal final game through existing virtual XInput must check image/input/stability and completed guest FPS-vs-baseline excluding FG. No auto install/select, original Vulkan/save edits or user-session interruption; continue ACTIVE full original-port goal.

## NEXT CODEX SESSION - START HERE

Continue fix/mixed-cmask from production94ace23279c472f275c36d556afdf0313e1be444, latest source/fixture01a1f6cea7a373b54b404f622ac93d6cf9cd3c26 and latest docs checkpoint. Read SESSION_HANDOFF FIRST section; verify actual HEAD/upstream/dirty code and preserve unknown .serena/CLAUDE.md. Use H=F:/PS4Dev/handoff/cached-depth-rt-20261017 FINAL CB-layer manifest/nativeab7a2c33 plus distinct INITIAL4/native12221e6e and saved Vulkan/DX12 baseline. Extra-mip RT helper cases are not original CB applicability; full CMASK member/runtime is not proven. Do not repeat129/crash/completed audits or tests just to fill metadata. Next FIRST implement private cached DEPTH preparation and actual-bound-DSV receipt/provenance/native revision/ownership/metadata/fence commit per H/NEXT_PRIVATE_RECREATION_PUBLICATION.md; verify before-bind original B/meta versus after-bind native I with real GPU operations. Then continue writable shader binding, guarded CMASK/RT format/geometry/source/ordered/MS/size implementations. Build clean pinned committed Release, retain exact separate binary/PDB/runtime/test evidence, manual-folder cores and normal verified push. Final suite and minimum game through existing virtual XInput must check image/input/stability/real completed guest FPS versus saved baseline excluding FG. No automatic install/select or original Vulkan/save/user-session disruption; full goal ACTIVE.


## NEXT CODEX SESSION — START HERE (2026-10-18, d52ac76)

Продолжай Skiters/ps4-dx12 на fix/mixed-cmask. Прочитай ПЕРВЫЙ раздел SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md и VULKAN_DX12_PARITY.md; проверь Git HEAD против d52ac76798704617d12ab7d6e28ac6625799474e и позднейших checkpoint. Восстанови текущую работу из F:/PS4Dev/handoff/private-depth-binding-20261018. Начни с NEXT_DEPTH_WRITABLE_BINDING.md: production перенос cached native depth -> shader-writable color в IsCachedDepthRecreationSource, RecreateCachedDepthAtBind, ExecuteGuestResources с исходным порядком shader bindings, законным UAV и сохранением old native D, B, S, metadata, visibility/order/state/lifetime. Не возвращай раннюю публикацию private depth plan; superseded copy/revision-token дизайн не реализуй без новой причины. Используй сохранённые Vulkan/DX12 метрики и доказанные native/GCN проверки, не повторяй завершённые исследования и129 аудит. Вноси C++/HLSL, собери и узко проверь реальными GPU/readback. Сохраняй точную identity каждого тестового и чистого Release-бинарника, отдельные implementation/behavior статусы, локальные checkpoints и обычный push. Полный suite и минимальные игровые испытания через существующий virtual XInput, изображение/управление/стабильность и сравнение завершённых guest FPS с baseline без FG — только на финальном этапе; игру после каждого коммита не запускай. Новые ядра оставляй в ручном каталоге версий, автоматически в приложение не добавляй. Сохрани .serena/ и неизвестный CLAUDE.md, оригинальный Vulkan, сохранения и пользовательскую сессию. Продолжай с последнего checkpoint, без повторного восстановления истории чата; полный перенос ещё НЕ завершён.


## NEXT CODEX SESSION — START HERE (2026-10-18, 641f6b9)

Продолжай полный перенос Skiters/ps4-dx12 на fix/mixed-cmask с последнего checkpoint. Прочитай ПЕРВЫЙ раздел SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md и VULKAN_DX12_PARITY.md; проверь HEAD относительно 641f6b9c057d23c489f0bda2cc52d046d72fa3c9 и позднейших checkpoint. Код и измерения: F:/PS4Dev/handoff/depth-writable-20261018. Начни production перенос из NEXT_NATIVE_DEPTH_COLOR_FORMATS.md: IsCachedDepthRecreationSource, CanMapIndependentDepthImage, ResolveDepthViewStorage — остальные исходные bit-compatible16/32 color-классы, затем native-only CMASK decoder/components/clear с сохранением old native D, S, B, padding, ownership/order/state/lifetime. Direct2/4 storage уже подключён; отдельные runtime-checks других number formats не обозначают отсутствующий код. Сохраняй CachedImageBindingPlan и actual BindPlannedImage, удерживай только HasCurrentNativeContents I, не оживляй unowned stale I. Vulkan связывает Fragment/PS раньше Vertex/VS; не путай с GPU execution order. Используй сохранённые Vulkan/DX12 baseline, не повторяй129 аудит и завершённые исследования. Вноси C++/HLSL, собирай и проверяй законченные механизмы узкими реальными GPU/readback. Авторские compute fixtures должны идти через реальную очередь PM4, не вызывать GetCsRegs вне неё. Сохраняй точные identity source/EXE/PDB/runtime, implementation и behavior отдельно, clean Release/manual-folder версии и обычный push. Полный финальный suite и минимальная игровая приёмка через существующий virtual XInput, изображение/управление/стабильность/завершённые guest FPS против baseline без FG — только на финальном этапе. Не запускай игру после каждого коммита, автоматически ядра в приложение не добавляй. Сохрани .serena/, неизвестный CLAUDE.md, original Vulkan, saves и пользовательскую сессию. Продолжай с checkpoint без восстановления старого чата; цель ACTIVE/INCOMPLETE.


## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 from the current checkpoint. Read the FIRST authoritative section of docs/codex-progress/SESSION_HANDOFF.md, STATE.md, NEXT_SESSION.md, VULKAN_DX12_PARITY.md and H/NEXT_NATIVE_FORMAT_CODE.md (H=F:/PS4Dev/handoff/depth-color-formats-20261018). Check actual HEAD/upstream/status; source e1eedf9 is preserved, docs HEAD recorded in END_STATE.json. Restore only the explicitly unfinished work. Reuse saved Vulkan/DX12 metrics and exact fixture identities; no repeated129 audit/original crash/completed studies. Continue production C++/HLSL native source packing in RetileImageVersion / GuestTiler before opening remaining original-capable packed classes. Keep explicit replaced-exclusive-image guard transfer, stronger B guards, exact contents/ordering/lifetime, capacity-only plans and original Fragment-before-Vertex binding. Distinguish connected, checked and concrete remaining code gaps. Compile and run narrow real GPU/readback regressions/negative cases per finished mechanism; save source, clean pinned Release, exact binary identities, local commits and normal upstream push. New cores go to manual folders; no auto install/select. When the whole agreed port is implemented, run final suite and minimal existing-XInput game/image/input/stability/completed guest FPS comparison to saved Vulkan baseline excluding FG. Preserve original Vulkan, user saves/session and unknown context files. Continue from last checkpoint; full port remains INCOMPLETE.


## NEXT CODEX SESSION — START HERE

Continue full Skiters/ps4-dx12 port from latest checkpoint. Read FIRST section of docs/codex-progress/SESSION_HANDOFF.md plus STATE.md/NEXT_SESSION.md/VULKAN_DX12_PARITY.md and F:/PS4Dev/handoff/native-packed6-20261018/NEXT_NATIVE_PACKED16.md. Verify actual HEAD/upstream/status (source fd8e5f6, docs SHA in END_STATE.json). Restore only unfinished source codec work. Reuse saved Vulkan/DX12 metrics and exact accepted fixture identities; no repeated129 audit/original crash/completed studies. Continue production C++/HLSL RetileImageVersion/GuestTiler source native packing separate from target addressing before opening original-capable16..19. Preserve original PS-before-VS, nonpublishing capacity plans/current I witness, explicit replaced-exclusive-I guard transfer/stronger B guards, enhanced model at CreatePlacedResource2 creation and resource/fence lifetime. Keep connected/verified/concrete-code-gap states separate. Build and run narrow GPU/readback/negative checks per completed mechanism; save clean pinned Release/manual-folder versions, source/EXE/PDB identities, commits and normal upstream push. Full final suite and minimal existing-XInput game/image/input/stability/completed guest FPS versus saved Vulkan baseline excluding FG only when full agreed port is implemented. Preserve original Vulkan/user saves/session/unknown context; no automatic install/select. Continue from last checkpoint; goal remains INCOMPLETE.


### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. Read the FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/packed16-codec-20261018/NEXT_ORDERED_DOMAIN.md, check actual Git HEAD/status/upstream and restore from the latest checkpoint without resetting. Source-native packed codec is implemented and narrowly GPU-proven at74519e5/e277da5/edb2d69 with separate binary identities; reuse saved Vulkan/DX12 metrics and capability evidence. Do not repeat completed audits/crash/matrices. Continue C++ late ordered cached-resource growth at PlanCachedImageBinding/BindPlannedImage/PrepareOrderedGuestStages/ValidatePreparedShaderBufferRange while preserving original I/B ownership/residency/binding order, barriers and lifetime. Build and run minimal actual GPU negative/readback regressions per finished mechanism. Maintain129 independent implementation/behavior records, clean committed Release and manual-folder core versions; normal push verified commits. Only at final acceptance run full suite and minimal existing virtual XInput game/image/input/stability checks, comparing completed guest frames with saved Vulkan baseline excluding FG. Full port remains incomplete; continue from last checkpoint.


### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 in F:/PS4Dev/work/shadps4-dx12-src on fix/mixed-cmask. Read FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/packed19-msaa-20261018/NEXT_EXISTING_ALIAS_DOMAIN.md; verify actual HEAD/status/upstream, preserve all current work. Native packed19 alpha/transfer production fix b5029e1 is compiled and narrowly GPU-proven with separate fixture/core identities; reuse saved Vulkan/DX12 metrics and source/capability evidence, do not repeat completed audits/crash/tests. Continue existing C++ converted-domain guard in PrepareOrderedGuestStages / RefreshBoundImage with original I/B publication, binding order, resource contents/states/visibility/lifetime and stronger PageWatch protections. Build/actual GPU negative-readback test each finished mechanism; preserve129 separate implementation/behavior records, clean committed Release and manual-folder kernels; normal upstream push verified commits. Only at final run full suite plus minimum existing virtual XInput image/input/stability/game checks and completed guest FPS comparison excluding FG. Full port incomplete; continue from latest checkpoint.


### NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 at F:/PS4Dev/work/shadps4-dx12-src, branch fix/mixed-cmask. Read FIRST SESSION_HANDOFF.md section and F:/PS4Dev/handoff/readonly-numeric-20261018/NEXT_EXCLUSIVE_I_TRANSFER.md, verify HEAD/status/upstream and preserve all work.21445a2 fixes reentrant closed command lists and readonly numeric typeless sharing;62c0d3b restores pending receipts; scoped GPU proofs have separate exact identities. b19feb3 provides current known-failing readonly-numeric-exclusive-owned: preserve oldff0000ff I after standalone image_store +interior B writer; actual typed views read B13579bdf. Localize ownership/dirty/history transition and fix production C++ without weakening guards,changing expectations or replacing the standalone case with joint seed. Reuse pinned source/metrics; no repeated audit/crash/unchanged full matrices. Build/narrow actual GPU negative/readback checks per mechanism, clean committed Release, manual kernel folders and normal push. Keep129 implementation/behavior records separate;8 broad gaps remain. F had disk-full; only new H was safely compressed and hashes checked; use C for new large evidence if needed, never delete old builds/saves. Full suite and minimal existing XInput game/image/input/stability/completed guest FPS comparison excludingFG remain final. Continue from latest checkpoint; full port incomplete.

## NEXT CODEX SESSION — START HERE (exclusive-image checkpoint)

Continue Skiters/ps4-dx12 on fix/mixed-cmask. Read the FIRST section of docs/codex-progress/SESSION_HANDOFF.md and STATE/NEXT_SESSION/VULKAN_DX12_PARITY, verify actual HEAD and preserve unknown files. Sourcec20963d fixes confirmed exclusive-I retirement during buffer admission; use C:/Users/skei/Documents/Codex/handoff/exclusive-i-20261018/VERIFICATION_FINAL.json and existing Vulkan/DX12 baselines. Do not repeat129 audit, completed matrices or original crash. Continue converted/multiview ordered image-domain transfer in guest_stage_bindings.cpp, preserving original I/B/history/protection/order/lifetime. Compile and run only relevant narrow real GPU/readback cases, fix defects, commit, verify clean Release and normal push. Store every core identity separately in manual folders; no auto app install/select. Full suite and minimal existing-XInput game/image/input/stability/completed guest FPS excluding FG comparison remain final. Continue full port from this checkpoint; it is incomplete.

## NEXT CODEX SESSION — START HERE (readonly word views)

Continue Skiters/ps4-dx12 on fix/mixed-cmask. Read FIRST SESSION_HANDOFF section, STATE/NEXT_SESSION/VULKAN_DX12_PARITY and verify HEAD; preserve unknown .serena/ and CLAUDE.md. Source7b6985f has narrow GPU-verified readonly R32/RGBA8 common allocation, following c20963d exclusive-I protection fix. Use C:/Users/skei/Documents/Codex/handoff/exclusive-i-20261018/{WORD_VERIFICATION,VERIFICATION_FINAL}.json and existing Vulkan/DX12 baselines. Do not repeat129 audit, completed matrices or d019 crash. Continue production ReadonlyWordCasts/CompatibleGuestReadonlyWordStorage for pinned direct32-bit readonly data5/6 with independent channel/word GPU oracles; preserve I/B/history/order/protection/leases and resource cast-list/device gates. Then existing converted/multiview/shifted/MS/large gaps. Build incrementally, relevant narrow GPU checks, clean committed Release, retain exact identities/manual folders, normal push. No auto install/select. Full suite/minimal existing-XInput game/image/input/stability/completed guest FPS excluding FG versus baseline remain final. Full port is incomplete; continue from this checkpoint.

## NEXT CODEX SESSION — START HERE (signed packed word views)

Continue Skiters/ps4-dx12 fix/mixed-cmask. Read the FIRST SESSION_HANDOFF section and STATE/NEXT_SESSION/VULKAN_DX12_PARITY; verify HEAD, preserve unknown context files. Production8a8f3db connects direct readonly mutable32 classes4/5/6/9UNORM-UINT/10/34; source package87ca490 adds the concrete unresolved signed word case. Use C:/Users/skei/Documents/Codex/handoff/mutable32-20261018/{VERIFICATION_FINAL,FINAL_GCN_VERIFICATION}.json and NEXT_SIGNED_WORD.md; retain saved Vulkan/DX12 metrics. Continue actual production compiler/backend readonly raw-word adaptation for readonly-snormword-owned (source I0xc00805ff, independent B13579bdf), not another129 audit or repeated completed matrix. Preserve contents/visibility/order/native class/history/protection/filtering/lifetime, no fake writer or disabled guard. Native SINT mode is prepared, not_run. Build, relevant GPU/readback controls, fix defects, clean committed Release, exact identities/manual folders and normal push. If the diagnostic still terminates after the explicit guard is resolved, capture its actual exception context before assigning a cause. Original d019 crash is separate, already captured/fixed. Full suite/minimal existing-XInput game/image/input/stability/completed guest FPS excluding FG versus baseline remain final. No auto app install/select. Full port incomplete; continue from this checkpoint.

## NEXT CODEX SESSION — START HERE (writable packed/scalar words)

Continue Skiters/ps4-dx12 fix/mixed-cmask. Read the FIRST SESSION_HANDOFF section, STATE/NEXT_SESSION/VULKAN_DX12_PARITY, verify HEAD and preserve unknown context files. Sourceec559dd connects true readonly packed10 R32-SRV decode, per-texel filtering/swizzle, specialization, actual cached-I proof and early source pins; the saved SNORM/R32 failure is FIXED, not the next task. Use C:/Users/skei/Documents/Codex/handoff/readonly-packed9-20261018/VERIFICATION_FINAL.json and NEXT_WRITABLE_WORD_ALIAS.md plus existing Vulkan/DX12 metrics. Continue production writable raw9/4 shared-image compatibility using existing packed10_image_test.cpp alias-coherent read/store/barrier/read oracles, original/native classification and then relevant real PM4. Do not repeat128 passing cases without changed-code reason,129 audit, old crash or unrelated matrices. Preserve original I/class/history/UID, B residency/publication, guest permissions, geometry, visibility/order/guard/fence/resource lifetime; no skipped operations or weakened oracles. Compile incrementally, narrow GPU checks, clean committed Release/exact identities/manual folder and normal push. No auto app install/select. Full suite/minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus baseline remain final. Full port incomplete; continue from this checkpoint.


## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 from SESSION_HANDOFF.md FIRST current section and verify actual Git HEAD. Read H/NEXT_CACHED_WORD_ALIAS.md. Preserve current code and unknown files; use existing saved Vulkan/DX12 evidence. Do not repeat129 audits, fixed d019 diagnosis or unchanged broad suites. Continue actual C++ cached R32-to-packed writable ownership/class transition: first real GCN warm I(GpuModified0/1) before mixed9/4 bind, capture concrete failure, preserve current I/B and exact permissions/lifetime, then fix production and narrow GPU readback. Retain independent before/after data and negatives. Clean committed Release with exact identities, manual kernel folder, normal push. Continue remaining port gaps; full suite and minimal existing-XInput game/FPS comparison excluding FG at final stage; checkpoint exact continuation.


## NEXT CODEX SESSION — START HERE

Continue Skiters/ps4-dx12 from SESSION_HANDOFF.md FIRST section; verify actual HEAD. Read H/NEXT_ORDERED_STREAM.md. Preserve code/unknown files, use saved Vulkan/DX12 measurements and exact current identities, avoid completed129 audits/fixed d019 replay. Resume the existing ordered-formatted GCN fixture: replace out-of-queue GetCsRegs/direct dispatch with actual local ComputeProgram and PM4 SetShReg/DispatchDirect; put reflection in the queue. Keep original oracles. Execute prepared narrow I/B/CPU-stream ordering checks and fix actual production at the correct stage; native adapter48 is already passed. Then cached R32 SINT/FLOAT donor capability/bit/history, remaining alias and full-port gaps. Clean committed Release, exact binary identity, manual kernel folder and normal push. Full suite/minimal existing-XInput final image/input/stability/completed FPS excluding FG versus saved baseline stay final. Checkpoint precise continuation; full port remains incomplete.
