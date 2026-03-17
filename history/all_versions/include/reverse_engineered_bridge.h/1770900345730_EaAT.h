// ============================================================================
// reverse_engineered_bridge.h — C++ Bridge for RawrXD_Complete_ReverseEngineered.asm
// ============================================================================
// Authoritative extern "C" prototypes for all PUBLIC exports from the
// INFINITY I/O, Work-Stealing Scheduler, Conflict Detector, Heartbeat
// Monitor, GPU DMA, and Tensor Operations subsystems.
//
// Pattern: PatchResult-style, no exceptions, no STL in extern "C" boundary.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdint>
#include <cstddef>

// ============================================================================
// Subsystem Status Constants
// ============================================================================

enum InfinityStreamStatus : uint32_t {
    INFINITY_STATUS_IDLE     = 0,
    INFINITY_STATUS_ACTIVE   = 1,
    INFINITY_STATUS_DRAINING = 2,
    INFINITY_STATUS_ERROR    = 3
};

enum HeartbeatNodeStatus : uint32_t {
    HB_NODE_UNKNOWN  = 0,
    HB_NODE_HEALTHY  = 1,
    HB_NODE_SUSPECT  = 2,
    HB_NODE_FAILED   = 3
};

enum TaskState : uint32_t {
    TASK_STATE_PENDING    = 0,
    TASK_STATE_RUNNING    = 1,
    TASK_STATE_COMPLETED  = 2,
    TASK_STATE_FAILED     = 3
};

enum ResourceLockState : uint32_t {
    RESOURCE_UNLOCKED = 0,
    RESOURCE_LOCKED   = 1,
    RESOURCE_DEADLOCK = 2
};

// ============================================================================
// Quantized Tensor Types (matching MASM constants)
// ============================================================================

enum QuantizedType : uint32_t {
    QUANT_Q2_K  = 0,
    QUANT_Q4_0  = 1,
    QUANT_Q8_0  = 2,
    QUANT_F16   = 3,
    QUANT_F32   = 4
};

// ============================================================================
// EXTERN "C" PROTOTYPES — RawrXD_Complete_ReverseEngineered.asm
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// INFINITY I/O Streaming Engine — Quad-buffered async I/O with IOCP
// --------------------------------------------------------------------------

/// Initialize the INFINITY stream engine for a GGUF model file.
/// @param filePath   Wide-char path to the .gguf file
/// @param pathLen    Length of filePath in characters
/// @param layerSize  Size of each model layer in bytes
/// @return           0 on success, negative on failure
int     INFINITY_InitializeStream(const wchar_t* filePath,
                                  uint32_t pathLen,
                                  uint64_t layerSize);

/// Check quad buffer status and trigger rotation if needed.
/// @return  Bitmap: bit0=buffer_ready, bit1=rotation_needed
int     INFINITY_CheckQuadBuffer(void);

/// Rotate the quad buffer ring (advance read/write cursors).
/// @return  0 on success, negative on error
int     INFINITY_RotateBuffers(void);

/// Handle a YTfnTrap (asynchronous I/O completion trap).
/// @param trapCode  The trap code from IOCP completion
/// @param context   Opaque context from the completion packet
/// @return          0 on success, negative on error
int     INFINITY_HandleYTfnTrap(uint32_t trapCode, void* context);

/// Release a specific quad buffer slot.
/// @param slotIndex  Buffer slot to release (0-3)
/// @return           0 on success, negative on error
int     INFINITY_ReleaseBuffer(uint32_t slotIndex);

/// Shut down the INFINITY stream engine. Cancels all pending I/O.
void    INFINITY_Shutdown(void);

// --------------------------------------------------------------------------
// Work-Stealing Task Scheduler — NUMA-aware with priority ordering
// --------------------------------------------------------------------------

/// Initialize the task scheduler with NUMA-aware worker threads.
/// @param workerCount  Number of worker threads (0 = auto-detect from cores)
/// @param numaEnabled  If nonzero, enable NUMA-local affinity binding
/// @return             0 on success, negative on failure
int     Scheduler_Initialize(uint32_t workerCount, uint32_t numaEnabled);

/// Submit a task to the scheduler's global priority queue.
/// @param taskFn     Function pointer: void (*)(void* arg)
/// @param arg        Argument passed to taskFn
/// @param priority   Priority level (higher = sooner, 0 = default)
/// @param depCount   Number of dependency task IDs
/// @param depIds     Array of task IDs this task depends on
/// @return           Positive task_id on success, negative on failure
int64_t Scheduler_SubmitTask(void* taskFn, void* arg,
                             uint32_t priority,
                             uint32_t depCount, const int64_t* depIds);

