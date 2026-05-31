#include "rank_decomposed_layer_swapper.h"
#include <algorithm>
#include <cmath>

namespace RawrXD::Memory {

RankDecomposedLayerSwapper::RankDecomposedLayerSwapper(size_t vramBudgetBytes)
    : m_vramBudget(vramBudgetBytes) {}

uint64_t RankDecomposedLayerSwapper::registerLayer(uint32_t layerIdx, const float* W, uint32_t rows, uint32_t cols, uint32_t rank) {
    std::lock_guard<std::mutex> lock(m_mutex);

    RDLMatrix m;
    m.rows = rows;
    m.cols = cols;
    m.rank = rank;
    decompose(W, rows, cols, rank, m);

    uint64_t id = m_nextId++;
    m_layers[id] = std::move(m);
    m_vramUsed += static_cast<size_t>(rows) * rank * sizeof(float); // Only A in VRAM
    return id;
}

bool RankDecomposedLayerSwapper::reconstruct(uint64_t layerId, float* out) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_layers.find(layerId);
    if (it == m_layers.end()) return false;

    reconstruct(it->second, out);
    return true;
}

void RankDecomposedLayerSwapper::setVRAMBudget(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_vramBudget = bytes;
}

size_t RankDecomposedLayerSwapper::vramUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_vramUsed;
}

void RankDecomposedLayerSwapper::decompose(const float* W, uint32_t rows, uint32_t cols, uint32_t rank, RDLMatrix& out) {
    // Simplified randomized SVD sketch (Halko et al.)
    out.A.resize(static_cast<size_t>(rows) * rank);
    out.B.resize(static_cast<size_t>(rank) * cols);

    // Seed random projection
    std::vector<float> Omega(static_cast<size_t>(cols) * rank);
    for (auto& v : Omega) v = (static_cast<float>(rand()) / RAND_MAX) * 2.0f - 1.0f;

    // Y = W × Omega
    std::vector<float> Y(static_cast<size_t>(rows) * rank, 0.0f);
    for (uint32_t i = 0; i < rows; ++i) {
        for (uint32_t k = 0; k < cols; ++k) {
            for (uint32_t j = 0; j < rank; ++j) {
                Y[i * rank + j] += W[i * cols + k] * Omega[k * rank + j];
            }
        }
    }

    // Store Y as A
    std::copy(Y.begin(), Y.end(), out.A.begin());

    // B ≈ (A^T A)^{-1} A^T W (simplified)
    std::fill(out.B.begin(), out.B.end(), 0.0f);
    for (uint32_t r = 0; r < rank; ++r) {
        for (uint32_t i = 0; i < rows; ++i) {
            for (uint32_t c = 0; c < cols; ++c) {
                out.B[r * cols + c] += out.A[i * rank + r] * W[i * cols + c];
            }
        }
    }
}

void RankDecomposedLayerSwapper::reconstruct(const RDLMatrix& m, float* out) {
    std::fill_n(out, static_cast<size_t>(m.rows) * m.cols, 0.0f);
    for (uint32_t i = 0; i < m.rows; ++i) {
        for (uint32_t r = 0; r < m.rank; ++r) {
            for (uint32_t c = 0; c < m.cols; ++c) {
                out[i * m.cols + c] += m.A[i * m.rank + r] * m.B[r * m.cols + c];
            }
        }
    }
}

} // namespace RawrXD::Memory
