#pragma once
// RawrXD_Sidebar_x64.h — C++ linkage for MASM64 sidebar subsystems
// Source: src/asm/RawrXD_Sidebar_x64.asm
// Zero Qt. Zero CRT. Pure Win64 ABI.
//
// Exports:
//   Logger     — File + OutputDebugString formatted logging
//   DebugEngine — Hardware single-stepping via trap flag
//   TreeVirt   — TreeView style + double-buffer setup
//   DarkMode   — DWM immersive dark mode attribute

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Logger ──────────────────────────────────────────────
// Initialize log file. Pass NULL for default "rawrxd_sidebar.log".
void RawrXD_Logger_Init(const char* filename);

// Formatted log write: [RAWRXD] <tick_hex> <pid> <level> <msg>\r\n
// Outputs to both OutputDebugStringA and the log file.
void RawrXD_Logger_Write(const char* level,      // "INFO","WARN","ERROR","CRIT"
                         const char* file,        // __FILE__
                         unsigned int line,       // __LINE__
                         const char* msg);        // message body

// ── Debug Engine ────────────────────────────────────────
// Attach debugger to a running process.
BOOL RawrXD_Debug_Attach(DWORD dwProcessId);

// Wait for a debug event (wraps WaitForDebugEvent).
BOOL RawrXD_Debug_Wait(void* lpDebugEvent,        // DEBUG_EVENT*
                       DWORD dwMilliseconds);

// Single-step one instruction via hardware trap flag.
// Caller must allocate CONTEXT struct (at least 1232 bytes, 16-byte aligned).
void RawrXD_Debug_Step(HANDLE hThread,
                       void* pContext);            // CONTEXT*

// ── Tree Virtualization ─────────────────────────────────
// Configure a TreeView for lazy-load style:
//   TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS
//   + TVS_EX_DOUBLEBUFFER | TVS_EX_FADEINOUTEXPANDOS
void RawrXD_Tree_LazyLoad(HWND hWndTree);

// ── Dark Mode ───────────────────────────────────────────
// Force DWM immersive dark mode on a window (DWMWA attribute 20).
void RawrXD_DarkMode_Force(HWND hWnd);

#ifdef __cplusplus
} // extern "C"
#endif

// ── Compile-time guard ──────────────────────────────────
// When MASM is available, the ASM object provides these symbols.
// When not (MinGW), the build system should provide C++ stubs.
#ifdef RAWRXD_LINK_SIDEBAR_ASM
  // Linked from RawrXD_Sidebar_x64.obj — no stubs needed
#else
  // TODO: Provide C++ fallback implementations if building without MASM
#endif
