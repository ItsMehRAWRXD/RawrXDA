// semantic_search_bridge.hpp — C++ side of semantic search MASM kernels
// Graph / mmap logic stays in C++; inner loops are in RawrXD_SemanticSearch_Kernels.asm

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

extern "C"
{

    // int8[256] · int8[256], scaled: result = scale * sum_i(a[i]*b[i])
    // MSVC x64: third float argument is passed in XMM2.
    float RawrXD_Q8_Q8_Dot256(const std::int8_t* a, const std::int8_t* b, float scale);

    // Dot(vec_f16, query_f32); elemCount must be a multiple of 16 (0 allowed).
    float RawrXD_F16F32_Dot(const std::uint16_t* vecF16, const float* queryF32, std::size_t elemCount);
}

namespace RawrXD::Search
{

enum class QuantFormat
{
    Q8_0,  // Plain int8 payload (caller supplies scales); hot path uses Dot256 when dim==256
    Q4_0,
    Q4_1,
    F16,
    F32,
};

// Slice into a memory-mapped index blob
struct IndexSlice
{
    const std::uint8_t* data{};
    std::size_t offset{};
    std::size_t length{};  // bytes in slice
    QuantFormat format{QuantFormat::F32};
    float scale{1.f};  // combined dequant / normalization for Q8 payloads
};

// Query workspace (F32 path + optional symmetric Q8 query for Q8·Q8 dots)
struct QueryContext
{
    alignas(64) float normalizedQuery[1024]{};
    alignas(64) std::int8_t q8Query[1024]{};
    std::size_t dim{};
    float q8Scale{1.f};
    QuantFormat targetFormat{QuantFormat::F32};
};

class SemanticSearcher
{
  public:
    explicit SemanticSearcher(void* dmaRingBuffer);

    float similarity(const IndexSlice& vec, const QueryContext& query);

    void batchScan(std::span<const IndexSlice> candidates, const QueryContext& query, float* outScores,
                   std::uint32_t* outIndices);

    std::vector<std::pair<std::uint32_t, float>> topK(const float* scores, const std::uint32_t* indices,
                                                      std::size_t count, std::size_t k);

  private:
    void* m_dmaBuffer{};
};

}  // namespace RawrXD::Search
