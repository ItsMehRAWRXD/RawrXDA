#include "sliding_window_kv_eviction.h"

namespace RawrXD::Memory {

SlidingWindowKVEviction::SlidingWindowKVEviction(uint32_t windowTokens)
    : m_windowTokens(windowTokens == 0 ? 1 : windowTokens) {}

std::vector<SlidingWindowEntry> SlidingWindowKVEviction::upsert(uint64_t kvId, uint32_t tokenPos, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_latestPos.find(kvId);
    if (it != m_latestPos.end()) {
        it->second = tokenPos;
    } else {
        m_latestPos.emplace(kvId, tokenPos);
    }

    m_order.push_back(SlidingWindowEntry{kvId, tokenPos, bytes});
    m_residentBytes += bytes;

    return evictLocked(tokenPos);
}

void SlidingWindowKVEviction::setWindow(uint32_t windowTokens) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_windowTokens = windowTokens == 0 ? 1 : windowTokens;
}

uint32_t SlidingWindowKVEviction::windowSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_windowTokens;
}

size_t SlidingWindowKVEviction::residentBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_residentBytes;
}

void SlidingWindowKVEviction::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_residentBytes = 0;
    m_order.clear();
    m_latestPos.clear();
}

std::vector<SlidingWindowEntry> SlidingWindowKVEviction::evictLocked(uint32_t newestPos) {
    std::vector<SlidingWindowEntry> evicted;
    const uint32_t minAllowed = (newestPos > m_windowTokens) ? (newestPos - m_windowTokens) : 0;

    while (!m_order.empty()) {
        const SlidingWindowEntry front = m_order.front();
        if (front.tokenPos >= minAllowed) {
            break;
        }
        m_order.pop_front();

        auto latest = m_latestPos.find(front.kvId);
        if (latest != m_latestPos.end() && latest->second == front.tokenPos) {
            m_latestPos.erase(latest);
            m_residentBytes = (m_residentBytes >= front.bytes) ? (m_residentBytes - front.bytes) : 0;
            evicted.push_back(front);
        }
    }

    return evicted;
}

} // namespace RawrXD::Memory
