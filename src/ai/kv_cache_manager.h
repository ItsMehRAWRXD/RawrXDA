// kv_cache_manager.h - Hash-based KV-cache reuse for prefix optimization
// Implements hash-based reuse across requests to prevent full recompute on every keystroke
//
// Key insight:
//   - Every keystroke currently = full recompute
//   - With hash-based reuse: only compute NEW tokens
//   - Massive TPS improvement for repeated contexts
//
// Strategy:
//   1. Hash prefix context (file path + cursor position + text before cursor)
//   2. Check if hash exists in KV cache
//   3. If hit: attach cache, only compute new tokens
//   4. If miss: full compute, store hash for future
//
// Part of the Copilot-like inference pipeline.

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace RawrXD {

// Hash type for context identification
using ContextHash = uint64_t;

// KV cache entry
struct KVCacheEntry {
    ContextHash hash;                    // Hash of prefix context
    std::vector<uint32_t> token_ids;     // Token IDs for this prefix
    std::vector<float> kv_cache_data;    // Actual KV cache tensors
    size_t seq_len;                      // Sequence length
    size_t vram_offset;                   // Offset in VRAM (if applicable)
    bool is_resident;                     // Whether cache is in VRAM
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point created;
    int access_count;                     // How many times this cache was reused
    size_t memory_size;                   // Bytes used by this cache
};

// Cache statistics
struct KVCacheStats {
    int total_entries;
    int resident_entries;
    size_t total_memory_bytes;
    size_t vram_used_bytes;
    int cache_hits;
    int cache_misses;
    int cache_evictions;
    float hit_rate;
    std::chrono::microseconds avg_hit_latency_saved;
    std::chrono::microseconds avg_miss_latency;
};

// Prefix hash for incremental updates
struct PrefixHash {
    ContextHash full_hash;               // Hash of entire prefix
    ContextHash incremental_hash;         // Hash of just new content
    int common_prefix_len;                // Length of common prefix with previous
    std::vector<ContextHash> segment_hashes;  // Hashes of segments (for partial reuse)
};

// KV cache manager
class KVCacheManager {
public:
    KVCacheManager(size_t max_entries = 100, size_t max_memory_mb = 1024);
    ~KVCacheManager();
    
    // Hash context for cache lookup
    ContextHash HashContext(
        const std::string& file_path,
        const std::string& prefix,
        int cursor_line,
        int cursor_column
    ) const;
    
    // Hash with incremental support
    PrefixHash HashIncremental(
        const std::string& file_path,
        const std::string& prefix,
        int cursor_line,
        int cursor_column,
        const std::string& previous_prefix
    ) const;
    
    // Check if cache exists for hash
    bool HasCache(ContextHash hash) const;
    
    // Get cache entry (increments access count)
    const KVCacheEntry* GetCache(ContextHash hash) const;
    
    // Store new cache entry
    void StoreCache(
        ContextHash hash,
        const std::vector<uint32_t>& token_ids,
        const std::vector<float>& kv_cache_data,
        size_t vram_offset = 0
    );
    
    // Update existing cache (for incremental)
    void UpdateCache(
        ContextHash hash,
        const std::vector<uint32_t>& new_tokens,
        const std::vector<float>& new_kv_data
    );
    
    // Mark cache as recently used (prevents eviction)
    void TouchCache(ContextHash hash);
    
    // Invalidate cache (remove entry)
    void InvalidateCache(ContextHash hash);
    
    // Invalidate all caches for file
    void InvalidateFileCaches(const std::string& file_path);
    
    // Find longest common prefix in cache
    // Returns cache entry and length of common prefix
    std::pair<const KVCacheEntry*, int> FindLongestPrefix(
        const std::vector<ContextHash>& segment_hashes
    ) const;
    
    // Get statistics
    KVCacheStats GetStats() const;
    
    // Set memory limits
    void SetMaxEntries(size_t max_entries);
    void SetMaxMemory(size_t max_memory_mb);
    
    // Enable/disable VRAM residency
    void EnableVRAMResidency(bool enable);
    
    // Prefetch cache into VRAM
    void PrefetchToVRAM(ContextHash hash);
    
    // Evict least recently used entries
    void EvictLRU(size_t bytes_to_free);
    
    // Clear all caches
    void Clear();
    
private:
    // FNV-1a hash implementation
    ContextHash FNV1aHash(const std::string& data) const;
    
    // Combine two hashes
    ContextHash CombineHashes(ContextHash a, ContextHash b) const;
    
    // Hash file path for quick lookup
    ContextHash HashFilePath(const std::string& file_path) const;
    
    // Find entry by hash
    KVCacheEntry* FindEntry(ContextHash hash);
    const KVCacheEntry* FindEntry(ContextHash hash) const;
    
    // Evict entries to free memory
    bool EvictIfNeeded(size_t required_bytes);
    
    // Members
    size_t max_entries_;
    size_t max_memory_bytes_;
    size_t current_memory_bytes_;
    bool vram_residency_enabled_;
    
    mutable std::shared_mutex cache_mutex_;
    std::unordered_map<ContextHash, std::unique_ptr<KVCacheEntry>> cache_entries_;
    
    // File path to cache hash mapping (for invalidation)
    std::unordered_map<ContextHash, std::unordered_set<ContextHash>> file_caches_;
    
    // LRU tracking
    mutable std::mutex lru_mutex_;
    std::vector<ContextHash> lru_list_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    KVCacheStats stats_;
};

// Inline implementations

inline bool KVCacheManager::HasCache(ContextHash hash) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    return cache_entries_.find(hash) != cache_entries_.end();
}

inline const KVCacheEntry* KVCacheManager::GetCache(ContextHash hash) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = cache_entries_.find(hash);
    if (it == cache_entries_.end()) {
        return nullptr;
    }
    
    // Update access count and last used
    it->second->access_count++;
    it->second->last_used = std::chrono::steady_clock::now();
    
    return it->second.get();
}

inline void KVCacheManager::TouchCache(ContextHash hash) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    auto it = cache_entries_.find(hash);
    if (it != cache_entries_.end()) {
        it->second->last_used = std::chrono::steady_clock::now();
    }
}

inline KVCacheStats KVCacheManager::GetStats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

inline ContextHash KVCacheManager::FNV1aHash(const std::string& data) const {
    ContextHash hash = 14695981039346656037ULL;
    for (char c : data) {
        hash ^= static_cast<ContextHash>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

inline ContextHash KVCacheManager::CombineHashes(ContextHash a, ContextHash b) const {
    // Boost hash combine
    return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

inline ContextHash KVCacheManager::HashFilePath(const std::string& file_path) const {
    return FNV1aHash(file_path);
}

} // namespace RawrXD