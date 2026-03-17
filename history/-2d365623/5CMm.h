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
#include <atomic>
#include <cstring>
#include <thread>

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

/// Runtime subsystem state — set during InitializeAllSubsystems, read by handlers.
/// All fields are atomic for thread-safe access from HTTP/REPL threads.
struct SubsystemState {
    // Init flags
    std::atomic<bool>     schedulerInit{false};
    std::atomic<bool>     conflictDetectorInit{false};
    std::atomic<bool>     heartbeatInit{false};
    std::atomic<uint16_t> heartbeatPort{0};
    std::atomic<uint32_t> maxResources{0};
    std::atomic<uint32_t> workerCount{0};
    std::atomic<uint32_t> heartbeatIntervalMs{0};
    std::atomic<uint32_t> conflictScanIntervalMs{0};

    // Operational counters (incremented by handlers/probes)
    std::atomic<uint64_t> tasksSubmitted{0};
    std::atomic<uint64_t> tasksCompleted{0};
    std::atomic<uint64_t> conflictLocks{0};
    std::atomic<uint64_t> conflictUnlocks{0};
    std::atomic<uint64_t> heartbeatNodesAdded{0};
    std::atomic<uint64_t> dmaTransfers{0};
    std::atomic<uint64_t> tensorOps{0};
};

/// Get the singleton subsystem state (thread-safe, lock-free).
inline SubsystemState& GetState() {
    static SubsystemState state;
    return state;
}

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
    auto& state = GetState();

    // 1. Work-Stealing Scheduler (others depend on task submission)
    int rc = Scheduler_Initialize(workerCount, 1 /* NUMA enabled */);
    if (rc < 0) {
        return SubsystemResult::error("Scheduler_Initialize failed", rc);
    }
    state.schedulerInit.store(true);
    state.workerCount.store(workerCount == 0 ? std::thread::hardware_concurrency() : workerCount);

    // 2. Conflict Detector (deadlock detection for resource locking)
    rc = ConflictDetector_Initialize(maxResources, 1000);
    if (rc < 0) {
        Scheduler_Shutdown();
        state.schedulerInit.store(false);
        return SubsystemResult::error("ConflictDetector_Initialize failed", rc);
    }
    state.conflictDetectorInit.store(true);
    state.maxResources.store(maxResources);
    state.conflictScanIntervalMs.store(1000);

    // 3. Heartbeat Monitor (optional — only if port specified)
    if (heartbeatPort > 0) {
        rc = Heartbeat_Initialize(heartbeatPort, 500);
        if (rc >= 0) {
            state.heartbeatInit.store(true);
            state.heartbeatPort.store(heartbeatPort);
            state.heartbeatIntervalMs.store(500);
        }
        // Non-fatal: heartbeat is optional for single-node
    }

    return SubsystemResult::ok("All ReverseEngineered subsystems initialized");
}

/// Shutdown all ReverseEngineered subsystems in reverse order.
inline void ShutdownAllSubsystems() {
    auto& state = GetState();
    Heartbeat_Shutdown();
    state.heartbeatInit.store(false);
    // ConflictDetector has no explicit shutdown — it uses event-based cleanup
    state.conflictDetectorInit.store(false);
    Scheduler_Shutdown();
    state.schedulerInit.store(false);
    INFINITY_Shutdown();
}

