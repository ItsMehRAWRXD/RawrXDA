// ExecutionScheduler_v2_test.cpp — Smoke test for 10 bottleneck fixes
#include "ExecutionScheduler_v2.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include <assert>

using namespace RawrXD;

static std::atomic<int> g_tasks_completed{0};
static std::atomic<int> g_budget_exceeded{0};

void test_passed(const char* name) {
    std::cout << "[PASS] " << name << "\n";
}

void test_failed(const char* name) {
    std::cout << "[FAIL] " << name << "\n";
}

// ============================================================================
// TEST 1: Lock-Free Queue Operations
// ============================================================================
void test_lockfree_queue() {
    LockFreeWorkStealingQueue<int, 1024> queue;
    
    // Test push/pop
    for (int i = 0; i < 100; ++i) {
        assert(queue.push(i));
    }
    
    for (int i = 0; i < 100; ++i) {
        auto val = queue.pop();
        assert(val.has_value());
        assert(*val == i);
    }
    
    // Test steal
    for (int i = 0; i < 50; ++i) {
        queue.push(i);
    }
    
    auto stolen = queue.steal();
    assert(stolen.has_value());
    
    test_passed("Lock-free queue operations");
}

// ============================================================================
// TEST 2: Incremental DAG Validation
// ============================================================================
void test_incremental_dag() {
    IncrementalDAG dag;
    
    // Create chain: A -> B -> C
    auto* nodeA = dag.submit_with_deps([]{}, {});
    auto* nodeB = dag.submit_with_deps([]{}, {nodeA});
    auto* nodeC = dag.submit_with_deps([]{}, {nodeB});
    
    // A should be ready immediately
    auto* ready = dag.pop_ready();
    assert(ready == nodeA);
    
    // Complete A, B should become ready
    dag.notify_completion(nodeA);
    ready = dag.pop_ready();
    assert(ready == nodeB);
    
    // Complete B, C should become ready
    dag.notify_completion(nodeB);
    ready = dag.pop_ready();
    assert(ready == nodeC);
    
    test_passed("Incremental DAG validation");
}

// ============================================================================
// TEST 3: Phase State Transitions
// ============================================================================
void test_phase_transitions() {
    PhaseStateMachine machine;
    
    // IDLE -> PREFILL (valid)
    assert(machine.transition_to(ExecutionPhase::PREFILL));
    assert(machine.current_phase() == ExecutionPhase::PREFILL);
    
    // PREFILL -> DECODE (valid)
    assert(machine.transition_to(ExecutionPhase::DECODE));
    assert(machine.current_phase() == ExecutionPhase::DECODE);
    
    // DECODE -> IDLE (invalid)
    assert(!machine.transition_to(ExecutionPhase::IDLE));
    
    // DECODE -> TAIL (valid)
    assert(machine.transition_to(ExecutionPhase::TAIL));
    assert(machine.current_phase() == ExecutionPhase::TAIL);
    
    test_passed("Phase state transitions");
}

// ============================================================================
// TEST 4: Slab Allocator
// ============================================================================
void test_slab_allocator() {
    SlabAllocator<int, 256> allocator;
    
    std::vector<int*> ptrs;
    for (int i = 0; i < 1000; ++i) {
        int* p = allocator.allocate();
        assert(p != nullptr);
        *p = i;
        ptrs.push_back(p);
    }
    
    // Verify values
    for (int i = 0; i < 1000; ++i) {
        assert(*ptrs[i] == i);
    }
    
    test_passed("Slab allocator");
}

// ============================================================================
// TEST 5: Integer Hash Registry
// ============================================================================
void test_integer_hash() {
    // Compile-time hash
    constexpr uint64_t hash1 = fnv1a_hash("test_kernel");
    constexpr uint64_t hash2 = fnv1a_hash("test_kernel");
    constexpr uint64_t hash3 = fnv1a_hash("other_kernel");
    
    static_assert(hash1 == hash2, "Same string should hash to same value");
    static_assert(hash1 != hash3, "Different strings should hash to different values");
    
    // Runtime hash should match
    assert(hash1 == fnv1a_hash("test_kernel"));
    
    test_passed("Integer hash registry");
}

// ============================================================================
// TEST 6: Deferred Command Queue
// ============================================================================
void test_deferred_commands() {
    CommandPacketQueue queue;
    
    // Enqueue commands
    for (int i = 0; i < 100; ++i) {
        DeferredCommand cmd;
        cmd.type = CommandType::CALLBACK;
        cmd.enqueue_time = MonotonicClock::now();
        cmd.deadline = cmd.enqueue_time + 1000000; // 1ms
        cmd.priority = i;
        cmd.payload = std::function<void()>([i]() {
            g_tasks_completed.fetch_add(1);
        });
        
        queue.enqueue(cmd);
    }
    
    // Dequeue and execute
    int count = 0;
    while (auto cmd = queue.dequeue()) {
        if (std::holds_alternative<std::function<void()>>(cmd->payload)) {
            std::get<std::function<void()>>(cmd->payload)();
        }
        ++count;
    }
    
    assert(count == 100);
    assert(g_tasks_completed.load() == 100);
    
    test_passed("Deferred command queue");
}

