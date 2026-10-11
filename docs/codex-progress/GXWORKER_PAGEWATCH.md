# Bloodborne GXWorker PageWatch checkpoint — 2026-10-11

Input: `40a8e683def56160ea110a2d5df1a48a8532c4ce`, `fix/mixed-cmask`.
Implementation revision: the commit containing this file. No push authorized.
Evidence root: `F:/PS4Dev/handoff/gxworker-20261011` (binaries, symbols,
profiles, debugger captures and logs are outside Git).

## A. Confirmed cause and implementation

The original `eboot+0x26b2b0b` write is a **class C PageWatch handling defect**.
It is not an unmapped/null guest pointer. A queued CPU write fault can enter VEH
after another CPU thread has resolved the watch and restored write permission.
The old retired-range map retained overlapping intervals but tested only the
immediate predecessor. Retiring a short interior range hid an earlier large
range that still covered the pending fault. An unrelated 4096-entry reset could
also discard this required history without knowing that pending faults drained.

Decisive game capture: `miss`, PID 31376, executable SHA256
`42C4AACC65CA5C547B0400D2094C867AD4C6B946D06B28CA47CE34FDE8B1EE8B`.
Matching `core/shadPS4-DX12-Core.pdb` and `core/native-core.map` are retained.
Native debugger returns real access violations as `DBG_EXCEPTION_NOT_HANDLED`;
it does not swallow the exception or resume an invalid store.

| Original write exception | Captured value |
|---|---|
| Code / access | `0xC0000005`, write |
| Thread | GXWorker:4, TID 27624 (also GXWorker:3 and Game:Main concurrently) |
| Eboot base / RIP | `0x5790000` / `0x7e42b0b` = `+0x26b2b0b` |
| Instruction | `89 58 f8`, `mov [rax-8], ebx` |
| RAX / RBX / RSP | `0x243e980c8` / `0x20ca7a00` / `0x7ef6d7ca0` |
| Fault address | `0x243e980c0` |
| Allocation backing | base `0x242c00000`, `MEM_MAPPED`, `MEM_COMMIT` |
| First-chance protection | `PAGE_READONLY`, region `0x243e98000`, size `0x48000` |
| At PageWatch rejection | same valid allocation, `PAGE_READWRITE` |
| Misleading predecessor | `0x243e97000..0x243e98000` |
| Hidden covering history | `0x243400000..0x243ee0000` |

See `miss/debugger.txt`, `original-contexts.json`, `pagewatch-misses.txt`,
`fault-2.context` (raw Windows CONTEXT), `fault-2.code`, `fault-2.stack`,
`pagewatch-fault-31376.context`, stdout and stderr. The debugger captures original
first-chance register contexts and mapped-page information, plus all thread
stacks on the eventual second-chance exception. Guest frames beyond available
unwind information are not assumed to form a complete symbolic backtrace.

`PageWatch::RememberRetired` now keeps the disjoint, coalesced union of retired
pages, without filling unwatched holes or dropping coverage at an arbitrary
entry count. `Untrack` records that history before restoring permissions.
`Fault` atomically examines owner generation and current protection under the
watch mutex. It releases that mutex before resolving through the renderer,
then handles a newly rearmed owner if necessary. A callback leaving the same
owner unresolved is rejected. Retries are bounded at 64. Unmapped, guarded,
unwritable and unknown pages still reject the write. GPU commit, physical alias,
fences and guest assertions remain enabled. The `40a8e68` recursion guard and
`ee765f4` mixed 0/F CMASK implementation are unchanged.

The generation fix alone was insufficient: run `fixed`, PID 18864, reached the
world but still failed at the original instruction at about 357 seconds. The
subsequent game capture above established the missing retired-interval fix.

