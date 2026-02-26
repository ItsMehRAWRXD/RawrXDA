# RawrXD Win32 IDE — Full Debug Walkthrough

Diagnostic steps when the IDE does not launch or exits immediately.

---

## 1. Quick Checks

| Check | Command | Expected |
|-------|---------|----------|
| Exe exists | `Test-Path "d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe"` | `True` |
| Exe size | `(Get-Item "d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe").Length` | ~65 MB |
| CWD for launch | Run from `d:\rawrxd\build_ide\bin` | Required (or exe sets CWD on startup) |

---

## 2. Dumpbin — Dependencies & Entry Point

From **Developer Command Prompt** (or with full path to dumpbin):

```cmd
dumpbin /DEPENDENTS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe
dumpbin /HEADERS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe | findstr "subsystem entry DLL"
```

### Actual dependencies (dumpbin /DEPENDENTS)

```
vulkan-1.dll
COMCTL32.dll
COMDLG32.dll
SHELL32.dll
ole32.dll
OLEAUT32.dll
SHLWAPI.dll
dbghelp.dll
WINHTTP.dll
WS2_32.dll
WINMM.dll
GDI32.dll
USER32.dll
ntdll.dll
d2d1.dll
DWrite.dll
d3d11.dll
dcomp.dll
D3DCOMPILER_47.dll
dwmapi.dll
ADVAPI32.dll
bcrypt.dll
WINTRUST.dll
OPENGL32.dll
WININET.dll
KERNEL32.dll
IPHLPAPI.DLL
dxgi.dll
```

**vulkan-1.dll** must be in PATH or `C:\Windows\System32`. Vulkan SDK: `C:\VulkanSDK\...\Bin`

### Subsystem & entry (dumpbin /HEADERS)

- **Subsystem:** 2 (Windows GUI)
- **Subsystem version:** 6.00
- **Entry point:** 0x744B0C (RVA 0xB44B0C)
- **DLL characteristics:** 0x8140

---

## 3. Startup Log — `ide_startup.log`

Location: `d:\rawrxd\build_ide\bin\ide_startup.log`

Each line: `<timestamp_ms> <step> [detail]`

### Last known successful trace

```
WinMain start
crash_containment_installed
dpi_awareness
init_common_controls
first_run_gauntlet_start
first_run_gauntlet_done
vsix_loader
plugin_signature
creating_ide_instance
createWindow_start
createWindow_ok          ← If missing, crash in createWindow/onCreate
enterprise_license_skipped
showWindow
camellia_skipped
masm_init_skipped
swarm_skipped
auto_update_done
layout_start
layout_done
message_loop_entered     ← If missing, crash before GetMessage loop
```

If the log **stops before** `createWindow_ok`, the crash is in window creation or `onCreate`.  
If it stops before `message_loop_entered`, the crash is in post-create init (engine, MMF, JS host, etc.).

---

## 4. Crash Log — `rawrxd_crash.log`

Location: `d:\rawrxd\build_ide\bin\rawrxd_crash.log`

### Known entries

```
BACKGROUND THREAD CRASH: Exception 0xC0000005   (ACCESS_VIOLATION)
BACKGROUND THREAD CRASH: Exception 0xC00000FD   (STACK_OVERFLOW)
```

These come from **background threads** (e.g. `deferredHeavyInitBody`, Enterprise License, Camellia, MASM). They are caught by SEH and are **non-fatal** — the main window should remain open. If the IDE still exits, the crash may be on the **main thread** or another path.

---

## 5. Run Commands

### A. Normal launch (from bin dir)

```powershell
cd d:\rawrxd\build_ide\bin
.\RawrXD-Win32IDE.exe
```

### B. With debug console (stdout/stderr visible)

```powershell
cd d:\rawrxd\build_ide\bin
$env:RAWRXD_DEBUG_CONSOLE = "1"
.\RawrXD-Win32IDE.exe
```

### C. Safe mode (disables extensions, Vulkan, GPU)

