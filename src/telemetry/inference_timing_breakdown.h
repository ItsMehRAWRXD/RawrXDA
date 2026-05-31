// ============================================================================
// inference_timing_breakdown.h — Three-Clock Fidelity Instrumentation
// ============================================================================
// Separates overlapped execution into honest compute/memory/token-emission clocks
// to eliminate pipeline compression artifacts from throughput metrics.
// ============================================================================

#pragma once

#include <cstdint>
#include <chrono>
#include <vector>
#include <array>
#include <algorithm>

namespace RawrXD::Telemetry {

// ============================================================================
// Three-Clock Timing Model
// ============================================================================
struct TimingBreakdown {
    // Clock 1: Compute cycles (FP32/FP16 ops on GPU/CPU)
    uint64_t compute_time_us = 0;
    
    // Clock 2: Memory staging (DDR5/VRAM transfers, prefetch, KV cache stage-in)
    uint64_t memory_time_us = 0;
    
    // Clock 3: Token emission (decode loop, logits sampling, output overhead)
    uint64_t emission_time_us = 0;
    
    // Derived: critical path (max of the three)
    uint64_t critical_path_us() const {
        return std::max({compute_time_us, memory_time_us, emission_time_us});
    }
    
    // Derived: total observed time (sum if sequential, critical_path if overlapped)
    uint64_t total_sequential_us() const {
        return compute_time_us + memory_time_us + emission_time_us;
    }
    
    // Derived: overlap factor (indicates how well-pipelined)
    double overlap_factor() const {
        if (total_sequential_us() == 0) return 0.0;
        return static_cast<double>(total_sequential_us()) / static_cast<double>(critical_path_us());
    }
};

// ============================================================================
// Per-Token Timing Instrumentation
// ============================================================================
class TokenTimingTracker {
public:
    TokenTimingTracker() : token_count_(0), breakdown_({0, 0, 0}) {}
    
    // Start compute phase
    void start_compute() {
        compute_start_ = std::chrono::high_resolution_clock::now();
    }
    
    // End compute phase, start memory phase
    void end_compute_start_memory() {
        auto now = std::chrono::high_resolution_clock::now();
        breakdown_.compute_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
            now - compute_start_
        ).count();
        memory_start_ = now;
    }
    
    // End memory phase, start token emission phase
    void end_memory_start_emission() {
        auto now = std::chrono::high_resolution_clock::now();
        breakdown_.memory_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
            now - memory_start_
        ).count();
        emission_start_ = now;
    }
    
    // End token emission phase, latch this token's timing
    void end_emission() {
        auto now = std::chrono::high_resolution_clock::now();
        breakdown_.emission_time_us += std::chrono::duration_cast<std::chrono::microseconds>(
            now - emission_start_
        ).count();
        ++token_count_;
    }
    
    // Get current breakdown (accumulated across all tokens)
    TimingBreakdown get_breakdown() const {
        return breakdown_;
    }
    
    // Get per-token averages
    TimingBreakdown get_per_token_avg() const {
        if (token_count_ == 0) return {0, 0, 0};
        return {
            breakdown_.compute_time_us / token_count_,
            breakdown_.memory_time_us / token_count_,
            breakdown_.emission_time_us / token_count_
        };
    }
    
    uint64_t token_count() const { return token_count_; }
    
    // Reset for next batch
    void reset() {
        token_count_ = 0;
        breakdown_ = {0, 0, 0};
    }
    
private:
    std::chrono::high_resolution_clock::time_point compute_start_;
    std::chrono::high_resolution_clock::time_point memory_start_;
    std::chrono::high_resolution_clock::time_point emission_start_;
    
    uint64_t token_count_;
    TimingBreakdown breakdown_;
};

// ============================================================================
// Batch-Level Timing Orchestration
// ============================================================================
class BatchTimingOrchestrator {
public:
    struct BatchResult {
        uint64_t total_tokens;
        TimingBreakdown total_timing;
        TimingBreakdown per_token_avg;
        double effective_tok_per_sec;  // accounting for overlap
        double honest_tok_per_sec;      // sequential bottleneck only
        double overlap_ratio;
    };
    
    void start_batch(uint32_t expected_tokens) {
        batch_tokens_ = expected_tokens;
        tracker_.reset();
    }
    
    TokenTimingTracker& get_tracker() { return tracker_; }
    
    BatchResult finalize_batch() {
        auto total = tracker_.get_breakdown();
        auto per_token = tracker_.get_per_token_avg();
        
        // Effective tok/sec = (tokens counted) / (elapsed real time)
        // This is what benchmarks report (and inflates due to pipelining)
        uint64_t total_elapsed_us = std::max({
            total.compute_time_us,
            total.memory_time_us,
            total.emission_time_us
        });
        
        double effective_tps = (total_elapsed_us > 0)
            ? (static_cast<double>(tracker_.token_count()) * 1e6) / total_elapsed_us
            : 0.0;
        
        // Honest tok/sec = (tokens) / (sum of sequential phases)
        // This is the true throughput without overlap illusions
        uint64_t sequential_us = total.compute_time_us + total.memory_time_us + total.emission_time_us;
        double honest_tps = (sequential_us > 0)
            ? (static_cast<double>(tracker_.token_count()) * 1e6) / sequential_us
            : 0.0;
        
        double overlap = total.overlap_factor();
        
        return {
            tracker_.token_count(),
            total,
            per_token,
            effective_tps,
            honest_tps,
            overlap
        };
    }
    
private:
    uint32_t batch_tokens_;
    TokenTimingTracker tracker_;
};

// ============================================================================
// Diagnostic Formatter
// ============================================================================
inline std::string format_timing_report(const BatchTimingOrchestrator::BatchResult& result) {
    char buffer[1024];
    snprintf(
        buffer, sizeof(buffer),
        "\n=== TIMING BREAKDOWN ===\n"
        "Tokens: %llu\n"
        "Compute:   %10llu µs (%.2f µs/tok)\n"
        "Memory:    %10llu µs (%.2f µs/tok)\n"
        "Emission:  %10llu µs (%.2f µs/tok)\n"
        "Critical:  %10llu µs\n"
        "\n--- INTERPRETATION ---\n"
        "Effective TPS (reported):  %.2f tok/s  [pipelined]\n"
        "Honest TPS (sequential):   %.2f tok/s  [bottleneck]\n"
        "Overlap ratio:             %.2fx       [pipeline compression]\n"
        "======================\n",
        result.total_tokens,
        result.total_timing.compute_time_us, 
        static_cast<double>(result.per_token_avg.compute_time_us),
        result.total_timing.memory_time_us,
        static_cast<double>(result.per_token_avg.memory_time_us),
        result.total_timing.emission_time_us,
        static_cast<double>(result.per_token_avg.emission_time_us),
        result.total_timing.critical_path_us(),
        result.effective_tok_per_sec,
        result.honest_tok_per_sec,
        result.overlap_ratio
    );
    return std::string(buffer);
}

}  // namespace RawrXD::Telemetry
