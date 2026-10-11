# Frame generation: measured sources and limits

Installed executable SHA256:
`7d350796bdd27515f85951226fed73a7167487f0eb5b590e1d0cfb2504eab773`.
It matches the [test release](https://github.com/sunlighthell/shadPS4-Bloodborne-DLSS-FSR/releases/tag/test)
published 2026-10-11 00:08:54 UTC, despite the older directory name.
The executable reports eFMann v0.19.0, 974f8c21-dirty. The exact dirty patch is
unknown; the [base source](https://github.com/eFMann/shadPS4-Bloodborne-DLSS-FSR/tree/974f8c211c5c6b277f8da2fadcd30c4d770cb27b)
explains integration intent, not a byte-exact instrumentable source tree.

Loaded during actual Bloodborne gameplay: shadps4_dlss.dll (release digest
`1447c5b632bf3859f33892f8898750904fe7c8a590008251437d6b226cdc7791`),
nvngx_dlss.dll and nvngx_dlssg.dll 310.9.1.0; Streamline sl.interposer/common/
dlss_g/pcl/reflex 2.14.1.0. Existing OBS graphics-hook and NVIDIA nvspcap were
also loaded. The companion injects no module. Full module paths remain local.

## The path through the renderer

Guest VideoOut output is prepared by the Vulkan presenter. DLSS temporal work
uses game image, depth, motion, camera and HUDless resources to produce the
upscaled image. Presenter work and its readiness dependencies feed a Vulkan
swapchain intercepted by Streamline. Frame-generation SetFrame tags inputs and
enables DLSS-G only for eligible game output; reuse/non-game presentation is
distinct. Interpolation and additional presentation happen inside the SDK.
Reflex markers/sleep coordinate latency. Copy/compute/graphics activity from
PDH cannot partition emulation, upscaling, interpolation or overlays.

Base vk_bb_frame_gen.cpp:750–790 calls slDLSSGGetState in EndPresent.
numFramesActuallyPresented includes generated frames. If no frame is open or
state cannot provide a value, the integration can count one instead. CountPresent
increments **EndPresent calls**, uses one-second windows, and logs rounded rates
periodically. Its label `base` does not establish guest simulation or unique
prepared-frame identities. The installed source may contain additional changes.

## What the HUD means

| Quantity | Current source | Status / interpretation |
|---|---|---|
| Base game FPS | No unique guest simulation identity | NOT MEASURED |
| New prepared emulator frames | No exported unique frame IDs | NOT MEASURED |
| Host present / independent display FPS | PresentMon 2.6 attempted | Access denied, exit 6; NOT MEASURED |
| APP PRESENT / SHOWN | Existing FRAME-GEN log | Rounded application-reported counts; stale after 12s |
| Generated-only frames | No exported typed frame events | NOT MEASURED |
| Requested multiplier | Existing unchanged dlss.ini | x4 requested; distinct from active state |
| Active generation | Existing On/Off and SDK log | Reported state; not an independent frame classification |
| Guest frame time / display latency | No supported successful capture | NOT MEASURED |

Actual gameplay showed the installed counter changing, e.g. 55→220, 52→209,
54→216. Those are **integration-reported values**, not a measured guest FPS or
independent display rate. Integration summaries sample held rounded log values;
their averages are not independent frame-window averages. Foreground changes
and reuse can change generation; x4 requested does not guarantee x4 continuously.

Do not divide SHOWN, PresentMon FPS, or GPU utilization by four. A supported
Streamline query belongs to the application integration; this external companion
cannot safely call an in-process viewport state merely by loading the DLL.
No injection or fake source was introduced. PresentMon's reader is implemented
but its successful real stream and frame classification remain unvalidated here.

## Controlled follow-up

Obtain the producer's exact source plus dirty patch and build/dependency manifest.
Reproduce the installed profile and DLSS/FG runtime in an isolated copy, never
replace the installed core. Audit PrepareFrame/Present/reuse/EndPresent separately;
export guest output IDs, prepared IDs, SDK actual-presented state and typed
generated/repeated events with UTC/QPC, queue submission/completion IDs and
clock calibration. Measure instrumented-on/off overhead, then compare the same
stationary scene to DX12 with equivalent resolution/readback/pacing and FG stated.
The old c7e065d1 instrumenter cannot establish compatibility with this binary.
