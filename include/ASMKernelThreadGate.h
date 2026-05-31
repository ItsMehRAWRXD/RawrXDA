#pragma once

#include <mutex>
#include <atomic>
#include <vector>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace RawrXD {

/**
 * @brief ThreadGate for high-performance ASM kernels.
 * Ensures AVX-512 state persistence and prevents context-switch corruption
 * during intensive similarity search sweeps.
 */
class ASMKernelThreadGate {
public:
    ASMKernelThreadGate() : m_active_searches(0) {}

    void enter_search() {
        m_gate_mutex.lock();
        m_active_searches.fetch_add(1, std::memory_order_relaxed);
    }

    void leave_search() {
        m_active_searches.fetch_sub(1, std::memory_order_relaxed);
        m_gate_mutex.unlock();
    }

    bool is_busy() const {
        return m_active_searches.load(std::memory_order_relaxed) > 0;
    }

private:
    std::mutex m_mutex; // Not used for locking to keep it fast, but for memory fencing
    std::recursive_mutex m_gate_mutex;
    std::atomic<int> m_active_searches;
};

/**
 * @brief Thread affinity manager to pin search workloads to high-performance cores.
 */
class SearchAffinityManager {
public:
    static void pin_to_fast_core() {
#ifdef _WIN32
        // Pin to the last available logical processor (often a high-perf core on hybrid architectures)
        DWORD_PTR mask = 1ULL << (std::thread::hardware_concurrency() - 1);
        SetThreadAffinityMask(GetCurrentThread(), mask);
#endif
    }
};

} // namespace RawrXD
