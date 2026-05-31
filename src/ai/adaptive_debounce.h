// adaptive_debounce.h - Adaptive debounce based on typing speed
// Implements smart debounce that adjusts timing based on user behavior
//
// Strategy:
//   - Fast typing: 200ms debounce (user is actively typing, wait longer)
//   - Slow typing: 80ms debounce (user is thinking, respond faster)
//   - Pauses: 50ms debounce (user stopped, respond immediately)
//
// This makes the system feel "psychic" by adapting to user rhythm.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>

namespace RawrXD {

// Typing speed classification
enum class TypingSpeed : uint8_t {
    FAST = 0,    // Rapid typing, long debounce
    NORMAL = 1,  // Normal typing, medium debounce
    SLOW = 2,    // Slow typing, short debounce
    PAUSED = 3,  // User stopped, minimal debounce
};

// Debounce statistics
struct DebounceStats {
    int total_keystrokes;
    int fast_intervals;
    int normal_intervals;
    int slow_intervals;
    int pause_intervals;
    std::chrono::milliseconds avg_interval;
    std::chrono::milliseconds current_debounce;
    TypingSpeed current_speed;
};

// Adaptive debounce manager
class AdaptiveDebounce {
public:
    AdaptiveDebounce();
    ~AdaptiveDebounce() = default;
    
    // Configure debounce timing
    struct Config {
        std::chrono::milliseconds fast_debounce{200};   // Fast typing
        std::chrono::milliseconds normal_debounce{150}; // Normal typing
        std::chrono::milliseconds slow_debounce{80};    // Slow typing
        std::chrono::milliseconds pause_debounce{50};   // User paused
        
        std::chrono::milliseconds fast_threshold{100};  // < 100ms = fast
        std::chrono::milliseconds slow_threshold{300};   // > 300ms = slow
        std::chrono::milliseconds pause_threshold{1000}; // > 1s = pause
        
        int history_size = 10;  // Keep last N keystroke intervals
        bool enable_adaptation = true;
    };
    void SetConfig(const Config& config);
    
    // Record keystroke (call on every keystroke)
    void RecordKeystroke();
    
    // Get current debounce delay based on typing speed
    std::chrono::milliseconds GetDebounceDelay() const;
    
    // Get current typing speed classification
    TypingSpeed GetTypingSpeed() const;
    
    // Check if user has paused (no keystroke for pause_threshold)
    bool IsPaused() const;
    
    // Get time since last keystroke
    std::chrono::milliseconds GetTimeSinceLastKeystroke() const;
    
    // Get statistics
    DebounceStats GetStats() const;
    
    // Reset state
    void Reset();
    
private:
    // Calculate typing speed from recent history
    TypingSpeed CalculateTypingSpeed() const;
    
    // Get average interval from history
    std::chrono::milliseconds GetAverageInterval() const;
    
    // Members
    Config config_;
    mutable std::mutex mutex_;
    
    // Keystroke history (stores intervals between keystrokes)
    std::deque<std::chrono::milliseconds> keystroke_history_;
    
    // Last keystroke time
    std::chrono::steady_clock::time_point last_keystroke_;
    
    // Statistics
    DebounceStats stats_;
    
    // Initialization flag
    bool initialized_;
};

// Inline implementations

inline std::chrono::milliseconds AdaptiveDebounce::GetDebounceDelay() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!config_.enable_adaptation) {
        return config_.normal_debounce;
    }
    
    TypingSpeed speed = CalculateTypingSpeed();
    
    switch (speed) {
        case TypingSpeed::FAST:
            return config_.fast_debounce;
        case TypingSpeed::NORMAL:
            return config_.normal_debounce;
        case TypingSpeed::SLOW:
            return config_.slow_debounce;
        case TypingSpeed::PAUSED:
            return config_.pause_debounce;
        default:
            return config_.normal_debounce;
    }
}

inline TypingSpeed AdaptiveDebounce::GetTypingSpeed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return CalculateTypingSpeed();
}

inline bool AdaptiveDebounce::IsPaused() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return true;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_keystroke_);
    
    return elapsed > config_.pause_threshold;
}

inline std::chrono::milliseconds AdaptiveDebounce::GetTimeSinceLastKeystroke() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return std::chrono::milliseconds(0);
    }
    
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_keystroke_);
}

inline DebounceStats AdaptiveDebounce::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

inline void AdaptiveDebounce::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    keystroke_history_.clear();
    initialized_ = false;
    stats_ = {};
}

} // namespace RawrXD