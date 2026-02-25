/*==============================================================================
 * Week1_API.h
 * RawrXD Week 1 Infrastructure - C/C++ Interface Header
 * 
 * Exposes the pure MASM64 Week 1 Background Thread Infrastructure to C/C++
 * callers. All functions use the standard x64 Windows calling convention.
 *
 * Components:
 *   - Heartbeat Monitor   - Distributed node health tracking
 *   - Conflict Detector   - Deadlock detection via wait graph analysis
 *   - Task Scheduler      - Work-stealing parallel task execution
 *   - Thread Management   - Worker thread creation and lifecycle
 *
 * Copyright (c) 2024 RawrXD Project
 * All rights reserved.
 *============================================================================*/

#ifndef WEEK1_API_H
#define WEEK1_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*==============================================================================
 * Build Configuration
 *============================================================================*/
#ifdef WEEK1_EXPORTS
    #define WEEK1_API __declspec(dllexport)
#else
    #define WEEK1_API __declspec(dllimport)
#endif

/* For static library usage */
#ifdef WEEK1_STATIC
    #undef WEEK1_API
    #define WEEK1_API
#endif

/*==============================================================================
 * Constants
 *============================================================================*/

/* Heartbeat Configuration */
#define WEEK1_HB_INTERVAL_MS        100     /* Check interval in ms */
#define WEEK1_HB_TIMEOUT_MS         500     /* Timeout threshold in ms */
#define WEEK1_HB_MAX_MISSES         3       /* Misses before DEAD state */
#define WEEK1_HB_MAX_NODES          128     /* Maximum tracked nodes */

/* Conflict Detection */
#define WEEK1_CD_SCAN_INTERVAL_MS   50      /* Scan interval in ms */
#define WEEK1_CD_MAX_RESOURCES      1024    /* Maximum tracked resources */
#define WEEK1_CD_MAX_WAIT_EDGES     4096    /* Maximum wait graph edges */

/* Task Scheduler */
#define WEEK1_TS_MAX_WORKERS        64      /* Maximum worker threads */
#define WEEK1_TS_GLOBAL_QUEUE_SIZE  10000   /* Global queue capacity */
#define WEEK1_TS_LOCAL_QUEUE_SIZE   256     /* Per-worker queue size */
#define WEEK1_TS_MAX_TASKS          100000  /* Maximum total tasks */

/* Node States */
typedef enum Week1NodeState {
    WEEK1_NODE_STATE_HEALTHY    = 0,
    WEEK1_NODE_STATE_SUSPECT    = 1,
    WEEK1_NODE_STATE_DEAD       = 2
} Week1NodeState;

/* Task States */
typedef enum Week1TaskState {
    WEEK1_TASK_STATE_PENDING    = 0,
    WEEK1_TASK_STATE_RUNNING    = 1,
    WEEK1_TASK_STATE_COMPLETE   = 2,
    WEEK1_TASK_STATE_CANCELLED  = 3
} Week1TaskState;

/* Error Codes */
typedef enum Week1Error {
    WEEK1_SUCCESS               = 0,
    WEEK1_ERROR_INVALID_PARAM   = 0x57,     /* ERROR_INVALID_PARAMETER */
    WEEK1_ERROR_OUT_OF_MEMORY   = 0x0E,     /* ERROR_OUTOFMEMORY */
    WEEK1_ERROR_TIMEOUT         = 0x5B4,    /* ERROR_TIMEOUT */
    WEEK1_ERROR_ALREADY_INIT    = 1,
    WEEK1_ERROR_NOT_INIT        = 2,
    WEEK1_ERROR_QUEUE_FULL      = 3,
    WEEK1_ERROR_NODE_NOT_FOUND  = 4,
    WEEK1_ERROR_RESOURCE_NOT_FOUND = 5
} Week1Error;

/*==============================================================================
 * Opaque Handle Types
 *============================================================================*/

/* Main infrastructure handle - opaque pointer to WEEK1_INFRASTRUCTURE */
typedef struct Week1Infrastructure* Week1Handle;

/* Task handle - opaque pointer to TASK structure */
typedef struct Week1Task* Week1TaskHandle;

