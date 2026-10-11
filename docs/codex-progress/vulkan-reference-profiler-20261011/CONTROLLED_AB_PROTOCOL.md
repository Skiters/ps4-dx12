# Controlled reference protocol — pending

No game run was launched by this task. A Mode B game launch requires separate
explicit user authorization under section 2 of the uploaded request. A free-play
passive capture cannot fulfill this benchmark.

1. Close the user's game normally at a time they choose. Verify no core remains.
   Record exact game version/patch hashes, applied game/shader patches, firmware,
   executable SHA, source revision, driver, adapter, clocks/power plan and monitor
   mode. Hash the original save **as a read-only seed**, then create independent
   Vulkan Relaxed, Vulkan Precise and DX12 profile copies from that seed. Never
   copy a benchmark save back to the original. A fresh controlled-seed-final copy was created after the user game exited.
   Re-snapshot while idle again if the source changes before matched A/B.
2. Choose one reproducible stationary scene/camera plus a separate loading scene.
   Record input timing; do not combine load/death/menu/free-play with steady-state.
   Resolution must be identical (proposed 1280x800 for compatibility with existing
   DX12 tests; change both deliberately). Match all applicable patches, extra guest
   memory, scaling, pacing and cache warmup; record backend-only differences.
3. Vulkan readbacks: Relaxed=1, Precise=2. Keep both independent. Preserve DX12
   PageWatch/ownership; a quicker relaxed path is not equivalent to stricter CPU
   visibility. Keep cache identities separate per backend and test run.
4. Warm each variant to the same scene, verify stability, then reset the independent
   seed for the actual run. Maintain separate cold-start, first-use and warmed data.
   Cache hashes must be recorded while processes are idle; a path-only identity
   from live monitoring is not a content identity.
5. Short diagnostics first: bounded Mode B events/counters; optional dispatch GPU
   timestamps; record trace loss and actual profiler CPU/I/O. Validate query values
   against known-duration GPU work without adding a CPU wait. Complete PM4-to-frame
   causal IDs, graphics/copy GPU timings and submission dependency edges before
   reporting complete CPU/GPU frame times. Existing prepared-frame hints cannot
   serve as full guest frame attribution.
6. Heavy ETW/context-switch/stack windows are separate, short and explicitly
   authorized if elevation is required. Do not reuse/stop unrelated PresentMon or
   WPR sessions. Installed PresentMon needs a successful local capture before any
   displayed-cadence claims; the first probes failed.
7. Performance captures: instrumentation off, then light aggregate mode (if overhead
   is validated), same duration (proposed 90s), scene and controlled input. Repeat
   three valid runs per variant in alternating order. Keep background builds off.
   Compare paired output-frame cadence; host presents alone cannot prove guest FPS.
8. Report median/P95/P99 and run-to-run variability only for supported timestamps.
   Label CPU scope nesting, wait time vs execution time, GPU dependency spans,
   query losses and noncausal frame hints. Reject mismatch/error/device-loss/capture
   failure intervals rather than silently averaging them.
9. Only then rank exclusive critical-path costs: buffer validity/residency,
   index preparation, detile, translation/PSO, actual waits, indirect commands,
   presentation and nongraphics guest/HLE costs. Every finding references an
   executable, interval, function, operation/resource identity and raw evidence.
10. Implement one proven change at a time in DX12; required byte/pixel/state/lifetime
    regressions plus same-scene A/B. No weakened CPU visibility or removed waits
    without a replacement dependency proof.

Prepared local files: `controlled-seed-final/user`, `profile-manifest.json`, exact
`mode-b-source`, `mode-b-build/shadps4.exe`, profile/launch scripts under
`tools/vulkan_reference`. No profile keys, saves, firmware or game assets belong
in Git. DLC seed is empty and must be matched deliberately if the chosen scene
uses installed DLC.

| Variant | Exact identity | Readback/cache/scenes | Run result |
|---|---|---|---|
| Original Vulkan | c7e065d1 / SHA in observed manifest | Relaxed, 3440x1440, free-play | Descriptive passive data only |
| Instrumented Vulkan Relaxed | c7e065d1 + recorded instrumentation patch | Isolated warmup, matched scene pending | PENDING |
| Instrumented Vulkan Precise | Same instrumentation/base | Independent profile, matched scene pending | PENDING |
| DX12 | Functional 9527c901 / current repo c5d3a878 | PageWatch preserved, matched scene pending | PENDING |

Historical DX12 0.64–0.84 output FPS at 1280x800 and current Vulkan free-play are
not an A/B pair. No cross-backend speed ratio is valid from those captures.
