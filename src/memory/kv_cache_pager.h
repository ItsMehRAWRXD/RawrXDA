#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace RawrXD::Memory {

struct KVPage {
    uint64_t pageId;
    uint32_t seqPos;   // sequence position of first token in this page
    size_t   bytes;
    bool     dirty;    // needs flush to backing store before eviction
};

// Page-based KV-cache manager with LRU eviction.
// Keeps a fixed number of pages resident; evicts cold pages to a RAM/disk sink.
class KVCachePager {
public:
    // maxResidentPages — pages kept in VRAM/RAM at any time
    explicit KVCachePager(size_t maxResidentPages, size_t pageSizeBytes);

    // Ensure page for seqPos is resident; returns pageId.
    // If eviction is needed, eldest page is retired via evictCallback.
    uint64_t pin(uint32_t seqPos,
                 std::function<void(const KVPage&)> evictCallback = nullptr);

    void     unpin(uint64_t pageId);
    void     markDirty(uint64_t pageId);
    size_t   residentCount() const;
    void     reset();

private:
    mutable std::mutex                           m_mutex;
    size_t                                       m_maxPages;
    size_t                                       m_pageBytes;
    uint64_t                                     m_nextId{1};
    std::list<KVPage>                            m_lruList;
    std::unordered_map<uint32_t, std::list<KVPage>::iterator> m_seqMap;
    std::unordered_map<uint64_t, std::list<KVPage>::iterator> m_idMap;

    void evictOne(std::function<void(const KVPage&)>& cb);
};

} // namespace RawrXD::Memory
