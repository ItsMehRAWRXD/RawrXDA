// ============================================================================
// vision_embedding_cache.hpp — Hash-Based Vision Embedding Cache with LRU
// ============================================================================
// Caches computed VisionEmbeddings keyed by image content hash.
// Avoids redundant re-encoding when the same image is processed multiple times
// (e.g., re-opening a file, multiple references to the same screenshot).
//
// Features:
//   - FNV-1a content hash of pixel data (fast, collision-resistant for images)
//   - LRU eviction with configurable max entries and memory budget
//   - Thread-safe with fine-grained locking
//   - Partial match support (image crops share parent hash chain)
//   - Statistics: hit rate, eviction rate, cache memory usage
//   - TTL-based expiry for stale entries
//
// Integration: Called by VisionEncoder before encode() to check cache,
//              and after encode() to insert results.
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vision_encoder.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Cache Configuration
// ============================================================================
struct EmbeddingCacheConfig {
    uint32_t maxEntries;           // Maximum cached embeddings (LRU evicts oldest)
    uint64_t maxMemoryBytes;       // Maximum cache memory budget
    uint32_t ttlSeconds;           // Time-to-live for cache entries (0 = no expiry)
    bool     enablePartialMatch;   // Allow cache hits on cropped/resized versions
    float    partialMatchThreshold;// Min hash similarity for partial match [0, 1]

    EmbeddingCacheConfig()
        : maxEntries(256)
        , maxMemoryBytes(256 * 1024 * 1024)  // 256 MB
        , ttlSeconds(3600)                    // 1 hour
        , enablePartialMatch(false)
        , partialMatchThreshold(0.9f)
    {}
};

// ============================================================================
// Cache Entry
// ============================================================================
struct CacheEntry {
    uint64_t        imageHash;       // FNV-1a hash of image pixel data
    VisionEmbedding embedding;       // Cached encoding result
    uint64_t        insertTimeMs;    // Timestamp when inserted (epoch ms)
    uint64_t        lastAccessMs;    // Timestamp of last access
    uint32_t        accessCount;     // Number of cache hits
    uint32_t        imageWidth;      // Original image dimensions (for partial match)
    uint32_t        imageHeight;
    uint64_t        memorySizeBytes; // Estimated memory footprint of this entry

    CacheEntry()
        : imageHash(0), insertTimeMs(0), lastAccessMs(0),
          accessCount(0), imageWidth(0), imageHeight(0), memorySizeBytes(0)
    {}
};

// ============================================================================
// Cache Statistics
// ============================================================================
struct CacheStats {
    uint64_t totalLookups;
    uint64_t totalHits;
    uint64_t totalMisses;
    uint64_t totalInserts;
    uint64_t totalEvictions;
    uint64_t totalExpired;       // TTL-based removals
    uint32_t currentEntries;
    uint64_t currentMemoryBytes;
    double   hitRate;            // hits / lookups
    double   avgAccessCount;     // Average hits per entry
};

// ============================================================================
// VisionEmbeddingCache — The cache engine
// ============================================================================
class VisionEmbeddingCache {
public:
    static VisionEmbeddingCache& instance();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    void configure(const EmbeddingCacheConfig& config);
    const EmbeddingCacheConfig& getConfig() const;

    // -----------------------------------------------------------------------
    // Core Cache Operations
    // -----------------------------------------------------------------------

    // Compute content hash for an image buffer (FNV-1a).
    // This is the cache key.
    uint64_t computeImageHash(const ImageBuffer& image) const;

    // Look up a cached embedding by image hash.
    // Returns true if found (and fills output).
    bool lookup(uint64_t hash, VisionEmbedding& output);

    // Look up by image buffer (computes hash internally).
    bool lookupImage(const ImageBuffer& image, VisionEmbedding& output);

    // Insert a new embedding into the cache.
    // May evict LRU entries if over capacity.
    VisionResult insert(uint64_t hash, const VisionEmbedding& embedding,
                        uint32_t imageWidth = 0, uint32_t imageHeight = 0);

    // Insert by image buffer (computes hash internally).
    VisionResult insertImage(const ImageBuffer& image,
                             const VisionEmbedding& embedding);

    // -----------------------------------------------------------------------
    // Cache Management
    // -----------------------------------------------------------------------

    // Remove a specific entry.
    bool remove(uint64_t hash);

    // Clear all entries.
    void clear();

    // Evict expired entries (TTL check).
    uint32_t evictExpired();

    // Manually trigger LRU eviction to fit within memory budget.
    uint32_t evictToFit();

    // -----------------------------------------------------------------------
    // Partial Match (approximate cache hit)
    // -----------------------------------------------------------------------

    // Find the closest cached embedding to a given hash.
    // Uses perceptual hash distance for near-duplicate detection.
    bool findClosest(uint64_t hash, float threshold,
                     VisionEmbedding& output, float& similarity);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    CacheStats getStats() const;
    void resetStats();

private:
    VisionEmbeddingCache();
    ~VisionEmbeddingCache() = default;
    VisionEmbeddingCache(const VisionEmbeddingCache&) = delete;
    VisionEmbeddingCache& operator=(const VisionEmbeddingCache&) = delete;

    // Internal helpers
    uint64_t currentTimeMs() const;
    uint64_t estimateEntrySize(const VisionEmbedding& emb) const;
    void evictLRU();
    void touchEntry(uint64_t hash);
    bool isExpired(const CacheEntry& entry) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mutex_;
    EmbeddingCacheConfig config_;

    // Hash map: imageHash → CacheEntry
    std::unordered_map<uint64_t, CacheEntry> entries_;

    // LRU list: front = most recently used, back = least recently used
    std::list<uint64_t> lruOrder_;
    std::unordered_map<uint64_t, std::list<uint64_t>::iterator> lruMap_;

    // Current memory usage
    uint64_t currentMemoryBytes_ = 0;

    // Statistics (atomic for lock-free reads)
    std::atomic<uint64_t> totalLookups_{0};
    std::atomic<uint64_t> totalHits_{0};
    std::atomic<uint64_t> totalMisses_{0};
    std::atomic<uint64_t> totalInserts_{0};
    std::atomic<uint64_t> totalEvictions_{0};
    std::atomic<uint64_t> totalExpired_{0};
};

} // namespace Vision
} // namespace RawrXD
