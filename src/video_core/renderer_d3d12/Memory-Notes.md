# Guest memory mapping integration

Read-only inspection of core/memory.cpp and Vulkan Rasterizer established:

- MapMemory adds GPU-valid virtual coverage; it does not upload guest memory.
- RegisterMemory notifies PageManager::OnGpuMap after host backing changes.
  Commit paths call it for physical fragments and MapMemory for the enclosing
  area. Decommit/unmap paths also call it after impl.Unmap.
- SetPrtArea marks an entire reserved PRT range as mapped, with a specific comment
  requiring CopySparseMemory instead of direct host reads over sparse holes.
- UnmapMemory may remove a subrange of an existing mapped interval.

The initial native eager allocation was therefore unsafe. Native MapMemory now
stores interval-union metadata and RegisterMemory only invalidates existing cached
GPU spans after their submissions complete. Neither dereferences the supplied
address nor creates a GPU buffer. Partial Unmap subtracts coverage and reconciles
only previously materialized buffers before host unmapping.

Actual Fill/Copy lazily materialize only the touched slice. A correctness-first
64 MiB GPU cache budget prevents one gigabyte mapping from becoming a gigabyte
GPU buffer/upload. Individual Copy is limited to 32 MiB so both participating
buffers fit the budget. Overlapping cached slices are reconciled back to CPU
before replacing them; same-resource overlapping Copy uses a GPU temporary.
Eviction is synchronous for now and is not a performance-optimized page cache.

Hardware regression tests include metadata-only mapping/registration of a fake
64 GiB address range, partial interval removal, and no GPU-cache allocations,
alongside actual buffer/GDS copy and readback checks on Arc B390.

Remaining blockers before integrating real guest memory callbacks:

1. Materialized buffer reads still require valid committed host memory. PRT
   sparse reads need MemoryManager::CopySparseMemory (or a supplied sparse-copy
   callback); direct access must never be used for holes.
2. Different guest virtual addresses may alias the same physical memory. This
   interface currently lacks backing-page identity. Per-physical-page ownership
   and alias invalidation must be supplied by MemoryManager/PageManager before
   claiming full alias coherency. Virtual-overlap handling is not physical-alias
   handling.
3. Calls must be serialized. Existing guest memory manager callbacks and GPU
   command processing may use different threads; a lock-order-compatible page
   manager integration is still required.
4. Large operations need chunked resources and sparse/reserved-resource mapping;
   the current explicit bounds reject them rather than exhausting RAM/VRAM.
5. CPU fault/write tracking and texture-buffer alias invalidation are absent.

No game compatibility is implied by the bounded lazy buffer test.
