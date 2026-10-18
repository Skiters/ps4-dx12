# Integrated Release checkpoint — 2026-10-17

## Current writable packed10/common-word checkpoint - 2026-10-18

Production source **696ae216e17148b0eaccfa86577b0c843b74dee7**, fix/mixed-cmask. Full port **ACTIVE / INCOMPLETE**;129 separate implementation/behavior records and8 broad production work items remain. This FIRST section supersedes older current-state sections. H=**C:/Users/skei/Documents/Codex/handoff/writable-word-20261018**; exact evidence H/VERIFICATION_FINAL.json.

**Confirmed new failure and implementation:** original `_32BIT` compatible image storage -> NativeImageStorageAliases / ResourcePatchingPass / BuildGraphics pipeline view hints / CompatibleGuestWritableStorage / Entry::Uav / ExecuteGuestResources now agree on raw4/9/10 words. The old GPU case rejected different storage layouts; original/native classification separately showed the aliased read incorrectly left readonly beside the writer. Signed AND unsigned9 codecs and cross-stage hints select matching raw declarations. Compatibility normalizes ONLY local comparison copies/flags, preserving actual descriptors, guest geometry, permissions, image class/history, ordering and lifetime. Actual shared word UAVs keep existing barriers and codecs. Typed/expanded/packed16, depth/RT, changed shape and readonly permission negatives remain. No assertion suppression, fake operation, draw skip or game/address/hash workaround.

**Executed narrow proof:** exact native17 tests PASS59.83s:15 real GPU read-before/changed-store/ImageMemory barrier/read-after modes (4 signed/unsigned9<-R32,3 R32<-9,4 packed9<-RGBA8,4 RGBA8<-9),3 mip/layer passes with slot order changes and full physical guest/output/padding; plus geometry/permission/representation negatives and original-profile/native resource classification, including external-stage hints. Exact GCN EXE **c2b04c67ba64a4daf86829b6af399ac8eb3b3c5b0285efcba1d22edacd0c4b03** passes8 actual PM4/Liverpool GCN signed9/R32 cases4.944s: real T# table loads/image_store/S_BARRIER, independent BEFORE(+511,-511,0,-1)/AFTER(-512,0,511,+1) endpoint constants, both descriptor orders, repeated shader-specialization reuse and whole65536 bytes. Same exact GCN EXE separately passes20 direct mutable32 cases4.406s and8 signed readonly regression cases4.398s.10 related memory tests PASS46.57s (envelope/residency/DMA/PageWatch strong ownership/fence/scheduler/allocation-view/independent-image lifetime). Full identities/PDB/runtimes/source snapshots/LastTest and negatives retained; canonical-LF source equality to committed production verified. GCN image allocations reset each case: this is shader warm reuse, NOT a new claim about retaining an older native I UID/class/history. Whole-group/full game/FPS remains unverified.

**Clean committed Release:** H/artifacts-696ae21,18 files,54 pinned dependencies,2283 compile inputs/ZERO primary-source paths; build28.093s. EXE **54100342d87c1f1944752282b523e600166ba86817c1395acdc0e45fe29bfc62**; PDB **9caf18a9f9a64fd346851b089db284adedb21d0a39cfa024e355c687f4cd46ae**. Exact rebuilt core runtime/game/FPS NOT_RUN; narrow fixture results remain separate. Manual folder only; no app install/select, game/UI/XInput/profiler/user-process/original Vulkan/save action. Unknown .serena/ and CLAUDE.md retained. Source patches/new-header snapshots retained at H. First GCN archive omitted its new header; final separately archived run contains it and is authoritative.

**Next actual mechanism:** H/NEXT_CACHED_WORD_ALIAS.md. Extend the existing GCN writable-word case with a preceding REAL R32 image_load/image_store to create cached I with original GpuModified0/1 before the mixed writable bind. Check image class/history/ownership/native allocation and full readback, then implement only the concrete failing transition in MatchesGuestUploadStorage / CompatibleGuestShaderStorage / HasOwnedShaderStorage / source pins as required. Current raw-word comparison intentionally requires both UsesRaw32; cached ordinary R32 class may lack that flag despite exact R32 bits. This transition is source-identified, NOT yet reproduced or fixed. A sampled-only cached resource still cannot invent UAV permission; allocate/transfer with correct lifetime if required. Preserve exact-base invalidation, PS-before-VS/buffers-before-images and separate I/B publication. Continue existing converted/multiview/shifted/MS/large alias queue and broad8 gaps; no129 re-audit, old d019 crash replay or unchanged full numeric matrix. Narrow compile/GPU checks permitted; full suite and minimal existing-XInput final image/input/stability/completed guest FPS excluding FG versus saved Vulkan remain final. Normal verified push; manually selected versions.

