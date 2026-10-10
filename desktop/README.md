# PS4 Desktop

Separate Windows library/menu for the shadPS4 native DirectX 12 port. Installed at C:\Games\PS4-Desktop\PS4Desktop.exe and registered in Orbit as “PS4 — библиотека и установка PKG”.

The library scans installed game folders/SFO metadata, caches cover thumbnails, imports folders, launches games and installs PKGs asynchronously with progress/cancellation and transactional extraction. Existing game directories are not replaced. Base games, update sidecars and DLC directories are supported.

The menu launches the experimental native core through native/shadPS4-DX12.exe. The core and required DLLs reside in native/core, independent of the source/build directory. Native-only Bloodborne title/menu pixels were verified. The latest core resolves the observed uniform-CMASK/null-resource/depth-bounds failures and reaches saved-world loading; saved-world rendering, practical performance and stability remain unverified. gameLaunchReady remains false.

New and currently installed settings select dx12. The original-core route has been removed from this menu. Native launches use native-profile with its own config and copied saves. Original profile/game data are preserved. Shared DLC data stay in the original install location.

Controller: D-pad selects games; A launches/activates; X opens PKG selection; Y refreshes; B cancels/returns focus; LB/RB moves control focus. The Windows file picker uses normal touch/mouse/keyboard controls.

Checks: --self-test constructs the menu and scans games; --native-probe-test checks the installed D3D12/DXGI connection; --prepare-native-profile prepares/checks the separate profile. These commands do not launch games. The probe checks device/presentation, not game readiness.

Build: dotnet publish PS4Desktop.csproj -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -o publish

Migration build (2026-10-09): clean installs use profile-template and create their
data under %LOCALAPPDATA%/PS4Desktop. No old Bloodborne folder, user name, external
Python or original emulator is required by the packaged menu/PKG installer.
Existing adjacent settings.json remains supported for the older manual install.
An empty library is valid. PKG installation prepares the native profile before
extracting DLC, and uses the same addcont folder as the emulator. Diagnostic child
flags are removed by NativeLaunchPolicy. See setup/Make-Installer.ps1 and the
migration directory for build/dependency/handoff instructions. The core remains
experimental; portability does not resolve saved-world loading.

The installer worker is bundled as installer/PS4PackageInstaller.exe with its runtime. test_installer.py covers synthetic encrypted PKG extraction, updates/DLC, cancellation, invalid paths, damaged input and overwrite protection. A synthetic PKG UI installation was also checked.

PKG reader provenance: https://github.com/aderumier/shadps4_pkg_to_zar revision 1cf25ce48aab8bb1de569b1448ca0854684a809c; local ps4_pkg.py and pkg_keys.py. Preserve bundled licenses when distributing. See ../NATIVE_DX12_PORT_STATUS.md for renderer status.
