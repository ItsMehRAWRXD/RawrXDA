// ============================================================================
// moe_telemetry.h - Flight Recorder for MoE Inference
// Zero dependencies. C++17.
// Tracks: acceptance rate, latency, expert performance, ROI
// ============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <array>
#include <vector>
#include <atomic>
#include <chrono>

namespace moe {

// ----------------------------------------------------------------------------
// Diagnostic Frame - Single inference burst metrics
// ----------------------------------------------------------------------------
struct DiagnosticFrame {
    float acceptance_rate;      // α: percentage of draft tokens kept (0.0-1.0)
    int tokens_produced;        // Total tokens generated
    int draft_tokens_accepted;  // How many draft tokens were kept
    int draft_tokens_rejected;  // How many were rejected
    double draft_latency_ms;    // Time in draft model (7800X3D L3 cache)
    double verify_latency_ms;   // Time in target verification
    double total_ms;            // End-to-end latency
    int expert_id;              // Which parent expert was used (-1 if none)
    int draft_model_id;         // Which draft model was used
    float expert_confidence;    // Confidence score at dispatch time
    uint64_t timestamp_ns;      // High-res timestamp
    bool cache_hit;             // Was this a prefix cache hit?
    
    // ROI calculation: (tokens_produced / total_ms) vs baseline
    float speedup_vs_baseline;  // 1.0 = no gain, 2.0 = 2x faster, 0.5 = 2x slower
    float roi_score;            // (speedup_vs_baseline - 1.0) * 100, negative = tax
    
    void compute_roi(float baseline_tps) {
        float actual_tps = tokens_produced / (total_ms / 1000.0);
        speedup_vs_baseline = (baseline_tps > 0) ? (actual_tps / baseline_tps) : 1.0f;
        roi_score = (speedup_vs_baseline - 1.0f) * 100.0f;
    }
};

// ----------------------------------------------------------------------------
// Expert Performance Tracker - Running statistics per expert
// ----------------------------------------------------------------------------
struct ExpertMetrics {
    std::atomic<int> total_calls{0};
    std::atomic<int> accepted_completions{0};
    std::atomic<int> rejected_completions{0};
    std::atomic<float> cumulative_acceptance_rate{0.0f};  // EMA
    std::atomic<double> total_latency_ms{0.0};
    std::atomic<double> avg_latency_ms{0.0};
    std::atomic<float> current_score{0.5f};  // 0.0-1.0, starts neutral
    
    // Task category specialization scores
    std::array<float, 6> category_scores{};  // CODE, BUG_FIX, REFACTOR, DOC, TEST, UNKNOWN
    
    ExpertMetrics() {
        category_scores.fill(0.5f);
    }
    
    void record_completion(bool accepted, float acceptance_rate, double latency_ms) {
        total_calls++;
        if (accepted) accepted_completions++; else rejected_completions++;
        
        // Update EMA of acceptance rate (alpha = 0.1)
        float old_rate = cumulative_acceptance_rate.load(std::memory_order_relaxed);
        float new_rate = old_rate * 0.9f + acceptance_rate * 0.1f;
        cumulative_acceptance_rate.store(new_rate, std::memory_order_relaxed);
        
        // Update latency EMA
        double old_lat = avg_latency_ms.load(std::memory_order_relaxed);
        double new_lat = old_lat * 0.9 + latency_ms * 0.1;
        avg_latency_ms.store(new_lat, std::memory_order_relaxed);
        total_latency_ms += latency_ms;
        
        // Update overall score
        float old_score = current_score.load(std::memory_order_relaxed);
        float target = accepted ? 1.0f : 0.0f;
        float new_score = old_score * 0.95f + target * 0.05f;
        current_score.store(new_score, std::memory_order_relaxed);
    }
    
    void record_category_score(int category, bool success) {
        if (category < 0 || category >= 6) return;
        float old = category_scores[category];
        float target = success ? 1.0f : 0.0f;
        category_scores[category] = old * 0.9f + target * 0.1f;
    }
};

// ----------------------------------------------------------------------------
// Telemetry Ring Buffer - Lock-free for hot path
// ----------------------------------------------------------------------------
template<size_t N = 1024>
class TelemetryRingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    
    std::array<DiagnosticFrame, N> buffer{};
    std::atomic<size_t> write_idx{0};
    std::atomic<size_t> read_idx{0};
    
public:
    static constexpr size_t MASK = N - 1;
    
