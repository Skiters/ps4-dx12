# Installed Vulkan live companion — 2026-10-11

The existing Bloodborne shortcut now launches the original DLSS executable with
an external, live HUD and an independent collector. Real game launches, world
rendering, controller input, recording controls and lifecycle failures were tested.
Production Vulkan/DX12 renderer code is unchanged. This is a functioning external
measurement companion with partial coverage, not a full internal pipeline trace.

## Installed paths and controls

- Shortcut: `C:/Users/skei/Desktop/Bloodborne - DLSS Quality + HDR + FG x4.lnk`.
- Original executable: `F:/Apps/shadPS4-DLSS-DLAA-1.8.4/shadPS4.exe`.
- Programs and local sessions: `F:/PS4Dev/handoff/vulkan-live-monitor`.
- Original shortcut: `integration-20261011T132118Z/original.lnk` under that root.
- Ctrl+Shift+Home opens controls; Ctrl+Shift+Insert hides/shows the HUD;
  Ctrl+Shift+PageUp records a performance marker. The tray has equivalent actions.
- Panel Start/Stop/Pause controls recording independently of the game. Export
  creates a separate immutable complete-line snapshot and generates its report.

The wrapper verifies executable identity and preserves the original shortcut's
arguments, working directory and ShellExecute semantics. It works without Codex
remaining open. HUD is non-activating and click-through, aligned to the game
client. Its explicit controls panel is interactive. A stopped/capped session can
be restarted through Start; each segment has a new identity.

## Evidence and important failures

The installed binary matches the verified DLSS test release and reports
974f8c21-dirty; the directory's 1.8.4 label is not its source version.
See [frame semantics and source identity](FRAME_GENERATION_MEASUREMENT.md).
Loaded DLSS, DLSS-G and Streamline modules, unchanged ini and visible changing
integration counter (for example 54→216) establish the original integration
continued running. They do not establish guest FPS or independent display FPS.

Real sessions and acceptance evidence are indexed in SESSION_MANIFEST.json.
An initial collector stopped on an atomic status-file sharing error; the game
continued. Bounded retries and shared-delete HUD reading corrected that path.
The failure is retained. An early game close produced 0xC0000409; it is recorded
as a nonzero exit with unknown cause. A later normal window close, PID 39216,
completed with exit code 0 in both retained supervisor handle and collector.
PID 18880 also finished with 0xC0000409. The collector's earlier code 0 disagreed
with the supervisor's final retained-handle result. The derived report preserves
both and uses the verified final result. Final tooling now waits for the process
handle to signal before reading an exit code, and the supervisor refreshes reports
after all exit evidence is present. Do not relabel either nonzero exit as
successful teardown. The cause in the installed game is not established.

HUD kill/restart did not stop recording. Duplicate shortcut launch produced no
duplicate core. Start/Stop/Pause, marker, snapshot/export, width, expansion and
borderless/windowed return were exercised. Multi-monitor/DPI transitions and
exclusive fullscreen are not validated. See [acceptance details](HUD_VALIDATION.md).

The user's instruction to test gameplay autonomously led to a separate explicit
virtual Xbox feeder. After the user switched off the physical Flydigi, virtual
XInput index 0 registered as guest slot 0. The agent confirmed offline dialogs,
Continue, Central Yharnam movement and camera rotation; Start/B operated the game
menu. Input report and neutral release were independently read back. Foreground
refusal, duplicate feeder refusal and device removal on Stop were also tested.
No driver installation, game injection or controller/profile edits were needed.
See [controller protocol and evidence](../../../tools/vulkan_reference/XINPUT_CONTROL.md).
Ordinary shortcut launches do not create a virtual controller or send input.

## Measured resource cost

The completed 109.7343s world segment `session-20261011T133954Z-122d2443` measured
collector CPU 0.04286 core equivalents, HUD 0.00869, and NVIDIA child CPU 0.00655:
approximately 0.0581 logical CPU cores total, or 0.18% of this 32-logical-CPU
machine. Collector peak working set was 39.82 MiB; HUD 77.80 MiB. Collector writes
were 9,521,323 bytes over the segment. An independent 145.7425s snapshot gave
approximately 0.0576 cores total. These are observed companion resource costs,
not an on/off FPS or latency overhead measurement. Supervisor CPU and NVIDIA
child peak RAM are not included. The final anomaly cooldown reduces repeated
window writes; those earlier values are not measurements of that final revision.

## Available conclusions for DX12

Real measurements cover CPU/thread activity, process RAM/private/commit, mixed
page faults, process I/O, individual GPU engines, process GPU memory and NVIDIA
whole-adapter telemetry. Sources, ages and limitations are preserved. A named
NexusRevolution socket thread consumes about one logical core in these sessions;
its name does not reveal its function or establish an emulator critical-path
bottleneck. Loaded external modules and the foreground environment matter when
comparing overall resource cost.

The GPU engine counters preserve instances rather than summing unrelated engine
percentages. NVIDIA adapter utilization includes other programs. Neither proves
GPU draw/dispatch duration or guest frame rate. Free gameplay and mixed startup,
loading, UI and foreground phases are not a matched stationary DX12 benchmark.

PresentMon failed without elevation (exit 6 / ETW access denied); host/display
FPS and trace loss are unknown. Base/pre-FG guest FPS is NOT MEASURED. Internal
PM4, cache ownership/transfers, readback waits, texture operations, queue edges,
GPU stage times and stacks remain unavailable. The exact dirty source patch is
missing, so compatible deep instrumentation cannot honestly be declared ready.
An isolated source/build/measurement protocol is documented in the
[DX12 parity matrix](DX12_PERFORMANCE_PARITY_MATRIX.md); old c7 instrumentation
must not be transplanted into this installed binary.

No rendering optimization or performance gain for DX12 is claimed by this work.
The useful next step is to measure the ownership/queue/transfer boundaries listed
in that matrix on exact reproducible source, with equivalent pacing/resolution
and instrumentation cost stated.

## Preservation and rollback

Original executable, DLSS libraries, game custom config and dlss.ini hashes were
preserved. The global profile was rewritten by the original core on the first
launch (its initial hash differs); it stayed equal to the pre-lifecycle baseline
afterward. We did not restore an older migrated config or edit saves. Normal
gameplay can write autosaves/cache/logs. Such mutable data is not asserted
byte-identical and is not published. Shortcut rollback was tested byte-for-byte
on a copy; the habitual shortcut remains integrated.

Raw local logs, game data, saves, portable profile, modules and binaries are not
committed. Only tools, documentation, sanitized identities and derived validation
results are published. Later records retain their own source/component hashes.
