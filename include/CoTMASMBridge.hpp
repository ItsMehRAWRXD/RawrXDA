// =============================================================================
// CoTMASMBridge.hpp — C++ Bridge to Pure MASM64 Chain-of-Thought Engine DLL
// =============================================================================
//
// Phase 37.1: Typed C++ interface for rawrxd_cot_engine.asm + rawrxd_cot_dll_entry.asm
//
// Links against: rawrxd_cot_engine.obj + rawrxd_cot_dll_entry.obj
//   (or rawrxd_cot.dll if loaded dynamically)
//
// Build: Include in chain_of_thought_engine.cpp and link .obj files.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

// =============================================================================
//                     MASM Structure Mirrors (C++ side)
// =============================================================================

// Must match COT_STEP_DESC in rawrxd_cot_engine.asm (1408 bytes)
struct CoTStepDesc {
    uint32_t roleId;                        // CoTRoleId (0-11)
    uint32_t skip;                          // 1 = skip this step
    char     modelName[256];                // Model override (empty = default)
    char     instructionOverride[1024];     // Custom instruction (empty = role default)
    char     _padding[92];                  // Pad to 1408 bytes
};
static_assert(sizeof(CoTStepDesc) == 1408, "CoTStepDesc size mismatch with MASM");

// Must match COT_STEP_RESULT in rawrxd_cot_engine.asm
struct CoTStepResult {
    uint32_t stepIndex;                     // 0-based
    uint32_t roleId;                        // CoTRoleId used
    char     modelUsed[256];                // Actual model name
    uint32_t success;                       // 1 = success
    uint32_t skipped;                       // 1 = skipped
    uint32_t errorCode;                     // 0 = none
    uint32_t latencyMs;                     // Step execution time
    uint32_t tokenCount;                    // Approximate token count
    uint32_t outputLen;                     // Bytes of output
    uint32_t _padding2;                     // Alignment
    void*    outputPtr;                     // Pointer to output data
};

// Must match COT_STATS in rawrxd_cot_engine.asm (112 bytes)
struct CoTStats {
    uint32_t totalChains;
    uint32_t successfulChains;
    uint32_t failedChains;
    uint32_t totalStepsExecuted;
    uint32_t totalStepsSkipped;
    uint32_t totalStepsFailed;
    uint32_t avgLatencyMs;
    uint32_t _pad0;
    uint32_t roleUsage[12];                 // Per-role usage counts (ROLE_COUNT=12)
    uint64_t _reserved[4];                  // Future expansion
};

// Must match COT_ROLE_ENTRY in rawrxd_cot_engine.asm (40 bytes)
struct CoTRoleEntry {
    uint32_t roleId;
    uint32_t _pad0;
    const char* namePtr;
    const char* labelPtr;
    const char* iconPtr;
    const char* instructionPtr;
};

// =============================================================================
//                     Role ID Enum (matches MASM constants)
// =============================================================================

enum class CoTRoleId : uint32_t {
    Reviewer        = 0,
    Auditor         = 1,
    Thinker         = 2,
    Researcher      = 3,
    DebaterFor      = 4,
    DebaterAgainst  = 5,
    Critic          = 6,
    Synthesizer     = 7,
    Brainstorm      = 8,
    Verifier        = 9,
    Refiner         = 10,
    Summarizer      = 11,
    COUNT           = 12
};

// =============================================================================
//                     Inference Callback Type
// =============================================================================

// Callback invoked for each CoT step during chain execution.
// Must return bytes written to outputBuf, or -1 on error.
typedef int64_t(__cdecl* CoTInferenceCallback)(
    const char* systemPrompt,       // Role instruction / system prompt
    const char* userMessage,        // User query (cumulative context)
    const char* modelName,          // Model to use for this step
    char*       outputBuf,          // Pre-allocated output buffer (1MB)
    int64_t     outputBufSize       // Size of outputBuf in bytes
);

// =============================================================================
//                     Opaque Handle for C callers
// =============================================================================

// CoTBuffer is the opaque handle returned by CoT_Initialize_Core.
// It maps to the internal arena (g_arenaBase). The actual state is global
// within the DLL; this handle is provided for API symmetry.
struct CoTBuffer {
    void*    arenaBase;
    uint64_t arenaUsed;
    uint64_t arenaCommitted;
    uint32_t initialized;
    uint32_t version;
};

// =============================================================================
//                   MASM Exported Functions (extern "C")
// =============================================================================