/*==============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Node state change callback
 * @param context   User-provided context pointer
 * @param nodeId    ID of the node whose state changed
 * @param oldState  Previous state (Week1NodeState)
 * @param newState  New state (Week1NodeState)
 */
typedef void (*Week1NodeStateCallback)(
    void*           context,
    uint32_t        nodeId,
    Week1NodeState  oldState,
    Week1NodeState  newState
);

/**
 * Task function signature
 * @param context   User-provided task context
 */
typedef void (*Week1TaskFunction)(void* context);

/**
 * Deadlock notification callback
 * @param context       User-provided context
 * @param threadIds     Array of thread IDs in the cycle
 * @param threadCount   Number of threads in the cycle
 */
typedef void (*Week1DeadlockCallback)(
    void*           context,
    const uint32_t* threadIds,
    uint32_t        threadCount
);

/*==============================================================================
 * Structure Definitions (for interop)
 *============================================================================*/

#pragma pack(push, 8)

/**
 * Task descriptor for task submission
 * Must match TASK structure in WEEK1_COMPLETE.asm (128 bytes)
 */
typedef struct Week1TaskDescriptor {
    uint64_t            taskId;         /* Assigned by scheduler */
    uint32_t            taskType;       /* User-defined type */
    uint32_t            priority;       /* 0-15 (0 = highest) */
    Week1TaskFunction   taskFunction;   /* Function to execute */
    void*               context;        /* User context */
    void*               resultBuffer;   /* Result storage (optional) */
    Week1TaskState      state;          /* Current state */
    uint32_t            workerThreadId; /* Executing worker */
    uint64_t            submitTime;     /* QPC timestamp */
    uint64_t            startTime;      /* QPC timestamp */
    uint64_t            completeTime;   /* QPC timestamp */
    void*               nextTask;       /* Internal linked list */
    void*               prevTask;       /* Internal linked list */
    uint32_t            stealCount;     /* Times stolen */
    uint32_t            retryCount;     /* Retry attempts */
    uint8_t             _padding[16];   /* Align to 128 bytes */
} Week1TaskDescriptor;

/**
 * Statistics snapshot
 */
typedef struct Week1Statistics {
    uint64_t    totalTasksSubmitted;
    uint64_t    totalTasksExecuted;
    uint64_t    totalTasksStolen;
    uint32_t    workerCount;
    uint32_t    globalQueueDepth;
    uint64_t    totalHeartbeatsSent;
    uint64_t    totalHeartbeatsReceived;
    uint64_t    totalDeadlocksDetected;
} Week1Statistics;

/**
 * Node registration info
 */
typedef struct Week1NodeInfo {
    uint32_t                nodeId;
    uint32_t                ipAddress;      /* IPv4 in network byte order */
    uint16_t                port;
    uint16_t                _padding;
    Week1NodeStateCallback  callback;
    void*                   callbackContext;
} Week1NodeInfo;

#pragma pack(pop)

/*==============================================================================
 * Core Lifecycle Functions
 *============================================================================*/

/**
 * Initialize the Week 1 infrastructure
 * Allocates all subsystems but does not start threads.
 *
 * @param ppHandle  [out] Receives the infrastructure handle
 * @return          WEEK1_SUCCESS on success, error code on failure
 *
 * @note Call Week1StartBackgroundThreads() to begin operation
 */
WEEK1_API int __cdecl Week1Initialize(Week1Handle* ppHandle);

/**
 * Start all background monitoring threads
 * Creates heartbeat monitor, conflict detector, coordinator, and worker threads.
 *
 * @param hHandle   Infrastructure handle from Week1Initialize
 * @return          WEEK1_SUCCESS on success, error code on failure
 *
 * @note Worker count is automatically set to CPU core count (max 64)
 */
WEEK1_API int __cdecl Week1StartBackgroundThreads(Week1Handle hHandle);

/**
 * Gracefully shutdown infrastructure
 * Signals all threads, waits for completion, frees all resources.
 *
 * @param hHandle   Infrastructure handle (becomes invalid after call)
 *
 * @note Blocks until all threads have terminated (up to 3 second timeout)
 */
WEEK1_API void __cdecl Week1Shutdown(Week1Handle hHandle);

/*==============================================================================
 * Heartbeat Monitor Functions
 *============================================================================*/

