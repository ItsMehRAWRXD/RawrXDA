// ============================================================================
// K2TokenEmbedding.hpp — Real Token Embedding Lookup (K2-009)
// ============================================================================
//
// Streams exactly one embedding row from token_embd.weight without loading
// the full ~918 MiB matrix. Reuses the proven GGUF streaming and Q6_K
// dequantization from K2-007.
//
// Architecture:
//   GGUF shards → GlobalTensorIndex → stream one row → dequantize → FP32
//
// Hard requirements:
//   - No full embedding-matrix residency
//   - Stream only the requested row
//   - Support the quantization actually present in the GGUF
//   - Deterministic output
//   - Explicit residency accounting
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace Deep2 {

// Forward declaration
class GlobalTensorIndex;
struct GlobalTensorRef;

class K2TokenEmbedding {
public:
    struct Config {
        std::size_t hiddenSize = 7168;
        std::size_t vocabSize = 163840;
        std::size_t maxResidentBytes = 256ULL * 1024ULL * 1024ULL;
    };

    struct Result {
        bool ok = false;
        std::uint32_t tokenId = 0;
        std::size_t dimensions = 0;
        std::size_t bytesRead = 0;
        std::size_t peakResidency = 0;
        std::size_t finalResidency = 0;
        std::uint64_t checksum = 0;
        std::string error;
    };

    explicit K2TokenEmbedding(const Config& config = Config{});

    // Bind to an existing GlobalTensorIndex (must outlive this object)
    bool initialize(const GlobalTensorIndex* index);

    // Fetch exactly one embedding row into output (must contain hiddenSize floats)
    Result lookup(std::uint32_t tokenId, float* output);

    std::size_t hiddenSize() const noexcept { return config_.hiddenSize; }
    std::size_t vocabSize() const noexcept { return config_.vocabSize; }
    const std::string& tensorName() const noexcept { return tensorName_; }

private:
    Config config_;
    const GlobalTensorIndex* index_ = nullptr;
    GlobalTensorRef ref_;
    bool hasRef_ = false;
    std::string tensorName_;

    std::size_t currentResidency_ = 0;
    std::size_t peakResidency_ = 0;

    bool locateEmbeddingTensor();
    bool decodeRow(const std::uint8_t* data, std::size_t bytes,
                   std::uint32_t ggmlType, float* output, std::size_t elements);

    static std::uint64_t checksum(const float* data, std::size_t count);
    void acquire(std::size_t bytes);
    void release(std::size_t bytes);
};

} // namespace Deep2
