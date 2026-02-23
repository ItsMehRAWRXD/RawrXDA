# IDE Launch Audit

This document describes how the RawrXD Win32 IDE starts, how to diagnose launch failures, and what was audited/fixed.

## Crash fix (0xC0000005 / ACCESS_VIOLATION)

The IDE was exiting with exit code **-1073741819 (0xC0000005)** before the message loop. Debugging (dumpbin, startup log, disabling background threads) showed the crash was in one of the **background threads** started from `main_win32.cpp`:

- Enterprise License init (`EnterpriseLicense::Initialize()`)
- Camellia-256 init
- MASM subsystems (SelfPatch, GGUF/Vulkan loader, LSP, Orchestrator, QuadBuffer, RE kernel)
- Swarm Reconciler init

**Current workaround:** Those inits are **skipped on startup** so the main thread reaches the message loop and the window stays up. License/Camellia/MASM/Swarm can be re-enabled later from a deferred message (e.g. WM_APP+200) once the message loop is running.

**To run with debug capture:** From `build_ide\bin` run `.\Run-IDE-Debug.ps1` to see exit code, stderr, and the last lines of `ide_startup.log`.

---

## Fixes applied (launch blocking removed)

Startup was blocking on several inits that could hang or crash. The following were **deferred to background threads** so the main window and message loop start immediately:

- **Enterprise License** — `EnterpriseLicense::Initialize()` (and 800B unlock) runs in a background thread.
- **Camellia-256** — `Camellia256Bridge::initialize()` and `encryptWorkspace` thread spawn run in a background thread.
- **MASM subsystems** — SelfPatch, GGUF Loader, LSP Bridge, Orchestrator, QuadBuffer, QuantHysteresis, and (if defined) **Reverse-Engineered kernel** init run in a background thread.
- **Swarm Reconciler** — init (which may call `EnterpriseLicense::GetHardwareHash()`) runs in a background thread.

**Layout:** `UpdateWindow(hwnd)` was removed from the startup path to avoid blocking on `WM_PAINT`. Layout is triggered via `PostMessage(WM_SIZE)` and will be processed when the message loop runs.

## How to run the IDE

- **Exe:** `build_ide\bin\RawrXD-Win32IDE.exe`
- **Working directory:** Run from `build_ide\bin` (or the exe sets CWD to its directory on startup).
- **Debug console:** Set env `RAWRXD_DEBUG_CONSOLE=1` to get a console window and stderr output.

## Startup trace (ide_startup.log)

On each launch, the IDE writes a **startup trace** to `ide_startup.log` in the exe directory (same folder as `RawrXD-Win32IDE.exe`). Each line is:

`<timestamp_ms> <step_name> [optional detail]`

If the IDE fails to launch or exits immediately, open `build_ide\bin\ide_startup.log` and see the **last step** that was written. That is the point after which the process crashed or exited.

### Trace steps (in order)

| Step | Meaning |
|------|--------|
| `WinMain start` | Entry; CWD already set to exe dir |
| `crash_containment_installed` | Crash handler (mini dump, rollback) installed |
| `dpi_awareness` | DPI awareness set |
| `init_common_controls` | ComCtl32 init (toolbar, tab, list, tree, etc.) |
| `vsix_loader` | VSIXLoader initialized for "plugins" |
| `enterprise_license` | License / defense shield init (non-fatal if fail) |
| `plugin_signature` | Plugin signature verifier init |
| `creating_ide_instance` | Win32IDE ctor done |
| `createWindow_start` | About to create main window and run WM_CREATE/onCreate |
| `createWindow_ok` | Main window created and shown |
| `showWindow` | showWindow() + BringWindowToTop / SetForegroundWindow |
| `masm_init_start` | About to init MASM subsystems (SelfPatch, GGUF, LSP, Orchestrator, QuadBuffer) |
| `masm_init_done` | MASM init finished |
| `re_kernel_done` | Reverse-engineered kernel init (if RAWRXD_LINK_REVERSE_ENGINEERED_ASM) done |
| `message_loop_entered` | About to run GetMessage loop; log closed |

If the log **stops at** e.g. `createWindow_start` and never shows `createWindow_ok`, the failure is inside **createWindow** (window class registration, CreateWindowEx, or **onCreate**). If it stops at `masm_init_start`, the failure is in one of the MASM inits, etc.

## Startup sequence (code path)

