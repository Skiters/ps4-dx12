# Next session

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
