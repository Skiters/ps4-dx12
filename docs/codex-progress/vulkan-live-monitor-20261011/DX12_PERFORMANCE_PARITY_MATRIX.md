# Execution interactions and DX12 measurement gates

This supplement distinguishes installed-runtime observations from base-source
mechanisms. See the previous detailed mechanism matrix in
../vulkan-reference-profiler-20261011/DX12_PERFORMANCE_PARITY_MATRIX.md for
existing native DX12 correctness tests and historical measured costs. That older
Vulkan source is c7e065d1; it is not proof of this dirty DLSS binary's internals.

| Path / interaction | Vulkan reference mechanism | Native DX12 counterpart / required evidence |
|---|---|---|
| Guest CPU → HLE → command processor | Guest threads produce PM4, synchronization and VideoOut work; named threads observed | Shared guest/HLE path. Export packet opcode, parent queue/guest frame, TID/QPC; stack sampling distinguishes actual hot/wait functions |
| PM4 → buffer bindings | Address-based cache identity, physical residency and dirty validity constrain resource views | rasterizer.cpp PrepareShaderBuffers, page_watch.h and allocation leases. Measure requested/dirty/transferred spans, proof-release reasons and alias producer/consumer |
| CPU writes ↔ GPU ownership | CPU/GPU validity and required CPU-visible readbacks determine downloads and synchronization | Preserve watch/ownership proof. Record CPU consumer, range, fence and actual wait before removing Finish or deferring readback |
| GPU upload/copy → draw/dispatch | Queue ordering and barriers make written bytes available to shader consumers | command_scheduler.cpp copy/direct dependencies and retirement. Queue waits are not host waits; measure both and preserve resource lifetime |
| Texture bytes → detile → sampled images | Texture cache converts tiled layouts, tracks views/subresources and aliases | guest_texture_cache.cpp CPU conversion fallback. Obtain actual layout/bytes/exclusive CPU and calibrated GPU timing before selecting GPU detile work |
| Indices / indirect arguments → draw | Resident index bindings and GPU-produced indirect argument paths | guest_rasterizer.cpp conversion/CPU indirect decoding. Confirm Bloodborne incidence and arguments/SGPR semantics before direct binding or ExecuteIndirect |
| Guest shaders → compiler → PSO/cache | Shared translation plus Vulkan pipeline creation/preload | guest_shader_cache / guest_pipeline_cache. Split cold/warm lookup/creation and parent-frame stall attribution; log entries alone are incomplete |
| Color/depth metadata → visible pixels | Clear/CMASK/HTILE/FMASK and ownership interact with texture materialization | Existing mixed-CMASK correctness work must survive optimizations. Collect exercised formats/state, compare surfaces; speed is insufficient |
| Game output → temporal upscaling | Installed DLSS evaluates depth/motion/camera/HUDless inputs, outputs 4K | Separate PS4 rendering cost from upscaling. Compare same internal/output resolutions; count actual evaluations and calibrated stages |
| Prepared output → FG → swapchain | Streamline tags resources, interpolates/presents, integrates Reflex | GuestPresenter/Presenter and native pacing. Distinguish unique/reused/blank/generated frames and latency; independent display stream unavailable here |
| Resource lifetime → presentation completion | GPU readiness and SDK ownership constrain reuse and teardown | Keep presentation and queue fence dependencies; observed close exit 0xC0000409 requires isolated teardown diagnosis, not exception suppression |

Measured external CPU/engine/memory data can select **where to add attribution**.
It cannot identify which Vulkan call costs most, whether a CPU wait is redundant,
or how much DX12 will gain. Existing historical DX12 snapshot comparison cost
supports investigating watch-proof retention first; source gaps in index binding,
detile, indirect dispatch and prewarming remain unranked pending real incidence.
No graphics backend implementation is changed by this monitor commit.

## Exact-source Mode B protocol

1. Obtain the release producer's 974f8c21 dirty diff, source archive, dependency
   gitlinks and build/runtime manifest. Pin archive hashes. A matching base hash
   alone does not pass this gate; the current gate is BLOCKED_SOURCE_PROVENANCE.
2. Create an isolated source/runtime/profile copy, preserve DLSS/FG DLL versions
   and patch/config semantics. Do not reuse prepare_mode_b.py's hardcoded c7 hooks
   without adapting and auditing each definition against this exact tree.
3. Instrument bounded scopes and events: guest/PM4/VideoOut IDs; buffer dirty,
   upload/download/sparse bind ranges and bytes; texture subresource conversions;
   shader/PSO cache decisions; queue waits/submissions and actual waited ticks.
   Attach parent IDs, reason and source confidence. Never invent missing IDs.
4. Add per-queue calibrated timestamp pairs, query validity, completion fence,
   timestamp period and wrap handling. Keep emulation/detile/DLSS/FG/presentation
   stages separate; CPU inclusive scopes cannot be added as exclusive time.
5. Export SDK actual-presented state with unique prepared and generated/repeated
   classification. Validate against a successful independent display capture.
6. Verify instrumented-on/off overhead and pixels/ownership on isolated saves.
   Then run repeated stationary Relaxed, Precise and DX12 scenarios with matched
   resolution/patches/pacing/cache state. Record FG requested/actual separately.

The current build cannot safely provide complete internal profiling from outside
its process. There is no new matched instrumented executable in this delivery.
The old c7 instrumented build remains a separate controlled reference, not the
user's installed DLSS/FG gameplay binary. This is an explicit technical gate,
not an assertion of full execution-pipeline coverage.