    bool push(const DiagnosticFrame& frame) {
        size_t idx = write_idx.load(std::memory_order_relaxed);
        size_t next = (idx + 1) & MASK;
        
        if (next == read_idx.load(std::memory_order_acquire)) {
            return false;  // Full
        }
        
        buffer[idx] = frame;
        write_idx.store(next, std::memory_order_release);
        return true;
    }
    
    bool pop(DiagnosticFrame& frame) {
        size_t idx = read_idx.load(std::memory_order_relaxed);
        
        if (idx == write_idx.load(std::memory_order_acquire)) {
            return false;  // Empty
        }
        
        frame = buffer[idx];
        read_idx.store((idx + 1) & MASK, std::memory_order_release);
        return true;
    }
    
    size_t size() const {
        return (write_idx.load(std::memory_order_acquire) - 
                read_idx.load(std::memory_order_acquire)) & MASK;
    }
    
    void clear() {
        read_idx.store(write_idx.load(std::memory_order_relaxed), 
                      std::memory_order_release);
    }
};

// ----------------------------------------------------------------------------
// Flight Recorder - Main telemetry system
// ----------------------------------------------------------------------------
class FlightRecorder {
public:
    static constexpr int MAX_EXPERTS = 32;
    static constexpr float BASELINE_TPS = 100.0f;  // Configurable
    
    TelemetryRingBuffer<4096> frame_buffer;
    std::array<ExpertMetrics, MAX_EXPERTS> expert_metrics;
    std::atomic<float> global_acceptance_rate{0.0f};
    std::atomic<double> total_inference_time_ms{0.0};
    std::atomic<int> total_tokens_generated{0};
    
    // Session statistics
    struct SessionStats {
        uint64_t start_time_ns;
        int frames_recorded;
        float avg_roi;
        float best_speedup;
        float worst_speedup;
    };
    SessionStats session{};
    
    FlightRecorder() {
        reset_session();
    }
    
    void reset_session() {
        session.start_time_ns = now_ns();
        session.frames_recorded = 0;
        session.avg_roi = 0.0f;
        session.best_speedup = 0.0f;
        session.worst_speedup = 999.0f;
        frame_buffer.clear();
    }
    
    // Call this after every inference burst
    void record_frame(const DiagnosticFrame& frame) {
        DiagnosticFrame mutable_frame = frame;
        mutable_frame.compute_roi(BASELINE_TPS);
        mutable_frame.timestamp_ns = now_ns();
        
        frame_buffer.push(mutable_frame);
        
        // Update expert metrics
        if (frame.expert_id >= 0 && frame.expert_id < MAX_EXPERTS) {
            bool accepted = frame.acceptance_rate > 0.5f;
            expert_metrics[frame.expert_id].record_completion(
                accepted, frame.acceptance_rate, frame.total_ms
            );
        }
        
        // Update global stats
        global_acceptance_rate = global_acceptance_rate * 0.99f + 
                                 mutable_frame.acceptance_rate * 0.01f;
        total_inference_time_ms += mutable_frame.total_ms;
        total_tokens_generated += mutable_frame.tokens_produced;
        
        // Update session stats
        session.frames_recorded++;
        session.avg_roi = session.avg_roi * 0.99f + mutable_frame.roi_score * 0.01f;
        session.best_speedup = std::max(session.best_speedup, mutable_frame.speedup_vs_baseline);
        session.worst_speedup = std::min(session.worst_speedup, mutable_frame.speedup_vs_baseline);
    }
    
    // Get current ROI color for UI
    enum class ROIColor { RED, YELLOW, GREEN };
    ROIColor get_roi_color(float roi) const {
        if (roi < 0.0f) return ROIColor::RED;      // Speculative tax
        if (roi < 50.0f) return ROIColor::YELLOW;  // Marginal gain
        return ROIColor::GREEN;                    // Good dividend
    }
    
    // Export to JSON for analysis
    std::string export_json() const;
    
    // Get summary for status bar
    std::string get_status_summary() const;
    
private:
    static uint64_t now_ns() {
        using namespace std::chrono;
        return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
    }
};

// Global instance
extern FlightRecorder g_flight_recorder;

} // namespace moe
