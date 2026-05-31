// ============================================================================
// TEST: RAWR Streaming Inference Core
// Demonstrates MoE + KV Cache + Speculative Decoding + Telemetry
// Compile: g++ -std=c++17 -O3 -o test_streaming test_streaming_inference.cpp
// ============================================================================
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>

#include "rawr_streaming_inference_core.hpp"
#include "moe_evolutionary_trainer.hpp"
#include "moe_telemetry_ui.hpp"

// Convert Inference::DiagnosticFrame to Telemetry::DiagnosticFrame
RawrXD::Telemetry::DiagnosticFrame ConvertFrame(const RawrXD::Inference::StreamingInferenceEngine::DiagnosticFrame& src) {
    RawrXD::Telemetry::DiagnosticFrame dst;
    dst.acceptance_rate = src.acceptance_rate;
    dst.tokens_produced = src.tokens_produced;
    dst.draft_latency_ms = src.draft_latency_ms;
    dst.verify_latency_ms = src.verify_latency_ms;
    dst.total_ms = src.total_ms;
    dst.expert_id = src.expert_id;
    dst.expert_score = src.expert_score;
    dst.expert_trials = src.expert_trials;
    dst.reward = src.reward;
    dst.timestamp_ms = static_cast<uint64_t>(std::time(nullptr)) * 1000;
    return dst;
}

using namespace RawrXD;

// ----------------------------------------------------------------------------
// Benchmark Harness
// ----------------------------------------------------------------------------
class BenchmarkHarness {
    Inference::StreamingInferenceEngine engine_;
    Telemetry::TelemetryAggregator telemetry_;
    
