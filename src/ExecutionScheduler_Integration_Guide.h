// ExecutionScheduler_Integration_Guide.h
// Step-by-step integration of 10 bottleneck fixes into existing RawrXD codebase
#pragma once

#include "ExecutionScheduler_v2.h"

namespace RawrXD {

// ============================================================================
// INTEGRATION OVERVIEW
// ============================================================================
//
// This file provides a migration path from ExecutionScheduler (v1) to
// ExecutionScheduler_v2 with all 10 bottleneck fixes.
//
// Migration Strategy:
// 1. Gradual rollout via feature flags
// 2. Backward compatibility layer
// 3. Performance validation at each step
// 4. Full cutover once validated

// ============================================================================
// BOTTLENECK FIX #1: Global Lock Contention
// ============================================================================
//
// BEFORE: std::mutex + std::deque per worker
// AFTER: LockFreeWorkStealingQueue per worker
//
// Integration Steps:
// 1. Replace WorkerQueue in ExecutionScheduler.h
// 2. Update EnqueueTask to use lock-free push
// 3. Update WorkerLoop to use lock-free pop/steal

class LockContentionFix {
public:
    // Drop-in replacement for existing queue
    template<typename Task>
    using QueueType = LockFreeWorkStealingQueue<Task, 1024>;
    
    // Migration helper: gradually migrate tasks
    static void migrate_queues(ExecutionScheduler* old_scheduler,
                               ExecutionScheduler_v2* new_scheduler);
};

// ============================================================================
// BOTTLENECK FIX #2: O(V+E) Cycle Detection  
// ============================================================================
//
// BEFORE: DFS validation on every submit
// AFTER: IncrementalDAG with atomic dependency counters
//
// Integration Steps:
// 1. Add IncrementalDAG member to scheduler
// 2. Replace submit() to use DAG tracking
// 3. Update task completion to notify children

class CycleDetectionFix {
public:
    // Wraps existing task submission with DAG tracking
    static TaskID submit_with_tracking(
        ExecutionScheduler_v2* scheduler,
        std::function<void()> work,
        const std::vector<TaskID>& deps
    );
    
    // Validates DAG is acyclic (incremental)
    static bool validate_incremental(const IncrementalDAG& dag);
};

// ============================================================================
// BOTTLENECK FIX #3: Phase Transition Barriers
// ============================================================================
//
// BEFORE: Mutex + condition variable blocking
// AFTER: PhaseStateMachine with atomic versioned state
//
// Integration Steps:
// 1. Replace phase state variables with PhaseStateMachine
// 2. Update phase transition logic
// 3. Make phase queries wait-free

class PhaseTransitionFix {
public:
    // Maps old phase enum to new
    static ExecutionPhase map_phase(int old_phase);
    
    // Non-blocking phase transition
    static bool transition_phase(PhaseStateMachine* machine,
                                  ExecutionPhase new_phase);
    
    // Wait-free phase query
    static ExecutionPhase current_phase(const PhaseStateMachine& machine);
};

// ============================================================================
// BOTTLENECK FIX #4: Per-Task Allocation
// ============================================================================
//
// BEFORE: new/delete per task (heap allocation)
// AFTER: SlabAllocator with preallocated blocks
//
// Integration Steps:
// 1. Add SlabAllocator member to scheduler
// 2. Replace new Task with slab.allocate()
// 3. Replace delete with slab.deallocate()

class AllocationFix {
public:
    using TaskAllocator = SlabAllocator<ExecTask, 4096>;
    
    // Allocate task from slab
    static ExecTask* allocate_task(TaskAllocator& allocator);
    
    // Return task to slab
    static void deallocate_task(TaskAllocator& allocator, ExecTask* task);
    
    // Pre-allocate blocks for expected load
    static void preallocate(TaskAllocator& allocator, size_t num_blocks);
};

// ============================================================================
// BOTTLENECK FIX #5: String-Keyed Registry
// ============================================================================
//
// BEFORE: std::map<std::string, Kernel>
// AFTER: Integer-hashed IDs with FNV-1a
//
// Integration Steps:
// 1. Replace string keys with TaskID/KernelID
// 2. Add compile-time hash for kernel names
// 3. Update all lookups to use integer IDs

class RegistryFix {
public:
    // Compile-time string hash
    template<const char* Name>
    static constexpr KernelID kernel_id() {
        return static_cast<KernelID>(fnv1a_hash(Name));
    }
    
    // Runtime hash (for dynamic names)
    static KernelID hash_runtime(const std::string& name);
    
    // Registry using integer keys
    template<typename T>
    using IntRegistry = std::unordered_map<KernelID, T>;
};

// ============================================================================
// BOTTLENECK FIX #6: KV + Scheduler Coupling
// ============================================================================
//
// BEFORE: Scheduler blocks on KV readiness
// AFTER: Async deferred command queue
//
// Integration Steps:
// 1. Add CommandPacketQueue to scheduler
// 2. Convert KV calls to deferred commands
// 3. Add background processor thread

class KVCouplingFix {
public:
    // Enqueue KV read (non-blocking)
    static void enqueue_kv_read(CommandPacketQueue& queue,
                                   uint64_t key,
                                   size_t size,
                                   std::function<void(void*)> callback);
    
    // Enqueue KV write (non-blocking)
    static void enqueue_kv_write(CommandPacketQueue& queue,
                                    uint64_t key,
                                    const void* data,
                                    size_t size);
    
