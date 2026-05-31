// cpu_inference_measurement_integration.h
// Integrates corrected measurement & pattern recognition into CPUInferenceEngine
// Phase-aware: Explicit scheduler phase markers for decode isolation

#pragma once

#include "speculative/rawr_benchmark_measurement_corrected.h"
#include "speculative/rawr_autopatch_realtime_recognizer.h"
#include <chrono>
#include <deque>
#include <memory>
#include <functional>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <string>

namespace RawrXD {
namespace Inference {

// ============================================================================
// EXPLICIT INFERENCE PHASES (Authoritative scheduler signals)
// ============================================================================
// Replaces heuristic token counting with explicit phase transitions.
// Measurement validity depends on accurate phase reporting from scheduler.
//
// Phase Transition Diagram:
//   IDLE → PREFILL → FIRST_TOKEN → STEADY_DECODE → TAIL → COMPLETE
//          ↑___________↓ (fallback if pipeline unstable)
//
enum class InferencePhase {
    IDLE,           // No active generation
    PREFILL,        // Processing input context (prompt tokens)
    FIRST_TOKEN,    // Generating token 0 (includes pipeline warmup)
    STEADY_DECODE,  // Stable token generation (measurement valid)
    TAIL,           // Final tokens / EOS approaching
    COMPLETE        // Generation finished
};

// Phase metadata for diagnostics
struct PhaseMetadata {
    InferencePhase phase;
    const char* name;
    bool is_measurement_valid;  // TPS measurements valid in this phase?
};

inline const char* PhaseName(InferencePhase phase) {
    switch (phase) {
        case InferencePhase::IDLE: return "IDLE";
        case InferencePhase::PREFILL: return "PREFILL";
        case InferencePhase::FIRST_TOKEN: return "FIRST_TOKEN";
        case InferencePhase::STEADY_DECODE: return "STEADY_DECODE";
        case InferencePhase::TAIL: return "TAIL";
        case InferencePhase::COMPLETE: return "COMPLETE";
        default: return "UNKNOWN";
    }
}

inline bool IsMeasurementPhase(InferencePhase phase) {
    return phase == InferencePhase::STEADY_DECODE;
}

using clock_t = std::chrono::high_resolution_clock;
using TelemetryWindow = RawrXD::Autopatch::TelemetryWindow;
using PatternRecognizer = RawrXD::Autopatch::RealtimePatternRecognizer;
using TelemetrySnapshot = RawrXD::Autopatch::TelemetrySnapshot;
using PerfPattern = RawrXD::Autopatch::PerfPattern;
using Diagnosis = RawrXD::Autopatch::Diagnosis;
using PatternDiagnosticEngine = RawrXD::Autopatch::PatternDiagnosticEngine;
using CorrectMeasurement = RawrXD::Benchmark::CorrectMeasurement;

struct AutopatchGateDecision {
    bool allow = false;
    std::string reason;
    double rolling_tps = 0.0;
    double tps_stddev = 0.0;
    size_t sample_count = 0;
};

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
    clock_t::time_point last_token_time_;
    clock_t::time_point decode_phase_start_;
    int tokens_generated_ = 0;
    bool first_token_emitted_ = false;
    bool decode_phase_stable_ = false;
    bool monotonic_timestamps_ok_ = true;
    bool grouped_emission_observed_ = false;
    uint64_t last_token_timestamp_ns_ = 0;

    std::vector<uint64_t> token_timestamps_ns_;
    std::deque<std::chrono::nanoseconds> decode_token_latencies_;
    std::deque<double> decode_tps_samples_;

    // Autopatch gating defaults: avoid overreacting to single noisy samples.
    size_t min_sample_window_ = 16;
    double max_tps_stddev_ = 20.0;
    int stable_decode_min_token_ = 3;

    static double ComputeStdDev(const std::deque<double>& values, double mean) {
        if (values.size() < 2) {
            return 0.0;
        }
        double sq = 0.0;
        for (double v : values) {
            const double d = v - mean;
            sq += d * d;
        }
        return std::sqrt(sq / static_cast<double>(values.size()));
    }

    static uint64_t ToEpochNs(clock_t::time_point tp) {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(tp.time_since_epoch()).count()
        );
    }
    
public:
    MeasurementCollector()
        : session_start_(clock_t::now()),
          first_token_time_(session_start_),
          last_token_time_(session_start_),
          decode_phase_start_(session_start_) {}
    
