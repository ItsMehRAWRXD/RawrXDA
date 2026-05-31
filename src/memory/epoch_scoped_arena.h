#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct EpochArena {
    uint64_t epochId = 0;
    size_t offset = 0;
    std::vector<uint8_t> buffer;
};

class EpochScopedArena {
public:
    explicit EpochScopedArena(size_t initialSize = 1 << 20);

    uint64_t beginEpoch();
    void* allocate(uint64_t epochId, size_t bytes, size_t align = 16);
    void endEpoch(uint64_t epochId);

    void promoteToGlobal(uint64_t epochId, const void* ptr, size_t bytes);

    size_t totalAllocated() const;
    size_t epochCount() const;

private:
    mutable std::mutex m_mutex;
    std::vector<EpochArena> m_epochs;
    std::vector<uint8_t> m_global;
    size_t m_globalOffset = 0;
    uint64_t m_nextEpochId = 1;
};

} // namespace RawrXD::Memory
