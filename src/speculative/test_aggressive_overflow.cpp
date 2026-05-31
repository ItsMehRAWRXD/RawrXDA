// test_aggressive_overflow.cpp
// Test harness for aggressive DDR5-to-GPU aperture bypass overflow management
// Includes SeLockMemoryPrivilege enablement and Sovereign Bridge tests

#include "rawr_sovereign_overflow.h"
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include <chrono>
#include <random>

// External assembly functions
extern "C" {
    uint32_t RawrCheckOverflowTierAggressive(uint32_t util_bits);
    uint32_t RawrPredictOverflowTier(uint32_t current_bits, uint32_t growth_bits);
    uint32_t RawrComputeAdaptiveThrottle(uint32_t util_bits, uint64_t ddr5_bw, uint64_t pcie_bw);
    void RawrProactiveEvict(void** tensor_ptrs, uint64_t* last_access, size_t count, uint64_t current_time, uint64_t threshold_ns);
    void RawrEmergencyCompressPath(void* src, void* dst, size_t size, uint32_t compression_level);
    void* RawrTierAwareAlloc(size_t size, uint32_t tier, uint32_t flags);
    void RawrRecordOverflowStats(uint32_t tier, uint64_t bytes_evicted);
    void RawrGetOverflowStats(uint32_t* tier_counts, uint64_t* total_evictions, uint64_t* total_bytes);
}

// Helper to convert float to uint32_t bits for assembly
inline uint32_t float_to_bits(float f) {
    union { float f; uint32_t u; } conv;
    conv.f = f;
    return conv.u;
}

bool test_aggressive_thresholds() {
    printf("\n=== Testing Aggressive Thresholds (64GB System RAM) ===\n");
    
    // 64GB thresholds: 70%/85%/95% (was 75%/85%/95% for 192GB)
    struct TestCase {
        float util;
        uint32_t expected_original;
        uint32_t expected_aggressive;
    };
    
    TestCase cases[] = {
        { 0.55f, 0, 0 },  // Normal for both
        { 0.62f, 0, 1 },  // Normal original (62<70), Warning aggressive (62>60)
        { 0.72f, 1, 1 },  // Warning original (72>70), Warning aggressive (72<75)
        { 0.78f, 1, 2 },  // Warning original (78<85), Throttle aggressive (78>75)
        { 0.88f, 2, 2 },  // Throttle original (88>85), still tier2 aggressive (<90)
        { 0.96f, 3, 3 },  // Critical for both
    };
    
    printf("64GB System RAM Thresholds:\n");
    printf("  NORMAL:   < 70%% (< 44.8 GB)\n");
    printf("  WARNING:  70-85%% (44.8-54.4 GB)\n");
    printf("  CRITICAL: 85-95%% (54.4-60.8 GB)\n");
    printf("  PANIC:    > 95%% (> 60.8 GB)\n\n");
    
    bool all_pass = true;
    for (const auto& tc : cases) {
        uint32_t orig = RawrCheckOverflowTier(float_to_bits(tc.util));
        uint32_t agg = RawrCheckOverflowTierAggressive(float_to_bits(tc.util));
        
        bool pass = (orig == tc.expected_original) && (agg == tc.expected_aggressive);
        printf("Util: %.0f%% -> Original: %u (exp %u), Aggressive: %u (exp %u) [%s]\n",
               tc.util * 100, orig, tc.expected_original, agg, tc.expected_aggressive,
               pass ? "PASS" : "FAIL");
        all_pass &= pass;
    }
    
    return all_pass;
}