Enabling the actual D3D12 Debug Layer exposed a separate startup blocker:
new textures submitted on the COPY queue started in COPY_DEST (error 1334).
`GuestTextureCache::Upload` now creates these in COMMON, relies on legal copy
promotion/decay, and transitions on the direct list after the existing copy
fence wait. Depth/stencil uploads stay on the direct list with explicit barriers.
The GPU ownership fixture now actually uses the copy queue and fails before
this correction. Upload lifetimes and scheduler ordering are preserved.
See Microsoft's [resource barrier rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).

Upstream reference: `0c9ca525b8add53e62b3ccd9c9c2f6ef2ebabbfa`,
`video_core/page_manager.cpp`, `SignalImpl::GuestFaultSignalHandler`, with
per-page tracking and rasterizer ReadMemory/InvalidateMemory callbacks.
Adapted guarantee: concurrent tracked faults must survive ownership retirement
and complete reconciliation before CPU retry. No Vulkan API was transplanted;
no paired Vulkan runtime result is claimed.

## B. Real game run

The generation-only `fixed` run visually reached the actual saved world and
responded to movement/camera input, then failed at the old instruction around
t=357 s. This is evidence of the reproduction, not of stable fixed gameplay.

The complete-fix `verified` run launched at 11:42:31, selected Continue at
11:46:30, then remained on an unchanged loading screen through 11:52. It was
stopped for investigation, without an original guest fault or D3D12 error.
Two native snapshots and raw per-thread contexts/stacks are retained in `verified`.
Windows wait-chain capture shows `Game:Main` TID 8652 waiting on a mutex owned by
`MOMainThread` TID 16932. The owner is in `sceKernelUsleep` / `Common::AccurateSleep`;
the GPU command processor waits for work and GX workers wait on guest semaphores.
WCT does not report a cycle; this is not proof of a renderer deadlock. The owner
continues context switching. Its guest condition/lock lifetime is not established.
The current run's diagnostic output files stayed empty despite the diagnostic
worker being present, so they do not establish command progress or frame timing.

Conservative milestone at this checkpoint: **LEVEL 1** (fault mechanism fixed
and failing-before/passing-after regression), pending the final committed-build
launch recorded externally. No LEVEL 3–7 claim: correct world pixels, sustained
image updates, stable control and playable performance remain unverified for
the complete fix. The final exact-revision launch is required before delivery.

## C. Performance limits

No correlated guest CPU/PM4/GPU/flip frame time or reliable FPS has been measured.
No full-frame improvement over baseline is claimed. Present/PrepareFrame counts
and nested stage totals must not be converted into gameplay FPS.

The preceding `fixed` run's two-second loading/world-transition sample at about
t=162 s reports 603.931 ms snapshot comparison (4,364,641,332 compared bytes),
205.169 ms materialization (584,270,188 bytes), 161.251 ms snapshot upload,
114.308 ms overlap retirement and 133.830 ms ownership transition; about 95 ms
fence waits and 121,130,100 readback bytes. These overlapping CPU stage totals
are clues to memory-coherence work, not additive frame times or a world benchmark.
The detailed report's 8192-record limit is exhausted before a sustained world
sample. Do not reuse the historical invalid 0.0064 ms GPU number.

## D. Build and isolated installation

Installed launcher: `C:/Users/skei/AppData/Local/Programs/PS4Desktop-DX12/PS4Desktop.exe`.
Its native host uses `native/native-core-path.txt` -> `core/shadPS4-DX12-Core.exe`.
Installed core SHA256 remains
`D97CB11E92E0FAD6AB1964FCBD845231B46C60996604A1ECDC97F994D997E543`.
Its precise source revision cannot be established from GITDIR-NOTFOUND; it is
not evidence of testing the current branch. Installed DX12/Vulkan are untouched.

Release build: `build-migration-core`, clang-cl LLVM 22.1.3. Reconfiguring CMake
with working Git regenerated the existing SCM source correctly. Startup now
prints full revision, branch and dirty description. Verified precommit core:
`verified/core/shadPS4-DX12-Core.exe`, PID 20908, SHA256
`F4325AA68CB1DBB01E28AD2D6E18225A2AA449A8B388C44C0049E8B990609547`.
Its startup accurately says input HEAD plus `40a8e68-dirty`; the source patch is
retained separately. Do not label this precommit binary a clean release revision.

