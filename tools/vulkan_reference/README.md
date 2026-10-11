# Vulkan reference profiler

This is measurement tooling, not a DX12 performance change. See
`docs/codex-progress/vulkan-reference-profiler-20261011/REPORT.md` and its coverage
and parity matrices. Only the exact exported Vulkan c7e065d1 is instrumented.
The repository's production DX12/Vulkan source files are unchanged.

## Passive capture

```powershell
./tools/vulkan_reference/Start-Vulkan-Reference-Monitor.ps1 -TargetProcessId 18920
./tools/vulkan_reference/Stop-Vulkan-Reference-Monitor.ps1
```

The start script checks executable SHA and Bloodborne command line, creates a
new directory, and starts a hidden Python process. It never starts a game.
`active-session.json` identifies the real collector and segment; `status.json`
records its heartbeat, samples, missed intervals and stop reason. Check that its
PID/command line still exist and `last_utc` advances; a stale `recording` file alone
does not prove a running collector. Each user restart requires a new invocation
and directory. Nothing attaches to a reused PID automatically.

Defaults: one second, eight hours, 1 GiB sample-file limit; adjustable interval,
storage limit and page-fault/private-growth thresholds. Three expensive samples
reduce frequency, up to five seconds (or the configured larger interval).
Skipped intervals and effective interval remain visible. A 30-second ring and
15-second post-window retain anomaly candidates, capped at 100 windows and four
pending windows. Windows have their own bounded storage in addition to the
sample-file cap. Causes remain unverified. Frame-stall rules cannot run without
an actual frame/presentation stream.

Stdlib Win32 process/thread counters and English PDH GPU counters need no package
installation. NVIDIA whole-adapter telemetry runs every fifth sample; subprocess
CPU is recorded, subprocess peak RAM is not. No ETW session, layer, debugger,
screenshot or UI interaction is enabled. Existing log tails are bounded. ETW
loss is unavailable, not zero. The installed PresentMon probe failed without a
CSV in this environment; it is not automatically retried on each capture.

Python defaults to the locally available bundled interpreter. Override `-Python`
and `-OutputRoot` on another machine. GPU counters depend on the installed WDDM
driver; reports derive coverage from available values rather than assuming success.

## Exact isolated source/build

```powershell
python tools/vulkan_reference/prepare_mode_b.py --repository F:/PS4Dev/work/shadps4-xess-src --output F:/PS4Dev/handoff/vulkan-reference-profiler-20261011/mode-b-source
```

Exports exact Git trees and gitlinks, fetching missing dependency objects into a
separate bare cache. No SDK, driver or profiler is installed. Git object alternates
provide exact commit metadata required by the FFmpeg build. Do not delete the
original object repository before building. `--instrument-only` restores only the
listed files in this dedicated export and reapplies hooks; never point it at the
user's checkout. `reference-instrumentation.json` lists hooked/missing definitions.

Configure with installed clang-cl, CMake and Ninja, Release, updater/Discord/tests
off. The verified machine-specific commands and full logs are in the handoff
directory as `Configure-Mode-B.cmd`, `Build-Mode-B.cmd` and
`mode-b-build-verified.log`. Build target: `shadps4`. Dependencies may download
their declared archives; installed toolchain is unchanged.

## Controlled game capture — requires separate user authorization

```powershell
./tools/vulkan_reference/Prepare-Controlled-Profile.ps1 -Destination F:/PS4Dev/handoff/vulkan-reference-profiler-20261011/new-controlled-profile
# Execute the following ONLY after explicit authorization for a game run:
./tools/vulkan_reference/Start-Controlled-Vulkan.ps1 -ProfileRoot <isolated-profile> -GamePath <eboot.bin> -TraceDirectory <new-trace-directory> -Executable <instrumented-exe> -ExpectedSha <verified-sha> -GpuTimestamps
python tools/vulkan_reference/analyze_mode_b.py <trace-directory>
```

A pre-existing `user` directory in the process working directory is mandatory:
Vulkan resolves user paths during static initialization, before CLI parsing.
`--override-root` is a game filesystem option, not a userdata isolation option.
The preparation script copies state, hashes the copy and redirects writable
profile paths. The seed has no shader cache and an empty isolated DLC directory;
copy matching DLC separately if needed and perform controlled warmup. Do not
publish keys, saves, firmware, game assets or complete portable profiles.

`SHADPS4_REFERENCE_TRACE=<new directory>` opts in to bounded tracing.
`SHADPS4_REF_GPU_TIMING=1` adds optional dispatch timestamp queries: max 64 pending
query pairs, no added CPU wait. It does not time graphics draws/copies. GPU spans
include dependencies, are duration-only, and have no GPU/CPU clock calibration.
Do not use this heavy query mode for the final performance baseline.

Events have ID, parent, thread, CPU monotonic timestamps, coarse thread CPU time,
exclusive instrumented-child time and four payload fields. `frame_hint` is the
most recently prepared output frame, not causal attribution of earlier PM4.
Actual Frame identity links ready/completion observation/present reuse/guest flip;
displayed time remains unavailable. No end-to-end guest CPU frame time is claimed.

The event ring holds 32768 entries, drains every 250 ms, and caps TSV at 512 MiB.
Contention/overflow is counted. Fixed aggregate counters and per-queue PM4 opcode
counts survive event loss; parameter distributions, P95 and call trees do not.
No sum of nested scopes should be reported as independent critical-path cost.

## Non-game verification

```powershell
python tools/vulkan_reference/verify_sensors.py <fresh-test-directory>
# Compile trace_smoke.cpp with C++20 and this directory on the include path;
# set SHADPS4_REFERENCE_TRACE to a new test directory before executing it.
```

The sensor test starts only a hidden Python fixture. The trace stress test checks
40,000 nested calls/byte events/PM4 packets and exact totals despite dropped
events. These tests do not establish GPU timestamp correctness or game behavior;
that validation and profiler-on/off overhead measurement remain pending.
