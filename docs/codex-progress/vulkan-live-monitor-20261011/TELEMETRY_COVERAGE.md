# Live telemetry coverage

Fixed denominator: 28 named metric families, defined in live_metrics.py and saved
in each manifest. Normal valid samples reach 18/28; startup/failed sources reduce
the numerator. This is not percent coverage of the full PS4 execution pipeline.

| Family | Collected source | Limits |
|---|---|---|
| Process user/kernel CPU and core equivalents | Win32 process times, deltas / QPC interval | No instruction or causal attribution; 1 core equivalent = 1 logical CPU fully occupied |
| Threads, names, priorities, CPU deltas | Toolhelp, GetThreadDescription/GetThreadTimes | Identity uses TID plus creation time; inaccessible/vanished threads omitted; names are not stacks |
| Working set, private bytes, commit | Process memory counters | Commit is PagefileUsage accounting, not physical pagefile traffic; not guest allocations |
| Page faults | Process cumulative counter | Soft and hard combined; hard fault rate unavailable |
| System RAM / pressure | GlobalMemoryStatusEx | Whole system; no allocation ownership |
| Read/write/other operations and bytes | Process I/O counters | Includes nondisk I/O, not disk-only bandwidth or decompression cost |
| Individual GPU engines | PDH GPU Engine, valid status 0/1 | Each instance retained; utilization is not GPU command duration |
| Process dedicated/shared/committed GPU memory | PDH GPU Process Memory | LUID joined to DXGI NVIDIA vendor, not assumed adapter index; not resource residency budget |
| NVIDIA clocks, power, temperature, utilization, memory controller, memory | nvidia-smi every fifth sample | Whole adapter; other apps/OBS/desktop contribute |
| Collector CPU, RAM, writes, NVIDIA child CPU | Own counters and child times | Child peak RAM, supervisor load and FPS impact not measured |
| HUD CPU/RAM/refreshes | HUD health record | Per HUD PID; restart continuity must not subtract counters across PIDs |
| UTC/QPC, elapsed, jitter, missed samples | Paired sample clocks / status | One-second sampling misses short episodes; pause resets delta history |
| Existing cache/PSO/shader/error log records | Incremental existing log | Observed records only, bounded tail, suppression/rotation; not total operations |
| App present/shown counts | Existing rounded FRAME-GEN log | Partial, source semantics in FRAME_GENERATION_MEASUREMENT.md; never guest FPS |
| Host/display FPS | PresentMon probe | NOT MEASURED: ETW session access denied, exit 6 |
| CPU stacks, context switches, ready/wait duration, blocking owner | Manual WPR preparation | NOT MEASURED, non-elevated token; no automatic elevation |
| Guest/prepared/generated IDs and latency | Matching internal instrumentation | NOT MEASURED |
| PM4 opcode and draw/dispatch/DMA counts | Matching internal instrumentation | NOT MEASURED |
| Buffer dirty transitions, transfers, sparse binds, aliases | Matching internal instrumentation | NOT MEASURED |
| Texture detile/retile/subresource costs, PSO creation cost | Matching internal instrumentation | NOT MEASURED |
| Queue dependency edges and calibrated GPU stage timestamps | Matching internal instrumentation | NOT MEASURED |

ETW losses are **unknown**, not zero. Sample skips are separate accounting.
PresentMon retention evictions are separate from upstream trace loss. Access
denied was saved in presentmon.stderr.txt; an empty event CSV is not zero FPS.
Frame metrics carry source, confidence, timestamp, age and loss availability.
Unavailable values remain null; stale integration values are flagged after 12s.
Collector/HUD stale state is visibly distinguished from active recording.

Anomaly windows retain up to 30s before and 15s after, bounded to 100 windows and
four pending windows and a 30s window cooldown. CPU/kernel, private growth, mixed page faults, system RAM,
I/O bursts, GPU low/CPU busy, adapter VRAM occupancy and HUD health rules run
where sources exist. Frame-drop/long-present rules require an actual stream;
wait duration and allocation churn rules cannot run from aggregate counters.
Nonzero process exits retain the exit code and a final incomplete post-window.
All classifications are candidates with cause unknown, not stall diagnoses.

Manual Capture-CPU-Stacks.ps1 does not elevate itself and refuses an unverified
existing WPR session. GeneralProfile is system-wide and must be filtered by the
verified PID/start identity with suitable symbols. It was not run during this task.
