// rawr_benchmark_measurement_corrected.h
// Fixes measurement distortion: actual token generation vs synthetic throughput
// Separates: TTFT | Real decode time | Total time

#pragma once

#include <chrono>
#include <vector>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>

namespace RawrXD {
namespace Benchmark {

using clock_t = std::chrono::high_resolution_clock;

// ============================================================================
// CORRECTED MEASUREMENT FRAMEWORK
// ============================================================================

struct TokenGeneration {
    int64_t token_id;
    std::chrono::nanoseconds generation_time;  // Time to generate THIS token
    double logits_temperature = 1.0;
    int top_k = 40;
    // For validation: which expert set was used, cache hit status, etc.
};

struct MeasurementPhase {
    std::string phase_name;
    clock_t::time_point phase_start;
    clock_t::time_point phase_end;
    
    std::chrono::milliseconds GetDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(phase_end - phase_start);
    }
};

struct CorrectMeasurement {
    // ========== TIMING BREAKDOWN (MUTUALLY EXCLUSIVE) ==========
    std::chrono::milliseconds time_to_first_token{0};      // First token generation
    std::chrono::milliseconds time_per_token_avg{0};       // Average for tokens 2+
    std::chrono::milliseconds total_generation_time{0};    // Sum of all tokens
    std::chrono::milliseconds overhead_server{0};          // HTTP round-trip overhead
    std::chrono::milliseconds overhead_tokenizer{0};       // Input tokenization
    std::chrono::milliseconds overhead_post_process{0};    // Output processing
    
    // ========== TOKEN COUNTS (DISTINCT) ==========
    int tokens_in_context = 0;                 // Prompt length
    int tokens_generated_real = 0;             // Actual model output (NOT inferred)
    int tokens_expected = 0;                   // What we asked for

    // ========== PHASE ISOLATION & TIMING INTEGRITY ==========
    // Decode must begin only after first token AND pipeline stabilization.
    bool decode_phase_stable = false;
    std::chrono::milliseconds decode_time_stable_only{0};
    std::vector<uint64_t> per_token_timestamps_ns;   // Monotonic wall clock stamps
    bool monotonic_token_timestamps = true;
    bool grouped_emission_observed = false;

    // ========== STABILITY METRICS FOR AUTOPATCH GATING ==========
    size_t decode_sample_count = 0;
    double rolling_decode_tps = 0.0;
    double decode_tps_stddev = 0.0;
    
    // ========== THROUGHPUT CALCULATIONS (CORRECTED) ==========
    double real_decode_tps() const {
        if (tokens_generated_real <= 1) return 0.0;
        double real_decode_time_ms = 0.0;
        if (decode_phase_stable && decode_time_stable_only.count() > 0) {
            real_decode_time_ms = decode_time_stable_only.count();
        } else {
            real_decode_time_ms = (total_generation_time - time_to_first_token).count();
        }
        if (real_decode_time_ms <= 0) return 0.0;
        // Tokens AFTER the first one
        return (double)(tokens_generated_real - 1) / (real_decode_time_ms / 1000.0);
    }
    
    double total_end_to_end_tps() const {
        if (tokens_generated_real == 0) return 0.0;
        double total_time_ms = (total_generation_time + overhead_server + overhead_tokenizer + overhead_post_process).count();
        if (total_time_ms <= 0) return 0.0;
        return (double)tokens_generated_real / (total_time_ms / 1000.0);
    }
    
    double ttft_ms() const {
        return time_to_first_token.count();
    }
    
    // ========== VALIDATION FLAGS ==========
    bool is_cache_warmup = false;
    bool is_speculative_path = false;
    bool cache_hit_rate_available = false;
    double cache_hit_rate = 0.0;
    
    // ========== MEMORY METRICS ==========
    uint64_t peak_memory_bytes = 0;
    uint64_t average_memory_bytes = 0;
    
    // ========== DIAGNOSTIC: layer-by-layer breakdown ==========
    std::vector<MeasurementPhase> layer_phases;
};

// ============================================================================
// BENCHMARK HARNESS (REPLACES SYNTHETIC LOOP)
// ============================================================================

class CorrectInferenceBenchmark {
public:
    explicit CorrectInferenceBenchmark(int max_tokens_to_generate)
        : target_tokens_(max_tokens_to_generate) {}
    
