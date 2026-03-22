#pragma once

#include <cstddef>
#include <cstdint>

// Standalone ggml-compatible K-quant row dequantization (no ggml link).
// Blocks match llama.cpp / ggml-common.h layout (QK_K = 256).

void rawrxd_dequantize_row_q4_K(const void* rawBlocks, float* y, std::int64_t k);
void rawrxd_dequantize_row_q2_K(const void* rawBlocks, float* y, std::int64_t k);

constexpr std::size_t rawrxd_qk_k() { return 256; }
constexpr std::size_t rawrxd_q4_k_block_bytes() { return 144; }
constexpr std::size_t rawrxd_q2_k_block_bytes() { return 84; }
