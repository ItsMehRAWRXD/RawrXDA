// sdma_ring_allocator.cpp — GPU SDMA Ring Buffer & Tensor Slot Allocator
// Replaces MASM implementation with portable C++ for Windows/Linux compatibility
// Features: 256MB BAR ring buffer management, memoryless bump tensor slot allocation,
//           lock-free operations, atomic slot tracking

#include <cstdint>
#include <atomic>

namespace RawrXD {
namespace SDMA {

// ─── Constants ────────────────────────────────────────────────────────────
constexpr size_t SDMA_DESCRIPTOR_SIZE = 32;
constexpr size_t SDMA_MAX_IN_FLIGHT = 256;
constexpr size_t BAR_RING_SIZE = 256 * 1024 * 1024;  // 256 MB
constexpr size_t BAR_RING_MASK = BAR_RING_SIZE - 1;
constexpr size_t TENSOR_SLOT_GRANULARITY = 64 * 1024; // 64 KB
constexpr size_t TENSOR_ARENA_SLOTS = (16ULL * 1024 * 1024 * 1024) / TENSOR_SLOT_GRANULARITY; // 16GB / 64KB

// ─── Tensor Slot Map ──────────────────────────────────────────────────────
struct TensorSlotMap {
    std::atomic<uint64_t> next_free_slot{0};           // Bump allocator pointer
};

// ─── Global Instances ────────────────────────────────────────────────────
static TensorSlotMap g_tensor_slot_map;

// ─── Implementation ──────────────────────────────────────────────────────

// Advance ring head pointer by 'desc_count' descriptors
extern "C" uint64_t sdma_ring_advance_head(uint64_t current_head, uint32_t desc_count) {
    uint64_t bytes_to_add = static_cast<uint64_t>(desc_count) * SDMA_DESCRIPTOR_SIZE;
    uint64_t new_head = (current_head + bytes_to_add) & BAR_RING_MASK;
    return new_head;
}

// Allocate contiguous tensor slots from the bump allocator
extern "C" int64_t allocate_tensor_slot(uint64_t size_bytes) {
    // Convert bytes to slot count
    uint64_t slots_needed = (size_bytes + TENSOR_SLOT_GRANULARITY - 1) / TENSOR_SLOT_GRANULARITY;
    if (slots_needed > TENSOR_ARENA_SLOTS) return -1;

    uint64_t start = g_tensor_slot_map.next_free_slot.load(std::memory_order_relaxed);
    while (true) {
        if (start + slots_needed > TENSOR_ARENA_SLOTS) return -1;
        if (g_tensor_slot_map.next_free_slot.compare_exchange_weak(start, start + slots_needed, std::memory_order_relaxed)) {
            return static_cast<int64_t>(start);
        }
    }
}

// Reset ring allocator state
extern "C" void sdma_ring_reset() {
    g_tensor_slot_map.next_free_slot.store(0, std::memory_order_relaxed);
}

} // namespace SDMA
} // namespace RawrXD
