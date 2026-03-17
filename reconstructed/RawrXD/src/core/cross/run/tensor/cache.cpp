// ============================================================================
// cross_run_tensor_cache.cpp — Cross-Run Tensor Slice Cache Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "cross_run_tensor_cache.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <cmath>

// ============================================================================
// Singleton
// ============================================================================

CrossRunTensorCache& CrossRunTensorCache::instance() {
    static CrossRunTensorCache s_instance;
    return s_instance;
}

CrossRunTensorCache::CrossRunTensorCache()
    : m_entryCount(0)
    , m_callback(nullptr)
    , m_callbackUserData(nullptr)
    , m_customEvictFn(nullptr)
    , m_customEvictUserData(nullptr)
{}

CrossRunTensorCache::~CrossRunTensorCache() {
    shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult CrossRunTensorCache::initialize(const CacheConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("CrossRunTensorCache already initialized", -1);
    }

    m_config     = config;
    m_entryCount = 0;
    m_usedBytes.store(0, std::memory_order_release);

    // Pre-allocate empty slots
    m_entries.resize(config.maxEntries);
    for (auto& entry : m_entries) {
        std::memset(&entry, 0, sizeof(CachedTensorSlice));
        entry.state = CacheEntryState::Empty;
        entry.data  = nullptr;
    }

    m_callback            = nullptr;
    m_callbackUserData    = nullptr;
    m_customEvictFn       = nullptr;
    m_customEvictUserData = nullptr;

    m_initialized.store(true, std::memory_order_release);
    return PatchResult::ok("CrossRunTensorCache initialized");
}

void CrossRunTensorCache::shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Free all owned data
    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        freeEntryData(i);
    }
    m_entries.clear();
    m_entryCount = 0;
    m_usedBytes.store(0, std::memory_order_release);

    m_initialized.store(false, std::memory_order_release);
}

// ============================================================================
// Cache Operations
// ============================================================================

PatchResult CrossRunTensorCache::lookup(const TensorSliceKey& key, CachedTensorSlice* outEntry) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!outEntry) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findEntry(key);
    if (idx < 0) {
        m_stats.misses.fetch_add(1, std::memory_order_relaxed);
        notifyCallback(CacheEvent::Miss, &key, 0);
        return PatchResult::error("Cache miss", -3);
    }

    CachedTensorSlice& entry = m_entries[idx];
    if (entry.state != CacheEntryState::Valid && entry.state != CacheEntryState::Stale) {
        m_stats.misses.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("Entry not valid", -4);
    }

    // Update access metadata
    entry.lastAccessTime = GetTickCount64();
    entry.accessCount++;

    *outEntry = entry;

    m_stats.hits.fetch_add(1, std::memory_order_relaxed);
    m_stats.bytesRead.fetch_add(entry.dataSize, std::memory_order_relaxed);
    notifyCallback(CacheEvent::Hit, &key, entry.dataSize);

    return PatchResult::ok("Cache hit");
}

PatchResult CrossRunTensorCache::insert(const TensorSliceKey& key, void* data, size_t dataSize,
                                          const uint32_t dims[4], uint8_t dtype,
                                          float contributionScore, double computeTimeMs) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }
    if (!data || dataSize == 0) {
        return PatchResult::error("Invalid data", -2);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if we need to evict
    size_t currentUsed = m_usedBytes.load(std::memory_order_relaxed);
    while (currentUsed + dataSize > m_config.maxCacheBytes || m_entryCount >= m_config.maxEntries) {
        int victim = findEvictionCandidate();
        if (victim < 0) {
            return PatchResult::error("Cannot evict: cache full with no candidates", -5);
        }
        size_t victimSize = m_entries[victim].dataSize;
        notifyCallback(CacheEvent::Evict, &m_entries[victim].key, victimSize);
        freeEntryData(victim);
        m_entries[victim].state = CacheEntryState::Empty;
        m_entryCount--;
        m_stats.evictions.fetch_add(1, std::memory_order_relaxed);
        m_stats.bytesEvicted.fetch_add(victimSize, std::memory_order_relaxed);
        currentUsed = m_usedBytes.load(std::memory_order_relaxed);
    }

    // Find existing entry or empty slot
    int existingIdx = findEntry(key);
    int targetIdx;

    if (existingIdx >= 0) {
        // Update existing entry
        freeEntryData(existingIdx);
        targetIdx = existingIdx;
    } else {
        // Find empty slot
        targetIdx = -1;
        for (uint32_t i = 0; i < m_entries.size(); ++i) {
            if (m_entries[i].state == CacheEntryState::Empty) {
                targetIdx = static_cast<int>(i);
                break;
            }
        }
        if (targetIdx < 0) {
            return PatchResult::error("No empty slots available", -6);
        }
        m_entryCount++;
    }

    // Fill entry
    CachedTensorSlice& entry = m_entries[targetIdx];
    entry.key               = key;
    entry.state             = CacheEntryState::Valid;
    entry.data              = data;
    entry.dataSize          = dataSize;
    std::memcpy(entry.tensorDim, dims, sizeof(uint32_t) * 4);
    entry.dtype             = dtype;
    entry.contributionScore = contributionScore;
    entry.computeTimeMs     = computeTimeMs;
    entry.lastAccessTime    = GetTickCount64();
    entry.accessCount       = 1;
    entry.validationCount   = 0;
    entry.validationDelta   = 0.0f;
    entry.needsRevalidation = false;

    m_usedBytes.fetch_add(dataSize, std::memory_order_relaxed);
    m_stats.inserts.fetch_add(1, std::memory_order_relaxed);
    m_stats.bytesWritten.fetch_add(dataSize, std::memory_order_relaxed);
    notifyCallback(CacheEvent::Insert, &key, dataSize);

    return PatchResult::ok("Inserted into cache");
}

