# Explicit virtual Xbox control

`xinput_control.py` feeds a separate virtual Xbox 360 pad and verifies every
report with `XInputGetState`. XInput itself reads controllers; `XInputSetState`
only controls vibration. Button/axis output uses the public ViGEm client ABI.
References: [Microsoft XInput](https://learn.microsoft.com/windows/win32/xinput/getting-started-with-xinput),
[ViGEmClient declarations](https://github.com/nefarius/ViGEmClient/blob/master/include/ViGEm/Client.h).

This machine already has Flydigi's compatible `hidvirtualdriver` bus and
`C:/Program Files/Flydigi Space Station/driver/flydigimapclient.dll`.
The installed Microsoft-signed driver reports version 16.19.18.884. Its client
DLL SHA256 is `b4af07797a2e3c8150a033a5c4425530a330036891d113cb78a22d065e8b4b5a`.
The client DLL is loaded only in the feeder. No driver installation, game DLL
injection, physical controller changes or profile edits are performed.
This is a verified local compatibility path, not a promise that any DLL with
these exports is interchangeable. On another machine identify a trusted matching
installed client/driver and pass its verified hash; do not bypass the guards.

## Invocation

Use the bundled interpreter recorded in local integration.json. The example
assumes the target PID has just been verified; substitute the current PID and
a **new** output directory. The game must already be running.

```powershell
$py = 'F:/PS4Dev/toolchain/python/cpython-3.11.16-windows-x86_64-none/python.exe'
$tool = 'F:/PS4Dev/handoff/vulkan-live-monitor/program/xinput_control.py'
$out = 'F:/PS4Dev/handoff/vulkan-live-monitor/xinput-new-session'
& $py $tool serve --root $out --target-pid <current-PID> `
  --expected-sha 7d350796bdd27515f85951226fed73a7167487f0eb5b590e1d0cfb2504eab773 `
  --client-dll 'C:/Program Files/Flydigi Space Station/driver/flydigimapclient.dll' `
  --expected-client-sha b4af07797a2e3c8150a033a5c4425530a330036891d113cb78a22d065e8b4b5a `
  --seconds 900
# From a second process, after observing ready status and focusing the game:
& $py $tool send --root $out --buttons a --hold-ms 150
& $py $tool send --root $out --ly 22000 --hold-ms 1200
& $py $tool send --root $out --rx 18000 --hold-ms 900
& $py $tool stop --root $out
```

The local agent runs serve hidden; no console overlay is required. It is never
started by the ordinary monitoring shortcut. Supported buttons are up/down/left/
right, start/back, ls/rs, lb/rb and a/b/x/y. Sticks lx/ly/rx/ry use signed 16-bit
values; triggers lt/rt use 0..255. Commands have 20..5000ms bounded holds.
No autonomous route, combat loop or unbounded held key is configured.

The feeder binds executable SHA, PID and creation time. A per-game named mutex
rejects another feeder. Foreground identity is checked before input and every
20ms during a hold. Stale commands/status are refused. Every command releases
to neutral and checks neutral readback. Stop, game exit, duration limit or error
remove only the newly created virtual device. Forced feeder termination cannot
run Python cleanup; the bus owns the disconnected client lifetime. Do not use
forced termination as the normal stop protocol.

Each run retains UTC/QPC action records, requested reports, XInput readback,
neutral readback, source bytes/hash and game identity. When available, its
manifest links the current telemetry segment. A successful driver readback is
separate from observing the game's reaction.

## Real Bloodborne validation, 2026-10-11

Initial virtual pad index 1 was readable but the physical Flydigi already owned
guest player slot 0. The exact base controller.cpp retains existing assignments
and opens new pads only for configured player slots. After the user switched
the physical pad off, the next normal shortcut launch logged zero initial pads,
then registered the virtual Xbox in guest slot 0; XInput index was also 0.
No SDL environment filter, Windows device disable or configuration edit was used.

PID 18880, creation FILETIME 134362011990702112, was controlled through this pad:
confirmed offline prompts, selected Continue, loaded Central Yharnam, moved the
character with left stick and rotated the view with right stick. Start opened
the inventory menu and B dismissed it. RB was sent and read back; a completed
attack/combat sequence was not separately validated. Visible game integration
counter examples were 54→216 and 52→209; these are not measured guest FPS.

Local `xinput-gameplay` retained neutral readbacks for the actions and one
foreground refusal when monitor controls were active. A duplicate serve failed
before device creation. Explicit stop removed the virtual USB Xbox device while
the game continued. `xinput-final` tests the final installed source separately.
The monitor/HUD continued recording throughout controller tests. The user can
turn the physical pad on for ordinary play; virtual control is an explicit test
tool, not the default input route.
