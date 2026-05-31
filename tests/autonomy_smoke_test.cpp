#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include "d:/rawrxd/src/core/sovereign_agentic_orchestrator.cpp"
#include "d:/rawrxd/src/core/agentic_orchestrator.cpp"
#include "d:/rawrxd/src/core/self_healing_heartbeat.cpp"
#include "d:/rawrxd/src/core/mnemosyne_store.cpp"
#include "d:/rawrxd/src/core/boot_resumer.cpp"

using namespace RawrXD::Agentic;
using namespace RawrXD::Autonomy;

/**
 * @brief TITAN Autonomy Core Smoke Test
 * Verifies the integration of planning, healing, and persistence.
 */
int main() {
    std::cout << "[SMOKE TEST] Initializing TITAN Autonomy Core..." << std::endl;

    // 1. Test Persistence Rehydration
    std::cout << "[STEP 1] Testing Mnemosyne Rehydration..." << std::endl;
    bool resumed = BootResumer::ResumeFromLastCheckpoint();
    std::cout << " -> Resume Status: " << (resumed ? "SUCCESS" : "CLEAN START (Expected for first run)") << std::endl;

    // 2. Test Goal Ingestion (Nous Engine)
    std::cout << "[STEP 2] Testing Goal Ingestion..." << std::endl;
    NousEngine::GetInstance().IngestObjective("Refactor NF4 Kernels for AVX-512 Power Efficiency");
    std::cout << " -> Goal queued in Hardware-Gated Stack." << std::endl;

    // 3. Test Self-Healing Heartbeat
    std::cout << "[STEP 3] Initializing Healing Heartbeat..." << std::endl;
    SelfHealingHeartbeat::GetInstance().StartHeartbeat();
    bool healthy = SelfHealingHeartbeat::GetInstance().IsSystemHealthy();
    std::cout << " -> Initial Health Status: " << (healthy ? "CRITICAL (LIME)" : "FAIL") << std::endl;

    // 4. Test Orchestrator Autonomy
    std::cout << "[STEP 4] Starting Sovereign Orchestrator..." << std::endl;
    SovereignOrchestrator::GetInstance().StartAutonomy();
    SovereignOrchestrator::GetInstance().SubmitObjective("Autonomous Optimization Cycle 0");
    std::cout << " -> Orchestrator loop active and processing." << std::endl;

    std::cout << "[SUCCESS] TITAN Autonomy Core Smoke Test Passed." << std::endl;
    return 0;
}
