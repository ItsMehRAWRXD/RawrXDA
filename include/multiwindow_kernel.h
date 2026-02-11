/**
 * @file multiwindow_kernel.h
 * @brief C/C++ interface to the RawrXD MultiWindow Task Scheduler Kernel (MASM64 DLL)
 *
 * This header declares the exported functions, enums, and structures that match
 * the MASM64 kernel's binary layout exactly.  Include this in any C or C++
 * translation unit that links against RawrXD_MultiWindow_Kernel.lib/.dll.
 *
 * ABI: x64 __fastcall (Microsoft), no exceptions.
 * Build: MSVC 2022 / C++20
 */

#pragma once

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Task Types — matches TASK_* EQU values in ASM
 * ═══════════════════════════════════════════════════════════════════════════ */
enum MW_TASK_TYPE : uint32_t {
    MW_TASK_CHAT            = 0,
    MW_TASK_AUDIT           = 1,
    MW_TASK_COT             = 2,    /* Chain of Thought */
    MW_TASK_SWARM           = 3,
    MW_TASK_CODE_EDIT       = 4,
    MW_TASK_TERMINAL        = 5,
    MW_TASK_FILE_BROWSE     = 6,
    MW_TASK_MODEL_MANAGE    = 7,
    MW_TASK_PERF_MONITOR    = 8,
    MW_TASK_HOTPATCH        = 9,
    MW_TASK_REVERSE_ENG     = 10,
    MW_TASK_DEBUG           = 11,
    MW_TASK_BENCHMARK       = 12,
    MW_TASK_COMPILE         = 13,
    MW_TASK_DEPLOY          = 14,
    MW_TASK_CUSTOM          = 15,
    MW_MAX_TASK_TYPES       = 16
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Task States — matches STATE_* EQU values in ASM
 * ═══════════════════════════════════════════════════════════════════════════ */
enum MW_TASK_STATE : uint32_t {
    MW_STATE_IDLE           = 0,
    MW_STATE_QUEUED         = 1,
    MW_STATE_RUNNING        = 2,
    MW_STATE_PAUSED         = 3,
    MW_STATE_COMPLETE       = 4,
    MW_STATE_FAILED         = 5,
    MW_STATE_CANCELLED      = 6
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Priority Levels — matches PRIORITY_* EQU values in ASM
 * ═══════════════════════════════════════════════════════════════════════════ */
enum MW_PRIORITY : uint32_t {
    MW_PRIORITY_CRITICAL    = 0,
    MW_PRIORITY_HIGH        = 1,
    MW_PRIORITY_NORMAL      = 2,
    MW_PRIORITY_LOW         = 3,
    MW_PRIORITY_BACKGROUND  = 4,
    MW_MAX_PRIORITIES       = 5
};

/* ═══════════════════════════════════════════════════════════════════════════
 * IPC Message Types — matches MSG_* EQU values in ASM
 * ═══════════════════════════════════════════════════════════════════════════ */
enum MW_MSG_TYPE : uint32_t {
    MW_MSG_TASK_SUBMIT      = 1,
    MW_MSG_TASK_CANCEL      = 2,
    MW_MSG_TASK_STATUS      = 3,
    MW_MSG_TASK_RESULT      = 4,
    MW_MSG_WINDOW_FOCUS     = 5,
    MW_MSG_WINDOW_RESIZE    = 6,
    MW_MSG_WINDOW_CLOSE     = 7,
    MW_MSG_MODEL_SWITCH     = 8,
    MW_MSG_SWARM_BROADCAST  = 9,
    MW_MSG_COT_CHAIN        = 10,
    MW_MSG_AUDIT_REQUEST    = 11,
    MW_MSG_HOTPATCH_APPLY   = 12,
    MW_MSG_PERF_SNAPSHOT    = 13,
    MW_MSG_STREAM_CHUNK     = 14,
    MW_MSG_HEARTBEAT        = 15,
    MW_MSG_SHUTDOWN         = 16
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Replay Operations — matches WriteReplayEntry op codes
 * ═══════════════════════════════════════════════════════════════════════════ */
enum MW_REPLAY_OP : uint32_t {
    MW_REPLAY_SUBMIT        = 1,
    MW_REPLAY_DEQUEUE       = 2,
    MW_REPLAY_COMPLETE      = 3,
    MW_REPLAY_CANCEL        = 4,
    MW_REPLAY_IPC           = 5
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Handle / ID types
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef uint64_t MW_TASK_ID;
typedef uint32_t MW_WINDOW_ID;

/* ═══════════════════════════════════════════════════════════════════════════
 * Task callback signature
 * The kernel calls: int __fastcall callback(TaskDesc* descriptor)
 * Return: non-zero = success (STATE_COMPLETE), zero = failure (STATE_FAILED)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef int (__fastcall *MW_TaskCallback)(void* taskDesc);

/* ═══════════════════════════════════════════════════════════════════════════
 * Task Descriptor (128 bytes, matches ASM TASK_DESC layout)
 * ═══════════════════════════════════════════════════════════════════════════ */
#pragma pack(push, 1)
typedef struct MW_TaskDesc {
    uint64_t    task_id;            /* 0x00 */
    uint32_t    task_type;          /* 0x08 */
    uint32_t    task_state;         /* 0x0C */
    uint32_t    priority;           /* 0x10 */
    uint32_t    window_id;          /* 0x14 */
    uint64_t    callback_fn;        /* 0x18 — MW_TaskCallback */
    uint64_t    user_data;          /* 0x20 */
    uint64_t    model_id;           /* 0x28 */
    uint64_t    submit_time;        /* 0x30 — QPC tick */
    uint64_t    start_time;         /* 0x38 */
    uint64_t    end_time;           /* 0x40 */
    uint64_t    result_ptr;         /* 0x48 */
    uint32_t    result_size;        /* 0x50 */
    uint32_t    error_code;         /* 0x54 */
    uint64_t    next_task;          /* 0x58 — linked list */
    uint64_t    depends_on;         /* 0x60 — dependency task_id */
    uint64_t    progress;           /* 0x68 — 0-10000 */
    uint64_t    flags;              /* 0x70 */
    uint64_t    reserved;           /* 0x78 */
} MW_TaskDesc;
#pragma pack(pop)

static_assert(sizeof(MW_TaskDesc) == 128, "TaskDesc must be exactly 128 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Kernel Stats (matches GetKernelStats output layout)
 * ═══════════════════════════════════════════════════════════════════════════ */
#pragma pack(push, 1)
typedef struct MW_KernelStats {
    uint32_t    activeWindows;              /* 0x00 */
    uint32_t    activeTasks;                /* 0x04 */
    uint64_t    totalProcessed;             /* 0x08 */
    uint64_t    totalFailed;                /* 0x10 */
    uint64_t    uptimeMs;                   /* 0x18 */
    uint32_t    workerCount;                /* 0x20 */
    uint32_t    queueDepth[5];              /* 0x24 — one per priority */
    uint64_t    reserved;                   /* 0x38 */
} MW_KernelStats;
#pragma pack(pop)

static_assert(sizeof(MW_KernelStats) == 64, "KernelStats must be exactly 64 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Replay Log Entry (64 bytes, matches ASM layout in shared memory)
 * ═══════════════════════════════════════════════════════════════════════════ */
#pragma pack(push, 1)
typedef struct MW_ReplayEntry {
    uint64_t    timestamp;          /* 0x00 — QPC tick */
    uint32_t    operation;          /* 0x08 — MW_REPLAY_OP */
    uint32_t    priority;           /* 0x0C */
    uint64_t    task_id;            /* 0x10 */
    uint32_t    head_before;        /* 0x18 */
    uint32_t    tail_before;        /* 0x1C */
    uint32_t    count_before;       /* 0x20 */
    uint32_t    thread_id;          /* 0x24 */
    uint8_t     reserved[24];       /* 0x28 */
} MW_ReplayEntry;
#pragma pack(pop)

static_assert(sizeof(MW_ReplayEntry) == 64, "ReplayEntry must be exactly 64 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Ring Buffer Constants (must match ASM EQU values)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MW_RING_BUFFER_SIZE     4096
#define MW_RING_ENTRY_SIZE      256
#define MW_MAX_WINDOWS          64
#define MW_MAX_TASKS            1024
#define MW_MAX_WORKER_THREADS   16
#define MW_IPC_SHARED_SIZE      1048576

/* Shared memory layout offsets */
#define MW_SHM_QUEUE_STRIDE     (MW_RING_BUFFER_SIZE * 8)         /* 32768 */
#define MW_SHM_QUEUE_REGION     (MW_MAX_PRIORITIES * MW_SHM_QUEUE_STRIDE) /* 163840 */
#define MW_SHM_REPLAY_BASE      MW_SHM_QUEUE_REGION
#define MW_SHM_REPLAY_ENTRY_SZ  64
#define MW_SHM_REPLAY_MAX       8192
#define MW_SHM_REPLAY_SIZE      (MW_SHM_REPLAY_MAX * MW_SHM_REPLAY_ENTRY_SZ) /* 524288 */

/* ═══════════════════════════════════════════════════════════════════════════
 * Kernel API — Imported from RawrXD_MultiWindow_Kernel.dll
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * For static linking:  #define MW_API
 * For DLL import:      #define MW_API __declspec(dllimport)   (default)
 * For DLL export:      #define MW_API __declspec(dllexport)
 */

#ifndef MW_API
#define MW_API __declspec(dllimport)
#endif

/* --- Lifecycle --- */

MW_API BOOL __fastcall KernelInit(uint32_t workerCount);
MW_API void __fastcall KernelShutdown(void);

/* --- Task Management --- */

MW_API MW_TASK_ID __fastcall SubmitTask(
    uint32_t        taskType,       /* MW_TASK_TYPE */
    uint32_t        priority,       /* MW_PRIORITY */
    MW_WINDOW_ID    windowId,
    MW_TaskCallback callback,
    void*           userData,
    uint64_t        modelId,
    MW_TASK_ID      dependsOn
);

MW_API BOOL __fastcall CancelTask(MW_TASK_ID taskId);

/* --- Window Management --- */

MW_API MW_WINDOW_ID __fastcall RegisterWindow(
    uint32_t windowType,
    int32_t  posX,
    int32_t  posY,
    uint32_t width,
    uint32_t height
);

MW_API BOOL __fastcall UnregisterWindow(MW_WINDOW_ID windowId);

/* --- IPC --- */

MW_API BOOL __fastcall SendIPCMessage(
    uint32_t        msgType,        /* MW_MSG_TYPE */
    MW_WINDOW_ID    srcWindow,
    MW_WINDOW_ID    dstWindow,      /* 0 = broadcast */
    const void*     payload,
    uint32_t        payloadSize
);

/* --- Swarm / CoT --- */

MW_API uint32_t __fastcall SwarmBroadcast(
    uint32_t    taskType,
    const void* payload,
    uint32_t    payloadSize,
    uint32_t    modelCount
);

MW_API MW_TASK_ID __fastcall ChainOfThought(
    MW_WINDOW_ID    windowId,
    uint32_t        stepCount,
    MW_TaskCallback* callbacks     /* array of stepCount function pointers */
);

/* --- Stats --- */

MW_API void __fastcall GetKernelStats(MW_KernelStats* out);

/* --- Timing --- */

MW_API uint64_t __fastcall GetMicroseconds(void);

/* --- Memory Barriers --- */

MW_API void __fastcall MemoryFence(void);
MW_API void __fastcall LoadFence(void);
MW_API void __fastcall StoreFence(void);

/* --- Dependency Query --- */

MW_API BOOL __fastcall IsTaskComplete(MW_TASK_ID taskId);

/* --- Replay Logging --- */

MW_API void __fastcall WriteReplayEntry(
    uint32_t    operation,          /* MW_REPLAY_OP */
    uint32_t    priority,
    MW_TASK_ID  taskId
);

#ifdef __cplusplus
} /* extern "C" */
#endif
