#pragma once

#include <vector>
#include <cstdint>

extern "C" {
    void flash_attention(float* q, float* k, float* v, int batch_size, int seq_len, int head_size, int num_heads, float* output);
    void attention_baseline(float* q, float* k, float* v, int batch_size, int seq_len, int head_size, int num_heads, float* output);
}