// ============================================================================
// TEST 7: Telemetry Ring Buffer
// ============================================================================
void test_telemetry_ring() {
    LockFreeTelemetryRing<1024> ring;
    
    // Push events
    for (int i = 0; i < 500; ++i) {
        TelemetryEvent evt;
        evt.timestamp = MonotonicClock::now();
        evt.task_id = i;
        evt.event_type = 0;
        evt.worker_id = i % 8;
        evt.duration_ns = i * 100;
        
        assert(ring.push(evt));
    }
    
    // Pop events
    int count = 0;
    while (ring.pop()) {
        ++count;
    }
    
    assert(count == 500);
    
    test_passed("Telemetry ring buffer");
}

// ============================================================================
// TEST 8: Monotonic Clock
// ============================================================================
void test_monotonic_clock() {
    auto t1 = MonotonicClock::now();
    
    // Busy wait
    for (volatile int i = 0; i < 1000000; ++i);
    
    auto t2 = MonotonicClock::now();
    
    assert(t2 > t1);
    
    auto diff_ns = MonotonicClock::nanoseconds(t2 - t1);
    auto diff_us = MonotonicClock::microseconds(t2 - t1);
    auto diff_ms = MonotonicClock::milliseconds(t2 - t1);
    
    assert(diff_ns > 0);
    assert(diff_us == diff_ns / 1000);
    assert(diff_ms == diff_ns / 1000000);
    
    test_passed("Monotonic clock");
}

// ============================================================================
// TEST 9: Phase Budget Enforcement
// ============================================================================
void test_phase_budget() {
    PhaseBudget budget;
    budget.max_ns = 1000000; // 1ms
    
    budget.begin();
    
    // Busy wait
    for (volatile int i = 0; i < 500000; ++i);
    
    budget.update();
    
    // Should still have budget
    assert(budget.has_budget());
    assert(budget.remaining_ns() > 0);
    
    // Set very small budget and exceed it
    PhaseBudget small_budget;
    small_budget.max_ns = 1; // 1ns
    small_budget.begin();
    
    // Immediate update should exceed
    small_budget.update();
    assert(small_budget.exceeded.load());
    assert(!small_budget.has_budget());
    
    test_passed("Phase budget enforcement");
}

// ============================================================================
// TEST 10: Work-Stealing
// ============================================================================
void test_work_stealing() {
    ExecutionScheduler_v2 scheduler(4);
    
    std::atomic<int> counter{0};
    
    // Submit many tasks
    for (int i = 0; i < 1000; ++i) {
        scheduler.submit([&counter]() {
            counter.fetch_add(1);
        }, 5, {});
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check stats
    const auto& stats = scheduler.stats();
    std::cout << "  Tasks submitted: " << stats.tasks_submitted.load() << "\n";
    std::cout << "  Tasks completed: " << stats.tasks_completed.load() << "\n";
    std::cout << "  Tasks stolen: " << stats.tasks_stolen.load() << "\n";
    
    assert(stats.tasks_submitted.load() >= 1000);
    assert(stats.tasks_completed.load() >= 1000);
    
    scheduler.shutdown();
    
    test_passed("Work-stealing");
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "ExecutionScheduler v2 Smoke Tests\n";
    std::cout << "10 Bottleneck Fixes Validation\n";
    std::cout << "========================================\n\n";
    
    int passed = 0;
    int failed = 0;
    
    try {
        test_lockfree_queue(); ++passed;
    } catch (...) { test_failed("Lock-free queue operations"); ++failed; }
    
    try {
        test_incremental_dag(); ++passed;
    } catch (...) { test_failed("Incremental DAG validation"); ++failed; }
    
    try {
        test_phase_transitions(); ++passed;
    } catch (...) { test_failed("Phase state transitions"); ++failed; }
    
    try {
        test_slab_allocator(); ++passed;
    } catch (...) { test_failed("Slab allocator"); ++failed; }
    
    try {
        test_integer_hash(); ++passed;
    } catch (...) { test_failed("Integer hash registry"); ++failed; }
    
    try {
        test_deferred_commands(); ++passed;
    } catch (...) { test_failed("Deferred command queue"); ++failed; }
    
    try {
        test_telemetry_ring(); ++passed;
    } catch (...) { test_failed("Telemetry ring buffer"); ++failed; }
    
    try {
        test_monotonic_clock(); ++passed;
    } catch (...) { test_failed("Monotonic clock"); ++failed; }
    
    try {
        test_phase_budget(); ++passed;
    } catch (...) { test_failed("Phase budget enforcement"); ++failed; }
    
    try {
        test_work_stealing(); ++passed;
    } catch (...) { test_failed("Work-stealing"); ++failed; }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    return failed;
}
