// benchmark_aperture_real.cpp
// Real PCIe bandwidth benchmark with GPU sync and NVMe fallback

#include "rawr_sovereign_bridge.h"
#include "rawr_gpu_sync.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <math>

using namespace rawr;
using namespace std::chrono;

// Test configuration for 64GB system
struct TestConfig {
    static constexpr size_t SYSTEM_RAM_GB = 64;
    static constexpr size_t VRAM_GB = 16;
    static constexpr size_t TOTAL_UNIFIED_GB = 80;
    
    // Tier thresholds (adjusted for 64GB)
    static constexpr float TIER_WARNING = 0.70f;   // 44.8GB
    static constexpr float TIER_CRITICAL = 0.85f; // 54.4GB
    static constexpr float TIER_PANIC = 0.95f;    // 60.8GB
    
    // Test sizes
    static constexpr size_t SMALL_TENSOR = 100 * 1024 * 1024;      // 100MB
    static constexpr size_t MEDIUM_TENSOR = 1024 * 1024 * 1024;    // 1GB
    static constexpr size_t LARGE_TENSOR = 8ULL * 1024 * 1024 * 1024; // 8GB
    static constexpr size_t EXPERT_SIZE = 2ULL * 1024 * 1024 * 1024;  // 2GB (typical MoE expert)
};

// GPU-synchronized bandwidth test
double MeasureRealPCIeBandwidth(void* aperture_addr, size_t size, GPUSync& gpu) {
    if (!gpu.IsInitialized()) {
        // Fallback: measure CPU→DDR5 time (what we had before)
        auto start = high_resolution_clock::now();
        
        // Touch all pages to force actual memory access
        volatile char* ptr = (volatile char*)aperture_addr;
        for (size_t i = 0; i < size; i += 4096) {
            ptr[i] = 0;
        }
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(end - start);
        
        // This is CPU-side bandwidth, not real PCIe
        double seconds = duration.count() / 1e6;
        return (size / (1024.0 * 1024 * 1024)) / seconds; // GB/s
    }
    
    // Real GPU sync test
    gpu.RecordReadCommand(aperture_addr, size);
    double ms = gpu.ExecuteAndWait();
    
    return (size / (1024.0 * 1024 * 1024)) / (ms / 1000.0); // GB/s
}

// NVMe fallback test
double TestNVMeFallback(NVMeFallback& nvme, void* buffer, size_t size) {
    if (!nvme.IsInitialized()) {
        return 0.0;
    }
    
    // Write to NVMe
    auto start = high_resolution_clock::now();
    if (!nvme.WriteToSwap(buffer, 0, size)) {
        return 0.0;
    }
    auto write_end = high_resolution_clock::now();
    
    // Read back
    if (!nvme.ReadFromSwap(buffer, 0, size)) {
        return 0.0;
    }
    auto read_end = high_resolution_clock::now();
    
    auto write_time = duration_cast<microseconds>(write_end - start);
    auto read_time = duration_cast<microseconds>(read_end - write_end);
    
    double write_bw = (size / (1024.0 * 1024 * 1024)) / (write_time.count() / 1e6);
    double read_bw = (size / (1024.0 * 1024 * 1024)) / (read_time.count() / 1e6);
    
    std::cout << "    NVMe Write: " << std::fixed << std::setprecision(2) << write_bw << " GB/s" << std::endl;
    std::cout << "    NVMe Read:  " << read_bw << " GB/s" << std::endl;
    
    return read_bw; // Return read bandwidth as that's the critical path
}

