# shadPS4 native DirectX 12 fork

Separate experimental Windows fork of shadPS4, derived from the Bloodborne DLSS/FSR project. GPL-2.0-or-later; upstream copyright and license notices are preserved in source files and LICENSES.

This fork uses D3D12/DXGI for guest rendering, presentation and the emulator UI. It has no Vulkan renderer, Vulkan bridge, or fallback to the original application. SPIR-V is used only as an offline intermediate when compiling PS4 shaders to DXIL. SDL Vulkan backends are disabled.

The downloaded original shadPS4 application and its Vulkan configuration are separate and unchanged.

## Current status

Native-only Bloodborne title/menu pixels have been verified. The latest build resolves observed uniform-CMASK, null-resource and depth-bounds failures and reaches saved-world loading, but playable-world rendering/performance remain unverified. 64/64 native checks pass, including isolated real XeSS SDK execution. XeSS game integration, XMX execution, frame generation and native 60 FPS remain unverified. See [port status](NATIVE_DX12_PORT_STATUS.md) for evidence.

## Local build and launch

- `native-port/Build-Core.cmd`: full Release emulator core.
- `native-port/Build-Check.cmd`: isolated native hardware/compiler checks.
- `desktop/PS4Desktop.csproj`: Windows library, PKG installation and native-core launch menu. See [desktop notes](desktop/README.md).

The scripts use the configured Visual Studio 18 toolchain and read-only dependency sources from the original checkout. Build products are separate. Installed menu: `C:\Games\PS4-Desktop\PS4Desktop.exe`, registered in Orbit as “PS4 — библиотека и установка PKG”.

