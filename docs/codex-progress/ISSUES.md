# Open issues

## Final-run priority — pathological world-frame cost

Committed ab1bf1a reaches world/control beyond 120 s and handles the original guest
write faults without an unhandled exception. Main remaining blocker: slow completed
world updates, with expensive buffer-coherence stages requiring full-frame correlation.
Color/edge artifacts remain, so correct-pixel and playable-state acceptance stay open.
The earlier Game:Main/MOMainThread loading wait did not recur; retain its captures
without assuming a fixed cause. See GXWORKER_PAGEWATCH.md and final-verification.json.

## Current priority — loading wait after PageWatch correction

- Fixed: original guest write fault at `eboot+0x26b2b0b`, caused by a shorter
  retired watch hiding earlier coverage for a queued exception. Coalesced history
  and generation-aware handling preserve ownership, permissions and GPU bytes.
- Fixed: Debug Layer error 1334 on new COPY queue texture uploads (COMMON state).
- Latest remaining observation: Game:Main mutex wait owned by MOMainThread while
  the GPU command processor waits for work. Guest condition/lock lifetime unknown;
  WCT reports no cycle. This is not yet classified as a renderer/HLE deadlock.
- World correctness, sustained control and full-frame performance remain open.
  Separate CMASK 1/2 and setup audio issues are not closed by this checkpoint.

See GXWORKER_PAGEWATCH.md and final external verification; older priorities below
are historical. Do not bypass locks, assertions, fences or unsupported CMASK.

## Current priority — 2026-10-11 saved-character profile

- Fixed: native ResolveGpuBuffers -> Finish -> CheckBacking recursion, confirmed
  by 1,943 repeated stack sequences and a failing-before/passing-after regression.
- Next: fixed core exits during saved-world loading on GXWorker:1 at
  `eboot+0x26b2b0b`, writing `0x24347e980`. Capture original context, protection,
  ownership and all stacks; distinguish PageWatch handling from an invalid guest address.
- World-frame freeze/sub-1 FPS remains unverified in these runs; full-frame correlation
  is still missing. The new run ends before the baseline's last draw count.
- Separate setup-profile audio assertion is now identified: `Mutex is not initialized.`,
  DLLightMutex.cpp:126, concurrent with AvPlayer teardown. Lifetime cause remains unknown.
- Separate captured CMASK code 2 surface has metadata 0x10..0x13. Producer/alias history
  remains unknown; mixed 0/F implementation stays enabled, unsupported codes stay rejected.

Use FULL_FRAME_PROFILE.md and NEXT_SESSION.md; older priorities below are historical.

## Integration checkpoint 2026-10-11

2026-10-11 observed blockers: repeated Mv_AudioPlayThread null write of 0xDEADBA
at eboot+0x20b56d2 (actual crash stack/trigger still needed); mixed CMASK exception
at initial setup; title/setup artifacts. Bounded buffer overlap lookup is fixed and
tested, but world entry remains unverified. Profile absolute-path mismatch prevents
controlled scene comparison; see GAMEPLAY_BASELINE.md.

Parity audit: GPU timestamp values at 774d095 require remeasurement; see BENCHMARKS.md.
PrimitiveType::None / depth-override ordering is corrected to the pinned Vulkan revision.
Seven classification states pass; end-to-end memory-side-effect reproduction remains outstanding.
Full command differential replay, texture alias parity and shader instruction parity remain
unverified. Existing dirty_ranges must be accounted for before redesigning memory tracking.

## Compute dispatch

- `resource-upload-dispatch-writeback` remains expensive: 4.044 ms average in the latest
  controlled run, with a mean two-second-window P95 of 11.980 ms. Its nested
  `resource-bindings` and `buffer-views` stages need a non-overlapping attribution pass.
- Copy-HLE still averages 4.628 ms over every dispatch-stage sample, including cheap
  non-copy probes. Although eager copy reconciliation was removed for owned pages, resource
  preparation and later coherence points still dominate long-tail samples.
- Copy submission batching reduced copy queue submissions by 33.8%, but direct submissions
  remain essentially unchanged. Further batching requires call-site dependency evidence.
- D3D12 timestamps show compute execution averaging about 0.0064 ms in the measured scene;
  shader execution is not the cause of the multi-millisecond dispatch wall time.
- Snapshot comparison, uploads and later GPU-owned readback still process roughly 5-6 GB in
  the 40-second steady-state window. The guest buffer ranges that cause these large coherence
  transfers need to be attributed before narrowing dirty ranges.

## Validation limits

- The two Bloodborne runs used cloned copies of the same isolated profile and equal duration,
  but no automated camera/save-state lock proves identical rendered frames. Do not infer an
  FPS change from these runs.
- The process was deliberately stopped after 66 seconds, so its `-1` exit status is expected
  and is not a crash result.