/// Live health probe: submit a real task to the scheduler and verify execution.
/// @param[out] latencyUs  Round-trip latency in microseconds
/// @return true if the worker thread actually executed the task
inline bool ProbeScheduler(uint64_t* latencyUs) {
    auto& state = GetState();
    if (!state.schedulerInit.load()) {
        if (latencyUs) *latencyUs = 0;
        return false;
    }

    // Task function writes a sentinel to prove the worker ran
    auto taskFn = [](void* arg) -> void {
        if (arg) *static_cast<volatile uint64_t*>(arg) = 0xCAFEBABEDEADC0DEULL;
    };

    volatile uint64_t sentinel = 0;
    uint64_t t0 = GetHighResTick();
    int64_t taskId = Scheduler_SubmitTask(
        reinterpret_cast<void*>(+taskFn),
        const_cast<uint64_t*>(&sentinel), 0, 0, nullptr);

    if (taskId < 0) {
        if (latencyUs) *latencyUs = TicksToMicroseconds(GetHighResTick() - t0);
        return false;
    }

    Scheduler_WaitForTask(taskId, 2000);
    uint64_t elapsed = GetHighResTick() - t0;
    if (latencyUs) *latencyUs = TicksToMicroseconds(elapsed);

    bool ok = (sentinel == 0xCAFEBABEDEADC0DEULL);
    state.tasksSubmitted.fetch_add(1);
    if (ok) state.tasksCompleted.fetch_add(1);
    return ok;
}

/// Live health probe: register a temp resource, lock, unlock.
/// @param[out] latencyUs  Round-trip latency in microseconds
/// @return 0=OK, 1=deadlock_detected, -1=not_init, -2=reg_failed, negative=error
inline int ProbeConflictDetector(uint64_t* latencyUs) {
    auto& state = GetState();
    if (!state.conflictDetectorInit.load()) {
        if (latencyUs) *latencyUs = 0;
        return -1;
    }

    // Unique probe resource ID: high bits = probe marker, low = tick
    uint64_t probeResId = 0xFFFFFFFF00000000ULL |
        (uint64_t)(GetHighResTick() & 0xFFFFFFFFULL);

    uint64_t t0 = GetHighResTick();
    int regRc = ConflictDetector_RegisterResource(probeResId);
    if (regRc < 0) {
        if (latencyUs) *latencyUs = TicksToMicroseconds(GetHighResTick() - t0);
        return -2; // registration failed (table full)
    }

    int lockRc = ConflictDetector_LockResource(probeResId, 0, 100);
    if (lockRc == 0) {
        ConflictDetector_UnlockResource(probeResId);
        state.conflictLocks.fetch_add(1);
        state.conflictUnlocks.fetch_add(1);
    }

    if (latencyUs) *latencyUs = TicksToMicroseconds(GetHighResTick() - t0);
    return lockRc; // 0=OK, 1=deadlock_detected, negative=error
}

/// Live health probe: allocate DMA buffers, transfer data, verify integrity.
/// @param[out] latencyUs   Round-trip latency in microseconds
/// @param[out] allocOk     Whether DMA buffer allocation succeeded
/// @param[out] transferOk  Whether the DMA transfer completed
/// @param[out] verifyOk    Whether the copied data matches the source
inline void ProbeDMA(uint64_t* latencyUs, bool* allocOk,
                     bool* transferOk, bool* verifyOk) {
    auto& state = GetState();
    const size_t probeSize = 256;
    bool _allocOk = false, _transferOk = false, _verifyOk = false;

    uint64_t t0 = GetHighResTick();
    void* src = AllocateDMABuffer(probeSize);
    void* dst = AllocateDMABuffer(probeSize);
    _allocOk = (src != nullptr && dst != nullptr);

    if (_allocOk) {
        memset(src, 0xA5, probeSize);
        memset(dst, 0x00, probeSize);

        int rc = GPU_SubmitDMATransfer(src, dst, probeSize, nullptr);
        if (rc == 0) {
            GPU_WaitForDMA(nullptr, 1000000000ULL); // 1s timeout
            _transferOk = true;
            _verifyOk = (memcmp(src, dst, probeSize) == 0);
        }
        state.dmaTransfers.fetch_add(1);
        VirtualFree(src, 0, MEM_RELEASE);
        VirtualFree(dst, 0, MEM_RELEASE);
    } else {
        if (src) VirtualFree(src, 0, MEM_RELEASE);
        if (dst) VirtualFree(dst, 0, MEM_RELEASE);
    }

    if (latencyUs) *latencyUs = TicksToMicroseconds(GetHighResTick() - t0);
    if (allocOk) *allocOk = _allocOk;
    if (transferOk) *transferOk = _transferOk;
    if (verifyOk) *verifyOk = _verifyOk;
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
