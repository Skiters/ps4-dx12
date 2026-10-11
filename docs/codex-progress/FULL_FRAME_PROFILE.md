# Bloodborne loading / freeze checkpoint — 2026-10-11

Final update: committed `ab1bf1a` reaches the saved world with camera/movement response
for over 120 s and handles `eboot+0x26b2b0b` protection faults on GXWorker:1. LEVEL 2;
no full-frame timing/FPS or correct-pixel claim. World-transition sample: 551.351 ms
snapshot comparison / 4.29 GB per two-second report, nested with resource preparation.
See GXWORKER_PAGEWATCH.md for the exact final binary and evidence. The precommit
loading wait below did not reproduce in the final run; historical profiles follow.

## Superseding GXWorker checkpoint

Read GXWORKER_PAGEWATCH.md first. Original-context capture proved the queued-fault
retired-interval lookup defect; it is fixed with a real failing-before GPU regression.
208/208 pass. Latest complete-fix run stopped on a separate guest mutex loading wait.
No full-frame/FPS gain is established. Existing SCM generation now correctly records
Git revision/dirty status; GITDIR-NOTFOUND below describes the historical binary.
Final exact-revision dev deployment is recorded in the new report and external manifest.

Source baseline: `ee765f40925fbe7c32aa3ede718161202e6a2a78`, branch
`fix/mixed-cmask`. Mixed CMASK support is preserved. This checkpoint fixes a
reproduced native reconciliation recursion. It does **not** establish the cause
of every reported sub-1-FPS world frame or a gameplay performance improvement.

## Executable and profile identity

The active build is `build-migration-core`, Release, LLVM 22.1.3 clang-cl.
The initial incremental build reported no work. At the user's request the
rasterizer, texture-cache and scheduler objects were then forcibly recompiled
and the core relinked before the decisive baseline run.

| Executable | SHA256 |
|---|---|
| Existing build at session start | `1FFEDEB47E52885241E5B9E634005F6576ADC6CF67D07E14DCB42CF3BFAE473B` |
| Forced baseline rebuild, PID 34752 | `523FE28E2BD9391182B165EF7D00532EADF2136770A1D475FC17D1270D35C2B4` |
| Fixed rebuild, PID 37116 | `2071659C10C5F5E3CE9E68E36074DC4C99CD6441260E3444761B2B5259BD4024` |
| Installed desktop core at session start | `D97CB11E92E0FAD6AB1964FCBD845231B46C60996604A1ECDC97F994D997E543` |

The installed core is at
`C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/native/core/`.
It was not replaced. Tests explicitly launched executables in the evidence
directory below. The generated build revision currently says `GITDIR-NOTFOUND`;
do not use that embedded string as SHA proof. Git state, build output, file
hashes and matching maps/PDBs are retained separately.

The earlier mixed-CMASK profile opens initial setup/character creation. The
user identified the intended saved-character profile as the original DX12
desktop profile. Both decisive runs use independent copies of
`C:/Users/skei/AppData/Local/PS4Desktop/native-profile/user`, with absolute
General paths rewritten. Save namespace is `1000/savedata/CUSA00207/SPRJ0005`
for installed CUSA03173 v1.09. Source userdata0000 SHA256:
`C7683FDCC05524369D71DA7B31F0F4D42CF485D44C8511D70FD90766904B7407`;
userdata0010: `D731035E3EA7727230AC6DCF551C31C7949CA669BF82A1A1FB62E665C72F72D2`.

Skip Intro and the existing 1280x800 patch remain enabled. The latter is a
large behavioral patch, not only a resolution change. Its line at patch
address `0x024B56D2` NOPs the known guest assertion (`eboot+0x20B56D2`, after
the patch address adjustment). That line was removed **only from both test
copies**. Other patch lines were preserved and remain a comparison caveat.
Original DX12/Vulkan profiles and saves were not modified. Both launches set
`SHADPS4_DX12_PARTIAL_RENDER=0`, explicitly overriding marker files. Native
diagnostics, startup tracing, and bounded CMASK capture were enabled.

## Confirmed failure and fix

Baseline PID 34752 continued loading until the GPU worker exhausted its stack.
LLDB first stopped in the signal handler after a driver stack probe touched
the inaccessible page below the 2 MiB worker stack. This was a secondary fault,
not proof of a driver defect. Capturing the entire readable worker stack and
resolving addresses with its exact map exposed 1,943 regularly spaced copies
of this sequence:

```
ExecuteGuestCopyShader -> CopyBufferBatch -> ValidateHostMemory
  -> GuestRasterizer::CheckBacking -> Rasterizer::ResolveGpuBuffers
    -> Rasterizer::Finish -> GuestRasterizer::CheckBacking
      -> Rasterizer::ResolveGpuBuffers -> Rasterizer::Finish -> ...
```

Repeated return addresses in the baseline map:
`Finish+0xbf0 = 0x700002019380`,
`CheckBacking+0x1a = 0x7000020a37ea`,
`ResolveGpuBuffers+0x217 = 0x70000200bbb7`.
Stack interval: `0x60030e5c0000..0x60030e7c0000`; repeated frame spacing `0x430`.
The original exception records a write to `0x60030e5bf000` from the driver
probe; the nested handler then faults writing `0x60030e5bf598`.

`resolving_buffers_` contains the outer GPU-owned request. During Finish,
writeback of a different CPU-visible dirty mapping calls virtual CheckBacking.
That range has no GPU-owned overlap, but the old code sees the **outer**
nonempty set and calls Finish again before completing the current writeback.
The recursive mutex permits this recursion; no lock cycle is involved.

