#include <windows.h>
#include <iostream>
#include <vector>
#include <cassert>
#include <thread>
#include <chrono>
#include "../hotpatch/Sentinel.hpp"

using namespace RawrXD::Agentic::Hotpatch;

void SentinelStressTest() {
    std::cout << "[SentinelTest] Starting Stress Test..." << std::endl;

    // 1. Create a dummy memory region (executable/writable)
    size_t regionSize = 64;
    void* dummyRegion = VirtualAlloc(NULL, regionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!dummyRegion) {
        std::cerr << "[SentinelTest] Failed to allocate memory" << std::endl;
        return;
    }

    // Initialize with "clean" data (e.g., NOPs 0x90)
    memset(dummyRegion, 0x90, regionSize);

    // 2. Register with Sentinel
    SentinelSystem::instance().registerRegion(dummyRegion, regionSize, "StressTest_Region");
    
    // Verify initial status
    auto status = SentinelSystem::instance().getStatus();
    bool found = false;
    for (const auto& r : status) {
        if (r.address == dummyRegion) {
            found = true;
            assert(!r.isCorrupt);
            assert(r.violations == 0);
        }
    }
    assert(found);
    std::cout << "[SentinelTest] Region registered successfully." << std::endl;

    // 3. Start background monitoring at high frequency (100ms)
    SentinelSystem::instance().startBackgroundMonitor(100);
    std::cout << "[SentinelTest] Background monitor active." << std::endl;

    // 4. Simulate corruption (inject 0xCC - INT3)
    std::cout << "[SentinelTest] Inducing corruption..." << std::endl;
    memset(dummyRegion, 0xCC, 8); // Corrupt first 8 bytes

    // 5. Wait and verify auto-heal
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    status = SentinelSystem::instance().getStatus();
    for (const auto& r : status) {
        if (r.address == dummyRegion) {
            std::cout << "[SentinelTest] Violations detected: " << r.violations << std::endl;
            assert(r.violations >= 1);
            
            // Verify data is restored to 0x90
            uint8_t* ptr = (uint8_t*)dummyRegion;
            assert(ptr[0] == 0x90);
            std::cout << "[SentinelTest] Auto-heal VERIFIED. Memory restored." << std::endl;
        }
    }

    // 6. Cleanup
    SentinelSystem::instance().stopBackgroundMonitor();
    VirtualFree(dummyRegion, 0, MEM_RELEASE);
    std::cout << "[SentinelTest] Test Complete. System SOLID." << std::endl;
}

int main() {
    SentinelStressTest();
    return 0;
}
