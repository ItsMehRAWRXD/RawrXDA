// ============================================================================
// backend_selector_masm.h — C++ declarations for MASM backend selector
// ============================================================================
// Provides extern "C" linkage to the MASM implementation in backend_selector.asm
//
// Usage:
//   #include "backend_selector_masm.h"
//   uint32_t mask = BackendSelector_ProbeAll();
//   if (mask & BACKEND_VULKAN) { ... }
//   int err = BackendSelector_CreateBest();
//   if (err == 0) { /* success */ }
// ============================================================================

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Backend capability flags (must match MASM EQU values)
#define BACKEND_VULKAN  0x00000001
#define BACKEND_HIP     0x00000002
#define BACKEND_CUDA    0x00000004
#define BACKEND_TITAN   0x00000008
#define BACKEND_CPU     0x80000000

// Probe all available GPU backends. Returns bitmask in lower 32 bits of RAX.
uint32_t BackendSelector_ProbeAll(void);

// Create the best available backend (priority: Titan > Vulkan > HIP > CUDA).
// Returns 0 on success, non-zero on failure (no GPU backend available).
int BackendSelector_CreateBest(void);

// Shutdown the active backend and release all handles.
void BackendSelector_Shutdown(void);

// Get the currently active backend type.
uint32_t BackendSelector_GetActive(void);

// Get the bitmask of available backends.
uint32_t BackendSelector_GetAvailable(void);

// Check if a specific backend is available.
// backendFlag: one of BACKEND_VULKAN, BACKEND_HIP, BACKEND_CUDA, BACKEND_TITAN
// Returns 1 if available, 0 if not.
int BackendSelector_IsBackendAvailable(uint32_t backendFlag);

#ifdef __cplusplus
}
#endif
