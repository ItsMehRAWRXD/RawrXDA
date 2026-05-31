#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct SlidingWindowEntry {
    uint64_t kvId = 0;
    uint32_t tokenPos = 0;
    size_t bytes = 0;
};

// Maintains a fixed-length decode window and evicts KV spans older than the window.
class SlidingWindowKVEviction {
public:
    explicit SlidingWindowKVEviction(uint32_t windowTokens);

    // Adds/updates a token KV span and returns evicted entries, if any.
    std::vector<SlidingWindowEntry> upsert(uint64_t kvId, uint32_t tokenPos, size_t bytes);

    void setWindow(uint32_t windowTokens);
    uint32_t windowSize() const;
    size_t residentBytes() const;
    void reset();

private:
    std::vector<SlidingWindowEntry> evictLocked(uint32_t newestPos);

    mutable std::mutex m_mutex;
    uint32_t m_windowTokens;
    size_t m_residentBytes{0};
    std::deque<SlidingWindowEntry> m_order;
    std::unordered_map<uint64_t, uint32_t> m_latestPos;
};

} // namespace RawrXD::Memory
