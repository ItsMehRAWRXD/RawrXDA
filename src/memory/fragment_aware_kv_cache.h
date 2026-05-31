#pragma once
#include <stdint.h>
#include <vector>
#include <mutex>
#include <chrono>

namespace RawrXD {

struct VRAMSegment {
    uint64_t offset;
    uint32_t size;
    bool is_free;
    uint32_t last_access;
    float reuse_probability; // Calculated based on sequence patterns
};

/**
 * FragmentAwareKVCache
 * 
 * Implements VRAM-Aware KV Buffer Recycling with a Confidence Layer.
 */
class FragmentAwareKVCache {
public:
    FragmentAwareKVCache(uint32_t total_vram) : m_capacity(total_vram) {
        m_segments.push_back({0, total_vram, true, 0, 0.0f});
    }

    uint64_t Acquire(uint32_t size, float expected_reuse = 0.5f) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint32_t now = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());

        for (auto& seg : m_segments) {
            if (seg.is_free && seg.size >= size) {
                if (seg.size > size + 4096) {
                    VRAMSegment extra = { seg.offset + size, seg.size - size, true, 0, 0.0f };
                    seg.size = size;
                    seg.is_free = false;
                    seg.last_access = now;
                    seg.reuse_probability = expected_reuse;
                    m_segments.push_back(extra);
                } else {
                    seg.is_free = false;
                    seg.last_access = now;
                    seg.reuse_probability = expected_reuse;
                }
                return seg.offset;
            }
        }

        return RecycleOptimal(size);
    }

private:
    uint64_t RecycleOptimal(uint32_t needed) {
        // Evict based on Cost = Coldness * (1.0 - ReuseProb)
        auto best_victim = m_segments.end();
        float max_evict_score = -1.0f;
        uint32_t now = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());

        for (auto it = m_segments.begin(); it != m_segments.end(); ++it) {
            if (!it->is_free && it->size >= needed) {
                float coldness = static_cast<float>(now - it->last_access) + 1.0f;
                float evict_score = coldness * (1.0f - it->reuse_probability);

                if (evict_score > max_evict_score) {
                    max_evict_score = evict_score;
                    best_victim = it;
                }
            }
        }

        if (best_victim != m_segments.end()) {
            best_victim->is_free = false;
            best_victim->last_access = now;
            return best_victim->offset;
        }

        return (uint64_t)-1;
    }

    uint32_t m_capacity;
    std::vector<VRAMSegment> m_segments;
    std::mutex m_mutex;
};

} // namespace RawrXD