    // Call this BEFORE generating each token
    void TokenGenerationStart() {}
    
    // Call this AFTER each token is generated
    void TokenGenerationEnd(
        int token_id,
        double current_bandwidth_gbps,
        double current_cache_hit_rate,
        int prefetch_depth,
        float memory_pressure_pct,
        int tier_current,
        bool pipeline_stable_hint = false,
        bool grouped_emission_tagged = false
    ) {
        (void)token_id;
        auto now = clock_t::now();
        const uint64_t now_ns = ToEpochNs(now);
        token_timestamps_ns_.push_back(now_ns);

        if (last_token_timestamp_ns_ != 0 && now_ns <= last_token_timestamp_ns_) {
            monotonic_timestamps_ok_ = false;
            if (!grouped_emission_tagged) {
                grouped_emission_observed_ = true;
            }
        }
        last_token_timestamp_ns_ = now_ns;
        
        // First token special handling
        if (!first_token_emitted_) {
            first_token_time_ = now;
            last_token_time_ = now;
            first_token_emitted_ = true;
            tokens_generated_ = 1;
            return;
        }
        
        // Subsequent tokens
        tokens_generated_++;
        auto token_latency = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_token_time_);
        last_token_time_ = now;

        if (token_latency.count() <= 0 && !grouped_emission_tagged) {
            grouped_emission_observed_ = true;
        }

        // Decode phase only starts after first token AND stable pipeline.
        if (!decode_phase_stable_) {
            if (pipeline_stable_hint || tokens_generated_ >= stable_decode_min_token_) {
                decode_phase_stable_ = true;
                decode_phase_start_ = now;
            }
        }

        if (!decode_phase_stable_) {
            return;
        }
        
        // Calculate instantaneous TPS
        double instantaneous_tps = 0.0;
        if (token_latency.count() > 0) {
            instantaneous_tps = 1e9 / static_cast<double>(token_latency.count());
        }
        decode_token_latencies_.push_back(token_latency);
        decode_tps_samples_.push_back(instantaneous_tps);

        constexpr size_t kMaxRollingSamples = 256;
        if (decode_token_latencies_.size() > kMaxRollingSamples) {
            decode_token_latencies_.pop_front();
        }
        if (decode_tps_samples_.size() > kMaxRollingSamples) {
            decode_tps_samples_.pop_front();
        }
        
        // Build telemetry snapshot
        TelemetrySnapshot snap;
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
        pattern_recognizer_.AddTelemetry(snap);
        
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
        const auto end_time = clock_t::now();
        if (first_token_emitted_) {
            m.time_to_first_token = std::chrono::duration_cast<std::chrono::milliseconds>(
                first_token_time_ - session_start_
            );
        }

        if (!decode_token_latencies_.empty()) {
            auto total_decode_ns = std::accumulate(
                decode_token_latencies_.begin(),
                decode_token_latencies_.end(),
                std::chrono::nanoseconds(0)
            );
            m.time_per_token_avg = std::chrono::duration_cast<std::chrono::milliseconds>(
                total_decode_ns / decode_token_latencies_.size()
            );
        } else {
            m.time_per_token_avg = std::chrono::milliseconds(0);
        }
        m.tokens_generated_real = tokens_generated_;

        m.total_generation_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - session_start_
        );

        m.decode_phase_stable = decode_phase_stable_;
        if (decode_phase_stable_) {
            m.decode_time_stable_only = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - decode_phase_start_
            );
        }

        m.per_token_timestamps_ns = token_timestamps_ns_;
        m.monotonic_token_timestamps = monotonic_timestamps_ok_;
        m.grouped_emission_observed = grouped_emission_observed_;

        if (!decode_tps_samples_.empty()) {
            const double sum = std::accumulate(decode_tps_samples_.begin(), decode_tps_samples_.end(), 0.0);
            m.decode_sample_count = decode_tps_samples_.size();
            m.rolling_decode_tps = sum / static_cast<double>(decode_tps_samples_.size());
            m.decode_tps_stddev = ComputeStdDev(decode_tps_samples_, m.rolling_decode_tps);
        }
        
        return m;
    }

    AutopatchGateDecision EvaluateAutopatchGate() const {
        AutopatchGateDecision gate;
        gate.sample_count = decode_tps_samples_.size();

        if (!monotonic_timestamps_ok_ || grouped_emission_observed_) {
            gate.allow = false;
            gate.reason = "timestamp_integrity_failed";
            return gate;
        }

        if (decode_tps_samples_.size() < min_sample_window_) {
            gate.allow = false;
            gate.reason = "insufficient_samples";
            return gate;
        }

        const double sum = std::accumulate(decode_tps_samples_.begin(), decode_tps_samples_.end(), 0.0);
        gate.rolling_tps = sum / static_cast<double>(decode_tps_samples_.size());
        gate.tps_stddev = ComputeStdDev(decode_tps_samples_, gate.rolling_tps);

        if (gate.rolling_tps <= 0.0) {
            gate.allow = false;
            gate.reason = "non_positive_tps";
            return gate;
        }

        if (gate.tps_stddev > max_tps_stddev_) {
            gate.allow = false;
            gate.reason = "variance_too_high";
            return gate;
        }

        gate.allow = true;
        gate.reason = "stable_window";
        return gate;
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

    void ConfigureAutopatchGate(size_t min_sample_window, double max_tps_stddev, int stable_decode_min_token = 3) {
        min_sample_window_ = std::max<size_t>(2, min_sample_window);
        max_tps_stddev_ = std::max(0.0, max_tps_stddev);
        stable_decode_min_token_ = std::max(2, stable_decode_min_token);
    }
};

