#include "tensor_compression_pool.h"

#include <algorithm>
#include <cmath>

namespace RawrXD::Memory {

TensorCompressionPool::TensorCompressionPool(PrecisionMode defaultMode)
    : m_default(defaultMode) {}

uint64_t TensorCompressionPool::compress(const float* src, size_t count, PrecisionMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!src || count == 0) {
        return 0;
    }

    if (mode == PrecisionMode::FP16 || mode == PrecisionMode::FP32) {
        mode = m_default;
    }

    CompressedTensor t{};
    t.tensorId = m_nextId++;
    t.mode = mode;
    t.originalBytes = count * sizeof(float);

    if (mode == PrecisionMode::INT4) {
        t.data = quantizeINT4(src, count, t.scale, t.zeroPoint);
    } else {
        t.mode = PrecisionMode::INT8;
        t.data = quantizeINT8(src, count, t.scale, t.zeroPoint);
    }

    t.compressedBytes = t.data.size();
    if (t.originalBytes > t.compressedBytes) {
        m_savedBytes += (t.originalBytes - t.compressedBytes);
    }
    m_pool.push_back(std::move(t));
    return m_pool.back().tensorId;
}

bool TensorCompressionPool::decompress(uint64_t tensorId, float* dst, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    const CompressedTensor* t = find(tensorId);
    if (!t || !dst) {
        return false;
    }

    if (t->mode == PrecisionMode::INT4) {
        dequantizeINT4(t->data.data(), dst, count, t->scale, t->zeroPoint);
    } else {
        dequantizeINT8(t->data.data(), dst, count, t->scale, t->zeroPoint);
    }

    return true;
}

size_t TensorCompressionPool::savedBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_savedBytes;
}

size_t TensorCompressionPool::poolSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pool.size();
}

void TensorCompressionPool::evict(uint64_t tensorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_pool.begin(), m_pool.end(), [tensorId](const CompressedTensor& t) {
        return t.tensorId == tensorId;
    });
    if (it == m_pool.end()) {
        return;
    }

    if (it->originalBytes > it->compressedBytes) {
        const size_t reclaimed = it->originalBytes - it->compressedBytes;
        m_savedBytes = (m_savedBytes >= reclaimed) ? (m_savedBytes - reclaimed) : 0;
    }
    m_pool.erase(it);
}

CompressedTensor* TensorCompressionPool::find(uint64_t id) {
    auto it = std::find_if(m_pool.begin(), m_pool.end(), [id](const CompressedTensor& t) {
        return t.tensorId == id;
    });
    return it == m_pool.end() ? nullptr : &(*it);
}

const CompressedTensor* TensorCompressionPool::find(uint64_t id) const {
    auto it = std::find_if(m_pool.begin(), m_pool.end(), [id](const CompressedTensor& t) {
        return t.tensorId == id;
    });
    return it == m_pool.end() ? nullptr : &(*it);
}

std::vector<uint8_t> TensorCompressionPool::quantizeINT8(const float* src, size_t n, float& scale, float& zp) {
    float minV = src[0];
    float maxV = src[0];
    for (size_t i = 1; i < n; ++i) {
        minV = std::min(minV, src[i]);
        maxV = std::max(maxV, src[i]);
    }

    const float range = std::max(1e-6f, maxV - minV);
    scale = range / 255.0f;
    zp = minV;

    std::vector<uint8_t> out(n);
    for (size_t i = 0; i < n; ++i) {
        const float q = (src[i] - zp) / scale;
        out[i] = static_cast<uint8_t>(std::clamp(q, 0.0f, 255.0f));
    }
    return out;
}

void TensorCompressionPool::dequantizeINT8(const uint8_t* src, float* dst, size_t n, float scale, float zp) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = static_cast<float>(src[i]) * scale + zp;
    }
}

std::vector<uint8_t> TensorCompressionPool::quantizeINT4(const float* src, size_t n, float& scale, float& zp) {
    float minV = src[0];
    float maxV = src[0];
    for (size_t i = 1; i < n; ++i) {
        minV = std::min(minV, src[i]);
        maxV = std::max(maxV, src[i]);
    }

    const float range = std::max(1e-6f, maxV - minV);
    scale = range / 15.0f;
    zp = minV;

    std::vector<uint8_t> out((n + 1) / 2, 0);
    for (size_t i = 0; i < n; ++i) {
        const float qf = (src[i] - zp) / scale;
        const uint8_t q = static_cast<uint8_t>(std::clamp(qf, 0.0f, 15.0f));
        const size_t bi = i / 2;
        if ((i & 1) == 0) {
            out[bi] = q;
        } else {
            out[bi] |= static_cast<uint8_t>(q << 4);
        }
    }
    return out;
}

void TensorCompressionPool::dequantizeINT4(const uint8_t* src, float* dst, size_t n, float scale, float zp) {
    for (size_t i = 0; i < n; ++i) {
        const uint8_t packed = src[i / 2];
        const uint8_t q = ((i & 1) == 0) ? (packed & 0x0F) : ((packed >> 4) & 0x0F);
        dst[i] = static_cast<float>(q) * scale + zp;
    }
}

} // namespace RawrXD::Memory
