// context_heat_map.cpp - Implementation of context heat mapping
// Part of the Copilot-like inference pipeline.

#include "context_heat_map.h"
#include <algorithm>
#include <cmath>

namespace RawrXD {

ContextHeatMap::ContextHeatMap(int context_size)
    : context_size_(context_size)
    , cursor_token_(0)
{
    stats_ = {};
    last_update_ = std::chrono::steady_clock::now();
}

ContextHeatMap::~ContextHeatMap() {
}

void ContextHeatMap::SetConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    config_ = config;
}

void ContextHeatMap::UpdateCursor(int cursor_token) {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    cursor_token_ = cursor_token;
    
    // Update heat for all tokens
    for (auto& pair : heat_map_) {
        TokenHeat& heat = pair.second;
        heat.distance_from_cursor = CalculateDistance(pair.first);
        heat.is_near_cursor = heat.distance_from_cursor < config_.hot_window;
        heat.heat = CalculateHeat(pair.first);
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cursor_distance_avg = 0;
        
        for (const auto& pair : heat_map_) {
            stats_.cursor_distance_avg += pair.second.distance_from_cursor;
        }
        
        if (!heat_map_.empty()) {
            stats_.cursor_distance_avg /= static_cast<int>(heat_map_.size());
        }
    }
}

void ContextHeatMap::AccessToken(int token_index) {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    auto& heat = heat_map_[token_index];
    heat.token_index = token_index;
    heat.access_count++;
    heat.last_access = std::chrono::steady_clock::now();
    heat.is_recent = true;
    
    // Boost heat
    heat.heat = std::min(1.0f, heat.heat + config_.access_boost);
    
    // Update distance
    heat.distance_from_cursor = CalculateDistance(token_index);
    heat.is_near_cursor = heat.distance_from_cursor < config_.hot_window;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.cache_hits++;
        
        if (stats_.cache_hits + stats_.cache_misses > 0) {
            stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                             (stats_.cache_hits + stats_.cache_misses);
        }
    }
}

std::vector<int> ContextHeatMap::GetHotTokens() const {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    std::vector<int> hot_tokens;
    
    for (const auto& pair : heat_map_) {
        if (pair.second.heat > 0.7f) {
            hot_tokens.push_back(pair.first);
        }
    }
    
    return hot_tokens;
}

std::vector<int> ContextHeatMap::GetColdTokens() const {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    std::vector<int> cold_tokens;
    
    for (const auto& pair : heat_map_) {
        if (pair.second.heat < 0.3f) {
            cold_tokens.push_back(pair.first);
        }
    }
    
    return cold_tokens;
}

std::vector<int> ContextHeatMap::EvictColdTokens(int count) {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    // Sort by heat (coldest first)
    std::vector<std::pair<int, float>> tokens;
    tokens.reserve(heat_map_.size());
    
    for (const auto& pair : heat_map_) {
        tokens.push_back({pair.first, pair.second.heat});
    }
    
    std::sort(tokens.begin(), tokens.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });
    
    // Evict coldest tokens
    std::vector<int> evicted;
    evicted.reserve(count);
    
    for (int i = 0; i < count && i < static_cast<int>(tokens.size()); i++) {
        evicted.push_back(tokens[i].first);
        heat_map_.erase(tokens[i].first);
    }
    
    return evicted;
}

void ContextHeatMap::UpdateHeatMap() {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_);
    
    // Decay heat over time
    for (auto& pair : heat_map_) {
        TokenHeat& heat = pair.second;
        
        // Time decay
        float time_decay = std::pow(config_.time_decay, elapsed.count());
        heat.heat *= time_decay;
        
        // Distance decay
        heat.distance_from_cursor = CalculateDistance(pair.first);
        float distance_decay = std::pow(config_.heat_decay, heat.distance_from_cursor);
        heat.heat *= distance_decay;
        
        // Update flags
        heat.is_near_cursor = heat.distance_from_cursor < config_.hot_window;
        heat.is_recent = (now - heat.last_access) < config_.hot_timeout;
        
        // Clamp heat
        heat.heat = std::max(0.0f, std::min(1.0f, heat.heat));
    }
    
    last_update_ = now;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        
        stats_.total_tokens = static_cast<int>(heat_map_.size());
        stats_.hot_tokens = 0;
        stats_.warm_tokens = 0;
        stats_.cold_tokens = 0;
        stats_.avg_heat = 0.0f;
        stats_.max_heat = 0.0f;
        stats_.min_heat = 1.0f;
        
        for (const auto& pair : heat_map_) {
            const TokenHeat& heat = pair.second;
            
            if (heat.heat > 0.7f) {
                stats_.hot_tokens++;
            } else if (heat.heat > 0.3f) {
                stats_.warm_tokens++;
            } else {
                stats_.cold_tokens++;
            }
            
            stats_.avg_heat += heat.heat;
            stats_.max_heat = std::max(stats_.max_heat, heat.heat);
            stats_.min_heat = std::min(stats_.min_heat, heat.heat);
        }
        
        if (!heat_map_.empty()) {
            stats_.avg_heat /= static_cast<float>(heat_map_.size());
        }
    }
}

void ContextHeatMap::Reset() {
    std::lock_guard<std::mutex> lock(heat_mutex_);
    heat_map_.clear();
    cursor_token_ = 0;
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_ = {};
    }
}

float ContextHeatMap::CalculateHeat(int token_index) const {
    auto it = heat_map_.find(token_index);
    if (it == heat_map_.end()) {
        return 0.0f;
    }
    
    const TokenHeat& heat = it->second;
    
    // Base heat from access count
    float base_heat = std::min(1.0f, heat.access_count * config_.access_boost);
    
    // Distance decay
    int distance = CalculateDistance(token_index);
    float distance_decay = std::pow(config_.heat_decay, distance);
    
    // Time decay
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - heat.last_access);
    float time_decay = std::pow(config_.time_decay, elapsed.count());
    
    // Combined heat
    float combined_heat = base_heat * distance_decay * time_decay;
    
    return std::max(0.0f, std::min(1.0f, combined_heat));
}

int ContextHeatMap::CalculateDistance(int token_index) const {
    return std::abs(token_index - cursor_token_);
}

} // namespace RawrXD