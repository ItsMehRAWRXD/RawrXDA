// ExecutionScheduler_v2.h — Lock-free, budgeted, event-driven execution fabric
// Addresses 10 critical bottlenecks:
// 1. Global lock contention → Lock-free work-stealing
// 2. O(V+E) cycle detection → Incremental topological validation
// 3. Phase transition barriers → Atomic versioned state
// 4. Per-task allocation → Slab allocator
// 5. String-keyed registry → Integer hashed IDs
// 6. KV + Scheduler coupling → Async demand/response
// 7. Synchronous telemetry → Ring-buffer pipeline
// 8. Mixed timing sources → TSC monotonic clock
// 9. Inline dependency expansion → Deferred command packets
// 10. No phase budgets → Hard per-phase budget enforcement
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <optional>
#include <chrono>
#include <array>
#include <variant>
#include <mutex>

// Platform-specific intrinsics for TSC
#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#ifdef CALLBACK
#undef CALLBACK
#endif
#else
#include <x86intrin.h>
#endif

namespace RawrXD {

// ============================================================================
// BOTTLENECK FIX #8: Single Monotonic Clock Source (TSC)
// ============================================================================

class MonotonicClock {
public:
    using TimePoint = uint64_t;
    using Duration = uint64_t;
    
    static TimePoint now() {
        // Use RDTSC for ultra-low-latency timing
        // In production, calibrate against QueryPerformanceCounter
        return __rdtsc();
    }
    
    static Duration nanoseconds(TimePoint t) {
        // Calibrated conversion (would be calibrated at startup)
        static double ns_per_tick = 0.3; // Approximate for 3.3GHz
        return static_cast<Duration>(t * ns_per_tick);
    }
    
    static Duration microseconds(TimePoint t) {
        return nanoseconds(t) / 1000;
    }
    
    static Duration milliseconds(TimePoint t) {
        return nanoseconds(t) / 1000000;
    }
};

// ============================================================================
// BOTTLENECK FIX #5: Integer-Hashed Registry (no string lookups)
// ============================================================================

using TaskID = uint64_t;
using PhaseID = uint32_t;
using KernelID = uint32_t;

constexpr TaskID INVALID_TASK_ID = 0;
constexpr KernelID INVALID_KERNEL_ID = 0;

// FNV-1a hash for compile-time string hashing
constexpr uint64_t fnv1a_hash(const char* str, uint64_t hash = 14695981039346656037ULL) {
    return *str == 0 ? hash : fnv1a_hash(str + 1, (hash ^ static_cast<uint64_t>(*str)) * 1099511628211ULL);
}

// ============================================================================
// BOTTLENECK FIX #10: Phase Budget Enforcement
// ============================================================================

enum class ExecutionPhase : uint8_t {
    IDLE = 0,
    PREFILL = 1,
    DECODE = 2,
    TAIL = 3,
    CLEANUP = 4
};

struct PhaseBudget {
    MonotonicClock::Duration max_ns{0};
    MonotonicClock::Duration used_ns{0};
    MonotonicClock::TimePoint start_time{0};
    std::atomic<bool> exceeded{false};
    
    void begin() {
        start_time = MonotonicClock::now();
        used_ns = 0;
        exceeded.store(false, std::memory_order_relaxed);
    }
    
    void update() {
        auto now = MonotonicClock::now();
        used_ns = MonotonicClock::nanoseconds(now - start_time);
        if (used_ns > max_ns) {
            exceeded.store(true, std::memory_order_relaxed);
        }
    }
    
    bool has_budget() const {
        return !exceeded.load(std::memory_order_relaxed);
    }
    
    MonotonicClock::Duration remaining_ns() const {
        if (exceeded.load(std::memory_order_relaxed)) return 0;
        auto remaining = max_ns - used_ns;
        return remaining > 0 ? remaining : 0;
    }
};

// ============================================================================
// BOTTLENECK FIX #4: Slab Allocator for Tasks
// ============================================================================

template<typename T, size_t BlockSize = 4096>
class SlabAllocator {
public:
    struct Block {
        alignas(64) std::array<T, BlockSize> items;
        alignas(64) std::array<std::atomic<bool>, BlockSize> used;
        std::atomic<size_t> next_slot{0};
        
        Block() {
            for (auto& u : used) u.store(false, std::memory_order_relaxed);
        }
    };
    
    SlabAllocator() {
        // Pre-allocate initial block
        blocks_.push_back(std::make_unique<Block>());
    }
    
    T* allocate() {
        // Try existing blocks first
        for (auto& block : blocks_) {
            size_t slot = block->next_slot.fetch_add(1, std::memory_order_relaxed);
            if (slot < BlockSize) {
                bool expected = false;
                if (block->used[slot].compare_exchange_strong(
                    expected, true, std::memory_order_acquire)) {
                    return &block->items[slot];
                }
            }
        }
        
        // Allocate new block
        auto new_block = std::make_unique<Block>();
        T* result = &new_block->items[0];
        new_block->used[0].store(true, std::memory_order_relaxed);
        new_block->next_slot.store(1, std::memory_order_relaxed);
        
        std::lock_guard<std::mutex> lock(blocks_mutex_);
        blocks_.push_back(std::move(new_block));
        return result;
    }
    
