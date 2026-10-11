# Existing shortcut integration

Habitual shortcut: `%USERPROFILE%/Desktop/Bloodborne - DLSS Quality + HDR + FG x4.lnk`.
Original target: `F:/Apps/shadPS4-DLSS-DLAA-1.8.4/shadPS4.exe`.
Original arguments: `--game "D:/Program Files (x86)/Steam/steamapps/common/CUSA03173/eboot.bin"`.
Original working directory: the same application directory. Window style 1,
original executable icon, description and empty hotkey retained. Link flags 247,
no RunAs. Portable profile: application `user` directory; no launcher substituted.

Original byte backup:
`F:/PS4Dev/handoff/vulkan-live-monitor/integration-20261011T132118Z/original.lnk`.
SHA256 `f31bfc7f22e510d46dbc68eced4e100ac2033bd6495c168f04e4ae67a72eca97`.
Full original properties and integration state are in local integration.json.

The existing shortcut now targets Windows wscript.exe / program/Launch.vbs.
It starts a hidden standalone Windows PowerShell supervisor. The supervisor
verifies executable SHA and invokes the **immutable original shortcut** through
ShellExecute, preserving its arguments/CWD/environment inheritance. The original
backup targets the game, so there is no recursive wrapper chain. Windows PS
modules are resolved explicitly because an inherited PS7 module path initially
prevented Get-FileHash loading; that failed launch is retained locally.

One named mutex prevents duplicate wrappers; the process check rejects a different
emulator. PID, creation time and executable SHA bind each collector. Each recording
segment gets a new UUID and directory. Start/Stop/Pause act on recording; the game
continues. Commands and markers preserve UTC/QPC/session/PID/start time.
HUD failure can restart twice; collection continues independently. Game exit
finalizes the collector/report and closes the HUD. Errors never kill the game.
Defaults: 1s system sampling, 8h duration and 1GiB sample-file cap; anomaly and
PresentMon files have separate bounded storage. A capped/stopped recording needs
an explicit Start command while the game continues.

## Controls

- Ctrl+Shift+Home: open the monitor controls.
- Ctrl+Shift+Insert: hide/show HUD.
- Ctrl+Shift+PageUp: add performance issue marker.
- Tray menu: the same commands and expanded HUD.
- Panel: Start, Stop, Pause/resume, gameplay/loading/combat/location/effect/issue/
  crash markers, anomaly snapshot, report export, recording folder, expanded HUD,
  right/top margins and width.

Program: `F:/PS4Dev/handoff/vulkan-live-monitor/program`.
Data: sibling unique session-* and snapshot-* directories. active-session.json
identifies the current segment; inspect advancing status and process identity.
The monitor is an actual local program and requires no continuing Codex chat.

## Restore

Run tools/vulkan_reference/Restore-LiveShortcut.ps1 with the local integration.json.
It validates the original backup SHA, copies it byte-for-byte to the habitual
shortcut and validates the result. Tested on a copy of the currently wrapped
shortcut: roundtrip matched the original SHA. The habitual shortcut remains
integrated. Restoring affects future launches; it does not close a running game.
Never rerun the installer blindly on an already wrapped shortcut: its direct-core
guard deliberately rejects that. Update components while collector/HUD are idle.
