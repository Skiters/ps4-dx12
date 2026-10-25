# Shifted writable views and tessellation control flow

## Latest production: 9cdf3a2

655a422 connects MapGuestWritableShaderSubview to shader access promotion
(BuildGraphics and BuildCompute) and cold-cache resource preparation. Exact
slice addressing, format and complete parent layout are required. Prepared
writable-shifted-array-owned exercises real GCN load/store/barrier/load, both
descriptor orders, cold/warm linear and micro-tiled parents, negative mapping
cases and full physical-byte oracle. NOT_RUN. Core/fixture compile/link PASS
16.122s (071012-27876), clean Release PASS18.859s (071109-13796), identity
shifted-shader-655a422.json. StageVersion50.

ed35cc3 connects shifted color/storage descriptors to a common draw envelope,
passed through ordinary, indirect and capture resource preparation. Conservative
overlap chooses ordered output, but actual resource sharing still requires the
physical mapping proof. Equivalent padded CB and shader envelopes keep one
owner. Prepared color-storage-shared-shifted-owned and
color-storage-shared-shifted-cold-owned retain real image stores/reads and color
exports, independent B and all physical padding; NOT_RUN. Missing forward
declaration was caught by compiler (071517-5424), corrected. Final fixture
compile/link PASS10.243s (071728-15136), clean Release PASS20.832s
(071759-5800), identity shifted-color-ed35cc3.json.

9cdf3a2 partitions oversized tessellation/auxiliary SO sources only at original
instance boundaries. No guest patch is cut or replayed. Preserve per-instance
PrimitiveId domain and original BaseInstance through the existing capture
runtime field. InstanceArguments retains GPU-produced index count while updating
the exact instance range. Intermediate Finish allows completed multi-gigabyte
SO arenas to be reused while logical bindings stay pinned; attachment final
actions run only on the final source/replay slice. One instance exceeding the
32-bit SO limit STILL requires a separate source scheduling mechanism. A normal
source fitting the limit retains one producer draw. StageVersion51.

Prepared source-instance-partitions reads real SO contents/counters for a nonzero
first instance, partial last slice, zero instances and arena reuse. Existing
GPU restart packet readback now checks1..4-instance ranges and retains the
unnormalized-first-index negative case. CPU plans check UINT32_MAX instance
coverage, empty draws and undersized budgets. Old factor-fixture fields updated
to guaranteed masks. ALL NOT_RUN. Core/primitive fixture compile/link
PASS19.082s (072216-27752), final fixture PASS7.456s (072343-30904), clean
Release PASS20.785s (072412-2864). Identity instance-source-9cdf3a2.json.

All three commits pushed normally to fix/mixed-cmask. No app-core replacement,
game run, executable fixture run or runtime archive. Identity/log directories
are unchanged below. Next: original-GS triangle polygon points and remaining
shared MS/expanded image domains; oversized single-instance sources remain
explicitly open. Dynamic-mip shifted view arrays are not admitted by the new
single-binding helper. Do not stop at this checkpoint.

Production checkpoint: 671a07e, after b8807f8, 47ffa7e and 4fdd1f6.
All executable checks NOT_RUN by the current user instruction. Compile/link is
not runtime, image, synchronization or FPS acceptance. Installed core unchanged.

- b8807f8: RebaseColorSubresource retains writable parents and exact shifted
  mip/layer views. Entry::Srv/Rtv/Uav require actual native cast/usage support.
  No converted snapshot of a writable owner. Prepared
  writable-color-array-subviews uses RTV/UAV writes, full-parent readback and
  padding oracle; compile/link only (064747-17388). Clean Release PASS
  (064829-11044), identity shifted-writable-b8807f8.json.
- 47ffa7e: polygon POINT PerVertexKHR and barycentric inputs lower to OpUndef;
  ordinary point primitives and defined inputs retain their semantics. The
  [Vulkan KHR contract](https://docs.vulkan.org/spec/latest/chapters/primsrast.html#primsrast-barycentric-interpolation)
  leaves these accesses undefined outside FILL. Do not reintroduce a
  three-vertex transport requirement for undefined accesses. Prepared
  polygon-undefined-inputs retains stores and defined inputs and tests the
  ordinary-point negative control; NOT_RUN, including its DXIL conversion mode.
  Fixture compile/link PASS (065304-12548), clean PASS (065417-11424), identity
  polygon-undefined-47ffa7e.json.
- 4fdd1f6/671a07e: DrawInternal selects active isoline/triangle/quad factors.
  CollectShaderInfoPass computes guaranteed writes through CFG predecessor
  intersections. Branches, early returns and loop bypasses cannot manufacture
  a bound. Missing/dynamic factors retain64; guest tessellation is unchanged.
  Cache StageVersion49. Prepared tessellation-source-bounds adds18 CFG/domain
  cases; NOT_RUN. Main core compile/link PASS33.106s (070003-23852), fixture
  PASS8.420s (070105-32092), clean Release PASS40.043s (070127-17608).

All identities are small JSON files under
F:/PS4Dev/handoff/native-port-comparison-20261019/:
domain-bound-4fdd1f6.json and cfg-bound-671a07e.json supplement the files above.
Full compiler logs: F:/Codex-Shared/devtools/logs/command-20261025-*.log.
No runtime archives or new binary acceptance claims.

Remaining: shifted simultaneous shader SRV/UAV promotion and attachment mapping
beyond cache lookup; shared MS/expanded image domains; original-GS polygon
triangles, unclamped polygon depth/bias, full varying payload; general large or
dynamic tessellation source scheduling. Mixed blending is connected/unaccepted.
Other-vendor clock remains conditional on the pinned original hardware capability.
Pinned vk_rasterizer.cpp:1559 rejects custom restart values other than0xffff and
0xffffffff. Our complete CPU fallback for custom32 is connected; native batching
of additional markers is an optimization, not absent pinned functionality.

Continue production after this checkpoint. Do not repeat the129-group audit.
Preserve unknown edits, profiles, original reference and unresolved crash data.
Reuse main/clean build trees and perform normal pushes. All executable tests
remain deferred until the seven implementations finish.