    // Process pending KV commands
    static void process_kv_commands(CommandPacketQueue& queue);
};

// ============================================================================
// BOTTLENECK FIX #7: Synchronous Telemetry
// ============================================================================
//
// BEFORE: Direct file write in hot path
// AFTER: Lock-free ring buffer + background flush
//
// Integration Steps:
// 1. Add LockFreeTelemetryRing to scheduler
// 2. Replace direct logging with ring push
// 3. Add background telemetry thread

class TelemetryFix {
public:
    using TelemetryBuffer = LockFreeTelemetryRing<65536>;
    
    // Non-blocking event record
    static void record_event(TelemetryBuffer& buffer,
                             const TelemetryEvent& event);
    
    // Background flush (called from dedicated thread)
    static void flush_events(TelemetryBuffer& buffer);
    
    // Initialize telemetry subsystem
    static void init_telemetry(TelemetryBuffer& buffer);
};

// ============================================================================
// BOTTLENECK FIX #8: Mixed Precision Timing
// ============================================================================
//
// BEFORE: std::chrono::steady_clock (system call)
// AFTER: RDTSC monotonic clock (~10ns)
//
// Integration Steps:
// 1. Replace all timing calls with MonotonicClock
// 2. Calibrate TSC at startup
// 3. Normalize all timestamps

class TimingFix {
public:
    using Clock = MonotonicClock;
    using TimePoint = MonotonicClock::TimePoint;
    using Duration = MonotonicClock::Duration;
    
    // Calibrate TSC against system clock
    static void calibrate();
    
    // Get current time (RDTSC)
    static TimePoint now();
    
    // Convert to microseconds
    static double microseconds(Duration d);
    
    // Convert to milliseconds
    static double milliseconds(Duration d);
};

// ============================================================================
// BOTTLENECK FIX #9: Inline Dependency Expansion
// ============================================================================
//
// BEFORE: Scheduler directly calls GPU/KV/inference
// AFTER: Deferred command packets
//
// Integration Steps:
// 1. Define DeferredCommand types for each engine
// 2. Add command packet queue
// 3. Add processor threads for each engine

class DependencyExpansionFix {
public:
    // Create inference command
    static DeferredCommand create_inference_cmd(
        const void* input_data,
        size_t input_size,
        std::function<void(const void*, size_t)> callback
    );
    
    // Create GPU dispatch command
    static DeferredCommand create_gpu_cmd(
        uint32_t kernel_id,
        const void* args,
        size_t arg_size
    );
    
    // Process all pending commands
    static void process_commands(CommandPacketQueue& queue);
};

// ============================================================================
// BOTTLENECK FIX #10: Phase Budget Enforcement
// ============================================================================
//
// BEFORE: No execution budget per phase
// AFTER: Hard PhaseBudget with spillover
//
// Integration Steps:
// 1. Add PhaseBudget array to scheduler
// 2. Check budget before task execution
// 3. Spillover to next tick when exceeded

class PhaseBudgetFix {
public:
    // Initialize budgets for all phases
    static void init_budgets(std::array<PhaseBudget, 5>& budgets);
    
    // Check if phase has remaining budget
    static bool has_budget(const PhaseBudget& budget);
    
    // Execute with budget check
    template<typename F>
    static bool execute_with_budget(PhaseBudget& budget, F&& work) {
        if (!has_budget(budget)) return false;
        
        auto start = MonotonicClock::now();
        work();
        budget.update();
        
        return true;
    }
    
    // Get remaining budget
    static MonotonicClock::Duration remaining(const PhaseBudget& budget);
};

// ============================================================================
// MIGRATION WRAPPER
// ============================================================================

class ExecutionScheduler_Migration {
public:
    // Feature flag for gradual rollout
    static std::atomic<bool> use_v2;
    
    // Initialize v2 scheduler
    static void init_v2(int num_threads);
    
    // Submit task (routes to v1 or v2 based on flag)
    static TaskID submit(std::function<void()> work,
                         uint32_t priority = 5,
                         const std::vector<TaskID>& deps = {});
    
    // Shutdown both schedulers
    static void shutdown();
    
    // Get v2 scheduler instance
    static ExecutionScheduler_v2* v2_scheduler();
    
    // Get v1 scheduler instance
    static ExecutionScheduler* v1_scheduler();
    
    // Validate v2 performance vs v1
    static bool validate_performance();
};

// ============================================================================
// PERFORMANCE VALIDATION
// ============================================================================

struct BottleneckMetrics {
    // Fix #1: Lock contention
    double avg_queue_wait_us;
    double max_queue_wait_us;
    uint64_t steal_events;
    
    // Fix #2: Cycle detection
    double avg_submit_latency_us;
    uint64_t dag_nodes_tracked;
    
    // Fix #3: Phase transitions
    double phase_transition_latency_us;
    uint64_t phase_transitions;
    
    // Fix #4: Allocation
    double avg_allocation_latency_ns;
    uint64_t slab_allocations;
    
    // Fix #5: Registry
    double avg_lookup_latency_ns;
    uint64_t registry_hits;
    
    // Fix #6: KV coupling
    double kv_queue_depth;
    uint64_t kv_commands_async;
    
    // Fix #7: Telemetry
    double telemetry_latency_ns;
    uint64_t telemetry_events_dropped;
    
    // Fix #8: Timing
    double timing_precision_ns;
    
    // Fix #9: Dependencies
    double command_queue_depth;
    uint64_t deferred_commands;
    
    // Fix #10: Budgets
    double budget_exceeded_rate;
    MonotonicClock::Duration avg_phase_overrun_ns;
};

class PerformanceValidator {
public:
    // Run comprehensive benchmark
    static BottleneckMetrics benchmark();
    
    // Compare v1 vs v2
    static bool validate_improvement(const BottleneckMetrics& v1,
                                      const BottleneckMetrics& v2);
    
    // Generate report
    static std::string generate_report(const BottleneckMetrics& metrics);
};

} // namespace RawrXD
