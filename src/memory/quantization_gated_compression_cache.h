#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD::Memory {

enum class QGCCPrecision : uint8_t {
    FP16 = 0,
    INT8 = 1,
    INT4 = 2
};

struct QGCCToken {
    uint64_t tokenId = 0;
    uint32_t headIdx = 0;
    uint32_t seqPos = 0;
    QGCCPrecision precision = QGCCPrecision::FP16;
    float entropy = 0.0f;
    std::vector<uint8_t> data;
};

class QuantizationGatedCompressionCache {
public:
    explicit QGCC(size_t recentWindow = 64, float entropyThreshold = 0.7f);

    uint64_t insert(uint32_t headIdx, uint32_t seqPos, const float* kv, size_t count, float entropy);
    bool retrieve(uint64_t tokenId, float* out, size_t count) const;
    void adaptWindow(uint32_t headIdx, float avgEntropy);

    size_t compressedBytes() const;
    size_t uncompressedBytes() const;

private:
    mutable std::mutex m_mutex;
    size_t m_recentWindow;
    float m_entropyThreshold;
    uint64_t m_nextId = 1;
    std::unordered_map<uint64_t, QGCCToken> m_tokens;
    std::unordered_map<uint32_t, size_t> m_headWindow;
    size_t m_compressedBytes = 0;
    size_t m_uncompressedBytes = 0;

    static std::vector<uint8_t> quantize(const float* src, size_t n, QGCCPrecision prec);
    static void dequantize(const uint8_t* src, float* dst, size_t n, QGCCPrecision prec);
};

} // namespace RawrXD::Memory
