# MASM IDE – No-SDK / No-ImportLibs Audit (Dec 21, 2025)

## Goal
Minimize build-time dependencies on SDK headers/import libraries by:
- Replacing `windows.inc` usage with hand-rolled constants where possible
- Removing `includelib *.lib` for modules that can dynamically bind APIs
- Using a PEB-based resolver to support **zero import tables** for stealth/pure modules
- Stubbing small missing symbols to keep MASM-only pipelines unblocked

## Current Static Dependencies (Repo Scan)

### `includelib` usage (files referencing the library)
High-frequency:
- `kernel32`: 70 files
- `user32`: 68 files
- `comctl32`: 18 files
- `shell32`: 18 files
- `shlwapi`: 14 files
- `gdi32`: 9 files

Low-frequency:
- `advapi32` (2), `psapi` (3), `ole32` (1), `crypt32` (1), `wininet` (1), `riched20` (1), `ntdll` (1)

Script:
- `scripts/audit_includelib.ps1`

### MASM32 SDK include usage
- `\masm32\include\windows.inc`: 77 files
- `\masm32\include\kernel32.inc`: 70 files
- `\masm32\include\user32.inc`: 66 files
- `\masm32\include\shell32.inc`: 18 files
- `\masm32\include\comctl32.inc`: 15 files
- `\masm32\include\shlwapi.inc`: 15 files
- `\masm32\include\gdi32.inc`: 10 files
- Others are rare (`ole32`, `psapi`, `wininet`)

Script:
- `scripts/audit_sdk_includes.ps1`

## What Can Be Done MASM-Only (No SDK / No Import Libs)

### 1) Stub Small Missing Symbols
Best for: glue modules that link against optional components.
- Pattern: create a minimal `PUBLIC` symbol (no-op proc or dummy global) to satisfy link.
- Example: `src/gguf_beacon_spoof.asm` exports dummy `g_hMainWindow` / `g_hInstance`.

### 2) Replace SDK headers where possible
Best for: non-UI modules that only need a small set of constants.
- Added: `include/mini_winconst.inc` (TRUE/FALSE/NULL + file/mapping flags + DLL reasons)

### 3) Remove static import libs (dynamic binding)
Best for: kernel32-heavy modules (GGUF, perf, logging, compression).
- Added:
  - `src/dynapi_x86.asm`: PEB-based resolver (no import libs)
  - `include/dynapi_x86.inc`: prototypes/macros
- Converted (opt-in): `src/gguf_loader_working.asm`
  - Supports `PURE_MASM_NO_IMPORTLIBS` (uses local WinAPI wrapper procs calling resolved pointers)
  - Default build remains unchanged (still supports static kernel32 import)

### 4) Zero-import modules (stealth/pure pipeline)
Best for: shim DLLs / beacons / perf shims.
- `src/gguf_beacon_spoof.asm` now compiles without `windows.inc` and without `includelib`.
- For true zero import tables, avoid any direct `invoke` to OS APIs; use PEB resolver (dynapi) or avoid OS calls entirely.

### 5) COM / Advanced APIs (gate behind flags)
Best for: modules that only sometimes need COM.
- Strategy:
  - Bind `ole32!CoInitializeEx`, `ole32!CoUninitialize`, etc. dynamically via dynapi.
  - Put COM code behind a feature flag (compile-time or runtime), so pure builds omit it.

## Recommended Conversion Order
1. **Kernel32-only / core**: `gguf_loader_*.asm`, `gguf_compression*.asm`, `perf_metrics.asm`
2. **Mixed kernel32+user32 (logging/UI-lite)**: `error_logging_*`, `performance_*`
3. **UI-heavy**: `window.asm`, `ui_layout*.asm`, `tab_control*.asm` (more constants + more API surface)
4. **COM**: `main_enhanced.asm` (bind `ole32` dynamically or gate)

## How to Build in No-ImportLibs Mode
- `build_pure_masm.ps1` now supports `-NoImportLibs`.
- This mode:
  - Defines `PURE_MASM_NO_IMPORTLIBS`
  - Auto-adds `dynapi_x86` to module list
  - Omits Win32 import libs at link time

Example (module proof-build):
```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\build_pure_masm.ps1 -Profile Stub -Modules dynapi_x86,gguf_loader_working -NoImportLibs -ExeName GGUF_NoImport.exe
```

## Next: Extend dynapi to UI libs
If you want to remove `user32/gdi32/comctl32` import libs, the next step is to:
- Bind those DLLs by name (e.g., `USER32.DLL`) via `DYN_FindModuleBaseA`
- Resolve required exports via `DYN_GetProcAddressA`
- Add wrapper procs for the small subset of APIs each module uses
