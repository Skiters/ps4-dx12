# Bloodborne integration baseline — 2026-10-11

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