    void deallocate(T* ptr) {
        // Mark as unused (actual cleanup optional for performance)
        // In production, would track which block/slot
        (void)ptr;
    }
    
private:
    std::vector<std::unique_ptr<Block>> blocks_;
    std::mutex blocks_mutex_;
};

// ============================================================================
// BOTTLENECK FIX #1: Lock-Free Work-Stealing Queue
// ============================================================================

template<typename T, size_t Capacity = 1024>
class LockFreeWorkStealingQueue {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    LockFreeWorkStealingQueue() : head_(0), tail_(0) {}
    
    // Producer: push to tail (local thread)
    bool push(T item) {
        size_t t = tail_.load(std::memory_order_relaxed);
        size_t h = head_.load(std::memory_order_acquire);
        
        if (t - h >= Capacity) return false; // Full
        
        buffer_[t & (Capacity - 1)] = std::move(item);
        tail_.store(t + 1, std::memory_order_release);
        return true;
    }
    
    // Consumer: pop from tail (local thread)
    std::optional<T> pop() {
        size_t t = tail_.load(std::memory_order_relaxed) - 1;
        tail_.store(t, std::memory_order_relaxed);
        
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        size_t h = head_.load(std::memory_order_relaxed);
        if (t < h) {
            tail_.store(h, std::memory_order_relaxed);
            return std::nullopt;
        }
        
        T item = std::move(buffer_[t & (Capacity - 1)]);
        if (t > h) return item;
        
        // Last item, race with steal
        if (!head_.compare_exchange_strong(h, h + 1,
            std::memory_order_seq_cst, std::memory_order_relaxed)) {
            tail_.store(h + 1, std::memory_order_relaxed);
            return std::nullopt;
        }
        
        return item;
    }
    
    // Thief: steal from head (other threads)
    std::optional<T> steal() {
        size_t h = head_.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t t = tail_.load(std::memory_order_acquire);
        
        if (h >= t) return std::nullopt;
        
        T item = buffer_[h & (Capacity - 1)];
        if (head_.compare_exchange_strong(h, h + 1,
            std::memory_order_seq_cst, std::memory_order_relaxed)) {
            return item;
        }
        return std::nullopt;
    }
    
    size_t size() const {
        return tail_.load(std::memory_order_relaxed) - 
               head_.load(std::memory_order_relaxed);
    }
    
    bool empty() const {
        return size() == 0;
    }
    
private:
    alignas(64) std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

// ============================================================================
// BOTTLENECK FIX #2: Incremental Topological Validation
// ============================================================================

struct TaskNode {
    std::atomic<int32_t> dependency_count{0};
    std::atomic<int32_t> remaining_deps{0};
    std::atomic<TaskNode*> next_ready{nullptr};
    std::vector<TaskNode*> children;
    std::function<void()> work;
    TaskID id{INVALID_TASK_ID};
    std::atomic<bool> completed{false};
    
    // Called when a parent completes
    bool on_parent_complete() {
        int32_t remaining = remaining_deps.fetch_sub(1, std::memory_order_acq_rel) - 1;
        return remaining == 0;
    }
    
    void reset(int32_t deps) {
        dependency_count.store(deps, std::memory_order_relaxed);
        remaining_deps.store(deps, std::memory_order_relaxed);
        completed.store(false, std::memory_order_relaxed);
        next_ready.store(nullptr, std::memory_order_relaxed);
    }
};

class IncrementalDAG {
public:
    // O(1) task submission with incremental validation
    TaskNode* submit_with_deps(std::function<void()> work, 
                               const std::vector<TaskNode*>& deps);
    
    // O(1) ready queue population
    void notify_completion(TaskNode* node);
    
    // Lock-free ready queue access
    TaskNode* pop_ready();
    
private:
    // Ready queue (lock-free stack for simplicity)
    alignas(64) std::atomic<TaskNode*> ready_head_{nullptr};
    
