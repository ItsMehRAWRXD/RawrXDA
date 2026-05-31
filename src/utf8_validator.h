#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/**
 * UTF8Validator: Safeguard against tokenizer/decoder corruption
 * 
 * Detects invalid UTF-8 sequences that produce replacement character (U+FFFD, "�")
 * and validates vocab pointer correctness before text emission.
 * 
 * This directly addresses output corruption where repeated "�" chars indicate:
 * - Broken UTF-8 sequences from tokenizer mismatch
 * - Vocab pointer out-of-bounds or misaligned
 * - Token ID not in valid range
 */

class UTF8Validator
{
public:
    /**
     * Check if a single byte is a valid UTF-8 start/continuation byte
     * UTF-8 encoding:
     * - 0xxxxxxx           = ASCII (1 byte)
     * - 110xxxxx 10xxxxxx  = 2-byte sequence
     * - 1110xxxx 10...     = 3-byte sequence
     * - 11110xxx 10...     = 4-byte sequence
     */
    static inline bool is_valid_utf8_byte(uint8_t b)
    {
        // Valid start bytes: 0xxxxxxx, 110xxxxx, 1110xxxx, 11110xxx
        if ((b & 0x80) == 0)
            return true;  // ASCII
        if ((b & 0xE0) == 0xC0)
            return true;  // 2-byte start
        if ((b & 0xF0) == 0xE0)
            return true;  // 3-byte start
        if ((b & 0xF8) == 0xF0)
            return true;  // 4-byte start
        if ((b & 0xC0) == 0x80)
            return true;  // Continuation byte
        return false;
    }

    /**
     * Validate a complete UTF-8 string
     * Returns: true if valid, false if invalid/incomplete sequences found
     */
    static inline bool is_valid_utf8_string(const char* str)
    {
        if (!str)
            return false;

        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(str);
        size_t i = 0;

        while (bytes[i] != 0)
        {
            uint8_t b = bytes[i];

            // Single-byte (ASCII)
            if ((b & 0x80) == 0)
            {
                i++;
                continue;
            }

            // Multi-byte sequence
            int expected_len = 0;
            if ((b & 0xE0) == 0xC0)
                expected_len = 2;
            else if ((b & 0xF0) == 0xE0)
                expected_len = 3;
            else if ((b & 0xF8) == 0xF0)
                expected_len = 4;
            else
                return false;  // Invalid start byte

            // Verify continuation bytes
            for (int j = 1; j < expected_len; j++)
            {
                if (bytes[i + j] == 0)
                    return false;  // Incomplete sequence
                if ((bytes[i + j] & 0xC0) != 0x80)
                    return false;  // Invalid continuation byte
            }

            i += expected_len;
        }

        return true;
    }

    /**
     * Sanitize string by replacing invalid UTF-8 sequences with a safe marker
     * Returns sanitized string
     */
    static inline std::string sanitize_utf8_string(const char* str)
    {
        std::string result;
        if (!str)
            return result;

        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(str);
        size_t i = 0;

        while (bytes[i] != 0)
        {
            uint8_t b = bytes[i];

            // Single-byte (ASCII)
            if ((b & 0x80) == 0)
            {
                result += (char)b;
                i++;
                continue;
            }

            // Multi-byte sequence
            int expected_len = 0;
            if ((b & 0xE0) == 0xC0)
                expected_len = 2;
            else if ((b & 0xF0) == 0xE0)
                expected_len = 3;
            else if ((b & 0xF8) == 0xF0)
                expected_len = 4;
            else
            {
                // Invalid start byte, replace with "?"
                result += '?';
                i++;
                continue;
            }

            // Check continuation bytes
            bool valid = true;
            for (int j = 1; j < expected_len; j++)
            {
                if (bytes[i + j] == 0 || (bytes[i + j] & 0xC0) != 0x80)
                {
                    valid = false;
                    break;
                }
            }

            if (valid)
            {
                // Valid sequence, copy it
                for (int j = 0; j < expected_len; j++)
                    result += (char)bytes[i + j];
                i += expected_len;
            }
            else
            {
                // Invalid sequence, replace with "?"
                result += '?';
                i++;
            }
        }

        return result;
    }

