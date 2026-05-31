// thinking_effort_smoke_test.cpp
#include "thinking_effort_adjuster.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace rawrxd;

struct TestResult {
    const char* name;
    bool passed;
};

int main() {
    std::vector<TestResult> results;

    // 1. Construction + config
    {
        ThinkingEffortAdjuster adj(1ULL * 1024 * 1024 * 1024);
        adj.set_effort_level(ThinkingEffort::Standard);
        adj.set_memory_strategy(MemoryStrategy::Hybrid);
        adj.enable_adaptive_quantization(true);
        results.push_back({"Construction + Config", true});
    }

    // 2. Query complexity estimation
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        float c1 = adj.estimate_query_complexity("hi");
        float c2 = adj.estimate_query_complexity(
            "explain how to prove this algorithm using function analyze compare");
        bool ok = (c2 > c1) && (c2 <= 1.0f) && (c1 >= 0.0f);
        results.push_back({"Query Complexity Estimation", ok});
    }

    // 3. Token estimation
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        size_t t0 = adj.estimate_tokens_needed("test", ThinkingEffort::Off);
        size_t t1 = adj.estimate_tokens_needed("test", ThinkingEffort::Minimal);
        size_t t2 = adj.estimate_tokens_needed("test", ThinkingEffort::Maximum);
        results.push_back({"Token Estimation Scales with Effort", t0 == 0 && t2 > t1});
    }

    // 3b. Six-level parsing + aliases
    {
        bool ok = true;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("off") == ThinkingEffort::Off;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("minimal") == ThinkingEffort::Low;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("standard") == ThinkingEffort::Medium;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("detailed") == ThinkingEffort::High;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("extra") == ThinkingEffort::Extra;
        ok = ok && ThinkingEffortAdjuster::effort_from_string("maximum") == ThinkingEffort::Max;
        ok = ok && std::string(ThinkingEffortAdjuster::effort_to_string(ThinkingEffort::Max)) == "MAX";
        results.push_back({"Six-Level Parsing + Aliases", ok});
    }

    // 3c. Resource budgets are monotonic and bounded
    {
        auto off = ThinkingEffortBudget::from_level(ThinkingEffort::Off);
        auto low = ThinkingEffortBudget::from_level(ThinkingEffort::Low);
        auto max = ThinkingEffortBudget::from_level(ThinkingEffort::Max);
        bool ok = off.max_tokens == 0 && low.max_tokens > off.max_tokens &&
                  max.max_tokens > low.max_tokens && max.max_time_ms >= 300000.0;
        results.push_back({"Resource Budget Mapping", ok});
    }

    // 3d. Recommendation and execution plan selection
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        auto trivial = adj.build_execution_plan("hi", ThinkingTaskPreset::FactChecking, 0.0f);
        auto complex = adj.build_execution_plan(
            "audit and optimize this security-critical architecture with performance constraints",
            ThinkingTaskPreset::ProblemSolving,
            1.0f);
        bool ok = trivial.level == ThinkingEffort::Off && !trivial.reasoning_enabled &&
                  complex.level == ThinkingEffort::Max && complex.reasoning_enabled &&
                  complex.tree_enabled && complex.beam_enabled && complex.agent_count == 8;
        results.push_back({"Execution Plan Selection", ok});
    }

    // 4. Adjust for complexity
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        adj.adjust_for_query_complexity(0.9f);
        adj.adjust_for_query_complexity(0.1f);
        results.push_back({"Adjust for Query Complexity", true});
    }

    // 5. KV cache init + eviction
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        adj.initialize_kv_cache(8, 8, 64);
        adj.evict_low_importance_tokens(0.2f);
        results.push_back({"KV Cache Init + Eviction", true});
    }

    // 6. Precision decision
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        TokenMetrics m;
        m.attention_entropy = 0.5f;
        m.attention_variance = 0.3f;
        m.token_rarity = 0.2f;
        m.quality_score = 0.8f;
        PrecisionLevel p = adj.determine_token_precision(m);
        bool ok = (p == PrecisionLevel::FP16 || p == PrecisionLevel::INT8 ||
                   p == PrecisionLevel::INT4 || p == PrecisionLevel::INT2);
        results.push_back({"Determine Token Precision", ok});
    }

    // 7. Memory pressure callback
    {
        ThinkingEffortAdjuster adj(1024ULL * 1024 * 1024);
        bool called = false;
        adj.set_memory_pressure_callback([&called](float) { called = true; });
        adj.adjust_for_memory_pressure(0.95f);
        results.push_back({"Memory Pressure Callback", called});
    }

    // 8. Memory estimation for context
    {
        ThinkingEffortAdjuster adj(1024 * 1024);
        size_t small = adj.estimate_memory_for_context(128);
        size_t large = adj.estimate_memory_for_context(4096);
        results.push_back({"Context Memory Estimation", large > small});
    }

    // 9. LayerwiseDecomposer
    {
        ThinkingEffortAdjuster::LayerwiseDecomposer dec;
        std::vector<float> w(64 * 64, 0.5f);
        dec.decompose_weights(w.data(), 64, 64, 4);
        auto rec = dec.reconstruct_weights(2);
        results.push_back({"Layerwise Decomposition", rec.size() == 64 * 64});
    }

    // Report
    int failed = 0;
    std::printf("=== Thinking Effort Adjuster Smoke Tests ===\n");
    for (auto& r : results) {
        std::printf("  %s: %s\n", r.name, r.passed ? "OK" : "FAIL");
        if (!r.passed) ++failed;
    }
    std::printf("=== %s === Exit code: %d\n",
                failed == 0 ? "All smoke tests passed" : "FAILURES PRESENT",
                failed == 0 ? 0 : 1);
    return failed == 0 ? 0 : 1;
}
