// predictive_scheduler.cpp - Implementation of predictive scheduling
// Part of the Copilot-like inference pipeline.

#include "predictive_scheduler.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {

PredictiveScheduler::PredictiveScheduler() {
    stats_ = {};
}

PredictiveScheduler::~PredictiveScheduler() {
}

void PredictiveScheduler::SetConfig(const Config& config) {
    config_ = config;
}

void PredictiveScheduler::RecordKeystroke() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    keystroke_history_.push_back(now);
    
    // Keep only last N keystrokes
    while (keystroke_history_.size() > static_cast<size_t>(config_.min_typing_history * 2)) {
        keystroke_history_.erase(keystroke_history_.begin());
    }
    
    // Update pattern
    {
        std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
        current_pattern_ = AnalyzePattern();
    }
}

PredictedStop PredictiveScheduler::PredictStop() {
    PredictedStop prediction;
    prediction.confidence = 0.0f;
    prediction.tokens_ahead = 0;
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (keystroke_history_.size() < static_cast<size_t>(config_.min_typing_history)) {
        prediction.reason = "Not enough typing history";
        return prediction;
    }
    
    // Get current pattern
    TypingPattern pattern;
    {
        std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
        pattern = current_pattern_;
    }
    
    // Calculate prediction confidence
    float confidence = CalculateConfidence(pattern);
    
    if (confidence < config_.confidence_threshold) {
        prediction.reason = "Low confidence: " + std::to_string(confidence);
        return prediction;
    }
    
    // Predict stop time
    auto now = std::chrono::steady_clock::now();
    auto predicted_delay = pattern.avg_interval + 
                           std::chrono::milliseconds(static_cast<int>(
                               pattern.std_deviation.count() * 2));
    
    prediction.predicted_time = now + predicted_delay;
    prediction.confidence = confidence;
    prediction.reason = "Pattern-based prediction";
    prediction.tokens_ahead = static_cast<int>(
        config_.prediction_window.count() / pattern.avg_interval.count());
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_predictions++;
    }
    
    return prediction;
}

bool PredictiveScheduler::ShouldStartCompletion() {
    PredictedStop prediction = PredictStop();
    
    if (prediction.confidence < config_.confidence_threshold) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_until_stop = std::chrono::duration_cast<std::chrono::milliseconds>(
        prediction.predicted_time - now);
    
    // Start if we're within early_start_offset of predicted stop
    if (time_until_stop <= config_.early_start_offset) {
        return true;
    }
    
    return false;
}

TypingPattern PredictiveScheduler::GetPattern() const {
    std::lock_guard<std::mutex> lock(pattern_mutex_);
    return current_pattern_;
}

void PredictiveScheduler::StartPredictiveCompletion(
    const std::string& context,
    std::function<void(const std::string&)> callback
) {
    if (completion_started_.load()) {
        return;
    }
    
    completion_started_.store(true);
    
    // Start completion in background
    std::thread([this, context, callback]() {
        // TODO: Call actual completion
        // For now, just simulate
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (callback) {
            callback("Predicted completion");
        }
        
        completion_started_.store(false);
    }).detach();
}

void PredictiveScheduler::CancelPredictiveCompletion() {
    completion_started_.store(false);
}

TypingPattern PredictiveScheduler::AnalyzePattern() const {
    TypingPattern pattern;
    
    if (keystroke_history_.size() < 2) {
        pattern.avg_interval = std::chrono::milliseconds(150);
        pattern.std_deviation = std::chrono::milliseconds(50);
        pattern.pause_probability = 0.5f;
        pattern.burst_length = 5;
        pattern.pause_length = 3;
        return pattern;
    }
    
    // Calculate intervals
    std::vector<std::chrono::milliseconds> intervals;
    intervals.reserve(keystroke_history_.size() - 1);
    
    for (size_t i = 1; i < keystroke_history_.size(); i++) {
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
            keystroke_history_[i] - keystroke_history_[i - 1]);
        intervals.push_back(interval);
    }
    
    // Calculate average
    int64_t total = 0;
    for (auto interval : intervals) {
        total += interval.count();
    }
    pattern.avg_interval = std::chrono::milliseconds(
        static_cast<int>(total / intervals.size()));
    
    // Calculate standard deviation
    float variance = 0.0f;
    for (auto interval : intervals) {
        float diff = static_cast<float>(interval.count() - pattern.avg_interval.count());
        variance += diff * diff;
    }
    variance /= intervals.size();
    pattern.std_deviation = std::chrono::milliseconds(static_cast<int>(std::sqrt(variance)));
    
    // Detect bursts and pauses
    int burst_count = 0;
    int pause_count = 0;
    int current_burst = 0;
    int current_pause = 0;
    
    for (auto interval : intervals) {
        if (interval < pattern.avg_interval) {
            // Fast typing = burst
            current_burst++;
            if (current_pause > 0) {
                pause_count++;
                current_pause = 0;
            }
        } else {
            // Slow typing = pause
            current_pause++;
            if (current_burst > 0) {
                burst_count++;
                current_burst = 0;
            }
        }
    }
    
    pattern.burst_length = burst_count > 0 ? current_burst / burst_count : 5;
    pattern.pause_length = pause_count > 0 ? current_pause / pause_count : 3;
    
    // Calculate pause probability
    pattern.pause_probability = static_cast<float>(pause_count) / 
                               (burst_count + pause_count + 1);
    
    return pattern;
}

bool PredictiveScheduler::IsInBurst() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (keystroke_history_.size() < 2) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto last_interval = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - keystroke_history_.back());
    
    TypingPattern pattern;
    {
        std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
        pattern = current_pattern_;
    }
    
    return last_interval < pattern.avg_interval;
}

bool PredictiveScheduler::IsPausing() const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    if (keystroke_history_.size() < 2) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto last_interval = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - keystroke_history_.back());
    
    TypingPattern pattern;
    {
        std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
        pattern = current_pattern_;
    }
    
    return last_interval > pattern.avg_interval * 2;
}

float PredictiveScheduler::CalculateConfidence(const TypingPattern& pattern) const {
    // Confidence based on:
    // 1. Amount of history
    // 2. Consistency of pattern
    // 3. Recent behavior
    
    float history_confidence = std::min(1.0f, 
        static_cast<float>(keystroke_history_.size()) / config_.min_typing_history);
    
    float consistency_confidence = 1.0f - std::min(1.0f, 
        static_cast<float>(pattern.std_deviation.count()) / pattern.avg_interval.count());
    
    float recent_confidence = pattern.pause_probability;
    
    // Weighted average
    float confidence = history_confidence * 0.3f + 
                      consistency_confidence * 0.4f + 
                      recent_confidence * 0.3f;
    
    return confidence;
}

} // namespace RawrXD