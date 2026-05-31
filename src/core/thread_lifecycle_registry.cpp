// =============================================================================
// thread_lifecycle_registry.cpp — Thread Lifecycle Registry Implementation
// =============================================================================

#include "core/thread_lifecycle_registry.h"
#include <fstream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Core {

ThreadLifecycleRegistry& ThreadLifecycleRegistry::Instance() {
    static ThreadLifecycleRegistry instance;
    return instance;
}

ThreadLifecycleRegistry::~ThreadLifecycleRegistry() {
    RequestShutdown();
    WaitForAllExited(10000);
}

void ThreadLifecycleRegistry::Register(std::thread::id tid,
                                       const std::string& subsystem,
                                       const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entry = std::make_shared<ThreadEntry>();
    entry->tid = tid;
    entry->subsystem = subsystem;
    entry->description = description;
    entry->startTime = std::chrono::steady_clock::now();
    entry->exited.store(false);
    entry->joined.store(false);
    entries_[tid] = entry;
}

void ThreadLifecycleRegistry::MarkExited(std::thread::id tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(tid);
    if (it != entries_.end()) {
        it->second->exited.store(true);
    }
}

void ThreadLifecycleRegistry::MarkJoined(std::thread::id tid) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(tid);
    if (it != entries_.end()) {
        it->second->joined.store(true);
    }
}

void ThreadLifecycleRegistry::RequestShutdown() {
    shuttingDown_.store(true, std::memory_order_release);
    shutdownTime_ = std::chrono::steady_clock::now();
}

bool ThreadLifecycleRegistry::IsShuttingDown() const {
    return shuttingDown_.load(std::memory_order_acquire);
}

bool ThreadLifecycleRegistry::WaitForAllExited(int timeoutMs) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bool allExited = true;
            for (const auto& kv : entries_) {
                if (!kv.second->exited.load() && !kv.second->joined.load()) {
                    allExited = false;
                    break;
                }
            }
            if (allExited) return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

std::string ThreadLifecycleRegistry::Snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    auto now = std::chrono::steady_clock::now();
    oss << "=== Thread Lifecycle Snapshot ===\n";
    oss << "ShuttingDown: " << (shuttingDown_.load() ? "YES" : "NO") << "\n";
    for (const auto& kv : entries_) {
        const auto& e = kv.second;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - e->startTime).count();
        oss << "  [" << e->subsystem << "] ";
        if (!e->description.empty()) oss << "(" << e->description << ") ";
        oss << "exited=" << (e->exited.load() ? "Y" : "N");
        oss << " joined=" << (e->joined.load() ? "Y" : "N");
        oss << " crashed=" << (e->crashed.load() ? "Y" : "N");
        if (e->crashed.load()) {
            std::lock_guard<std::mutex> crashLock(e->crashMutex);
            oss << " crashInfo=" << e->crashInfo;
        }
        oss << " ageMs=" << elapsed;
        oss << "\n";
    }
    return oss.str();
}

void ThreadLifecycleRegistry::RecordCrash(std::thread::id tid, const std::string& exceptionInfo) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(tid);
    if (it != entries_.end()) {
        it->second->crashed.store(true);
        {
            std::lock_guard<std::mutex> crashLock(it->second->crashMutex);
            it->second->crashInfo = exceptionInfo;
        }
    }
}

bool ThreadLifecycleRegistry::HasCrashedThreads() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : entries_) {
        if (kv.second->crashed.load()) {
            return true;
        }
    }
    return false;
}

std::string ThreadLifecycleRegistry::CrashReport() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;
    oss << "=== Thread Crash Report ===\n";
    bool anyCrashes = false;
    for (const auto& kv : entries_) {
        if (kv.second->crashed.load()) {
            anyCrashes = true;
            const auto& e = kv.second;
            oss << "  [" << e->subsystem << "] ";
            if (!e->description.empty()) oss << "(" << e->description << ") ";
            {
                std::lock_guard<std::mutex> crashLock(e->crashMutex);
                oss << e->crashInfo;
            }
            oss << "\n";
        }
    }
    if (!anyCrashes) {
        oss << "  No crashed threads recorded.\n";
    }
    return oss.str();
}

bool ThreadLifecycleRegistry::HasLiveThreads(const std::string& subsystem) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& kv : entries_) {
        if (kv.second->subsystem == subsystem &&
            !kv.second->exited.load() &&
            !kv.second->joined.load()) {
            return true;
        }
    }
    return false;
}

} // namespace Core
} // namespace RawrXD
