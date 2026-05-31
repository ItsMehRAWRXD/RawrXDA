#include "epoch_scoped_arena.h"
#include <algorithm>

namespace RawrXD::Memory {

EpochScopedArena::EpochScopedArena(size_t initialSize) {
    m_global.resize(initialSize);
}

uint64_t EpochScopedArena::beginEpoch() {
    std::lock_guard<std::mutex> lock(m_mutex);

    EpochArena e;
    e.epochId = m_nextEpochId++;
    e.offset = 0;
    e.buffer.resize(4096); // Start small
    m_epochs.push_back(std::move(e));
    return m_epochs.back().epochId;
}

void* EpochScopedArena::allocate(uint64_t epochId, size_t bytes, size_t align) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& e : m_epochs) {
        if (e.epochId == epochId) {
            size_t aligned = (e.offset + align - 1) & ~(align - 1);
            size_t needed = aligned + bytes;
            if (needed > e.buffer.size()) {
                e.buffer.resize(needed * 2);
            }
            void* ptr = e.buffer.data() + aligned;
            e.offset = needed;
            return ptr;
        }
    }
    return nullptr;
}

void EpochScopedArena::endEpoch(uint64_t epochId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_epochs.erase(
        std::remove_if(m_epochs.begin(), m_epochs.end(),
            [epochId](const EpochArena& e) { return e.epochId == epochId; }),
        m_epochs.end());
}

void EpochScopedArena::promoteToGlobal(uint64_t epochId, const void* ptr, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_globalOffset + bytes > m_global.size()) {
        m_global.resize(m_globalOffset + bytes);
    }
    std::memcpy(m_global.data() + m_globalOffset, ptr, bytes);
    m_globalOffset += bytes;
}

size_t EpochScopedArena::totalAllocated() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = m_globalOffset;
    for (const auto& e : m_epochs) {
        total += e.offset;
    }
    return total;
}

size_t EpochScopedArena::epochCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_epochs.size();
}

} // namespace RawrXD::Memory
