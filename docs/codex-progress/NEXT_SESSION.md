# Next session

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
