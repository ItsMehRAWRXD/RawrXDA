/**
 * @file codec_stub.cpp
 * @brief Stub implementation of codec namespace functions for tests
 * 
 * These stubs bypass actual compression for test builds without ZLIB.
 * For production, use utils/codec.cpp with actual zlib.
 */

#include <QByteArray>

namespace codec {
    // Stub deflate - passes through without compression
    QByteArray deflate(const QByteArray& input, bool* success) {
        if (success) *success = true;
        return input;  // Pass through unchanged
    }
    
    // Stub inflate - passes through without decompression
    QByteArray inflate(const QByteArray& input, bool* success) {
        if (success) *success = true;
        return input;  // Pass through unchanged
    }
}