Closest integration milestone reached: accumulated production sources are committed and a complete native Release core compiled and linked from a separate clean checkout. This is build reproducibility from committed inputs, not bit-identical binary reproducibility or game acceptance. Full-port development remains active.

## Preserved original state

`F:/PS4Dev/handoff/integrated-release-20261016/initial-snapshot/` contains exact staged/unstaged patches, source-files.zip (326 files), per-file hashes, status and restore instructions. Archive SHA256: `c2ef80163896fac2c402b94717611d64ab2bf27b19b903cee594a4a71689e699`. Restore only into a separate checkout at the manifest HEAD; never overlay current work.

All 22 originally staged deletions were reviewed: the obsolete flat network/NP files are replaced by the pinned upstream network/NP layout and corresponding root CMake source lists. The removed guest_depth_metadata.h was an obsolete standalone map; live HTILE state is carried by GuestTextureCache::Entry::htile_initial_mask. No source was discarded. Personal `.serena/` cache remains outside commits.

## Source commits and contents

| Commit | Actual contents |
|---|---|
| b9edd857f5f4e628a8ef7c4e75ed1d5874957432 | 141 pinned core/CMake sources plus 22 approved obsolete-file deletions |
| b43c3c0c2cbb70d7b14778b7640cdc76c476f94c | Four desktop integration files and obsolete depth metadata header deletion |
| ea2ec8dfe361a3ec5049e85bb20044b0611465b6 | 144 native production/build/source files, including guest_bc_decoder.h/.cpp, guest_tile_address.h, HLSL, generators and vendor source headers/licenses |
| 94ba0b5f1df97a6109521b79dd4412e5be6d5389 | 37 prepared fixture/reference-tool sources; not executed |
| 5a0d4f3 | Rename prefix local conflicting with Windows SDK RPC small macro; incremental Release passed |
| 72569a5 | Lock dependency revisions and LF byte identity for hashed Mesa inputs; clean Release passed |

Three middle commit subjects were paired with the wrong manifest group during scripting; the contents above and Git trees are authoritative. No history was rewritten.

## Build identity

Clean source checkout: `F:/PS4Dev/work/shadps4-dx12-clean-release-20261016`.
Built commit: `72569a531e1c262fc828e80d56400f7f1507d75a`.
Build: `build-release`, Release, LLVM clang-cl/lld-link 22.1.3, VS18 MSVC environment, Windows SDK10.0.26100.0, Ninja, jobs12 (Mesa uses its pinned jobs20).
EXE SHA256: `ddf89b38fada81c5865d1dfd1d5cb13b052f855ae261ad3804d2da4114a4955a`.
PDB SHA256: `a819b053abe680451ea52a78f2cc0fa38ccb759a30a56b278442a7adebf80ffa`.
Preserved executable/PDB/runtime/compiler/licenses: `F:/PS4Dev/handoff/integrated-release-20261016/artifacts-72569a5/`.
Complete binary hashes and input/log paths: `CLEAN_RELEASE_IDENTITY.json` in the evidence root.

Compile database was checked: zero references to the primary dirty checkout. External sources are the clean pinned e0a46675 tree; 54 recursive submodule SHAs/clean tracked contents were checked, including Mesa b628375f. Clean build fetched AGS by exact commit into its own _deps. DXC hashes are enforced by Native-DXC.cmake. No primary build/_deps or local uncommitted sources participate.

The clean build exposed CRLF checkout conversion of byte-hashed Mesa inputs. `.gitattributes` now pins those two inputs to LF, preserving the manifest checks. The existing clean checkout's two files were re-materialized exclusively from its committed Git blobs, and the refreshed index matches HEAD exactly. A new checkout receives LF automatically.

