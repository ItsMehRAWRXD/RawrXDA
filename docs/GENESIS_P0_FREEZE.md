# Genesis P0 — Feature Freeze Checklist

**Goal:** Set the six GenesisP0 MASM64 modules in stone so they get **done** (build + one call path) instead of expanded.

---

## Current state (in-tree)

| Module | File | Exports (current) | CMake | C++ API header |
|--------|------|-------------------|-------|----------------|
| Vulkan Compute | `GenesisP0_VulkanCompute.asm` | `Genesis_VulkanCompute_Init`, `_Dispatch`, `_Shutdown` | ✅ Linked | `genesis_exports.h` |
| Extension Host | `GenesisP0_ExtensionHost.asm` | `Genesis_ExtensionHost_Create`, `_LoadExtension`, `_InvokeCommand` | ✅ Linked | `genesis_exports.h` |
| White-Screen Guard | `GenesisP0_WhiteScreenGuard.asm` | `Genesis_WhiteScreenGuard_StartMonitoring`, `_Ping`, `_ForceRepaint` | ✅ Linked | `genesis_exports.h` |
| Self-Hosting | `GenesisP0_SelfHosting.asm` | `Genesis_SelfHosting_CompileASM`, `_LinkEXE`, `_Verify` | ✅ Linked | `genesis_exports.h` |
| AI Backend Bridge | `GenesisP0_AiBackendBridge.asm` | `Genesis_AiBackendBridge_Init`, `_SendPrompt`, `_StreamResponse` | ✅ Linked | `genesis_exports.h` |
| Build Orchestrator | `GenesisP0_BuildOrchestrator.asm` | `Genesis_BuildOrchestrator_Init`, `_AddJob`, `_WaitAll` | ✅ Linked | `genesis_exports.h` |

- **CMake:** All six are assembled and linked into `RawrXD-Win32IDE` when MSVC + all 6 `.asm` files exist. Defines: `RAWR_GENESIS_P0_MASM=1`, `RAWR_GENESIS_P0_VULKAN=1`, etc.
- **C++:** `src/asm/genesis_exports.h` declares the API. **No call site** in the codebase yet — the API is declared but not invoked.

---

## What was done to freeze (minimal fixes)

1. **GenesisP0_BuildOrchestrator.asm** — Added missing `EXTERN WaitForSingleObject:PROC` (used by `WorkerThread`).
2. **GenesisP0_WhiteScreenGuard.asm** — Fixed `Genesis_WhiteScreenGuard_ForceRepaint`: use saved `RCX` (hwnd) for `InvalidateRect`/`UpdateWindow` instead of `[rbp+16]`; save/restore `RBX`.
3. **genesis_exports.h** — Added `#include <windows.h>` so `HWND` (and usual `size_t`) are defined when the header is included.

---

## What remains to “set in stone” (no expansion)

| # | Task | Scope |
|---|------|--------|
| 1 | **Build** — Run `cmake --build build --target RawrXD-Win32IDE --config Release` and fix any remaining MASM/link errors. | Fix only; no new features. |
| 2 | **Single call path** — From `Win32IDE` (or one startup path), call **one** Genesis P0 API so the pack is “used” and not dead code, e.g. `Genesis_WhiteScreenGuard_StartMonitoring(mainHwnd)` after main window create, and `Genesis_WhiteScreenGuard_Ping()` on a timer or paint. | One integration point only. |
| 3 | **Verification one-liner** — Run the PowerShell one-liner (or equivalent) to confirm: 6 MASM OBJs built, Genesis tool exists if used, IDE exe links. | Docs/CI only. |

**Explicitly out of scope for freeze:**  
Adding new exports, new modules, new dependencies, or rewriting the six files to the “copy/paste” alternate API (e.g. `VulkanInit` / `ExtensionHostInit` / `OllamaInit` / etc.). Those would require changing `genesis_exports.h` and all future call sites; treat that as a **later** change if desired.

---

## If you switch to the “copy/paste” six-file set

The alternate versions you pasted use different names and more Win32/Vulkan surface. To make **those** build and link without expanding further you’d need at least:

- **VulkanCompute:** Implement or remove `PrintError`; fix Vulkan C ABI (VkCreateInstance etc. take different args); save/restore `r12`/`r13` in `VulkanDispatchCompute`.
- **ExtensionHost:** Save/restore `r12`/`r13` in `ExtensionLoad`; add `CreateWindowExA`/`RegisterClassExA`/`GetMessageA` etc. to EXTERN if used.
- **WhiteScreenGuard:** Add `EXTERN CreateSolidBrush:PROC` (from user32) and remove the local `CreateSolidBrush` forward.
- **SelfHosting:** Add `EXTERN RtlZeroMemory:PROC` (kernel32) or keep a local implementation; fix `CreateProcessA`/`StrCopy` usage so the command line is correct.
- **AiBackendBridge:** WinHttp APIs use **wide** strings for server name; convert or use `WinHttpOpen` with a suitable agent; implement or stub `BuildRequestJson` and `requestBuffer`/`requestLen`.
- **BuildOrchestrator:** Add `EXTERN WaitForSingleObject:PROC`; implement `CheckDependencies` and `StartJobProcess` (or stub so submit/wait don’t hang); fix `g_jobCount`/`g_jobSlots` so `WaitAll` waits on real job handles.

Then either:

- Update `genesis_exports.h` (and any callers) to the new names (`VulkanInit`, `ExtensionHostInit`, …), or  
- Add a small C/C++ shim that exports the **current** `Genesis_*` names and calls the new ASM procs internally.

---

## Verification one-liner (current CMake layout)

```powershell
cd D:\rawrxd
cmake -B build -S . 2>&1 | Out-Null
cmake --build build --target RawrXD-Win32IDE --config Release 2>&1 | Select-String "error|Assembling|GenesisP0"
$objs = Get-ChildItem build/GenesisP0*.obj -ErrorAction SilentlyContinue | Measure-Object | Select-Object -ExpandProperty Count
$ide = Test-Path build/Release/RawrXD-Win32IDE.exe
Write-Host "GenesisP0 OBJs: $objs/6 | IDE: $([int]$ide)" -NoNewline
if ($objs -eq 6 -and $ide) { Write-Host " OK" -ForegroundColor Green } else { Write-Host " FAIL" -ForegroundColor Red }
```

(Adjust `build/Release/` if your generator uses a different output path.)

---

## Summary

- **To freeze the current design:** (1) Build and fix any remaining MASM/link errors, (2) add **one** call path (e.g. WhiteScreenGuard on main window), (3) run the verification one-liner. No new features, no API expansion.
- **To adopt the pasted six-file set:** Treat as a separate change: fix the listed ASM/API issues, then either rename the C++ API to match or add a thin C shim so existing `Genesis_*` names still work.
