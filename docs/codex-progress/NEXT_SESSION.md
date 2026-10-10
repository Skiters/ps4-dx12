# Next session

Prioritize `resource-upload-dispatch-writeback`, which is now the other large compute stage.

1. Add a report that attributes non-overlapping `resource-bindings` sub-stages rather than
   summing nested scopes.
2. Separate buffer preparation cache hits, newly materialized bytes, upload bytes and calls
   that enter `Finish()` from `PrepareShaderBuffers`.
3. Add D3D12 timestamp queries around compute command ranges before drawing conclusions about
   GPU execution time.
4. Test the hypothesis that repeated buffer validation/materialization, rather than descriptor
   allocation or PSO compilation, drives the remaining 11–13 ms long tail.
5. Keep exact guest memory coherence and the PRT-edge fallback; do not batch submissions until
   the producing and consuming call sites establish safe queue ordering.
