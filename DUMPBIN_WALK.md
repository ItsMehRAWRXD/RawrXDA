# RawrXD-Win32IDE — Dumpbin Walk

Generated for `build_ide\bin\RawrXD-Win32IDE.exe`.

## Headers (summary)

| Field | Value |
|-------|--------|
| **Entry point (RVA)** | `0x48A1B8` (→ `0x88A1B8` in memory with base 0x400000) |
| **Subsystem** | 2 = Windows GUI |
| **Machine** | 8664 = x64 |
| **DLL characteristics** | Dynamic base, NX compatible, Terminal Server Aware |
| **Size of image** | 0x4FC8000 |
| **Sections** | .text, .rdata, .data, .pdata, _DATA64, TELEMETR, .rsrc, .reloc |

## Dependencies (load-order)

All DLLs are system or Vulkan; no custom DLLs required in `bin` except optional:

- **Delay-loaded** (app starts even if missing; first use loads them; missing-DLL shows a clear message and exit):
  - `vulkan-1.dll` — Vulkan Runtime or SDK
  - `D3DCOMPILER_47.dll` — DirectX Redist
- Load-time: `COMCTL32.dll`, `COMDLG32.dll`, `SHELL32.dll`, `ole32.dll`, `OLEAUT32.dll`, `SHLWAPI.dll`
- `dbghelp.dll`, `WINHTTP.dll`, `WS2_32.dll`, `WINMM.dll`
- `GDI32.dll`, `USER32.dll`, `ntdll.dll`
- `d2d1.dll`, `DWrite.dll`, `d3d11.dll`, `dcomp.dll`, `dwmapi.dll`
- `ADVAPI32.dll`, `bcrypt.dll`, `WINTRUST.dll`, `OPENGL32.dll`, `WININET.dll`, `KERNEL32.dll`

Optional (loaded from same dir if present): `RawrXD_MultiWindow_Kernel.dll`.

## Launch

- **Recommended:** From repo root run `Launch_RawrXD_IDE.bat` so CWD is the repo (config, crash_dumps, plugins).
- Or run `build_ide\bin\RawrXD-Win32IDE.exe` from that directory; config then falls back to exe dir.

## If the IDE crashes on startup

1. **Rebuild** with current CMake so `vulkan-1.dll` and `D3DCOMPILER_47.dll` are **delay-loaded** (no load-time dependency).
2. **Exit code:** Run from cmd: `build_ide\bin\RawrXD-Win32IDE.exe` and note exit code or message box.
3. If you see "RawrXD IDE could not load a required DLL" — install Vulkan Runtime and/or DirectX Redist, or run from the build directory.
4. **Dumpbin:** `dumpbin /DEPENDENTS build\bin\RawrXD-Win32IDE.exe` to confirm delay-load descriptors.
