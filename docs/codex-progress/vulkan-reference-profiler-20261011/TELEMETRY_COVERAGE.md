# Reference telemetry coverage

Status as of 2026-10-11. Source hooks are prepared and compiled; they are not
measurements of Bloodborne until a controlled Mode B trace is collected.
Mode A clocks pair UTC and QPC. Mode B uses CPU steady-clock nanoseconds with a
UTC anchor; no GPU-to-CPU calibration. All Mode B event distributions/call trees
are incomplete if `dropped_events != 0`; fixed totals survive when aggregate
overflow is zero. No sampled CPU stacks were collected.

| Subsystem / metric | Mode / source point | Actual status | Timestamp / frame / accuracy / limitation |
|---|---|---|---|
| Process CPU user/kernel, RAM/private, I/O, faults | A / Win32 process counters | MEASURED | UTC/QPC; no frame; faults include soft faults; I/O is process accounting, not storage bandwidth |
| Individual thread CPU/name/priority | A / Toolhelp, GetThreadTimes/Description | PARTIALLY_MEASURED | CPU clock quantized; first 700-sample segment has no names due HRESULT bug; corrected subsequent PID uses names; inaccessible/exited threads omitted |
| Ready time, context switches, affinity, migrations, hot stacks | A / ETW/WPR | NOT_MEASURED | Installed WPR/WPA/xperf; non-admin token, no kernel recording/elevation; no fabricated loss count |
| Process GPU engines + LUID | A / English PDH GPU Engine | MEASURED | Per engine and adapter; accounting intervals; 3D includes compute workloads on that engine; not GPU operation timing |
| Process dedicated/shared/committed GPU bytes | A / GPU Process Memory | MEASURED | Separate from adapter-wide usage; not allocation lifetime/fragmentation or physical sparse backing |
| Adapter power/clocks/temp/utilization/memory | A / nvidia-smi | PARTIALLY_MEASURED | Initial collector path bug: missing; corrected sensor test and new game segment: available; whole adapter includes other processes |
| System-wide pressure/hard fault attribution/evictions | A / not yet integrated | NOT_MEASURED | Old 5-second collector has system memory counters; no per-resource eviction trace |
| Collector CPU/RAM/I/O/jitter/missed samples | A / self counters | PARTIALLY_MEASURED | NVIDIA child CPU included only after fix; child RAM absent; workload interference is separately marked |
| Host Present/display cadence | A / installed PresentMon probe | UNSUPPORTED_BY_TOOL | Probe exited 1 with no CSV/diagnostic; this is a local probe result, not universal Vulkan incompatibility |
| Log and device-error candidates | A / capped existing log tail; Application Event Log 1000/1001 | PARTIALLY_MEASURED | No screen observation; errors in logs are distinct from collector errors; no matching event is not proof of normal exit |
| Anomaly pre/post windows | A / configurable memory/fault/kernel/log rules | PARTIALLY_MEASURED | 30s before / 15s after, bounded; no frame-based stall detection without frame stream |
| Native guest CPU execution vs HLE | B / native entry; HLE mutex/condvar/sema/event/sleep scopes | REQUIRES_INSTRUMENTED_BUILD | Native x86 guest; no separate total guest execution timer or exhaustive HLE instrumentation |
| Guest JIT/code-cache metrics | Source / Linker native entry | UNSUPPORTED_BY_TOOL | This guest path runs native x86; no assumed guest JIT counters; do not confuse shader translation with guest JIT |
| PM4 opcode/queue incidence | B / three Liverpool type-3 decode sites; fixed opcode counters | REQUIRES_INSTRUMENTED_BUILD | CE=0, graphics=1, compute=2; exact counts independent of event drops; type-0 packets and coroutine wall timing not covered |
| Draw/index/indirect/dispatch/copy/fill CPU work | B / Rasterizer Draw/DrawIndirect/DispatchDirect/DispatchIndirect/CopyBuffer/FillBuffer | REQUIRES_INSTRUMENTED_BUILD | Nested CPU/wall scopes, indexed flags/addresses/counts; no causal per-frame assignment or repeated-parameter exhaustive map |
| BufferCache residency/requests/stream/upload/download | B / ObtainBuffer, EnsureResident, GetArena, SynchronizeMemory, DownloadMemory, ReadMemory | REQUIRES_INSTRUMENTED_BUILD | Requested spans differ from actual recorded transfer bytes; resident-hit bytes, new physical allocation and sparse bind counts separate |
| CPU/GPU dirty transitions and partial ranges | B / MemoryTracker mark/unmark/invalidate/ForEachUploadRange/ForEachDownloadRange; PageManager | REQUIRES_INSTRUMENTED_BUILD | Range scopes are implemented; exact changed-page cardinality, protection-fault stacks and reasons for every proof release remain missing |
| Staging request/reuse/waits | B / StagingBufferPool Request/RequestLarge/RequestFromRing/FreeDeferred/TickFrame; StreamBuffer | REQUIRES_INSTRUMENTED_BUILD | Request bytes/type/alignment; exact fragmentation and lifetime attribution not implemented |
| Texture lookup/refresh/alias/invalidation | B / TextureCache FindImage/FindTexture/RefreshImage/Download/Register/Unregister | REQUIRES_INSTRUMENTED_BUILD | CPU scopes; full view/subresource identity and dirty reason taxonomy still missing |
| GPU detile/retile | B / TileManager DetileImage/TileImage + dispatch query pairs | REQUIRES_INSTRUMENTED_BUILD | Guest address/size/tile mode/bits at detile; GPU dependency span; mips/layers/retile resource correlation partial; 13 pipeline creations are historical log evidence, not dispatch counts |
| Ordinary index residency / conversion frequency | B / BindIndexBuffer; index.binding | REQUIRES_INSTRUMENTED_BUILD | Address/size/16-bit/restart; no new real command counts yet; conversion branch reason census pending |
| Compute shader hash/dimensions/timing | B / compute.shader + GPU direct/indirect dispatch queries | REQUIRES_INSTRUMENTED_BUILD | Direct hash via parent; indirect hash, wave size and per-shader resource bytes missing; no real top-20 yet |
| GCN translation/SPIR-V/PSO caches | B / CompileModule, GCN_translate, SPIRV_emit, GetProgram, GetGraphics/ComputePipeline, WarmUp | REQUIRES_INSTRUMENTED_BUILD | Hash/new-program/new-pipeline events; creation happens inside Get*Pipeline, not nonexistent Create* methods; first-use vs startup must be labeled by interval |
| Descriptor/binding/render state CPU | B / BindResources/PrepareRenderState/BeginRendering | REQUIRES_INSTRUMENTED_BUILD | No exhaustive format/blend/CMASK/HTILE/MSAA state-combination census; pipeline-key hashes alone do not establish correctness |
| Barriers/copies/clear/resolve semantics | B / Runtime AccessBuffer/FlushBarriers/CopyBuffer/FillBuffer/UploadImage/DownloadImage/CopyImage; Image GetBarriers | REQUIRES_INSTRUMENTED_BUILD | CPU scope counts; full producer-resource-consumer graph and GPU graphics/copy timing remain missing |
| Submission/Finish/Wait/retirement | B / Scheduler + Semaphore Wait/Refresh | REQUIRES_INSTRUMENTED_BUILD | Tick/object at waits; call lifetime includes early non-blocking paths; necessary-vs-redundant classification remains unverified |
| Sparse backing / image physical allocation | B / EnsureResident allocation + UniqueImage Create/Destroy | REQUIRES_INSTRUMENTED_BUILD | Physical sizes separate from virtual arenas; complete resource lifetime, texture eviction and device heap fragmentation not reconstructed |
| Guest flip/prepared frame/completion/present reuse | B / VideoOut SubmitFlipInternal, Presenter Frame ID, frame.ready/completion, Present identity | REQUIRES_INSTRUMENTED_BUILD | Actual output Frame IDs and flip association; completion CPU observation is not exact GPU instant; no displayed time/cancel reason; earlier PM4 attribution is only a hint |
| End-to-end CPU/GPU guest frame times and idle/latency P95/P99 | B / causal flow not completed | NOT_MEASURED | Prepared-frame data is partial; no full frame dependency ledger, graphics timestamps or calibrated display timing |
| File loading/decompression/audio/asset lifetime | A process I/O; B memory mapping scopes | PARTIALLY_MEASURED | Loading scenes not labeled; file-path stacks/decompressor/audio costs and asset attribution missing |
| Vulkan/DX12 matched performance | Controlled protocol | NOT_MEASURED | Different current resolution/readback/cache/scenes; no speed ratio or FPS gain estimate |

The zero-sample failed segment is INVALID_MEASUREMENT. Synthetic fixture/stress
data is explicitly separate from game data. A successful build or test does not
promote any gameplay-only row above to MEASURED.
