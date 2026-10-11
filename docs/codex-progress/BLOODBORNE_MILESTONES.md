# Bloodborne milestones

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
