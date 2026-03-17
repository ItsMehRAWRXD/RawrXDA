/**
 * @file multiwindow_scheduler.cpp
 * @brief Implementation of the C++20 RAII wrapper for the MASM64 MultiWindow Kernel
 *
 * All public methods acquire m_mutex for thread safety.
 * No Qt, no exceptions, no std::function.
 * Callbacks are raw function pointers per project convention.
 *
 * Build: MSVC 2022 / C++20
 * Link:  RawrXD_MultiWindow_Kernel.lib (or dynamic via LoadDLL)
 */

#include "multiwindow_scheduler.hpp"

#include <cstdio>
#include <cstring>

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════
// Static members
// ═══════════════════════════════════════════════════════════════════════════

std::atomic<bool>                   MultiWindowScheduler::s_dllLoaded{false};
HMODULE                             MultiWindowScheduler::s_hModule = nullptr;
MultiWindowScheduler::DynAPI        MultiWindowScheduler::s_dyn{};

// ═══════════════════════════════════════════════════════════════════════════
// Dynamic DLL loading
// ═══════════════════════════════════════════════════════════════════════════

bool MultiWindowScheduler::LoadDLL(const char* dllPath) {
    if (s_dllLoaded.load(std::memory_order_acquire))
        return true;

    s_hModule = LoadLibraryA(dllPath);
    if (!s_hModule) {
        fprintf(stderr, "[MultiWindowScheduler] LoadLibraryA failed for '%s' (err=%lu)\n",
                dllPath, GetLastError());
        return false;
    }

    // Resolve all exports
    #define RESOLVE(name) \
        s_dyn.p##name = reinterpret_cast<decltype(s_dyn.p##name)>( \
            GetProcAddress(s_hModule, #name)); \
        if (!s_dyn.p##name) { \
            fprintf(stderr, "[MultiWindowScheduler] Missing export: " #name "\n"); \
            FreeLibrary(s_hModule); s_hModule = nullptr; return false; \
        }

    RESOLVE(KernelInit)
    RESOLVE(KernelShutdown)
    RESOLVE(SubmitTask)
    RESOLVE(CancelTask)
    RESOLVE(RegisterWindow)
    RESOLVE(UnregisterWindow)
    RESOLVE(SendIPCMessage)
    RESOLVE(GetKernelStats)
    RESOLVE(SwarmBroadcast)
    RESOLVE(ChainOfThought)
    RESOLVE(GetMicroseconds)
    RESOLVE(IsTaskComplete)

    #undef RESOLVE

    s_dllLoaded.store(true, std::memory_order_release);
    fprintf(stderr, "[MultiWindowScheduler] DLL loaded: %s\n", dllPath);
    return true;
}

void MultiWindowScheduler::UnloadDLL() {
    if (s_hModule) {
        FreeLibrary(s_hModule);
        s_hModule = nullptr;
    }
    std::memset(&s_dyn, 0, sizeof(s_dyn));
    s_dllLoaded.store(false, std::memory_order_release);
}

bool MultiWindowScheduler::IsDLLLoaded() {
    return s_dllLoaded.load(std::memory_order_acquire);
}

// ═══════════════════════════════════════════════════════════════════════════
// Kernel call helpers — use dynamic pointers if loaded, else static linkage
// ═══════════════════════════════════════════════════════════════════════════

#define KCALL(fn, ...) \
    (s_dyn.p##fn ? s_dyn.p##fn(__VA_ARGS__) : fn(__VA_ARGS__))

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

MultiWindowScheduler::MultiWindowScheduler(uint32_t workers) {
    BOOL ok = KCALL(KernelInit, workers);
    if (!ok) {
        fprintf(stderr, "[MultiWindowScheduler] KernelInit FAILED\n");
        m_initialized = false;
        return;
    }
    m_initialized = true;
    fprintf(stderr, "[MultiWindowScheduler] Kernel initialized (workers=%u)\n",
            workers == 0 ? 4 : workers);
}

MultiWindowScheduler::~MultiWindowScheduler() {
    if (!m_initialized) return;

    // Destroy all tracked windows
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, win] : m_windows) {
            KCALL(UnregisterWindow, id);
        }
        m_windows.clear();
    }

    KCALL(KernelShutdown);
    m_initialized = false;
    fprintf(stderr, "[MultiWindowScheduler] Kernel shut down\n");
}

// ═══════════════════════════════════════════════════════════════════════════
// Window Management
// ═══════════════════════════════════════════════════════════════════════════

Window MultiWindowScheduler::CreateWindow(uint32_t type, int32_t x, int32_t y,
                                           uint32_t w, uint32_t h,
                                           const char* title) {
    Window win{};
    if (!m_initialized) return win;

    MW_WINDOW_ID id = KCALL(RegisterWindow, type, x, y, w, h);
    if (id == 0) {
        fprintf(stderr, "[MultiWindowScheduler] RegisterWindow failed\n");
        return win;
    }

    win.id   = id;
    win.type = type;
    win.title = title ? title : ("RawrXD_Window_" + std::to_string(id));

    std::lock_guard<std::mutex> lock(m_mutex);
    m_windows[id] = win;
    return win;
}

