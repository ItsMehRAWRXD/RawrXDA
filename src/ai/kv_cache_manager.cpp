// kv_cache_manager.cpp - Implementation of hash-based KV-cache reuse
// Part of the Copilot-like inference pipeline.

#include "kv_cache_manager.h"
#include <algorithm>
#include <cassert>

namespace RawrXD {

KVCacheManager::KVCacheManager(size_t max_entries, size_t max_memory_mb)
    : max_entries_(max_entries)
    , max_memory_bytes_(max_memory_mb * 1024 * 1024)
    , current_memory_bytes_(0)
    , vram_residency_enabled_(false)
{
    stats_ = {};
}

KVCacheManager::~KVCacheManager() {
    Clear();
}

ContextHash KVCacheManager::HashContext(
    const std::string& file_path,
    const std::string& prefix,
    int cursor_line,
    int cursor_column
) const {
    // Combine file path, prefix, and cursor position
    ContextHash hash = HashFilePath(file_path);
    hash = CombineHashes(hash, FNV1aHash(prefix));
    hash = CombineHashes(hash, static_cast<ContextHash>(cursor_line));
    hash = CombineHashes(hash, static_cast<ContextHash>(cursor_column));
    return hash;
}

PrefixHash KVCacheManager::HashIncremental(
    const std::string& file_path,
    const std::string& prefix,
    int cursor_line,
    int cursor_column,
    const std::string& previous_prefix
) const {
    PrefixHash result;
    result.full_hash = HashContext(file_path, prefix, cursor_line, cursor_column);
    
    // Find common prefix length
    size_t common_len = 0;
    size_t min_len = std::min(prefix.length(), previous_prefix.length());
    
    while (common_len < min_len && prefix[common_len] == previous_prefix[common_len]) {
        common_len++;
    }
    
    result.common_prefix_len = static_cast<int>(common_len);
    
    // Hash just the new content
    if (common_len < prefix.length()) {
        result.incremental_hash = FNV1aHash(prefix.substr(common_len));
    } else {
        result.incremental_hash = 0;
    }
    
    // Create segment hashes for partial reuse
    // Split prefix into segments (e.g., by lines or by token count)
    std::vector<std::string> segments;
    std::string current_segment;
    
    for (size_t i = 0; i < prefix.length(); i++) {
        current_segment += prefix[i];
        
        // Split on newlines or every 100 characters
        if (prefix[i] == '\n' || current_segment.length() >= 100) {
            segments.push_back(current_segment);
            current_segment.clear();
        }
    }
    
    if (!current_segment.empty()) {
        segments.push_back(current_segment);
    }
    
    // Hash each segment
    result.segment_hashes.reserve(segments.size());
    ContextHash running_hash = HashFilePath(file_path);
    
    for (const auto& segment : segments) {
        running_hash = CombineHashes(running_hash, FNV1aHash(segment));
        result.segment_hashes.push_back(running_hash);
    }
    
    return result;
}

void KVCacheManager::StoreCache(
    ContextHash hash,
    const std::vector<uint32_t>& token_ids,
    const std::vector<float>& kv_cache_data,
    size_t vram_offset
) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    // Check if we need to evict
    size_t entry_size = kv_cache_data.size() * sizeof(float);
    if (!EvictIfNeeded(entry_size)) {
        // Can't store this cache
        return;
    }
    
    // Create new entry
    auto entry = std::make_unique<KVCacheEntry>();
    entry->hash = hash;
    entry->token_ids = token_ids;
    entry->kv_cache_data = kv_cache_data;
    entry->seq_len = token_ids.size();
    entry->vram_offset = vram_offset;
    entry->is_resident = false;
    entry->last_used = std::chrono::steady_clock::now();
    entry->created = std::chrono::steady_clock::now();
    entry->access_count = 1;
    entry->memory_size = entry_size;
    
    // Store entry
    current_memory_bytes_ += entry_size;
    cache_entries_[hash] = std::move(entry);
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_entries++;
        stats_.total_memory_bytes = current_memory_bytes_;
        stats_.cache_misses++;
        
        if (stats_.total_entries > 0) {
            stats_.hit_rate = static_cast<float>(stats_.cache_hits) / 
                             (stats_.cache_hits + stats_.cache_misses);
        }
    }
}

void KVCacheManager::UpdateCache(
    ContextHash hash,
    const std::vector<uint32_t>& new_tokens,
    const std::vector<float>& new_kv_data
) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = cache_entries_.find(hash);
    if (it == cache_entries_.end()) {
        return;
    }
    
    KVCacheEntry& entry = *it->second;
    
    // Update token IDs
    entry.token_ids.insert(entry.token_ids.end(), new_tokens.begin(), new_tokens.end());
    
    // Update KV cache data
    size_t old_size = entry.kv_cache_data.size();
    entry.kv_cache_data.insert(entry.kv_cache_data.end(), new_kv_data.begin(), new_kv_data.end());
    
    // Update metadata
    size_t new_size = new_kv_data.size() * sizeof(float);
    current_memory_bytes_ += new_size;
    entry.memory_size += new_size;
    entry.seq_len = entry.token_ids.size();
    entry.last_used = std::chrono::steady_clock::now();
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_memory_bytes = current_memory_bytes_;
    }
}