// ============================================================================
// PHASE-AWARE MEASUREMENT COLLECTOR (Production-grade)
// ============================================================================
// Replaces heuristic token counting with explicit scheduler phase signals.
// Key improvements:
//   - Authoritative phase transitions from execution_scheduler
//   - Phase-validated TPS (only STEADY_DECODE contributes to throughput)
//   - Cross-metric correlation (TPS drop ↔ KV miss, stall ↔ GPU latency)
//   - Hysteresis for pattern detection (sustained trends, not spikes)

class PhaseAwareMeasurementCollector {
public:
    struct CrossMetricSnapshot {
        double tps = 0.0;
        double kv_miss_rate = 0.0;
        double gpu_queue_latency_ms = 0.0;
        double aperture_flush_rate = 0.0;
        float memory_pressure_pct = 0.0f;
        int tier_current = 0;
        InferencePhase phase = InferencePhase::IDLE;
    };

    struct TrendAnalysis {
        double slope = 0.0;           // Linear regression slope
        double r_squared = 0.0;       // Fit quality
        bool sustained = false;       // Sustained for N windows?
        size_t windows_sustained = 0;
    };

private:
    // Core measurement state
    MeasurementCollector base_collector_;
    InferencePhase current_phase_ = InferencePhase::IDLE;
    InferencePhase previous_phase_ = InferencePhase::IDLE;
    
    // Phase timing
    clock_t::time_point phase_entered_time_;
    std::unordered_map<InferencePhase, std::chrono::milliseconds> phase_durations_;
    
    // Cross-metric correlation (for diagnostic targeting)
    std::deque<CrossMetricSnapshot> cross_metric_history_;
    static constexpr size_t kMaxCrossMetricHistory = 128;
    
    // Trend analysis for pattern detection
    std::deque<double> tps_window_samples_;
    static constexpr size_t kTrendWindowSize = 32;
    static constexpr size_t kSustainedThreshold = 3;  // Windows before "sustained"
    TrendAnalysis current_trend_;
    
    // Hysteresis state
    bool in_degradation_ = false;
    size_t degradation_windows_ = 0;
    
    // Configuration
    size_t min_stable_tokens_ = 16;      // Tokens before STEADY_DECODE valid
    double degradation_slope_threshold_ = -2.0;  // TPS/sec decline rate
    double stall_gpu_latency_threshold_ms_ = 32.0;

public:
    PhaseAwareMeasurementCollector() {
        phase_entered_time_ = clock_t::now();
    }

    // =========================================================================
    // PHASE TRANSITION API (Called by execution_scheduler)
    // =========================================================================
    // These methods provide AUTHORITATIVE phase signals, replacing heuristics.
    
    void SetPhase(InferencePhase new_phase) {
        if (new_phase == current_phase_) return;
        
        // Record duration of previous phase
        auto now = clock_t::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - phase_entered_time_
        );
        phase_durations_[current_phase_] += duration;
        
        previous_phase_ = current_phase_;
        current_phase_ = new_phase;
        phase_entered_time_ = now;
        
