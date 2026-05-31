// =============================================================================
// thread_lifecycle_registry.h — Thread Lifecycle Registry & Shutdown Fencing
// =============================================================================
// Purpose:
//   - Track every background thread spawned during engine initialization
//   - Provide deterministic shutdown fencing (no thread accesses shared state
//     after shutdown begins)
//   - Capture thread ownership snapshots for crash attribution
//
// Design:
//   - Singleton registry (thread-safe, lock-free where possible)
//   - Each registered thread gets a subsystem name + start timestamp
//   - Shutdown flag is atomic; all worker threads must check it before
//     touching shared state
//   - Destructor waits for all registered threads to exit (join or detach
//     with confirmation)
//
// Usage:
//   REGISTER_THREAD("PrometheusExporter", myThread);
//   if (ThreadLifecycleRegistry::Instance().IsShuttingDown()) return;
//   // safe to touch shared state
// =============================================================================

#pragma once

#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <cstdint>

namespace RawrXD {
namespace Core {

struct ThreadEntry {
    std::thread::id tid;
    std::string subsystem;
    std::string description;
    std::chrono::steady_clock::time_point startTime;
    std::atomic<bool> exited{false};
    std::atomic<bool> joined{false};
    std::atomic<bool> crashed{false};
    std::string crashInfo;
    std::mutex crashMutex;
};

class ThreadLifecycleRegistry {
public:
    static ThreadLifecycleRegistry& Instance();

    // Register a thread before it starts work
    void Register(std::thread::id tid,
                  const std::string& subsystem,
                  const std::string& description = "");

    // Mark a thread as exited (call at end of thread proc)
    void MarkExited(std::thread::id tid);

    // Mark a thread as joined (call after join() or detach())
    void MarkJoined(std::thread::id tid);

    // Shutdown fencing
    void RequestShutdown();
    bool IsShuttingDown() const;

    // Wait for all registered threads to exit (with timeout)
    bool WaitForAllExited(int timeoutMs = 5000);

    // Crash attribution: dump current thread state
    std::string Snapshot() const;

    // Crash attribution: record a thread crash with exception info
    void RecordCrash(std::thread::id tid, const std::string& exceptionInfo);

    // Check if any threads have crashed
    bool HasCrashedThreads() const;

    // Get crash report for all crashed threads
    std::string CrashReport() const;

    // Check if a specific subsystem has any live threads
    bool HasLiveThreads(const std::string& subsystem) const;

private:
    ThreadLifecycleRegistry() = default;
    ~ThreadLifecycleRegistry();

    ThreadLifecycleRegistry(const ThreadLifecycleRegistry&) = delete;
    ThreadLifecycleRegistry& operator=(const ThreadLifecycleRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::thread::id, std::shared_ptr<ThreadEntry>> entries_;
    std::atomic<bool> shuttingDown_{false};
    std::chrono::steady_clock::time_point shutdownTime_;
};

// Convenience macro for registration
#define REGISTER_THREAD(subsystem, description) \
    RawrXD::Core::ThreadLifecycleRegistry::Instance().Register(std::this_thread::get_id(), subsystem, description)

#define CHECK_SHUTDOWN_AND_RETURN() \
    if (RawrXD::Core::ThreadLifecycleRegistry::Instance().IsShuttingDown()) return

#define CHECK_SHUTDOWN_AND_RETURN_VAL(val) \
    if (RawrXD::Core::ThreadLifecycleRegistry::Instance().IsShuttingDown()) return val

} // namespace Core
} // namespace RawrXD
