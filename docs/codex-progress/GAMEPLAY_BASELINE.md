# Bloodborne integration baseline — 2026-10-11

## Latest — GXWorker PageWatch checkpoint

See GXWORKER_PAGEWATCH.md. Generation-only run reached world/control then crashed
at the original instruction (~357 s). Diagnostic capture proved lost retired-range
coverage; the complete fix and exact before/after CPU/GPU regression now pass.
Complete-fix precommit run stayed on loading, with Game:Main waiting on MOMainThread's
mutex. Do not equate absence of a crash with working gameplay. Original full profile
is copied, General paths rewritten, only assertion NOP removed, partial render 0.
Final committed-build result/hash/path live in the external verification manifest.

## Latest baseline — saved-character profile and rebuilt core

Source `ee765f4`; both decisive runs used independent copies of the original installed
DX12 profile with an existing character and Skip Intro, rewritten absolute paths,
and `SHADPS4_DX12_PARTIAL_RENDER=0`. Both removed only the existing resolution patch's
assertion-suppression NOP at patch address `0x024B56D2`. Other behavioral patch lines
remain a comparison caveat. Original profiles and saves were preserved.

| Run | Last verified stage | Outcome |
|---|---|---|
| Forced baseline rebuild, PID 34752 | Menu, saved-world loading/item screen | GPU-worker stack exhaustion from recursive buffer reconciliation |
| Fixed rebuilt core, PID 37116 | Saved-world loading | GXWorker:1 fault at `eboot+0x26b2b0b`, write `0x24347e980` |

The baseline saved-profile menu lacked the previously prominent colored noise, but
no correct world image was verified. Fixed run ended before baseline's last draw count;
no later gameplay stage or FPS improvement is proven. The deterministic regression
proves the recursion fix. FULL_FRAME_PROFILE.md records exact executable/save hashes,
matching maps, stacks and timing limits. Installed desktop core was not replaced.
The sections below describe earlier profiles and are not the current launch recipe.

Baseline: `a824f9f65668d61080d2e90488435589efcd4f90`. After runs use this source plus
the bounded ResolveGpuBuffers lookup in this checkpoint. Native core rebuilt;
diagnostics enabled; installed Bloodborne CUSA03173. No original Vulkan installation
was modified and no Vulkan backend substituted.

| Run | Last visually verified stage | Outcome |
|---|---|---|
| Baseline, PID 36288 | Title, online/offline choices; colored sparkling artifacts | Guest access violation in Mv_AudioPlayThread |
| Bounded lookup, PID 36876, seed home | Intro and brightness setup; artifacts remain | Draw exception: mixed CMASK codes |
| Bounded lookup, PID 3740, copied prior home | No reliable screenshot before exit | Same guest fault offset as baseline |

Save selection, completed level loading, correct world image and stable gameplay were
not verified. Guest execution and draws/dispatches continued, but this is not evidence
of world rendering. Present counts are not game-frame measurements. No same-scene
FPS, 1% low or CPU/GPU frame-time comparison is available.

Profile caveat: the seed config contains absolute General.home_dir paths. Baseline
therefore used the existing DX12 test-profile home. First after run redirected to seed
home and reached initial setup. Second after run copied the prior home to its own
saved-home-copy before launching. These are not controlled identical-scene samples.
Freeze/copy one complete home and rewrite absolute paths before future comparisons.
The DX12 test-profile home is not the original installed Vulkan profile.

## Confirmed faults

Baseline and copied-home rerun logged 0xc0000005, write to address zero, thread
Mv_AudioPlayThread, at **eboot+0x20b56d2**. Baseline base=0x57a0000/RIP=0x78556d2;
rerun base=0x5800000/RIP=0x78b56d2. Instruction: `mov dword ptr [0], 0xDEADBA`.
This suggests a guest assertion; the triggering condition is unknown. AvPlayer
Stop/already-stopped and sysmodule unload messages precede it, without proving causation.
The fault logger emitted zero stack words; no valid crash stack was recovered. Earlier
live debugger stacks are not crash stacks. These exits were not intentional termination.

Seed-home run threw `Native CMASK mixed tile codes require address equation:
address=10360684544 layer=0`. GuestColorClearPending rejects nonuniform slices.
Upstream 0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa Rasterizer::EliminateFastClear uses
tracked IsMetaCleared/TouchMeta state and a native image clear. DX12 supports only
uniform 00/FF slice expansion. Ignoring mixed codes or clearing the whole image would
not preserve pixels. Neither this failure nor the guest assertion is fixed here.

## Verified lookup optimization

Two live GPU-worker samples resolved with the matching native-core.map to
ResolveGpuBuffers: once through CheckBacking/ValidateHostMemory/CopyBufferBatch/
ExecuteGuestCopyShader/Dispatch, once through draw preparation. About 10,831 mappings
were resident. Previously each backing check scanned every mapping, even with no
GPU-owned match. Later samples moved elsewhere: no infinite loop/deadlock established.
Late copy-HLE calls approached 170 ms, but sampling does not attribute that entire
cost to lookup.

The ordered disjoint-map invariant permits lower_bound, one predecessor and only
intersecting entries. Finish, readback and ownership release are unchanged.

| Controlled microbenchmark, CPU wall total | Before | After |
|---|---:|---:|
| 10,000 lookups, 1,024 resident mappings | 26.789 ms | 0.366 ms |

One sample per version, D3D12 Debug Layer enabled. This is local lookup cost, not GPU
execution time or game FPS. No controlled integration upload/readback/submission delta
was established. Core build passed. New boundary/ownership/data-preservation regression
passed with Debug Layer and message validation. Full native suite: **208/208**, 60.06 s.
Passing tests does not establish visual correctness. No new LEVEL 1–5 is claimed.

Local evidence excluded from Git: F:/PS4Dev/handoff/gameplay-a824f9f,
gameplay-range-after, gameplay-range-saved-profile; gameplay-range-suite.log and
gameplay-range-core.log. Fault records are under each run's user/log. Live stacks
require the matching map; exported-symbol names alone are unreliable. No saves,
binaries, memory dumps or large traces are included in this checkpoint.

## Mixed CMASK implementation checkpoint — 2026-10-11

DX12 now plans CMASK per selected layer and per 8x8 tile. Uniform zero slices retain the existing fast path; mixed zero/F slices use a GCN AddrLib-derived pipe-interleaved equation, materialize only zero tiles, preserve expanded tiles, and mark only materialized nibbles expanded. The native regression performs exact changed/unchanged pixel checks, idempotence, untouched-layer checks and D3D12 GPU readback.

The paired Bloodborne run used independent copies of one profile with rewritten absolute paths. The source run visually reached brightness/setup and reproduced the existing colored artifacts; the same run later hit the known guest audio assertion at `eboot+0x20b56d2` before a CMASK capture was produced. The post-change game run was built but did not provide an additional CMASK-stage proof because the audio assertion remained earlier in that attempt. No world-entry or full image-correctness claim is made.