extern "C" {

// ---- Lifecycle ----

// Reserve 1GB VA, init data structures. Attempts large pages + NUMA if available.
// Returns: 0 on success, NTSTATUS on failure
uint32_t __cdecl CoT_Initialize_Core();

// Release all virtual memory and reset all state (full shutdown).
// Returns: 0 on success
uint32_t __cdecl CoT_Shutdown_Core();

// Release only the arena memory (partial cleanup, stats preserved).
// Returns: 0 on success, STATUS_UNSUCCESSFUL if no arena
uint32_t __cdecl CoT_Release_Core();

// ---- Configuration ----

// Set max chain steps (clamped to [1, 8]). Returns clamped value.
uint32_t __cdecl CoT_Set_Max_Steps(uint32_t maxSteps);

// Get current max steps.
uint32_t __cdecl CoT_Get_Max_Steps();

// Remove all configured steps.
void __cdecl CoT_Clear_Steps();

// Add a step. Returns 0 on success, -1 if chain full.
int32_t __cdecl CoT_Add_Step(
    uint32_t    roleId,             // CoTRoleId (0-11)
    const char* modelName,          // NULL for default
    const char* instruction,        // NULL for role default
    uint32_t    skip                // 0 = don't skip, 1 = skip
);

// Load a named preset (replaces current steps).
// Names: "review", "audit", "think", "research", "debate", "custom"
// Returns: 0 on success, -1 if not found
int32_t __cdecl CoT_Apply_Preset(const char* presetName);

// Get number of configured steps.
uint32_t __cdecl CoT_Get_Step_Count();

// ---- Execution ----

// Execute the chain synchronously. Callback invoked per non-skipped step.
// Returns: steps completed (RAX), pointer to results array (RDX)
uint32_t __cdecl CoT_Execute_Chain(
    const char*           userQuery,
    CoTInferenceCallback  callback
);

// Set cancellation flag. Running chain stops after current step.
void __cdecl CoT_Cancel();

// Check if chain is currently executing.
// Returns: 1 if running, 0 otherwise
uint32_t __cdecl CoT_Is_Running();

// ---- Statistics ----

// Copy stats to caller buffer. Returns 0 on success.
int32_t __cdecl CoT_Get_Stats(CoTStats* outStats);

// Zero all statistics counters.
void __cdecl CoT_Reset_Stats();

// ---- Introspection ----

// Build JSON status into buffer. Returns bytes written (excl NUL).
uint32_t __cdecl CoT_Get_Status_JSON(char* buffer, uint32_t bufferSize);

// Get role info by ID. Returns 0 on success.
int32_t __cdecl CoT_Get_Role_Info(uint32_t roleId, CoTRoleEntry* outEntry);

// ---- Arena (raw data) ----

// Append raw token data to 1GB arena. Atomic, thread-safe for debate mode.
// Returns: new total bytes used, or 0 on failure
uint64_t __cdecl CoT_Append_Data(const char* data, uint64_t size);

// Get base pointer to arena data.
void* __cdecl CoT_Get_Data_Ptr();

// Get bytes used in arena.
uint64_t __cdecl CoT_Get_Data_Used();

// ---- Version ----

// Returns engine version (0x00010000 = 1.0.0)
uint32_t __cdecl CoT_Get_Version();

// ---- Threading (from rawrxd_cot_dll_entry.asm) ----

// Get count of attached threads.
uint32_t __cdecl CoT_Get_Thread_Count();

// ---- Per-Thread Error Reporting (TLS) ----

// Get last error code for calling thread (set by SEH handler).
uint32_t __cdecl CoT_TLS_GetLastError();

// Get fault RIP for calling thread (instruction that caused AV).
uint64_t __cdecl CoT_TLS_GetFaultRIP();

// Set per-thread error (typically called by SEH handler internals).
int32_t __cdecl CoT_TLS_SetError(
    uint32_t errorCode,
    uint64_t faultRIP,
    uint64_t faultAddress
);

// Check if large pages (MEM_LARGE_PAGES) are available.
// Returns 1 if SeLockMemoryPrivilege detected.
uint32_t __cdecl CoT_Has_Large_Pages();

// ---- SRW Lock (direct access, use with caution) ----

void __cdecl Acquire_CoT_Lock();
void __cdecl Release_CoT_Lock();
void __cdecl Acquire_CoT_Lock_Shared();
void __cdecl Release_CoT_Lock_Shared();

} // extern "C"

// =============================================================================
//                  C++ RAII Wrappers (convenience layer)
// =============================================================================

namespace RawrXD {
namespace CoT {

// RAII lock guard for exclusive access
class ExclusiveLockGuard {
public:
    ExclusiveLockGuard()  { Acquire_CoT_Lock(); }
    ~ExclusiveLockGuard() { Release_CoT_Lock(); }
    ExclusiveLockGuard(const ExclusiveLockGuard&) = delete;
    ExclusiveLockGuard& operator=(const ExclusiveLockGuard&) = delete;
};

// RAII lock guard for shared (read) access
class SharedLockGuard {
public:
    SharedLockGuard()  { Acquire_CoT_Lock_Shared(); }
    ~SharedLockGuard() { Release_CoT_Lock_Shared(); }
    SharedLockGuard(const SharedLockGuard&) = delete;
    SharedLockGuard& operator=(const SharedLockGuard&) = delete;
};

// RAII arena lifetime manager
class ArenaGuard {
public:
    ArenaGuard() {
        m_ok = (CoT_Initialize_Core() == 0);
    }
    ~ArenaGuard() {
        if (m_ok) CoT_Release_Core();
    }
    bool valid() const { return m_ok; }
    ArenaGuard(const ArenaGuard&) = delete;
    ArenaGuard& operator=(const ArenaGuard&) = delete;
private:
    bool m_ok;
};

// =============================================================================
//                  Integration Example (for chain_of_thought_engine.cpp)
// =============================================================================
//
// Usage in ChainOfThoughtEngine:
//
//   void ChainOfThoughtEngine::allocateHugeBuffer() {
//       m_hugeBuffer = reinterpret_cast<void*>(1); // sentinel — arena is global
//       uint32_t rc = CoT_Initialize_Core();       // calls MASM, tries large pages
//       if (rc != 0) {
//           logError("CoT arena init failed: 0x%08X", rc);
//           m_hugeBuffer = nullptr;
//       }
//   }
//
//   void ChainOfThoughtEngine::appendStepOutput(const std::string& output) {
//       uint64_t used = CoT_Append_Data(output.data(), output.size());
//       if (used == 0) {
//           uint32_t err = CoT_TLS_GetLastError();
//           uint64_t rip = CoT_TLS_GetFaultRIP();
//           logError("Append failed: err=0x%X, fault_rip=0x%llX", err, rip);
//       }
//   }
//
//   void ChainOfThoughtEngine::releaseHugeBuffer() {
//       CoT_Release_Core();       // Just free the arena
//       m_hugeBuffer = nullptr;
//   }
//
// =============================================================================

} // namespace CoT
} // namespace RawrXD
