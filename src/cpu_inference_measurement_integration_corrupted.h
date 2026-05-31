// cpu_inference_measurement_integration.h
// Integrates corrected measurement & pattern recognition into CPUInferenceEngine

#pragma once

#include "rawr_benchmark_measurement_corrected.h"
#include "rawr_autopatch_realtime_recognizer.h"
#include <chrono>
#include <deque>
#include <memory>
#include <functional>
#include <numeric>

namespace RawrXD {
namespace Inference {

using clock_t = std::chrono::high_resolution_clock;
using TelemetryWindow = RawrXD::Autopatch::TelemetryWindow;
using PatternRecognizer = RawrXD::Autopatch::RealtimePatternRecognizer;
using TelemetrySnapshot = RawrXD::Autopatch::TelemetrySnapshot;
using PerfPattern = RawrXD::Autopatch::PerfPattern;
using Diagnosis = RawrXD::Autopatch::Diagnosis;
using PatternDiagnosticEngine = RawrXD::Autopatch::PatternDiagnosticEngine;
using CorrectMeasurement = RawrXD::Benchmark::CorrectMeasurement;

// ============================================================================
// MEASUREMENT INTEGRATION LAYER (wire into GenerateStreaming)
// ============================================================================

class MeasurementCollector {
private:
    TelemetryWindow telemetry_window_;
    PatternRecognizer pattern_recognizer_;
    std::function<void(const Diagnosis&)> on_diagnosis_callback_;
    
    clock_t::time_point session_start_;
    clock_t::time_point first_token_time_;
    int tokens_generated_ = 0;
    bool first_token_emitted_ = false;
    std::vector<std::chrono::nanoseconds> token_generation_times_;
    
public:
    MeasurementCollector() : session_start_(clock_t::now()) {}
    
    // Call this BEFORE generating each token
    void TokenGenerationStart() {}
    
    // Call this AFTER each token is generated
    void TokenGenerationEnd(
        int token_id,
        double current_bandwidth_gbps,
        double current_cache_hit_rate,
        int prefetch_depth,
        float memory_pressure_pct,
        int tier_current
    ) {
        auto now = clock_t::now();
        
        // First token special handling
        if (!first_token_emitted_) {
            first_token_time_ = now;
            first_token_emitted_ = true;
            tokens_generated_ = 1;
            return;
        }
        
        // Subsequent tokens
        tokens_generated_++;
        auto generation_time = now - session_start_;
        token_generation_times_.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(generation_time)
        );
        
        // Calculate instantaneous TPS
        #include "speculative/rawr_benchmark_measurement_corrected.h"
        #include "speculative/rawr_autopatch_realtime_recognizer.h"
        TelemetrySnapshot snap;
        #include "speculative/rawr_benchmark_measurement_corrected.h"
        #include "speculative/rawr_autopatch_realtime_recognizer.h"
        snap.timestamp = now;
        snap.tps = instantaneous_tps;
        snap.bandwidth_gbps = current_bandwidth_gbps;
        snap.cache_hit_rate = current_cache_hit_rate;
        snap.prefetch_depth = prefetch_depth;
        snap.memory_pressure_percent = memory_pressure_pct;
        snap.latency_per_token_us = token_latency.count() / 1000.0;
        snap.tier_current = tier_current;
        snap.is_first_token = false;
        
        // Add to window
        telemetry_window_.AddSnapshot(snap);
        
        // Check patterns every 10 tokens
        if (tokens_generated_ % 10 == 0) {
            PerfPattern pattern = pattern_recognizer_.RecognizePatterns();
            Diagnosis diag = PatternDiagnosticEngine::DiagnosePerformanceIssue(pattern);
            
            // Optional: emit diagnostic to observer
            if (on_diagnosis_callback_) {
                on_diagnosis_callback_(diag);
            }
        }
    }
    
    // Get final measurement after generation completes
    CorrectMeasurement GetFinalMeasurement() {
        CorrectMeasurement m;
            m.time_to_first_token = std::chrono::duration_cast<std::chrono::milliseconds>(
                first_token_time_ - session_start_
            );
        
        if (token_generation_times_.size() > 1) {
                auto total_decode_ns =
                std::accumulate(
                    token_generation_times_.begin() + 1,
                    token_generation_times_.end(),
                        std::chrono::nanoseconds(0)
                    );
                m.time_per_token_avg = std::chrono::duration_cast<std::chrono::milliseconds>(
                    total_decode_ns / (token_generation_times_.size() - 1)
                );
                m.tokens_generated_real = tokens_generated_;
        } else {
                m.time_per_token_avg = std::chrono::milliseconds(0);
                m.tokens_generated_real = tokens_generated_;
        }
        
            m.total_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock_t::now() - session_start_
            );
        
        return m;
    }
    
    // Get current pattern diagnosis
    Diagnosis GetCurrentDiagnosis() {
        auto pattern = pattern_recognizer_.RecognizePatterns();
        return PatternDiagnosticEngine::DiagnosePerformanceIssue(pattern);
    }
    
    // Register callback for when patterns are detected
    void SetDiagnosisCallback(std::function<void(const Diagnosis&)> callback) {
        on_diagnosis_callback_ = callback;
    }
};

}  // namespace Inference
}  // namespace RawrXD

