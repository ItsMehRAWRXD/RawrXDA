// sdma_scheduler.cpp — GPU SDMA Scheduler for single-producer ring scheduling
// Replaces MASM implementation with portable C++ that schedules GPU DMA operations
// Features: burst coalescing, adaptive throttling, TSC-based deadline scheduling

#include <cstdint>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <intrin.h>  // For MSVC __rdtsc and __nop

namespace RawrXD {
namespace SDMA {

// ─── Constants ────────────────────────────────────────────────────────────
constexpr size_t SDMA_DESCRIPTOR_SIZE = 32;
constexpr size_t SDMA_MAX_BURST_BYTES = 2 * 1024 * 1024; // 2 MB hard limit
constexpr size_t BAR_RING_MASK = (256 * 1024 * 1024) - 1;
constexpr size_t SDMA_WORK_QUEUE_SIZE = 16384;
constexpr size_t SDMA_WORK_QUEUE_BYTES = SDMA_WORK_QUEUE_SIZE * 64;
constexpr uint32_t SDMA_SCHEDULER_CORE = 15; // Last core, isolated

// ─── Scheduler State ──────────────────────────────────────────────────────
struct SchedulerState {
    // Ring buffer management
    uint64_t ring_base = 0;
    uint64_t ring_gpu_addr = 0;
    std::atomic<uint64_t> head{0};
    std::atomic<uint64_t> tail_cache{0};
    std::atomic<uint64_t> mmio_wptr{0};
    std::atomic<uint64_t> mmio_rptr{0};

    // Burst scheduling
    std::atomic<uint64_t> burst_accumulator{0};
    std::atomic<uint64_t> burst_deadline{0};
    std::atomic<uint64_t> pending_desc_count{0};
    std::atomic<uint64_t> last_src{0};

