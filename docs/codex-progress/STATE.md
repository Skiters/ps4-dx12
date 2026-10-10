# Native DX12 state

Updated 2026-10-10 from baseline `01c0374`.

The native D3D12 backend now records bounded per-stage CPU wall-time distributions when
`SHADPS4_NATIVE_DIAGNOSTICS=1`. The two-second report includes calls, total, average,
maximum, P50, P95 and P99, plus counters for compute PSO/DXIL caches, descriptor pages,
GPU submissions and copy-shader HLE traffic. Diagnostics remain dormant otherwise.

Bloodborne's recognized copy shader was the largest measured compute-dispatch stage.
`Rasterizer::CopyBufferBatch` now materializes a 4 KiB page envelope when the complete
source and destination pages are validated. Exact `CopyBufferRegion` ranges are unchanged.
Whole page destinations can remain GPU-owned after submission and reconcile on their first
CPU access; PRT edges and partial/unprotected backing retain the eager `Finish()` path.

Confirmed in the controlled 66-second run: 1726 of 1727 copy-HLE dispatches used deferred
reconciliation, no DXIL recompilation occurred after disk-cache loading, and no D3D12 debug,
device-removal or native exception marker was logged.

GPU timestamp queries are not implemented. Stage values are CPU wall time; `fence-wait`
and `buffer-reconcile/submit-and-wait` isolate CPU wait wall time. CPU execution time cannot
be separated from scheduler preemption with the current portable instrumentation.
