// rawr_inference_measurement_integration.h
// Integrates corrected measurement & pattern recognition into CPUInferenceEngine

#pragma once

#include "rawr_benchmark_measurement_corrected.h"
#include "rawr_autopatch_realtime_recognizer.h"
#include <chrono>
#include <deque>
#include <memory>
#include <functional>

namespace RawrXD {
namespace Inference {

using clock_t = std::chrono::high_resolution_clock;

// ============================================================================
// MEASUREMENT INTEGRATION LAYER (wire into GenerateStreaming)
// ============================================================================

class MeasurementCollector {
private:
    RawrXD::Benchmark::TelemetryWindow telemetry_window_;
    RawrXD::Autopatch::RealtimePatternRecognizer pattern_recognizer_;
    
    clock_t::time_point session_start_;
    clock_t::time_point first_token_time_;
    int tokens_generated_ = 0;
    bool first_token_emitted_ = false;
    
    // Raw measurement
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
        
        // Calculate instantaneous TPS (this token only)
        auto token_latency = token_generation_times_.back();
        double instantaneous_tps = 1e9 / token_latency.count();
        
        // Build telemetry snapshot
        Autopatch::TelemetrySnapshot snap;
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
            Autopatch::PerfPattern pattern = pattern_recognizer_.RecognizePatterns();
            Autopatch::Diagnosis diag = 
                Autopatch::PatternDiagnosticEngine::DiagnosePerformanceIssue(pattern);
            
            // Optional: emit diagnostic to observer
            if (on_diagnosis_callback_) {
                on_diagnosis_callback_(diag);
            }
        }
    }
    
    // Get final measurement after generation completes
    Benchmark::CorrectMeasurement GetFinalMeasurement(
        int tokens_in_context,
        int tokens_expected,
        const std::string& diagnostic_notes = ""
    ) {
        Benchmark::CorrectMeasurement result;
        auto now = clock_t::now();
        
        // Fill in measurements
        result.tokens_in_context = tokens_in_context;
        result.tokens_expected = tokens_expected;
        result.tokens_generated_real = tokens_generated_;
        
        // Timing
        result.time_to_first_token = std::chrono::duration_cast<std::chrono::milliseconds>(
            first_token_time_ - session_start_
        );
        
        result.total_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - first_token_time_
        );
        
        // Calculate average per-token (for reporting)
        if (token_generation_times_.size() > 1) {
            double avg_ns = 0.0;
            for (size_t i = 1; i < token_generation_times_.size(); i++) {
                avg_ns += token_generation_times_[i].count();
            }
            avg_ns /= (token_generation_times_.size() - 1);
            result.time_per_token_avg = std::chrono::milliseconds(
                static_cast<long>(avg_ns / 1e6)
            );
        }
        
        // Default zero for server overhead (can be set separately if needed)
        result.overhead_server = std::chrono::milliseconds(0);
        result.overhead_tokenizer = std::chrono::milliseconds(0);
        result.overhead_post_process = std::chrono::milliseconds(0);
        
        return result;
    }
    
    // Get current pattern diagnosis
    Autopatch::Diagnosis GetCurrentDiagnosis() {
        auto pattern = pattern_recognizer_.RecognizePatterns();
        return Autopatch::PatternDiagnosticEngine::DiagnosePerformanceIssue(pattern);
    }
    
    // Register callback for when patterns are detected
    void SetDiagnosisCallback(
        std::function<void(const Autopatch::Diagnosis&)> callback
    ) {
        on_diagnosis_callback_ = callback;
    }
    
private:
    std::function<void(const Autopatch::Diagnosis&)> on_diagnosis_callback_;
};

// ============================================================================
// INTEGRATION PATCH (copy into CPUInferenceEngine class)
// ============================================================================

/*
HOW TO INTEGRATE INTO CPUInferenceEngine::GenerateStreaming():

1. Add member variable in CPUInferenceEngine:
   ```cpp
   std::unique_ptr<MeasurementCollector> measurement_collector_;
   ```

2. In GenerateStreaming(), at the start:
   ```cpp
   measurement_collector_ = std::make_unique<MeasurementCollector>();
   
   // Optional: register diagnosis callback
   measurement_collector_->SetDiagnosisCallback(
       [this](const Autopatch::Diagnosis& diag) {
           // Log diagnosis or trigger autopatch
           if (m_swarmTelemetryCallback) {
               m_swarmTelemetryCallback(
                   "DIAGNOSIS: " + diag.root_cause + 
                   " (confidence: " + std::to_string(diag.confidence) + ")"
               );
           }
       }
   );
   ```

3. In the token generation loop, after generating each token:
   ```cpp
   // Collect telemetry
   measurement_collector_->TokenGenerationEnd(
       token_id,
       GetCurrentBandwidth(),           // From aperture system
       GetCacheHitRate(),               // From CPU/GPU counters
       prefetch_depth_,
       GetMemoryPressurePercent(),
       aperture_tier_
   );
   ```

4. After generation completes, capture measurement:
   ```cpp
   auto measurement = measurement_collector_->GetFinalMeasurement(
       input_tokens.size(),
       max_tokens
   );
   
   // Validate
   if (Benchmark::MeasurementValidator::ValidateMeasurement(measurement)) {
       Benchmark::MeasurementValidator::PrintReport(measurement);
   } else {
       std::cerr << "WARNING: Invalid measurement detected\n";
   }
   
   // Store for telemetry system
   last_good_measurement_ = measurement;
   ```

5. Optional: hook autopatch for auto-tuning:
   ```cpp
   if (enable_auto_tuning_) {
       auto diag = measurement_collector_->GetCurrentDiagnosis();
       if (diag.confidence > 0.70f) {
           ApplyAutoPatchTuning(diag);
       }
   }
   ```
*/

}  // namespace Inference
}  // namespace RawrXD