```powershell
cd d:\rawrxd\build_ide\bin
.\RawrXD-Win32IDE.exe --safe-mode
```

### D. Headless (CLI only, no GUI)

```powershell
cd d:\rawrxd\build_ide\bin
.\RawrXD-Win32IDE.exe --headless
```

---

## 6. Debugging with Dumpbin

**Note:** `dumpbin` is not in PATH by default. Use **Developer Command Prompt for VS** or invoke by full path, e.g.:
`C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe`

```cmd
:: Dependencies (DLLs required at load time)
dumpbin /DEPENDENTS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe

:: Imports (which symbols are used)
dumpbin /IMPORTS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe > ide_imports.txt

:: Headers (subsystem, entry, sections)
dumpbin /HEADERS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe > ide_headers.txt

:: Exports (if any)
dumpbin /EXPORTS d:\rawrxd\build_ide\bin\RawrXD-Win32IDE.exe
```

---

## 7. Typical Failure Modes

| Symptom | Likely cause | Action |
|---------|--------------|--------|
| Nothing happens | Exe not found or wrong CWD | Run from `build_ide\bin` |
| "Failed to initialize IDE" | createWindow failed | Check ide_startup.log, last step before failure |
| Window flashes and closes | Crash in main or background thread | Check rawrxd_crash.log, crash_dumps\*.dmp |
| Exit code 0 immediately | WM_QUIT posted early | Check for dialogs/automation that close the app |
| Exit code -1073741819 (0xC0000005) | Access violation | Use --safe-mode; check crash_dumps\ |
| Missing vulkan-1.dll | Vulkan not in PATH | Add Vulkan SDK Bin to PATH or install Vulkan runtime |
| No window visible, log shows message_loop_entered | Window off-screen or minimized | Try Win+Shift+Left/Right to move window; or use `--safe-mode` |

---

## 8. Crash Dumps

If the crash handler runs, a minidump is written to:

```
d:\rawrxd\build_ide\bin\crash_dumps\*.dmp
```

Open with Visual Studio: **File → Open → File** → select `.dmp` → Debug with Native.

---

## 9. Verify Vulkan

```powershell
where.exe vulkan-1.dll
# Expected: C:\Windows\System32\vulkan-1.dll or C:\VulkanSDK\...\Bin\vulkan-1.dll
```

---

## 10. Quick Diagnostic Script

Already saved as `build_ide\bin\Run-IDE-Debug.ps1`. Run:

```powershell
cd d:\rawrxd\build_ide\bin
.\Run-IDE-Debug.ps1
```

Script content:

```powershell
Set-Location $PSScriptRoot
Write-Host "=== RawrXD IDE Debug Launch ===" -ForegroundColor Cyan
Write-Host "CWD: $(Get-Location)"
Write-Host "Exe exists: $(Test-Path '.\RawrXD-Win32IDE.exe')"
if (Test-Path 'ide_startup.log') {
    Write-Host "`nLast 5 lines of ide_startup.log:"
    Get-Content 'ide_startup.log' -Tail 5
}
if (Test-Path 'rawrxd_crash.log') {
    Write-Host "`nrawrxd_crash.log:"
    Get-Content 'rawrxd_crash.log'
}
$env:RAWRXD_DEBUG_CONSOLE = "1"
Write-Host "`nLaunching IDE (debug console enabled)..."
$p = Start-Process -FilePath ".\RawrXD-Win32IDE.exe" -PassThru -Wait
Write-Host "Exit code: $($p.ExitCode)"
```

---

## Summary

1. Run from `build_ide\bin`
2. Use `RAWRXD_DEBUG_CONSOLE=1` to see console output
3. Use `--safe-mode` if crashes persist
4. Inspect `ide_startup.log` to see how far startup got
5. Inspect `rawrxd_crash.log` and `crash_dumps\` for crash details
6. Ensure Vulkan DLL is findable (PATH or System32)
