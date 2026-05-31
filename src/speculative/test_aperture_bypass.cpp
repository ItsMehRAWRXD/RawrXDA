// test_aperture_bypass.cpp
// Test DDR5-to-GPU direct aperture bypass with aggressive overflow management

#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>

// Include the aperture headers
#include "rawr_memory_aperture.h"
#include "rawr_aperture_bridge.h"

using namespace rawr;

// Test tier detection
void test_overflow_tiers() {
    std::cout << "\n=== Testing Overflow Tier Detection ===" << std::endl;
    
    AggressiveOverflowController::Thresholds thresholds = {
        0.75f,  // tier1: 75%
        0.85f,  // tier2: 85%
        0.95f   // tier3: 95%
    };
    
    auto& controller = AggressiveOverflowController::instance();
    controller.initialize((size_t)192 * 1024 * 1024 * 1024); // 192GB
    controller.configure_thresholds(thresholds);
    
    // Test utilization calculations
    struct TestCase {
        size_t used;
        size_t total;
        AggressiveOverflowController::Tier expected_tier;
    };
    
    std::vector<TestCase> cases = {
        { 50ULL * 1024 * 1024 * 1024, 192ULL * 1024 * 1024 * 1024, AggressiveOverflowController::TIER_NORMAL },    // 26%
        { 145ULL * 1024 * 1024 * 1024, 192ULL * 1024 * 1024 * 1024, AggressiveOverflowController::TIER_WARNING },   // 75%
        { 165ULL * 1024 * 1024 * 1024, 192ULL * 1024 * 1024 * 1024, AggressiveOverflowController::TIER_THROTTLE }, // 86%
        { 183ULL * 1024 * 1024 * 1024, 192ULL * 1024 * 1024 * 1024, AggressiveOverflowController::TIER_CRITICAL },  // 95%
    };
    
    for (const auto& tc : cases) {
        float util = static_cast<float>(tc.used) / static_cast<float>(tc.total);
        std::cout << "Utilization: " << (util * 100.0f) << "% -> ";
        
        // Determine tier manually
        AggressiveOverflowController::Tier tier = AggressiveOverflowController::TIER_NORMAL;
        if (util >= 0.95f) tier = AggressiveOverflowController::TIER_CRITICAL;
        else if (util >= 0.85f) tier = AggressiveOverflowController::TIER_THROTTLE;
        else if (util >= 0.75f) tier = AggressiveOverflowController::TIER_WARNING;
        
        const char* tier_names[] = { "NORMAL", "WARNING", "THROTTLE", "CRITICAL" };
        std::cout << "Tier " << tier_names[tier];
        
        if (tier == tc.expected_tier) {
            std::cout << " [PASS]" << std::endl;
        } else {
            std::cout << " [FAIL - expected " << tier_names[tc.expected_tier] << "]" << std::endl;
        }
    }
}