void KVCacheManager::InvalidateCache(ContextHash hash) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = cache_entries_.find(hash);
    if (it == cache_entries_.end()) {
        return;
    }
    
    current_memory_bytes_ -= it->second->memory_size;
    cache_entries_.erase(it);
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_entries--;
        stats_.total_memory_bytes = current_memory_bytes_;
        stats_.cache_evictions++;
    }
}

void KVCacheManager::InvalidateFileCaches(const std::string& file_path) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    ContextHash file_hash = HashFilePath(file_path);
    auto it = file_caches_.find(file_hash);
    
    if (it == file_caches_.end()) {
        return;
    }
    
    // Remove all caches for this file
    for (ContextHash cache_hash : it->second) {
        auto cache_it = cache_entries_.find(cache_hash);
        if (cache_it != cache_entries_.end()) {
            current_memory_bytes_ -= cache_it->second->memory_size;
            cache_entries_.erase(cache_it);
        }
    }
    
    file_caches_.erase(it);
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_entries = cache_entries_.size();
        stats_.total_memory_bytes = current_memory_bytes_;
    }
}

std::pair<const KVCacheEntry*, int> KVCacheManager::FindLongestPrefix(
    const std::vector<ContextHash>& segment_hashes
) const {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    const KVCacheEntry* best_entry = nullptr;
    int best_len = 0;
    
    // Try to find the longest matching prefix
    for (int i = static_cast<int>(segment_hashes.size()) - 1; i >= 0; i--) {
        auto it = cache_entries_.find(segment_hashes[i]);
        if (it != cache_entries_.end()) {
            // Found a match
            best_entry = it->second.get();
            best_len = i + 1;
            break;
        }
    }
    
    return {best_entry, best_len};
}

void KVCacheManager::SetMaxEntries(size_t max_entries) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    max_entries_ = max_entries;
    
    // Evict if needed
    while (cache_entries_.size() > max_entries_) {
        EvictLRU(0);  // Evict one entry
    }
}

void KVCacheManager::SetMaxMemory(size_t max_memory_mb) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    max_memory_bytes_ = max_memory_mb * 1024 * 1024;
    
    // Evict if needed
    if (current_memory_bytes_ > max_memory_bytes_) {
        EvictLRU(current_memory_bytes_ - max_memory_bytes_);
    }
}

void KVCacheManager::EnableVRAMResidency(bool enable) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    vram_residency_enabled_ = enable;
}

void KVCacheManager::PrefetchToVRAM(ContextHash hash) {
    std::shared_lock<std::shared_mutex> lock(cache_mutex_);
    
    auto it = cache_entries_.find(hash);
    if (it == cache_entries_.end()) {
        return;
    }
    
    // TODO: Implement VRAM prefetch
    // This would copy the KV cache data to GPU memory
    it->second->is_resident = true;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.resident_entries++;
    }
}

void KVCacheManager::EvictLRU(size_t bytes_to_free) {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    // Find least recently used entry
    auto oldest = cache_entries_.end();
    auto oldest_time = std::chrono::steady_clock::now();
    
    for (auto it = cache_entries_.begin(); it != cache_entries_.end(); ++it) {
        if (it->second->last_used < oldest_time) {
            oldest_time = it->second->last_used;
            oldest = it;
        }
    }
    
    if (oldest == cache_entries_.end()) {
        return;
    }
    
    size_t freed = oldest->second->memory_size;
    current_memory_bytes_ -= freed;
    cache_entries_.erase(oldest);
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.total_entries--;
        stats_.total_memory_bytes = current_memory_bytes_;
        stats_.cache_evictions++;
    }
}

void KVCacheManager::Clear() {
    std::unique_lock<std::shared_mutex> lock(cache_mutex_);
    
    cache_entries_.clear();
    file_caches_.clear();
    current_memory_bytes_ = 0;
    
    // Update stats
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_ = {};
    }
}

KVCacheEntry* KVCacheManager::FindEntry(ContextHash hash) {
    auto it = cache_entries_.find(hash);
    return it != cache_entries_.end() ? it->second.get() : nullptr;
}

const KVCacheEntry* KVCacheManager::FindEntry(ContextHash hash) const {
    auto it = cache_entries_.find(hash);
    return it != cache_entries_.end() ? it->second.get() : nullptr;
}

bool KVCacheManager::EvictIfNeeded(size_t required_bytes) {
    // Check entry limit
    if (cache_entries_.size() >= max_entries_) {
        EvictLRU(0);
    }
    
    // Check memory limit
    while (current_memory_bytes_ + required_bytes > max_memory_bytes_) {
        if (cache_entries_.empty()) {
            return false;  // Can't free enough memory
        }
        EvictLRU(0);
    }
    
    return true;
}

} // namespace RawrXD