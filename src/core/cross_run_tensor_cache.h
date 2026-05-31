// ============================================================================
// cross_run_tensor_cache.h — Cross-Run Tensor Slice Cache
// ============================================================================
// Caches "good" tensor computation results across iterative inference passes.
// Instead of recomputing stable layers from scratch each pass, we cache their
// outputs and reuse them. This is the key efficiency optimization that makes
// iterative inference practical: each pass only computes NEW work.
//
// Cache eviction strategies:
//   1. LRU — Least recently used (default)
//   2. Contribution-based — Evict lowest-contribution layers first
//   3. Staleness — Evict entries that haven't been validated recently
//   4. Memory pressure — Aggressive eviction when VRAM is tight
//
// Cache validation:
//   - Entries are invalidated when input context changes significantly
//   - Entries are invalidated when the traversal strategy changes
//   - Stale entries are re-validated via quick probe passes
//
// Design:
//   - No exceptions — PatchResult-style returns
//   - No std::function — raw function pointers
//   - Thread-safe singleton
//   - Memory-mapped backing for large caches
//   - Cache keys include: layer index + context hash + strategy hash
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef CROSS_RUN_TENSOR_CACHE_H
#define CROSS_RUN_TENSOR_CACHE_H

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

// ============================================================================
// Enums
// ============================================================================

// CacheEvictionPolicy — How to choose which entries to evict
enum class CacheEvictionPolicy : uint8_t {
    LRU                = 0,   // Least recently used
    ContributionBased  = 1,   // Lowest contribution score first
    Staleness          = 2,   // Oldest unvalidated first
    MemoryPressure     = 3,   // Most aggressive: evict largest first
    Custom             = 4    // User-defined via function pointer
};

// CacheEntryState — Lifecycle state of a cached tensor slice
enum class CacheEntryState : uint8_t {
    Empty       = 0,   // Slot is unused
    Valid       = 1,   // Entry is current and usable
    Stale       = 2,   // Entry may be outdated (needs re-validation)
    Locked      = 3,   // Entry is being read/written (busy)
    Invalidated = 4    // Entry has been explicitly invalidated
};

// CacheEvent — Events emitted by the cache
enum class CacheEvent : uint8_t {
    Hit        = 0,
    Miss       = 1,
    Insert     = 2,
    Evict      = 3,
    Invalidate = 4,
    Validate   = 5,
    Flush      = 6,
    Resize     = 7
};

// ============================================================================
// TensorSliceKey — Unique identifier for a cached tensor slice
// ============================================================================
struct TensorSliceKey {
    uint32_t    layerIndex;        // Which layer produced this
    uint64_t    contextHash;       // Hash of the input context that produced this
    uint64_t    strategyHash;      // Hash of the traversal strategy used
    uint32_t    passNumber;        // Which pass produced this (for staleness)

    bool operator==(const TensorSliceKey& other) const {
        return layerIndex   == other.layerIndex
            && contextHash  == other.contextHash
            && strategyHash == other.strategyHash;
    }

    // Simple hash for map lookup
    uint64_t hash() const {
        uint64_t h = layerIndex;
        h = h * 6364136223846793005ULL + contextHash;
        h = h * 6364136223846793005ULL + strategyHash;
        return h;
    }
};

// ============================================================================
// CachedTensorSlice — A single cached tensor computation result
// ============================================================================
struct CachedTensorSlice {
    TensorSliceKey   key;
    CacheEntryState  state;

    // The cached data
    void*            data;           // Pointer to cached tensor data (owned)
    size_t           dataSize;       // Size in bytes
    uint32_t         tensorDim[4];   // Dimensions (batch, seq, hidden, heads)
    uint8_t          dtype;          // Data type: 0=fp32, 1=fp16, 2=bf16, 3=int8, 4=int4

    // Metadata
    float            contributionScore;  // From LayerContributionScorer
    double           computeTimeMs;      // How long this took to compute originally
    uint64_t         lastAccessTime;     // For LRU
    uint32_t         accessCount;        // How many times this was reused
    uint32_t         validationCount;    // How many times this was re-validated

    // Validation
    float            validationDelta;    // Last re-validation delta (0 = perfect match)
    bool             needsRevalidation;
};

// ============================================================================
// CacheConfig — Configuration for the tensor cache
// ============================================================================
struct CacheConfig {
    // Maximum cache size in bytes
    size_t              maxCacheBytes        = 2ULL * 1024 * 1024 * 1024;  // 2 GB default

    // Maximum number of entries
    uint32_t            maxEntries           = 4096;

    // Eviction policy
    CacheEvictionPolicy evictionPolicy       = CacheEvictionPolicy::LRU;

    // Staleness threshold: entries older than this many passes are stale
    uint32_t            stalenessThreshold   = 5;

    // Re-validation delta threshold: if re-validation shows delta > this, invalidate
    float               revalidationThreshold = 0.15f;

    // Memory pressure threshold: start evicting when cache is this full
    float               pressureThreshold    = 0.90f;

