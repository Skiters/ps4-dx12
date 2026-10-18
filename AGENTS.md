# User preferences

- After completing and appropriately verifying task commits, automatically push
  them to the current branch's configured upstream. The user explicitly requested
  this persistent default on 2026-10-11. Later instructions to defer or skip push
  take precedence. This does not authorize force-push, rewriting remote history,
  or merging other branches.

# Bloodborne emulation research

- Storage policy (user correction, 2026-10-18): maintain ONE current DX12
  application core with matching PDB, runtime dependencies, source/build identity,
  SHA256 and verification status. Replace it safely after relevant checks; never
  replace/delete a core while a process uses it. Preserve the selected profile.
  `desktop/package-core.ps1` publishes the stable `dx12-current` package; do not
  create a versioned artifact directory after each test/commit. This supersedes
  older instructions to keep intermediate DX12 packages or require manual install.
- Reuse `build-migration-core`, `build-migration-checks` and the existing single
  clean Release checkout. Git retains source history. Reports retain exact
  commands, commit/dirty patch, EXE/PDB/runtime hashes, required inputs and results;
  do NOT copy full runtimes/EXE/PDB/MAP for successful intermediate tests. Runtime
  acceptance belongs only to the exact tested hash, never a rebuilt executable.
- Unresolved crashes may temporarily retain a minimal exact binary/PDB/runtime,
  input and exception/dump report. Retire that bundle only after diagnosis,
  regression acceptance and a reproducible case are saved. Preserve original
  shadPS4, games, all profiles/saves, toolchains/dependencies and unknown sources.
  Cleanup must validate absolute paths and skip reparse points, retain unique
  sources/diagnostics, and record actual free-space changes per disk. Never move
  all archives to another disk as a substitute for cleanup.

- For native DirectX renderer/core work, use
  `docs/codex-progress/vulkan-emulation-20261011/CORE_CONTRACT.md` as the saved
  model of guest execution, memory ownership, queues and resource reuse. Consult
  its linked evidence and current STATE/NEXT_SESSION before proposing a port.
- The user wants understanding of PS4 emulation mechanisms and subsequent
  production integration. Percentage-of-resource comparisons alone do not
  satisfy this objective. Vulkan understanding precedes further DX12 experiments.
- Separate installed `974f8c21-dirty` observations, public base-source analysis,
  and the different pinned Vulkan probe. DLSS/FG/Reflex run in the emulator's
  output path and do not establish native guest-engine integration or guest FPS.
- Use the existing XInput control workflow for authorized autonomous gameplay.
  Keep isolated save/profile copies, verify controller assignment and actual
  game reaction, and release the virtual device at the end. Existing authorization
  for the complete isolated engineering cycle remains in effect.