ResolveGpuBuffers now starts Finish only for new owned overlaps selected by
the current request. Scope cleanup removes only this request's additions and
also runs after an exception. It preserves outer resolution state and does not
skip GPU copies, fence waits, backing commits, dirty ranges or ownership release.

Corresponding upstream reference:
`shadps4-emu/shadPS4@0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa`,
`BufferCache::ReadMemory/DownloadMemory`. Vulkan builds request-local download
copies from GPU-modified ranges, returns when there are no bytes, submits and
waits, writes physical backing, then unmarks GPU modification. No unrelated
outer pending set triggers a second complete download. Only these functions
were compared; this is not another broad parity audit.

## Validation and actual game outcome

The native_buffer_range_lookup regression now uses a derived rasterizer with
GuestRasterizer's real CheckBacking call pattern, alongside GPU-owned and
CPU-visible dirty destinations. Actual D3D12 copies verify exact changed bytes,
untouched padding, one reconciliation, and idempotence. A bounded recursion
check fails the baseline safely with `FAIL: recursive buffer reconciliation`.
After the fix it passes, including an injected backing-commit failure followed
by retry without premature ownership release. D3D12 Debug Layer is enabled.

Core build passed. Complete native suite: **208/208**, 96.62 seconds. The game
overlapped part of the suite run, so timings from that period are not suitable
for performance comparisons. Do not interpret the suite duration as a speedup
or regression.

Fixed PID 37116 again reached saved-world loading and did not show the observed
native recursion. It exited around diagnostic t=90s with a different guest
fault: **GXWorker:1**, write to `0x24347e980`,
`eboot+0x26b2b0b`, instruction `mov [rax-0x08], ebx`.
That run ended before baseline's last draw count; absence of recursion in it
is not proof of passing the identical problematic game operation. Root-cause
correction is proven by the deterministic regression; game-level improvement
remains unverified. No world frame, stable gameplay, or A–E milestone is claimed.

## Timing evidence and limitations

At baseline t=153.615 the preceding ~2s loading window recorded 523 draws,
233 dispatches, 634 direct submissions, 29 copy submissions, 1,279 buffer misses,
692,477,504 materialized bytes, 2,559,965,256 snapshot-comparison bytes and
389,699,720 readback bytes. Fence-wait wall time was 114.473ms across 1,267 calls.
These show continuing work before the recursive fault, not a stuck fence.
One loading stack sampled MaterializeGuestColorClear allocating a 4,096,000-byte
linear temporary; another menu sample hit ReleaseCpuBufferWatches. They are
samples, not attribution of all frame time.

The final fixed ~2s window records snapshot comparison 605.094ms,
materialization 182.217ms, overlap retirement 108.483ms and snapshot upload
162.979ms within buffer preparation. These are stage totals for that window,
not same-scene before/after numbers. Aggregate draw, dispatch, resource, upload,
readback and fence times are nested and must not all be added together.

Debugger pauses contaminated baseline t~45 and t~99 wall times (20s and 35s).
Exclude them. GPU compute timestamps use the e43ba4d submission-lifetime fix,
but cover only compute, not total GPU/frame execution. No forced Finish was
added for profiling. Full guest-frame IDs, CPU/PM4-to-present correlation and
full-list GPU timings remain outstanding. Present/PrepareFrame counts are not
game FPS; no full-frame speedup or 1% low is available.

## Separate early blockers

The earlier setup-profile debug run stopped **before** the audio null write.
Its assertion text is `Mutex is not initialized.` (`DLLightMutex.cpp`, line 126).
Guest stack offsets: `0x20b56d2 -> 0x207eebd -> 0x263b22c -> _runOnAnotherStack`.
At that instant Game:Main was in sceAvPlayerClose / AvPlayerSource destruction.
This establishes the assertion condition and concurrent teardown, not why the
mutex lifetime failed. It does not establish the cause of world slowdown.

A separate setup run reached character creation and failed on CMASK tile code
2 at metadata `0x2698b8000`, color `0x2695e0000`, 784x784, pitch 896, padded
height 832, tile mode 14. All 8192 captured metadata bytes were 0x10..0x13
(mostly 0x11/0x12), not a mixed 0/F image. Descriptor, color and metadata are
captured. Do not invent semantics for these bytes or clear/skip the surface.
Their producer/alias history still needs investigation.

## Local evidence and next operation

Evidence root: `F:/PS4Dev/handoff/freeze-20261011/` (not in Git):
`debug/stacks-assert-full.txt`, `debug/assert-message.txt`, `cmask/`,
`fresh-saved/manifest.json`, `fresh-saved/stacks-load-1.txt`,
`fresh-saved/gpu-stack.bin`, `fresh-saved/stack-candidates.txt`,
`fresh-saved/original-exception.txt`, `fixed-saved/user/log/`,
`regression-before.txt`, `regression-after.txt`, `fix-suite.log`.
Matching binaries/maps/PDBs are in `fresh-core/` and `fixed-core/`.
All session-owned game/debugger processes have stopped.

Next: start the fixed core from another copy of the **saved-character** profile,
with the assertion NOP removed and partial render explicitly 0. Attach before
the GXWorker fault and capture its original context, page protection, ownership
and all thread stacks at `eboot+0x26b2b0b`. Determine whether PageWatch should
handle that write or the guest address is invalid. Preserve this recursion fix.
Only after reaching a stable world scene complete full-frame correlation and
measure performance. Do not restart from the setup-profile audio audit.
