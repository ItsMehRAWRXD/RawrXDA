# Win32 IDE builds but does not open a window

Symptoms: **`RawrXD-Win32IDE.exe` links and runs**, then **nothing appears** (or the process vanishes instantly).

## 1. Read `ide_startup.log` (same folder as the exe)

Startup writes a trace next to the binary (if the folder is writable). Last lines show where startup stopped:

| Log hint | Meaning |
|----------|---------|
| `createWindow_FAILED` | `Win32IDE::createWindow()` returned false — see debugger / Output window. |
| Stops before `showWindow` | A **quick phase** failed; you should also get a **“Failed to initialize IDE”** message box. |
| `headless_mode` | You launched with **`--headless` / `--server`** or **`RAWRXD_HEADLESS=1`** — no main window by design. |
| `selftest_mode` / `--selftest` | Console self-test path; may exit without a persistent GUI. |

## 2. Missing delay-loaded DLLs (`vulkan-1.dll`, `D3DCOMPILER_47.dll`)

Some GPU / shader paths are **delay-loaded**. If the DLL is missing, older builds could **crash before any window** with no explanation.

**Current behavior:** `main_win32.cpp` registers **`__pfnDliFailureHook2`** so you should get a **message box** naming the missing DLL.

**Fix:** Install the [Vulkan Runtime](https://vulkan.lunarg.com/) (for `vulkan-1.dll`) and ensure the DirectX / `D3DCOMPILER_47` stack is present (Windows Update or GPU vendor runtime).

## 3. Environment forcing non-GUI

- **`RAWRXD_HEADLESS=1`** → headless path, no main window. Unset for normal IDE.
- **`RAWRXD_SKIP_INTEGRATED_RUNTIME=1`** — does **not** skip the window; only skips integrated coordinator boot (see `INTEGRATED_RUNTIME.md`).

## 4. Early crash (dump / message)

Crash containment may write under **`crash_dumps\`** and show a dialog. Run under the **debugger** or set **`RAWRXD_DEBUG_CONSOLE=1`** before launch to attach a console (see `main_win32.cpp`).

## 5. Window exists but is invisible

Rare: off-screen geometry, zero size, or compositor issues. See **`docs/VSCODE_BLANK_WINDOW_FIX.md`** / WebView2 notes if the shell is blank after the frame appears.

## 6. Safe mode

Try:

```text
RawrXD-Win32IDE.exe --safe-mode
```

(disables some extension / GPU paths — see `main_win32.cpp`.)

---

**Related:** `docs/IDE_LAUNCH_AUDIT_FINDINGS.md`, `docs/PROMPT_REVERSE_ENGINEER_IDE_LAUNCH_AUDIT.md`.
