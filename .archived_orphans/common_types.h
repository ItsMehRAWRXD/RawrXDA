#pragma once
#include <cstdint>
#include <vector>
#include <string>

// GGUF magic: "GGUF" = 0x46554747
#define GGUF_MAGIC 0x46554747

enum ggml_type {
    GGML_TYPE_F32  = 0,
    GGML_TYPE_F16  = 1,
    GGML_TYPE_Q4_0 = 2,
    GGML_TYPE_Q4_1 = 3,
    GGML_TYPE_Q5_0 = 6,
    GGML_TYPE_Q5_1 = 7,
    GGML_TYPE_Q8_0 = 8,
    GGML_TYPE_Q2_K = 14,
    GGML_TYPE_Q3_K = 15,
    GGML_TYPE_Q4_K = 16,
    GGML_TYPE_Q5_K = 17,
    GGML_TYPE_Q6_K = 18,
};

// Quantized block structures
struct block_q4_0 {
    uint16_t d;         // delta (scale)
    uint8_t qs[16];     // 32 4-bit weights packed
};

struct block_q4_1 {
    uint16_t d;         // delta
    uint16_t m;         // min
    uint8_t qs[16];     // 32 4-bit weights
};

struct block_q8_0 {
    uint16_t d;         // delta
    int8_t qs[32];      // 32 8-bit weights
};

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> dims;
    ggml_type type;
    uint64_t offset;
    size_t size;
    void* data;
};