The first complete compile/link log is `F:/Codex-Shared/devtools/logs/command-20261017-000805-25176.log`; the log wrapper failed to print Unicode under CP1251 after compilation. A UTF-8 incremental verification completed with original exit0: `command-20261017-001502-67760.log`. No compiler error was hidden.

## Repeat build

Use `tools/porting/build-release.ps1` from a committed clean checkout, with explicit pinned tool/dependency paths. For this host:

```powershell
./tools/porting/build-release.ps1 -ExternalsRoot F:/PS4Dev/work/shadps4-original-e0a46675/externals -DxcRoot F:/PS4Dev/work/dxc-sdk -LlvmRoot F:/PS4Dev/toolchain/llvm-22.1.3 -VsDevCmd 'C:/Program Files/Microsoft Visual Studio/18/Community/Common7/Tools/VsDevCmd.bat' -BuildDir F:/PS4Dev/work/shadps4-dx12-clean-release-20261016/build-release
```

The script refuses tracked source/dependency dirt, checks exact dependency SHAs, builds only shadps4 and writes release-identity.json. It does not run tests, games or UI. Meson/Python/Ninja dependencies use existing native Mesa CMake discovery. Dependency repositories/SDKs are required inputs; they are not silently bundled as arbitrary local files.

## Implementation versus acceptance

See IMPLEMENTATION_REGISTRY.md and tools/porting/contracts.json: all129 entries separately record connected implementation, concrete known production changes and behavior verification. Nine shared production gaps remain recorded across58 groups;71 groups have no specific remaining production change established by retained evidence. This does not prove exhaustive parity. Prepared checks are listed separately. Broad historic acceptance notes remain historical, not missing-code counts.

Pinned on-chip geometry/stream output skips and excluded depth feedback are not expanded into new implementation requirements. Real native color feedback, formatted alias ordering, depth/color ownership, signed packed attachment quantization, packed19 MSAA attachment support, polygon point fill, old-device mixed blend constants, non-AMD/NVIDIA clock and oversized tessellation point capture remain explicit.

No final suite, game, XInput, profiling or UI was run. No new FPS/image/control/stability claim. Installed preview24, original Vulkan and saves are unchanged. Tests/game remain deferred until the full agreed implementation boundary; compilation/linking remain authorized now.

## Immediate continuation

Finish verifying the committed build script in the clean checkout and persist its identity. Continue the retained production target: GuestTextureCache::UploadVersion / GuestBcDecoder expanded BC1/2/3/7 UNORM/sRGB shared typed sampling, without reinterpreting BC4/5/6H signed float decodes. Preserve compressed snapshots, CPU PageWatch, ownership and fences. Then work through concrete production gap IDs; do not repeat completed architectural audits or confuse unrun fixtures with absent code.

## Latest continuation build

Production BC continuation commit `6cbaef71857b04275be11013e62c05c2b3c672b3` passed the committed build script from the same clean checkout (exit0, `command-20261017-004316-56776.log`). No primary-checkout inputs in 2282 compile commands; tracked source tree clean.

Preserved complete executable/PDB/runtime/compiler/licenses: `F:/PS4Dev/handoff/integrated-release-20261016/artifacts-6cbaef7`. EXE SHA256 `7751a7b565c94920c3ac15271dba0088308506a1a1717406aa8c1eb2aaf450c8`; PDB SHA256 `ee44494e2fa0dc1ba99d46401966dd365bae5670f332d9d7c964c7239b6132be`. Full manifest/proof: `LATEST_RELEASE_IDENTITY.json` / `LATEST_RELEASE_PROOF.json` in the evidence root. Initial72569a5 artifacts remain separately preserved. This build is untested in game and is not installed as an accepted application core.

## Ordered interior writer build

Clean Release `daa76fc50a3d07978a8804d7a6e3646dbe4c647a` passed the committed build script, log command-20261017-010800-71224.log. Artifacts `F:/PS4Dev/handoff/ordered-offset-writers-20261017/artifacts-daa76fc`, EXE SHA256 `74e783a09fd2e3ed5f7695235acf4eefa99bd515f7e9d4d7d9b21abcb599b9b3`. Full proof/identity in ordered-offset-writers-20261017. No primary checkout inputs. Native fixture C++ typechecks passed separately; no GPU/game acceptance. New manual-folder policy forbids automatic core installation/selection; use the installed folder selector.
