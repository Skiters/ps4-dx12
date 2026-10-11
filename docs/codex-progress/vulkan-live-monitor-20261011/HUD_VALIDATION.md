# Real acceptance record

All game observations below concern the installed executable with SHA256
7d350796bdd27515f85951226fed73a7167487f0eb5b590e1d0cfb2504eab773.
Desktop UI observations were explicit acceptance tests, not passive recording
features. The sensor fixture is separate and supplies no Bloodborne FPS result.

| Check | Actual outcome |
|---|---|
| A: existing shortcut launches original core | Multiple real launches; original ShellExecute backup and args preserved |
| B: DLSS/FG continuity | Actual world render, loaded release-matching libraries, changing original integration counter; independent frame rate unmeasured |
| C–F: right HUD/time/CPU/GPU/VRAM | Observed above borderless game, timer/sample counts advance; actual memory and engine values change |
| G: overhead | Process resource cost measured; on/off game FPS impact NOT MEASURED |
| H: input interference | Virtual XInput menu, walking and camera work while HUD records; no-activate/click-through style set; full keyboard/mouse camera test not claimed |
| I: focus | HUD hides for unrelated foreground apps; explicit controls take focus; XInput refuses input there |
| J: geometry | F11 windowed then borderless return observed, HUD realigns after timer; width/expanded controls tested. Multi-monitor/DPI and exclusive fullscreen NOT TESTED |
| K: close | PID 27008 and 18880 exit nonzero 0xC0000409, cause unknown; PID 39216 exits with code 0. Collector/HUD finalize; the 18880 collector's provisional 0 is retained alongside final supervisor code, and wait-signaled exit handling is added |
| L: export | Actual panel snapshot/export creates separate report/JSON/CSV while recording continues |
| M/N: restart | New segment on recording Start and new identity on game relaunch; old directories retained |
| O: collector failure | Initial sharing error stopped collector without closing the game; traceback and partial report retained; retry/read-sharing repair validated |
| P: HUD failure | Original HUD 13452 terminated intentionally; collector 37964 stayed alive and samples advanced 188→204; supervisor restarted HUD 36484 |
| Q: duplicate shortcut | Actual second invocation: no second game/core/collector |
| R: rollback | Copy of wrapped shortcut restored and SHA matched original f31bfc7f22e510d46dbc68eced4e100ac2033bd6495c168f04e4ae67a72eca97 |

Control buttons Start, Stop, Pause/resume, markers, anomaly snapshot and export
were operated through their real UI. The explicit panel's first version clipped
controls under system DPI; pixel fonts and fixed AutoScaleMode corrected it.
The final panel is TopMost when explicitly opened, while the passive HUD itself
does not activate. Hotkeys use Home/Insert/PageUp to avoid the emulator's F9/F10/
F11 actions. Final registrations succeeded; hide/show changed HUD visibility,
and opening controls delivered a hotkey message and created the panel. Window
inventory updates lag briefly, so an immediate empty listing is not failure.

Controller acceptance: virtual XInput slot 1 initially failed to address player
1 while the physical pad was assigned. After the user switched it off, normal
relaunch plus virtual slot 0 entered the game. A/Start/B, left-stick walking and
right-stick view change were observed. Each command and neutral release was read
back by XInput. A foreground refusal and duplicate-feeder refusal were retained;
explicit Stop removed only the virtual device while the game stayed running.
Sending RB was verified electrically but full combat correctness was not tested.

Final wait-signaled lifecycle check on PID 7416 retained 113 samples, zero skips
and identical collector/supervisor exit code 0. The final feeder removed its
virtual device successfully on game exit; game, collector, HUD and supervisor
were no longer running. The earlier exit disagreement remains explicitly present
in the derived report and SESSION_MANIFEST.json.

The final collector stores source copies, individual GPU counters and a persistent
last candidate. Window snapshots have a 30s cooldown, at most 100 events and four
pending windows. Expanded HUD shows Copy/Compute separately; unavailable Compute
is N/A. Collection error and stale samples are not replaced by invented zeroes.

Limits: these runs include menus, loading, idle world and brief commanded input.
They do not establish long-session stability under arbitrary gameplay, guest FPS,
latency, hard faults, CPU stacks, causal cache costs or DX12 parity. Missing data
and the known early nonzero exit are part of acceptance, not hidden exclusions.
