# Next bounded production change — ordered offset buffer writers

Continue `ExecuteGuestResources` in src/video_core/renderer_d3d12/guest_compute.cpp, specifically same_base_uavs and the ordered transfer guard around lines485–532 (locate by symbol/text after edits).

Pinned source facts, already read; do not repeat a broad audit:

- Original buffer_cache.cpp:381–441, BufferCache::SynchronizeMemoryFromImage: metadata precedes image selection; line409 explicitly asserts the formatted reader address equals the image base. A non-base formatted reader is not new required functionality.
- TextureCache::FindImageFromRange:601–625 selects one address candidate or exact size, otherwise returns empty. Do not invent support for ambiguous images beyond that.
- Native NotifyGpuBufferWrite uses original logical sharp base, not native envelope base. An interior writer must preserve the preceding independent image I while changing buffer B; existing tests/implementation already preserve this for ordinary buffer-only writes.

Current narrower native guard accepts only overlapping UAV views with original and native addresses equal to image base. A bounded first extension is an offset, DWORD-aligned buffer writer fully contained in the full-image formatted reader range, with that reader before every writer. Preserve all existing same-base paths. Do not admit offset formatted readers, mixed image operands, DMA, multi-stage or two ambiguous image versions by weakening this guard.

Retile I into a captured B snapshot before preparing logical writes; preserve complete physical padding and existing protected I. The reader and writer must bind the same resident arena after the ordered replacement; original sharp limits and exact dirty ranges remain unchanged. Retain image pins and all notification/fence exception paths. A prior offset writer differs from a prior exact-base writer for image dirtiness: extending that order requires a separate explicit proof, so keep it rejected initially.

Use existing native-port/gcn_ordered_formatted_fixture.h and gcn_ordered_disjoint_fixture.h. Add a real-GCN offset-writer control to the existing sequence mechanism, with B != I, full image/padding checks, post-dispatch CPU B and image I probes. Prepared checks remain unrun until the complete implementation boundary. Compile/link production incrementally, commit, then run build-release.ps1 in the separate clean checkout. Do not overwrite preserved artifacts-6cbaef7 or install an unaccepted core as accepted.

Current reproducible production binary: commit6cbaef71857b04275be11013e62c05c2b3c672b3, artifacts in F:/PS4Dev/handoff/integrated-release-20261016/artifacts-6cbaef7. Latest source commits are normally pushed to origin/fix/mixed-cmask; verify actual HEAD/upstream before resuming. Goal remains active; nine broader confirmed gap IDs remain in IMPLEMENTATION_REGISTRY.md, each separate from behavior verification.