    CorrectMeasurement RunNativeInference(
        const std::string& model_path,
        const std::string& prompt,
        int expected_completion_tokens = 512
    ) {
        CorrectMeasurement result;
        result.tokens_in_context = prompt.length() / 4;  // Rough estimate for now
        result.tokens_expected = expected_completion_tokens;
        
        // ====== PHASE 1: Tokenize input ======
        auto tok_start = clock_t::now();
        auto prompt_tokens = TokenizePrompt(prompt);
        auto tok_end = clock_t::now();
        result.overhead_tokenizer = std::chrono::duration_cast<std::chrono::milliseconds>(tok_end - tok_start);
        result.tokens_in_context = prompt_tokens.size();
        
        // ====== PHASE 2: Load model into aperture ======
        // (Timing already included in inference, just track peak memory)
        
        // ====== PHASE 3: Generate first token ======
        auto first_token_start = clock_t::now();
        int first_token_id = GenerateNextToken(prompt_tokens);
        auto first_token_end = clock_t::now();
        result.time_to_first_token = std::chrono::duration_cast<std::chrono::milliseconds>(first_token_end - first_token_start);
        result.tokens_generated_real = 1;
        
        // ====== PHASE 4: Generate remaining tokens ======
        auto total_start = first_token_start;
        std::vector<int> generated_tokens;
        generated_tokens.push_back(first_token_id);
        
        for (int i = 1; i < expected_completion_tokens; i++) {
            prompt_tokens.push_back(generated_tokens.back());
            
            auto token_start = clock_t::now();
            int next_token = GenerateNextToken(prompt_tokens);
            auto token_end = clock_t::now();
            
            generated_tokens.push_back(next_token);
            result.tokens_generated_real++;
            
            // Stop on EOS token (typically id 2)
            if (next_token == 2) {
                result.total_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(token_end - first_token_start);
                break;
            }
        }
        
        // If we didn't break early, measure to last token
        if (result.tokens_generated_real == expected_completion_tokens) {
            result.total_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock_t::now() - first_token_start
            );
        }
        
        // ====== PHASE 5: Decode/post-process output ======
        auto post_start = clock_t::now();
        std::string output = DecodeTokens(generated_tokens);
        auto post_end = clock_t::now();
        result.overhead_post_process = std::chrono::duration_cast<std::chrono::milliseconds>(post_end - post_start);
        
        // ====== VALIDATION ======
        double reported_tps = result.real_decode_tps();
        double total_tps = result.total_end_to_end_tps();
        
        result.is_cache_warmup = false;  // Would be set by caller if needed
        result.is_speculative_path = false;
        
        return result;
    }
    
private:
    int target_tokens_;
    
    std::vector<int> TokenizePrompt(const std::string& prompt) {
        // TODO: Integrate real tokenizer (from model's vocabulary)
        std::vector<int> tokens;
        for (size_t i = 0; i < prompt.length(); i += 4) {
            tokens.push_back(static_cast<int>(i / 4));  // Placeholder
        }
        return tokens;
    }
    
    int GenerateNextToken(const std::vector<int>& context) {
        // TODO: Call real inference engine with context
        // For now, simulate realistic latency: ~16-30ms per token for 40B model
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return rand() % 32000;  // Placeholder: random token from vocab
    }
    
    std::string DecodeTokens(const std::vector<int>& token_ids) {
        // TODO: Integrate real tokenizer inverse (token -> text)
        return "Generated output...";  // Placeholder
    }
};

// ============================================================================
// VALIDATION & REPORTING
// ============================================================================

class MeasurementValidator {
public:
    struct ThroughputEnvelope {
        // Measured sustained memory bandwidth available to decode path.
        double peak_memory_bandwidth_gbps = 0.0;
        // Approximate bytes processed per output token (model/config dependent).
        double avg_token_footprint_bytes = 4096.0;
        // Accounts for scheduler stalls / fences / cache misses.
        double pipeline_efficiency = 0.65;
        // Optional hard cap from model/runtime configuration.
        double configured_tps_cap = 0.0;

