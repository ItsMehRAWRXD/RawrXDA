#include "demand_paged_weight_streaming.h"
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace RawrXD::Memory {

DemandPagedWeightStreaming::DemandPagedWeightStreaming(size_t windowPages, size_t pageSize)
    : m_windowPages(windowPages), m_pageSize(pageSize) {}

uint64_t DemandPagedWeightStreaming::mapPage(const void* fileBase, size_t offset, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    evictIfNeeded();

    DPWSPage p;
    p.pageId = m_nextId++;
    p.offset = offset;
    p.size = size;
    p.accessCount = 0;
    p.lastAccess = m_tick++;

    m_pages[p.pageId] = std::move(p);
    return m_pages.rbegin()->first;
}

bool DemandPagedWeightStreaming::prefetch(uint64_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pages.find(pageId);
    if (it == m_pages.end()) return false;

#ifdef _WIN32
    WIN32_MEMORY_RANGE_ENTRY range;
    range.VirtualAddress = const_cast<void*>(reinterpret_cast<const void*>(it->second.offset));
    range.NumberOfBytes = it->second.size;
    PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
#else
    // POSIX: madvise with MADV_WILLNEED
    madvise(reinterpret_cast<void*>(it->second.offset), it->second.size, MADV_WILLNEED);
#endif

    it->second.accessCount++;
    it->second.lastAccess = m_tick++;
    return true;
}

void DemandPagedWeightStreaming::touch(uint64_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pages.find(pageId);
    if (it == m_pages.end()) return;

    it->second.accessCount++;
    it->second.lastAccess = m_tick++;
}

void DemandPagedWeightStreaming::unmap(uint64_t pageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pages.erase(pageId);
}

size_t DemandPagedWeightStreaming::windowSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_windowPages;
}

size_t DemandPagedWeightStreaming::pageSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pageSize;
}

size_t DemandPagedWeightStreaming::mappedPages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pages.size();
}

void DemandPagedWeightStreaming::evictIfNeeded() {
    if (m_pages.size() <= m_windowPages) return;

    // Find least recently used page
    uint64_t lruId = 0;
    uint64_t lruTick = UINT64_MAX;
    for (const auto& [id, p] : m_pages) {
        if (p.lastAccess < lruTick) {
            lruTick = p.lastAccess;
            lruId = id;
        }
    }

    m_pages.erase(lruId);
}

} // namespace RawrXD::Memory
