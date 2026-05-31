// prefix_pinning.cpp - Implementation of prefix pinning for context freezing
// Part of the Copilot-like inference pipeline.

#include "prefix_pinning.h"
#include <algorithm>
#include <sstream>

namespace RawrXD {

PrefixPinning::PrefixPinning(size_t max_memory_mb)
    : max_memory_bytes_(max_memory_mb * 1024 * 1024)
    , current_memory_bytes_(0)
{
    stats_ = {};
}

PrefixPinning::~PrefixPinning() {
    Clear();
}

void PrefixPinning::SetConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    config_ = config;
}

PrefixHash PrefixPinning::FreezePrefix(
    const std::string& context,
    int cursor_line,
    int cursor_column
) {
    // Calculate freeze point
    int freeze_line = CalculateFreezePoint(context, cursor_line);
    
    // Split context at freeze point
    auto [frozen_prefix, mutable_suffix] = SplitContext(context, freeze_line, cursor_column);
    
    // Hash frozen prefix
    PrefixHash hash = HashPrefix(frozen_prefix);
    
    // Check if already frozen
    {
        std::lock_guard<std::mutex> lock(prefixes_mutex_);
        auto it = prefixes_.find(hash);
        if (it != prefixes_.end()) {
            // Already frozen, update last used
            it->second->last_used = std::chrono::steady_clock::now();
            it->second->ref_count++;
            
            // Update stats
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.cache_hits++;
                if (stats_.cache_hits + stats_.cache_misses > 0) {
                    stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                                     (stats_.cache_hits + stats_.cache_misses);
                }
            }
            
            return hash;
        }
    }
    
    // Create new pinned prefix
    auto prefix = std::make_unique<PinnedPrefix>();
    prefix->hash = hash;
    prefix->content = frozen_prefix;
    prefix->is_pinned = false;
    prefix->ref_count = 1;
    prefix->last_used = std::chrono::steady_clock::now();
    prefix->created = std::chrono::steady_clock::now();
    
    // TODO: Tokenize prefix
    // prefix->token_ids = tokenizer_.Encode(frozen_prefix);
    
    // TODO: Compute KV cache
    // prefix->kv_cache = ComputeKVCache(prefix->token_ids);
    
    prefix->memory_size = frozen_prefix.size() + prefix->token_ids.size() * sizeof(uint32_t);
    
    // Check memory limit
    if (current_memory_bytes_ + prefix->memory_size > max_memory_bytes_) {
        // Evict LRU
        EvictLRU(prefix->memory_size);
    }
    
    // Store prefix
    {
        std::lock_guard<std::mutex> lock(prefixes_mutex_);
        current_memory_bytes_ += prefix->memory_size;
        prefixes_[hash] = std::move(prefix);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_prefixes++;
            stats_.total_memory_bytes = current_memory_bytes_;
            stats_.cache_misses++;
            if (stats_.cache_hits + stats_.cache_misses > 0) {
                stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                                 (stats_.cache_hits + stats_.cache_misses);
            }
        }
    }
    
    return hash;
}

bool PrefixPinning::PinInVRAM(PrefixHash hash) {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    
    auto it = prefixes_.find(hash);
    if (it == prefixes_.end()) {
        return false;
    }
    
    PinnedPrefix& prefix = *it->second;
    
    if (prefix.is_pinned) {
        return true;  // Already pinned
    }
    
    // TODO: Pin in VRAM
    // This would copy the KV cache to GPU memory and prevent eviction
    
    prefix.is_pinned = true;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.pinned_prefixes++;
        stats_.pins++;
    }
    
    return true;
}

void PrefixPinning::UnpinFromVRAM(PrefixHash hash) {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    
    auto it = prefixes_.find(hash);
    if (it == prefixes_.end()) {
        return;
    }
    
    PinnedPrefix& prefix = *it->second;
    
    if (!prefix.is_pinned) {
        return;  // Already unpinned
    }
    
    // TODO: Unpin from VRAM
    // This would allow the KV cache to be evicted
    
    prefix.is_pinned = false;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.pinned_prefixes--;
        stats_.unpins++;
    }
}

