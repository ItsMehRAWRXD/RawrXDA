#include "src/memory/strategy_arbitration.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace RawrXD::Memory;

void test_priority_order() {
    StrategyArbitrator arbitrator;
    ArbitrationConfig config;
    config.policy = ConflictPolicy::PriorityOrder;
    config.highConfidenceThreshold = 0.8f;
    arbitrator.setConfig(config);

    uint64_t tensorId = 123;
    
    // CSMD wants Retain (Priority High)
    // Oracle wants Evict (Priority Low)
    std::vector<StrategyRecommendation> recs = {
        { StrategyId::Oracle, MemoryAction::Evict, 0.1f, 0.9f, "Oracle says evict" },
        { StrategyId::CSMD, MemoryAction::Retain, 1.0f, 0.9f, "CSMD says retain", 456 }
    };

    auto result = arbitrator.resolve(tensorId, recs);
    
    std::cout << "[Test PriorityOrder] Result Action: " << (int)result.action << " (Expected: Retain/0)" << std::endl;
    assert(result.action == MemoryAction::Retain);
    assert(result.winningStrategy == static_cast<uint32_t>(StrategyId::CSMD));
    std::cout << "[PASS] PriorityOrder respected CSMD over Oracle." << std::endl;
}

void test_override_csmd() {
    StrategyArbitrator arbitrator;
    ArbitrationConfig config;
    config.csmdHitOverridesEvict = true;
    config.highConfidenceThreshold = 0.8f;
    arbitrator.setConfig(config);

    uint64_t tensorId = 789;
    
    // Even if policy was HighestScore, the override should trigger.
    std::vector<StrategyRecommendation> recs = {
        { StrategyId::Oracle, MemoryAction::Evict, 2.0f, 0.9f, "Oracle says evict with high score" },
        { StrategyId::CSMD, MemoryAction::Retain, 0.5f, 0.9f, "CSMD says retain", 1010 }
    };

    auto result = arbitrator.resolve(tensorId, recs);
    
    std::cout << "[Test CSMD Override] Action: " << (int)result.action << " (Expected: Retain/0)" << std::endl;
    assert(result.action == MemoryAction::Retain);
    assert(result.winningStrategy == static_cast<uint32_t>(StrategyId::CSMD));
    std::cout << "[PASS] CSMD override triggered successfully." << std::endl;
}

void test_conflict_detection() {
    StrategyArbitrator arbitrator;
    std::vector<StrategyRecommendation> recs = {
        { StrategyId::Oracle, MemoryAction::Evict, 0.5f, 0.5f, "Evict" },
        { StrategyId::AHZP, MemoryAction::Retain, 0.5f, 0.5f, "Retain" }
    };

    auto result = arbitrator.resolve(999, recs);
    std::cout << "[Test Conflict Detection] Had Conflict: " << result.hadConflict << " (Expected: 1)" << std::endl;
    assert(result.hadConflict == true);
    std::cout << "[PASS] Conflict detected correctly." << std::endl;
}

int main() {
    try {
        test_priority_order();
        test_override_csmd();
        test_conflict_detection();
        std::cout << "\nALL ARBITRATION TESTS PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