/**
 * Register a node for heartbeat monitoring
 *
 * @param hHandle   Infrastructure handle
 * @param nodeId    Unique node identifier
 * @param ipAddress IPv4 address (network byte order)
 * @param port      UDP port
 * @param callback  Optional state change callback
 * @param context   Optional callback context
 * @return          1 on success, 0 on failure
 */
WEEK1_API int __cdecl Week1RegisterNode(
    Week1Handle             hHandle,
    uint32_t                nodeId,
    uint32_t                ipAddress,
    uint16_t                port,
    Week1NodeStateCallback  callback,
    void*                   context
);

/**
 * Process a received heartbeat from a node
 * Updates node state to HEALTHY and resets miss counter.
 *
 * @param hHandle       Infrastructure handle
 * @param nodeId        Node that sent the heartbeat
 * @param timestamp     QPC timestamp when received
 * @param latencyNs     Measured round-trip latency in nanoseconds
 * @return              1 on success, 0 if node not found
 */
WEEK1_API int __cdecl ProcessReceivedHeartbeat(
    Week1Handle hHandle,
    uint32_t    nodeId,
    uint64_t    timestamp,
    uint64_t    latencyNs
);

/**
 * Get node state
 *
 * @param hHandle   Infrastructure handle
 * @param nodeId    Node identifier
 * @param pState    [out] Receives current state
 * @return          1 on success, 0 if node not found
 */
WEEK1_API int __cdecl Week1GetNodeState(
    Week1Handle     hHandle,
    uint32_t        nodeId,
    Week1NodeState* pState
);

/*==============================================================================
 * Conflict Detection Functions
 *============================================================================*/

/**
 * Register a resource for conflict tracking
 *
 * @param hHandle       Infrastructure handle
 * @param resourceId    Unique resource identifier
 * @param name          Optional debug name (max 63 chars)
 * @return              1 on success, 0 on failure
 */
WEEK1_API int __cdecl Week1RegisterResource(
    Week1Handle hHandle,
    uint64_t    resourceId,
    const char* name
);

/**
 * Report resource acquisition
 * Call when a thread acquires a resource.
 *
 * @param hHandle       Infrastructure handle
 * @param resourceId    Resource being acquired
 * @param threadId      Thread acquiring (0 = current thread)
 * @return              1 on success, 0 on failure
 */
WEEK1_API int __cdecl Week1AcquireResource(
    Week1Handle hHandle,
    uint64_t    resourceId,
    uint32_t    threadId
);

/**
 * Report resource release
 * Call when a thread releases a resource.
 *
 * @param hHandle       Infrastructure handle
 * @param resourceId    Resource being released
 * @return              1 on success, 0 on failure
 */
WEEK1_API int __cdecl Week1ReleaseResource(
    Week1Handle hHandle,
    uint64_t    resourceId
);

/**
 * Report resource wait (potential deadlock)
 * Call when a thread begins waiting for a resource.
 *
 * @param hHandle       Infrastructure handle
 * @param resourceId    Resource being waited for
 * @param threadId      Waiting thread (0 = current thread)
 * @return              1 on success, 0 on failure
 */
WEEK1_API int __cdecl Week1WaitForResource(
    Week1Handle hHandle,
    uint64_t    resourceId,
    uint32_t    threadId
);

/**
 * Set deadlock notification callback
 *
 * @param hHandle   Infrastructure handle
 * @param callback  Callback function
 * @param context   User context
 */
WEEK1_API void __cdecl Week1SetDeadlockCallback(
    Week1Handle             hHandle,
    Week1DeadlockCallback   callback,
    void*                   context
);

/*==============================================================================
 * Task Scheduler Functions
 *============================================================================*/

/**
 * Submit a task to the global queue
 *
 * @param hHandle   Infrastructure handle
 * @param pTask     Task descriptor (taskFunction and context are required)
 * @return          1 on success, 0 if queue full or invalid parameters
 *
 * @note The task's taskId, submitTime, and state will be set by the scheduler
 */
WEEK1_API int __cdecl SubmitTask(
    Week1Handle         hHandle,
    Week1TaskDescriptor* pTask
);

