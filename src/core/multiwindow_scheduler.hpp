/**
 * @file multiwindow_scheduler.hpp
 * @brief Production-grade C++20 RAII wrapper for the MASM64 MultiWindow Kernel
 *
 * Provides type-safe task submission, window management, swarm orchestration,
 * CoT pipelines, and replay log access.  No Qt, no exceptions in hot paths.
 *
 * Callbacks are raw function pointers (MW_TaskCallback) per project convention.
 * The scheduler can load the DLL dynamically or link statically.
 *
 * Thread safety: All public methods are thread-safe via internal mutex.
 */

#pragma once

#include "multiwindow_kernel.h"

// Win32 macros conflict with our method names
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateWindowW
#undef CreateWindowW
#endif
#ifdef DestroyWindow
#undef DestroyWindow
#endif

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <cstdio>
#include <cstring>

namespace RawrXD {

/* ═══════════════════════════════════════════════════════════════════════════
 * Window descriptor (C++ side bookkeeping)
 * ═══════════════════════════════════════════════════════════════════════════ */
struct Window {
    MW_WINDOW_ID    id      = 0;
    uint32_t        type    = 0;
    std::string     title;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Task submission options
 * ═══════════════════════════════════════════════════════════════════════════ */
struct TaskOptions {
    MW_TASK_TYPE    type        = MW_TASK_CHAT;
    MW_PRIORITY     priority   = MW_PRIORITY_NORMAL;
    MW_WINDOW_ID    windowId   = 0;
    uint64_t        modelId    = 0;
    MW_TASK_ID      dependsOn  = 0;
    MW_TaskCallback callback   = nullptr;
    void*           userData   = nullptr;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Callback typedefs (function pointers, not std::function)
 * ═══════════════════════════════════════════════════════════════════════════ */

/// Called when a task completes or fails.   args: task_id, success
typedef void (*TaskCompletionCb)(MW_TASK_ID taskId, bool success);

/// Called when kernel stats are polled.     args: stats ptr
typedef void (*StatsPollCb)(const MW_KernelStats* stats);

/* ═══════════════════════════════════════════════════════════════════════════
 * MultiWindowScheduler — RAII owner of the kernel lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */
class MultiWindowScheduler {
public:
    // ── Construction / Destruction ──────────────────────────────────────

    /// Construct and initialize the kernel.
    /// @param workers  0 = auto (defaults to 4 in ASM).
    explicit MultiWindowScheduler(uint32_t workers = 0);

    /// Shutdown kernel, release all windows, free DLL if dynamically loaded.
    ~MultiWindowScheduler();

    // Non-copyable, non-movable (owns kernel singleton)
    MultiWindowScheduler(const MultiWindowScheduler&) = delete;
    MultiWindowScheduler& operator=(const MultiWindowScheduler&) = delete;

    // ── Dynamic DLL loading (optional) ─────────────────────────────────

    /// Load the kernel DLL at runtime.  Must be called before construction
    /// if not statically linked.
    static bool LoadDLL(const char* dllPath);

    /// Unload the kernel DLL.
    static void UnloadDLL();

    /// @return true if the DLL has been loaded (or statically linked).
    static bool IsDLLLoaded();

    // ── Window Management ──────────────────────────────────────────────

    /// Register a new window with the kernel.
    /// @return Window descriptor (id=0 on failure).
    Window CreateWindow(uint32_t type, int32_t x, int32_t y,
                        uint32_t w, uint32_t h,
                        const char* title = nullptr);

    /// Unregister a window.  Returns false if not found.
    bool DestroyWindow(MW_WINDOW_ID id);

    /// @return Number of windows currently tracked by this wrapper.
    size_t GetActiveWindowCount() const;

    // ── Task Submission ────────────────────────────────────────────────

    /// Submit a task.  Returns task_id (0 = overflow / failure).
    MW_TASK_ID Submit(const TaskOptions& opts);

    /// Cancel a queued task.  Returns false if not found.
    bool Cancel(MW_TASK_ID taskId);

    /// Check if a task is in a terminal state.
    bool IsComplete(MW_TASK_ID taskId) const;

    // ── Swarm ──────────────────────────────────────────────────────────

    /// Broadcast a task to up to @p modelCount active windows.
    /// @return Number of windows that received the task.
    uint32_t SwarmExecute(MW_TASK_TYPE type, const void* payload,
                          uint32_t payloadSize, uint32_t modelCount);

    // ── Chain of Thought ───────────────────────────────────────────────

    /// Create a sequential chain of tasks in @p windowId.
    /// Each step depends on the previous.
    /// @return First task_id in the chain (0 on failure).
    MW_TASK_ID CreateCoTPipeline(MW_WINDOW_ID windowId,
                                 const MW_TaskCallback* steps,
                                 uint32_t stepCount);

    // ── IPC ────────────────────────────────────────────────────────────

    /// Send an IPC message.  dstWindow=0 means broadcast.
    bool SendIPC(MW_MSG_TYPE msgType, MW_WINDOW_ID src,
                 MW_WINDOW_ID dst, const void* payload,
                 uint32_t payloadSize);

    // ── Stats & Timing ─────────────────────────────────────────────────

    MW_KernelStats GetStats() const;
    uint64_t GetUptimeMs() const;
    uint64_t GetMicros() const;

    // ── Replay Log ─────────────────────────────────────────────────────

    /// Read replay entries from shared memory.
    /// @param out       Output buffer.
    /// @param maxCount  Max entries to read.
    /// @return Number of entries written.
    uint32_t ReadReplayLog(MW_ReplayEntry* out, uint32_t maxCount) const;

    // ── Callback registration ──────────────────────────────────────────

    TaskCompletionCb    onTaskComplete  = nullptr;
    StatsPollCb         onStatsPoll     = nullptr;

    // ── Status ─────────────────────────────────────────────────────────

    bool IsInitialized() const { return m_initialized; }

private:
    bool                                            m_initialized = false;
    mutable std::mutex                              m_mutex;
    std::unordered_map<MW_WINDOW_ID, Window>        m_windows;

    // ── Dynamic loading state ──────────────────────────────────────────
    static std::atomic<bool>    s_dllLoaded;
    static HMODULE              s_hModule;

    // ── Runtime-resolved function pointers (used only in dynamic mode) ─
    // When statically linked these are unused; the linker resolves symbols.
    struct DynAPI {
        decltype(&KernelInit)               pKernelInit             = nullptr;
        decltype(&KernelShutdown)           pKernelShutdown         = nullptr;
        decltype(&SubmitTask)               pSubmitTask             = nullptr;
        decltype(&CancelTask)               pCancelTask             = nullptr;
        decltype(&RegisterWindow)           pRegisterWindow         = nullptr;
        decltype(&UnregisterWindow)         pUnregisterWindow       = nullptr;
        decltype(&SendIPCMessage)           pSendIPCMessage         = nullptr;
        decltype(&GetKernelStats)           pGetKernelStats         = nullptr;
        decltype(&SwarmBroadcast)           pSwarmBroadcast         = nullptr;
        decltype(&ChainOfThought)           pChainOfThought         = nullptr;
        decltype(&::GetMicroseconds)        pGetMicroseconds        = nullptr;
        decltype(&::IsTaskComplete)         pIsTaskComplete         = nullptr;
    };
    static DynAPI s_dyn;
};

} // namespace RawrXD