    static constexpr int WARMUP_ITERS = 10;
    static constexpr int BENCHMARK_ITERS = 100;
    static constexpr float BASELINE_TPS = 10.0f;  // Simulated baseline
    
public:
    void Run() {
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║  RAWR STREAMING INFERENCE CORE - BENCHMARK                   ║\n");
        printf("║  MoE + KV Cache + Speculative Decoding + Telemetry         ║\n");
        printf("╚══════════════════════════════════════════════════════════════╝\n\n");
        
        // Warmup
        printf("Warming up...\n");
        for (int i = 0; i < WARMUP_ITERS; i++) {
            auto tokens = engine_.GenerateStream(100, 20, 0.8f);
            auto frame = engine_.GetDiagnostics();
            telemetry_.RecordFrame(ConvertFrame(frame));
        }
        
        // Benchmark
        printf("\nRunning benchmark (%d iterations)...\n\n", BENCHMARK_ITERS);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < BENCHMARK_ITERS; i++) {
            int start_token = 100 + (i % 50);  // Vary start token
            auto tokens = engine_.GenerateStream(start_token, 32, 0.8f);
            
            auto frame = engine_.GetDiagnostics();
            telemetry_.RecordFrame(ConvertFrame(frame));
            
            // Print progress every 10 iterations
            if ((i + 1) % 10 == 0) {
                printf("  Iteration %d/%d: %d tokens, %.1fms, acceptance=%.1f%%\n",
                       i + 1, BENCHMARK_ITERS,
                       frame.tokens_produced,
                       frame.total_ms,
                       frame.acceptance_rate * 100);
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double total_time = std::chrono::duration<double, std::milli>(end - start).count();
        
        // Print summary
        PrintSummary(total_time);
    }
    
private:
    void PrintSummary(double total_time) {
        auto summary = telemetry_.GetSummary(BASELINE_TPS);
        
        printf("\n");
        printf("╔══════════════════════════════════════════════════════════════╗\n");
        printf("║  BENCHMARK RESULTS                                           ║\n");
        printf("╠══════════════════════════════════════════════════════════════╣\n");
        printf("║  Total Time:        %10.2f ms                            ║\n", total_time);
        printf("║  Total Tokens:      %10llu                               ║\n", summary.total_tokens);
        printf("║  Generations:       %10llu                               ║\n", summary.total_generations);
        printf("║  Overall TPS:       %10.2f tok/s                         ║\n", summary.overall_tps);
        printf("║                                                              ║\n");
        printf("║  ACCEPTANCE RATE                                           ║\n");
        printf("║    Average:         %10.1f%%                              ║\n", summary.avg_acceptance_rate * 100);
        printf("║    Range:           %10.1f%% - %5.1f%%                     ║\n",
               summary.min_acceptance_rate * 100, summary.max_acceptance_rate * 100);
        printf("║                                                              ║\n");
        printf("║  LATENCY                                                     ║\n");
        printf("║    Average:         %10.2f ms                            ║\n", summary.avg_latency_ms);
        printf("║    Range:           %10.2f - %5.2f ms                   ║\n",
               summary.min_latency_ms, summary.max_latency_ms);
        printf("║                                                              ║\n");
        printf("║  ROI (vs %.0f TPS baseline):                                  ║\n", BASELINE_TPS);
        
        const char* roi_status = summary.roi >= 1.0f ? "SPEEDUP ✓" : "REGRESSION ✗";
        // ROI color indicator (green for speedup, red for regression)
        const char* roi_indicator = summary.roi >= 1.0f ? "✓" : "✗";
        printf("║    %.2fx %s                                          ║\n", 
               summary.roi, roi_status);
        
        printf("║                                                              ║\n");
        printf("║  EXPERT PERFORMANCE                                          ║\n");
        for (int i = 0; i < 8; i++) {
            const char* status = summary.expert_scores[i] > 0.7f ? "★" :
                                summary.expert_scores[i] > 0.4f ? "◐" : "○";
            printf("║    Expert %d:  %.2f %s  (%d trials)                    ║\n",
                   i, summary.expert_scores[i], status, summary.expert_trials[i]);
        }
        printf("╚══════════════════════════════════════════════════════════════╝\n");
        
        // Performance assessment
        printf("\n");
        if (summary.roi >= 2.0f) {
            printf("✓ EXCELLENT: Significant speedup achieved!\n");
        } else if (summary.roi >= 1.0f) {
            printf("✓ GOOD: Modest speedup, system is working.\n");
        } else if (summary.roi >= 0.5f) {
            printf("⚠ WARNING: Regression detected. Check acceptance rate.\n");
        } else {
            printf("✗ CRITICAL: Major regression. Draft model may need tuning.\n");
        }
        
        if (summary.avg_acceptance_rate < 0.3f) {
            printf("  → Low acceptance rate (%.1f%%). Draft and target models may be misaligned.\n",
                   summary.avg_acceptance_rate * 100);
        }
        
        printf("\n");
    }
};

// ----------------------------------------------------------------------------
// Evolutionary Trainer Demo
// ----------------------------------------------------------------------------
void DemoEvolutionaryTrainer() {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  EVOLUTIONARY TRAINER DEMO                                   ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    using namespace Training;
    
    // Create trainer
    ExpertTrainer trainer;
    
    // Add training tasks
    for (int i = 0; i < 50; i++) {
        TaskExample task;
        task.input_tokens = {100, 200, 300};
        task.expected_tokens = {400, 500, 600};
        task.importance = 1.0f + (i % 3) * 0.5f;  // Vary importance
        trainer.AddTrainingTask(task);
    }
    
    // Create dummy weights
    std::vector<Q4_1_Block> weights(1024);
    for (auto& w : weights) {
        w.d = Q4_1_Block::FP32ToFP16(1.0f);
        w.m = Q4_1_Block::FP32ToFP16(0.0f);
        for (int j = 0; j < 32; j++) {
            w.qs[j] = static_cast<uint8_t>(rand() & 0xFF);
        }
    }
    
    // Evolve for several generations
    printf("Evolving expert for 10 generations...\n\n");
    
    for (int gen = 0; gen < 10; gen++) {
        trainer.EvolveGeneration(weights.data(), weights.size());
        
        printf("Generation %d: Best fitness = %.4f\n",
               trainer.Generation(), trainer.BestFitness());
    }
    
    printf("\n✓ Evolution complete. Best fitness: %.4f\n\n", trainer.BestFitness());
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    srand(static_cast<unsigned>(time(nullptr)));
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                              ║\n");
    printf("║   RAWR XD - STREAMING INFERENCE CORE TEST                    ║\n");
    printf("║   Zero Dependencies | C++17 | MoE + Speculative Decoding   ║\n");
    printf("║                                                              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // Run evolutionary trainer demo
    DemoEvolutionaryTrainer();
    
    // Run streaming inference benchmark
    BenchmarkHarness benchmark;
    benchmark.Run();
    
    printf("\nPress Enter to exit...\n");
    getchar();
    
    return 0;
}
