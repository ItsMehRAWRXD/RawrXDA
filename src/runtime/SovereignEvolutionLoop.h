#pragma once

#pragma once
// =============================================================================
// SovereignEvolutionLoop.h — Phase 53: Background self-improvement cycle thread
// =============================================================================
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace RawrXD::Runtime {

class SovereignEvolutionLoop {
public:
    static SovereignEvolutionLoop& instance();

    /// Start the background cycle thread (idempotent).
    bool initialize();

    /// Execute one cycle synchronously (also called by background thread).
    void runCycle();

    /// Stop the background thread and join.
    void stop();

    uint32_t cycleCount() const { return m_cycleCount.load(); }

    SovereignEvolutionLoop();
    ~SovereignEvolutionLoop();

private:
    std::atomic<bool>     m_active;
    std::atomic<uint32_t> m_cycleCount;
    std::thread           m_thread;
    std::mutex            m_cv_mutex;
    std::condition_variable m_cv;
};

} // namespace RawrXD::Runtime
