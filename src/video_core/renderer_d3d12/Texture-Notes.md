The texture substrate is native D3D12 and accepts linear CPU texels only. Staging
objects must outlive the submission fence; no implicit queue waits are added.
GetCopyableFootprints defines GPU row pitch; source/destination pitches are
independent and padding is excluded from the transfer's visible rows.

Read-only source inspection: texture_cache/image_info.h carries PS4 TileMode and
IsTiled(); texture_cache/image.h owns Vulkan images/allocator and image views. A
port still needs actual PS4 detiling (micro/macro tile, bank swizzle, mip/slice
layout), AMD format/number-format to DXGI mapping, depth/HTILE/CMASK/FMASK handling,
compressed formats, cubemaps, arrays, per-subresource tracking, cache coherency and
aliasing. None is simulated or silently treated as linear by these helpers.

Hardware test covers RGBA8 odd-width/padded rows, exact readback, preserved CPU row
padding, SRV/RTV creation, D32 depth/DSV GPU clear and rejected short source. It
does not render a PS4 draw or prove tiled guest texture compatibility.