PatchResult CrossRunTensorCache::invalidate(const TensorSliceKey& key) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findEntry(key);
    if (idx < 0) return PatchResult::ok("Entry not found (no-op)");

    freeEntryData(idx);
    m_entries[idx].state = CacheEntryState::Invalidated;
    m_entryCount--;

    m_stats.invalidations.fetch_add(1, std::memory_order_relaxed);
    notifyCallback(CacheEvent::Invalidate, &key, 0);
    return PatchResult::ok("Entry invalidated");
}

PatchResult CrossRunTensorCache::invalidateLayer(uint32_t layerIndex) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state != CacheEntryState::Empty &&
            m_entries[i].key.layerIndex == layerIndex) {
            freeEntryData(i);
            m_entries[i].state = CacheEntryState::Invalidated;
            m_entryCount--;
            count++;
        }
    }

    m_stats.invalidations.fetch_add(count, std::memory_order_relaxed);
    return PatchResult::ok("Layer entries invalidated");
}

PatchResult CrossRunTensorCache::invalidateContext(uint64_t contextHash) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return PatchResult::error("Not initialized", -1);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state != CacheEntryState::Empty &&
            m_entries[i].key.contextHash == contextHash) {
            freeEntryData(i);
            m_entries[i].state = CacheEntryState::Invalidated;
            m_entryCount--;
            count++;
        }
    }

    m_stats.invalidations.fetch_add(count, std::memory_order_relaxed);
    return PatchResult::ok("Context entries invalidated");
}

PatchResult CrossRunTensorCache::markStale(const TensorSliceKey& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int idx = findEntry(key);
    if (idx < 0) return PatchResult::ok("Entry not found");

    m_entries[idx].state = CacheEntryState::Stale;
    m_entries[idx].needsRevalidation = true;
    return PatchResult::ok("Entry marked stale");
}

PatchResult CrossRunTensorCache::revalidate(const TensorSliceKey& key, const void* freshData,
                                              size_t freshSize, float* outDelta) {
    if (!freshData || freshSize == 0) return PatchResult::error("Invalid fresh data", -2);

    std::lock_guard<std::mutex> lock(m_mutex);

    int idx = findEntry(key);
    if (idx < 0) return PatchResult::error("Entry not found", -3);

    CachedTensorSlice& entry = m_entries[idx];
    if (!entry.data) return PatchResult::error("Entry has no data", -4);

    // Compare cached data with fresh data
    // Simple byte-level comparison (L2 distance of float arrays)
    size_t compareSize = std::min(entry.dataSize, freshSize);
    uint32_t floatCount = static_cast<uint32_t>(compareSize / sizeof(float));

    double sumSqDiff = 0.0;
    double sumSqOrig = 0.0;
    const float* cached = reinterpret_cast<const float*>(entry.data);
    const float* fresh  = reinterpret_cast<const float*>(freshData);

    for (uint32_t i = 0; i < floatCount; ++i) {
        double diff = static_cast<double>(cached[i]) - static_cast<double>(fresh[i]);
        sumSqDiff += diff * diff;
        sumSqOrig += static_cast<double>(cached[i]) * static_cast<double>(cached[i]);
    }

    float delta = 0.0f;
    if (sumSqOrig > 1e-10) {
        delta = static_cast<float>(std::sqrt(sumSqDiff) / std::sqrt(sumSqOrig));
    }

    entry.validationDelta = delta;
    entry.validationCount++;
    entry.needsRevalidation = false;

    if (delta < m_config.revalidationThreshold) {
        entry.state = CacheEntryState::Valid;
        m_stats.revalidations.fetch_add(1, std::memory_order_relaxed);
    } else {
        // Delta too large: invalidate
        entry.state = CacheEntryState::Invalidated;
        m_stats.revalidationFailures.fetch_add(1, std::memory_order_relaxed);
    }

    if (outDelta) *outDelta = delta;
    notifyCallback(CacheEvent::Validate, &key, entry.dataSize);
    return PatchResult::ok("Revalidation complete");
}

