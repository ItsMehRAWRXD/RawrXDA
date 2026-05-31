// adaptive_debounce.cpp - Implementation of adaptive debounce
// Part of the Copilot-like inference pipeline.

#include "adaptive_debounce.h"
#include <algorithm>

namespace RawrXD {

AdaptiveDebounce::AdaptiveDebounce()
    : initialized_(false)
{
    stats_ = {};
}

void AdaptiveDebounce::SetConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

void AdaptiveDebounce::RecordKeystroke() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    if (initialized_) {
        // Calculate interval since last keystroke
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_keystroke_);
        
        // Add to history
        keystroke_history_.push_back(interval);
        
        // Trim history to configured size
        while (keystroke_history_.size() > static_cast<size_t>(config_.history_size)) {
            keystroke_history_.pop_front();
        }
        
        // Update statistics
        stats_.total_keystrokes++;
        
        if (interval < config_.fast_threshold) {
            stats_.fast_intervals++;
        } else if (interval < config_.slow_threshold) {
            stats_.normal_intervals++;
        } else if (interval < config_.pause_threshold) {
            stats_.slow_intervals++;
        } else {
            stats_.pause_intervals++;
        }
        
        stats_.avg_interval = GetAverageInterval();
    }
    
    last_keystroke_ = now;
    initialized_ = true;
    
    // Update current speed and debounce
    stats_.current_speed = CalculateTypingSpeed();
    stats_.current_debounce = GetDebounceDelay();
}

TypingSpeed AdaptiveDebounce::CalculateTypingSpeed() const {
    if (!initialized_ || keystroke_history_.empty()) {
        return TypingSpeed::NORMAL;
    }
    
    // Check if user has paused
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_keystroke_);
    
    if (elapsed > config_.pause_threshold) {
        return TypingSpeed::PAUSED;
    }
    
    // Calculate average interval
    auto avg_interval = GetAverageInterval();
    
    // Classify based on average interval
    if (avg_interval < config_.fast_threshold) {
        return TypingSpeed::FAST;
    } else if (avg_interval < config_.slow_threshold) {
        return TypingSpeed::NORMAL;
    } else {
        return TypingSpeed::SLOW;
    }
}

std::chrono::milliseconds AdaptiveDebounce::GetAverageInterval() const {
    if (keystroke_history_.empty()) {
        return std::chrono::milliseconds(150);  // Default
    }
    
    // Calculate weighted average (recent intervals weighted more)
    int64_t total_ms = 0;
    float total_weight = 0.0f;
    
    int index = 0;
    for (auto interval : keystroke_history_) {
        // Weight recent intervals more heavily
        float weight = 1.0f + (index / static_cast<float>(keystroke_history_.size()));
        total_ms += static_cast<int64_t>(interval.count() * weight);
        total_weight += weight;
        index++;
    }
    
    return std::chrono::milliseconds(static_cast<int64_t>(total_ms / total_weight));
}

} // namespace RawrXD