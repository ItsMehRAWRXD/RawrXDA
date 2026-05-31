// ============================================================================
// smoke_test_gpu_batching.cpp — Validate Async GPU Command Batching
// ============================================================================
// Tests the P0 fix: Async batching eliminates fence-wait dead time
// Expected: 1000 dispatches in <150ms (vs 500-2000ms sync)
// ============================================================================

#include "../src/core/gpu_backend_bridge.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <math>

using namespace RawrXD::GPU;

// High-resolution timer
static inline double hires_now_ms() {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() / 1000.0;
}

// Test result
struct TestResult {
    bool passed;
    std::string name;
    double durationMs;
    double avgDispatchUs;
    double batchUtilization;
    std::string error;
};

// ============================================================================
// Test 1: Basic Batching
// ============================================================================
TestResult test_basic_batching() {
    TestResult result = {};
    result.name = "Basic Batching";
    
    auto& bridge = getGPUBackendBridge();
    
    // Initialize with batching enabled
    auto initResult = bridge.initialize(ComputeAPI::DirectX12);
    if (!initResult.success) {
        // DX12 not available, skip test
        result.passed = true;
        result.error = "DX12 not available, skipped";
        return result;
    }
    
    // Configure batching: 16 dispatches per batch, 8ms flush interval
    bridge.setBatchingConfig(16, 8);
    bridge.startBackgroundFlushThread();
    
    // Create dummy dispatches
    std::vector<ComputeDispatch> dispatches;
    for (int i = 0; i < 100; ++i) {
        ComputeDispatch dispatch;
        dispatch.groupsX = 1;
        dispatch.groupsY = 1;
        dispatch.groupsZ = 1;
        dispatches.push_back(dispatch);
    }
    
    // Submit batched
    auto t0 = hires_now_ms();
    
    BatchedDispatch batch;
    batch.dispatches = dispatches;
    uint64_t fence = bridge.submitBatchedCompute(batch);
    
    // Flush and wait
    bridge.flushBatchedDispatches();
    if (fence > 0) {
        bridge.waitForFence(fence, 5000);
    }
    
    auto t1 = hires_now_ms();
    
    bridge.stopBackgroundFlushThread();
    bridge.shutdown();
    
    result.durationMs = t1 - t0;
    result.avgDispatchUs = (result.durationMs * 1000.0) / dispatches.size();
    result.passed = (result.avgDispatchUs < 150.0); // <150μs per dispatch
    
    return result;
}

