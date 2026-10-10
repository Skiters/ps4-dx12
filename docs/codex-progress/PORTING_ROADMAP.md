# Functional port roadmap

Reference and evidence boundaries are in VULKAN_DX12_PARITY.md.

1. EPIC E: timestamp retirement corrected; pending copy API now exposes recording rather
   than a false fence. Preserve batching and test exact completion visibility.
2. EPIC A: reproduce PrimitiveType::None / depth-override ordering mismatch, then match
   Vulkan FilterDraw ordering. Add an integration test through GuestRasterizer, not only
   a test of a duplicated predicate.
3. EPIC A/D: differential fixtures for indirect dispatch, especially direct-only HLE paths
   and arguments produced by previous GPU writes. Native ExecuteIndirect migration requires
   adapting resource preparation and resource lifetime; do not simply substitute the call.
4. EPIC B: compare MemoryTracker regional state with existing DX12 dirty_ranges. First
   record bounded address/range samples to establish whether whole-mapping ownership release
   explains the traffic. Invariants: preserve untouched bytes, aliases, PRT and fence ownership.
5. EPIC C/D: exact pixel/result comparisons for texture aliases and shader operations selected
   by the first divergent command in a reproducible Bloodborne trace.
6. EPIC F/G: establish saved-world milestone evidence before same-scene performance claims.

No broad buffer-cache rewrite is authorized by evidence yet. Full differential runner and
full all-files audit remain outstanding; current source comparison is not runtime parity.
