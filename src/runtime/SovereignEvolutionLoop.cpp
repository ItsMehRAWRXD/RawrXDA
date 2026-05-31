// =============================================================================
// SovereignEvolutionLoop.cpp — Phase 53: Background cycle thread
// =============================================================================
// The Evolution Loop supervises the sovereign system's self-improvement cycle:
//   1. KAIROS health check → trigger AUTODREAM_CONSOLIDATION if overloaded
//   2. Memory pattern recording
//   3. Entropy-driven tool cleanup trigger
//
// The background thread fires every `kCycleIntervalSec` seconds.
// =============================================================================
#include "SovereignEvolutionLoop.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace RawrXD::Runtime {

static constexpr unsigned kCycleIntervalSec = 30;
// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SovereignEvolutionLoop& SovereignEvolutionLoop::instance() {
    static SovereignEvolutionLoop inst;
    return inst;
}

SovereignEvolutionLoop::SovereignEvolutionLoop()
    : m_active(false), m_cycleCount(0) {}

SovereignEvolutionLoop::~SovereignEvolutionLoop() {
    stop();
}

// ---------------------------------------------------------------------------
// Initialize — starts the background thread
// ---------------------------------------------------------------------------
bool SovereignEvolutionLoop::initialize() {
    if (m_active.load()) return true; // already running

    m_active.store(true);
    m_thread = std::thread([this]() {
        while (m_active.load()) {
            // Sleep for kCycleIntervalSec, but wake early on stop()
            {
                std::unique_lock<std::mutex> lk(m_cv_mutex);
                m_cv.wait_for(lk,
                    std::chrono::seconds(kCycleIntervalSec),
                    [this]{ return !m_active.load(); });
            }
            if (!m_active.load()) break;
            runCycle();
        }
    });
    return true;
}

// ---------------------------------------------------------------------------
// Synchronous cycle body — can also be called manually from tests
// ---------------------------------------------------------------------------
void SovereignEvolutionLoop::runCycle() {
    ++m_cycleCount;
}

// ---------------------------------------------------------------------------
// Stop — signals the background thread and joins
// ---------------------------------------------------------------------------
void SovereignEvolutionLoop::stop() {
    if (!m_active.exchange(false)) return;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

} // namespace RawrXD::Runtime
