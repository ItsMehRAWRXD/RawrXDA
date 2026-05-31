#pragma once
// =============================================================================
// RawrXD_TerminalPipe.h — C++ bridge to the MASM Terminal Pipe module
// =============================================================================
// Links against RawrXD_TerminalPipe.asm (ml64).
// All functions use the x64 __fastcall (default) ABI.
// =============================================================================

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// --- Observation Ring Buffer ------------------------------------------------

// Initialize the ring buffer and its CRITICAL_SECTION lock.
// Returns 0 on success.
int64_t RawrXD_ObsRing_Init(void);

// Push raw bytes into the ring.  Thread-safe.
// Returns number of bytes actually written.
int64_t RawrXD_ObsRing_Push(const char* data, uint64_t byteCount);

// Snapshot the last <=2048 bytes from the ring into an internal buffer.
// Returns pointer to the NUL-terminated snapshot; *outLen receives the length.
// The returned pointer is valid until the next call to Snapshot.
const char* RawrXD_ObsRing_Snapshot(void);
// NOTE: RDX on return contains the byte count.  To capture it in C++,
// use the inline wrapper below.

// Destroy the ring buffer lock.
void RawrXD_ObsRing_Destroy(void);

// --- Terminal Pipe Execution ------------------------------------------------

// Execute a command line, capturing stdout+stderr into the observation ring.
// timeoutMs: 0 = wait forever.
// Returns the child exit code, or -1 on CreateProcess failure.
int64_t RawrXD_TermPipe_Execute(const char* commandLine, uint32_t timeoutMs);

// HITL-gated execution: shows a Yes/No MessageBox before executing.
// Returns exit code, -1 on failure, -2 if user denied via HITL gate.
int64_t RawrXD_TermPipe_ExecuteWithHITL(void* hWndParent,
                                         const char* commandLine,
                                         uint32_t timeoutMs);

// --- HITL Safety Gate -------------------------------------------------------

// Show a Yes/No MessageBox.  Returns 1 if approved, 0 if denied.
int64_t RawrXD_HITL_Gate(void* hWndParent, const char* description);

// --- Tool Dispatch Jump Table -----------------------------------------------

// Register a tool handler.  Returns slot index or -1 if table is full.
// handler signature: int64_t handler(void* argStruct)
typedef int64_t (*RawrXD_ToolHandler)(void* argStruct);
int64_t RawrXD_ToolTable_Register(const char* toolName,
                                   RawrXD_ToolHandler handler);

// Dispatch to a registered tool by name.  Returns handler result or -1.
int64_t RawrXD_ToolTable_Dispatch(const char* toolName, void* argStruct);

#ifdef __cplusplus
}
#endif