    /**
     * Validate token ID is within vocab bounds
     * Also checks for common bounds-checking errors
     */
    static inline bool is_valid_token_id(uint32_t token_id, uint32_t vocab_size)
    {
        if (vocab_size == 0)
            return false;
        if (token_id >= vocab_size)
            return false;
        // Detect suspicious values
        if (token_id == 0xFFFFFFFFu)
            return false;  // -1 cast
        if (token_id == 0xFFFFFFFEu)
            return false;  // Suspicious boundary
        return true;
    }

    /**
     * Check if a string contains the replacement character (U+FFFD = "�")
     * This indicates broken UTF-8 emission somewhere in the pipeline
     */
    static inline bool contains_replacement_char(const std::string& str)
    {
        // U+FFFD in UTF-8 is: EF BF BD
        static const uint8_t replacement_utf8[] = {0xEF, 0xBF, 0xBD};

        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(str.c_str());
        for (size_t i = 0; i + 2 < str.length(); i++)
        {
            if (bytes[i] == replacement_utf8[0] &&
                bytes[i + 1] == replacement_utf8[1] &&
                bytes[i + 2] == replacement_utf8[2])
            {
                return true;
            }
        }
        return false;
    }

    /**
     * Comprehensive vocab pointer validation
     * Checks for common memory corruption patterns
     */
    static inline bool is_valid_vocab_pointer(const char* vocab_entry, const char* vocab_base, size_t vocab_total_size)
    {
        if (!vocab_entry || !vocab_base)
            return false;

        // Check if pointer is within vocab region
        ptrdiff_t offset = vocab_entry - vocab_base;
        if (offset < 0 || static_cast<size_t>(offset) >= vocab_total_size)
            return false;

        // Check for null-termination within reasonable bounds
        size_t max_string_len = 256;
        size_t remaining = vocab_total_size - offset;
        for (size_t i = 0; i < std::min(max_string_len, remaining); i++)
        {
            if (vocab_entry[i] == 0)
                return true;  // Valid: found null terminator
        }

        return false;  // No null terminator found (corrupted)
    }

    /**
     * Log detailed diagnostic info about a token
     * Useful for triaging tokenizer/vocab mismatches
     */
    struct TokenDiagnostic
    {
        uint32_t token_id = 0;
        bool id_valid = false;
        const char* vocab_string = nullptr;
        bool pointer_valid = false;
        bool string_valid_utf8 = false;
        bool string_has_replacement = false;
        size_t string_length = 0;

        std::string describe() const
        {
            char buffer[256];
            snprintf(buffer, sizeof(buffer),
                "token=%u valid=%d ptr=%d utf8=%d repl=%d len=%zu",
                token_id,
                id_valid ? 1 : 0,
                pointer_valid ? 1 : 0,
                string_valid_utf8 ? 1 : 0,
                string_has_replacement ? 1 : 0,
                string_length);
            return std::string(buffer);
        }
    };

    /**
     * Perform full token diagnostic
     */
    static inline TokenDiagnostic diagnose_token(uint32_t token_id, const char* vocab_string, 
                                                  uint32_t vocab_size, const char* vocab_base, 
                                                  size_t vocab_total_size)
    {
        TokenDiagnostic diag;
        diag.token_id = token_id;
        diag.id_valid = is_valid_token_id(token_id, vocab_size);
        diag.vocab_string = vocab_string;

        if (vocab_string)
        {
            diag.pointer_valid = is_valid_vocab_pointer(vocab_string, vocab_base, vocab_total_size);
            diag.string_length = strlen(vocab_string);
            diag.string_valid_utf8 = is_valid_utf8_string(vocab_string);
            diag.string_has_replacement = contains_replacement_char(vocab_string);
        }

        return diag;
    }
};
