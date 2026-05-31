// context_heat_map.h - Context heat mapping for token prioritization
// Implements prioritization of tokens near cursor vs far from cursor
//
// Architecture:
//   - Heat map: Tokens near cursor have higher priority
//   - Priority: Tokens near cursor > tokens far from cursor
//   - Eviction: Evict cold tokens first
//
// Key insight:
//   - Not: All tokens are equal
//   - But: Tokens near cursor matter more
//
// This is where we surpass Copilot.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Token heat
struct TokenHeat {
    int token_index;
    float heat;              // 0.0 = cold, 1.0 = hot
    int distance_from_cursor;
    int access_count;
    std::chrono::steady_clock::time_point last_access;
    bool is_near_cursor;
    bool is_recent;
};

// Heat map statistics
struct HeatMapStats {
    int total_tokens;
    int hot_tokens;
    int warm_tokens;
    int cold_tokens;
    float avg_heat;
    float max_heat;
    float min_heat;
    int cursor_distance_avg;
    int cache_hits;
    int cache_misses;
    float hit_rate;
};

// Context heat map
class ContextHeatMap {
public:
    ContextHeatMap(int context_size = 4096);
    ~ContextHeatMap();
    
    // Configure heat map
    struct Config {
        int hot_window = 128;          // Tokens near cursor considered hot
        int warm_window = 512;         // Tokens near cursor considered warm
        float heat_decay = 0.9f;       // Heat decay per token away from cursor
        float access_boost = 0.1f;     // Heat boost per access
        float time_decay = 0.95f;      // Heat decay over time
        std::chrono::seconds hot_timeout{30};  // Time before token goes cold
    };
    void SetConfig(const Config& config);
    
    // Update cursor position
    void UpdateCursor(int cursor_token);
    
    // Access token (increases heat)
    void AccessToken(int token_index);
    
    // Get token heat
    float GetHeat(int token_index) const;
    
    // Get token priority (higher = more important)
    float GetPriority(int token_index) const;
    
    // Get hot tokens (near cursor)
    std::vector<int> GetHotTokens() const;
    
    // Get cold tokens (far from cursor)
    std::vector<int> GetColdTokens() const;
    
    // Evict cold tokens
    std::vector<int> EvictColdTokens(int count);
    
    // Update heat map (decay over time)
    void UpdateHeatMap();
    
    // Get statistics
    HeatMapStats GetStats() const;
    
    // Reset heat map
    void Reset();
    
private:
    // Calculate heat for token
    float CalculateHeat(int token_index) const;
    
    // Calculate distance from cursor
    int CalculateDistance(int token_index) const;
    
    // Members
    Config config_;
    int context_size_;
    int cursor_token_;
    
    mutable std::mutex heat_mutex_;
    std::unordered_map<int, TokenHeat> heat_map_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    HeatMapStats stats_;
    
    // Last update time
    std::chrono::steady_clock::time_point last_update_;
};

// Inline implementations

inline float ContextHeatMap::GetHeat(int token_index) const {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    auto it = heat_map_.find(token_index);
    if (it == heat_map_.end()) {
        return 0.0f;
    }
    
    return it->second.heat;
}

inline float ContextHeatMap::GetPriority(int token_index) const {
    return GetHeat(token_index);
}

inline HeatMapStats ContextHeatMap::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace RawrXD