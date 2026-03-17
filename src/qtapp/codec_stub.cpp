/**
 * @file codec_stub.cpp
 * @brief Stub implementation of codec namespace functions for tests
 * 
 * These stubs bypass actual compression for test builds without ZLIB.
 * For production, use utils/codec.cpp with actual zlib.
 */

namespace codec {
    // Stub deflate - passes through without compression
    std::vector<uint8_t> deflate(const std::vector<uint8_t>& input, bool* success) {
        if (success) *success = true;
        return input;  // Pass through unchanged
    }
    
    // Stub inflate - passes through without decompression
    std::vector<uint8_t> inflate(const std::vector<uint8_t>& input, bool* success) {
        if (success) *success = true;
        return input;  // Pass through unchanged
    }
}

