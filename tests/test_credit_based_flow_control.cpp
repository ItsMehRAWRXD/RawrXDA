// ============================================================================
// test_credit_based_flow_control.cpp - Unit tests for credit-based system
// ============================================================================
// Tests:
// 1. Credit acquisition and release
// 2. Backpressure behavior
// 3. Partial credit acquisition
// 4. Batch credit returns
// 5. Thread safety
// ============================================================================

#include "flow_control/credit_based_flow_control.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>

using namespace RawrXD::FlowControl;

// Test 1: Basic credit acquisition and release
bool TestBasicCredits() {
    printf("\n[Test] Basic credit acquisition/release...\n");
    
    CreditConfig config;
    config.initialCredits = 100;
    config.maxCredits = 100;
    config.minCredits = 10;
    
    CreditCounter counter;
    counter.Initialize(config);
    
    // Acquire 50 credits
    auto result = counter.TryAcquire(50);
    bool pass1 = (result == CreditResult::Success);
    printf("  Acquire 50: %s\n", pass1 ? "SUCCESS" : "FAILED");
    
    // Check remaining
    uint32_t remaining = counter.GetAvailableCredits();
    bool pass2 = (remaining == 50);
    printf("  Remaining: %u (expected 50)\n", remaining);
    
    // Return 30 credits
    counter.ReturnCredits(30);
    remaining = counter.GetAvailableCredits();
    bool pass3 = (remaining == 80);
    printf("  After return 30: %u (expected 80)\n", remaining);
    
    // Try to acquire more than available (should fail)
    result = counter.TryAcquire(100);
    bool pass4 = (result == CreditResult::Blocked);
    printf("  Acquire 100 (should block): %s\n", pass4 ? "BLOCKED" : "FAILED");
    
    bool pass = pass1 && pass2 && pass3 && pass4;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 2: Backpressure threshold
bool TestBackpressure() {
    printf("\n[Test] Backpressure threshold...\n");
    
    CreditConfig config;
    config.initialCredits = 100;
    config.maxCredits = 100;
    config.minCredits = 20;  // Backpressure at or below 20
    config.reserveForPartial = false;  // Disable partial reserve for this test
    
    CreditCounter counter;
    counter.Initialize(config);
    
    // Acquire down to threshold (leaves exactly 20, which is backpressured)
    auto result = counter.TryAcquire(80);  // Leaves 20 credits
    bool acquired = (result == CreditResult::Success);
    printf("  Acquire 80: %s\n", acquired ? "SUCCESS" : "FAILED");
    
    bool isBackpressured = counter.IsBackpressured();
    printf("  After acquiring 80, backpressured: %s (credits=%u)\n", 
           isBackpressured ? "YES" : "NO", counter.GetAvailableCredits());
    bool pass1 = isBackpressured;  // 20 <= 20, so should be backpressured
    
    // Try to acquire more (should fail due to minCredits)
    result = counter.TryAcquire(10);
    bool pass2 = (result == CreditResult::Blocked);
    printf("  Acquire 10 more: %s\n", pass2 ? "BLOCKED" : "FAILED");
    
    // Return credits to relieve backpressure
    counter.ReturnCredits(20);
    isBackpressured = counter.IsBackpressured();
    bool pass3 = !isBackpressured;
    printf("  After returning 20, backpressured: %s (credits=%u)\n", 
           isBackpressured ? "YES" : "NO", counter.GetAvailableCredits());
    
    bool pass = acquired && pass1 && pass2 && pass3;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 3: Partial credit acquisition
bool TestPartialAcquisition() {
    printf("\n[Test] Partial credit acquisition...\n");
    
    CreditConfig config;
    config.initialCredits = 100;
    config.maxCredits = 100;
    config.minCredits = 30;
    config.reserveForPartial = true;
    config.partialReserve = 20;
    
    CreditCounter counter;
    counter.Initialize(config);
    
    // Acquire most credits, leaving less than requested
    counter.TryAcquire(60);  // Leaves 40 credits
    
    // Try to acquire 30 - should get partial (40 - 30 reserve = 10 available)
    uint32_t partial = counter.TryAcquirePartial(30);
    printf("  Requested 30, got: %u (expected 10)\n", partial);
    bool pass1 = (partial == 10);
    
    // Try to acquire more - should fail
    partial = counter.TryAcquirePartial(10);
    printf("  Requested 10 more, got: %u (expected 0)\n", partial);
    bool pass2 = (partial == 0);
    
    bool pass = pass1 && pass2;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 4: Batch credit returns
bool TestBatchReturns() {
    printf("\n[Test] Batch credit returns...\n");
    
    CreditConfig config;
    config.initialCredits = 100;
    config.maxCredits = 100;
    config.minCredits = 10;
    config.returnBatchSize = 10;  // Batch every 10 returns
    config.reserveForPartial = false;  // Disable partial reserve for this test
    
    CreditCounter counter;
    counter.Initialize(config);
    
    // Acquire all credits
    auto result = counter.TryAcquire(90);  // Leaves 10 credits
    bool acquired = (result == CreditResult::Success);
    printf("  Acquire 90: %s\n", acquired ? "SUCCESS" : "FAILED");
    
    // Return in small increments (should batch)
    for (int i = 0; i < 5; i++) {
        counter.ReturnCreditsBatch(2);  // 5 * 2 = 10 total
    }
    
    // Check stats
    auto stats = counter.GetStats();
    printf("  Batch returns: %llu (expected 1)\n", (unsigned long long)stats.batchReturns);
    bool pass1 = (stats.batchReturns == 1);
    
    // Credits should be returned (10 + 10 = 20)
    uint32_t remaining = counter.GetAvailableCredits();
    printf("  Available credits: %u (expected 20)\n", remaining);
    bool pass2 = (remaining == 20);
    
    bool pass = acquired && pass1 && pass2;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 5: Statistics tracking
bool TestStatistics() {
    printf("\n[Test] Statistics tracking...\n");
    
    CreditConfig config;
    config.initialCredits = 100;
    config.maxCredits = 100;
    config.minCredits = 10;
    config.reserveForPartial = false;  // Disable partial reserve for predictable stats
    
    CreditCounter counter;
    counter.Initialize(config);
    
    // Perform various operations
    counter.TryAcquire(30);   // Success (100 >= 30+10=40), leaves 70
    counter.TryAcquire(70);   // Blocked (70 < 70+10=80)
    counter.TryAcquire(60);   // Success (70 >= 60+10=70), leaves 10
    counter.ReturnCredits(20);  // Returns 20, now 30
    counter.TryAcquirePartial(50);  // Partial: min(50, 30-10=20) = 20
    
    auto stats = counter.GetStats();
    printf("  Attempts: %llu (expected 4)\n", (unsigned long long)stats.acquireAttempts);
    printf("  Success: %llu (expected 2)\n", (unsigned long long)stats.acquireSuccess);
    printf("  Blocked: %llu (expected 1)\n", (unsigned long long)stats.acquireBlocked);
    printf("  Partial: %llu (expected 1)\n", (unsigned long long)stats.acquirePartial);
    
    bool pass = (stats.acquireAttempts == 4) && 
                (stats.acquireSuccess == 2) &&
                (stats.acquireBlocked == 1) &&
                (stats.acquirePartial == 1);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 6: Thread safety (basic)
bool TestThreadSafety() {
    printf("\n[Test] Thread safety (basic)...\n");
    
    CreditConfig config;
    config.initialCredits = 10000;
    config.maxCredits = 10000;
    config.minCredits = 100;
    config.reserveForPartial = false;
    
    CreditCounter counter;
    counter.Initialize(config);
    
    const int numThreads = 4;
    const int opsPerThread = 500;  // Reduced from 1000 for faster execution
    std::atomic<int> successCount{0};
    std::atomic<int> blockedCount{0};
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&, t]() {  // Capture t by value
            for (int i = 0; i < opsPerThread; i++) {
                auto result = counter.TryAcquire(10);
                if (result == CreditResult::Success) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                    // Very brief yield to allow other threads to run
                    if (i % 10 == 0) {
                        std::this_thread::yield();
                    }
                    counter.ReturnCredits(10);
                } else {
                    blockedCount.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    int successes = successCount.load();
    int blocked = blockedCount.load();
    printf("  Success: %d, Blocked: %d\n", successes, blocked);
    printf("  Total: %d (expected %d)\n", 
           successes + blocked, 
           numThreads * opsPerThread);
    
    // Most should succeed since we return credits immediately
    bool pass = (successes > blocked) && (successes + blocked == numThreads * opsPerThread);
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Test 7: Pipeline budget
bool TestPipelineBudget() {
    printf("\n[Test] Pipeline budget...\n");
    
    PipelineCreditBudget budget;
    budget.Initialize(100, 200, 100);  // ingress, decode, egress
    
    // Acquire ingress credits
    bool pass1 = budget.AcquireIngressCredits(50);
    printf("  Acquire ingress 50: %s\n", pass1 ? "SUCCESS" : "FAILED");
    
    // Transfer to decode
    budget.TransferIngressToDecode(50);
    
    // Acquire decode credits
    bool pass2 = budget.AcquireDecodeCredits(100);
    printf("  Acquire decode 100: %s\n", pass2 ? "SUCCESS" : "FAILED");
    
    // Transfer to egress
    budget.TransferDecodeToEgress(100);
    
    // Acquire egress credits
    bool pass3 = budget.AcquireEgressCredits(50);
    printf("  Acquire egress 50: %s\n", pass3 ? "SUCCESS" : "FAILED");
    
    // Release egress
    budget.ReleaseEgressCredits(50);
    
    auto stats = budget.GetStats();
    printf("  Ingress available: %u\n", stats.ingressAvailable);
    printf("  Decode available: %u\n", stats.decodeAvailable);
    printf("  Egress available: %u\n", stats.egressAvailable);
    
    bool pass = pass1 && pass2 && pass3;
    printf("  %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

// Main test runner
int main() {
    printf("========================================\n");
    printf("Credit-Based Flow Control Tests\n");
    printf("========================================\n");
    
    int passed = 0;
    int total = 7;
    
    if (TestBasicCredits()) passed++;
    if (TestBackpressure()) passed++;
    if (TestPartialAcquisition()) passed++;
    if (TestBatchReturns()) passed++;
    if (TestStatistics()) passed++;
    if (TestThreadSafety()) passed++;
    if (TestPipelineBudget()) passed++;
    
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", passed, total);
    printf("========================================\n");
    
    return (passed == total) ? 0 : 1;
}
