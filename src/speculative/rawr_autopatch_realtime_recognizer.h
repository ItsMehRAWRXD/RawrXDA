// rawr_autopatch_realtime_recognizer.h
// Stage 2: Real-Time Pattern Recognition Layer (missing from current autopatch)
// Converts raw telemetry → semantic performance patterns → actionable decisions

#pragma once

#include <chrono>
#include <deque>
#include <cmath>
#include <algorithm>

namespace RawrXD {
namespace Autopatch {

// ============================================================================
// STAGE 2: PATTERN RECOGNITION (Converting Telemetry → Meaning)
// ============================================================================

struct PerfPattern {
    // ===== DETECTION FLAGS (what's actually happening) =====
    bool tps_degrading = false;              // Token throughput dropping
    bool bandwidth_saturated = false;        // PCIe/memory bus at ceiling
    bool cache_thrashing = false;            // High cache miss rates
    bool under_prefetching = false;          // Prefetch depth too low
    bool over_prefetching = false;           // Prefetch depth too high (causing eviction)
    bool memory_pressure_high = false;       // RAM usage > 85%
    bool compute_stalled = false;            // GPU/CPU waiting for data
    bool expert_reuse_poor = false;          // MoE cache inefficiency
    bool aperture_fragmented = false;        // Coherence/allocation fragmentation
    
    // ===== CONFIDENCE METRICS (how sure are we?) =====
    float confidence_tps_degrading = 0.0f;
    float confidence_cache_thrash = 0.0f;
    float confidence_memory_pressure = 0.0f;
    
    // ===== SEVERITY (how urgent) =====
    float severity = 0.0f;  // 0.0 (nominal) to 1.0 (critical)
};

// ============================================================================
// TELEMETRY WINDOW (rolling buffer for pattern detection)
// ============================================================================

struct TelemetrySnapshot {
    std::chrono::high_resolution_clock::time_point timestamp;
    double tps;                      // Current tokens/sec (REAL, not synthetic)
    double bandwidth_gbps;           // Memory throughput
    double cache_hit_rate;           // 0.0-1.0
    int prefetch_depth;              // Current lookahead depth
    float memory_pressure_percent;   // 0-100
    double latency_per_token_us;    // Microseconds
    int tier_current;                // 0=NORMAL, 1=WARNING, 2=CRITICAL
    bool is_first_token;             // True if this is TTFT
};

class TelemetryWindow {
private:
    static constexpr size_t MAX_WINDOW = 100;  // Last 100 token generations
    std::deque<TelemetrySnapshot> window_;
    
public:
    void AddSnapshot(const TelemetrySnapshot& snap) {
        window_.push_back(snap);
        if (window_.size() > MAX_WINDOW) {
            window_.pop_front();
        }
    }
    
    const std::deque<TelemetrySnapshot>& GetWindow() const {
        return window_;
    }
    
    size_t Size() const { return window_.size(); }
    
    // Get statistics over window
    double AvgTPS() const {
        if (window_.empty()) return 0.0;
        double sum = 0.0;
        int count = 0;
        for (const auto& snap : window_) {
            if (!snap.is_first_token) {  // Exclude TTFT outliers
                sum += snap.tps;
                count++;
            }
        }
        return count > 0 ? sum / count : 0.0;
    }
    
    double TPSTrend() const {
        // Calculate slope: is TPS getting worse or better over recent tokens?
        if (window_.size() < 10) return 0.0;
        
        double recent_avg = 0.0, old_avg = 0.0;
        for (size_t i = 0; i < window_.size() / 2; i++) {
            old_avg += window_[i].tps;
        }
        old_avg /= (window_.size() / 2);
        
        for (size_t i = window_.size() / 2; i < window_.size(); i++) {
            recent_avg += window_[i].tps;
        }
        recent_avg /= (window_.size() - window_.size() / 2);
        
        return (recent_avg - old_avg) / std::max(1.0, old_avg);  // Normalized slope
    }
    
    double AvgCacheHitRate() const {
        if (window_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& snap : window_) {
            sum += snap.cache_hit_rate;
        }
        return sum / window_.size();
    }
    
