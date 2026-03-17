#pragma once

/**
 * @brief GGUF file format constants.
 *
 * Centralizes all magic numbers and constants used in GGUF parsing to improve
 * code readability and maintainability.
 */

namespace GGUFConstants
{
    // GGUF magic number ("GGUF" in little-endian)
    constexpr uint32_t GGUF_MAGIC = 0x46554747;

    // Supported GGUF version
    constexpr uint32_t GGUF_VERSION = 3;

    // GGUF metadata value types
    constexpr uint32_t GGUF_VALUE_TYPE_STRING = 1;          // Legacy/compat path
    constexpr uint32_t GGUF_VALUE_TYPE_UINT32 = 4;
    constexpr uint32_t GGUF_VALUE_TYPE_INT32 = 5;
    constexpr uint32_t GGUF_VALUE_TYPE_FLOAT32 = 6;
    // Spec-aligned values (GGUF v3)
    constexpr uint32_t GGUF_VALUE_TYPE_BOOL = 7;
    constexpr uint32_t GGUF_VALUE_TYPE_STRING_SPEC = 8;
    constexpr uint32_t GGUF_VALUE_TYPE_ARRAY = 9;
    constexpr uint32_t GGUF_VALUE_TYPE_UINT64 = 10;
    constexpr uint32_t GGUF_VALUE_TYPE_INT64 = 11;
    constexpr uint32_t GGUF_VALUE_TYPE_FLOAT64 = 12;

    // Default zone memory limit (MB)
    constexpr size_t DEFAULT_ZONE_MEMORY_MB = 512;

    // Common metadata keys
    constexpr const char* META_GENERAL_ARCHITECTURE = "general.architecture";
    constexpr const char* META_LLAMA_BLOCK_COUNT = "llama.block_count";
    constexpr const char* META_LLAMA_CONTEXT_LENGTH = "llama.context_length";
    constexpr const char* META_LLAMA_EMBEDDING_LENGTH = "llama.embedding_length";
    constexpr const char* META_LLAMA_VOCAB_SIZE = "llama.vocab_size";

    // Zone names
    constexpr const char* ZONE_EMBEDDING = "embedding";
    constexpr const char* ZONE_ATTENTION = "attention";
    constexpr const char* ZONE_FFN = "ffn";
    constexpr const char* ZONE_OUTPUT = "output";
}