/// Wait for a task to complete.
/// @param taskId     Task ID returned by Scheduler_SubmitTask
/// @param timeoutMs  Maximum wait time in milliseconds
/// @return           Task result pointer on success, NULL on timeout/failure
void*   Scheduler_WaitForTask(int64_t taskId, uint32_t timeoutMs);

/// Gracefully shut down the scheduler. Signals all workers and waits.
void    Scheduler_Shutdown(void);

// --------------------------------------------------------------------------
// Conflict Detector — Wait-for Graph with DFS Deadlock Detection
// --------------------------------------------------------------------------

/// Initialize the conflict detection subsystem.
/// @param maxResources  Maximum number of trackable resources
/// @param checkIntervalMs  Deadlock scan interval in ms (default 1000)
/// @return              0 on success, negative on failure
int     ConflictDetector_Initialize(uint32_t maxResources,
                                    uint32_t checkIntervalMs);

/// Register a named resource for conflict tracking.
/// @param resourceId   Unique identifier for the resource
/// @return             0 on success, negative on duplicate/full
int     ConflictDetector_RegisterResource(uint64_t resourceId);

/// Acquire a lock on a resource with deadlock detection.
/// @param resourceId   Resource to lock
/// @param taskId       ID of the requesting task (from Scheduler)
/// @param timeoutMs    Maximum wait time
/// @return             0 on success, 1 if deadlock detected, negative on error
int     ConflictDetector_LockResource(uint64_t resourceId,
                                      int64_t taskId,
                                      uint32_t timeoutMs);

/// Release a previously acquired resource lock.
/// @param resourceId   Resource to unlock
/// @return             0 on success, negative on error
int     ConflictDetector_UnlockResource(uint64_t resourceId);

// --------------------------------------------------------------------------
// Heartbeat Monitor — UDP Gossip Protocol for Distributed Health
// --------------------------------------------------------------------------

/// Initialize the heartbeat monitor.
/// @param listenPort   UDP port for heartbeat recv
/// @param sendIntervalMs  Interval between heartbeat sends (default 500)
/// @return              0 on success, negative on failure
int     Heartbeat_Initialize(uint16_t listenPort, uint32_t sendIntervalMs);

/// Add a remote node to the heartbeat monitor.
/// @param nodeId       Unique identifier for the node
/// @param ipAddr       IPv4 address string (e.g., "192.168.1.100")
/// @param port         UDP port of the remote node
/// @return             0 on success, negative on full/duplicate
int     Heartbeat_AddNode(uint32_t nodeId, const char* ipAddr, uint16_t port);

/// Shut down the heartbeat monitor. Closes socket, terminates threads.
void    Heartbeat_Shutdown(void);

// --------------------------------------------------------------------------
// GPU DMA Transfer Engine — Vulkan-style fence-based transfers
// --------------------------------------------------------------------------

/// Submit a DMA transfer to/from GPU memory.
/// @param srcAddr  Source address (CPU or GPU mapped)
/// @param dstAddr  Destination address
/// @param size     Transfer size in bytes
/// @param slotPtr  Quad buffer slot pointer for fence tracking
/// @return         0 on success, negative on failure
int     GPU_SubmitDMATransfer(void* srcAddr, void* dstAddr,
                              uint64_t size, void* slotPtr);

/// Wait for a DMA transfer to complete.
/// @param slotPtr    Quad buffer slot with fence
/// @param timeoutNs  Timeout in nanoseconds
/// @return           1 on success, 0 on timeout
int     GPU_WaitForDMA(void* slotPtr, uint64_t timeoutNs);

// --------------------------------------------------------------------------
// Tensor Operations — AVX-512/AVX2 Quantized Matrix Multiplication
// --------------------------------------------------------------------------

/// Quantized matrix multiplication: C = A * B
/// @param A           Pointer to quantized matrix A [M x K]
/// @param B           Pointer to quantized matrix B [K x N]
/// @param C           Pointer to output matrix C [M x N] (float32)
/// @param M           Rows of A / rows of C
/// @param N           Columns of B / columns of C
/// @param K           Columns of A / rows of B
/// @param quantType   Quantization type (see QuantizedType enum)
void    Tensor_QuantizedMatMul(const void* A, const void* B, float* C,
                               uint32_t M, uint32_t N, uint32_t K,
                               uint32_t quantType);

// --------------------------------------------------------------------------
// Utility Functions
// --------------------------------------------------------------------------

/// High-resolution timestamp (QueryPerformanceCounter wrapper).
/// @return  Raw tick count
uint64_t GetHighResTick(void);

