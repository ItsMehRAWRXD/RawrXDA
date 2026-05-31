#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct DPWSPage {
    uint64_t pageId = 0;
    size_t offset = 0;
    size_t size = 0;
    uint32_t accessCount = 0;
    uint64_t lastAccess = 0;
};

class DemandPagedWeightStreaming {
public:
    explicit DemandPagedWeightStreaming(size_t windowPages = 16, size_t pageSize = 64 * 1024);

    uint64_t mapPage(const void* fileBase, size_t offset, size_t size);
    bool prefetch(uint64_t pageId);
    void touch(uint64_t pageId);
    void unmap(uint64_t pageId);

    size_t windowSize() const;
    size_t pageSize() const;
    size_t mappedPages() const;

private:
    mutable std::mutex m_mutex;
    size_t m_windowPages;
    size_t m_pageSize;
    uint64_t m_nextId = 1;
    uint64_t m_tick = 0;
    std::unordered_map<uint64_t, DPWSPage> m_pages;

    void evictIfNeeded();
};

} // namespace RawrXD::Memory
