#include "kv_cache_calculator.h"

namespace RawrXD {

float KVCacheCalculator::recommendQuantization(int32_t target_context,
                                               int64_t available_memory_bytes,
                                               int32_t num_layers,
                                               int32_t hidden_size,
                                               int32_t num_kv_heads)
{
    KVCacheCalculator fp16;
    fp16.context_length = target_context;
    fp16.num_layers = num_layers;
    fp16.hidden_size = hidden_size;
    fp16.num_kv_heads = num_kv_heads;
    fp16.head_dim = hidden_size / (num_kv_heads * 4);
    fp16.bytes_per_element = 2.0f;

    const int64_t neededFp16 = fp16.calculateMemoryBytes();
    if (neededFp16 < static_cast<int64_t>(available_memory_bytes * 0.8f)) {
        return 2.0f;
    }

    KVCacheCalculator int8 = fp16;
    int8.bytes_per_element = 1.0f;
    const int64_t neededInt8 = int8.calculateMemoryBytes();
    if (neededInt8 < static_cast<int64_t>(available_memory_bytes * 0.8f)) {
        return 1.0f;
    }

    return 0.5f;
}

bool KVCacheCalculator::canFitContext(int32_t target_context,
                                      int64_t available_memory_bytes,
                                      float safety_margin)
{
    KVCacheCalculator calc;
    calc.context_length = target_context;
    calc.bytes_per_element = 1.0f;

    const int64_t needed = calc.calculateMemoryBytes();
    return needed < static_cast<int64_t>(available_memory_bytes * safety_margin);
}

} // namespace RawrXD
