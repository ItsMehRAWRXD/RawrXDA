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

Refresh all mirrors:

```powershell
git fetch rawrxda --prune
git branch -f rawrxda-mirror-main rawrxda/main
git branch -f rawrxda-mirror-backup-20260316-ide rawrxda/backup/20260316-ide
git branch -f rawrxda-mirror-cursor-2b27 rawrxda/cursor/unlinked-dissolved-symbols-2b27
git branch -f rawrxda-mirror-cursor-4264 rawrxda/cursor/unlinked-dissolved-symbols-4264
git branch -f rawrxda-mirror-cursor-2559 rawrxda/cursor/unlinked-and-dissolved-symbols-2559
git branch -f rawrxda-mirror-cursor-841f rawrxda/cursor/unlinked-and-dissolved-symbols-841f
git branch -f rawrxda-mirror-cursor-fcc6 rawrxda/cursor/full-historic-source-dumps-fcc6
```

## Merged into `backup/20260316-ide` (this repo)

Agent lines are merged **into the same branch as the IDE** (repeat `git fetch` + merge when remotes move):

| Wave | Remote branch | Highlights |
|------|----------------|------------|
| 1 | `cursor/full-historic-source-dumps-fcc6` | MinGW duplicate-definition exclusions (`CMakeLists.txt`). |
| 1 | `cursor/unlinked-and-dissolved-symbols-2559` | `runtime_symbol_bridge.cpp`, `minigw_runtime_symbol_batch7.cpp`. |
| 1 | `cursor/unlinked-dissolved-symbols-4264` | Non-MSVC fallback exclusions + `multi_response_engine.cpp` kept. |
| 1 | `cursor/unlinked-dissolved-symbols-2b27` | `rawr_engine_link_shims.cpp` expansion; MSVC dedup vs `win32ide_asm_fallback.cpp`; `find_pattern_asm` ABI. |
| 2 | `cursor/unlinked-and-dissolved-symbols-841f` | RawrEngine CMake / SSOT handler defaults (resolved duplicate `rawr_engine_link_shims` list entry). |
| 2 | `cursor/unlinked-dissolved-symbols-4264` | Batches 17–22: stricter stub detection (`EnforceNoStubs`), inference engine source hygiene, audit markdown batches 17–22. |
| 2 | `cursor/unlinked-and-dissolved-symbols-2559` | Large `minigw_runtime_symbol_batch7.cpp`, `multi_response_engine_runtime_ctor`, Win32 debug/vision/OS explorer tweaks. |
| 2 | `cursor/unlinked-dissolved-symbols-2b27` | Additional mesh / hwsynth / spengine / speciator / neural / omega shim batches in `rawr_engine_link_shims.cpp`. |

After a full sync, `git merge-base --is-ancestor rawrxda/cursor/unlinked-dissolved-symbols-2b27 HEAD` should exit **0** (nothing left to pull on that branch until the next push).

## `rawr_engine_link_shims.cpp` + MSVC

`win32ide_asm_fallback.cpp` and the shims both define many `extern "C"` symbols. On **MSVC**, overlapping bodies stay under `#if !defined(_MSC_VER)` in the shims so the fallback TU remains the single provider for those symbols; **MinGW** (fallback often excluded in CMake) keeps the full shim implementations. **`find_pattern_asm`** must remain the `const void*` / `size_t` form expected by `byte_level_hotpatcher`.

## Ongoing workflow

```powershell
git fetch rawrxda --prune
git log --oneline HEAD..rawrxda/cursor/unlinked-dissolved-symbols-2b27
git merge --no-ff rawrxda/cursor/unlinked-dissolved-symbols-2b27 -m "merge(rawrxda): cursor/unlinked-dissolved-symbols-2b27 (...)"
```

If CMake conflicts, prefer **keeping** a single `rawr_engine_link_shims.cpp` entry for RawrEngine (conditional `target_sources` already gates it) and **merging** stricter `EnforceNoStubs` patterns from 4264.

**Win32IDE:** use `src/core/rawr_engine_link_shims.cpp` for link closure (and production-strip `REMOVE_ITEM` of that TU when stripping stubs). Do **not** reference `win32_ide_link_stubs.cpp` — it is not in the tree; stale list entries break consistency with SSOT merges.

---

## 2026-03-21 — sequential merge into `main`

On branch `backup/20260316-ide`, each RawrXDA agent line was merged **one at a time** (inspect → resolve → commit), then `main` was **fast-forwarded** to that tip:

| Order | Remote merged | Notes |
|------|----------------|--------|
| 1 | `rawrxda/cursor/unlinked-and-dissolved-symbols-841f` | CMake conflicts: took incoming `CMakeLists.txt` then re-added `unified_memory_executor.cpp` (RawrEngine + Win32IDE lists). |
| 2 | `rawrxda/cursor/unlinked-and-dissolved-symbols-2559` | `agentic_task_graph.cpp` + `rawrxd_telemetry_exports.h` small conflicts. |
| 3 | `rawrxda/cursor/unlinked-dissolved-symbols-2b27` | Took incoming for 8 fallback/bridge TUs. |
| 4 | `rawrxda/cursor/unlinked-dissolved-symbols-4264` | CMake: same pattern; `audit/PRODUCTION_SYMBOL_WIRING_BATCH_23.md` merged both narratives. |
| 5 | `rawrxda/cursor/full-historic-source-dumps-fcc6` | Took incoming for 12 fallback TUs. |

`rawrxda/backup/20260316-ide` was **already an ancestor** of local `backup/20260316-ide` — no separate merge.

**Local branches removed** after they were contained in `main` (or force-deleted when Git refused `-d` due to upstream tracking): includes `backup/20260316-ide`, `rawrxda-mirror-*`, `2026-02-12-nsh4`, `pr-q4-avx2-forward-wire`, `preserve-untracked-output`, `production-lazy-init`, `recovery-clean-source`, `codex/*`, `combined/all-updates`, `integrate/rawrxda-all-symbols`, etc.

**Re-create mirror pointers** (optional):

```powershell
git fetch rawrxda --prune
git branch -f rawrxda-mirror-main rawrxda/main
# …same table as above for cursor/* and backup…
```

### Branches left locally (not merged into `main`)

These tips have **unrelated history** vs current `main` (`fatal: refusing to merge unrelated histories`). They were **not** merged and **not** deleted (would drop parallel work):

- `agentic-ide-production`, `avx2-q4-v0.3.0`, `avx2-q4-v0.3.0-clean`, `clean-main`
- `copilot/merge-repos-into-main`, `copilot/sub-pr-5`
- `cursor/configure-vscode-github-copilot-for-powershell-gpt-5.1-codex-5162`
- `dependabot/pip/pip-e1fbd7af7b`
- `feature/pure-masm-ide-integration`, `feature/q4-avx2-dispatch`, `feature/q4-avx2-forward-wire`, `feature/q4-avx2-q4-dispatch-wire`, `feature/q8-avx2-dispatch`, `feature/q8-avx2-v2`
- `lazy-production-init`, `pr-q4-avx2-dispatch`, `sync-source-20260114`

To fold one in anyway: `git merge <branch> --allow-unrelated-histories` (expect mass conflicts) or cherry-pick specific commits.
