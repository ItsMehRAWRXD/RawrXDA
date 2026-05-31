#pragma once

#include <cstdint>
#include <vector>

namespace weights
{

#pragma pack(push, 1)
struct Q4_KBlock
{
    uint8_t scales[16];
    uint8_t qs[128];
    uint8_t d;
    uint8_t dmin;
};
static_assert(sizeof(Q4_KBlock) == 146, "Q4_K block size mismatch");

struct Q5_KBlock
{
    uint8_t scales[12];
    uint8_t qh[32];
    uint8_t qs[128];
    uint8_t d;
    uint8_t dmin;
};
static_assert(sizeof(Q5_KBlock) == 174, "Q5_K block size mismatch");

struct Q6_KBlock
{
    uint8_t ql[128];
    uint8_t qh[64];
    uint8_t scales[12];
    uint8_t d;
    uint8_t dmin;
};
static_assert(sizeof(Q6_KBlock) == 206, "Q6_K block size mismatch");
#pragma pack(pop)

std::vector<float> dequantize_q4_k(const Q4_KBlock* blocks, int n_blocks);
std::vector<float> dequantize_q5_k(const Q5_KBlock* blocks, int n_blocks);
std::vector<float> dequantize_q6_k(const Q6_KBlock* blocks, int n_blocks);

float matvec_q4_k(const Q4_KBlock* weights, const float* input, int rows, int cols);
float matvec_q5_k(const Q5_KBlock* weights, const float* input, int rows, int cols);
float matvec_q6_k(const Q6_KBlock* weights, const float* input, int rows, int cols);

float matvec_q4_k_avx2(const Q4_KBlock* weights, const float* input, int rows, int cols);
float matvec_q5_k_avx2(const Q5_KBlock* weights, const float* input, int rows, int cols);
float matvec_q6_k_avx2(const Q6_KBlock* weights, const float* input, int rows, int cols);

} // namespace weights