    void push_ready(TaskNode* node) {
        TaskNode* expected = nullptr;
        do {
            expected = ready_head_.load(std::memory_order_relaxed);
            node->next_ready.store(expected, std::memory_order_relaxed);
        } while (!ready_head_.compare_exchange_weak(
            expected, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }
};

// ============================================================================
// BOTTLENECK FIX #9: Deferred Command Packets
// ============================================================================

enum class CommandType : uint8_t {
    INFERENCE_REQUEST = 0,
    KV_READ = 1,
    KV_WRITE = 2,
    GPU_DISPATCH = 3,
    CALLBACK = 4
};

struct DeferredCommand {
    CommandType type;
    MonotonicClock::TimePoint enqueue_time;
    MonotonicClock::TimePoint deadline;
    uint32_t priority;
    std::variant<
        std::function<void()>,           // CALLBACK
        std::pair<const void*, size_t>,  // INFERENCE_REQUEST (data, size)
        std::pair<uint64_t, size_t>,     // KV_READ/WRITE (key, size)
        uint32_t                         // GPU_DISPATCH (kernel_id)
    > payload;
};

class CommandPacketQueue {
public:
    void enqueue(DeferredCommand cmd);
    std::optional<DeferredCommand> dequeue();
    std::optional<DeferredCommand> dequeue_before(MonotonicClock::TimePoint deadline);
    
private:
    LockFreeWorkStealingQueue<DeferredCommand, 4096> queue_;
};

// ============================================================================
// BOTTLENECK FIX #7: Ring-Buffer Telemetry Pipeline
// ============================================================================

struct TelemetryEvent {
    MonotonicClock::TimePoint timestamp;
    TaskID task_id;
    uint8_t event_type; // 0=start, 1=complete, 2=steal, 3=budget_exceeded
    uint32_t worker_id;
    MonotonicClock::Duration duration_ns;
};

template<size_t Capacity = 65536>
class LockFreeTelemetryRing {
public:
    bool push(const TelemetryEvent& event) {
        size_t h = head_.load(std::memory_order_relaxed);
        size_t next = (h + 1) % Capacity;
        
        if (next == tail_.load(std::memory_order_acquire)) return false;
        
        buffer_[h] = event;
        head_.store(next, std::memory_order_release);
        return true;
    }
    
    std::optional<TelemetryEvent> pop() {
        size_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return std::nullopt;
        
        TelemetryEvent event = buffer_[t];
        tail_.store((t + 1) % Capacity, std::memory_order_release);
        return event;
    }
    
private:
    alignas(64) std::array<TelemetryEvent, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
};

// ============================================================================
// BOTTLENECK FIX #3: Atomic Versioned Phase State
// ============================================================================

class PhaseStateMachine {
public:
    struct State {
        ExecutionPhase phase;
        uint32_t version;
        MonotonicClock::TimePoint transition_time;
    };
    
    // Atomic transition with version bump
    bool transition_to(ExecutionPhase new_phase);
    
    // Lock-free state read
    State current_state() const;
    
    // Wait-free phase query
    ExecutionPhase current_phase() const {
        return decode_state(state_.load(std::memory_order_acquire)).phase;
    }
    
private:
    alignas(64) std::atomic<uint64_t> state_{0};
    
    static uint64_t encode_state(const State& s);
    static State decode_state(uint64_t encoded);
};

// ============================================================================
// MAIN EXECUTION SCHEDULER V2
// ============================================================================

class ExecutionScheduler_v2 {
public:
    explicit ExecutionScheduler_v2(int num_threads = 0);
    ~ExecutionScheduler_v2();
    
    static ExecutionScheduler_v2& Instance();
    
    // Task submission (lock-free)
    TaskID submit(std::function<void()> work, 
                  uint32_t priority = 5,
                  const std::vector<TaskID>& deps = {});
    
    // Phase-bounded execution
    void execute_phase(ExecutionPhase phase, const PhaseBudget& budget);
    
    // Deferred command submission (decouples scheduler from engines)
    void submit_deferred(DeferredCommand cmd);
    
    // Telemetry (non-blocking)
    void record_telemetry(const TelemetryEvent& event);
    
    // Budget queries
    PhaseBudget& budget_for(ExecutionPhase phase);
    const PhaseBudget& budget_for(ExecutionPhase phase) const;
    
    // Stats
    struct Stats {
        std::atomic<uint64_t> tasks_submitted{0};
        std::atomic<uint64_t> tasks_completed{0};
        std::atomic<uint64_t> tasks_stolen{0};
        std::atomic<uint64_t> budget_exceeded{0};
        std::atomic<uint64_t> deferred_commands{0};
    };
    const Stats& stats() const { return stats_; }
    
    void shutdown();
    void wait_all();
    
private:
    struct Worker {
        LockFreeWorkStealingQueue<std::function<void()>, 1024> local_queue;
        std::thread thread;
        std::atomic<bool> running{true};
        uint32_t id{0};
    };
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<bool> shutdown_{false};
    
    // Phase budgets
    std::array<PhaseBudget, 5> phase_budgets_;
    PhaseStateMachine phase_state_;
    
    // Incremental DAG
    IncrementalDAG dag_;
    SlabAllocator<TaskNode, 4096> task_slab_;
    std::atomic<TaskID> next_task_id_{1};
    
    // Deferred commands
    CommandPacketQueue deferred_queue_;
    std::thread deferred_processor_;
    
    // Telemetry
    LockFreeTelemetryRing<65536> telemetry_ring_;
    std::thread telemetry_flusher_;
    std::atomic<bool> telemetry_running_{true};
    
    // Stats
    Stats stats_;
    
    void worker_loop(uint32_t worker_id);
    void process_deferred_commands();
    void flush_telemetry();
    bool steal_task(uint32_t thief_id, std::function<void()>& task);
};

} // namespace RawrXD