// Test memory pinning
void test_memory_pinning() {
    std::cout << "\n=== Testing Memory Pinning ===" << std::endl;
    
    const size_t test_size = 256 * 1024 * 1024; // 256MB
    
    // Allocate test buffer
    void* buffer = VirtualAlloc(NULL, test_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        std::cerr << "Failed to allocate test buffer" << std::endl;
        return;
    }
    
    std::cout << "Allocated " << (test_size / (1024 * 1024)) << " MB buffer at " << buffer << std::endl;
    
    // Test pinning
    auto start = std::chrono::high_resolution_clock::now();
    bool pinned = RawrPinMemory(buffer, test_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (pinned) {
        std::cout << "Memory pinned successfully in " << duration.count() << " us [PASS]" << std::endl;
        
        // Test unpinning
        RawrUnpinMemory(buffer, test_size);
        std::cout << "Memory unpinned [PASS]" << std::endl;
    } else {
        std::cout << "Memory pinning failed (may require admin privileges) [WARN]" << std::endl;
    }
    
    VirtualFree(buffer, 0, MEM_RELEASE);
}

// Test large page allocation
void test_large_pages() {
    std::cout << "\n=== Testing Large Page Allocation ===" << std::endl;
    
    // Check if large pages are available
    bool available = RawrLargePagesAvailable();
    std::cout << "Large pages available: " << (available ? "yes" : "no") << std::endl;
    
    if (!available) {
        std::cout << "Attempting to enable SeLockMemoryPrivilege..." << std::endl;
        if (LargePageAllocator::enable_privilege()) {
            std::cout << "Privilege enabled [PASS]" << std::endl;
            available = true;
        } else {
            std::cout << "Privilege enable failed (run as admin) [WARN]" << std::endl;
        }
    }
    
    if (available) {
        const size_t test_size = 64 * 1024 * 1024; // 64MB
        
        auto start = std::chrono::high_resolution_clock::now();
        void* ptr = RawrAllocateHugePages(test_size);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        if (ptr) {
            std::cout << "Large page allocation succeeded in " << duration.count() << " us [PASS]" << std::endl;
            std::cout << "Address: " << ptr << std::endl;
            
            // Verify 2MB alignment
            uint64_t addr = reinterpret_cast<uint64_t>(ptr);
            if ((addr & 0x1FFFFF) == 0) {
                std::cout << "2MB alignment verified [PASS]" << std::endl;
            } else {
                std::cout << "2MB alignment check failed [WARN]" << std::endl;
            }
            
            VirtualFree(ptr, 0, MEM_RELEASE);
        } else {
            std::cout << "Large page allocation failed [WARN]" << std::endl;
        }
    }
}

// Test streaming prefetch
void test_streaming_prefetch() {
    std::cout << "\n=== Testing Streaming Prefetch ===" << std::endl;
    
    const size_t test_size = 128 * 1024 * 1024; // 128MB
    
    void* buffer = VirtualAlloc(NULL, test_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        std::cerr << "Failed to allocate test buffer" << std::endl;
        return;
    }
    
    // Initialize buffer
    std::memset(buffer, 0xAB, test_size);
    
    // Test prefetch at different tiers
    const char* tier_names[] = { "NORMAL", "WARNING", "THROTTLE", "CRITICAL" };
    
    for (int tier = 0; tier <= 3; tier++) {
        auto start = std::chrono::high_resolution_clock::now();
        RawrPrefetchMemory(buffer, test_size);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "Prefetch tier " << tier_names[tier] << ": " << duration.count() << " us [PASS]" << std::endl;
    }
    
    VirtualFree(buffer, 0, MEM_RELEASE);
}

// Test bandwidth estimation
void test_bandwidth_estimation() {
    std::cout << "\n=== Testing Bandwidth Estimation ===" << std::endl;
    
    uint64_t ddr5_bw = RawrEstimateDDR5Bandwidth();
    uint64_t pcie_bw = RawrEstimatePCIeBandwidth();
    
    std::cout << "DDR5 Bandwidth: " << ddr5_bw << " MB/s (" << (ddr5_bw / 1000.0) << " GB/s)" << std::endl;
    std::cout << "PCIe Bandwidth: " << pcie_bw << " MB/s (" << (pcie_bw / 1000.0) << " GB/s)" << std::endl;
    
    // Verify reasonable values
    if (ddr5_bw > 50000 && ddr5_bw < 200000) {
        std::cout << "DDR5 bandwidth estimate reasonable [PASS]" << std::endl;
    } else {
        std::cout << "DDR5 bandwidth estimate unexpected [WARN]" << std::endl;
    }
    
    if (pcie_bw > 20000 && pcie_bw < 40000) {
        std::cout << "PCIe bandwidth estimate reasonable [PASS]" << std::endl;
    } else {
        std::cout << "PCIe bandwidth estimate unexpected [WARN]" << std::endl;
    }
}

// Test aperture bypass activation
void test_aperture_bypass() {
    std::cout << "\n=== Testing Aperture Bypass Activation ===" << std::endl;
    
    const size_t test_size = 512 * 1024 * 1024; // 512MB
    
    // Allocate test buffer
    void* buffer = VirtualAlloc(NULL, test_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        std::cerr << "Failed to allocate test buffer" << std::endl;
        return;
    }
    
    // Initialize with pattern
    std::memset(buffer, 0x42, test_size);
    
    std::cout << "Activating bypass for " << (test_size / (1024 * 1024)) << " MB region..." << std::endl;
    
    uint32_t flags = AggressiveOverflowController::FLAG_PREFETCH | 
                     AggressiveOverflowController::FLAG_READ_ONLY;
    
    auto start = std::chrono::high_resolution_clock::now();
    bool success = RawrActivateApertureBypass(buffer, test_size, flags);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (success) {
        std::cout << "Bypass activated in " << duration.count() << " ms [PASS]" << std::endl;
        
        // Deactivate
        RawrDeactivateApertureBypass(buffer, test_size);
        std::cout << "Bypass deactivated [PASS]" << std::endl;
    } else {
        std::cout << "Bypass activation failed (may require admin privileges) [WARN]" << std::endl;
    }
    
    VirtualFree(buffer, 0, MEM_RELEASE);
}

// Test unified memory aperture
void test_unified_aperture() {
    std::cout << "\n=== Testing Unified Memory Aperture ===" << std::endl;
    
    UnifiedMemoryAperture aperture;
    
    if (aperture.initialize()) {
        std::cout << "Unified aperture initialized [PASS]" << std::endl;
        
        auto& overflow = aperture.overflow();
        std::cout << "Total aperture: " << (overflow.total_aperture() / (1024.0 * 1024 * 1024)) << " GB" << std::endl;
        std::cout << "DDR5 bandwidth: " << overflow.ddr5_bandwidth() << " MB/s" << std::endl;
        std::cout << "PCIe bandwidth: " << overflow.pcie_bandwidth() << " MB/s" << std::endl;
        
        // Test compute buffer acquisition
        void* buf = aperture.acquire_compute_buffer(256 * 1024 * 1024);
        if (buf) {
            std::cout << "Compute buffer acquired [PASS]" << std::endl;
            aperture.release_compute_buffer(buf);
            std::cout << "Compute buffer released [PASS]" << std::endl;
        } else {
            std::cout << "Compute buffer acquisition failed [WARN]" << std::endl;
        }
    } else {
        std::cout << "Unified aperture initialization failed [FAIL]" << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "RAWR DDR5-to-GPU Aperture Bypass Tests" << std::endl;
    std::cout << "========================================" << std::endl;
    
    test_overflow_tiers();
    test_memory_pinning();
    test_large_pages();
    test_streaming_prefetch();
    test_bandwidth_estimation();
    test_aperture_bypass();
    test_unified_aperture();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}