// ============================================================================
// RawrXD Thread Lifecycle Registry Implementation
// ============================================================================

#include "thread_lifecycle_registry.h"
#include <windows.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace rawrxd {

// ============================================================================
// ShutdownFence Implementation
// ============================================================================

ShutdownFence& ShutdownFence::Instance() {
    static ShutdownFence instance;
    return instance;
}

void ShutdownFence::SignalShutdown() {
    m_shuttingDown.store(true, std::memory_order_release);
    
    // Run shutdown callbacks
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    for (auto& cb : m_shutdownCallbacks) {
        if (cb) {
            try {
                cb();
            } catch (...) {
                // Callback exceptions should not prevent shutdown
            }
        }
    }
}

bool ShutdownFence::IsShuttingDown() const {
    return m_shuttingDown.load(std::memory_order_acquire);
}

bool ShutdownFence::WaitForAllThreads(uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (m_activeThreadCount.load(std::memory_order_acquire) > 0) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            return false; // Timeout
        }
        Sleep(10);
    }
    return true;
}

void ShutdownFence::RegisterShutdownCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_shutdownCallbacks.push_back(std::move(callback));
}

// ============================================================================
// ThreadLifecycleRegistry Implementation
// ============================================================================

thread_local int32_t ThreadLifecycleRegistry::s_currentSlot = -1;

ThreadLifecycleRegistry& ThreadLifecycleRegistry::Instance() {
    static ThreadLifecycleRegistry instance;
    return instance;
}

int32_t ThreadLifecycleRegistry::RegisterThread(ThreadSubsystem subsystem, const char* name) {
    // Check if shutdown is in progress
    if (ShutdownFence::Instance().IsShuttingDown()) {
        return -1; // Reject new threads during shutdown
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Find an available slot
    uint32_t startSlot = m_nextSlot.load(std::memory_order_relaxed);
    for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
        uint32_t slot = (startSlot + i) % kMaxTrackedThreads;
        ThreadState expected = ThreadState::Uninitialized;
        
        if (m_records[slot].state.compare_exchange_strong(
                expected, ThreadState::Spawning,
                std::memory_order_acq_rel)) {
            
            // Initialize the record
            m_records[slot].threadId.store(
                static_cast<uint64_t>(GetCurrentThreadId()),
                std::memory_order_release);
            m_records[slot].subsystem.store(subsystem, std::memory_order_release);
            m_records[slot].spawnTime.store(
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count()),
                std::memory_order_release);
            m_records[slot].exitTime.store(0, std::memory_order_release);
            m_records[slot].crashTime.store(0, std::memory_order_release);
            m_records[slot].crashCode.store(0, std::memory_order_release);
            m_records[slot].stackPointer.store(0, std::memory_order_release);
            m_records[slot].instructionPointer.store(0, std::memory_order_release);
            m_records[slot].refCount.store(1, std::memory_order_release);
            
            // Copy name
            strncpy_s(m_records[slot].name, sizeof(m_records[slot].name), 
                     name ? name : "unnamed", _TRUNCATE);
            m_records[slot].crashLocation[0] = '\0';
            
            m_nextSlot.store((slot + 1) % kMaxTrackedThreads, 
                           std::memory_order_relaxed);
            m_activeCount.fetch_add(1, std::memory_order_acq_rel);
            
            // Set thread-local slot
            s_currentSlot = static_cast<int32_t>(slot);
            
            ShutdownFence::Instance().m_activeThreadCount.fetch_add(
                1, std::memory_order_acq_rel);
            
            return static_cast<int32_t>(slot);
        }
    }
    
    return -1; // No slots available
}

void ThreadLifecycleRegistry::MarkRunning(int32_t slot) {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) return;
    
    ThreadState expected = ThreadState::Spawning;
    m_records[slot].state.compare_exchange_strong(
        expected, ThreadState::Running,
        std::memory_order_acq_rel);
}

void ThreadLifecycleRegistry::MarkShuttingDown(int32_t slot) {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) return;
    
    ThreadState expected = ThreadState::Running;
    m_records[slot].state.compare_exchange_strong(
        expected, ThreadState::ShuttingDown,
        std::memory_order_acq_rel);
}

