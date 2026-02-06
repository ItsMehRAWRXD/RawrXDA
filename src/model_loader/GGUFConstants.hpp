#pragma once

/**
 * @brief GGUF file format constants.
 *
 * Centralizes all magic numbers and constants used in GGUF parsing to improve
 * code readability and maintainability.
 *
 * GGUF v3 Specification: All 13 metadata value types and full GGMLType coverage.
 * Reference: https://github.com/ggerganov/ggml/blob/master/docs/gguf.md
 */

#include <cstdint>

namespace GGUFConstants
{
    // GGUF magic number ("GGUF" in little-endian)
    constexpr uint32_t GGUF_MAGIC = 0x46554747;

    // Supported GGUF version
    constexpr uint32_t GGUF_VERSION = 3;

    // ============================================================================
    // GGUF metadata value types (COMPLETE v3 spec — all 13 types)
    // ============================================================================
    constexpr uint32_t GGUF_VALUE_TYPE_UINT8    = 0;
    constexpr uint32_t GGUF_VALUE_TYPE_INT8     = 1;
    constexpr uint32_t GGUF_VALUE_TYPE_UINT16   = 2;
    constexpr uint32_t GGUF_VALUE_TYPE_INT16    = 3;
    constexpr uint32_t GGUF_VALUE_TYPE_UINT32   = 4;
    constexpr uint32_t GGUF_VALUE_TYPE_INT32    = 5;
    constexpr uint32_t GGUF_VALUE_TYPE_FLOAT32  = 6;
    constexpr uint32_t GGUF_VALUE_TYPE_BOOL     = 7;
    constexpr uint32_t GGUF_VALUE_TYPE_STRING   = 8;
    constexpr uint32_t GGUF_VALUE_TYPE_ARRAY    = 9;
    constexpr uint32_t GGUF_VALUE_TYPE_UINT64   = 10;
    constexpr uint32_t GGUF_VALUE_TYPE_INT64    = 11;
    constexpr uint32_t GGUF_VALUE_TYPE_FLOAT64  = 12;

    // Default zone memory limit (MB)
    constexpr size_t DEFAULT_ZONE_MEMORY_MB = 512;

    // GGUF tensor data alignment (per spec)
    constexpr uint64_t GGUF_TENSOR_ALIGNMENT = 32;

    // Common metadata keys
    constexpr const char* META_GENERAL_ARCHITECTURE = "general.architecture";
    constexpr const char* META_GENERAL_NAME = "general.name";
    constexpr const char* META_GENERAL_QUANTIZATION_VERSION = "general.quantization_version";
    constexpr const char* META_GENERAL_FILE_TYPE = "general.file_type";
    constexpr const char* META_LLAMA_BLOCK_COUNT = "llama.block_count";
    constexpr const char* META_LLAMA_CONTEXT_LENGTH = "llama.context_length";
    constexpr const char* META_LLAMA_EMBEDDING_LENGTH = "llama.embedding_length";
    constexpr const char* META_LLAMA_VOCAB_SIZE = "llama.vocab_size";
    constexpr const char* META_LLAMA_HEAD_COUNT = "llama.attention.head_count";
    constexpr const char* META_LLAMA_HEAD_COUNT_KV = "llama.attention.head_count_kv";
    constexpr const char* META_LLAMA_FFN_LENGTH = "llama.feed_forward_length";
    constexpr const char* META_TOKENIZER_MODEL = "tokenizer.ggml.model";
    constexpr const char* META_TOKENIZER_TOKENS = "tokenizer.ggml.tokens";
    constexpr const char* META_TOKENIZER_SCORES = "tokenizer.ggml.scores";
    constexpr const char* META_TOKENIZER_TOKEN_TYPE = "tokenizer.ggml.token_type";

    // Zone names
    constexpr const char* ZONE_EMBEDDING = "embedding";
    constexpr const char* ZONE_ATTENTION = "attention";
    constexpr const char* ZONE_FFN = "ffn";
    constexpr const char* ZONE_OUTPUT = "output";

    // Model source types (for unified model resolution)
    enum class ModelSourceType : uint32_t {
        LOCAL_FILE       = 0,  // Direct filesystem path (.gguf file)
        HUGGINGFACE_REPO = 1,  // HuggingFace model ID (e.g., "TheBloke/Llama-2-7B-GGUF")
        OLLAMA_BLOB      = 2,  // Ollama model name (e.g., "llama3.2:3b")
        HTTP_URL         = 3,  // Direct HTTP/HTTPS URL to .gguf file
        UNKNOWN          = 255
    };
}
