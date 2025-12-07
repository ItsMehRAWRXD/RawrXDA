#include "quant_utils.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>

static inline quint16 float_to_half_impl(float f) {
    union { uint32_t u; float f; } v{0}; v.f = f;
    uint32_t x = v.u;
    uint32_t sign = (x >> 31) & 0x1;
    int32_t  exp  = int32_t((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = (x >> 13) & 0x3FF; // keep top 10 bits
    if (exp <= 0) {
        if (exp < -10) return quint16(sign << 15);
        mant = (mant | 0x400) >> (1 - exp);
        return quint16((sign << 15) | (mant & 0x3FF));
    } else if (exp >= 31) {
        return quint16((sign << 15) | (0x1F << 10) | (mant ? 0x1 : 0));
    }
    return quint16((sign << 15) | ((exp & 0x1F) << 10) | (mant & 0x3FF));
}

QByteArray quantize_q8k(const QByteArray& raw) {
    if (raw.size() % 4 != 0) return raw;
    const float* f = reinterpret_cast<const float*>(raw.constData());
    int n = raw.size() / 4;
    float amax = 0.f;
    for (int i = 0; i < n; ++i) amax = std::max(amax, std::fabs(f[i]));
    float scale = (amax > 0.f) ? (amax / 127.f) : 1.f;
    QByteArray out;
    out.resize(4 + n);
    std::memcpy(out.data(), &scale, 4);
    int8_t* q = reinterpret_cast<int8_t*>(out.data() + 4);
    for (int i = 0; i < n; ++i) {
        int v = int(std::round(f[i] / scale));
        v = std::clamp(v, -127, 127);
        q[i] = int8_t(v);
    }
    return out;
}

QByteArray quantize_q4_0(const QByteArray& raw) {
    if (raw.size() % 4 != 0) return raw; 
    const float* f = reinterpret_cast<const float*>(raw.constData());
    int n = raw.size() / 4;
    float amax = 0.f;
    for (int i = 0; i < n; ++i) amax = std::max(amax, std::fabs(f[i]));
    float scale = (amax > 0.f) ? (amax / 7.f) : 1.f; // 4-bit signed approx (-7..7)
    int packed = (n + 1) / 2; // two 4-bit per byte
    QByteArray out;
    out.resize(4 + packed);
    std::memcpy(out.data(), &scale, 4);
    uint8_t* q = reinterpret_cast<uint8_t*>(out.data() + 4);
    int qi = 0;
    for (int i = 0; i < n; i += 2) {
        int v0 = int(std::round(f[i] / scale));
        v0 = std::clamp(v0, -8, 7) & 0xF;
        int v1 = 0;
        if (i + 1 < n) {
            v1 = int(std::round(f[i+1] / scale));
            v1 = std::clamp(v1, -8, 7) & 0xF;
        }
        q[qi++] = uint8_t((v1 & 0xF) << 4 | (v0 & 0xF));
    }
    return out;
}

QByteArray quantize_generic_bits(const QByteArray& raw, int bits) {
    if (raw.size() % 4 != 0) return raw; 
    const float* f = reinterpret_cast<const float*>(raw.constData());
    int n = raw.size() / 4;
    float amax = 0.f;
    for (int i = 0; i < n; ++i) amax = std::max(amax, std::fabs(f[i]));
    int qmax = (1 << (bits - 1)) - 1; // symmetric positive max
    float scale = (amax > 0.f) ? (amax / float(qmax)) : 1.f;

    int totalBits = n * bits;
    int totalBytes = (totalBits + 7) / 8;

    QByteArray out;
    out.resize(4 + totalBytes); // 4 bytes scale + packed data
    std::memcpy(out.data(), &scale, 4);
    uint8_t* dst = reinterpret_cast<uint8_t*>(out.data() + 4);
    std::memset(dst, 0, totalBytes);

    int bitPos = 0;
    for (int i = 0; i < n; ++i) {
        int v = int(std::round(f[i] / scale));
        v = std::clamp(v, -qmax, qmax);
        uint32_t u = (uint32_t)(v & ((1u << bits) - 1));

        int byteIdx = bitPos / 8;
        int off = bitPos % 8;
        uint64_t carry = (uint64_t)u << off;
        for (int b = 0; b < ((bits + off + 7) / 8); ++b) {
            if (byteIdx + b >= totalBytes) break;
            dst[byteIdx + b] |= uint8_t((carry >> (8 * b)) & 0xFF);
        }
        bitPos += bits;
    }

    return out;
}

QByteArray to_f16(const QByteArray& raw) {
    if (raw.size() % 4 != 0) return raw;
    const float* f = reinterpret_cast<const float*>(raw.constData());
    int n = raw.size() / 4;
    QByteArray out;
    out.resize(n * 2);
    quint16* h = reinterpret_cast<quint16*>(out.data());
    for (int i = 0; i < n; ++i) h[i] = float_to_half_impl(f[i]);
    return out;
}

QByteArray apply_quant(const QByteArray& raw, const QString& mode) {
    if (mode == "F32") return raw;
    if (mode == "F16") return to_f16(raw);
    if (mode == "Q8_K") return quantize_q8k(raw);
    if (mode == "Q4_0" || mode == "Q4_1") return quantize_q4_0(raw);
    if (mode == "Q5_0" || mode == "Q5_1") return quantize_generic_bits(raw, 5);
    if (mode == "Q6_K" || mode == "Q6k" ) return quantize_generic_bits(raw, 6);
    return raw;
}

QVector<float> unpack_generic_bits(const QByteArray& packed, int bits) {
    QVector<float> result;
    if (packed.size() < 4) return result;
    float scale;
    std::memcpy(&scale, packed.constData(), 4);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(packed.constData() + 4);
    int totalBytes = packed.size() - 4;
    int totalBits = totalBytes * 8;
    int n = totalBits / bits;
    result.resize(n);
    for (int i = 0; i < n; ++i) {
        int bitPos = i * bits;
        int byteIdx = bitPos / 8;
        int off = bitPos % 8;
        uint32_t mask = (1u << bits) - 1u;
        uint64_t block = 0;
        for (int b = 0; b < 8; ++b) {
            if (byteIdx + b < totalBytes) block |= uint64_t(data[byteIdx + b]) << (8 * b);
        }
        uint32_t u = (block >> off) & mask;
        int32_t signMask = 1 << (bits - 1);
        int32_t v = (u & signMask) ? (int32_t)(u | (~int32_t(mask))) : (int32_t)u;
        result[i] = float(v) * scale;
    }
    return result;
}

QVector<float> unpack_f16(const QByteArray& packed) {
    QVector<float> out;
    if (packed.size() % 2 != 0) return out;
    int n = packed.size() / 2;
    out.resize(n);
    const quint16* h = reinterpret_cast<const quint16*>(packed.constData());
    for (int i = 0; i < n; ++i) {
        // Convert half to float using a simple algorithm
        uint16_t hh = h[i];
        uint32_t sign = (hh >> 15) & 0x1;
        uint32_t exp = (hh >> 10) & 0x1F;
        uint32_t mant = hh & 0x3FF;
        uint32_t f32;
        if (exp == 0) {
            if (mant == 0) f32 = sign << 31; // zero
            else {
                // subnormal
                exp = 1;
                while ((mant & 0x400) == 0) {
                    mant <<= 1;
                    exp--;
                }
                mant &= 0x3FF;
                exp = exp + (127 - 15);
                f32 = (sign << 31) | (exp << 23) | (mant << 13);
            }
        } else if (exp == 0x1F) {
            f32 = (sign << 31) | (0xFF << 23) | (mant ? 1 : 0);
        } else {
            exp = exp + (127 - 15);
            f32 = (sign << 31) | (exp << 23) | (mant << 13);
        }
        float f;
        std::memcpy(&f, &f32, 4);
        out[i] = f;
    }
    return out;
}