std::string PrefixPinning::GetMutableSuffix(
    const std::string& context,
    int cursor_line,
    int cursor_column
) const {
    auto [frozen, suffix] = SplitContext(context, cursor_line, cursor_column);
    return suffix;
}

std::pair<std::string, std::string> PrefixPinning::SplitContext(
    const std::string& context,
    int cursor_line,
    int cursor_column
) const {
    int freeze_line = CalculateFreezePoint(context, cursor_line);
    
    // Split into lines
    std::vector<std::string> lines;
    std::istringstream stream(context);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    
    // Split at freeze point
    std::string frozen_prefix;
    std::string mutable_suffix;
    
    for (int i = 0; i < static_cast<int>(lines.size()); i++) {
        if (i < freeze_line) {
            frozen_prefix += lines[i] + "\n";
        } else {
            mutable_suffix += lines[i] + "\n";
        }
    }
    
    return {frozen_prefix, mutable_suffix};
}

void PrefixPinning::UpdateFrozenPrefix(
    PrefixHash old_hash,
    const std::string& new_context,
    int cursor_line,
    int cursor_column
) {
    // Release old prefix
    ReleasePrefix(old_hash);
    
    // Freeze new prefix
    FreezePrefix(new_context, cursor_line, cursor_column);
}

void PrefixPinning::ReleasePrefix(PrefixHash hash) {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    
    auto it = prefixes_.find(hash);
    if (it == prefixes_.end()) {
        return;
    }
    
    PinnedPrefix& prefix = *it->second;
    prefix.ref_count--;
    
    if (prefix.ref_count <= 0) {
        // Remove prefix
        current_memory_bytes_ -= prefix.memory_size;
        prefixes_.erase(it);
        
        // Update stats
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.total_prefixes--;
            stats_.total_memory_bytes = current_memory_bytes_;
        }
    }
}

void PrefixPinning::Clear() {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    prefixes_.clear();
    current_memory_bytes_ = 0;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_prefixes = 0;
        stats_.pinned_prefixes = 0;
        stats_.total_memory_bytes = 0;
    }
}

void PrefixPinning::EvictLRU(size_t bytes_to_free) {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    
    // Find least recently used prefix
    auto oldest = prefixes_.end();
    auto oldest_time = std::chrono::steady_clock::now();
    
    for (auto it = prefixes_.begin(); it != prefixes_.end(); ++it) {
        if (!it->second->is_pinned && it->second->last_used < oldest_time) {
            oldest_time = it->second->last_used;
            oldest = it;
        }
    }
    
    if (oldest == prefixes_.end()) {
        return;  // Nothing to evict
    }
    
    // Evict
    current_memory_bytes_ -= oldest->second->memory_size;
    prefixes_.erase(oldest);
}

int PrefixPinning::CalculateFreezePoint(
    const std::string& context,
    int cursor_line
) const {
    // Count lines
    int line_count = 0;
    for (char c : context) {
        if (c == '\n') {
            line_count++;
        }
    }
    
    // Calculate freeze point
    int freeze_line = static_cast<int>(line_count * config_.freeze_ratio);
    
    // Clamp to min/max
    freeze_line = std::max(freeze_line, config_.min_freeze_lines);
    freeze_line = std::min(freeze_line, config_.max_freeze_lines);
    
    // Don't freeze past cursor
    freeze_line = std::min(freeze_line, cursor_line - 1);
    
    return freeze_line;
}

PinnedPrefix* PrefixPinning::FindPrefix(PrefixHash hash) {
    auto it = prefixes_.find(hash);
    return it != prefixes_.end() ? it->second.get() : nullptr;
}

const PinnedPrefix* PrefixPinning::FindPrefix(PrefixHash hash) const {
    auto it = prefixes_.find(hash);
    return it != prefixes_.end() ? it->second.get() : nullptr;
}

} // namespace RawrXD