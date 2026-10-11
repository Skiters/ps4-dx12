# Instrumented event schema and accuracy

`internal-events.tsv`: name, id, parent, frame_hint, tid, start_ns, end_ns, cpu_ns,
exclusive_wall_ns, exclusive_cpu_ns, a/b/c/d. IDs are trace-local. Scope events
are written on return; instant events have equal start/end. Parent is the active
instrumented scope, not a sampled stack. GPU deferred events preserve the recording
parent ID. Missing parents can result from event loss; they are not invented.

CPU time is GetThreadTimes user+kernel, coarse 100-ns units with OS accounting
quantization. Wall includes waiting. Exclusive subtracts only instrumented
children on the same thread. Untimed callees remain in the parent's exclusive
cost. Instrumentation overhead is present; profiler-on/off calibration is pending.
`trace-clock.json` pairs CPU monotonic and UTC at start; it is not GPU calibration.

| Name / scope | a | b | c | d |
|---|---|---|---|---|
| ObtainBuffer / SynchronizeMemory | guest address | requested bytes | GPU-written flag | texel flag |
| EnsureResident / GetArena | first block | last block (inclusive) | 0 | 0 |
| buffer.resident.hit | already resident requested block span bytes | first block | last block | 0 |
| buffer.physical.allocate | physical allocated bytes | newly resident blocks | 0 | 0 |
| buffer.upload.bytes / buffer.download.bytes | actual recorded transfer bytes | requested guest address | requested span bytes | 0 |
| buffer.sync.result | actual dirty upload bytes | requested span bytes | copy region count | 0 |
| buffer.stream.cpu_copy.bytes | logical CPU copy bytes | guest address | 0 | 0 |
| buffer.sparse.bind | buffer bind-info count | memory signal tick | 0 | 0 |
| MemoryTracker scopes | guest range address | requested range size | 0 | 0 |
| StagingBufferPool Request | requested bytes | memory type enum | alignment | deferred flag |
| image.physical.allocate | VMA physical allocation bytes | memory type | width | height |
| image.physical.free | physical allocation bytes | 0 | 0 | 0 |
| pm4.opcode | enum opcode | queue kind 0=CE,1=graphics,2=compute | 0 | 0 |
| Draw | indexed flag | index offset | 0 | 0 |
| DrawIndirect | indexed flag | argument address | max count | 0 |
| DispatchIndirect | address | offset | size | 0 |
| index.binding | guest address | bound bytes | index16 flag | primitive-restart flag |
| compute.shader | direct shader hash | dispatch X | dispatch Y | dispatch Z |
| gpu.guest_compute / gpu.guest_compute_indirect / gpu.detile_or_retile | GPU dependency span ns | raw GPU start ticks | raw GPU end ticks | scheduler tick |
| TileManager::DetileImage | guest address | guest bytes | tile mode enum | bits per element |
| pipeline.graphics/compute.lookup | key hash | newly inserted flag | 0 | 0 |
| shader.program.lookup | program hash | newly inserted flag | 0 | 0 |
| CompileModule | shader hash | permutation | 0 | 0 |
| shader.GCN_translate / shader.SPIRV_emit | shader hash | 0 | 0 | 0 |
| Scheduler::Wait | tick | 0 | 0 | 0 |
| Semaphore::Wait | tick | semaphore object address | 0 | 0 |
| PthreadMutex/Cond wait | object address | timeout us | 0 | 0 |
| frame.ready | ready timeline tick | width | height | 0 |
| frame.gpu_completion_observed / frame.return | ready tick | 0 | 0 | 0 |
| frame.guest_flip_link | video buffer index | flip argument | EOP flag | 0 |
| Presenter::Present.frame_identity | actual Frame ID | reuse flag | game-frame flag | 0 |

For frame events `frame_hint` is explicitly the corresponding output Frame ID.
For earlier unrelated PM4/resources it is only a global latest-prepared hint.
No CPU-frame duration, display timestamp or full dependency graph can be recovered
from that hint. PrepareBlankFrame can reuse an object; the game-frame/reuse flags
must be retained when interpreting Present identity. Guest flip attempts can be
rejected; SubmitFlip call counts alone do not equal accepted frames.

`aggregate-counters.csv`: exact per-label calls and inclusive/exclusive CPU/wall
totals, plus sum_a. **sum_a is bytes only for byte-valued labels**, not arbitrary
request-address/hash/opcode fields. Fixed storage has 4096 label slots; overflow
invalidates completeness. `pm4-opcode-counts.csv` separately retains exact
type-3 opcode/queue counts across event drops. Type-0 packet counts are absent.

The 32768-entry event ring intentionally drops under contention/full capacity;
draining/writing is separate every 250ms, TSV is capped at 512MiB. Dropped-event
P95, parameter distributions, detailed call trees, resource/parent edges and GPU
sample distributions describe only retained events. The stress fixture forced
loss and validated aggregate totals; it did not measure game overhead or GPU correctness.

Report omissions are recorded in `summary.json.unavailable_outputs`. No empty
CSV is presented as a successful unavailable measurement. Full graphics-state
census, resource lifetimes, heaps, file/decompression scopes, calibrated graphics
GPU timestamps and PM4-to-output causal propagation remain specific follow-up points.
