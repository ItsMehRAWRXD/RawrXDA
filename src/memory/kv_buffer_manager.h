#pragma once
#include <stdint.h>
#include <vector>
#include <mutex>

namespace RawrXD {

struct KVFragment {
    uint64_t physical_offset;
    uint32_t size_tokens;
    uint32_t last_used_step;
    bool is_free;
};

class KVBufferManager {
public:
    KVBufferManager(uint32_t max_tokens) : m_totalTokens(max_tokens) {
        m_fragments.push_back({0, max_tokens, 0, true});
    }

    uint64_t Allocate(uint32_t tokens, uint32_t current_step) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_fragments.begin(); it != m_fragments.end(); ++it) {
            if (it->is_free && it->size_tokens >= tokens) {
                if (it->size_tokens > tokens) {
                    KVFragment remainder = { it->physical_offset + tokens, it->size_tokens - tokens, 0, true };
                    it->size_tokens = tokens;
                    it->is_free = false;
                    it->last_used_step = current_step;
                    m_fragments.insert(std::next(it), remainder);
                    return it->physical_offset;
                } else {
                    it->is_free = false;
                    it->last_used_step = current_step;
                    return it->physical_offset;
                }
            }
        }
        return (uint64_t)-1; // Fragmented or OOM
    }

    void Defragment() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_fragments.begin(); it != m_fragments.end(); ) {
            auto next = std::next(it);
            if (next != m_fragments.end() && it->is_free && next->is_free) {
                it->size_tokens += next->size_tokens;
                m_fragments.erase(next);
            } else {
                ++it;
            }
        }
    }

private:
    uint32_t m_totalTokens;
    std::vector<KVFragment> m_fragments;
    std::mutex m_mutex;
};

} // namespace RawrXD
