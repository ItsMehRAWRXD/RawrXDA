# RawrXDA → local IDE integration

Remote: `rawrxda` → `https://github.com/ItsMehRAWRXD/RawrXDA.git`

## Local mirror branches (same commit as `rawrxda/*`)

After `git fetch rawrxda --prune`, these local branches track the agent lines without a “separate workspace”:

| Local branch | Tracks |
|--------------|--------|
| `rawrxda-mirror-main` | `rawrxda/main` |
| `rawrxda-mirror-backup-20260316-ide` | `rawrxda/backup/20260316-ide` |
| `rawrxda-mirror-cursor-2b27` | `rawrxda/cursor/unlinked-dissolved-symbols-2b27` |
| `rawrxda-mirror-cursor-4264` | `rawrxda/cursor/unlinked-dissolved-symbols-4264` |
| `rawrxda-mirror-cursor-2559` | `rawrxda/cursor/unlinked-and-dissolved-symbols-2559` |
| `rawrxda-mirror-cursor-841f` | `rawrxda/cursor/unlinked-and-dissolved-symbols-841f` |
| `rawrxda-mirror-cursor-fcc6` | `rawrxda/cursor/full-historic-source-dumps-fcc6` |

Refresh mirrors:

```powershell
git fetch rawrxda --prune
git branch -f rawrxda-mirror-cursor-2b27 rawrxda/cursor/unlinked-dissolved-symbols-2b27
# … repeat for other refs as needed
```

## Merged into `backup/20260316-ide` (this repo)

These merges bring agent work **into the same branch as the IDE** (not a side repo):

1. `merge(rawrxda): cursor/full-historic-source-dumps-fcc6` — MinGW duplicate-definition exclusions in `CMakeLists.txt`.
2. `merge(rawrxda): cursor/unlinked-and-dissolved-symbols-2559` — `runtime_symbol_bridge.cpp`, `minigw_runtime_symbol_batch7.cpp`.
3. `merge(rawrxda): cursor/unlinked-dissolved-symbols-4264` — stricter non-MSVC fallback exclusions (combined with existing `multi_response_engine.cpp` line).
4. `merge(rawrxda): cursor/unlinked-dissolved-symbols-2b27` — expanded `rawr_engine_link_shims.cpp` (with MSVC/`win32ide_asm_fallback.cpp` dedup and `find_pattern_asm` ABI for `byte_level_hotpatcher`).

Branches that were **already contained** in local `HEAD` at merge time (0 commits to pull): `rawrxda/main`, `rawrxda/backup/20260316-ide`, `rawrxda/cursor/unlinked-and-dissolved-symbols-841f`.

## `rawr_engine_link_shims.cpp` + MSVC

`win32ide_asm_fallback.cpp` and the shims both define many `extern "C"` symbols. On **MSVC**, overlapping bodies are wrapped in `#if !defined(_MSC_VER)` in the shims so the fallback TU remains the single provider for those symbols; **MinGW** (fallback often excluded in CMake) keeps the full shim implementations.

## Ongoing workflow

```powershell
git fetch rawrxda --prune
git log --oneline HEAD..rawrxda/cursor/unlinked-dissolved-symbols-2b27
# then merge or cherry-pick as needed
```
