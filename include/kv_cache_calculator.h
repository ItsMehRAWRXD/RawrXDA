#pragma once

#include <cstdint>

namespace RawrXD {

struct KVCacheCalculator {
    int32_t num_layers = 32;
    int32_t hidden_size = 4096;
    int32_t num_kv_heads = 8;
    int32_t head_dim = 128;
    int32_t context_length = 131072;

    // Precision multiplier: 2.0=FP16, 1.0=INT8, 0.5=INT4
    float bytes_per_element = 1.0f;

    int64_t calculateMemoryBytes() const {
        const int64_t perToken = 2LL * num_layers * num_kv_heads * head_dim;
        const int64_t totalElements = perToken * context_length;
        return static_cast<int64_t>(totalElements * bytes_per_element);
    }

    double calculateMemoryGB() const {
        return calculateMemoryBytes() / (1024.0 * 1024.0 * 1024.0);
    }

    static float recommendQuantization(int32_t target_context,
                                       int64_t available_memory_bytes,
                                       int32_t num_layers = 32,
                                       int32_t hidden_size = 4096,
                                       int32_t num_kv_heads = 8);

    static bool canFitContext(int32_t target_context,
                              int64_t available_memory_bytes,
                              float safety_margin = 0.8f);
};

} // namespace RawrXD