1. **main_win32.cpp WinMain**
   - Set CWD to exe dir.
   - Open `ide_startup.log`, write `WinMain start`.
   - Optional: if `RAWRXD_DEBUG_CONSOLE=1`, AllocConsole + freopen stdout/stderr.
   - Install crash containment (mini dump, patch rollback, quarantine).
   - DPI awareness.
   - If `--headless`: run HeadlessIDE and exit.
   - InitCommonControlsEx.
   - First-run gauntlet (optional; can be skipped via first_run.flag).
   - VSIXLoader init, Enterprise License init, Plugin Signature init.
   - Safe-mode flag: set `RAWRXD_SAFE_MODE=1` if `--safe-mode`.

2. **Create IDE window**
   - `Win32IDE ide(hInstance);`
   - `ide.createWindow()`:
     - LoadLibrary riched20.dll, msftedit.dll.
     - Register window class, then `CreateWindowExA` for main window.
     - **WM_CREATE** → **onCreate** (createMenuBar, createToolbar, createActivityBar, createPrimarySidebar, createTabBar, createBreadcrumbBar, createLineNumberGutter, createEditor, createTerminal, createStatusBar, createOutputTabs, createPowerShellPanel, createSecondarySidebar, initSyntaxColorizer, initGhostText, restoreSession, applyTheme).
     - ShowWindow(SW_SHOW), UpdateWindow, SetWindowPos.
   - If createWindow fails → MessageBox "Failed to initialize IDE", return 1.

3. **Post-create init (main thread)**
   - pumpMessages().
   - EngineManager, CodexUltimate, set on IDE.
   - RawrXDStateMmf (MMF init).
   - JSExtensionHost (QuickJS).
   - Plugin sandbox (QuickJS trust boundary).
   - ide.showWindow() (BringWindowToTop, SetForegroundWindow).
   - Camellia-256 init; encryptWorkspace in background thread.
   - MASM subsystems: asm_spengine_init, asm_gguf_loader_init, asm_lsp_bridge_init, asm_orchestrator_init, asm_quadbuf_init.
   - If RAWRXD_LINK_REVERSE_ENGINEERED_ASM: InitializeAllSubsystems (stubbed in Win32IDE via reverse_engineered_stubs.cpp).
   - Swarm reconciler, auto-update async check.
   - WM_SIZE to force layout, SetFocus(editor).
   - Write `message_loop_entered`, close log.
   - **ide.runMessageLoop()** (GetMessage loop).

4. **Exit**
   - WM_CLOSE → DestroyWindow → WM_DESTROY → PostQuitMessage(0) → message loop exits.
   - Cleanup (engine manager, codex, RE kernel shutdown, MASM shutdown, etc.), then return exitCode.

## Common failure points

- **No window appears**
  - Check `ide_startup.log`: last step before failure (e.g. createWindow_start → crash in onCreate, or createWindow_ok but crash before message_loop_entered).
  - If last step is `createWindow_start`: crash or exception inside **onCreate** (e.g. createMenuBar, createToolbar, …, createSecondarySidebar, or initSyntaxColorizer/initGhostText/restoreSession/applyTheme). Check DebugView or attach a debugger to see exception.
  - If window is created but not visible: try running from `build_ide\bin`, or check multiple monitors / off-screen position (main window is created at 50,50 with size 1600x1000).

- **"Failed to initialize IDE" message**
  - createWindow() returned false: either RegisterClassExA failed (class already exists is OK) or CreateWindowExA returned NULL. Check GetLastError() or add logging in Win32IDE_Core.cpp createWindow().

- **Process exits immediately with code 0**
  - Something triggered WM_QUIT (e.g. WM_CLOSE then DestroyWindow). Check whether a dialog or automation is sending close. ide_startup.log should show at least `message_loop_entered` if the loop was reached.

- **Process exits with code 1**
  - createWindow failed; see above.

- **Crash / unhandled exception**
  - Crash handler should create a mini dump in `crash_dumps\`. Use ide_startup.log to see how far startup got, then inspect the dump.

## Build

- **IDE only:** `cmake --build . --config Release --target RawrXD-Win32IDE` (from build_ide).
- **Full build:** `cmake --build . --config Release` (RawrXD_Gold may have link errors; IDE target is independent).

## Audit status

- Startup trace added in **main_win32.cpp** (ide_startup.log) at each critical step.
- Sequence documented above; failure points and how to use the log described.
- Window is shown with ShowWindow(SW_SHOW), UpdateWindow, SetWindowPos(SWP_SHOWWINDOW), then later showWindow() (BringWindowToTop, SetForegroundWindow).
- Reverse-engineered kernel: Win32IDE uses **reverse_engineered_stubs.cpp** so it does not require the full ASM kernel; InitializeAllSubsystems runs against stubs and is non-fatal.

If the IDE still does not launch, send the contents of `build_ide\bin\ide_startup.log` (and, if present, `crash_dumps\` minidump and any DebugView output) so the failing step can be identified.
