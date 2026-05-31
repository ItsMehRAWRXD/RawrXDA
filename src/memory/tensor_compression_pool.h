#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <span>

namespace RawrXD::Memory {

enum class PrecisionMode : uint8_t { FP32 = 0, FP16, INT8, INT4 };

struct CompressedTensor {
    uint64_t    tensorId;
    PrecisionMode mode;
    size_t      originalBytes;
    size_t      compressedBytes;
    std::vector<uint8_t> data;
    float       scale{1.0f};
    float       zeroPoint{0.0f};
};

// Compresses idle tensors to INT8/INT4 in-place to free VRAM.
// Decompresses on demand before use.
class TensorCompressionPool {
public:
    explicit TensorCompressionPool(PrecisionMode defaultMode = PrecisionMode::INT8);

    // Compress raw FP32/FP16 data; returns tensorId.
    uint64_t compress(const float* src, size_t count, PrecisionMode mode = PrecisionMode::INT8);

    // Decompress back to FP32 destination buffer (must be count floats).
    bool decompress(uint64_t tensorId, float* dst, size_t count) const;

    size_t savedBytes() const;   // total bytes saved by compression
    size_t poolSize() const;     // number of compressed tensors
    void   evict(uint64_t tensorId);

private:
    mutable std::mutex               m_mutex;
    PrecisionMode                    m_default;
    uint64_t                         m_nextId{1};
    std::vector<CompressedTensor>    m_pool;
    size_t                           m_savedBytes{0};

    CompressedTensor* find(uint64_t id);
    const CompressedTensor* find(uint64_t id) const;

    static std::vector<uint8_t> quantizeINT8(const float* src, size_t n, float& scale, float& zp);
    static void dequantizeINT8(const uint8_t* src, float* dst, size_t n, float scale, float zp);
    static std::vector<uint8_t> quantizeINT4(const float* src, size_t n, float& scale, float& zp);
    static void dequantizeINT4(const uint8_t* src, float* dst, size_t n, float scale, float zp);
};

} // namespace RawrXD::Memory
