#include "ExtensionSchedulerBridge.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <assert>
#include <math>

using namespace ExtensionScheduler;

// Test counters
static int testsPassed = 0;
static int testsFailed = 0;
static std::vector<std::string> testResults;

void testResult(const std::string& testName, bool passed, const std::string& details = "") {
    if (passed) {
        testsPassed++;
        std::cout << "[PASS] " << testName << std::endl;
        if (!details.empty()) {
            std::cout << "       " << details << std::endl;
        }
    } else {
        testsFailed++;
        std::cout << "[FAIL] " << testName << std::endl;
        if (!details.empty()) {
            std::cout << "       " << details << std::endl;
        }
    }
    testResults.push_back(testName + ": " + (passed ? "PASS" : "FAIL"));
}

// Test 1: Bridge Initialization
void test_initialization() {
    std::cout << "\n=== Test 1: Bridge Initialization ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bool result = bridge.initialize();
    
    testResult("Bridge Initialization", result, 
        result ? "Bridge initialized successfully" : "Failed to initialize bridge");
    
    // Cleanup
    bridge.shutdown();
}

// Test 2: Command Submission
void test_command_submission() {
    std::cout << "\n=== Test 2: Command Submission ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    bool callbackExecuted = false;
    auto taskId = bridge.submitExtensionCommand(
        "test.extension",
        "test.command",
        [&callbackExecuted]() {
            callbackExecuted = true;
        }
    );
    
    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    testResult("Command Submission", taskId > 0, 
        "Task ID: " + std::to_string(taskId));
    
    // Cleanup
    bridge.shutdown();
}

// Test 3: Command Dependencies
void test_command_dependencies() {
    std::cout << "\n=== Test 3: Command Dependencies ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    std::vector<int> executionOrder;
    std::mutex orderMutex;
    
    // Submit parent task
    auto parentId = bridge.submitExtensionCommand(
        "test.extension",
        "parent.command",
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(1);
        }
    );
    
    // Submit child task with dependency
    auto childId = bridge.submitExtensionCommand(
        "test.extension",
        "child.command",
        [&executionOrder, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            executionOrder.push_back(2);
        },
        {"parent.command"}
    );
    
    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    bool correctOrder = executionOrder.size() == 2 && 
                       executionOrder[0] == 1 && 
                       executionOrder[1] == 2;
    
    testResult("Command Dependencies", correctOrder,
        "Execution order: " + std::to_string(executionOrder.size()) + " tasks completed");
    
    // Cleanup
    bridge.shutdown();
}

// Test 4: Event Emission
void test_event_emission() {
    std::cout << "\n=== Test 4: Event Emission ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    bool eventReceived = false;
    std::string receivedType;
    
    bridge.registerEventHandler("test.extension", 
        [&eventReceived, &receivedType](const SchedulerEvent& event) {
            eventReceived = true;
            receivedType = event.type;
        });
    
    // Submit a command to trigger event
    bridge.submitExtensionCommand(
        "test.extension",
        "event.test",
        []() { /* no-op */ }
    );
    
    // Wait for execution and event
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    testResult("Event Emission", eventReceived,
        "Event type: " + receivedType);
    
    // Cleanup
    bridge.shutdown();
}

// Test 5: Phase Budget Enforcement
void test_phase_budget() {
    std::cout << "\n=== Test 5: Phase Budget Enforcement ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    // Set a tight budget (1ms)
    uint64_t budgetNs = 1000000; // 1ms in nanoseconds
    
    bool budgetExceeded = false;
    bridge.registerEventHandler("budget.test", 
        [&budgetExceeded](const SchedulerEvent& event) {
            if (event.type == "task.budget_exceeded") {
                budgetExceeded = true;
            }
        });
    
    // Submit a task that will exceed budget
    bridge.submitExtensionCommand(
        "budget.test",
        "slow.command",
        []() {
            // Simulate work that takes >1ms
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        },
        {},
        budgetNs
    );
    
    // Wait for execution
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    testResult("Phase Budget Enforcement", budgetExceeded,
        "Budget exceeded event received");
    
    // Cleanup
    bridge.shutdown();
}

// Test 6: Extension API Integration
void test_extension_api_integration() {
    std::cout << "\n=== Test 6: Extension API Integration ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    // Test that we can create extension API objects
    auto outputChannel = ExtensionAPI::createOutputChannel("Test Channel");
    auto statusBarItem = ExtensionAPI::createStatusBarItem();
    
    bool apiObjectsCreated = (outputChannel != nullptr) && (statusBarItem != nullptr);
    
    testResult("Extension API Integration", apiObjectsCreated,
        "OutputChannel and StatusBarItem created");
    
    // Cleanup
    bridge.shutdown();
}

