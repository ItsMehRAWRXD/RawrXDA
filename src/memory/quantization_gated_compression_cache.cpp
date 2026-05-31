#include "quantization_gated_compression_cache.h"
#include <algorithm>
#include <cmath>

namespace RawrXD::Memory {

QGCC::QGCC(size_t recentWindow, float entropyThreshold)
    : m_recentWindow(recentWindow), m_entropyThreshold(entropyThreshold) {}

uint64_t QGCC::insert(uint32_t headIdx, uint32_t seqPos, const float* kv, size_t count, float entropy) {
    std::lock_guard<std::mutex> lock(m_mutex);

    QGCCToken t;
    t.tokenId = m_nextId++;
    t.headIdx = headIdx;
    t.seqPos = seqPos;
    t.entropy = entropy;

    // Decide precision based on entropy and position
    size_t window = m_headWindow[headIdx];
    if (seqPos < window || entropy > m_entropyThreshold) {
        t.precision = QGCCPrecision::FP16;
    } else if (entropy > m_entropyThreshold * 0.5f) {
        t.precision = QGCCPrecision::INT8;
    } else {
        t.precision = QGCCPrecision::INT4;
    }

    t.data = quantize(kv, count, t.precision);
    m_tokens[t.tokenId] = std::move(t);

    m_uncompressedBytes += count * sizeof(float);
    m_compressedBytes += t.data.size();
    return m_tokens.rbegin()->first;
}

bool QGCC::retrieve(uint64_t tokenId, float* out, size_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tokens.find(tokenId);
    if (it == m_tokens.end()) return false;

    dequantize(it->second.data.data(), out, count, it->second.precision);
    return true;
}

void QGCC::adaptWindow(uint32_t headIdx, float avgEntropy) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (avgEntropy < m_entropyThreshold * 0.5f) {
        m_headWindow[headIdx] = std::min(m_headWindow[headIdx] + 8, m_recentWindow);
    } else if (avgEntropy > m_entropyThreshold) {
        m_headWindow[headIdx] = m_headWindow[headIdx] > 8 ? m_headWindow[headIdx] - 8 : 0;
    }
}

size_t QGCC::compressedBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_compressedBytes;
}

size_t QGCC::uncompressedBytes() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_uncompressedBytes;
}

std::vector<uint8_t> QGCC::quantize(const float* src, size_t n, QGCCPrecision prec) {
    std::vector<uint8_t> out;
    if (prec == QGCCPrecision::FP16) {
        out.resize(n * 2);
        for (size_t i = 0; i < n; ++i) {
            uint16_t h = static_cast<uint16_t>(src[i] * 65535.0f);
            out[i * 2] = h & 0xFF;
            out[i * 2 + 1] = (h >> 8) & 0xFF;
        }
    } else if (prec == QGCCPrecision::INT8) {
        out.resize(n);
        for (size_t i = 0; i < n; ++i) {
            out[i] = static_cast<uint8_t>(std::clamp(src[i] * 127.0f + 128.0f, 0.0f, 255.0f));
        }
    } else {
        out.resize((n + 1) / 2);
        for (size_t i = 0; i < n; ++i) {
            uint8_t q = static_cast<uint8_t>(std::clamp(src[i] * 7.0f + 8.0f, 0.0f, 15.0f));
            if (i % 2 == 0) out[i / 2] = q;
            else out[i / 2] |= (q << 4);
        }
    }
    return out;
}

void QGCC::dequantize(const uint8_t* src, float* dst, size_t n, QGCCPrecision prec) {
    if (prec == QGCCPrecision::FP16) {
        for (size_t i = 0; i < n; ++i) {
            uint16_t h = src[i * 2] | (src[i * 2 + 1] << 8);
            dst[i] = static_cast<float>(h) / 65535.0f;
        }
    } else if (prec == QGCCPrecision::INT8) {
        for (size_t i = 0; i < n; ++i) {
            dst[i] = (static_cast<float>(src[i]) - 128.0f) / 127.0f;
        }
    } else {
        for (size_t i = 0; i < n; ++i) {
            uint8_t q = (i % 2 == 0) ? (src[i / 2] & 0x0F) : ((src[i / 2] >> 4) & 0x0F);
            dst[i] = (static_cast<float>(q) - 8.0f) / 7.0f;
        }
    }
}

} // namespace RawrXD::Memory
