# Shifted writable views and tessellation control flow

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