bool MultiWindowScheduler::DestroyWindow(MW_WINDOW_ID id) {
    if (!m_initialized) return false;

    BOOL ok = KCALL(UnregisterWindow, id);
    if (!ok) return false;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_windows.erase(id);
    return true;
}

size_t MultiWindowScheduler::GetActiveWindowCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_windows.size();
}

// ═══════════════════════════════════════════════════════════════════════════
// Task Submission
// ═══════════════════════════════════════════════════════════════════════════

MW_TASK_ID MultiWindowScheduler::Submit(const TaskOptions& opts) {
    if (!m_initialized) return 0;

    MW_TASK_ID id = KCALL(SubmitTask,
        static_cast<uint32_t>(opts.type),
        static_cast<uint32_t>(opts.priority),
        opts.windowId,
        opts.callback,
        opts.userData,
        opts.modelId,
        opts.dependsOn
    );

    return id;
}

bool MultiWindowScheduler::Cancel(MW_TASK_ID taskId) {
    if (!m_initialized) return false;
    return KCALL(CancelTask, taskId) != 0;
}

bool MultiWindowScheduler::IsComplete(MW_TASK_ID taskId) const {
    if (!m_initialized) return true;
    return KCALL(IsTaskComplete, taskId) != 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Swarm
// ═══════════════════════════════════════════════════════════════════════════

uint32_t MultiWindowScheduler::SwarmExecute(MW_TASK_TYPE type,
                                             const void* payload,
                                             uint32_t payloadSize,
                                             uint32_t modelCount) {
    if (!m_initialized) return 0;
    return KCALL(SwarmBroadcast,
        static_cast<uint32_t>(type),
        payload,
        payloadSize,
        modelCount
    );
}

// ═══════════════════════════════════════════════════════════════════════════
// Chain of Thought
// ═══════════════════════════════════════════════════════════════════════════

MW_TASK_ID MultiWindowScheduler::CreateCoTPipeline(MW_WINDOW_ID windowId,
                                                    const MW_TaskCallback* steps,
                                                    uint32_t stepCount) {
    if (!m_initialized || !steps || stepCount == 0) return 0;
    return KCALL(ChainOfThought, windowId, stepCount,
                 const_cast<MW_TaskCallback*>(steps));
}

// ═══════════════════════════════════════════════════════════════════════════
// IPC
// ═══════════════════════════════════════════════════════════════════════════

bool MultiWindowScheduler::SendIPC(MW_MSG_TYPE msgType,
                                    MW_WINDOW_ID src,
                                    MW_WINDOW_ID dst,
                                    const void* payload,
                                    uint32_t payloadSize) {
    if (!m_initialized) return false;
    return KCALL(SendIPCMessage,
        static_cast<uint32_t>(msgType),
        src, dst, payload, payloadSize
    ) != 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Stats & Timing
// ═══════════════════════════════════════════════════════════════════════════

MW_KernelStats MultiWindowScheduler::GetStats() const {
    MW_KernelStats stats{};
    if (m_initialized) {
        KCALL(GetKernelStats, &stats);
    }
    if (onStatsPoll) {
        onStatsPoll(&stats);
    }
    return stats;
}

uint64_t MultiWindowScheduler::GetUptimeMs() const {
    MW_KernelStats s = GetStats();
    return s.uptimeMs;
}

uint64_t MultiWindowScheduler::GetMicros() const {
    if (!m_initialized) return 0;
    return KCALL(GetMicroseconds);
}

// ═══════════════════════════════════════════════════════════════════════════
// Replay Log
// ═══════════════════════════════════════════════════════════════════════════

uint32_t MultiWindowScheduler::ReadReplayLog(MW_ReplayEntry* out,
                                              uint32_t maxCount) const {
    if (!m_initialized || !out || maxCount == 0) return 0;

    // The replay log lives in shared memory at SHM_REPLAY_BASE.
    // We don't have direct access to g_SharedMemBase from C++,
    // but we can open the same named file mapping.

    HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, L"RawrXD_MW_IPC");
    if (!hMap) return 0;

    const uint8_t* base = static_cast<const uint8_t*>(
        MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, MW_IPC_SHARED_SIZE));
    if (!base) {
        CloseHandle(hMap);
        return 0;
    }

    const MW_ReplayEntry* entries = reinterpret_cast<const MW_ReplayEntry*>(
        base + MW_SHM_REPLAY_BASE);

    uint32_t count = 0;
    for (uint32_t i = 0; i < MW_SHM_REPLAY_MAX && count < maxCount; ++i) {
        if (entries[i].timestamp == 0) continue; // empty slot
        out[count++] = entries[i];
    }

    UnmapViewOfFile(base);
    CloseHandle(hMap);
    return count;
}

#undef KCALL

} // namespace RawrXD
