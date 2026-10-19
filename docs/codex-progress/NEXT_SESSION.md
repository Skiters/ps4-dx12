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

# Next session

## Latest continuation — 2026-10-19, source 7c794a8

Production835ad54 dirty-alias retirement/current-B refresh remains focused GPU verified (new depth/color fixture7.91s and four related tests115.37s). Clean835ad54 Release10.658s, EXE435d4700c53bd94195f493337fcb6cdbd01632f4a86affca03e7bd646561e3d1/PDB80c6cdb8a7016128102cb925146d39ca6fea7d08eec493ae448847f3df7e49b3 now has actual Continue FAIL: Independent depth/color overlap needs an exact physical transfer plan, exit0x80000003, before world. F:/PS4Dev/handoff/dirty-alias-835ad54 retains run/exit/stderr/profile logs and XInput evidence. Initial virtual pad1001 did not control title; neutral detach/reconnect registered1000, and actual intro skip/title/menu/Continue reaction was observed. Do not confuse earlier intro hunter imagery with world.

7c794a8 adds geometry output only at this unchanged overlap rejection. Main ReleasePASS5.364s after exact source commit, EXE72ad3609f0249620792c045725ca61ca7c69f040167e7d4aeb056bdd1dd92ada/PDB83a0d3fd4597ecfa4396057cb20cddf544a0bf07d54046e67e7f9ade05d7989e. One same-PID78660 run reached menu/Continue via virtual1000, then waited on loading until explicit close after9m17s, exit0; no overlap rejection/world/control acceptance. Non-invasive stacks and actual mutex-owner chain saved at F:/PS4Dev/handoff/depth-overlap-7c794a8/result.txt. GPU queue idle; Game:Main waits owner MOMainThread, which polls guest completion bit0x40 via2ms sleeps. Cause of missing completion and exact failed835 overlap descriptors remain unproven, separate blockers. No blanket mapping widening or kernel patch from these observations.

Next: inspect saved mo-caller/context and identify generic completion producer/wait contract using existing code/evidence before another unchanged game replay; obtain rejected overlap geometry on a causally justified controlled failure before adding its physical mapping fixture. Existing dirty-cache/ownership mechanisms must not be reimplemented. Debugger executable is C:/Program Files/WindowsApps/Microsoft.WinDbg_1.2610.1001.0_x64__8wekyb3d8bbwe/amd64/cdb.exe (not SDK path); local -y avoids symbol-server delay. Sole installed3680578 retained; reusable clean835 crash and main7c diagnostic remain, no new binary archives. Own game/debugger/feeder ended and virtual device removed. Unknown untracked untouched. SESSION_HANDOFF.md remains authoritative. Older sections below are historical checkpoints.

## Current continuation boundary - remaining BC block domains

Production **51abbc717cab66032c60582c20755ffedf57998e** is clean-built and focused GPU verified. The complete 27-case writable BC alias set now covers exact, multilevel, shifted/rebased, duplicate-slot, native-reader and cross-transaction prefix behavior. The sole application core is `dx12-current`, EXE SHA256 **acce7997e1ff08562daf05c3400ced4b84b4336032764f166f3d15a60c7d2b32**. Runtime/game/full suite are not run.

Continue with an original-applicable **MSAA BC block-view domain** from the existing registry, preserving the parent image, exact block words, all samples, padding, visibility, ownership and fence lifetime. Add one focused real-GPU/readback oracle before changing production admission. Clipped and large block domains remain separate following items. Do not repeat the completed exact/multilevel/shifted prefix work. The separate broad `native_resources` `invalid map<K, T> key` failure is unresolved and must not be used as evidence against the accepted BC transaction mechanism without a focused reproducer.


## Current continuation boundary - multilevel BC/word terminal prefix

Production **10e19f15c44dd6313b4d5afa6911c23abeb885e9** is pushed, clean-built and focused GPU verified. Exact one-level and multilevel terminal UINT prefixes now preserve the early BC parent I, late current B, padding, untouched tail, ownership and lifetime. The focused test and complete ordered-stage matrix pass. The sole installed `dx12-current` has EXE SHA256 **be5b93e9f625041ea388bad028511f9741f5153445b54860ccfba932e9775457**; game/final suite were not run.

