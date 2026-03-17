// =============================================================================
// analyzer_distiller.h — C ABI header for RawrXD-AnalyzerDistiller MASM module
// =============================================================================
// Declares extern "C" symbols exported by RawrXD-AnalyzerDistiller.asm
// The MASM module: parses GGUF v3, analyzes tensor structure, distills to .exec
//
// When RAWR_HAS_MASM=1 and the ASM .obj is linked, these resolve to native ASM.
// Otherwise, the stub file (analyzer_distiller_stubs.cpp) provides C++ fallbacks.
// =============================================================================
#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Opaque structures (match MASM struct layouts)
// ---------------------------------------------------------------------------

// GGUF file header (24 bytes)
struct AD_GGUFHeader {
    uint64_t magic;         // 0x46554747666C6C67 ("ggllFUGF")
    uint32_t version;       // Must be 3
    uint64_t tensor_count;
    uint64_t metadata_kv;
};

// Tensor metadata (120 bytes, matches MASM TensorInfo)
struct AD_TensorInfo {
    uint64_t name_len;
    uint64_t name_ptr;      // Pointer to name buffer
    uint32_t dtype;          // GGUF dtype enum
    uint64_t shape_rank;
    uint64_t shape[4];       // Max 4D
    uint64_t offset;         // File offset (never read for analysis)
    uint32_t pattern_type;   // FFN=1, ATTN=2, EMBED=3, NORM=4, UNKNOWN=0
    uint64_t param_count;    // Computed parameter count
    uint32_t layer_index;
    uint64_t data_offset;
};

// Analysis result summary
struct AD_AnalysisResult {
    uint64_t ffn_blocks;
    uint64_t attn_heads;
    uint64_t embed_tokens;
    uint64_t norm_layers;
    uint64_t unknown_layers;
    uint64_t layer_count;
    uint64_t total_params;
    uint64_t tensor_count;
    uint64_t hidden_dim;
    uint32_t size_tier;
};

// Pattern type constants
constexpr uint32_t AD_PATTERN_UNKNOWN   = 0;
constexpr uint32_t AD_PATTERN_FFN       = 1;
constexpr uint32_t AD_PATTERN_ATTENTION = 2;
constexpr uint32_t AD_PATTERN_EMBED     = 3;
constexpr uint32_t AD_PATTERN_NORM      = 4;

// GGUF dtype constants
constexpr uint32_t AD_GGUF_TYPE_FLOAT32 = 6;
constexpr uint32_t AD_GGUF_TYPE_FLOAT16 = 1;  // INT8 in spec, F16 in practice
constexpr uint32_t AD_GGUF_TYPE_STRING  = 8;

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Core API — GGUF Analysis Pipeline
// ---------------------------------------------------------------------------

// Open a GGUF file for analysis. Returns HANDLE or -1 on error.
intptr_t  AD_OpenGGUFFile(const char* path);

// Validate the GGUF header (magic, version, counts). Returns 1/0.
int       AD_ValidateGGUFHeader(AD_GGUFHeader* header, intptr_t fileHandle);

// Skip metadata key-value pairs (positions file pointer past metadata). Returns 1/0.
int       AD_SkipMetadataKV(uint64_t kvCount, intptr_t fileHandle);

// Parse all tensor metadata (names, shapes, dtypes) without loading tensor data. Returns 1/0.
int       AD_ParseTensorMetadata(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis,
                                 intptr_t fileHandle);

// Identify pattern type for a single tensor (FFN/ATTN/EMBED/NORM). Modifies structs.
void      AD_IdentifyPattern(AD_TensorInfo* tensorInfo, AD_AnalysisResult* analysis);

// Analyze full structure: classify all tensors, compute layer count. Returns 1/0.
int       AD_AnalyzeStructure(AD_TensorInfo* tensorTable, AD_AnalysisResult* analysis);

// Distill analysis to .exec format and write to file. Returns 1/0.
int       AD_DistillToExec(const char* outputPath, AD_AnalysisResult* analysis,
                           intptr_t fileHandle);

// Write .exec file from analysis result. Returns 1/0.
int       AD_WriteExecFile(const char* outputPath, AD_AnalysisResult* analysis);

// Count parameters for a single tensor (product of shape dimensions). Returns count.
uint64_t  AD_CountParameters(AD_TensorInfo* tensorInfo);

// Extract layer index from tensor name string (e.g., "blk.12.attn" → 12). Returns -1 if none.
int32_t   AD_ExtractLayerIndex(const char* name);

// ---------------------------------------------------------------------------
// Convenience: Full pipeline (open → validate → parse → analyze → distill)
// ---------------------------------------------------------------------------
int       AD_ProcessGGUF(const char* inputPath, const char* outputExecPath);

#ifdef __cplusplus
}
#endif
