/**
 * @file sdma_coordinator.hpp
 * @brief SDMA Scheduler Coordination & Thread Management (Phase 1)
 * Bridges MASM SDMA scheduler loop with C++ work queue + beacon gating
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <windows.h>

namespace rawrxd::sdma {

// ============================================================
// SDMA WORK QUEUE ENTRY (64 bytes, zero-copy descriptor)
// ============================================================
struct alignas(64) SDMAWorkItem {
    uint64_t src_gpu_va;        // [0-7]   Source GPU VA (BAR-mapped or device)
    uint64_t dst_gpu_va;        // [8-15]  Destination GPU VA
    uint64_t size_bytes;        // [16-23] Transfer size (bytes, 4-aligned)
    uint32_t flags;             // [24-27] FENCE | INTERRUPT | PRIORITY
    uint32_t completion_fence;  // [28-31] Fence value to write on completion
    uint8_t  padding[32];       // [32-63] Pad to 64 bytes
};

static_assert(sizeof(SDMAWorkItem) == 64, "SDMAWorkItem must be 64 bytes");

// ============================================================
// SDMA SCHEDULER STATE (exported to MASM)
// ============================================================
struct alignas(64) SDMASchedulerState {
    void* ring_base;            // Host VA (Write-Combined)
    uint64_t ring_gpu_addr;     // GPU-visible bus address
    uint64_t head;              // Write pointer (bytes, 32-aligned)
    uint64_t tail_cache;        // Cached read pointer
    volatile uint32_t* mmio_wptr;  // GPU MMIO: SDMA0_QUEUE0_RB_WPTR
    volatile uint32_t* mmio_rptr;  // GPU MMIO: SDMA0_QUEUE0_RB_RPTR
    
    // Burst scheduling state
    uint64_t burst_accumulator;
    uint64_t burst_deadline;    // RDTSC deadline
    uint64_t pending_desc_count;
    uint64_t last_src;
    
    // Statistics
    std::atomic<uint64_t> descriptors_submitted;
    std::atomic<uint64_t> bytes_moved;
    std::atomic<uint64_t> coalescing_hits;
    std::atomic<uint64_t> scheduling_stalls;
};

// ============================================================
// SDMA COORDINATOR - Global singleton
// ============================================================
class SDMACoordinator {
public:
    static SDMACoordinator& instance();
    
    /// Initialize scheduler state (called once at startup)
    bool initialize(
        void* ring_base,
        uint64_t ring_gpu_addr,
        volatile uint32_t* mmio_wptr,
        volatile uint32_t* mmio_rptr
    );
    
    /// Submit inference transfer to work queue
    /// Returns: true if enqueued, false if queue is full
    bool submit_transfer(
        uint64_t src_gpu_va,
        uint64_t dst_gpu_va,
        uint64_t size_bytes,
        bool wait_for_completion = false
    );
    
    /// Spawn isolated scheduler thread on core 15
    bool start_scheduler_thread();
    
    /// Stop scheduler gracefully
    void stop_scheduler();
    
    /// Get scheduler statistics
    SDMASchedulerState get_stats() const;
    
    /// Check if beacon is full (0x3) for dispatch gating
    bool is_beacon_full() const;
    
private:
    SDMACoordinator() = default;
    ~SDMACoordinator();
    
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
    HANDLE m_scheduler_thread{nullptr};
    
    // Work queue (lock-free MPSC ring)
    static constexpr uint32_t WORK_QUEUE_SIZE = 16384; // 16K entries
    alignas(64) SDMAWorkItem m_work_queue[WORK_QUEUE_SIZE];
    std::atomic<uint32_t> m_work_head{0};
    std::atomic<uint32_t> m_work_tail{0};
};

// ============================================================
// MASM EXTERN DECLARATIONS
// ============================================================
extern "C" {
    // Scheduler state (defined in sdma_scheduler.asm)
    extern SDMASchedulerState g_sdma_scheduler_state;
    
    // Work queue pointers (modified by MASM)
    extern uint64_t g_sdma_work_queue_head;
    extern uint64_t g_sdma_work_queue_tail;
    
    // TSC frequency constant
    extern uint64_t g_tsc_freq_500ns;
    extern uint8_t  g_ssot_full_beacon;
    
    // Entry point
    void sdma_scheduler_entry();
    void sdma_scheduler_init_state(
        void* ring_base,
        uint64_t ring_gpu_addr,
        volatile uint32_t* mmio_wptr,
        volatile uint32_t* mmio_rptr
    );
    
    // Ring allocator
    int64_t allocate_tensor_slot(uint64_t size_bytes);
    void free_tensor_slot(uint64_t slot_index, uint32_t slot_count);
    uint64_t sdma_ring_advance_head(uint64_t head, uint32_t count);
}

} // namespace rawrxd::sdma