Continue with shifted/rebased mip and layer block views already present in `gcn_bc_writable_alias_fixture.h`. Exercise the real frontend `RebaseBlockSubresource` -> ordered-stage -> `BeginWritableBlockAlias` / `CommitWritableBlockAlias` chain, including parent tail preservation and actual GPU writes. Do not repeat exact-base or terminal-prefix work. MSAA, clipped and large domains remain separate gaps.



## Current continuation boundary - one-level BC/word ordered domain

Production **4def6f26aca64357010b160cc512270acf0fef57** is pushed, clean-built and focused GPU verified. Exact one-level BC parent and UINT block-word views now use ordered converted allocations and preserve early logical I, late current B, guest padding, ownership and fence lifetime. The focused test, full ordered-stage matrix, independent transfer and BC hardware checks pass. The sole installed `dx12-current` has EXE SHA256 **480282eac32fd2f3d5235309fad387aec5f6bda0103b9f902fd95e1db065ebf4**. Game/final suite were not run.

Continue with the multilevel BC-parent/word-prefix cases already represented in `gcn_bc_writable_alias_fixture.h`. Preserve untouched parent mips and layers while materializing the selected word view at its actual stage position. Add a focused GPU contents/padding/lifetime control before expanding `CompatibleGuestBlockWordStorage`. Shifted, MSAA, clipped and large domains remain separate gaps. Do not repeat the completed one-level, converted-packed or late-envelope work.



## Current continuation boundary - converted packed stage domains

Production **a082b64cfd67f52b692163d9762ece23d67b025c** is pushed, clean-built and focused GPU verified. Ordered packed views that require separate native sampled and writable allocations now bind in original stage order. The focused GPU fixture proves distinct early I and late current-B contents with full tiled padding; the existing ordered matrix and adjacent packed10/independent transfer regressions pass. The sole installed `dx12-current` has EXE SHA256 **7dac81c4a8f235eb6136f1c75a148d154c8422d073185c55b07819aad794989f**. Game/final suite were not run.

Continue the existing formatted-alias queue at the BC/word-view physical-domain guard in `PrepareOrderedGuestStages`. Build one concrete same-address BC parent/word view from `gcn_bc_writable_alias_fixture.h`, map pinned Vulkan `ResolveOverlap`/`ExpandImage`, and preserve original buffers-before-images order, exact guest words, current I/B selection, barriers, ownership and allocation lifetime. Do not repeat the late-envelope or converted-packed work. Shifted bases, MSAA, clipped and large domains remain separate explicit gaps.

## Current continuation boundary - mixed constant MRT blending

Production **2b03a4d0dfe07882984e5b8f36babc0a80302e5b** is pushed, clean-built and focused GPU verified. On adapters without native constant-alpha RGB factors, supported RGBA8 UNORM and packed10 SNORM targets execute mixed constant-color/alpha MRT equations through per-sample ordered storage. Eight exact GCN sample1/4 draws passed, along with state, graphics-resource, RGBA8 logic, SNORM and signed regressions. The sole installed `dx12-current` has EXE SHA256 **44ad271ebea9dfaa59c2a02ea4ef89ea351df23cd103e038c91c011dd114896c**. Game/final suite were not run.

Continue the existing late formatted-alias envelope boundary. Establish an actual missing branch in `PrepareOrderedGuestStages`, `PlanCachedImageBinding` / `BindPlannedImage`, and `ValidatePreparedShaderBufferRange`. Current code already unions same-address levels and writable extents; if that covers the pinned case, record applicability and move to the next existing converted/multiview/shifted/MS/large guard. Preserve final-bind publication, I/B authority, ownership, barriers and lifetime. Do not repeat mixed-blend or packed19 capability work.


## Current continuation boundary - per-sample ordered color

Production **e132726c97fa794e9e53e4cc8c9d80866b47ebf2** is pushed, clean-built and focused GPU verified. Packed-SNORM sample1/2/4/8 ordered blend/logic passed 104 actual GCN draws; RGBA8 sample1/4 independent logic passed 136 draws with complete backing/padding checks. The sole installed dx12-current matches EXE 96b0ef3657ea9fcbd05e27387d57d4de18d70ab80a6c0761bcd88dbd6371a06e. Game/full suite remain not run. Packed format 19 MSAA is already connected on an original-capable adapter; do not invent a fallback for an adapter where the pinned Vulkan backend also lacks B4 attachment support. Continue late formatted-alias source envelope reservation without early semantic publication. Do not repeat the completed ordered-color/sample-plane or packed19 capability audits.