    // Whether to use memory-mapped backing store
    bool                useMmapBacking       = false;

    // Path for mmap backing store (if enabled)
    char                mmapPath[512]        = "";

    // Pre-warm: on initialization, pre-allocate this many empty slots
    uint32_t            prewarmSlots         = 256;
};

// ============================================================================
// Cache event callback (function pointer, NOT std::function)
// ============================================================================
typedef void (*CacheEventCallback)(
    CacheEvent         event,
    const TensorSliceKey* key,
    size_t             entrySize,
    void*              userData
);

// Custom eviction callback (function pointer, NOT std::function)
typedef uint32_t (*CustomEvictionFn)(
    const CachedTensorSlice* entries,
    uint32_t                 entryCount,
    void*                    userData
);

// ============================================================================
// CrossRunTensorCache — Main class (singleton)
// ============================================================================
class CrossRunTensorCache {
public:
    static CrossRunTensorCache& instance();

    // ----- Lifecycle -----
    PatchResult initialize(const CacheConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- Cache Operations -----
    // Look up a cached tensor slice. Returns pointer to data if found (cache HIT).
    // Caller must NOT free the returned pointer.
    PatchResult lookup(const TensorSliceKey& key, CachedTensorSlice* outEntry);

    // Insert or update a tensor slice in the cache.
    // The cache takes ownership of `data` — caller must NOT free it after this call.
    PatchResult insert(const TensorSliceKey& key, void* data, size_t dataSize,
                        const uint32_t dims[4], uint8_t dtype,
                        float contributionScore, double computeTimeMs);

    // Invalidate a specific entry
    PatchResult invalidate(const TensorSliceKey& key);

    // Invalidate all entries for a specific layer
    PatchResult invalidateLayer(uint32_t layerIndex);

    // Invalidate all entries for a specific context hash
    PatchResult invalidateContext(uint64_t contextHash);

    // Mark an entry as needing re-validation
    PatchResult markStale(const TensorSliceKey& key);

    // Re-validate an entry by comparing with fresh computation
    PatchResult revalidate(const TensorSliceKey& key, const void* freshData,
                            size_t freshSize, float* outDelta);

    // ----- Eviction -----
    PatchResult evict(uint32_t count);  // Evict `count` entries per policy
    PatchResult flush();                // Evict ALL entries

    // Set custom eviction function
    PatchResult setCustomEviction(CustomEvictionFn fn, void* userData);

    // ----- Queries -----
    uint32_t    entryCount() const;
    size_t      usedBytes() const;
    size_t      maxBytes() const;
    float       hitRate() const;
    float       fillRatio() const;

    // Get all entries for a given layer
    PatchResult getLayerEntries(uint32_t layerIndex,
                                 CachedTensorSlice* outBuf, uint32_t* outCount,
                                 uint32_t maxCount) const;

    // ----- Callback -----
    PatchResult registerCallback(CacheEventCallback cb, void* userData);
    PatchResult clearCallback();

    // ----- Statistics -----
    struct Stats {
        std::atomic<uint64_t> hits{0};
        std::atomic<uint64_t> misses{0};
        std::atomic<uint64_t> inserts{0};
        std::atomic<uint64_t> evictions{0};
        std::atomic<uint64_t> invalidations{0};
        std::atomic<uint64_t> revalidations{0};
        std::atomic<uint64_t> revalidationFailures{0};
        std::atomic<uint64_t> bytesWritten{0};
        std::atomic<uint64_t> bytesRead{0};
        std::atomic<uint64_t> bytesEvicted{0};
    };

    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    CrossRunTensorCache();
    ~CrossRunTensorCache();
    CrossRunTensorCache(const CrossRunTensorCache&) = delete;
    CrossRunTensorCache& operator=(const CrossRunTensorCache&) = delete;

    // ----- Internal -----
    int findEntry(const TensorSliceKey& key) const;   // Returns index or -1
    int findEvictionCandidate() const;                 // Per policy
    int findEvictionCandidateLRU() const;
    int findEvictionCandidateContribution() const;
    int findEvictionCandidateStaleness() const;
    int findEvictionCandidateMemPressure() const;
    void freeEntryData(uint32_t index);
    void notifyCallback(CacheEvent event, const TensorSliceKey* key, size_t size);

    // ----- Members -----
    std::atomic<bool>              m_initialized{false};
    mutable std::mutex             m_mutex;

    CacheConfig                    m_config;

    // Entry storage (flat array for cache-friendly access)
    std::vector<CachedTensorSlice> m_entries;
    uint32_t                       m_entryCount;
    std::atomic<size_t>            m_usedBytes{0};

    // Callback
    CacheEventCallback             m_callback;
    void*                          m_callbackUserData;

    // Custom eviction
    CustomEvictionFn               m_customEvictFn;
    void*                          m_customEvictUserData;

    Stats                          m_stats;
};

#endif // CROSS_RUN_TENSOR_CACHE_H