// ============================================================================
// Eviction
// ============================================================================

PatchResult CrossRunTensorCache::evict(uint32_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t evicted = 0;

    for (uint32_t i = 0; i < count && m_entryCount > 0; ++i) {
        int victim = findEvictionCandidate();
        if (victim < 0) break;

        size_t victimSize = m_entries[victim].dataSize;
        notifyCallback(CacheEvent::Evict, &m_entries[victim].key, victimSize);
        freeEntryData(victim);
        m_entries[victim].state = CacheEntryState::Empty;
        m_entryCount--;
        evicted++;
        m_stats.evictions.fetch_add(1, std::memory_order_relaxed);
        m_stats.bytesEvicted.fetch_add(victimSize, std::memory_order_relaxed);
    }

    return PatchResult::ok("Eviction complete");
}

PatchResult CrossRunTensorCache::flush() {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state != CacheEntryState::Empty) {
            freeEntryData(i);
            m_entries[i].state = CacheEntryState::Empty;
        }
    }
    m_entryCount = 0;
    m_usedBytes.store(0, std::memory_order_release);

    notifyCallback(CacheEvent::Flush, nullptr, 0);
    return PatchResult::ok("Cache flushed");
}

PatchResult CrossRunTensorCache::setCustomEviction(CustomEvictionFn fn, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_customEvictFn       = fn;
    m_customEvictUserData = userData;
    return PatchResult::ok("Custom eviction set");
}

// ============================================================================
// Internal Eviction Candidates
// ============================================================================

int CrossRunTensorCache::findEntry(const TensorSliceKey& key) const {
    uint64_t keyHash = key.hash();
    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state != CacheEntryState::Empty &&
            m_entries[i].state != CacheEntryState::Invalidated &&
            m_entries[i].key == key) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int CrossRunTensorCache::findEvictionCandidate() const {
    switch (m_config.evictionPolicy) {
        case CacheEvictionPolicy::LRU:               return findEvictionCandidateLRU();
        case CacheEvictionPolicy::ContributionBased:  return findEvictionCandidateContribution();
        case CacheEvictionPolicy::Staleness:          return findEvictionCandidateStaleness();
        case CacheEvictionPolicy::MemoryPressure:     return findEvictionCandidateMemPressure();
        case CacheEvictionPolicy::Custom: {
            if (m_customEvictFn && !m_entries.empty()) {
                return static_cast<int>(
                    m_customEvictFn(m_entries.data(),
                                     static_cast<uint32_t>(m_entries.size()),
                                     m_customEvictUserData));
            }
            return findEvictionCandidateLRU();
        }
        default: return findEvictionCandidateLRU();
    }
}