## Current continuation boundary - RGBA8 independent MRT logic

Production **48c9795b6901e94087b1b795ccfeb43bb08e6563** is pushed, clean-built and narrowly GPU verified: 68 actual GCN draws cover all 17 logic operations, three independent masks, disjoint array layers and full backing/padding. `OrderedColorUav` now preserves the actual attachment view. The one installed `dx12-current` matches that commit and EXE hash `47a3eb7b55311362e42302e075296271b5398647cb018c3529a04654733af3dd`; game/full suite remain not run. Continue packed-SNORM MSAA per-sample ordered blend/logic through the existing storage-plane path. Preserve exact sample bits, masks, ordering, ownership and lifetime. Do not repeat the completed ROV, single-sample SNORM or RGBA8 audits.


## Current continuation boundary - independent SNORM MRT logic

Production **bcc852b** completed independent write masks for single-sample packed-SNORM logic operations and passed the 44-case real GCN/GPU fixture plus shared SINT/MSAA and graphics-resource regressions. Clean source **88c9326** repeated the exact 44-case fixture and is installed as the sole identity-validated `dx12-current` core. Continue a recorded open mechanism. Packed-SNORM MSAA lacks per-sample UAV access; ordinary/non-castable independent MRT logic still rejects. Do not repeat this audit without new evidence.

## Current continuation boundary - 2026-10-19

Do not repeat the ROV compiler, resource-binding or single-sample packed-SNORM audit. Commit **3b886393c6e7d345bbe298763ed4a9e045f60d91** passed the 27-case production GCN fixture plus focused metadata, hardware-ordering and resource tests. Clean commit **825905c866988dbdc2ee2f57116e3ad93022be75** is installed and identity-validated as the sole `dx12-current` core. Implement independent MRT logic operations and per-target write masks through the ordered path. Keep packed-SNORM MSAA as an explicit gap, and preserve real contents, visibility, ordering, ownership and lifetime in each focused GPU/readback check.

## Current ordered-render compiler checkpoint - 2026-10-19

The authored hardware fixture already proves 64 ordered increments in every 4x4 UAV texel. Reuse it; extend production guest fragment export and blend/logic selection rather than rebuilding another generic ROV probe.

Continue from the ROV compiler selector now present in `SpirvConvertOptions`, the scoped Mesa export and `nir_to_dxil` UAV metadata. The focused reflection test and incremental Release core build pass. Do not reimplement or re-audit that compiler work. Next, lower only the affected pixel export path to the selected UAV, connect the descriptor/root range and resource transitions, and prove ordered destination read/modify/write with GPU readback. Then use it for exact packed-SNORM post-blend quantization and independent MRT logic masks. Keep current guards until those contents/order checks pass.

The single installed core remains 378bda3. Its exact-HWND capture shows a rendered Bloodborne scene and it exited 0, but foreground/XInput/world/FPS acceptance is still open. Do not repeat blind input while another elevated window owns foreground.

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

Latest diagnostic narrows the next action: fences are `before=0 prepared=0 copied=1`. Investigate CPU-visible-dirty reconciliation and envelope growth inside CopyBufferBatchImpl; initial source preparation itself did not submit. The original no-submit/data oracle remains unchanged. Production/runtime is7a59dd2; see H/copy-diagnostic-followup.json and newest SESSION_HANDOFF.md.

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

## 2026-10-19 immediate continuation

Start at `7335a30` on `fix/mixed-cmask`. Add producer-side command-writer range attribution in `gnmdriver.cpp`, validate at `PerformSubmit`, and identify the function that last covered the first malformed DWORD. Existing copied-buffer, previous-submit, PM4 direct-write, DX12 backing-write, and physical-alias checks are complete and must not be repeated. After a production fix plus regression, remove diagnostic rings/snapshots and continue shifted/rebased block-subresource views.