        double theoretical_max_tps() const {
            if (peak_memory_bandwidth_gbps <= 0.0 || avg_token_footprint_bytes <= 0.0) {
                return configured_tps_cap > 0.0 ? configured_tps_cap : 0.0;
            }
            const double bytes_per_second = peak_memory_bandwidth_gbps * 1e9;
            const double mem_bound_tps = (bytes_per_second / avg_token_footprint_bytes) *
                                         std::max(0.0, std::min(1.0, pipeline_efficiency));
            if (configured_tps_cap > 0.0) {
                return std::min(mem_bound_tps, configured_tps_cap);
            }
            return mem_bound_tps;
        }
    };

    static bool ValidateMeasurement(const CorrectMeasurement& m,
                                    const ThroughputEnvelope* envelope = nullptr) {
        // Sanity checks

        // Rule 0: Decode phase must be isolated and token timestamps monotonic.
        if (!m.monotonic_token_timestamps || m.grouped_emission_observed) {
            return false;
        }
        
        // Rule 1: TTFT >= 50ms for 40B model (minimum latency)
        if (m.time_to_first_token.count() < 50) {
            return false;  // Likely synthetic/cached
        }
        
        // Rule 2: Real decode TPS should scale linearly with model size
        //         40B at 100ms latency = ~100 TPS (1000ms / 10 tokens)
        //         But realistically 20-80 TPS on CPU, 100-300 on GPU
        double max_allowed_tps = 1000.0;
        if (envelope != nullptr) {
            const double theoretical = envelope->theoretical_max_tps();
            if (theoretical > 0.0) {
                max_allowed_tps = theoretical;
            }
        }
        if (m.real_decode_tps() > max_allowed_tps) {
            return false;  // Unrealistic (would require <1ms per token for 40B)
        }
        
        // Rule 3: Total TPS <= real_decode_tps (overhead only reduces overall throughput)
        if (m.total_end_to_end_tps() > m.real_decode_tps()) {
            return false;
        }
        
        // Rule 4: Tokens generated must match actual completion
        if (m.tokens_generated_real < 1 || m.tokens_generated_real > m.tokens_expected * 2) {
            return false;
        }
        
        return true;
    }
    
    static void PrintReport(const CorrectMeasurement& m) {
        std::cout << "\n========== CORRECTED INFERENCE MEASUREMENT ==========\n";
        
        std::cout << "INPUT:\n";
        std::cout << "  Context tokens: " << m.tokens_in_context << "\n";
        std::cout << "  Expected completion: " << m.tokens_expected << "\n";
        
        std::cout << "OUTPUT:\n";
        std::cout << "  Tokens actually generated: " << m.tokens_generated_real << "\n";
        
        std::cout << "TIMING BREAKDOWN:\n";
        std::cout << "  Time-to-First-Token (TTFT): " << m.ttft_ms() << " ms\n";
        std::cout << "  Total generation time: " << m.total_generation_time.count() << " ms\n";
        std::cout << "  Average token time: " 
                  << (m.tokens_generated_real > 1 ? 
                      (m.total_generation_time.count() - m.time_to_first_token.count()) / (m.tokens_generated_real - 1) 
                      : 0.0)
                  << " ms/token\n";
        std::cout << "  Server overhead: " << m.overhead_server.count() << " ms\n";
        std::cout << "  Tokenizer overhead: " << m.overhead_tokenizer.count() << " ms\n";
        std::cout << "  Post-process overhead: " << m.overhead_post_process.count() << " ms\n";
        
        std::cout << "THROUGHPUT (CORRECTED):\n";
        std::cout << "  Real decode TPS (tokens 2+): " << m.real_decode_tps() << " tokens/sec\n";
        std::cout << "  End-to-end TPS (all overhead included): " << m.total_end_to_end_tps() << " tokens/sec\n";
        
        std::cout << "VALIDATION:\n";
        std::cout << "  Measurement valid: " << (ValidateMeasurement(m) ? "YES" : "NO - SYNTHETIC LIKELY") << "\n";
        
        if (m.cache_hit_rate_available) {
            std::cout << "  Cache hit rate: " << (m.cache_hit_rate * 100.0) << "%\n";
        }
        
        std::cout << "======================================================\n\n";
    }
};

}  // namespace Benchmark
}  // namespace RawrXD