/**
 * Submit a simple task (convenience wrapper)
 *
 * @param hHandle       Infrastructure handle
 * @param taskFunc      Function to execute
 * @param context       User context passed to function
 * @param priority      Task priority (0-15, 0 = highest)
 * @return              Task ID on success, 0 on failure
 */
WEEK1_API uint64_t __cdecl Week1SubmitSimpleTask(
    Week1Handle         hHandle,
    Week1TaskFunction   taskFunc,
    void*               context,
    uint32_t            priority
);

/**
 * Wait for a task to complete
 *
 * @param hHandle   Infrastructure handle
 * @param taskId    Task ID from submission
 * @param timeoutMs Maximum wait time in milliseconds (INFINITE = forever)
 * @return          WEEK1_SUCCESS, WEEK1_ERROR_TIMEOUT, or error code
 */
WEEK1_API int __cdecl Week1WaitForTask(
    Week1Handle hHandle,
    uint64_t    taskId,
    uint32_t    timeoutMs
);

/**
 * Cancel a pending task
 *
 * @param hHandle   Infrastructure handle
 * @param taskId    Task ID to cancel
 * @return          1 if cancelled, 0 if not found or already running/complete
 */
WEEK1_API int __cdecl Week1CancelTask(
    Week1Handle hHandle,
    uint64_t    taskId
);

/*==============================================================================
 * Statistics and Diagnostics
 *============================================================================*/

/**
 * Get current statistics
 *
 * @param hHandle   Infrastructure handle
 * @param pStats    [out] Statistics structure to fill
 */
WEEK1_API void __cdecl Week1GetStatistics(
    Week1Handle         hHandle,
    Week1Statistics*    pStats
);

/**
 * Get version string
 *
 * @return  Static version string (e.g., "1.0.0")
 */
WEEK1_API const char* __cdecl Week1GetVersion(void);

/**
 * Get build timestamp
 *
 * @return  Static build timestamp string
 */
WEEK1_API const char* __cdecl Week1GetBuildTime(void);

/**
 * Get worker thread count
 *
 * @param hHandle   Infrastructure handle
 * @return          Number of active worker threads
 */
WEEK1_API uint32_t __cdecl Week1GetWorkerCount(Week1Handle hHandle);

/**
 * Get global queue depth
 *
 * @param hHandle   Infrastructure handle
 * @return          Number of tasks in global queue
 */
WEEK1_API uint32_t __cdecl Week1GetQueueDepth(Week1Handle hHandle);

/*==============================================================================
 * C++ Convenience Wrappers
 *============================================================================*/

#ifdef __cplusplus

namespace Week1 {

/**
 * RAII wrapper for Week1 infrastructure
 */
class Infrastructure {
public:
    Infrastructure() : m_handle(nullptr) {
        Week1Initialize(&m_handle);
    }
    
    ~Infrastructure() {
        if (m_handle) {
            Week1Shutdown(m_handle);
        }
    }
    
    // Non-copyable
    Infrastructure(const Infrastructure&) = delete;
    Infrastructure& operator=(const Infrastructure&) = delete;
    
    // Movable
    Infrastructure(Infrastructure&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    
    Infrastructure& operator=(Infrastructure&& other) noexcept {
        if (this != &other) {
            if (m_handle) Week1Shutdown(m_handle);
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    
    int Start() { return Week1StartBackgroundThreads(m_handle); }
    
    Week1Handle Handle() const { return m_handle; }
    operator Week1Handle() const { return m_handle; }
    explicit operator bool() const { return m_handle != nullptr; }
    
private:
    Week1Handle m_handle;
};

/**
 * RAII resource lock tracker
 */
class ResourceLock {
public:
    ResourceLock(Week1Handle handle, uint64_t resourceId)
        : m_handle(handle), m_resourceId(resourceId) {
        Week1AcquireResource(m_handle, m_resourceId, 0);
    }
    
    ~ResourceLock() {
        Week1ReleaseResource(m_handle, m_resourceId);
    }
    
    // Non-copyable, non-movable
    ResourceLock(const ResourceLock&) = delete;
    ResourceLock& operator=(const ResourceLock&) = delete;
    
private:
    Week1Handle m_handle;
    uint64_t m_resourceId;
};

} // namespace Week1

#endif /* __cplusplus */

#ifdef __cplusplus
}
#endif

#endif /* WEEK1_API_H */