After the commit, regenerate SCM and relink the exact committed revision into
`F:/PS4Dev/handoff/gxworker-20261011/dev-release`. User entry point:
`Launch-Bloodborne.cmd`. `release-manifest.json`, `process.json`, startup logs and
`final-verification.json` record the actual committed executable, SHA256, process
path and final launch evidence. Matching PDB/map travel with the executable.

Each profile is a full independent copy of
`C:/Users/skei/AppData/Local/PS4Desktop/native-profile/user`, with General paths
rewritten. Installed game is CUSA03173 v1.09, saved character uses namespace
`1000/savedata/CUSA00207/SPRJ0005`; the differing title labels are not a failure.
Only assertion NOP `0x024B56D2` is removed from copied patch XML. Skip Intro and
the rest of the large behavioral 1280x800 patch remain comparison caveats.
Partial rendering is explicitly 0. Original source save hashes are in manifests;
no original save/profile or installed application is modified.

## E. Regression and validation

- Genuine delayed CPU write: park a writer before PageWatch, retire its 16 KiB
  renderer watch, retire a 4 KiB interior watch, then deliver the pending fault
  at byte 8193. Old code exits C0000005 (`exact-before.log`); corrected code
  passes (`exact-after.log`). GPU readback checks every byte after reconciliation
  and subsequent CPU writes, watch removal, resource reuse and completed fence.
- Genuine CPU write with rearming between resolution and permission inspection:
  old handler fails, generation-aware handler passes; expected resolver count
  and PAGE_READONLY/PAGE_READWRITE transitions are checked.
- Negative injected faults reject null, unowned read-only, unresolved and
  indefinitely rearmed pages (64-callback bound). 4097 disjoint retirements
  preserve the first/last intervals and leave gaps untracked.
- COPY queue texture regression fails Debug Layer error 1334 before its state
  fix and passes with exact GPU pixels afterward (`copy-before/after.log`).
- Full native suite after the final source changes: **208/208**, 55.52 seconds
  (`verified-suite.log`). D3D12 hardware tests use the Debug Layer.
- A copied header preserved an older modification time during one restoration;
  the resulting no-work build was detected and not accepted as verification.
  `exact-after-rebuild.log` confirms compilation before the passing result.

## F. Next focused step

First resolve the outstanding loading wait if it repeats in the final launch:
inspect `MOMainThread`'s guest condition while it owns the mutex awaited by
`Game:Main`; retain `verified/loading-stacks*.txt`, `main-wait-chain.txt`,
`owner-wait-chain.txt` and raw thread dumps. Identify the mutex and its guest
owner before changing `PthreadMutex::Lock/Unlock`, `sceKernelUsleep` or the
renderer. Do not force an unlock or skip the wait. This is a separate unresolved
blocker, not evidence that the captured retired-range defect was imaginary.

After confirming fault-free saved-world execution, correlate actual guest flips
with Liverpool PM4, resource preparation and submitted GPU fence timestamps.
Start at `VideoOutDriver::SubmitFlip/SubmitFlipInternal/Flip`,
`sceVideoOutSubmitEopFlip` and Liverpool's patched-flip interrupt, distinguishing
`DrawLastFrame` from a newly completed guest frame. Capture the same saved scene
and then attribute `Rasterizer::PrepareShaderBuffers`, `CheckBacking`,
`ResolveGpuBuffers`, snapshot comparison, materialization and fence waits to
those frame IDs before choosing an optimization. Preserve exact bytes, ownership
and fences. Stable playable performance and visually correct rendering remain
separate acceptance criteria. CMASK codes 1/2 and setup-profile audio lifetime
issues remain open; do not suppress them or call them fixed without reproduction.
