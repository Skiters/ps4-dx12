# Bloodborne milestones

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
