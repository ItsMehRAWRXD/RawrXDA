// ============================================================================
// PrefixCache.h — Prefix Cache for Ghost Text Latency Masking
// ============================================================================
// Provides speculative prefetch and prefix reuse to make latency invisible.
//
// Design:
// - Cache key: file hash + line hash + cursor position
// - Cache entry: prefix, suggestion, logits, timestamp
// - LRU eviction for memory bounds
// - Idle detection for speculative prefetch trigger
//
// Integration:
// - GhostTextContextSubscriber checks cache before requesting inference
// - Idle timer in Win32IDE_Core triggers prefetch
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace RawrXD {

// ---------------------------------------------------------------------------
// PrefixCacheKey — Hash key for cache lookup
// ---------------------------------------------------------------------------
struct PrefixCacheKey {
    uint64_t fileHash;      // Hash of file path (FNV-1a)
    uint32_t lineHash;      // Hash of current line content
    uint32_t cursorPos;     // Cursor position in line
    uint32_t languageId;   // Language identifier hash
    
    bool operator==(const PrefixCacheKey& other) const {
        return fileHash == other.fileHash &&
               lineHash == other.lineHash &&
               cursorPos == other.cursorPos &&
               languageId == other.languageId;
    }
};

// ---------------------------------------------------------------------------
// PrefixCacheKeyHash — Hash function for unordered_map
// ---------------------------------------------------------------------------
struct PrefixCacheKeyHash {
    size_t operator()(const PrefixCacheKey& key) const {
        // Combine hashes with prime multipliers
        size_t h = key.fileHash;
        h ^= key.lineHash + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= key.cursorPos + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= key.languageId + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// ---------------------------------------------------------------------------
// PrefixCacheEntry — Cached completion data
// ---------------------------------------------------------------------------
struct PrefixCacheEntry {
    std::string prefix;              // The prefix that triggered this
    std::string suggestion;          // The full suggestion text
    std::vector<float> logits;      // Cached logits for continuation (optional)
    std::chrono::steady_clock::time_point timestamp;  // For LRU eviction
    uint64_t hitCount;               // Popularity metric
    float confidence;                // Model confidence score
    bool isStreaming;                // True if still receiving tokens
    bool isComplete;                 // True if full suggestion received
    
    // Age in milliseconds
    int64_t AgeMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - timestamp).count();
    }
};

// ---------------------------------------------------------------------------
// PrefetchState — State machine for speculative prefetch
// ---------------------------------------------------------------------------
enum class PrefetchState {
    IDLE,           // No prefetch pending
    PENDING,        // Queued but not started
    IN_PROGRESS,    // Inference running
    VALID,          // Ready to display
    STALE           // User typed, discard
};

// ---------------------------------------------------------------------------
// PrefetchRequest — A speculative prefetch task
// ---------------------------------------------------------------------------
struct PrefetchRequest {
    PrefixCacheKey key;
    std::string prefix;
    std::string filePath;
    std::string languageId;
    std::vector<std::string> symbols;
    PrefetchState state;
    std::chrono::steady_clock::time_point startTime;
    uint64_t sequenceId;            // For cancellation
};

// ---------------------------------------------------------------------------
// PrefixCache — LRU cache with prefetch queue
// ---------------------------------------------------------------------------
class PrefixCache {
public:
    PrefixCache(size_t maxEntries = 256, int64_t maxAgeMs = 30000);
    ~PrefixCache();
    
    // Core cache operations
    bool Lookup(const PrefixCacheKey& key, PrefixCacheEntry& entry);
    void Store(const PrefixCacheKey& key, const PrefixCacheEntry& entry);
    void Invalidate(const PrefixCacheKey& key);
    void Clear();
    
    // Prefetch queue management
    uint64_t QueuePrefetch(const std::string& prefix,
                          const std::string& filePath,
                          const std::string& languageId,
                          const std::vector<std::string>& symbols);
    
    void MarkPrefetchComplete(uint64_t sequenceId, const std::string& suggestion);
    void MarkPrefetchStale(uint64_t sequenceId);
    void CancelPendingPrefetches();
    
    // Check if a valid prefetch is ready
    bool HasValidPrefetch(const PrefixCacheKey& key) const;
    bool GetPrefetchResult(const PrefixCacheKey& key, std::string& suggestion);
    
    // Statistics
    size_t Size() const;
    size_t HitCount() const { return m_hitCount; }
    size_t MissCount() const { return m_missCount; }
    double HitRate() const;
    
    // Idle detection for speculative prefetch
    void OnKeystroke();
    void OnCursorMove();
    bool IsIdle(uint32_t thresholdMs = 150) const;
    
    // Hash utilities
    static uint64_t HashFile(const std::string& filePath);
    static uint32_t HashLine(const std::string& line);
    static uint32_t HashLanguage(const std::string& languageId);
    
private:
    void EvictOldest();
    void PruneExpired();
    
    std::unordered_map<PrefixCacheKey, PrefixCacheEntry, PrefixCacheKeyHash> m_cache;
    std::vector<PrefetchRequest> m_prefetchQueue;
    
    mutable std::mutex m_mutex;
    size_t m_maxEntries;
    int64_t m_maxAgeMs;
    
    size_t m_hitCount;
    size_t m_missCount;
    uint64_t m_nextSequenceId;
    
    // Idle detection
    std::chrono::steady_clock::time_point m_lastKeystroke;
    std::chrono::steady_clock::time_point m_lastCursorMove;
};

// ---------------------------------------------------------------------------
// Global cache instance (singleton pattern for simplicity)
// ---------------------------------------------------------------------------
PrefixCache& GetGlobalPrefixCache();

} // namespace RawrXD