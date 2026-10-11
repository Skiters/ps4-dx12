# Bloodborne milestones

## Latest — confirmed fault fix, world acceptance still open

GXWORKER_PAGEWATCH.md records LEVEL 1 with original game context and a real
failing-before/passing-after regression. A partial fix reached the world and
responded to input but later crashed. The complete-fix precommit run remained
in a guest mutex loading wait. This does not establish LEVEL 3–7, correct pixels,
continuous 30-second updates or 60-second controlled gameplay. Consult external
`gxworker-20261011/final-verification.json` for the final exact-revision launch.

## Latest verification — 2026-10-11 saved-character loading

Forced baseline and fixed core both visually reached saved-world loading using copies
of the original DX12 profile. Baseline hit native reconciliation stack exhaustion;
fixed run hit a separate GXWorker:1 guest fault before a world frame. No new gameplay
milestone, stable world rendering, full-frame timing or FPS gain is claimed.
The recursion fix is proven by regression tests, not by passing the same game operation.
See FULL_FRAME_PROFILE.md. Older setup-profile observations remain separate below.

## Integration checkpoint 2026-10-11

2026-10-11: startup/boot/title online-offline choices reverified visually. Colored
artifacts prevent a correctness claim. Seed-home run reached brightness setup with
different home state, not a new gameplay milestone. Save selection/world/60-second
stable gameplay remain unverified. No new LEVEL 1–5 claimed. See GAMEPLAY_BASELINE.md.

2026-10-10: this audit cycle has not launched either game backend. Historical evidence is
listed separately so component tests cannot be mistaken for world-entry verification.

| Milestone | Evidence / status |
|---|---|
| M1 startup | Prior core runs; current core compiles, no new runtime startup evidence |
| M2 boot | Prior isolated-profile Bloodborne runs |
| M3 title | Historical README/status evidence, not revalidated this cycle |
| M4 menu | Historical README/status evidence, not revalidated this cycle |
| M5 save selection | No new captured evidence |
| M6 level loading | Historical status reports; not a gameplay frame |
| M7 correct gameplay frame | UNVERIFIED |
| M8 stable gameplay 60 seconds | UNVERIFIED |
| M9 stable gameplay 5 minutes | UNVERIFIED |
| M10 same-scene frame benchmark | UNVERIFIED; Present counts are insufficient |

Future comparison must clone the same profile, save and settings into independent test
directories. Original installed Vulkan application, configuration and saves remain untouched.

## 2026-10-11 — mixed CMASK implementation

Native mixed CMASK regression passes with exact tile readback. Bloodborne source/post runs remain below world entry: the known `Mv_AudioPlayThread` null write at guest offset `eboot+0x20b56d2` recurred. Colored setup artifacts remain. No new gameplay milestone is claimed.
