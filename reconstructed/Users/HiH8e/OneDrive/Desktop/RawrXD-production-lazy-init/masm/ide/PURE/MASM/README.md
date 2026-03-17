# Pure MASM (No SDK/Import Libs) Strategy

This document outlines how to minimize or eliminate dependencies on Windows SDK import libraries and C runtime by using MASM-only techniques.

## Goals
- Build DLLs/EXEs with MASM toolchain only (ml/link from MASM32)
- Avoid static `includelib` references to Windows SDK libs
- Resolve Win32 APIs dynamically at runtime
- Replace CRT and `lstr*` usage with tiny assembly helpers

## Approaches

### 1) Dynamic Binding via GetProcAddress
- Keep a minimal import table (only `LoadLibraryA` and `GetProcAddress` from `kernel32.dll`).
- On startup, resolve all required APIs into a jump table.
- Pros: Simple, stable; Cons: Still a tiny import table.

### 2) PEB-Based Resolver (Zero Imports)
- Walk the PEB to locate `kernel32.dll`, parse its export table to obtain `LoadLibraryA` & `GetProcAddress`.
- Use those to bind everything else. No import libs or SDK needed.
- Pros: Zero static imports; Cons: More code and maintenance.

### 3) Replace C/Win helper APIs
- Avoid `lstrcpyA/lstrlenA/strcmp` by adding tiny assembly routines (e.g., `StrLenA`, `StrCpyA`, `StrCmpA`).
- Avoid CRT entirely.

## Rollout Plan
1. Audit `includelib` usage (scripts/audit_includelib.ps1) and rank by frequency.
2. Convert non-UI, non-COM modules first (e.g., GGUF loaders, perf metrics) to dynamic binding.
3. Add `PURE_MASM_NO_IMPORTS` compile-time switch:
   - Default OFF: existing behavior
   - ON: include `dynapi_x86.inc` and call `DYN_Init` at module init
4. Gradually remove `includelib`s from modules after verifying runtime binds.

## Sample Usage (Proposed)
```asm
; at top of module
IFDEF PURE_MASM_NO_IMPORTS
    INCLUDE include\dynapi_x86.inc
ENDIF

; later in init code
IFDEF PURE_MASM_NO_IMPORTS
    call DYN_Init ; resolves kernel32!LoadLibraryA/GetProcAddress and core APIs
ENDIF

; calling CreateFileA
IFDEF PURE_MASM_NO_IMPORTS
    DYN_CALL CreateFileA, addr path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
ELSE
    invoke CreateFileA, addr path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0
ENDIF
```

## Notes
- For UI modules (`user32`, `gdi32`, `comctl32`), dynamic binding also works; ensure all return types and stdcall conventions are correct.
- For COM (`ole32`), prefer `CoInitializeEx` dynamic bind; isolate COM code behind feature flags.
- For security/advapi, bind only the functions you use.

## Where to Start
- GGUF modules: `gguf_loader_*.asm`, `perf_metrics.asm`, `gguf_stream.asm`
- Spoof DLL: `gguf_beacon_spoof.asm` (already MASM-only constants, no SDK includes)

## Tooling
- `scripts/audit_includelib.ps1` — lists `includelib` usage across the repo.
- `build_pure_masm.ps1` — MASM-only build driver; add `-DPURE_MASM_NO_IMPORTS` when modules are converted.

---
Have me wire the first module (e.g., `gguf_loader_working.asm`) to `dynapi_x86.inc` and provide a pure-import build variant when you’re ready.
