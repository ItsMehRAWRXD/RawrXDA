// ExtensionScheduler_Integration_Test.cpp — Systematic validation of Extension API + Scheduler v2 integration
#include "ExtensionSchedulerBridge.h"
#include "extensions/extension_api_bridge.h"
#include "ExecutionScheduler_v2.h"
#include <iostream>
#include <assert>
#include <chrono>
#include <math>

using namespace RawrXD;
using namespace RawrXD::Extensions;

static std::atomic<int> g_tasksCompleted{0};
static std::atomic<int> g_eventsReceived{0};

void test_passed(const char* name) {
    std::cout << "[PASS] " << name << "\n";
}

void test_failed(const char* name) {
    std::cout << "[FAIL] " << name << "\n";
}

// ============================================================================
// TEST 1: Bridge Initialization
// ============================================================================
void test_bridge_initialization() {
    std::cout << "\n=== TEST 1: Bridge Initialization ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    
    // Should not be initialized before init
    assert(!bridge.isInitialized());
    
    // Initialize
    auto& scheduler = ExecutionScheduler_v2::Instance();
    bridge.initialize(&scheduler);
    
    assert(bridge.isInitialized());
    
    test_passed("Bridge initialization");
}

// ============================================================================
// TEST 2: Extension Command Submission
// ============================================================================
void test_extension_command_submission() {
    std::cout << "\n=== TEST 2: Extension Command Submission ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    g_tasksCompleted.store(0);
    
    // Submit 100 commands
    for (int i = 0; i < 100; ++i) {
        bridge.submitExtensionCommand(
            "test.extension",
            "test.command." + std::to_string(i),
            []() {
                g_tasksCompleted.fetch_add(1);
            },
            5
        );
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    assert(g_tasksCompleted.load() == 100);
    
    auto stats = bridge.getStats();
    assert(stats.commandsSubmitted == 100);
    assert(stats.commandsCompleted == 100);
    
    test_passed("Extension command submission");
}

// ============================================================================
// TEST 3: Command Dependencies
// ============================================================================
void test_command_dependencies() {
    std::cout << "\n=== TEST 3: Command Dependencies ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    std::atomic<int> order{0};
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    // Submit parent task
    TaskID parent = bridge.submitExtensionCommand(
        "test.extension",
        "parent.task",
        [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(1);
        },
        5
    );
    
    // Submit child task with dependency
    bridge.submitExtensionCommandWithDeps(
        "test.extension",
        "child.task",
        [&]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(2);
        },
        {parent},
        5
    );
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Verify order
    std::lock_guard<std::mutex> lock(orderMutex);
    assert(executionOrder.size() == 2);
    assert(executionOrder[0] == 1);
    assert(executionOrder[1] == 2);
    
    test_passed("Command dependencies");
}

