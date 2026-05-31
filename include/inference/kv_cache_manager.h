#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <map>

namespace RawrXD::Inference {

// Handle to allocated KV cache
struct KVCacheHandle {
    std::vector<size_t> pageIds;
    uint32_t layerCount = 0;
    uint32_t headCount = 0;
    uint32_t headDim = 0;
    size_t tokenCount = 0;
    bool valid = false;
};

// Page metadata
struct KVPage {
    size_t id = 0;
    bool inUse = false;
    size_t refCount = 0;
};

class KVCacheManager {
public:
    KVCacheManager(size_t pageSize = 256, size_t maxPages = 1024);
    ~KVCacheManager() = default;
    
    // Allocation
    KVCacheHandle allocate(size_t numTokens, uint32_t numLayers, uint32_t numHeads, uint32_t headDim);
    void release(KVCacheHandle& handle);
    
    // Page management
    bool growAllocation(KVCacheHandle& handle, size_t additionalTokens);
    bool shrinkAllocation(KVCacheHandle& handle, size_t newTokenCount);
    
    // Query
    size_t getFreePages() const;
    size_t getTotalPages() const;
    size_t getUsedPages() const;
    
    // Statistics
    size_t getTotalMemoryBytes() const;
    double getUtilizationRatio() const;
    
    // Cache eviction (for memory pressure)
    void evictLeastRecentlyUsed(size_t pagesToFree);

private:
    size_t m_pageSize;
    size_t m_maxPages;
    std::vector<KVPage> m_pages;
    std::vector<size_t> m_freePages;
    std::map<size_t, size_t> m_pageRefCount;
    mutable std::mutex m_mutex;
    
    size_t calculatePagesNeeded(size_t numTokens) const;
};

} // namespace RawrXD::Inference
