#pragma once
// ============================================================================
// cot_phase39_exports.h — Phase 39–42 Enterprise CoT Finishers (C++ Bindings)
// ============================================================================
//
// Provides extern "C" declarations for the 16 new MASM64 procs introduced
// in Phase 39–42 of the RawrXD CoT Engine.
//
// Usage: Include this header in any C++ translation unit that needs to call
// the Phase 39–42 procs. The linker resolves symbols against
// rawrxd_cot_phase39.obj (built by ml64 from rawrxd_cot_phase39.asm).
//
// All functions follow the project's PatchResult-style contract:
//   - Return 0 on success, -1 on failure (unless documented otherwise)
//   - No exceptions thrown
//   - Thread-safe (SRW lock + atomics as appropriate)
//
// Generated: 2026-02-08 | Phase 39–42 Complete
// ============================================================================

#ifndef RAWRXD_COT_PHASE39_EXPORTS_H
#define RAWRXD_COT_PHASE39_EXPORTS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Structures (must match MASM layout exactly)
// ============================================================================

// Adaptive Commit Governor state (Phase 39)
struct CoT_CommitGovernor {
    uint64_t CommitRateEMA;         // bytes/sec exponential moving average
    uint64_t LastCommitTS;          // GetTickCount64 timestamp of last commit
    uint32_t ThrottleLevel;         // 0=none, 1=soft, 2=hard
    uint32_t TotalCommitEvents;     // counter for telemetry
    uint64_t BytesCommittedTotal;   // lifetime bytes committed
    uint64_t SoftThrottleCount;     // times soft throttle engaged
    uint64_t HardThrottleCount;     // times hard throttle engaged
    uint64_t _reserved;             // alignment pad
};

// Snapshot descriptor — O(1) capture of arena state (Phase 40)
struct CoT_Snapshot {
    uint64_t UsedLength;            // g_arenaUsed at snapshot time
    uint64_t CommitMark;            // g_arenaCommitted at snapshot time
    uint64_t Timestamp;             // GetTickCount64 at snapshot time
    uint32_t ExecutionState;        // g_executionState at snapshot
    uint32_t Valid;                 // 1 = valid, 0 = empty slot
};

// Shared-Memory Telemetry Page (Phase 41)
// Single struct, mapped once, read-only to observers.
// No locks needed for reads — counters are updated via atomic ops.
struct CoT_Telemetry {
    // Arena metrics
    uint64_t TotalCommitted;        // bytes committed in arena
    uint64_t TotalUsed;             // bytes used in arena
    uint64_t ArenaBase;             // base address (for diagnostics)
    // Throughput metrics
    uint64_t AppendRate;            // bytes/sec (EMA)
    uint64_t AppendCount;           // total append calls
    uint64_t AppendBytesTotal;      // total bytes appended
    // Copy engine metrics
    uint64_t CopyERMSCount;         // copies via ERMS engine
    uint64_t CopyAVX2Count;         // copies via AVX2 engine
    uint64_t CopyAVX512Count;       // copies via AVX-512 engine
    uint64_t CopyERMSBytes;         // bytes copied via ERMS
    uint64_t CopyAVX2Bytes;         // bytes copied via AVX2
    uint64_t CopyAVX512Bytes;       // bytes copied via AVX-512
    // Governance metrics
    uint64_t ThrottleEvents;        // total throttle engagements
    uint64_t SoftThrottleEvents;
    uint64_t HardThrottleEvents;
    // Snapshot metrics
    uint64_t SnapshotsTaken;        // total snapshots created
    uint64_t SnapshotsRestored;     // total rollbacks
    // Multi-producer metrics
    uint64_t MPTicketCount;         // total tickets issued
    uint64_t MPContentionCount;     // contention events
    // Backpressure
    uint64_t GPUBackpressure;       // current GPU queue depth signal
    uint64_t BackpressureEvents;    // times backpressure engaged
    // Security
    uint64_t SealCount;             // times arena was sealed
    uint64_t QuotaViolations;       // tenant quota refusals
    uint64_t CanaryIntact;          // 1 if canary pages untouched
    // Timing
    uint64_t LastUpdateTS;          // GetTickCount64 of last telemetry refresh
    uint64_t UptimeMs;              // ms since CoT_Initialize_Core
    uint64_t InitTimestamp;         // GetTickCount64 at init
    // Reserved
    uint64_t _reserved[16];
};

// Tenant quota entry (Phase 42)
struct CoT_TenantEntry {
    uint64_t QuotaBytes;            // max bytes this tenant may append
    uint64_t UsedBytes;             // bytes already appended
};

// ============================================================================
// Throttle level constants
// ============================================================================
enum CoT_ThrottleLevel : uint32_t {
    COT_THROTTLE_NONE = 0,
    COT_THROTTLE_SOFT = 1,
    COT_THROTTLE_HARD = 2
};

// Copy engine IDs
enum CoT_CopyEngineId : uint32_t {
    COT_COPY_ERMS   = 0,           // rep movsb (microcode-optimized)
    COT_COPY_AVX2   = 1,           // 256-bit YMM copy (256B–32KB)
    COT_COPY_AVX512 = 2            // 512-bit ZMM copy (>32KB)
};

