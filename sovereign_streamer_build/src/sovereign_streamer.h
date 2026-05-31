#pragma once

#include <windows.h>
#include <immintrin.h> // For AVX-512 intrinsics

// --- Configuration ---
// Tile dimensions - must be a multiple of AVX-512 vector width (16 floats)
constexpr size_t TILE_DIM_M = 32;
constexpr size_t TILE_DIM_N = 64;
constexpr size_t TILE_SIZE_BYTES = TILE_DIM_M * TILE_DIM_N * sizeof(float);

// --- Data Structures ---

// Represents the multi-layered, quantized data for a single tensor tile on disk.
// This is the "360p to 16k" ladder.
struct AlignedTileData {
    // L0: "360p" - The base 4-bit quantized data. Always required.
    // Stored as [scale (fp16)] [zero_point (fp16)] [q4_data]
    BYTE* q4_data_l0;
    size_t q4_data_l0_size;

    // L1: "+2 bits" - The first refinement layer to reach 6-bit precision.
    BYTE* q2_refinement_l1;
    size_t q2_refinement_l1_size;

    // L2: "+2 bits" - The second refinement layer to reach 8-bit precision.
    BYTE* q2_refinement_l2;
    size_t q2_refinement_l2_size;
};

// Holds the metadata for a single model layer, including tensor shapes and
// a map of tile offsets within the memory-mapped file.
struct LayerMetadata {
    // TODO: Add tensor dimension info (rows, cols, etc.)
    // TODO: Add a map or flat array of AlignedTileData offsets
};

// The core context for the streaming engine.
// Manages file handles, memory mappings, and the compute buffer.
struct SovereignStreamerContext {
    HANDLE h_model_file;
    HANDLE h_file_mapping;
    PVOID p_mapped_base; // Base address of the entire mapped model file
    
    // A small, pinned buffer in physical RAM where tiles are decrypted and processed.
    // This is our "working window".
    PVOID p_compute_buffer; 
    size_t compute_buffer_size;

    // TODO: Add LayerMetadata array
};

// --- Core Functions ---

/**
 * @brief Initializes the streamer, maps the encrypted model file into virtual memory.
 * 
 * @param ctx Pointer to the streamer context.
 * @param model_path Path to the encrypted model file.
 * @return TRUE on success, FALSE on failure.
 */
BOOL streamer_init(SovereignStreamerContext* ctx, const wchar_t* model_path);

/**
 * @brief Main execution loop to process a model layer by streaming tiles.
 * 
 * @param ctx The streamer context.
 * @param hidden_state The input/output hidden state buffer.
 * @param layer_meta Metadata for the layer to be processed.
 * @return TRUE on success, FALSE on failure.
 */
BOOL streamer_execute_layer(SovereignStreamerContext* ctx, float* hidden_state, const LayerMetadata* layer_meta);

/**
 * @brief Cleans up resources, unmaps files, and securely zeroes buffers.
 * 
 * @param ctx The streamer context.
 */
void streamer_shutdown(SovereignStreamerContext* ctx);


// --- Security & JIT Kernels ---

/**
 * @brief JIT-compiled AVX-512 kernel to perform dequantization and GEMM.
 * This is a placeholder for the actual JIT emitter.
 * 
 * @param weights_q4 The 4-bit quantized weight tile.
 * @param input The input activation vector.
 * @param output The output vector.
 */
void kernel_avx512_q4_gemm(const BYTE* weights_q4, const float* input, float* output);

/**
 * @brief Securely erases the content of a memory buffer.
 * 
 * @param buffer Pointer to the buffer to be wiped.
 * @param size The size of the buffer in bytes.
 */
void secure_zero_memory(void* buffer, size_t size);
