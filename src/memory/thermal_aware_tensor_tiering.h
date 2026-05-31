#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

enum class TensorTier : uint8_t {
    VRAM = 0,
    PinnedRAM = 1,
    LargePage = 2
};

struct TensorHeat {
    uint64_t tensorId = 0;
    size_t bytes = 0;
    uint64_t accessCount = 0;
    uint64_t heat = 0; // bytes × accesses
    TensorTier tier = TensorTier::PinnedRAM;
};

class ThermalAwareTensorTiering {
public:
    static constexpr uint64_t HOT_THRESHOLD = 1ULL << 28;  // 256 MB·acc
    static constexpr uint64_t COLD_THRESHOLD = 1ULL << 22;  // 4 MB·acc

    uint64_t allocate(size_t bytes);
    void touch(uint64_t tensorId);
    void decayAll(float factor = 0.5f);
    void free(uint64_t tensorId);

    std::vector<uint64_t> rebalance();
    const TensorHeat* getTensor(uint64_t tensorId) const;
    size_t totalBytes() const;
    size_t vramBytes() const;

private:
    mutable std::mutex m_mutex;
    uint64_t m_nextId = 1;
    std::unordered_map<uint64_t, TensorHeat> m_tensors;
    size_t m_vramBytes = 0;

    TensorTier computeTier(uint64_t heat) const;
};

} // namespace RawrXD::Memory