// ============================================================================
// Phase 39: Adaptive Commit Governor
// ============================================================================

// Returns pointer to the commit governor state struct.
// Callers can read CommitRateEMA, ThrottleLevel, etc.
CoT_CommitGovernor* CoT_GetCommitGovernor(void);

// ============================================================================
// Phase 39: Copy Engine Selector
// ============================================================================

// Probe CPUID to detect ERMS, AVX2, AVX-512F capabilities.
// Populate internal dispatch table. Call once at init (cached).
// Returns: selected engine ID (CoT_CopyEngineId)
uint32_t CoT_SelectCopyEngine(void);

// Runtime-dispatched copy using the best available engine.
// Size-based routing: <256B → ERMS, 256B–32KB → AVX2, >32KB → AVX-512.
// Returns: bytes copied (= size on success)
uint64_t CoT_CopyDispatch(void* dest, const void* src, uint64_t size);

// ============================================================================
// Phase 39: Multi-Producer Mode
// ============================================================================

// Enable or disable lock-free multi-producer append path.
// Default: disabled (single-producer fast path).
// Returns: previous state (0 or 1)
uint32_t CoT_EnableMultiProducer(uint32_t enable);

// Lock-free multi-producer append using ticket reservation.
// Returns: new total arena used, or 0 on failure.
uint64_t CoT_MultiProducerAppend(const void* src, uint64_t size);

// ============================================================================
// Phase 40: Snapshot / Rollback
// ============================================================================

// O(1) snapshot — captures arena UsedLength, CommitMark, Timestamp.
// Returns: snapshot slot index (0–15), or -1 on failure.
int32_t CoT_CreateSnapshot(void);

// Rollback arena state to a previously captured snapshot.
// Safe even mid-generation. Does NOT free pages (they remain committed).
// Returns: 0 on success, -1 on invalid slot.
int32_t CoT_RestoreSnapshot(int32_t slotIndex);

// ============================================================================
// Phase 40: Deterministic Replay Mode
// ============================================================================

// Enable or disable replay mode. When enabled: append is frozen.
// Returns: previous mode (0 or 1)
uint32_t CoT_SetReplayMode(uint32_t enable);

// Check if replay mode is active.
// Returns: 1 if active, 0 otherwise.
uint32_t CoT_IsReplayMode(void);

// ============================================================================
// Phase 41: Shared-Memory Telemetry
// ============================================================================

// Get pointer to the shared telemetry struct.
// Returns: pointer (or nullptr if not initialized).
const CoT_Telemetry* CoT_GetTelemetryPage(void);

// Refresh telemetry counters from engine state.
// Lock-free. Returns: 0 (always succeeds).
int32_t CoT_UpdateTelemetry(void);

// ============================================================================
// Phase 41: GPU Backpressure Integration
// ============================================================================

// Set the GPU queue depth signal. When depth exceeds threshold,
// multi-producer appends are throttled.
// Returns: 0 (always succeeds).
int32_t CoT_SetBackpressure(uint64_t gpuQueueDepth);

// ============================================================================
// Phase 42: Memory Integrity Sealing
// ============================================================================

// Mark arena read-only after final output. Prevents corruption.
// Returns: 0 on success, -1 on failure.
int32_t CoT_SealMemory(void);

// Restore read-write access for next generation cycle.
// Returns: 0 on success, -1 on failure.
int32_t CoT_UnsealMemory(void);

// ============================================================================
// Phase 42: Tenant / Extension Quotas
// ============================================================================

// Set per-tenant byte ceiling. tenantId = 0–63, quota = 0 for unlimited.
// Returns: 0 on success, -1 on invalid tenant.
int32_t CoT_SetTenantQuota(uint32_t tenantId, uint64_t quotaBytes);

// Check remaining quota for tenant given proposed append size.
// Returns: remaining bytes after append, or -1 if would exceed.
int64_t CoT_CheckTenantQuota(uint32_t tenantId, uint64_t proposedSize);

// ============================================================================
// Existing Phase 37 exports (for completeness — also in chain_of_thought_engine.h)
// ============================================================================

// Engine version (Phase 39–42: returns 0x00020000 = 2.0.0)
uint32_t CoT_Get_Version(void);

// Arena data access
void*    CoT_Get_Data_Ptr(void);
uint64_t CoT_Get_Data_Used(void);
uint64_t CoT_Append_Data(const void* src, uint64_t size);

// DLL entry helpers
uint32_t CoT_Get_Thread_Count(void);
uint32_t CoT_TLS_GetLastError(void);
uint64_t CoT_TLS_GetFaultRIP(void);
int32_t  CoT_TLS_SetError(uint32_t errorCode, uint64_t faultRIP, uint64_t faultAddr);
uint32_t CoT_Has_Large_Pages(void);

// Lock primitives
void Acquire_CoT_Lock(void);
void Release_CoT_Lock(void);
void Acquire_CoT_Lock_Shared(void);
void Release_CoT_Lock_Shared(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // RAWRXD_COT_PHASE39_EXPORTS_H