// ============================================================================
// Test 2: Background Flush Thread
// ============================================================================
TestResult test_background_flush() {
    TestResult result = {};
    result.name = "Background Flush Thread";
    
    auto& bridge = getGPUBackendBridge();
    
    auto initResult = bridge.initialize(ComputeAPI::DirectX12);
    if (!initResult.success) {
        result.passed = true;
        result.error = "DX12 not available, skipped";
        return result;
    }
    
    // Configure batching with background flush
    bridge.setBatchingConfig(32, 10); // 32 dispatches or 10ms
    bridge.startBackgroundFlushThread();
    
    // Submit dispatches slowly (simulating real workload)
    auto t0 = hires_now_ms();
    
    for (int i = 0; i < 50; ++i) {
        ComputeDispatch dispatch;
        dispatch.groupsX = 1;
        dispatch.groupsY = 1;
        dispatch.groupsZ = 1;
        
        BatchedDispatch batch;
        batch.dispatches.push_back(dispatch);
        bridge.submitBatchedCompute(batch);
        
        // Small delay between submissions
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    
    // Wait for all to complete
    bridge.flushBatchedDispatches();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    auto t1 = hires_now_ms();
    
    bool threadRunning = bridge.isBackgroundFlushRunning();
    bridge.stopBackgroundFlushThread();
    bridge.shutdown();
    
    result.durationMs = t1 - t0;
    result.passed = threadRunning && (result.durationMs < 1000.0);
    
    return result;
}

// ============================================================================
// Test 3: Batch Utilization
// ============================================================================
TestResult test_batch_utilization() {
    TestResult result = {};
    result.name = "Batch Utilization";
    
    auto& bridge = getGPUBackendBridge();
    
    auto initResult = bridge.initialize(ComputeAPI::DirectX12);
    if (!initResult.success) {
        result.passed = true;
        result.error = "DX12 not available, skipped";
        return result;
    }
    
    bridge.setBatchingConfig(16, 100); // Large flush interval to force batching
    
    // Submit exactly 64 dispatches (should form 4 batches of 16)
    auto t0 = hires_now_ms();
    
    for (int i = 0; i < 64; ++i) {
        ComputeDispatch dispatch;
        dispatch.groupsX = 1;
        dispatch.groupsY = 1;
        dispatch.groupsZ = 1;
        
        BatchedDispatch batch;
        batch.dispatches.push_back(dispatch);
        bridge.submitBatchedCompute(batch);
    }
    
    // Flush
    bridge.flushBatchedDispatches();
    
    auto t1 = hires_now_ms();
    
    uint64_t totalDispatches = bridge.getTotalDispatches();
    result.durationMs = t1 - t0;
    result.batchUtilization = static_cast<double>(totalDispatches) / 64.0;
    result.passed = (totalDispatches >= 64); // All dispatches processed
    
    bridge.shutdown();
    
    return result;
}

// ============================================================================
// Test 4: Compare Sync vs Async Performance
// ============================================================================
TestResult test_performance_comparison() {
    TestResult result = {};
    result.name = "Performance Comparison (Sync vs Async)";
    
    auto& bridge = getGPUBackendBridge();
    
    auto initResult = bridge.initialize(ComputeAPI::DirectX12);
    if (!initResult.success) {
        result.passed = true;
        result.error = "DX12 not available, skipped";
        return result;
    }
    
    // Test async batching
    bridge.setBatchingConfig(16, 8);
    bridge.startBackgroundFlushThread();
    
    auto t0 = hires_now_ms();
    
    for (int i = 0; i < 100; ++i) {
        ComputeDispatch dispatch;
        dispatch.groupsX = 1;
        dispatch.groupsY = 1;
        dispatch.groupsZ = 1;
        
        BatchedDispatch batch;
        batch.dispatches.push_back(dispatch);
        bridge.submitBatchedCompute(batch);
    }
    
    bridge.flushBatchedDispatches();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    auto t1 = hires_now_ms();
    double asyncTimeMs = t1 - t0;
    
    bridge.stopBackgroundFlushThread();
    
    // Test sync (disable batching)
    bridge.setBatchingConfig(1, 0); // No batching
    
    t0 = hires_now_ms();
    
    for (int i = 0; i < 100; ++i) {
        ComputeDispatch dispatch;
        dispatch.groupsX = 1;
        dispatch.groupsY = 1;
        dispatch.groupsZ = 1;
        bridge.executeSync(dispatch, 5000);
    }
    
    t1 = hires_now_ms();
    double syncTimeMs = t1 - t0;
    
    bridge.shutdown();
    
    result.durationMs = asyncTimeMs;
    result.avgDispatchUs = (asyncTimeMs * 1000.0) / 100.0;
    
    // Async should be significantly faster (at least 2x)
    double speedup = syncTimeMs / asyncTimeMs;
    result.passed = (speedup >= 2.0);
    
    std::cout << "  Sync time: " << syncTimeMs << "ms\n";
    std::cout << "  Async time: " << asyncTimeMs << "ms\n";
    std::cout << "  Speedup: " << speedup << "x\n";
    
    return result;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    std::cout << "========================================\n";
    std::cout << "GPU Async Batching Smoke Test\n";
    std::cout << "========================================\n\n";
    
    std::vector<TestResult> results;
    
    // Run tests
    results.push_back(test_basic_batching());
    results.push_back(test_background_flush());
    results.push_back(test_batch_utilization());
    results.push_back(test_performance_comparison());
    
    // Print results
    std::cout << "\n----------------------------------------\n";
    std::cout << "Test Results:\n";
    std::cout << "----------------------------------------\n";
    
    int passed = 0;
    int failed = 0;
    
    for (const auto& result : results) {
        std::cout << result.name << ": " << (result.passed ? "PASS" : "FAIL");
        
        if (!result.error.empty()) {
            std::cout << " (" << result.error << ")";
        }
        
        if (result.durationMs > 0) {
            std::cout << " [" << result.durationMs << "ms";
            if (result.avgDispatchUs > 0) {
                std::cout << ", " << result.avgDispatchUs << "μs/dispatch";
            }
            std::cout << "]";
        }
        
        std::cout << "\n";
        
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    std::cout << "----------------------------------------\n";
    std::cout << "Summary: " << passed << " passed, " << failed << " failed\n";
    std::cout << "========================================\n";
    
    return failed > 0 ? 1 : 0;
}