        // Phase-specific handling
        switch (new_phase) {
            case InferencePhase::STEADY_DECODE:
                OnEnterSteadyDecode();
                break;
            case InferencePhase::COMPLETE:
            case InferencePhase::IDLE:
                OnPhaseEnd();
                break;
            default:
                break;
        }
    }
    
    InferencePhase GetPhase() const { return current_phase_; }
    
    bool IsMeasurementPhase() const {
        return current_phase_ == InferencePhase::STEADY_DECODE;
    }

    // =========================================================================
    // TOKEN GENERATION (Phase-validated)
    // =========================================================================
    
    void OnTokenGenerated(
        int token_id,
        double bandwidth_gbps,
        double cache_hit_rate,
        int prefetch_depth,
        float memory_pressure_pct,
        int tier_current,
        double kv_miss_rate = 0.0,
        double gpu_queue_latency_ms = 0.0,
        double aperture_flush_rate = 0.0
    ) {
        // Only record cross-metrics in measurement phases
        if (IsMeasurementPhase()) {
            CrossMetricSnapshot snap;
            snap.tps = base_collector_.EvaluateAutopatchGate().rolling_tps;
            snap.kv_miss_rate = kv_miss_rate;
            snap.gpu_queue_latency_ms = gpu_queue_latency_ms;
            snap.aperture_flush_rate = aperture_flush_rate;
            snap.memory_pressure_pct = memory_pressure_pct;
            snap.tier_current = tier_current;
            snap.phase = current_phase_;
            
            cross_metric_history_.push_back(snap);
            if (cross_metric_history_.size() > kMaxCrossMetricHistory) {
                cross_metric_history_.pop_front();
            }
            
            // Update trend analysis
            UpdateTrendAnalysis();
        }
        
        // Always pass to base collector (it handles its own gating)
        base_collector_.TokenGenerationEnd(
            token_id, bandwidth_gbps, cache_hit_rate,
            prefetch_depth, memory_pressure_pct, tier_current,
            IsMeasurementPhase(),  // pipeline_stable_hint
            false
        );
    }

    // =========================================================================
    // AUTOPATCH GATING (Phase-aware + Cross-metric correlation)
    // =========================================================================
    
    struct PhaseAwareGateDecision {
        bool allow = false;
        std::string reason;
        double rolling_tps = 0.0;
        double tps_stddev = 0.0;
        size_t sample_count = 0;
        InferencePhase current_phase = InferencePhase::IDLE;
        
        // Cross-metric diagnostics (populated when blocked)
        bool kv_pressure_suspected = false;
        bool gpu_stall_suspected = false;
        bool aperture_thrash_suspected = false;
        double correlated_kv_miss_rate = 0.0;
        double correlated_gpu_latency_ms = 0.0;
    };
    
    PhaseAwareGateDecision EvaluatePhaseAwareGate() const {
        PhaseAwareGateDecision decision;
        decision.current_phase = current_phase_;
        
        // Phase gate: Only allow in STEADY_DECODE
        if (!IsMeasurementPhase()) {
            decision.reason = std::string("not_in_measurement_phase: ") + PhaseName(current_phase_);
            return decision;
        }
        
        // Base gate evaluation
        auto base_gate = base_collector_.EvaluateAutopatchGate();
        decision.allow = base_gate.allow;
        decision.reason = base_gate.reason;
        decision.rolling_tps = base_gate.rolling_tps;
        decision.tps_stddev = base_gate.tps_stddev;
        decision.sample_count = base_gate.sample_count;
        
        if (!decision.allow) {
            // Populate cross-metric diagnostics
            if (!cross_metric_history_.empty()) {
                const auto& latest = cross_metric_history_.back();
                decision.correlated_kv_miss_rate = latest.kv_miss_rate;
                decision.correlated_gpu_latency_ms = latest.gpu_queue_latency_ms;
                
                decision.kv_pressure_suspected = (latest.kv_miss_rate > 0.1);
                decision.gpu_stall_suspected = (latest.gpu_queue_latency_ms > stall_gpu_latency_threshold_ms_);
                decision.aperture_thrash_suspected = (latest.aperture_flush_rate > 100.0);  // >100 flushes/sec
            }
        }
        
        return decision;
    }

    // =========================================================================
    // PATTERN RECOGNITION (Trend-aware)
    // =========================================================================
    
    enum class TrendPattern {
        NOMINAL,
        DEGRADATION_TREND,      // Sustained TPS decline
        THERMAL_THROTTLING,     // Gradual slowdown
        MEMORY_PRESSURE_RAMP,   // Increasing pressure
        PREFETCH_INEFFICIENCY,  // Cache misses increasing
        STALL_DETECTED          // GPU/scheduler stall
    };
    
    struct TrendDiagnosis {
        TrendPattern pattern = TrendPattern::NOMINAL;
        double confidence = 0.0;
        std::string evidence;
        std::vector<std::string> correlated_metrics;
    };
    
    TrendDiagnosis AnalyzeTrend() const {
        TrendDiagnosis diag;
        
        if (current_trend_.sustained && current_trend_.slope < degradation_slope_threshold_) {
            diag.pattern = TrendPattern::DEGRADATION_TREND;
            diag.confidence = std::min(1.0, std::abs(current_trend_.r_squared));
            diag.evidence = "Sustained TPS decline slope: " + std::to_string(current_trend_.slope);
            
            // Correlate with other metrics
            if (!cross_metric_history_.empty()) {
                const auto& recent = cross_metric_history_.back();
                if (recent.kv_miss_rate > 0.05) {
                    diag.correlated_metrics.push_back("kv_miss_rate=" + std::to_string(recent.kv_miss_rate));
                }
                if (recent.gpu_queue_latency_ms > 20.0) {
                    diag.correlated_metrics.push_back("gpu_latency_ms=" + std::to_string(recent.gpu_queue_latency_ms));
                }
                if (recent.aperture_flush_rate > 50.0) {
                    diag.correlated_metrics.push_back("aperture_flush_rate=" + std::to_string(recent.aperture_flush_rate));
                }
            }
        }
        
        return diag;
    }

    // =========================================================================
    // MEASUREMENT RESULTS
    // =========================================================================
    
    CorrectMeasurement GetFinalMeasurement() {
        auto m = base_collector_.GetFinalMeasurement();
        
        // Override with phase-aware timing if available
        auto it = phase_durations_.find(InferencePhase::STEADY_DECODE);
        if (it != phase_durations_.end() && it->second.count() > 0) {
            m.decode_time_stable_only = it->second;
            m.decode_phase_stable = true;
        }
        
        return m;
    }
    
    std::chrono::milliseconds GetPhaseDuration(InferencePhase phase) const {
        auto it = phase_durations_.find(phase);
        if (it != phase_durations_.end()) {
            return it->second;
        }
        return std::chrono::milliseconds(0);
    }