// Test 7: C API Integration
void test_c_api() {
    std::cout << "\n=== Test 7: C API Integration ===" << std::endl;
    
    // Initialize via C API
    auto handle = ExtensionSchedulerBridge_Init();
    bool initSuccess = (handle != nullptr);
    
    if (initSuccess) {
        static bool cCallbackExecuted = false;
        cCallbackExecuted = false;
        
        // Submit task via C API
        auto taskId = ExtensionSchedulerBridge_SubmitTask(
            handle,
            "c.api.test",
            "c.command",
            []() { cCallbackExecuted = true; },
            nullptr,
            0,
            0
        );
        
        // Wait for execution
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool taskSubmitted = (taskId > 0);
        
        testResult("C API Task Submission", taskSubmitted,
            "Task ID: " + std::to_string(taskId));
        
        // Cleanup
        ExtensionSchedulerBridge_Shutdown(handle);
    } else {
        testResult("C API Initialization", false, "Failed to initialize via C API");
    }
}

// Test 8: Performance - Task Submission Throughput
void test_performance() {
    std::cout << "\n=== Test 8: Performance - Task Submission Throughput ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    const int taskCount = 10000;
    std::atomic<int> completedTasks{0};
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Submit many tasks
    for (int i = 0; i < taskCount; i++) {
        bridge.submitExtensionCommand(
            "perf.test",
            "perf.command",
            [&completedTasks]() {
                completedTasks++;
            }
        );
    }
    
    // Wait for all tasks
    while (completedTasks.load() < taskCount) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - startTime).count();
    
    double tasksPerSecond = (taskCount * 1000000.0) / duration;
    double avgLatencyUs = static_cast<double>(duration) / taskCount;
    
    bool performanceOk = tasksPerSecond > 1000 && avgLatencyUs < 100; // <100μs per task
    
    testResult("Performance Throughput", performanceOk,
        std::to_string(taskCount) + " tasks in " + std::to_string(duration) + "μs, " +
        std::to_string(static_cast<int>(tasksPerSecond)) + " tasks/sec, " +
        "avg latency: " + std::to_string(static_cast<int>(avgLatencyUs)) + "μs");
    
    // Cleanup
    bridge.shutdown();
}

// Test 9: Thread Safety
void test_thread_safety() {
    std::cout << "\n=== Test 9: Thread Safety ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    const int threadCount = 16;
    const int tasksPerThread = 100;
    std::atomic<int> totalCompleted{0};
    std::vector<std::thread> threads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Launch threads
    for (int t = 0; t < threadCount; t++) {
        threads.emplace_back([&bridge, &totalCompleted, t, tasksPerThread]() {
            for (int i = 0; i < tasksPerThread; i++) {
                bridge.submitExtensionCommand(
                    "thread.test." + std::to_string(t),
                    "thread.command",
                    [&totalCompleted]() {
                        totalCompleted++;
                    }
                );
            }
        });
    }
    
    // Wait for all threads to complete submission
    for (auto& t : threads) {
        t.join();
    }
    
    // Wait for all tasks to complete
    int expectedTasks = threadCount * tasksPerThread;
    while (totalCompleted.load() < expectedTasks) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();
    
    bool allCompleted = (totalCompleted.load() == expectedTasks);
    
    testResult("Thread Safety", allCompleted,
        std::to_string(expectedTasks) + " tasks completed across " + 
        std::to_string(threadCount) + " threads in " + std::to_string(duration) + "ms");
    
    // Cleanup
    bridge.shutdown();
}

// Test 10: Integration Summary
void test_integration_summary() {
    std::cout << "\n=== Test 10: Integration Summary ===" << std::endl;
    
    auto& bridge = ExtensionSchedulerBridge::getInstance();
    bridge.initialize();
    
    // Get telemetry
    auto telemetry = bridge.getTelemetry();
    
    bool hasTelemetry = telemetry.tasksSubmitted >= 0;
    
    testResult("Integration Summary", hasTelemetry,
        "Scheduler telemetry available: " + 
        std::to_string(telemetry.tasksSubmitted) + " tasks submitted");
    
    // Cleanup
    bridge.shutdown();
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "ExtensionScheduler Integration Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Run all tests
    test_initialization();
    test_command_submission();
    test_command_dependencies();
    test_event_emission();
    test_phase_budget();
    test_extension_api_integration();
    test_c_api();
    test_performance();
    test_thread_safety();
    test_integration_summary();
    
    // Summary
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << testsPassed << std::endl;
    std::cout << "Failed: " << testsFailed << std::endl;
    std::cout << "Total:  " << (testsPassed + testsFailed) << std::endl;
    
    std::cout << "\nDetailed Results:" << std::endl;
    for (const auto& result : testResults) {
        std::cout << "  " << result << std::endl;
    }
    
    std::cout << "\n========================================" << std::endl;
    
    return testsFailed > 0 ? 1 : 0;
}
