# RawrXD IDE — Startup Module Audit

Audit of IDE startup sequence so the window is shown and all modules are fully loaded (or explicitly deferred). Sequence is **dynamic** (from `config/startup_phases.txt`) and **lazy** phases run on first use.

## Entry point

- **Before**: `src/ui/entry_point.cpp` (wWinMain) created a minimal window and called `ShowWindow(hwnd, nCmdShow)`. If the process was started with `SW_HIDE` (e.g. by a launcher), the IDE stayed hidden.
- **After**: `src/win32app/main_win32.cpp` (WinMain) is the Win32 IDE entry. Full flow: crash containment → DPI → first-run gauntlet → VSIX/plugin signature → **createWindow** → **showWindow** (with forced visibility) → layout → message loop.

## Visibility fix (no minimize / disappear)

- **showWindow()** (Win32IDE_Core): If `IsIconic`, call `ShowWindow(SW_RESTORE)`. Always `ShowWindow(SW_SHOWNORMAL)`, then `GetWindowPlacement` / `SetWindowPlacement` so `showCmd` is normal. Then `SetWindowPos(..., SWP_SHOWWINDOW)`, `BringWindowToTop`, `SetForegroundWindow`, `SetActiveWindow`, `FlashWindowEx`.
- **ensureMainWindowVisible()** (main_win32): After show and after layout, if the main window is iconic or not visible, restore it and bring to front. Prevents the window from opening minimized or disappearing when the shortcut uses "Run minimized".

## Startup trace (ide_startup.log)

| Step | Status | Notes |
|------|--------|--------|
| WinMain start | ✅ | Entry |
| crash_containment_installed | ✅ | Cathedral boundary guard |
| dpi_awareness | ✅ | Per-Monitor V2 |
| init_common_controls | ✅ | ComCtl32 |
| first_run_gauntlet_start / first_run_gauntlet_done | ✅ | Final gauntlet |
| vsix_loader | ✅ | VSIX loader init |
| plugin_signature | ✅ | Plugin signature verifier |
| creating_ide_instance | ✅ | Win32IDE ctor |
| createWindow_start / createWindow_ok | ✅ | Main window created (WS_VISIBLE + SW_SHOWNORMAL in Core) |
| enterprise_license_done | ✅ **Phase-in** | Was skipped; now runs `EnterpriseLicense::Instance().Initialize()` |
| showWindow | ✅ | ide.showWindow() + force visible if hidden |
| camellia_init_done | ✅ **Phase-in** | Was skipped; now runs `Camellia256Bridge::instance().initialize()` |
| masm_init_deferred | ⏸️ Deferred | MASM + RE kernel intentionally deferred to avoid AV in Vulkan/GGUF path. Re-enable from menu or WM_APP+150. |
| swarm_done | ✅ **Phase-in** | Was skipped; now runs `SwarmReconciler::instance().initialize(localNodeId, 0)` |
| auto_update_done | ✅ | Async update check |
| layout_start / layout_done | ✅ | WM_SIZE + InvalidateRect |
| message_loop_entered | ✅ | Before runMessageLoop() |

## Modules fully loaded

- **Crash containment** — installed first.
- **DPI awareness** — before any window.
- **Common controls** — init before UI.
- **First-run gauntlet** — final gauntlet verification.
- **VSIX loader** — initialized.
- **Plugin signature** — verifier initialized.
- **Win32IDE** — createWindow (Core: WS_VISIBLE, SW_SHOWNORMAL), showWindow, layout.
- **Enterprise license** — Initialize() called; g_800B_Unlocked and feature gates set.
- **Camellia-256** — engine initialized (HWID-derived key). Workspace encrypt optional/background.
- **Swarm reconciler** — initialized with local node id; DAG/vector clock ready.
- **Auto-update** — async check started.
- **Layout** — WM_SIZE + InvalidateRect on main window.

## Deferred (by design)

- **MASM + RE kernel** — not run at startup to avoid AV in Vulkan/GGUF path. Can be enabled from menu or WM_APP+150 when needed. Shutdown still runs (asm_orchestrator_shutdown, asm_spengine_shutdown, etc.).

## Phase-in summary

1. **Entry point**: Switched Win32 IDE entry from `entry_point.cpp` to `main_win32.cpp` so the full IDE runs and the window is shown.
2. **Visibility**: After showWindow, force main window visible if it was hidden.
3. **Enterprise license**: Now initialized at startup (was “enterprise_license_skipped”).
4. **Camellia-256**: Engine init at startup (was “camellia_skipped”).
5. **Swarm**: Reconciler init at startup (was “swarm_skipped”).
6. **MASM init**: Left deferred; documented in this audit.

## How to verify

1. Build RawrXD-Win32IDE with current CMake (entry = main_win32.cpp).
2. Run the IDE; confirm the main window appears.
3. Check `ide_startup.log` in the exe directory for the trace above (enterprise_license_done, camellia_init_done, swarm_done, masm_init_deferred).

## Dynamic, lazy startup (no hardcoded sequence)

- **Phase order** is loaded from `config/startup_phases.txt` (one phase name per line; `#` comments; `lazy:name` = run on first use only). If the file is missing, a built-in default order is used.
- **Registry**: `include/startup_phase_registry.h`, `src/core/startup_phase_registry.cpp`. `getPhaseOrder()` returns the ordered list; `isPhaseLazy(name)` and `runPhaseLazy(name)` support lazy phases.
- **main_win32** runs `for (name : getPhaseOrder()) { if (!isPhaseLazy(name)) runPhase(name, ide, ...); }`. No hardcoded sequence in code.
- **Lazy example**: `masm_init` is in the config as `lazy:masm_init`; it is skipped at boot and can be run later via `RawrXD::Startup::runPhaseLazy("masm_init")`.

---

*Last updated: 2026-03-06 — Startup module audit, phase-in, dynamic order, visibility fix.*