int main() {
    std::cout << "=== RawrXD Aperture Bypass - Real PCIe Benchmark ===" << std::endl;
    std::cout << "System: 64GB DDR5 + 16GB VRAM = 80GB Unified" << std::endl;
    std::cout << "Target: PCIe 4.0 x16 (31.5 GB/s theoretical)" << std::endl;
    std::cout << std::endl;
    
    // Initialize components
    std::cout << "[Init] Initializing Sovereign Bridge..." << std::endl;
    if (!InitializeSovereignBridge(48)) { // 48GB aperture for 64GB system
        std::cerr << "[ERROR] Failed to initialize bridge" << std::endl;
        return 1;
    }
    
    auto& bridge = GetSovereignBridge();
    std::cout << "  Pool: " << (bridge.PoolSize() / (1024*1024*1024)) << " GB" << std::endl;
    std::cout << "  Large pages: " << (bridge.LargePagesActive() ? "YES" : "NO (fallback)") << std::endl;
    
    // Initialize GPU sync
    std::cout << "[Init] Initializing GPU sync..." << std::endl;
    GPUSync gpu;
    bool gpu_available = gpu.Initialize();
    std::cout << "  GPU sync: " << (gpu_available ? "AVAILABLE" : "SOFTWARE FALLBACK") << std::endl;
    
    // Initialize NVMe fallback
    std::cout << "[Init] Initializing NVMe fallback..." << std::endl;
    NVMeFallback nvme;
    bool nvme_available = nvme.Initialize(L"rawr_swap.bin", 32); // 32GB swap
    std::cout << "  NVMe swap: " << (nvme_available ? "AVAILABLE" : "UNAVAILABLE") << std::endl;
    
    std::cout << std::endl;
    
    // Allocate test tensors
    std::cout << "[Setup] Allocating test tensors..." << std::endl;
    void* small_tensor = bridge.AllocateApertureSpace(TestConfig::SMALL_TENSOR);
    void* medium_tensor = bridge.AllocateApertureSpace(TestConfig::MEDIUM_TENSOR);
    void* large_tensor = bridge.AllocateApertureSpace(TestConfig::LARGE_TENSOR);
    
    // Initialize with pattern
    memset(small_tensor, 0xAA, TestConfig::SMALL_TENSOR);
    memset(medium_tensor, 0xBB, TestConfig::MEDIUM_TENSOR);
    memset(large_tensor, 0xCC, TestConfig::LARGE_TENSOR);
    
    // ============================================================================
    // TEST 1: CPU Prefetch Bandwidth (what we measured before)
    // ============================================================================
    std::cout << "=== TEST 1: CPU Prefetch Bandwidth (DDR5→CPU) ===" << std::endl;
    
    for (int depth = 1; depth <= 4; depth *= 2) {
        auto start = high_resolution_clock::now();
        
        for (int i = 0; i < depth; i++) {
            RawrPrefetchMemory(medium_tensor, TestConfig::MEDIUM_TENSOR);
        }
        RawrMemoryBarrier();
        
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start).count();
        
        double bw = (TestConfig::MEDIUM_TENSOR * depth / (1024.0*1024*1024)) / (us / 1e6);
        std::cout << "  Depth " << depth << ": " << std::fixed << std::setprecision(2) << bw << " GB/s" << std::endl;
    }
    
    // ============================================================================
    // TEST 2: Real PCIe Bandwidth (if GPU sync available)
    // ============================================================================
    std::cout << std::endl << "=== TEST 2: Real PCIe Bandwidth (DDR5→GPU) ===" << std::endl;
    
    if (gpu_available) {
        double pcie_bw = MeasureRealPCIeBandwidth(medium_tensor, TestConfig::MEDIUM_TENSOR, gpu);
        std::cout << "  Measured PCIe: " << std::fixed << std::setprecision(2) << pcie_bw << " GB/s" << std::endl;
        std::cout << "  Utilization: " << (pcie_bw / 31.5 * 100) << "%" << std::endl;
    } else {
        std::cout << "  [SKIPPED] GPU sync not available - install GPU drivers for real PCIe test" << std::endl;
        std::cout << "  Note: Previous 300+ GB/s numbers were CPU→DDR5, not real PCIe" << std::endl;
    }
    
    // ============================================================================
    // TEST 3: Tier Threshold Simulation
    // ============================================================================
    std::cout << std::endl << "=== TEST 3: Memory Pressure Tier Simulation ===" << std::endl;
    
    size_t used_ram = 0;
    const size_t ram_limit = TestConfig::SYSTEM_RAM_GB * 1024ULL * 1024 * 1024;
    
    auto get_tier = [](float usage) -> const char* {
        if (usage > TestConfig::TIER_PANIC) return "PANIC";
        if (usage > TestConfig::TIER_CRITICAL) return "CRITICAL";
        if (usage > TestConfig::TIER_WARNING) return "WARNING";
        return "NORMAL";
    };
    
    auto get_prefetch_depth = [](float usage) -> int {
        if (usage > TestConfig::TIER_PANIC) return 4;
        if (usage > TestConfig::TIER_CRITICAL) return 2;
        return 1;
    };
    
    // Simulate filling RAM
    std::vector<void*> allocations;
    size_t alloc_size = 4ULL * 1024 * 1024 * 1024; // 4GB chunks
    
    while (used_ram < ram_limit * 0.98) {
        void* ptr = bridge.AllocateApertureSpace(alloc_size);
        if (!ptr) break;
        
        allocations.push_back(ptr);
        used_ram += alloc_size;
        
        float usage = (float)used_ram / ram_limit;
        const char* tier = get_tier(usage);
        int depth = get_prefetch_depth(usage);
        
        // Only print at tier boundaries
        if (allocations.size() % 3 == 0 || usage > TestConfig::TIER_PANIC) {
            std::cout << "  Usage: " << std::fixed << std::setprecision(1) << (usage * 100) << "% (" << tier << ")";
            std::cout << " - Prefetch depth: " << depth << std::endl;
        }
        
        // Test NVMe fallback at PANIC tier
        if (usage > TestConfig::TIER_PANIC && nvme_available) {
            std::cout << "    [PANIC] Testing NVMe fallback..." << std::endl;
            double nvme_bw = TestNVMeFallback(nvme, ptr, 64 * 1024 * 1024); // Test 64MB
            if (nvme_bw > 0) {
                std::cout << "    NVMe fallback operational: " << nvme_bw << " GB/s" << std::endl;
            }
            break; // Stop after PANIC test
        }
    }
    
    // ============================================================================
    // TEST 4: MoE Expert Streaming Simulation
    // ============================================================================
    std::cout << std::endl << "=== TEST 4: MoE Expert Streaming Simulation ===" << std::endl;
    std::cout << "Simulating 200B MoE model with 2GB experts..." << std::endl;
    
    const int NUM_EXPERTS = 8;
    const int ACTIVE_EXPERTS = 2; // Top-2 routing
    
    void* experts[NUM_EXPERTS];
    for (int i = 0; i < NUM_EXPERTS; i++) {
        experts[i] = bridge.AllocateApertureSpace(TestConfig::EXPERT_SIZE);
        memset(experts[i], i, TestConfig::EXPERT_SIZE);
    }
    
    // Simulate token generation with expert switching
    std::cout << "  Generating 10 tokens with expert switching..." << std::endl;
    
    auto start = high_resolution_clock::now();
    
    for (int token = 0; token < 10; token++) {
        // Simulate router selecting experts 0,1 for first 5 tokens, then 2,3
        int expert_a = (token < 5) ? 0 : 2;
        int expert_b = (token < 5) ? 1 : 3;
        
        // Stage experts (prefetch + activate)
        bridge.ActivateAperture(experts[expert_a], TestConfig::EXPERT_SIZE);
        bridge.ActivateAperture(experts[expert_b], TestConfig::EXPERT_SIZE);
        
        // Simulate compute time (would be GPU work)
        // RawrMemoryBarrier(); // Ensure GPU sees data
    }
    
    auto end = high_resolution_clock::now();
    auto ms = duration_cast<milliseconds>(end - start).count();
    
    double tps = 1000.0 / (ms / 10.0);
    std::cout << "  Total time: " << ms << " ms" << std::endl;
    std::cout << "  Tokens/sec: " << std::fixed << std::setprecision(1) << tps << std::endl;
    std::cout << "  Note: This is CPU-side staging time, not real GPU inference" << std::endl;
    
    // ============================================================================
    // Summary
    // ============================================================================
    std::cout << std::endl << "=== BENCHMARK SUMMARY ===" << std::endl;
    std::cout << "System Configuration:" << std::endl;
    std::cout << "  System RAM: " << TestConfig::SYSTEM_RAM_GB << " GB" << std::endl;
    std::cout << "  VRAM: " << TestConfig::VRAM_GB << " GB" <> std::endl;
    std::cout << "  Unified Pool: " << TestConfig::TOTAL_UNIFIED_GB << " GB" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Tier Thresholds:" << std::endl;
    std::cout << "  WARNING:   " << (TestConfig::TIER_WARNING * 100) << "% (" << (TestConfig::SYSTEM_RAM_GB * TestConfig::TIER_WARNING) << " GB)" << std::endl;
    std::cout << "  CRITICAL:  " << (TestConfig::TIER_CRITICAL * 100) << "% (" << (TestConfig::SYSTEM_RAM_GB * TestConfig::TIER_CRITICAL) << " GB)" << std::endl;
    std::cout << "  PANIC:     " << (TestConfig::TIER_PANIC * 100) << "% (" << (TestConfig::SYSTEM_RAM_GB * TestConfig::TIER_PANIC) << " GB)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Expected Performance (64GB system):" << std::endl;
    std::cout << "  Small models (<16GB): 100% VRAM speed (624 GB/s)" << std::endl;
    std::cout << "  Medium models (16-44GB): Mixed VRAM + aperture" << std::endl;
    std::cout << "  Large models (44-60GB): Aperture streaming (~20-30 GB/s)" << std::endl;
    std::cout << "  Extreme models (>60GB): NVMe fallback (~3-7 GB/s)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "Ready for 192GB upgrade:" << std::endl;
    std::cout << "  Current: 80GB unified (64+16)" << std::endl;
    std::cout << "  Future:  208GB unified (192+16)" << std::endl;
    std::cout << "  Gain:    2.6x more model capacity" << std::endl;
    
    // Cleanup
    ShutdownSovereignBridge();
    nvme.Shutdown();
    
    return 0;
}
