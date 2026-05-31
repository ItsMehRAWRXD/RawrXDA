// prefix_pinning.h - Prefix pinning for context freezing
// Implements frozen prefix optimization to prevent re-tokenization/re-encoding
//
// Key insight:
//   - First ~80% of context is often unchanged between keystrokes
//   - Only last few lines change
//   - Keep frozen prefix in GPU memory
//   - Only mutate the suffix
//
// This massively reduces per-keystroke cost.
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// Prefix hash for identification
using PrefixHash = uint64_t;

// Pinned prefix entry
struct PinnedPrefix {
    PrefixHash hash;                    // Hash of prefix content
    std::string content;                 // Actual prefix text
    std::vector<uint32_t> token_ids;     // Tokenized prefix
    std::vector<float> kv_cache;          // KV cache for prefix
    size_t vram_offset;                   // Offset in GPU memory
    size_t memory_size;                   // Bytes used
    bool is_pinned;                       // Whether pinned in VRAM
    int ref_count;                         // Reference count
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point created;
};

// Pinning statistics
struct PinningStats {
    int total_prefixes;
    int pinned_prefixes;
    size_t total_memory_bytes;
    size_t vram_used_bytes;
    int cache_hits;
    int cache_misses;
    int pins;
    int unpins;
    float hit_rate;
    std::chrono::microseconds avg_pin_latency;
    std::chrono::microseconds avg_unpin_latency;
};

// Prefix pinning manager
class PrefixPinning {
public:
    PrefixPinning(size_t max_memory_mb = 512);
    ~PrefixPinning();
    
    // Configure pinning
    struct Config {
        float freeze_ratio = 0.8f;        // Freeze first 80% of context
        int min_freeze_lines = 10;       // Minimum lines to freeze
        int max_freeze_lines = 400;      // Maximum lines to freeze
        bool enable_vram_pinning = true;  // Pin in GPU memory
        bool enable_tokenization_cache = true;  // Cache tokenization
        bool enable_kv_cache = true;      // Cache KV cache
        std::chrono::seconds pin_timeout{300};  // Unpin after 5 min idle
    };
    void SetConfig(const Config& config);
    
    // Freeze prefix from context
    // Returns hash of frozen prefix
    PrefixHash FreezePrefix(
        const std::string& context,
        int cursor_line,
        int cursor_column
    );
    
    // Get frozen prefix
    const PinnedPrefix* GetPinnedPrefix(PrefixHash hash) const;
    
    // Check if prefix is frozen
    bool IsFrozen(PrefixHash hash) const;
    
    // Pin prefix in VRAM (prevent eviction)
    bool PinInVRAM(PrefixHash hash);
    
    // Unpin prefix (allow eviction)
    void UnpinFromVRAM(PrefixHash hash);
    
    // Get mutable suffix (last ~20% of context)
    std::string GetMutableSuffix(
        const std::string& context,
        int cursor_line,
        int cursor_column
    ) const;
    
    // Split context into frozen prefix and mutable suffix
    std::pair<std::string, std::string> SplitContext(
        const std::string& context,
        int cursor_line,
        int cursor_column
    ) const;
    
    // Update frozen prefix (when context changes significantly)
    void UpdateFrozenPrefix(
        PrefixHash old_hash,
        const std::string& new_context,
        int cursor_line,
        int cursor_column
    );
    
    // Release frozen prefix
    void ReleasePrefix(PrefixHash hash);
    
    // Get statistics
    PinningStats GetStats() const;
    
    // Clear all pinned prefixes
    void Clear();
    
    // Evict least recently used
    void EvictLRU(size_t bytes_to_free);
    
private:
    // Calculate freeze point (line number where we freeze)
    int CalculateFreezePoint(
        const std::string& context,
        int cursor_line
    ) const;
    
    // Hash prefix content
    PrefixHash HashPrefix(const std::string& prefix) const;
    
    // Find prefix by hash
    PinnedPrefix* FindPrefix(PrefixHash hash);
    const PinnedPrefix* FindPrefix(PrefixHash hash) const;
    
    // Members
    Config config_;
    size_t max_memory_bytes_;
    size_t current_memory_bytes_;
    
    mutable std::mutex prefixes_mutex_;
    std::unordered_map<PrefixHash, std::unique_ptr<PinnedPrefix>> prefixes_;
    
    // LRU tracking
    mutable std::mutex lru_mutex_;
    std::vector<PrefixHash> lru_list_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    PinningStats stats_;
};

// Inline implementations

inline bool PrefixPinning::IsFrozen(PrefixHash hash) const {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    return prefixes_.find(hash) != prefixes_.end();
}

inline const PinnedPrefix* PrefixPinning::GetPinnedPrefix(PrefixHash hash) const {
    std::lock_guard<std::mutex> lock(prefixes_mutex_);
    auto it = prefixes_.find(hash);
    if (it == prefixes_.end()) {
        return nullptr;
    }
    
    it->second->last_used = std::chrono::steady_clock::now();
    return it->second.get();
}

inline PinningStats PrefixPinning::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline PrefixHash PrefixPinning::HashPrefix(const std::string& prefix) const {
    // FNV-1a hash
    PrefixHash hash = 14695981039346656037ULL;
    for (char c : prefix) {
        hash ^= static_cast<PrefixHash>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace RawrXD