private:
    void OnEnterSteadyDecode() {
        // Reset trend analysis for new decode phase
        tps_window_samples_.clear();
        current_trend_ = TrendAnalysis{};
        in_degradation_ = false;
        degradation_windows_ = 0;
    }
    
    void OnPhaseEnd() {
        // Final trend analysis
        if (!tps_window_samples_.empty()) {
            UpdateTrendAnalysis();
        }
    }
    
    void UpdateTrendAnalysis() {
        if (tps_window_samples_.size() < kTrendWindowSize) {
            return;
        }
        
        // Simple linear regression on TPS samples
        double n = static_cast<double>(tps_window_samples_.size());
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        
        for (size_t i = 0; i < tps_window_samples_.size(); ++i) {
            double x = static_cast<double>(i);
            double y = tps_window_samples_[i];
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_x2 += x * x;
        }
        
        double denom = (n * sum_x2 - sum_x * sum_x);
        if (std::abs(denom) > 1e-10) {
            current_trend_.slope = (n * sum_xy - sum_x * sum_y) / denom;
            
            // R-squared calculation
            double y_mean = sum_y / n;
            double ss_tot = 0.0, ss_res = 0.0;
            for (size_t i = 0; i < tps_window_samples_.size(); ++i) {
                double x = static_cast<double>(i);
                double y = tps_window_samples_[i];
                double y_pred = current_trend_.slope * x + (sum_y - current_trend_.slope * sum_x) / n;
                ss_tot += (y - y_mean) * (y - y_mean);
                ss_res += (y - y_pred) * (y - y_pred);
            }
            current_trend_.r_squared = (ss_tot > 1e-10) ? 1.0 - (ss_res / ss_tot) : 0.0;
        }
        
        // Update sustained flag
        if (current_trend_.slope < degradation_slope_threshold_) {
            degradation_windows_++;
            if (degradation_windows_ >= kSustainedThreshold) {
                current_trend_.sustained = true;
                current_trend_.windows_sustained = degradation_windows_;
            }
        } else {
            degradation_windows_ = 0;
            current_trend_.sustained = false;
        }
    }
};

}  // namespace Inference
}  // namespace RawrXD