void ThreadLifecycleRegistry::MarkExited(int32_t slot) {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) return;
    
    ThreadState expected = ThreadState::ShuttingDown;
    if (!m_records[slot].state.compare_exchange_strong(
            expected, ThreadState::Exited,
            std::memory_order_acq_rel)) {
        // Also accept Running -> Exited transitions
        expected = ThreadState::Running;
        m_records[slot].state.compare_exchange_strong(
            expected, ThreadState::Exited,
            std::memory_order_acq_rel);
    }
    
    m_records[slot].exitTime.store(
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()),
        std::memory_order_release);
    
    m_activeCount.fetch_sub(1, std::memory_order_acq_rel);
    ShutdownFence::Instance().m_activeThreadCount.fetch_sub(
        1, std::memory_order_acq_rel);
}

void ThreadLifecycleRegistry::MarkCrashed(int32_t slot, uint32_t exceptionCode,
                                          const char* location, 
                                          uintptr_t rip, uintptr_t rsp) {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) return;
    
    m_records[slot].state.store(ThreadState::Crashed, std::memory_order_release);
    m_records[slot].crashTime.store(
        static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()),
        std::memory_order_release);
    m_records[slot].crashCode.store(exceptionCode, std::memory_order_release);
    m_records[slot].instructionPointer.store(rip, std::memory_order_release);
    m_records[slot].stackPointer.store(rsp, std::memory_order_release);
    
    if (location) {
        strncpy_s(m_records[slot].crashLocation, 
                 sizeof(m_records[slot].crashLocation),
                 location, _TRUNCATE);
    }
    
    m_crashedCount.fetch_add(1, std::memory_order_acq_rel);
    m_activeCount.fetch_sub(1, std::memory_order_acq_rel);
    ShutdownFence::Instance().m_activeThreadCount.fetch_sub(
        1, std::memory_order_acq_rel);
}

ThreadRecord* ThreadLifecycleRegistry::GetRecord(int32_t slot) {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) 
        return nullptr;
    return &m_records[slot];
}

int32_t ThreadLifecycleRegistry::FindSlotByThreadId(uint64_t threadId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
        if (m_records[i].threadId.load(std::memory_order_acquire) == threadId &&
            m_records[i].state.load(std::memory_order_acquire) != ThreadState::Uninitialized) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

bool ThreadLifecycleRegistry::HasCrashedThreads() const {
    return m_crashedCount.load(std::memory_order_acquire) > 0;
}

std::string ThreadLifecycleRegistry::GetCrashReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
        if (m_records[i].state.load(std::memory_order_acquire) == ThreadState::Crashed) {
            std::ostringstream oss;
            oss << "=== THREAD CRASH REPORT ===\n"
                << "Thread ID: " << m_records[i].threadId.load() << "\n"
                << "Name: " << m_records[i].name << "\n"
                << "Subsystem: " << static_cast<uint32_t>(
                    m_records[i].subsystem.load()) << "\n"
                << "Exception Code: 0x" << std::hex << std::uppercase
                << m_records[i].crashCode.load() << std::dec << "\n"
                << "Location: " << m_records[i].crashLocation << "\n"
                << "RIP: 0x" << std::hex << m_records[i].instructionPointer.load()
                << std::dec << "\n"
                << "RSP: 0x" << std::hex << m_records[i].stackPointer.load()
                << std::dec << "\n"
                << "Spawn Time: " << m_records[i].spawnTime.load() << "\n"
                << "Crash Time: " << m_records[i].crashTime.load() << "\n"
                << "===========================\n";
            return oss.str();
        }
    }
    return "No crashed threads found.\n";
}

std::string ThreadLifecycleRegistry::DumpAllThreads() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    
    oss << "=== THREAD REGISTRY DUMP ===\n"
        << "Active: " << m_activeCount.load() << "\n"
        << "Crashed: " << m_crashedCount.load() << "\n\n";
    
    for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
        ThreadState state = m_records[i].state.load(std::memory_order_acquire);
        if (state == ThreadState::Uninitialized) continue;
        
        oss << "[" << i << "] "
            << "TID=" << m_records[i].threadId.load() << " "
            << "Name=\"" << m_records[i].name << "\" "
            << "State=" << static_cast<uint32_t>(state) << " "
            << "Subsys=" << static_cast<uint32_t>(
                m_records[i].subsystem.load()) << "\n";
    }
    
    oss << "============================\n";
    return oss.str();
}

uint32_t ThreadLifecycleRegistry::GetActiveThreadCount() const {
    return m_activeCount.load(std::memory_order_acquire);
}

