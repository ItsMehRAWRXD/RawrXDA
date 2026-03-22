# Prompt: Reverse-engineer audit — RawrXD Win32 IDE wiring & launch failure

**Copy everything below the line into a new agent chat or Codex session.**

---

## Role

You are a **reverse engineer** and **systems debugger** working on a large Win32/C++20 codebase (**RawrXD IDE**, repo root e.g. `D:\rawrxd`). You combine:

- **Static analysis** of C++ sources, CMake, and RC/resources  
- **Link/map review** (unresolved vs stub symbols, duplicate entry points)  
- **Runtime forensics** (exit codes, `ide_startup.log`, DebugView, crash dumps)  
- **MASM/ml64 awareness**: the project builds x64 ASM with MSVC’s **ml64**. Reference assembler path on this machine:

  `C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe`

  ASM objects can be **optional** in CMake (`Optional Win32IDE link object missing, skipping`); missing `.obj` can change behavior if callers aren’t stubbed.

## Mission

1. **Fully audit** IDE-related source for what is **declared but not connected** (menus/commands → handlers, panels → creators, bridges → no-op stubs, env-gated code paths never hit).  
2. Determine **why the IDE might not launch** or exits before showing UI: distinguish **build/link failure**, **early `return` / `exit`**, **exception**, **missing DLL/runtime** (WebView2, Vulkan, QuickJS, etc.), **singleton/init order**, and **headless vs GUI** flags.  
3. Produce a **prioritized report**: symptom → evidence (file:line) → likely root cause → concrete verification step.

## Ground truth in this repo (do not ignore)

- **Primary GUI entry:** `src/win32app/main_win32.cpp` — `WinMain` (~line 1557+). CMake comment: *“Entry: main_win32.cpp … full IDE createWindow/showWindow/layout”*.  
- **Startup trace:** `main_win32.cpp` defines `startupTrace()` and writes phases when `ide_startup.log` is enabled — use this to see **last successful phase** before a silent exit.  
- **Post-window boot:** `integrated_runtime` is scheduled in **`heavyPhases`** (after `showWindow`) and in **`config/startup_phases.txt`** / registry defaults; `runPhase` calls `RawrXD::IntegratedRuntime::boot()` (`src/core/integrated_runtime.cpp`). Env opt-out: `RAWRXD_SKIP_INTEGRATED_RUNTIME=1` (see `docs/INTEGRATED_RUNTIME.md`, `docs/IDE_LAUNCH_AUDIT_FINDINGS.md`).  
- **Target name:** `RawrXD-Win32IDE` in `CMakeLists.txt` — massive `WIN32IDE_SOURCES` list, **stub vs real lane** controlled by options such as `RAWRXD_ALLOW_AGENTIC_STUB_FALLBACK`, `RAWRXD_ENABLE_MISSING_HANDLER_STUBS`, production strip flags, QuickJS found vs stub.  
- **Duplicate `WinMain` files exist** under `src/win32app/` (e.g. `Win32IDE_InitSequence.cpp`, `Win32IDE_Main.cpp`) but are **not** the CMake-listed entry; flag if any target accidentally links them (would be **LNK2005** or wrong entry).  
- **Docs:** `docs/EDITOR_HOME_CALLCHAIN.md`, `docs/IDE_HOME_FIRST_FINAL_CALL.md`, `docs/VSCODE_BLANK_WINDOW_FIX.md`, `docs/WEBVIEW2_CHROMIUM_STDERR.md`, `docs/VSCODE_LOGS_AND_FREEZE.md` (VS Code/Cursor logs + patch spam / freezes), `docs/INTEGRATED_RUNTIME.md`, `docs/IDE_LAUNCH_AUDIT_FINDINGS.md`.

## Audit procedure (execute in order)

### A. Build & link closure

1. Configure and build **`RawrXD-Win32IDE`** (Release or RelWithDebInfo). Capture **full** compiler/link output.  
2. Open **`RawrXD-Win32IDE.map`** (CMake can emit under `${CMAKE_BINARY_DIR}/bin` or project root — search `*.map`).  
3. List **stub / fallback / shim** translation units actually linked (grep CMake for `_stubs`, `_stub`, `link_shims`, `fallback`, `headless`). Cross-check map: are “real” symbols replaced by empty bodies?

### B. Entry → message pump (call graph)

1. From **`WinMain`**, trace every branch before the **first** `CreateWindow` / `ShowWindow` (or equivalent). Note **license**, **enterprise**, **crash handler**, **CWD**, **console alloc**, **headless** argv parsing.  
2. Trace **`Win32IDE`** construction / `createWindow` / init: `Win32IDE.cpp`, `Win32IDE_InitSequence.cpp` (if used), `Win32IDE_Window.cpp`, `Win32IDE_Core.cpp`.  
3. Identify **return codes** from `WinMain` and any `PostQuitMessage` paths.

### C. “Not connected” heuristic (systematic grep)

Search under `src/win32app/`, `src/core/`, `src/agentic/` for patterns such as:

- `TODO`, `FIXME`, `NotImplemented`, `stub`, `no-op`, `return false`, empty `WM_COMMAND` branches  
- Registered menu/command IDs **without** a handler in `Win32IDE_CommandHandlers.cpp` / `unified_command_dispatch`  
- Panels or features **constructed** but **never added** to layout / tab host  
- `ifdef` **0** or **feature flags** that default off and have no UI to enable  

For each hit, classify: **dead code**, **intentional optional**, **broken wiring**, **unsafe (crash if invoked)**.

### D. Runtime launch matrix

Run the built **`RawrXD-Win32IDE.exe`** with:

| Variant | Purpose |
|--------|---------|
| Normal double-click / Explorer | CWD = exe dir? resources load? |
| `RAWRXD_SKIP_INTEGRATED_RUNTIME=1` | Isolate coordinator crash |
| WebView2 disabled / safe launch (see `docs/VSCODE_BLANK_WINDOW_FIX.md`, `Launch-VSCode-Safe.ps1` patterns) | Blank window / GPU / mutex |
| Under **WinDbg** or **Visual Studio** debugger | Break on `ExitProcess`, first-chance exceptions |

Correlate with **`ide_startup.log`** lines and **Windows Event Viewer** / **WER** if applicable.

### E. Binary-level checks (reverse engineer)

1. **Dependencies:** `dumpbin /dependents` on the IDE exe — missing **WebView2Loader**, **Vulkan**, **VC++ runtime**?  
2. **Delay-load failures:** `main_win32.cpp` includes `<delayimp.h>` — trace **delayimp** usage and failure hooks.  
3. If ASM is suspected, confirm **ml64** version matches linker toolchain; disassemble hot paths only if needed.

## Deliverable format

Output a **single report** with:

1. **Executive summary** — top 1–3 reasons the IDE might not launch *for this tree*.  
2. **Launch timeline** — ordered steps from process start to visible window, with **failure injection points**.  
3. **Disconnected / stub inventory** — table: *Subsystem | Symptom | File:line | Connected? | Fix or owner*.  
4. **Build-time vs run-time** — separate sections.  
5. **Next actions** — exact commands, env vars, and files to patch.

## Constraints

- Treat **security-sensitive** items (UAC bypass, license bypass) as **audit for crash/wiring only**; do not suggest exploits.  
- Prefer **evidence** (paths, symbols, logs) over speculation.  
- Respect **no Qt** project rules; UI is Win32/native.

---

*Prompt file: `docs/PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md` — paste the block above into another session to run the audit.*
