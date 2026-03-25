#if !defined(_MSC_VER)

#include <cstdint>
#include <cstring>

namespace {

static float halfToFloat(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000) << 16;
    const uint32_t exp = (h >> 10) & 0x1F;
    const uint32_t mant = h & 0x03FF;

    uint32_t bits = 0;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            uint32_t m = mant;
            uint32_t e = 113;
            while ((m & 0x0400) == 0) {
                m <<= 1;
                --e;
            }
            m &= 0x03FF;
            bits = sign | (e << 23) | (m << 13);
        }
    } else if (exp == 31) {
        bits = sign | 0x7F800000U | (mant << 13);
    } else {
        bits = sign | ((exp + 112U) << 23) | (mant << 13);
    }

    union {
        uint32_t u;
        float f;
    } conv{};
    conv.u = bits;
    return conv.f;
}

}  // namespace

extern "C" uint64_t Quant_DequantQ4_0(const void* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) {
        return 0;
    }
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        const uint8_t packed = in[i / 2];
        const uint8_t nibble = (i & 1ULL) ? ((packed >> 4) & 0x0F) : (packed & 0x0F);
        dst[i] = static_cast<float>(static_cast<int>(nibble) - 8) * 0.125f;
    }
    return numElements;
}

extern "C" uint64_t Quant_DequantQ8_0(const void* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) {
        return 0;
    }
    const auto* in = static_cast<const int8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        dst[i] = static_cast<float>(in[i]) * (1.0f / 127.0f);
    }
    return numElements;
}

extern "C" uint64_t KQuant_DequantizeQ4_K(const void* src, float* dst, uint64_t numElements) {
    return Quant_DequantQ4_0(src, dst, numElements);
}

extern "C" uint64_t KQuant_DequantizeQ6_K(const void* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) {
        return 0;
    }
    const auto* in = static_cast<const uint8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        const uint8_t v = in[i];
        const int32_t centered = static_cast<int32_t>(v) - 32;
        dst[i] = static_cast<float>(centered) * (1.0f / 32.0f);
    }
    return numElements;
}

extern "C" uint64_t KQuant_DequantizeF16(const uint16_t* src, float* dst, uint64_t numElements) {
    if (!src || !dst || numElements == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < numElements; ++i) {
        dst[i] = halfToFloat(src[i]);
    }
    return numElements;
}

extern "C" uint64_t asm_kquant_cpuid_check(void) {
    return 0;
}

extern "C" uint64_t RawrXD_AVX512_DequantFusion(const uint8_t* src_q,
                                                 const float* scales,
                                                 float* dst_f32,
                                                 uint64_t count) {
    if (!src_q || !dst_f32 || count == 0) {
        return 0;
    }

    const float s = scales ? scales[0] : 1.0f;
    for (uint64_t i = 0; i < count; ++i) {
        dst_f32[i] = static_cast<float>(src_q[i]) * s;
    }
    return count;
}

extern "C" uint64_t RawrXD_MASM_BPETokenize(const char* text,
                                             uint64_t text_len,
                                             uint32_t* out_token_ids,
                                             uint64_t max_tokens) {
    if (!text || !out_token_ids || text_len == 0 || max_tokens == 0) {
        return 0;
    }

    uint64_t written = 0;
    for (uint64_t i = 0; i < text_len && written < max_tokens; ++i) {
        const unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            continue;
        }
        out_token_ids[written++] = static_cast<uint32_t>(ch);
    }
    return written;
}

extern "C" uint64_t RawrXD_ASMToolDispatchFastPath(uint32_t opcode,
                                                    const void* in_payload,
                                                    void* out_payload,
                                                    uint64_t payload_bytes) {
    (void)opcode;
    if (!out_payload || !in_payload) {
        return 0;
    }
    if (payload_bytes > 0) {
        std::memcpy(out_payload, in_payload, static_cast<size_t>(payload_bytes));
    }
    return 1;
}

#endif  // !defined(_MSC_VER)
