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
    constexpr uint32_t GGUF_VALUE_TYPE_STRING = 1;
    constexpr uint32_t GGUF_VALUE_TYPE_UINT32 = 4;
    constexpr uint32_t GGUF_VALUE_TYPE_INT32 = 5;
    constexpr uint32_t GGUF_VALUE_TYPE_FLOAT32 = 6;
    constexpr uint32_t GGUF_VALUE_TYPE_ARRAY = 9;

    // Default zone memory limit (MB)
    constexpr size_t DEFAULT_ZONE_MEMORY_MB = 512;

    // Common metadata keys
    constexpr const char* META_GENERAL_ARCHITECTURE = "general.architecture";
    constexpr const char* META_LLAMA_BLOCK_COUNT = "llama.block_count";
    constexpr const char* META_LLAMA_CONTEXT_LENGTH = "llama.context_length";
    constexpr const char* META_LLAMA_EMBEDDING_LENGTH = "llama.embedding_length";
    constexpr const char* META_LLAMA_VOCAB_SIZE = "llama.vocab_size";
    constexpr const char* META_LLAMA_HEAD_COUNT = "llama.attention.head_count";
    constexpr const char* META_LLAMA_KV_HEAD_COUNT = "llama.attention.head_count_kv";

    // Tokenizer metadata keys
    constexpr const char* META_TOKENIZER_TOKENS = "tokenizer.ggml.tokens";
    constexpr const char* META_TOKENIZER_SCORES = "tokenizer.ggml.scores";
    constexpr const char* META_TOKENIZER_TOKEN_TYPE = "tokenizer.ggml.token_type";

    // Zone names
    constexpr const char* ZONE_EMBEDDING = "embedding";
    constexpr const char* ZONE_ATTENTION = "attention";
    constexpr const char* ZONE_FFN = "ffn";
    constexpr const char* ZONE_OUTPUT = "output";
}