    double BandwidthUtilization() const {
        if (window_.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& snap : window_) {
            sum += snap.bandwidth_gbps;
        }
        double avg = sum / window_.size();
        // PCIe 4.0 peak: ~31.5 GB/s, PCIe 5.0: ~63 GB/s
        return avg / 31.5;  // Normalize to PCIe 4.0
    }
};

// ============================================================================
// PATTERN RECOGNIZER (Cursor-style semantic analysis)
// ============================================================================

class RealtimePatternRecognizer {
private:
    TelemetryWindow telemetry_;
    static constexpr float THRESHOLD_TPS_DEGRADING = 0.10f;     // >10% drop
    static constexpr float THRESHOLD_BW_SAT = 0.85f;             // >85% PCIe util
    static constexpr float THRESHOLD_CACHE_THRASH = 0.40f;       // <40% hit rate
    static constexpr float THRESHOLD_MEMORY_PRESSURE = 85.0f;    // >85% RAM
    static constexpr float THRESHOLD_PREFETCH_UNDERUTILIZED = 60.0f;  // <60% BW util
    
public:
    void AddTelemetry(const TelemetrySnapshot& snap) {
        telemetry_.AddSnapshot(snap);
    }
    
    PerfPattern RecognizePatterns() {
        PerfPattern pattern;
        
        // Guard: need at least 10 snapshots for statistical confidence
        if (telemetry_.Size() < 10) {
            return pattern;  // Return empty pattern
        }
        
        // ===== PATTERN 1: TPS Degrading =====
        // Signal: Recent tokens slower than historical baseline
        double trend = telemetry_.TPSTrend();
        if (trend < -THRESHOLD_TPS_DEGRADING) {
            pattern.tps_degrading = true;
            pattern.confidence_tps_degrading = std::min(1.0f, std::abs(static_cast<float>(trend)) / 0.5f);
            pattern.severity = std::max(pattern.severity, pattern.confidence_tps_degrading);
        }
        
        // ===== PATTERN 2: Bandwidth Saturated =====
        // Signal: PCIe/memory bus running at peak capacity but TPS not keeping up
        double bw_util = telemetry_.BandwidthUtilization();
        if (bw_util > THRESHOLD_BW_SAT) {
            // Check if TPS still low (indicates compute bottleneck, not memory)
            double avg_tps = telemetry_.AvgTPS();
            if (avg_tps < 200.0) {  // 40B model should hit 100+ TPS if BW was the issue
                pattern.bandwidth_saturated = true;
                pattern.severity = std::max(pattern.severity, 0.7f);
            }
        }
        
        // ===== PATTERN 3: Cache Thrashing =====
        // Signal: Low cache hit rate (< 40%) AND high memory pressure
        double cache_hit = telemetry_.AvgCacheHitRate();
        if (cache_hit < THRESHOLD_CACHE_THRASH) {
            pattern.cache_thrashing = true;
            pattern.confidence_cache_thrash = std::min(1.0f, 
                (THRESHOLD_CACHE_THRASH - static_cast<float>(cache_hit)) / THRESHOLD_CACHE_THRASH);
            pattern.severity = std::max(pattern.severity, pattern.confidence_cache_thrash);
        }
        
        // ===== PATTERN 4: Under-Prefetching =====
        // Signal: Bandwidth < 60% utilized AND TPS is low
        if (bw_util < THRESHOLD_PREFETCH_UNDERUTILIZED / 100.0) {
            double avg_tps = telemetry_.AvgTPS();
            if (avg_tps < 150.0) {  // Should be higher if prefetch was sufficient
                pattern.under_prefetching = true;
                pattern.severity = std::max(pattern.severity, 0.6f);
            }
        }
        
        // ===== PATTERN 5: Over-Prefetching =====
        // Signal: High bandwidth but DEGRADING TPS (prefetch is creating eviction)
        if (bw_util > THRESHOLD_BW_SAT && trend < -0.05f) {
            pattern.over_prefetching = true;
            pattern.severity = std::max(pattern.severity, 0.65f);
        }
        
        // ===== PATTERN 6: Memory Pressure High =====
        // Signal: RAM usage edge-case (this would come from system metrics)
        const auto& window = telemetry_.GetWindow();
        if (!window.empty()) {
            float max_pressure = 0.0f;
            for (const auto& snap : window) {
                max_pressure = std::max(max_pressure, snap.memory_pressure_percent);
            }
            if (max_pressure > THRESHOLD_MEMORY_PRESSURE) {
                pattern.memory_pressure_high = true;
                pattern.confidence_memory_pressure = (max_pressure - 85.0f) / 15.0f;
                pattern.severity = std::max(pattern.severity, pattern.confidence_memory_pressure);
            }
        }
        
        // ===== PATTERN 7: Compute Stalled =====
        // Signal: TPS very low despite healthy bandwidth and cache (GPU waiting?)
        if (bw_util > 0.5f && cache_hit > THRESHOLD_CACHE_THRASH && 
            std::abs(trend) < 0.05f && telemetry_.AvgTPS() < 50.0) {
            pattern.compute_stalled = true;
            pattern.severity = std::max(pattern.severity, 0.85f);
        }
        
        // ===== PATTERN 8: Expert Reuse Poor (MoE-specific) =====
        // Signal: Tier oscillation or expert cache misses increasing trend
        if (!window.empty() && window.size() >= 5) {
            int tier_changes = 0;
            for (size_t i = 1; i < window.size(); i++) {
                if (window[i].tier_current != window[i-1].tier_current) {
                    tier_changes++;
                }
            }
            if (tier_changes > window.size() / 3) {  // >33% flipping tiers
                pattern.expert_reuse_poor = true;
                pattern.severity = std::max(pattern.severity, 0.6f);
            }
        }
        
        return pattern;
    }
};

// ============================================================================
// STAGE 2 OUTPUT: Structured diagnostic for Stage 3 (patch synthesis)
// ============================================================================

struct Diagnosis {
    PerfPattern pattern;
    std::string root_cause;           // Human-readable diagnosis
    std::vector<std::string> evidence; // Supporting metrics
    float confidence = 0.0f;           // Overall confidence in diagnosis
};

class PatternDiagnosticEngine {
public:
    static Diagnosis DiagnosePerformanceIssue(const PerfPattern& pattern) {
        Diagnosis diag;
        diag.pattern = pattern;
        diag.confidence = pattern.severity;
        
        // Rank patterns by severity to determine root cause
        if (pattern.compute_stalled) {
            diag.root_cause = "COMPUTE_STALLED: GPU/CPU waiting for data despite healthy memory";
            diag.evidence.push_back("Low TPS with healthy bandwidth");
            diag.evidence.push_back("Low cache miss rate");
        } else if (pattern.bandwidth_saturated && pattern.tps_degrading) {
            diag.root_cause = "MEMORY_BOTTLENECK: PCIe/DDR bandwidth at ceiling";
            diag.evidence.push_back(">85% PCIe utilization");
            diag.evidence.push_back("TPS degrading over recent tokens");
        } else if (pattern.over_prefetching) {
            diag.root_cause = "CACHE_EVICTION: Prefetch depth too aggressive, evicting active data";
            diag.evidence.push_back("Cache hit rate degrading");
            diag.evidence.push_back("Prefetch depth >2 causing thrash");
        } else if (pattern.cache_thrashing) {
            diag.root_cause = "CACHE_COHERENCE: High miss rate, memory row churn";
            diag.evidence.push_back("Cache hit rate < 40%");
            diag.evidence.push_back("Memory pressure rising");
        } else if (pattern.under_prefetching) {
            diag.root_cause = "UNDER_PREFETCH: Current depth insufficient for model size";
            diag.evidence.push_back("Bandwidth utilization < 60%");
            diag.evidence.push_back("TPS capacity not reached");
        } else if (pattern.expert_reuse_poor) {
            diag.root_cause = "MoE_EFFICIENCY: Expert cache misses, tier oscillation";
            diag.evidence.push_back("Tier flipping >33% of tokens");
            diag.evidence.push_back("Expert reuse window insufficient");
        } else if (pattern.memory_pressure_high) {
            diag.root_cause = "MEMORY_PRESSURE: RAM usage > 85%, allocation stalling";
            diag.evidence.push_back("Peak memory pressure detected");
        } else {
            diag.root_cause = "NOMINAL: No significant performance issues detected";
            diag.confidence = 0.1f;  // Low confidence in "broken" diagnosis when not broken
        }
        
        return diag;
    }
};

}  // namespace Autopatch
}  // namespace RawrXD
