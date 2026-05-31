#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

struct RDLMatrix {
    uint32_t rows = 0;
    uint32_t cols = 0;
    uint32_t rank = 0;
    std::vector<float> A; // rows × rank
    std::vector<float> B; // rank × cols
};

class RankDecomposedLayerSwapper {
public:
    explicit RankDecomposedLayerSwapper(size_t vramBudgetBytes);

    uint64_t registerLayer(uint32_t layerIdx, const float* W, uint32_t rows, uint32_t cols, uint32_t rank);
    bool reconstruct(uint64_t layerId, float* out) const;
    void setVRAMBudget(size_t bytes);
    size_t vramUsage() const;

private:
    mutable std::mutex m_mutex;
    size_t m_vramBudget;
    size_t m_vramUsed = 0;
    uint64_t m_nextId = 1;
    std::unordered_map<uint64_t, RDLMatrix> m_layers;

    static void decompose(const float* W, uint32_t rows, uint32_t cols, uint32_t rank, RDLMatrix& out);
    static void reconstruct(const RDLMatrix& m, float* out);
};

} // namespace RawrXD::Memory
