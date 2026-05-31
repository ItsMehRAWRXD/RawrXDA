// predictive_scheduler.h - Predictive scheduling for completions before user stops typing
// Implements completions that start before user stops typing
//
// Architecture:
//   - Predict when user will stop typing
//   - Start completions before they stop
//   - Have results ready when they pause
//
// Key insight:
//   - Not: Wait for user to stop, then start
//   - But: Predict stop, start early, have results ready
//
// This is where we surpass Copilot.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include "adaptive_debounce.h"
#include "token_prefetch.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD {

// Typing pattern
struct TypingPattern {
    std::chrono::milliseconds avg_interval;
    std::chrono::milliseconds std_deviation;
    float pause_probability;     // Probability of pausing after this token
    int burst_length;            // Average burst length
    int pause_length;            // Average pause length
};

// Predicted stop
struct PredictedStop {
    std::chrono::steady_clock::time_point predicted_time;
    float confidence;            // Confidence in prediction
    std::string reason;          // Why we think they'll stop
    int tokens_ahead;            // How many tokens ahead to predict
};

// Predictive scheduler statistics
struct PredictiveStats {
    int total_predictions;
    int correct_predictions;     // User stopped when predicted
    int early_predictions;       // Predicted too early
    int late_predictions;        // Predicted too late
    float accuracy;              // Correct / total
    std::chrono::microseconds avg_prediction_latency;
    std::chrono::microseconds avg_early_amount;
    std::chrono::microseconds avg_late_amount;
};

// Predictive scheduler
class PredictiveScheduler {
public:
    PredictiveScheduler();
    ~PredictiveScheduler();
    
    // Configure scheduler
    struct Config {
        std::chrono::milliseconds prediction_window{200};  // How far ahead to predict
        float confidence_threshold = 0.7f;                  // Min confidence to act
        int min_typing_history = 5;                        // Min keystrokes before prediction
        bool enable_burst_detection = true;              // Detect typing bursts
        bool enable_pause_prediction = true;               // Predict pauses
        bool enable_early_start = true;                    // Start before predicted stop
        std::chrono::milliseconds early_start_offset{50};  // Start this early
    };
    void SetConfig(const Config& config);
    
    // Record keystroke for pattern learning
    void RecordKeystroke();
    
    // Predict when user will stop typing
    PredictedStop PredictStop();
    
    // Check if we should start completion now
    bool ShouldStartCompletion();
    
    // Get predicted typing pattern
    TypingPattern GetPattern() const;
    
    // Start predictive completion
    void StartPredictiveCompletion(
        const std::string& context,
        std::function<void(const std::string&)> callback
    );
    
    // Cancel predictive completion
    void CancelPredictiveCompletion();
    
    // Get statistics
    PredictiveStats GetStats() const;
    
    // Reset statistics
    void ResetStats();
    
private:
    // Analyze typing pattern
    TypingPattern AnalyzePattern() const;
    
    // Detect typing burst
    bool IsInBurst() const;
    
    // Detect pause
    bool IsPausing() const;
    
    // Calculate prediction confidence
    float CalculateConfidence(const TypingPattern& pattern) const;
    
    // Members
    Config config_;
    
    // Typing history
    std::vector<std::chrono::steady_clock::time_point> keystroke_history_;
    mutable std::mutex history_mutex_;
    
    // Current pattern
    TypingPattern current_pattern_;
    mutable std::mutex pattern_mutex_;
    
    // Prediction state
    std::atomic<bool> predicting_{false};
    std::atomic<bool> completion_started_{false};
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PredictiveStats stats_;
};

// Inline implementations

inline PredictiveStats PredictiveScheduler::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline void PredictiveScheduler::ResetStats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
}

} // namespace RawrXD