int CrossRunTensorCache::findEvictionCandidateLRU() const {
    int bestIdx = -1;
    uint64_t oldestTime = UINT64_MAX;

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == CacheEntryState::Valid ||
            m_entries[i].state == CacheEntryState::Stale) {
            if (m_entries[i].lastAccessTime < oldestTime) {
                oldestTime = m_entries[i].lastAccessTime;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    return bestIdx;
}

int CrossRunTensorCache::findEvictionCandidateContribution() const {
    int bestIdx = -1;
    float lowestScore = 2.0f;

    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == CacheEntryState::Valid ||
            m_entries[i].state == CacheEntryState::Stale) {
            if (m_entries[i].contributionScore < lowestScore) {
                lowestScore = m_entries[i].contributionScore;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    return bestIdx;
}

int CrossRunTensorCache::findEvictionCandidateStaleness() const {
    int bestIdx = -1;
    uint64_t oldestAccess = UINT64_MAX;

    // Prefer stale entries over valid ones
    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == CacheEntryState::Stale) {
            if (m_entries[i].lastAccessTime < oldestAccess) {
                oldestAccess = m_entries[i].lastAccessTime;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    if (bestIdx >= 0) return bestIdx;

    // Fallback to LRU
    return findEvictionCandidateLRU();
}

int CrossRunTensorCache::findEvictionCandidateMemPressure() const {
    int bestIdx = -1;
    size_t largestSize = 0;

    // Evict the largest entry first
    for (uint32_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].state == CacheEntryState::Valid ||
            m_entries[i].state == CacheEntryState::Stale) {
            if (m_entries[i].dataSize > largestSize) {
                largestSize = m_entries[i].dataSize;
                bestIdx = static_cast<int>(i);
            }
        }
    }
    return bestIdx;
}

void CrossRunTensorCache::freeEntryData(uint32_t index) {
    if (index < m_entries.size() && m_entries[index].data) {
        size_t freed = m_entries[index].dataSize;
        free(m_entries[index].data);
        m_entries[index].data     = nullptr;
        m_entries[index].dataSize = 0;
        // Atomic subtract
        size_t current = m_usedBytes.load(std::memory_order_relaxed);
        if (current >= freed) {
            m_usedBytes.fetch_sub(freed, std::memory_order_relaxed);
        } else {
            m_usedBytes.store(0, std::memory_order_relaxed);
        }
    }
}

void CrossRunTensorCache::notifyCallback(CacheEvent event, const TensorSliceKey* key, size_t size) {
    if (m_callback) {
        m_callback(event, key, size, m_callbackUserData);
    }
}

// ============================================================================
// Queries
// ============================================================================

uint32_t CrossRunTensorCache::entryCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entryCount;
}

size_t CrossRunTensorCache::usedBytes() const {
    return m_usedBytes.load(std::memory_order_relaxed);
}

size_t CrossRunTensorCache::maxBytes() const {
    return m_config.maxCacheBytes;
}

float CrossRunTensorCache::hitRate() const {
    uint64_t h = m_stats.hits.load(std::memory_order_relaxed);
    uint64_t m = m_stats.misses.load(std::memory_order_relaxed);
    uint64_t total = h + m;
    if (total == 0) return 0.0f;
    return static_cast<float>(h) / total;
}

float CrossRunTensorCache::fillRatio() const {
    size_t used = m_usedBytes.load(std::memory_order_relaxed);
    if (m_config.maxCacheBytes == 0) return 0.0f;
    return static_cast<float>(used) / m_config.maxCacheBytes;
}

PatchResult CrossRunTensorCache::getLayerEntries(uint32_t layerIndex,
                                                   CachedTensorSlice* outBuf, uint32_t* outCount,
                                                   uint32_t maxCount) const {
    if (!outBuf || !outCount) return PatchResult::error("Null output pointer", -2);

    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t found = 0;

    for (uint32_t i = 0; i < m_entries.size() && found < maxCount; ++i) {
        if (m_entries[i].state != CacheEntryState::Empty &&
            m_entries[i].state != CacheEntryState::Invalidated &&
            m_entries[i].key.layerIndex == layerIndex) {
            outBuf[found++] = m_entries[i];
        }
    }

    *outCount = found;
    return PatchResult::ok("Layer entries retrieved");
}

// ============================================================================
// Callback
// ============================================================================

PatchResult CrossRunTensorCache::registerCallback(CacheEventCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = cb;
    m_callbackUserData = userData;
    return PatchResult::ok("Callback registered");
}

PatchResult CrossRunTensorCache::clearCallback() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_callback         = nullptr;
    m_callbackUserData = nullptr;
    return PatchResult::ok("Callback cleared");
}

// ============================================================================
// Statistics
// ============================================================================

void CrossRunTensorCache::resetStats() {
    m_stats.hits.store(0, std::memory_order_relaxed);
    m_stats.misses.store(0, std::memory_order_relaxed);
    m_stats.inserts.store(0, std::memory_order_relaxed);
    m_stats.evictions.store(0, std::memory_order_relaxed);
    m_stats.invalidations.store(0, std::memory_order_relaxed);
    m_stats.revalidations.store(0, std::memory_order_relaxed);
    m_stats.revalidationFailures.store(0, std::memory_order_relaxed);
    m_stats.bytesWritten.store(0, std::memory_order_relaxed);
    m_stats.bytesRead.store(0, std::memory_order_relaxed);
    m_stats.bytesEvicted.store(0, std::memory_order_relaxed);
}
