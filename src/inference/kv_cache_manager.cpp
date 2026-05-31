#include "kv_cache_manager.h"

namespace RawrXD::Inference {
    KVCacheManager::KVCacheManager(size_t pageSize, size_t maxPages)
        : m_pageSize(pageSize), m_maxPages(maxPages) {
        m_freePages.reserve(maxPages);
        for (size_t i = 0; i < maxPages; ++i) m_freePages.push_back(i);
    }

    KVCacheHandle KVCacheManager::allocate(size_t numTokens, uint32_t numLayers, uint32_t numHeads, uint32_t headDim) {
        size_t pagesNeeded = (numTokens + m_pageSize - 1) / m_pageSize;
        if (pagesNeeded > m_freePages.size()) return KVCacheHandle{};

        KVCacheHandle handle;
        handle.layerCount = numLayers;
        handle.headCount = numHeads;
        handle.headDim = headDim;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t i = 0; i < pagesNeeded; ++i) {
            size_t pageId = m_freePages.back();
            m_freePages.pop_back();
            handle.pageIds.push_back(pageId);
            m_pageRefCount[pageId] = 1;
        }
        return handle;
    }

    void KVCacheManager::release(KVCacheHandle& handle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (size_t pageId : handle.pageIds) {
            auto it = m_pageRefCount.find(pageId);
            if (it != m_pageRefCount.end()) {
                if (--it->second == 0) {
                    m_freePages.push_back(pageId);
                    m_pageRefCount.erase(it);
                }
            }
        }
        handle = {};
    }
}
