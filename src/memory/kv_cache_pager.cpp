#include "kv_cache_pager.h"

#include <algorithm>
#include <functional>

namespace RawrXD::Memory {

KVCachePager::KVCachePager(size_t maxResidentPages, size_t pageSizeBytes)
    : m_maxPages(maxResidentPages), m_pageBytes(pageSizeBytes) {}

uint64_t KVCachePager::pin(uint32_t seqPos, std::function<void(const KVPage&)> evictCallback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto existing = m_seqMap.find(seqPos);
    if (existing != m_seqMap.end()) {
        m_lruList.splice(m_lruList.begin(), m_lruList, existing->second);
        return existing->second->pageId;
    }

    if (m_maxPages == 0) {
        return 0;
    }

    while (m_lruList.size() >= m_maxPages) {
        evictOne(evictCallback);
    }

    KVPage page{};
    page.pageId = m_nextId++;
    page.seqPos = seqPos;
    page.bytes = m_pageBytes;
    page.dirty = false;

    m_lruList.push_front(page);
    auto it = m_lruList.begin();
    m_seqMap[seqPos] = it;
    m_idMap[it->pageId] = it;
    return it->pageId;
}

void KVCachePager::unpin(uint64_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto idIt = m_idMap.find(pageId);
    if (idIt == m_idMap.end()) {
        return;
    }
    // Keep recently used pages near the front.
    m_lruList.splice(m_lruList.begin(), m_lruList, idIt->second);
}

void KVCachePager::markDirty(uint64_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto idIt = m_idMap.find(pageId);
    if (idIt != m_idMap.end()) {
        idIt->second->dirty = true;
    }
}

size_t KVCachePager::residentCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lruList.size();
}

void KVCachePager::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lruList.clear();
    m_seqMap.clear();
    m_idMap.clear();
    m_nextId = 1;
}

void KVCachePager::evictOne(std::function<void(const KVPage&)>& cb) {
    if (m_lruList.empty()) {
        return;
    }

    auto it = std::prev(m_lruList.end());
    const KVPage victim = *it;
    if (cb) {
        cb(victim);
    }

    m_seqMap.erase(victim.seqPos);
    m_idMap.erase(victim.pageId);
    m_lruList.erase(it);
}

} // namespace RawrXD::Memory