bool ThreadLifecycleRegistry::WaitForSubsystemThreads(ThreadSubsystem subsystem, 
                                                      uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        bool anyRunning = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
                if (m_records[i].subsystem.load(std::memory_order_acquire) == subsystem &&
                    (m_records[i].state.load(std::memory_order_acquire) == ThreadState::Running ||
                     m_records[i].state.load(std::memory_order_acquire) == ThreadState::Spawning)) {
                    anyRunning = true;
                    break;
                }
            }
        }
        
        if (!anyRunning) return true;
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeoutMs) {
            return false;
        }
        
        Sleep(10);
    }
}

bool ThreadLifecycleRegistry::IsSafeToAccessSharedState(int32_t slot) const {
    if (slot < 0 || slot >= static_cast<int32_t>(kMaxTrackedThreads)) return false;
    
    ThreadState state = m_records[slot].state.load(std::memory_order_acquire);
    return state == ThreadState::Running || state == ThreadState::Spawning;
}

// ============================================================================
// ThreadGuard Implementation
// ============================================================================

ThreadGuard::ThreadGuard(ThreadSubsystem subsystem, const char* name) {
    m_slot = ThreadLifecycleRegistry::Instance().RegisterThread(subsystem, name);
    if (m_slot >= 0) {
        ThreadLifecycleRegistry::Instance().MarkRunning(m_slot);
    }
}

ThreadGuard::~ThreadGuard() {
    if (m_slot >= 0) {
        ThreadLifecycleRegistry::Instance().MarkShuttingDown(m_slot);
        ThreadLifecycleRegistry::Instance().MarkExited(m_slot);
    }
}

void ThreadGuard::MarkReady() {
    if (m_slot >= 0) {
        ThreadLifecycleRegistry::Instance().MarkRunning(m_slot);
    }
}

bool ThreadGuard::ShouldExit() const {
    return ShutdownFence::Instance().IsShuttingDown();
}

// ============================================================================
// Crash Handler
// ============================================================================

static LONG WINAPI ThreadCrashHandler(EXCEPTION_POINTERS* ExceptionInfo) {
    uint32_t exceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
    uintptr_t rip = reinterpret_cast<uintptr_t>(
        ExceptionInfo->ExceptionRecord->ExceptionAddress);
    uintptr_t rsp = reinterpret_cast<uintptr_t>(
        ExceptionInfo->ContextRecord->Rsp);
    
    uint64_t threadId = static_cast<uint64_t>(GetCurrentThreadId());
    int32_t slot = ThreadLifecycleRegistry::Instance().FindSlotByThreadId(threadId);
    
    if (slot >= 0) {
        ThreadLifecycleRegistry::Instance().MarkCrashed(
            slot, exceptionCode, "SEH_EXCEPTION", rip, rsp);
    }
    
    // Write to crash log
    FILE* f = nullptr;
    fopen_s(&f, "rawrxd_crash_attribution.log", "a");
    if (f) {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char timeStr[64];
        ctime_s(timeStr, sizeof(timeStr), &time_t_now);
        timeStr[strlen(timeStr) - 1] = '\0'; // Remove newline
        
        fprintf(f, "[%s] THREAD_CRASH TID=%llu EXC=0x%08X RIP=0x%p RSP=0x%p SLOT=%d\n",
                timeStr, threadId, exceptionCode, 
                reinterpret_cast<void*>(rip), 
                reinterpret_cast<void*>(rsp), slot);
        fclose(f);
    }
    
    // Don't handle - let default handler create dump
    return EXCEPTION_CONTINUE_SEARCH;
}

void InstallThreadCrashHandler() {
    SetUnhandledExceptionFilter(ThreadCrashHandler);
}

bool GetLastCrashContext(CrashContext& ctx) {
    ThreadLifecycleRegistry& reg = ThreadLifecycleRegistry::Instance();
    
    for (uint32_t i = 0; i < kMaxTrackedThreads; ++i) {
        ThreadRecord* rec = reg.GetRecord(static_cast<int32_t>(i));
        if (rec && rec->state.load(std::memory_order_acquire) == ThreadState::Crashed) {
            ctx.exceptionCode = rec->crashCode.load();
            ctx.threadId = rec->threadId.load();
            ctx.instructionPointer = rec->instructionPointer.load();
            ctx.stackPointer = rec->stackPointer.load();
            strncpy_s(ctx.subsystemName, sizeof(ctx.subsystemName),
                     "Unknown", _TRUNCATE);
            strncpy_s(ctx.threadName, sizeof(ctx.threadName),
                     rec->name, _TRUNCATE);
            strncpy_s(ctx.location, sizeof(ctx.location),
                     rec->crashLocation, _TRUNCATE);
            return true;
        }
    }
    return false;
}

} // namespace rawrxd
