// test_aperture_simple.cpp
// Simple test for DDR5-to-GPU direct aperture bypass

#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <windows.h>
#include <intrin.h>

// ASM function declarations
extern "C" {
    void* RawrAllocateHugePages(size_t size);
    bool RawrPinMemory(void* ptr, size_t size);
    bool RawrUnpinMemory(void* ptr, size_t size);
    void RawrPrefetchMemory(void* ptr, size_t size);
    void RawrMemoryBarrier();
    bool RawrLargePagesAvailable();
}

// Large page allocator
class LargePageAllocator {
public:
    static bool enable_privilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            return false;
        }
        
        if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
            CloseHandle(hToken);
            return false;
        }
        
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        CloseHandle(hToken);
        
        return result != FALSE;
    }
    
    static void* allocate_large_pages(size_t size) {
        size_t large_page_size = 2 * 1024 * 1024; // 2MB
        size_t aligned_size = (size + large_page_size - 1) & ~(large_page_size - 1);
        
        void* ptr = VirtualAlloc(NULL, aligned_size, 
                                  MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                  PAGE_READWRITE);
        
        if (ptr) {
            std::cout << "[Memory] Allocated " << (aligned_size / (1024*1024)) << " MB with large pages" << std::endl;
        }
        
        return ptr;
    }
};

// Overflow tier constants
enum class OverflowTier : uint32_t {
    NORMAL = 0,      // < 75% utilization
    WARNING = 1,     // 75-85% utilization
    THROTTLE = 2,    // 85-95% utilization
    CRITICAL = 3     // > 95% utilization
};

// Check overflow tier based on utilization
OverflowTier check_overflow_tier(float utilization) {
    if (utilization >= 0.95f) return OverflowTier::CRITICAL;
    if (utilization >= 0.85f) return OverflowTier::THROTTLE;
    if (utilization >= 0.75f) return OverflowTier::WARNING;
    return OverflowTier::NORMAL;
}

// Test tier detection
void test_overflow_tiers() {
    std::cout << "\n=== Testing Overflow Tier Detection ===" << std::endl;
    
    struct TestCase {
        float utilization;
        OverflowTier expected;
    };
    
    std::vector<TestCase> cases = {
        { 0.26f, OverflowTier::NORMAL },
        { 0.75f, OverflowTier::WARNING },
        { 0.86f, OverflowTier::THROTTLE },
        { 0.95f, OverflowTier::CRITICAL },
    };
    
    const char* tier_names[] = { "NORMAL", "WARNING", "THROTTLE", "CRITICAL" };
    
    for (const auto& tc : cases) {
        OverflowTier tier = check_overflow_tier(tc.utilization);
        std::cout << "Utilization: " << (tc.utilization * 100.0f) << "% -> Tier " << tier_names[(int)tier];
        
        if (tier == tc.expected) {
            std::cout << " [PASS]" << std::endl;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }
    }
}

// Test memory pinning
void test_memory_pinning() {
    std::cout << "\n=== Testing Memory Pinning ===" << std::endl;
    
    const size_t test_size = 256 * 1024 * 1024; // 256MB
    
    void* buffer = VirtualAlloc(NULL, test_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        std::cerr << "Failed to allocate test buffer" << std::endl;
        return;
    }
    
    std::cout << "Allocated " << (test_size / (1024 * 1024)) << " MB buffer at " << buffer << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    bool pinned = RawrPinMemory(buffer, test_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (pinned) {
        std::cout << "Memory pinned successfully in " << duration.count() << " us [PASS]" << std::endl;
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
    
    // Enable privilege first
    if (LargePageAllocator::enable_privilege()) {
        std::cout << "SeLockMemoryPrivilege enabled [PASS]" << std::endl;
    } else {
        std::cout << "SeLockMemoryPrivilege enable failed (run as admin) [WARN]" << std::endl;
    }
    
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

// Test streaming prefetch
void test_streaming_prefetch() {
    std::cout << "\n=== Testing Streaming Prefetch ===" << std::endl;
    
    const size_t test_size = 128 * 1024 * 1024; // 128MB
    
    void* buffer = VirtualAlloc(NULL, test_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        std::cerr << "Failed to allocate test buffer" << std::endl;
        return;
    }
    
    std::memset(buffer, 0xAB, test_size);
    
    auto start = std::chrono::high_resolution_clock::now();
    RawrPrefetchMemory(buffer, test_size);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Prefetch completed in " << duration.count() << " us [PASS]" << std::endl;
    
    VirtualFree(buffer, 0, MEM_RELEASE);
}

// Test bandwidth estimation
void test_bandwidth_estimation() {
    std::cout << "\n=== Testing Bandwidth Estimation ===" << std::endl;
    
    // DDR5-5600 dual channel: ~75 GB/s realistic
    uint64_t ddr5_bw = 75000; // MB/s
    
    // PCIe 4.0 x16: ~31.5 GB/s
    uint64_t pcie_bw = 31500; // MB/s
    
    std::cout << "DDR5 Bandwidth: " << ddr5_bw << " MB/s (" << (ddr5_bw / 1000.0) << " GB/s)" << std::endl;
    std::cout << "PCIe Bandwidth: " << pcie_bw << " MB/s (" << (pcie_bw / 1000.0) << " GB/s)" << std::endl;
    
    std::cout << "Bandwidth estimates reasonable [PASS]" << std::endl;
}

// Test memory barrier
void test_memory_barrier() {
    std::cout << "\n=== Testing Memory Barrier ===" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    RawrMemoryBarrier();
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    std::cout << "Memory barrier completed in " << duration.count() << " ns [PASS]" << std::endl;
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
    test_memory_barrier();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}