// ============================================================================
// test_timing_breakdown_validation.cpp — Three-Clock Measurement Validation
// ============================================================================
// Demonstrates the difference between pipelined (reported) and honest throughput
// ============================================================================

#include <iostream>
#include <cmath>
#include <iomanip>
#include "telemetry/inference_timing_breakdown.h"

using namespace RawrXD::Telemetry;

// ============================================================================
// Realistic timing scenarios (based on your 8813 tok/s run)
// ============================================================================

struct InferenceScenario {
    const char* name;
    uint64_t tokens;
    uint64_t compute_per_token_us;   // Time to run transformer ops
    uint64_t memory_per_token_us;    // Time for KV cache stage-in + prefetch
    uint64_t emission_per_token_us;  // Time for sampling + decode
};

void test_scenario(const InferenceScenario& scenario) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "SCENARIO: " << scenario.name << "\n";
    std::cout << std::string(70, '=') << "\n";
    
    BatchTimingOrchestrator orchestrator;
    orchestrator.start_batch(static_cast<uint32_t>(scenario.tokens));
    auto& tracker = orchestrator.get_tracker();
    
    // Simulate token generation with three-clock tracking
    for (uint64_t i = 0; i < scenario.tokens; ++i) {
        // Clock 1: Compute
        tracker.start_compute();
        for (volatile uint64_t j = 0; j < scenario.compute_per_token_us * 1000; ++j) {}
        tracker.end_compute_start_memory();
        
        // Clock 2: Memory
        for (volatile uint64_t j = 0; j < scenario.memory_per_token_us * 1000; ++j) {}
        tracker.end_memory_start_emission();
        
        // Clock 3: Emission
        for (volatile uint64_t j = 0; j < scenario.emission_per_token_us * 1000; ++j) {}
        tracker.end_emission();
    }
    
    auto result = orchestrator.finalize_batch();
    
    // Print results
    std::cout << format_timing_report(result);
    
    // Additional analysis
    double speedup_ratio = result.effective_tok_per_sec / result.honest_tok_per_sec;
    std::cout << "Speedup factor from pipelining: " << std::fixed << std::setprecision(2) 
              << speedup_ratio << "x\n";
    
    // Identify critical path
    std::cout << "Critical path (bottleneck): ";
    if (result.total_timing.compute_time_us >= result.total_timing.memory_time_us &&
        result.total_timing.compute_time_us >= result.total_timing.emission_time_us) {
        std::cout << "COMPUTE\n";
    } else if (result.total_timing.memory_time_us >= result.total_timing.emission_time_us) {
        std::cout << "MEMORY\n";
    } else {
        std::cout << "EMISSION\n";
    }
}

int main() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  THREE-CLOCK TIMING BREAKDOWN VALIDATION                         ║\n";
    std::cout << "║  Separating pipelined illusions from honest throughput           ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════════╝\n";
    
    // ========================================================================
    // Scenario 1: Your reported 8813 tok/s run
    // Reverse-engineer timing from: 514 tokens in 58.32 ms = 8813 tok/s
    // ========================================================================
    {
        InferenceScenario scenario = {
            "Your 8813 tok/s Run (40B Codestral Q4_K_M)",
            514,  // Total tokens reported
            
            // If 8813 tok/s on 514 tokens = 58.32 ms total
            // With heavy pipelining, this suggests very tight clocks:
            // Estimate: compute dominant, memory + emission overlapped
            100,  // compute: ~100 µs per token (transformer forward)
             15,  // memory:  ~15 µs per token (overlapped prefetch)
             10   // emission: ~10 µs per token (sampling + output)
        };
        test_scenario(scenario);
    }
    
    // ========================================================================
    // Scenario 2: Worst-case non-overlapped baseline
    // ========================================================================
    {
        InferenceScenario scenario = {
            "Worst-Case Sequential (no pipelining)",
            100,
            100,  // compute: ~100 µs per token
            100,  // memory:  ~100 µs per token
            100   // emission: ~100 µs per token
        };
        test_scenario(scenario);
    }
    
    // ========================================================================
    // Scenario 3: Memory-saturated (typical GPU setting)
    // ========================================================================
    {
        InferenceScenario scenario = {
            "Memory-Saturated Scenario",
            256,
            50,   // compute: ~50 µs per token (compute-light, memory-heavy)
            300,  // memory:  ~300 µs per token (VRAM bottleneck)
            10    // emission: ~10 µs per token
        };
        test_scenario(scenario);
    }
    
    // ========================================================================
    // Scenario 4: Compute-intensive (large model, small cache)
    // ========================================================================
    {
        InferenceScenario scenario = {
            "Compute-Intensive Scenario (100B+ model)",
            256,
            500,  // compute: ~500 µs per token (heavy ops)
            50,   // memory:  ~50 µs per token (prefetch overlapped)
            20    // emission: ~20 µs per token
        };
        test_scenario(scenario);
    }
    
    // ========================================================================
    // Scenario 5: Perfect scalability target
    // ========================================================================
    {
        InferenceScenario scenario = {
            "Perfect Scalability Target (all 3 clocks balanced)",
            1024,
            100,  // compute: ~100 µs per token
            100,  // memory:  ~100 µs per token
            100   // emission: ~100 µs per token
        };
        test_scenario(scenario);
    }
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "KEY INSIGHT:\n";
    std::cout << "━━━━━━━━━━━━\n";
    std::cout << "Your actual system is now so well-pipelined that aggregate timing\n";
    std::cout << "doesn't tell you which phase is the bottleneck.\n\n";
    std::cout << "Use this breakdown to:\n";
    std::cout << "  1. Identify if compute/memory/emission is limiting\n";
    std::cout << "  2. Separate real throughput from pipeline compression\n";
    std::cout << "  3. Make precise optimization decisions\n";
    std::cout << std::string(70, '=') << "\n\n";
    
    return 0;
}
