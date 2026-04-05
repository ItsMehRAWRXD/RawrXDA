#pragma once

#include <cstdint>

namespace RawrXD {
namespace KernelOps {

extern "C" int kv_accumulate_avx512(const float* src, float* dst, int count);
extern "C" int kv_accumulate_scaled_avx512(const float* src, float* dst, int count, float scale);
extern "C" unsigned int rawr_cpu_has_avx512();

inline bool HasAVX512Runtime() noexcept {
    return rawr_cpu_has_avx512() != 0;
}

inline bool AccumulateKV_AVX512(const float* src, float* dst, int count) noexcept {
    if (!src || !dst || count <= 0) {
        return false;
    }
    if (!HasAVX512Runtime()) {
        return false;
    }
    return kv_accumulate_avx512(src, dst, count) != 0;
}

inline void AccumulateKV_Fallback(const float* src, float* dst, int count) noexcept {
    if (!src || !dst || count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        dst[i] += src[i];
    }
}

inline void AccumulateKV(const float* src, float* dst, int count) noexcept {
    if (!AccumulateKV_AVX512(src, dst, count)) {
        AccumulateKV_Fallback(src, dst, count);
    }
}

inline bool AccumulateScaledKV_AVX512(const float* src, float* dst, int count, float scale) noexcept {
    if (!src || !dst || count <= 0) {
        return false;
    }
    if (!HasAVX512Runtime()) {
        return false;
    }
    return kv_accumulate_scaled_avx512(src, dst, count, scale) != 0;
}

inline void AccumulateScaledKV_Fallback(const float* src, float* dst, int count, float scale) noexcept {
    if (!src || !dst || count <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        dst[i] += src[i] * scale;
    }
}

inline void AccumulateScaledKV(const float* src, float* dst, int count, float scale) noexcept {
    if (!AccumulateScaledKV_AVX512(src, dst, count, scale)) {
        AccumulateScaledKV_Fallback(src, dst, count, scale);
    }
}

}  // namespace KernelOps
}  // namespace RawrXD
