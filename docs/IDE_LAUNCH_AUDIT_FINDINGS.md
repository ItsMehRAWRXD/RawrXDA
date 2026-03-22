# IDE launch audit — continuation (source-level)

Companion to `docs/PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md`. This file records **evidence-based** findings from tracing `main_win32.cpp` and related wiring.

## Fixed during this audit

| Issue | Evidence | Change |
|--------|-----------|--------|
| **`IntegratedRuntime::boot()` never ran on normal GUI startup** | `runPhase("integrated_runtime", …)` existed (~991) but `integrated_runtime` was **not** listed in `quickPhases` or `heavyPhases`; only `shutdown()` ran at exit (`s_bootInvoked` stayed false → coordinator never shut down via integrated path either). | Added **`integrated_runtime`** as the **first** `heavyPhases` entry (after `showWindow`), and to **`config/startup_phases.txt`** + **`startup_phase_registry.cpp`** default order after `showWindow`. |

## Launch path (GUI) — where it can stop

1. **`setCwdToExeDirectory()`** / `bootstrapRuntimeSurface()` — failure modes depend on implementations.
2. **CLI early exits** (no main window): `--test-deep-thinking`, feature-probe, autofix, **`--headless`**, **`--selftest`** (first block), **`--vsix-test`**, post-loop **`--selftest`**.
3. **`ide_startup.log`** — created in exe dir; if the directory is not writable, trace is disabled (pointer nulled); not fatal.
4. **Crash containment** — `RawrXD::Crash::Install` with `terminateAfterDump` + optional message box; can look like “won’t launch” if crash is immediate.
5. **Delay-load hook** — **`__pfnDliFailureHook2`** must point at `DelayLoadFailureHook` so **missing `vulkan-1.dll` or `D3DCOMPILER_47.dll`** yields a **`MessageBox`** + **`ExitProcess(1)`** instead of an immediate **AV** with no window. (If the hook is not registered, delay-load failure looks like “exe does nothing.”)
6. **Quick phases** — any `runPhase` returns **false** → `MessageBoxW` “Failed to initialize IDE” → **return 1**. The only hard failure in the quick list is typically **`createWindow`** (`ide.createWindow()` false).
7. **Heavy phases** — failures are **logged only** (`OutputDebugString`); **do not** abort startup (except if a phase were changed to return false — currently deferred phases return true).

## “Not connected” / misleading docs

- **`INTEGRATED_RUNTIME.md`** stated the integrated runtime ran in heavy phases after `showWindow`; the code **did not** include that phase until the fix above.
- **`layout` phase** — explicitly **SKIPPED** in `runPhase` (“avoid window close”); doc/comments may still imply full layout runs at boot.
- **`TranscendenceCoordinator::initializeAll()`** remains callable from **Transcendence panel / commands** even when integrated boot was previously absent; behavior depended on which path the user hit.

## Practical diagnostics

| Symptom | Check |
|--------|--------|
| Instant exit **code 1** | Delay-load dialog? Or quick phase failure (see `ide_startup.log` last line: `createWindow_FAILED`?). |
| **No log file** | CWD/permissions; run from writable folder. |
| Blank / no window | WebView2 / GPU (see `docs/VSCODE_BLANK_WINDOW_FIX.md`, `docs/WEBVIEW2_CHROMIUM_STDERR.md`). |
| Crash dump | `crash_dumps\`, crash containment dialog. |

## Entry point hygiene

- **CMake** lists **`src/win32app/main_win32.cpp`** as the Win32 IDE entry (single `WinMain`).
- Other files under `src/win32app/` containing `WinMain` are **not** linked into `RawrXD-Win32IDE` per `CMakeLists.txt` — watch for accidental addition (would cause **LNK2005**).

---

*Last updated: audit continuation + integrated_runtime wiring fix.*