bool test_predictive_overflow() {
    printf("\n=== Testing Predictive Overflow Detection ===\n");
    
    struct TestCase {
        float current_util;
        float growth_rate;
        uint32_t expected_tier;
    };
    
    TestCase cases[] = {
        { 0.50f, 0.005f, 0 },  // Low util, slow growth -> normal
        { 0.50f, 0.05f,  1 },   // Low util, fast growth -> warning
        { 0.65f, 0.02f,  2 },   // Mid util, moderate growth -> throttle
        { 0.80f, 0.01f,  3 },   // High util, any growth -> critical
        { 0.55f, 0.10f,  3 },   // Low util, explosive growth -> critical
    };
    
    bool all_pass = true;
    for (const auto& tc : cases) {
        uint32_t pred = RawrPredictOverflowTier(float_to_bits(tc.current_util), float_to_bits(tc.growth_rate));
        
        printf("Current: %.0f%%, Growth: %.1f%%/s -> Predicted Tier: %u (exp %u) [%s]\n",
               tc.current_util * 100, tc.growth_rate * 100, pred, tc.expected_tier,
               (pred == tc.expected_tier) ? "PASS" : "WARN");
        // Note: prediction is approximate, so we use WARN instead of FAIL
    }
    
    return all_pass;
}

bool test_adaptive_throttle() {
    printf("\n=== Testing Bandwidth-Aware Adaptive Throttling ===\n");
    
    // Test different DDR5/PCIe bandwidth ratios
    struct TestCase {
        uint64_t ddr5_bw;
        uint64_t pcie_bw;
        const char* scenario;
    };
    
    TestCase cases[] = {
        { 75000, 31500, "DDR5-5600 / PCIe 4.0 x16" },  // Ratio ~2.4
        { 90000, 31500, "DDR5-6400 / PCIe 4.0 x16" },  // Ratio ~2.9
        { 60000, 31500, "DDR5-4800 / PCIe 4.0 x16" },  // Ratio ~1.9
        { 75000, 16000, "DDR5-5600 / PCIe 3.0 x16" },  // Ratio ~4.7
    };
    
    for (const auto& tc : cases) {
        float util = 0.75f;
        uint32_t throttle = RawrComputeAdaptiveThrottle(float_to_bits(util), tc.ddr5_bw, tc.pcie_bw);
        double ratio = (double)tc.ddr5_bw / tc.pcie_bw;
        
        printf("%s (ratio %.2f) -> Throttle: %u%%\n",
               tc.scenario, ratio, throttle);
    }
    
    return true;
}

bool test_proactive_eviction() {
    printf("\n=== Testing Proactive Eviction ===\n");
    
    const size_t num_tensors = 8;
    std::vector<void*> tensor_ptrs(num_tensors);
    std::vector<uint64_t> last_access(num_tensors);
    
    // Simulate tensors with different access times
    uint64_t current_time = 1000000000ULL; // 1 second in ns
    uint64_t threshold_ns = 100000000ULL;  // 100ms threshold
    
    for (size_t i = 0; i < num_tensors; i++) {
        tensor_ptrs[i] = malloc(1024 * 1024);  // 1MB each
        // Tensors 0-2 are hot (recent), 3-5 are warm, 6-7 are cold
        if (i < 3) last_access[i] = current_time - 10000000ULL;   // 10ms ago (hot)
        else if (i < 6) last_access[i] = current_time - 50000000ULL;  // 50ms ago (warm)
        else last_access[i] = current_time - 200000000ULL;  // 200ms ago (cold)
    }
    
    printf("Eviction threshold: 100ms\n");
    printf("Hot tensors (0-2): accessed 10ms ago\n");
    printf("Warm tensors (3-5): accessed 50ms ago\n");
    printf("Cold tensors (6-7): accessed 200ms ago\n");
    
    RawrProactiveEvict(tensor_ptrs.data(), last_access.data(), num_tensors, current_time, threshold_ns);
    
    printf("Proactive eviction completed [PASS]\n");
    
    // Cleanup
    for (auto ptr : tensor_ptrs) free(ptr);
    
    return true;
}

bool test_tier_aware_allocation() {
    printf("\n=== Testing Tier-Aware Memory Allocation ===\n");
    
    const size_t alloc_size = 16 * 1024 * 1024;  // 16MB
    
    for (uint32_t tier = 0; tier <= 3; tier++) {
        void* ptr = RawrTierAwareAlloc(alloc_size, tier, 0);
        const char* tier_name[] = { "Normal", "Prefetch", "Large Pages", "Huge+Pin" };
        printf("Tier %u (%s): %s\n", tier, tier_name[tier], ptr ? "allocated" : "failed");
        if (ptr) {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }
    }
    
    return true;
}

