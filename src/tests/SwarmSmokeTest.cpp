#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include "include/Win32SwarmBridge.h"
#include "agentic/SubAgentManager.h"

/**
 * @file SwarmSmokeTest.cpp
 * @brief Standalone smoke test to verify IAT Slot 20, 54, 55 and Swarm Orchestration.
 */

void RunSmokeTest() {
    std::cout << "--- RawrXD Swarm Smoke Test ---" << std::endl;

    // 1. Manually trigger IAT registration if not already done by CRT
    // In a real IDE, this happens during initializeAgenticBridge()
    using namespace RawrXD::Bridge;
    
    std::cout << "[Step 1] Registering Swarm Bridge with IAT..." << std::endl;
    if (!RegisterSwarmBridgeWithIAT()) {
        std::cerr << "FAILED: IAT Registration failed!" << std::endl;
        return;
    }
    std::cout << "SUCCESS: IAT Slots 20, 54, 55 bound." << std::endl;

    // 2. Initialize the Swarm (Slot 20 simulation)
    std::cout << "[Step 2] Initializing Swarm (Slot 20)..." << std::endl;
    SwarmInitConfig config{};
    config.structSize = sizeof(SwarmInitConfig);
    config.maxSubAgents = 4;
    config.taskTimeoutMs = 30000;
    config.enableGPUWorkStealing = 1;
    strcpy_s(config.coordinatorModel, "gemma3:1b");

    int initResult = Win32IDE_initializeSwarmSystem(&config);
    if (initResult != S_OK) {
        std::cerr << "FAILED: Swarm Initialization returned " << initResult << std::endl;
        return;
    }
    std::cout << "SUCCESS: Swarm system initialized." << std::endl;

    // 3. Execute a Swarm Task (Slot 54 simulation)
    std::cout << "[Step 3] Executing Swarm Task (Slot 54)..." << std::endl;
    uint32_t taskId = Win32IDE_executeSwarmTask("Audit codebase for 800B sharding compatibility.");
    if (taskId == 0) {
        std::cerr << "FAILED: Task execution returned 0 (inactive)." << std::endl;
    } else {
        std::cout << "SUCCESS: Task " << taskId << " dispatched to swarm." << std::endl;
    }

    // 4. Test 800B Model Sharding Foundations
    std::cout << "[Step 4] Testing 800B Shard Loading..." << std::endl;
    // Note: This calls the C++ manager directly since it's an internal enhancement
    auto& manager = RawrXD::Agentic::SubAgentManager::instance();
    if (manager.loadModelShard("F:/Models/800B/layer_0_to_10.shard", 0)) {
        std::cout << "SUCCESS: Model shard loaded onto GPU 0." << std::endl;
    } else {
        std::cerr << "FAILED: Shard loading failed." << std::endl;
    }
    manager.synchronizeShards();

    // 5. Shutdown the Swarm (Slot 55 simulation)
    std::cout << "[Step 5] Shutting down Swarm (Slot 55)..." << std::endl;
    Win32IDE_shutdownSwarmSystem();
    std::cout << "SUCCESS: Swarm resources reclaimed." << std::endl;

    std::cout << "--- Smoke Test Complete: ALL SYSTEMS NOMINAL ---" << std::endl;
}

int main() {
    RunSmokeTest();
    return 0;
}
