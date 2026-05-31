// d:/rawrxd/src/atc_codec.h
#pragma once

#include <cstdint>
#include <windows.h>
#include <immintrin.h> // For AVX-512 intrinsics
#include "braided_quantizer.h"

// Represents the on-disk layout and metadata for a single tensor tile.
// This is the "map" that allows us to stream different quality levels (LODs).
using TileMeta = BraidedTileMeta;

// A buffer to hold a working tile in its decoded, floating-point state.
// Sized for a common block size like 64x64.
struct TileBuffer {
    float data[64 * 64];
};

// The Adaptive Tensor Codec (ATC)
// This class orchestrates the streaming, decoding, and execution of model tensors
// on a tile-by-tile basis, managing the memory footprint.
class AdaptiveTensorCodec {
public:
    AdaptiveTensorCodec();
    ~AdaptiveTensorCodec();

    // Maps the model file into virtual address space without committing physical RAM.
    bool map_model(const wchar_t* model_path);

    // Main execution loop that processes a sequence of tokens.
    bool generate_tokens(int* input_ids, int num_tokens);

private:
    // File and memory mapping handles
    HANDLE h_model_file;
    HANDLE h_map_object;
    void*  mapped_base_addr; // Base address of the entire mapped file (virtual)

    // Prefetches a tile's data from disk into the OS standby list.
    void prefetch_tile(const TileMeta& tile);

    // Decodes the base L0 (4-bit) tile data into a float buffer.
    void decode_tile_l0(const TileMeta& tile, TileBuffer* buffer);

    // Applies a refinement bit-plane to an already decoded tile.
    void refine_tile(const TileMeta& tile, int level, TileBuffer* buffer);

    // Determines if a higher-quality refinement is needed based on output variance.
    bool needs_refinement(const TileBuffer* output_buffer);

    // The core compute kernel for a tile (e.g., GEMM).
    // This would be a function pointer to a JIT-compiled or static AVX-512 kernel.
    void compute_tile(const TileBuffer* input, const TileBuffer* weights, TileBuffer* output);

    // AVX-512 kernel for dequantizing a 4-bit block.
    void dequant_q4_avx512(const void* q_data, float* f_data, float scale, int8_t zero_point, int n_blocks);
};