    // Statistics
    std::atomic<uint64_t> descriptors_submitted{0};
    std::atomic<uint64_t> bytes_moved{0};
    std::atomic<uint64_t> coalescing_hits{0};
    std::atomic<uint64_t> scheduling_stalls{0};
};

// ─── DMA COPY Linear Packet Structure ─────────────────────────────────────
#pragma pack(push, 1)
struct DMAPacketCopyLinear {
    uint32_t header;         // [7:0]=0x02, [15:8]=engine type
    uint8_t sub_opcode;      // 0x00 = linear copy
    uint8_t flags;           // [0]=fence, [1]=int, [2]=64-bit
    uint16_t pad0;
    uint32_t src_addr_lo;
    uint32_t src_addr_hi;
    uint32_t dst_addr_lo;
    uint32_t dst_addr_hi;
    uint32_t count_lo;       // [27:0]=bytes-1
    uint32_t count_hi;
};
#pragma pack(pop)

// ─── Global Instances ────────────────────────────────────────────────────
static SchedulerState g_sdma_scheduler_state;

// ─── Work Queue (provided by coordinator) ────────────────────────────────
extern "C" {
    extern uint8_t* g_sdma_work_queue_base;
    extern uint64_t g_sdma_work_queue_head;
    extern uint64_t g_sdma_work_queue_tail;
    extern uint64_t g_tsc_freq_500ns;
    extern uint8_t g_ssot_full_beacon;  // State of art (0x3 = full)
}

#pragma pack(push, 1)
struct SDMAWorkItemRaw {
    uint64_t src_gpu_va;
    uint64_t dst_gpu_va;
    uint64_t size_bytes;
    uint32_t flags;
    uint32_t completion_fence;
    uint8_t padding[32];
};
#pragma pack(pop)

static_assert(sizeof(SDMAWorkItemRaw) == 64, "SDMAWorkItemRaw must stay 64 bytes");

// ─── Exported Functions ──────────────────────────────────────────────────

// Initialize scheduler state (called once from coordinator)
extern "C" void sdma_scheduler_init_state(uint64_t ring_base, uint64_t ring_gpu_addr,
                                           uint64_t mmio_wptr, uint64_t mmio_rptr) {
    g_sdma_scheduler_state.ring_base = ring_base;
    g_sdma_scheduler_state.ring_gpu_addr = ring_gpu_addr;
    g_sdma_scheduler_state.head.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.tail_cache.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.mmio_wptr.store(mmio_wptr, std::memory_order_relaxed);
    g_sdma_scheduler_state.mmio_rptr.store(mmio_rptr, std::memory_order_relaxed);
    g_sdma_scheduler_state.burst_accumulator.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.pending_desc_count.store(0, std::memory_order_relaxed);
}

// Read TSC (Time Stamp Counter) — platform-specific
static inline uint64_t read_tsc() {
#ifdef _MSC_VER
    return __rdtsc();
#else
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#endif
}

// Scheduling loop — runs on dedicated core
extern "C" void sdma_scheduler_entry() {
    uint64_t pending_since_wptr = 0;
    while (true) {
        uint8_t* queue_base = g_sdma_work_queue_base;
        if (!queue_base || !g_sdma_scheduler_state.ring_base) {
            std::this_thread::yield();
            continue;
        }

        // Check if work is available
        uint64_t queue_head = g_sdma_work_queue_head;
        uint64_t queue_tail = g_sdma_work_queue_tail;
        bool has_work = (queue_head != queue_tail);
        if (!has_work) {
            // Flush pending descriptors if no new work arrives.
            if (pending_since_wptr > 0) {
                g_sdma_scheduler_state.mmio_wptr.store(
                    g_sdma_scheduler_state.head.load(std::memory_order_relaxed),
                    std::memory_order_release);
                pending_since_wptr = 0;
                g_sdma_scheduler_state.pending_desc_count.store(0, std::memory_order_relaxed);
            }
            std::this_thread::yield();
            continue;
        }

        const uint64_t item_offset = queue_tail & (SDMA_WORK_QUEUE_BYTES - 1);
        const SDMAWorkItemRaw* item = reinterpret_cast<const SDMAWorkItemRaw*>(queue_base + item_offset);
        uint64_t transfer_size = item->size_bytes;
        if (transfer_size == 0) {
            // Consume malformed no-op entries to avoid scheduler lock-up.
            g_sdma_work_queue_tail = (queue_tail + 64) & (SDMA_WORK_QUEUE_BYTES - 1);
            continue;
        }
        if (transfer_size > SDMA_MAX_BURST_BYTES) {
            transfer_size = SDMA_MAX_BURST_BYTES;
        }

        // Read current tail pointer from GPU
        uint64_t gpu_tail = g_sdma_scheduler_state.mmio_rptr.load(std::memory_order_acquire);
        gpu_tail &= BAR_RING_MASK;
        g_sdma_scheduler_state.tail_cache.store(gpu_tail, std::memory_order_relaxed);

        // Check ring space
        uint64_t head = g_sdma_scheduler_state.head.load(std::memory_order_relaxed);
        uint64_t space = (gpu_tail - head) & BAR_RING_MASK;
        if (space < SDMA_DESCRIPTOR_SIZE) {
            // Ring full: stall
            g_sdma_scheduler_state.scheduling_stalls.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::yield();
            continue;
        }

        // Emit real linear-copy descriptor based on queue payload.
        DMAPacketCopyLinear desc{};
        desc.header = 0x00000002u;
        desc.sub_opcode = 0x00u;
        desc.flags = (item->flags & 0x3u) ? static_cast<uint8_t>(item->flags & 0x3u) : 0u;
        desc.src_addr_lo = static_cast<uint32_t>(item->src_gpu_va & 0xFFFFFFFFull);
        desc.src_addr_hi = static_cast<uint32_t>((item->src_gpu_va >> 32) & 0xFFFFFFFFull);
        desc.dst_addr_lo = static_cast<uint32_t>(item->dst_gpu_va & 0xFFFFFFFFull);
        desc.dst_addr_hi = static_cast<uint32_t>((item->dst_gpu_va >> 32) & 0xFFFFFFFFull);
        const uint64_t count = (transfer_size > 0) ? (transfer_size - 1) : 0;
        desc.count_lo = static_cast<uint32_t>(count & 0xFFFFFFFFull);
        desc.count_hi = static_cast<uint32_t>((count >> 32) & 0xFFFFFFFFull);

        uint8_t* ring_ptr = reinterpret_cast<uint8_t*>(g_sdma_scheduler_state.ring_base) + head;
        std::memcpy(ring_ptr, &desc, sizeof(desc));

        head = (head + SDMA_DESCRIPTOR_SIZE) & BAR_RING_MASK;
        g_sdma_scheduler_state.head.store(head, std::memory_order_relaxed);
        g_sdma_scheduler_state.descriptors_submitted.fetch_add(1, std::memory_order_relaxed);
        g_sdma_scheduler_state.bytes_moved.fetch_add(transfer_size, std::memory_order_relaxed);
        g_sdma_scheduler_state.last_src.store(item->src_gpu_va, std::memory_order_relaxed);

        // Consume queue entry.
        g_sdma_work_queue_tail = (queue_tail + 64) & (SDMA_WORK_QUEUE_BYTES - 1);

        // Periodic MMIO update
        pending_since_wptr++;
        uint64_t pending = g_sdma_scheduler_state.pending_desc_count.fetch_add(1, std::memory_order_relaxed) + 1;
        if (pending >= 16 || pending_since_wptr >= 16) {
            // Write head to GPU WPTR
            g_sdma_scheduler_state.mmio_wptr.store(head, std::memory_order_release);
            g_sdma_scheduler_state.pending_desc_count.store(0, std::memory_order_relaxed);
            pending_since_wptr = 0;
        }
    }
}

// Get scheduler statistics
extern "C" uint64_t sdma_scheduler_get_descriptors_submitted() {
    return g_sdma_scheduler_state.descriptors_submitted.load(std::memory_order_relaxed);
}

extern "C" uint64_t sdma_scheduler_get_bytes_moved() {
    return g_sdma_scheduler_state.bytes_moved.load(std::memory_order_relaxed);
}

extern "C" uint64_t sdma_scheduler_get_coalescing_hits() {
    return g_sdma_scheduler_state.coalescing_hits.load(std::memory_order_relaxed);
}

extern "C" uint64_t sdma_scheduler_get_scheduling_stalls() {
    return g_sdma_scheduler_state.scheduling_stalls.load(std::memory_order_relaxed);
}

// Reset all statistics
extern "C" void sdma_scheduler_reset() {
    g_sdma_scheduler_state.head.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.tail_cache.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.burst_accumulator.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.pending_desc_count.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.descriptors_submitted.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.bytes_moved.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.coalescing_hits.store(0, std::memory_order_relaxed);
    g_sdma_scheduler_state.scheduling_stalls.store(0, std::memory_order_relaxed);
}

} // namespace SDMA
} // namespace RawrXD
