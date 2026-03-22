/**
 * @file sdma_coordinator.cpp
 * @brief SDMA Scheduler Coordinator Implementation (Phase 1)
 */

#include "sdma_coordinator.hpp"
#include <cassert>
#include <cstring>
#include <intrin.h>
#include <thread>
#include <chrono>

namespace rawrxd::sdma {

// ============================================================
// EXTERNAL SYMBOLS (defined in MASM)
// ============================================================
alignas(64) SDMASchedulerState g_sdma_scheduler_state{};
uint64_t g_sdma_work_queue_head = 0;
uint64_t g_sdma_work_queue_tail = 0;
uint64_t g_tsc_freq_500ns = 0;  // Initialized at startup
uint8_t  g_ssot_full_beacon = 0;

// ============================================================
// SINGLETON INSTANCE
// ============================================================
static SDMACoordinator* g_sdma_coordinator = nullptr;

SDMACoordinator& SDMACoordinator::instance() {
    if (!g_sdma_coordinator) {
        g_sdma_coordinator = new SDMACoordinator();
    }
    return *g_sdma_coordinator;
}

// ============================================================
// INITIALIZATION
// ============================================================
bool SDMACoordinator::initialize(
    void* ring_base,
    uint64_t ring_gpu_addr,
    volatile uint32_t* mmio_wptr,
    volatile uint32_t* mmio_rptr
) {
    if (m_initialized.exchange(true, std::memory_order_acq_rel)) {
        return false;  // Already initialized
    }
    
    // Copy scheduler state for MASM
    g_sdma_scheduler_state.ring_base = ring_base;
    g_sdma_scheduler_state.ring_gpu_addr = ring_gpu_addr;
    g_sdma_scheduler_state.mmio_wptr = mmio_wptr;
    g_sdma_scheduler_state.mmio_rptr = mmio_rptr;
    g_sdma_scheduler_state.head = 0;
    g_sdma_scheduler_state.tail_cache = 0;
    g_sdma_scheduler_state.burst_accumulator = 0;
    g_sdma_scheduler_state.descriptors_submitted = 0;
    g_sdma_scheduler_state.bytes_moved = 0;
    g_sdma_scheduler_state.coalescing_hits = 0;
    g_sdma_scheduler_state.scheduling_stalls = 0;
    
    // Calculate TSC frequency for 500ns deadline calculations
    // Assume ~3GHz CPU: 500ns = 1500 TSC ticks
    // (In production, measure RDTSC frequency at boot)
    const uint64_t assumed_freq_hz = 3000000000ULL;
    g_tsc_freq_500ns = (assumed_freq_hz / 2000000ULL);  // 500ns in ticks
    
    return true;
}

// ============================================================
// WORK QUEUE SUBMISSION (lock-free MPSC)
// ============================================================
bool SDMACoordinator::submit_transfer(
    uint64_t src_gpu_va,
    uint64_t dst_gpu_va,
    uint64_t size_bytes,
    bool wait_for_completion
) {
    if (!m_running.load(std::memory_order_acquire)) {
        return false;  // Scheduler not running
    }
    
    // Allocate slot in work queue
    uint32_t head = m_work_head.load(std::memory_order_acquire);
    uint32_t tail = m_work_tail.load(std::memory_order_acquire);
    
    // Check for space (leave 1 slot gap)
    uint32_t next_head = (head + 1) % WORK_QUEUE_SIZE;
    if (next_head == tail) {
        return false;  // Queue full
    }
    
    // Write work item
    SDMAWorkItem& item = m_work_queue[head];
    item.src_gpu_va = src_gpu_va;
    item.dst_gpu_va = dst_gpu_va;
    item.size_bytes = size_bytes;
    item.flags = wait_for_completion ? 0x1 : 0x0;
    item.completion_fence = 0;
    
    // Advance head (publish)
    m_work_head.store(next_head, std::memory_order_release);
    g_sdma_work_queue_head = (uint64_t)next_head * 64;  // Convert to byte offset
    
    return true;
}

// ============================================================
// THREAD MANAGEMENT
// ============================================================
bool SDMACoordinator::start_scheduler_thread() {
    if (m_running.exchange(true, std::memory_order_acq_rel)) {
        return false;  // Already running
    }
    
    // Create thread on isolated core 15
    HANDLE hThread = CreateThread(
        nullptr,  // lpThreadAttributes
        0,        // dwStackSize (default)
        [](LPVOID param) -> DWORD {
            SDMACoordinator* self = static_cast<SDMACoordinator*>(param);
            
            // Bind to core 15
            HANDLE hCurrent = GetCurrentThread();
            DWORD_PTR mask = 1ULL << 15;  // Core 15
            if (!SetThreadAffinityMask(hCurrent, mask)) {
                OutputDebugStringW(L"[SDMA] Failed to set thread affinity\n");
                return 1;
            }
            
            // Raise priority
            if (!SetThreadPriority(hCurrent, THREAD_PRIORITY_TIME_CRITICAL)) {
                OutputDebugStringW(L"[SDMA] Failed to raise thread priority\n");
                return 1;
            }
            
            OutputDebugStringW(L"[SDMA] Scheduler started on core 15\n");
            
            // Call MASM scheduler loop (never returns)
            sdma_scheduler_entry();
            
            return 0;
        },
        this,
        CREATE_SUSPENDED,  // Start suspended
        nullptr
    );
    
    if (!hThread) {
        m_running.store(false, std::memory_order_release);
        return false;
    }
    
    m_scheduler_thread = hThread;
    
    // Resume thread
    if (ResumeThread(hThread) == (DWORD)-1) {
        CloseHandle(hThread);
        m_scheduler_thread = nullptr;
        m_running.store(false, std::memory_order_release);
        return false;
    }
    
    return true;
}

// ============================================================
// SHUTDOWN
// ============================================================
void SDMACoordinator::stop_scheduler() {
    m_running.store(false, std::memory_order_release);
    
    if (m_scheduler_thread) {
        // Wait for graceful shutdown (timeout 1s)
        WaitForSingleObject(m_scheduler_thread, 1000);
        CloseHandle(m_scheduler_thread);
        m_scheduler_thread = nullptr;
    }
}

// ============================================================
// STATISTICS
// ============================================================
SDMASchedulerState SDMACoordinator::get_stats() const {
    return g_sdma_scheduler_state;
}

bool SDMACoordinator::is_beacon_full() const {
    return g_ssot_full_beacon == 0x3;
}

// ============================================================
// DESTRUCTOR
// ============================================================
SDMACoordinator::~SDMACoordinator() {
    stop_scheduler();
}

} // namespace rawrxd::sdma
