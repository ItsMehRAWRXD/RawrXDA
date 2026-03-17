; ═══════════════════════════════════════════════════════════════════════════════
; rawrxd_master.inc - Master Include File for RawrXD (x64)
; ═══════════════════════════════════════════════════════════════════════════════
; Single include that resolves ALL undefined CALL targets across the codebase.
; Include this at the top of any 64-bit .asm file to get full cross-module
; symbol resolution at link time.
;
; Usage:
;   OPTION CASEMAP:NONE
;   INCLUDE rawrxd_master.inc
;
; The include guard prevents double-inclusion. Each sub-include also has its
; own guard, so you can include subsets individually if preferred.
; ═══════════════════════════════════════════════════════════════════════════════

IFNDEF RAWRXD_MASTER_INC
RAWRXD_MASTER_INC EQU 1

; ─── Win32 API EXTERN Declarations ────────────────────────────────────────────
; kernel32, user32, gdi32, advapi32, ole32, shell32, shlwapi, comctl32,
; comdlg32, wininet, winhttp, httpapi, ws2_32, d2d1, d3d11, dxgi, dwrite,
; dbghelp, psapi, ntdll, WebView2Loader + common constants
INCLUDE rawrxd_win32_api.inc

; ─── C Runtime EXTERN Declarations ────────────────────────────────────────────
; printf, malloc, free, strlen, atoi, fread, fwrite, atexit, etc.
INCLUDE rawrxd_crt.inc

; ─── Vulkan API Dispatch Table ────────────────────────────────────────────────
; g_vk* function pointers + direct vk* API calls + VK_ constants
INCLUDE rawrxd_vulkan_dispatch.inc

; ─── Internal Cross-Module Declarations ───────────────────────────────────────
; All ~800+ PROCs across the RawrXD codebase: Titan, Phase, Swarm, DAP,
; Extension Host, Marketplace, GPU, Thermal, Crypto, LSP, JSON, etc.
INCLUDE rawrxd_internal_protos.inc

; ─── Compiler Toolchain Declarations ──────────────────────────────────────────
; Lexer, Parser, AST, Semantic Analysis, Code Generation, PE Writer
INCLUDE rawrxd_compiler_protos.inc

ENDIF ; RAWRXD_MASTER_INC