// ============================================================================
// TEST 4: Event Emission
// ============================================================================
void test_event_emission() {
    std::cout << "\n=== TEST 4: Event Emission ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    g_eventsReceived.store(0);
    
    // Subscribe to events
    auto subId = bridge.subscribeToSchedulerEvents(
        "test.extension",
        [](const TelemetryEvent& evt) {
            g_eventsReceived.fetch_add(1);
            (void)evt;
        }
    );
    
    // Emit events
    for (int i = 0; i < 50; ++i) {
        bridge.emitExtensionEvent(
            "test.extension",
            "test.event",
            "{\"index\":" + std::to_string(i) + "}"
        );
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    auto stats = bridge.getStats();
    assert(stats.eventsEmitted == 50);
    
    test_passed("Event emission");
}

// ============================================================================
// TEST 5: Phase Budget Enforcement
// ============================================================================
void test_phase_budget() {
    std::cout << "\n=== TEST 5: Phase Budget Enforcement ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    auto& scheduler = ExecutionScheduler_v2::Instance();
    
    // Set small budget
    auto& budget = scheduler.budget_for(ExecutionPhase::DECODE);
    budget.max_ns = 1000; // 1 microsecond
    budget.begin();
    
    // Try to execute work that exceeds budget
    bool executed = bridge.executeWithBudget(
        ExecutionPhase::DECODE,
        "test.extension",
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    );
    
    // Should fail due to budget
    assert(!executed);
    
    auto stats = bridge.getStats();
    assert(stats.budgetExceeded >= 1);
    
    test_passed("Phase budget enforcement");
}

// ============================================================================
// TEST 6: Extension API Bridge Integration
// ============================================================================
void test_extension_api_integration() {
    std::cout << "\n=== TEST 6: Extension API Bridge Integration ===\n";
    
    auto& extBridge = ExtensionAPIBridge::instance();
    
    // Register command through Extension API
    int callbackCount = 0;
    extBridge.registerCommand(
        "scheduler.test",
        "Test Scheduler Command",
        [](void* data) {
            (*(int*)data)++;
        },
        &callbackCount
    );
    
    // Execute through Extension API
    extBridge.executeCommand("scheduler.test");
    
    assert(callbackCount == 1);
    
    // Submit through scheduler bridge
    auto& schedBridge = ExtensionSchedulerBridge::instance();
    schedBridge.submitExtensionCommand(
        "test.extension",
        "api.test",
        [&extBridge]() {
            extBridge.executeCommand("scheduler.test");
        },
        5
    );
    
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(callbackCount == 2);
    
    test_passed("Extension API bridge integration");
}

// ============================================================================
// TEST 7: C API Integration
// ============================================================================
void test_c_api() {
    std::cout << "\n=== TEST 7: C API Integration ===\n";
    
    // Initialize through C API
    bool initialized = ExtensionSchedulerBridge_Init();
    assert(initialized);
    
    // Submit task through C API
    std::atomic<int> cApiCounter{0};
    
    auto taskId = ExtensionSchedulerBridge_SubmitTask(
        "c.api.extension",
        "c.api.command",
        [](void* data) {
            (*(std::atomic<int>*)data)++;
        },
        &cApiCounter,
        5
    );
    
    assert(taskId != INVALID_TASK_ID);
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    assert(cApiCounter.load() == 1);
    
    // Emit event through C API
    ExtensionSchedulerBridge_EmitEvent(
        "c.api.extension",
        "c.api.event",
        "{\"test\":true}"
    );
    
    test_passed("C API integration");
}

// ============================================================================
// TEST 8: Performance Validation
// ============================================================================
void test_performance() {
    std::cout << "\n=== TEST 8: Performance Validation ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    const int NUM_TASKS = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Submit many tasks
    for (int i = 0; i < NUM_TASKS; ++i) {
        bridge.submitExtensionCommand(
            "perf.test",
            "perf.command",
            []() {
                // Minimal work
                volatile int x = 0;
                x++;
            },
            5
        );
    }
    
    auto submitEnd = std::chrono::high_resolution_clock::now();
    auto submitTime = std::chrono::duration_cast<std::chrono::microseconds>(
        submitEnd - start).count();
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto completeEnd = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(
        completeEnd - start).count();
    
    double submitPerTask = static_cast<double>(submitTime) / NUM_TASKS;
    double totalPerTask = static_cast<double>(totalTime) / NUM_TASKS;
    
    std::cout << "  Submit throughput: " << (1000000.0 / submitPerTask) 
              << " tasks/sec\n";
    std::cout << "  Submit latency: " << submitPerTask << " μs/task\n";
    std::cout << "  Total latency: " << totalPerTask << " μs/task\n";
    
    // Validate performance targets
    assert(submitPerTask < 10.0); // Should be under 10μs per task
    
    test_passed("Performance validation");
}

// ============================================================================
// TEST 9: Thread Safety
// ============================================================================
void test_thread_safety() {
    std::cout << "\n=== TEST 9: Thread Safety ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    std::atomic<int> counter{0};
    
    const int NUM_THREADS = 16;
    const int TASKS_PER_THREAD = 100;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&bridge, &counter, t]() {
            for (int i = 0; i < TASKS_PER_THREAD; ++i) {
                bridge.submitExtensionCommand(
                    "thread.test." + std::to_string(t),
                    "thread.command." + std::to_string(i),
                    [&counter]() {
                        counter.fetch_add(1);
                    },
                    5
                );
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Wait for completion
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    assert(counter.load() == NUM_THREADS * TASKS_PER_THREAD);
    
    test_passed("Thread safety");
}

// ============================================================================
// TEST 10: Integration Summary
// ============================================================================
void test_integration_summary() {
    std::cout << "\n=== TEST 10: Integration Summary ===\n";
    
    auto& bridge = ExtensionSchedulerBridge::instance();
    auto stats = bridge.getStats();
    
    std::cout << "\nIntegration Statistics:\n";
    std::cout << "  Commands Submitted: " << stats.commandsSubmitted << "\n";
    std::cout << "  Commands Completed: " << stats.commandsCompleted << "\n";
    std::cout << "  Events Emitted: " << stats.eventsEmitted << "\n";
    std::cout << "  Budget Exceeded: " << stats.budgetExceeded << "\n";
    
    // Validate all components working together
    assert(stats.commandsSubmitted > 0);
    assert(stats.commandsCompleted > 0);
    assert(stats.eventsEmitted > 0);
    
    test_passed("Integration summary");
}

// ============================================================================
// MAIN
// ============================================================================
int main() {
    std::cout << "========================================\n";
    std::cout << "Extension + Scheduler Integration Tests\n";
    std::cout << "========================================\n";
    
    int passed = 0;
    int failed = 0;
    
    try { test_bridge_initialization(); ++passed; }
    catch (...) { test_failed("Bridge initialization"); ++failed; }
    
    try { test_extension_command_submission(); ++passed; }
    catch (...) { test_failed("Extension command submission"); ++failed; }
    
    try { test_command_dependencies(); ++passed; }
    catch (...) { test_failed("Command dependencies"); ++failed; }
    
    try { test_event_emission(); ++passed; }
    catch (...) { test_failed("Event emission"); ++failed; }
    
    try { test_phase_budget(); ++passed; }
    catch (...) { test_failed("Phase budget enforcement"); ++failed; }
    
    try { test_extension_api_integration(); ++passed; }
    catch (...) { test_failed("Extension API bridge integration"); ++failed; }
    
    try { test_c_api(); ++passed; }
    catch (...) { test_failed("C API integration"); ++failed; }
    
    try { test_performance(); ++passed; }
    catch (...) { test_failed("Performance validation"); ++failed; }
    
    try { test_thread_safety(); ++passed; }
    catch (...) { test_failed("Thread safety"); ++failed; }
    
    try { test_integration_summary(); ++passed; }
    catch (...) { test_failed("Integration summary"); ++failed; }
    
    std::cout << "\n========================================\n";
    std::cout << "Results: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    if (failed == 0) {
        std::cout << "\n✅ ALL INTEGRATION TESTS PASSED\n";
        std::cout << "Extension API Bridge + ExecutionScheduler v2\n";
        std::cout << "are fully integrated and operational.\n\n";
    }
    
    return failed;
}