/// Convert QPC ticks to microseconds.
/// @param ticks  Raw tick value from GetHighResTick
/// @return       Microseconds
uint64_t TicksToMicroseconds(uint64_t ticks);

/// Convert QPC ticks to milliseconds.
/// @param ticks  Raw tick value from GetHighResTick
/// @return       Milliseconds
uint64_t TicksToMilliseconds(uint64_t ticks);

/// Compute CRC32 using hardware SSE4.2 instructions.
/// @param data  Pointer to data buffer
/// @param len   Length of data in bytes
/// @return      CRC32 checksum
uint32_t CalculateCRC32(const void* data, uint64_t len);

/// Allocate a page-aligned DMA buffer via VirtualAlloc.
/// @param size  Buffer size in bytes
/// @return      Pointer to allocated buffer, NULL on failure
void*   AllocateDMABuffer(uint64_t size);

/// Lock INFINITY stream status (SRW exclusive).
void    Infinity_LockStatusExclusive(void);

/// Unlock INFINITY stream status (SRW exclusive).
void    Infinity_UnlockStatusExclusive(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Convenience Wrappers
// ============================================================================

#ifdef __cplusplus

namespace RawrXD {
namespace ReverseEngineered {

/// Subsystem initialization result
struct SubsystemResult {
    bool success;
    const char* detail;
    int errorCode;

    static SubsystemResult ok(const char* msg = "OK") {
        return {true, msg, 0};
    }
    static SubsystemResult error(const char* msg, int code = -1) {
        return {false, msg, code};
    }
};

/// Initialize all ReverseEngineered MASM subsystems in correct dependency order.
/// Call AFTER Tier-2 MASM subsystems (SelfPatch, GGUF Loader, etc.).
///
/// Order: Scheduler → ConflictDetector → Heartbeat → INFINITY
///
/// @param workerCount     Worker threads (0 = auto)
/// @param heartbeatPort   UDP port for heartbeat (0 = disabled)
/// @param maxResources    Max conflict-tracked resources
/// @return                SubsystemResult with aggregate status
inline SubsystemResult InitializeAllSubsystems(
    uint32_t workerCount = 0,
    uint16_t heartbeatPort = 0,
    uint32_t maxResources = 256)
{
    // 1. Work-Stealing Scheduler (others depend on task submission)
    int rc = Scheduler_Initialize(workerCount, 1 /* NUMA enabled */);
    if (rc < 0) {
        return SubsystemResult::error("Scheduler_Initialize failed", rc);
    }

    // 2. Conflict Detector (deadlock detection for resource locking)
    rc = ConflictDetector_Initialize(maxResources, 1000);
    if (rc < 0) {
        Scheduler_Shutdown();
        return SubsystemResult::error("ConflictDetector_Initialize failed", rc);
    }

    // 3. Heartbeat Monitor (optional — only if port specified)
    if (heartbeatPort > 0) {
        rc = Heartbeat_Initialize(heartbeatPort, 500);
        if (rc < 0) {
            // Non-fatal: heartbeat is optional for single-node
            // Continue without it
        }
    }

    return SubsystemResult::ok("All ReverseEngineered subsystems initialized");
}

/// Shutdown all ReverseEngineered subsystems in reverse order.
inline void ShutdownAllSubsystems() {
    Heartbeat_Shutdown();
    // ConflictDetector has no explicit shutdown — it uses event-based cleanup
    Scheduler_Shutdown();
    INFINITY_Shutdown();
}

/// Submit a task and wait for its result.
/// @param fn         Task function
/// @param arg        Task argument
/// @param timeoutMs  Max wait time
/// @return           Task result pointer (NULL on timeout)
inline void* SubmitAndWait(void* fn, void* arg, uint32_t timeoutMs = 30000) {
    int64_t id = Scheduler_SubmitTask(fn, arg, 0, 0, nullptr);
    if (id < 0) return nullptr;
    return Scheduler_WaitForTask(id, timeoutMs);
}

/// High-resolution timer scope for performance measurement.
struct ScopedTimer {
    uint64_t start;
    uint64_t* out_us;

    explicit ScopedTimer(uint64_t* microseconds_out)
        : start(GetHighResTick()), out_us(microseconds_out) {}

    ~ScopedTimer() {
        if (out_us) {
            uint64_t elapsed = GetHighResTick() - start;
            *out_us = TicksToMicroseconds(elapsed);
        }
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};

} // namespace ReverseEngineered
} // namespace RawrXD

#endif // __cplusplus

#endif // REVERSE_ENGINEERED_BRIDGE_H
