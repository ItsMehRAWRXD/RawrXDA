// ============================================================================
// RawrXD Thread Lifecycle Registry
// Fault Attribution & Thread Safety Enforcement
// ============================================================================
// Purpose: Track thread creation/exit, prevent use-after-free in background
//          threads, and provide crash attribution to specific subsystems.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <thread>
#include <unordered_map>
#include <functional>

namespace rawrxd {

// Maximum number of tracked threads
constexpr size_t kMaxTrackedThreads = 64;

// Thread state enumeration
enum class ThreadState : uint32_t {
    Uninitialized = 0,
    Spawning      = 1,
    Running       = 2,
    ShuttingDown  = 3,
    Exited        = 4,
    Crashed       = 5
};

// Subsystem identifiers for attribution
enum class ThreadSubsystem : uint32_t {
    Unknown        = 0,
    Prometheus     = 1,
    HeadlessServer = 2,
    VulkanLoader   = 3,
    ModelInference = 4,
    ExtensionHost  = 5,
    MemoryMapper   = 6,
    Quantization   = 7,
    Count
};

// Thread registration record
struct ThreadRecord {
    std::atomic<uint64_t> threadId{0};
    std::atomic<ThreadState> state{ThreadState::Uninitialized};
    std::atomic<ThreadSubsystem> subsystem{ThreadSubsystem::Unknown};
    std::atomic<uint64_t> spawnTime{0};
    std::atomic<uint64_t> exitTime{0};
    std::atomic<uint64_t> crashTime{0};
    std::atomic<uint32_t> crashCode{0};
    char name[64] = {};
    char crashLocation[128] = {};
    std::atomic<uintptr_t> stackPointer{0};
    std::atomic<uintptr_t> instructionPointer{0};
    std::atomic<uint32_t> refCount{0};
};

// Global shutdown fence
class ShutdownFence {
public:
    static ShutdownFence& Instance();
    
    // Signal that shutdown has begun - prevents new thread registration
    void SignalShutdown();
    
    // Check if shutdown is in progress
    bool IsShuttingDown() const;
    
    // Wait for all registered threads to exit (with timeout)
    bool WaitForAllThreads(uint32_t timeoutMs);
    
    // Register a thread-safe callback to run during shutdown
    void RegisterShutdownCallback(std::function<void()> callback);

private:
    ShutdownFence() = default;
    std::atomic<bool> m_shuttingDown{false};
    std::atomic<uint32_t> m_activeThreadCount{0};
    std::mutex m_callbackMutex;
    std::vector<std::function<void()>> m_shutdownCallbacks;
};

// Thread lifecycle registry
class ThreadLifecycleRegistry {
public:
    static ThreadLifecycleRegistry& Instance();
    
    // Register a new thread (returns slot index, or -1 if full)
    int32_t RegisterThread(ThreadSubsystem subsystem, const char* name);
    
    // Mark thread as running (called after successful spawn)
    void MarkRunning(int32_t slot);
    
    // Mark thread as shutting down
    void MarkShuttingDown(int32_t slot);
    
    // Mark thread as exited (must be called before thread destructor)
    void MarkExited(int32_t slot);
    
    // Mark thread as crashed with exception code
    void MarkCrashed(int32_t slot, uint32_t exceptionCode, 
                     const char* location, uintptr_t rip, uintptr_t rsp);
    
    // Get thread record by slot
    ThreadRecord* GetRecord(int32_t slot);
    
    // Find slot by thread ID
    int32_t FindSlotByThreadId(uint64_t threadId);
    
    // Check if any threads are in crashed state
    bool HasCrashedThreads() const;
    
    // Get crash report for first crashed thread
    std::string GetCrashReport() const;
    
    // Dump all thread states to string
    std::string DumpAllThreads() const;
    
    // Get count of active (running) threads
    uint32_t GetActiveThreadCount() const;
    
    // Wait for all threads in a subsystem to exit
    bool WaitForSubsystemThreads(ThreadSubsystem subsystem, uint32_t timeoutMs);
    
    // Atomic check: is it safe for this thread to access shared state?
    bool IsSafeToAccessSharedState(int32_t slot) const;

private:
    ThreadLifecycleRegistry() = default;
    
    mutable std::mutex m_mutex;
    ThreadRecord m_records[kMaxTrackedThreads];
    std::atomic<uint32_t> m_nextSlot{0};
    std::atomic<uint32_t> m_activeCount{0};
    std::atomic<uint32_t> m_crashedCount{0};
    
    // Thread-local storage for current thread's slot
    static thread_local int32_t s_currentSlot;
};

// RAII thread guard - automatically registers/unregisters thread
class ThreadGuard {
public:
    ThreadGuard(ThreadSubsystem subsystem, const char* name);
    ~ThreadGuard();
    
    // Mark thread as ready for work
    void MarkReady();
    
    // Check if shutdown has been requested
    bool ShouldExit() const;
    
    // Get this thread's slot index
    int32_t GetSlot() const { return m_slot; }

private:
    int32_t m_slot = -1;
};

// Macro for easy thread guard usage
#define RAWRXD_THREAD_GUARD(subsystem, name) \
    rawrxd::ThreadGuard _threadGuard(subsystem, name)

// Crash attribution helper
struct CrashContext {
    uint32_t exceptionCode;
    uint64_t threadId;
    uintptr_t instructionPointer;
    uintptr_t stackPointer;
    char subsystemName[32];
    char threadName[64];
    char location[128];
};

// Global crash handler installation
void InstallThreadCrashHandler();

// Get last crash context
bool GetLastCrashContext(CrashContext& ctx);

} // namespace rawrxd