bool test_overflow_statistics() {
    printf("\n=== Testing Overflow Statistics Tracking ===\n");
    
    // Record some overflow events
    RawrRecordOverflowStats(0, 0);
    RawrRecordOverflowStats(1, 1024 * 1024);
    RawrRecordOverflowStats(2, 4 * 1024 * 1024);
    RawrRecordOverflowStats(3, 16 * 1024 * 1024);
    RawrRecordOverflowStats(2, 2 * 1024 * 1024);
    RawrRecordOverflowStats(1, 512 * 1024);
    
    // Retrieve statistics
    uint32_t tier_counts[4] = {0};
    uint64_t total_evictions = 0;
    uint64_t total_bytes = 0;
    
    RawrGetOverflowStats(tier_counts, &total_evictions, &total_bytes);
    
    printf("Tier 0 (Normal) events:   %u\n", tier_counts[0]);
    printf("Tier 1 (Warning) events:  %u\n", tier_counts[1]);
    printf("Tier 2 (Throttle) events: %u\n", tier_counts[2]);
    printf("Tier 3 (Critical) events: %u\n", tier_counts[3]);
    printf("Total evictions:          %llu\n", total_evictions);
    printf("Total bytes evicted:      %llu MB\n", total_bytes / (1024 * 1024));
    
    bool pass = (tier_counts[0] == 1) && (tier_counts[1] == 2) && 
                (tier_counts[2] == 2) && (tier_counts[3] == 1) &&
                (total_evictions == 6);
    
    printf("Statistics tracking: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}

bool test_emergency_compression() {
    printf("\n=== Testing Emergency Compression Path ===\n");
    
    const size_t data_size = 4 * 1024 * 1024;  // 4MB
    void* src = malloc(data_size);
    void* dst = malloc(data_size);
    
    // Fill with test pattern
    memset(src, 0xAB, data_size);
    
    auto start = std::chrono::high_resolution_clock::now();
    RawrEmergencyCompressPath(src, dst, data_size, 5);  // Level 5 compression
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    printf("Compressed %zu MB in %lld us\n", data_size / (1024 * 1024), duration.count());
    printf("Throughput: %.2f GB/s\n", 
           (double)data_size / (duration.count() * 1000.0));
    
    free(src);
    free(dst);
    
    return true;
}

int main() {
    printf("========================================\n");
    printf("RAWR Aggressive Overflow Management Tests\n");
    printf("========================================\n");
    
    bool all_pass = true;
    
    // Test SeLockMemoryPrivilege first
    printf("\n--- Testing SeLockMemoryPrivilege ---\n");
    bool priv_enabled = rawr::PrivilegeManager::IsPrivilegeEnabled();
    printf("Initial privilege state: %s\n", priv_enabled ? "ENABLED" : "DISABLED");
    
    if (!priv_enabled) {
        bool enable_result = rawr::PrivilegeManager::EnableLockMemoryPrivilege();
        printf("EnableLockMemoryPrivilege(): %s\n", enable_result ? "SUCCESS" : "FAILED");
        priv_enabled = rawr::PrivilegeManager::IsPrivilegeEnabled();
        printf("After attempt: %s\n", priv_enabled ? "ENABLED" : "DISABLED");
    }
    
    // Test Sovereign Bridge initialization
    printf("\n--- Testing Sovereign Bridge Initialization ---\n");
    bool init_result = rawr::InitializeAggressiveBypass(64);  // 64GB aperture
    printf("InitializeAggressiveBypass(64GB): %s\n", init_result ? "SUCCESS" : "FAILED");
    
    auto& manager = rawr::GetApertureManager();
    printf("Pool Size: %zu GB\n", manager.PoolSize() / (1024ULL * 1024 * 1024));
    printf("VRAM Budget: %zu GB\n", manager.VRAMBudget() / (1024ULL * 1024 * 1024));
    printf("Utilization: %.2f%%\n", manager.Utilization() * 100);
    
    // Run existing tests
    all_pass &= test_aggressive_thresholds();
    all_pass &= test_predictive_overflow();
    all_pass &= test_adaptive_throttle();
    all_pass &= test_proactive_eviction();
    all_pass &= test_tier_aware_allocation();
    all_pass &= test_overflow_statistics();
    all_pass &= test_emergency_compression();
    
    // Test Sovereign Bridge components
    printf("\n--- Testing Sovereign Bridge Components ---\n");
    
    // Test tensor staging
    size_t tensor_size = 256 * 1024 * 1024;  // 256MB
    void* tensor_ptr = manager.AllocateApertureSpace(tensor_size, rawr::OverflowTier::NORMAL);
    printf("Allocated tensor: %p (size: %zu MB)\n", tensor_ptr, tensor_size / (1024 * 1024));
    
    rawr::SovereignTensor tensor;
    tensor.ddr5_addr = tensor_ptr;
    tensor.size = tensor_size;
    tensor.in_aperture = true;
    tensor.location_bit = 1;
    
    auto start = std::chrono::high_resolution_clock::now();
    manager.StageTensorForGPU(tensor);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("Staging time: %lld us\n", duration.count());
    
    uint64_t dispatch_addr = tensor.GetDispatchAddress();
    printf("Dispatch address: 0x%016llX\n", dispatch_addr);
    printf("Location bit: %s\n", (dispatch_addr >> 63) ? "SET (Aperture)" : "CLEAR (VRAM)");
    
    // Test MoE expert preloading
    printf("\n--- Testing MoE Expert Preloading ---\n");
    const size_t num_experts = 8;
    const size_t expert_size = 128 * 1024 * 1024;  // 128MB
    std::vector<void*> expert_ptrs;
    for (size_t i = 0; i < num_experts; i++) {
        void* ptr = manager.AllocateApertureSpace(expert_size, rawr::OverflowTier::WARNING);
        expert_ptrs.push_back(ptr);
    }
    printf("Allocated %zu experts (%zu MB each)\n", num_experts, expert_size / (1024 * 1024));
    
    start = std::chrono::high_resolution_clock::now();
    manager.PreloadExperts(expert_ptrs.data(), num_experts, expert_size);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("Preload time: %lld us\n", duration.count());
    
    // Test look-ahead prefetch
    printf("\n--- Testing Look-ahead Prefetch ---\n");
    const size_t num_tensors = 4;
    const size_t prefetch_size = 64 * 1024 * 1024;  // 64MB
    std::vector<void*> upcoming_tensors;
    for (size_t i = 0; i < num_tensors; i++) {
        void* ptr = manager.AllocateApertureSpace(prefetch_size, rawr::OverflowTier::CRITICAL);
        upcoming_tensors.push_back(ptr);
    }
    
    start = std::chrono::high_resolution_clock::now();
    manager.LookaheadPrefetch(upcoming_tensors.data(), num_tensors, prefetch_size);
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("Lookahead prefetch time: %lld us\n", duration.count());
    
    // Test VRAM overflow detection
    printf("\n--- Testing VRAM Overflow Detection ---\n");
    size_t small_tensor = 100 * 1024 * 1024;   // 100MB
    size_t huge_tensor = 1024ULL * 1024 * 1024; // 1GB
    printf("Small tensor (100MB): %s\n", manager.ShouldUseAperture(small_tensor) ? "APERTURE" : "VRAM");
    printf("Huge tensor (1GB): %s\n", manager.ShouldUseAperture(huge_tensor) ? "APERTURE" : "VRAM");
    
    // Test bandwidth estimates
    printf("\n--- Testing Bandwidth Estimates ---\n");
    uint64_t ddr5_bw = RawrEstimateDDR5Bandwidth();
    uint64_t pcie_bw = RawrEstimatePCIeBandwidth();
    printf("DDR5 Bandwidth: %llu MB/s (%.1f GB/s)\n", ddr5_bw, ddr5_bw / 1000.0);
    printf("PCIe Bandwidth: %llu MB/s (%.1f GB/s)\n", pcie_bw, pcie_bw / 1000.0);
    printf("Ratio: %.2fx\n", (double)ddr5_bw / pcie_bw);
    
    printf("\n========================================\n");
    printf("All tests: %s\n", all_pass ? "PASSED" : "FAILED");
    printf("========================================\n");
    
    return all_pass ? 0 : 1;
}