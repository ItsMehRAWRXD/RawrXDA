// ============================================================================
// Custom ZLIB Wrapper - C++20/Win32 interface to MASM compression
// Qt-free: QByteArray→std::vector<uint8_t>, qDebug→(removed)
// ============================================================================

#ifndef CUSTOM_ZLIB_WRAPPER_H
#define CUSTOM_ZLIB_WRAPPER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cstdio>

// ============================================================================
// External MASM function declarations
// ============================================================================
extern "C" {
    int64_t CustomZlibCompress(const uint8_t* input, uint64_t inputSize,
                               uint8_t* output, uint64_t outputSize);

    int64_t CustomZlibDecompress(const uint8_t* input, uint64_t inputSize,
                                 uint8_t* output, uint64_t outputSize);

    uint64_t CustomZlibGetCompressedSize(uint64_t uncompressedSize);

    int64_t CustomZlibGetDecompressedSize(const uint8_t* compressed, uint64_t compressedSize);

    int64_t CustomZlibInit(void* context, uint64_t contextSize);

    int64_t CustomZlibFree(void* context);
}

// ============================================================================
// CustomZlibWrapper - High-level C++20 interface
// ============================================================================
class CustomZlibWrapper {
public:
    // ========================================================================
    // Compression methods
    // ========================================================================

    /**
     * @brief Compress data using custom MASM DEFLATE implementation
     * @param data Input data to compress
     * @return Compressed data, or empty vector on error
     */
    static std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        if (data.empty()) {
            return {};
        }

        uint64_t maxCompressedSize = CustomZlibGetCompressedSize(data.size());
        std::vector<uint8_t> compressed(maxCompressedSize, 0);

        int64_t compressedSize = CustomZlibCompress(
            data.data(),
            static_cast<uint64_t>(data.size()),
            compressed.data(),
            static_cast<uint64_t>(compressed.size())
        );

        if (compressedSize < 0) {
            return {};
        }

        compressed.resize(static_cast<size_t>(compressedSize));
        return compressed;
    }

    /**
     * @brief Decompress data using custom MASM DEFLATE implementation
     * @param compressed Compressed data
     * @param maxDecompressedSize Maximum expected decompressed size (safety limit)
     * @return Decompressed data, or empty vector on error
     */
    static std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed,
                                           int64_t maxDecompressedSize = 100 * 1024 * 1024) {
        if (compressed.empty()) {
            return {};
        }

        int64_t estimatedSize = CustomZlibGetDecompressedSize(
            compressed.data(),
            static_cast<uint64_t>(compressed.size())
        );

        if (estimatedSize < 0) {
            return {};
        }

        uint64_t outputSize = static_cast<uint64_t>(std::min(estimatedSize, maxDecompressedSize));
        std::vector<uint8_t> decompressed(outputSize, 0);

        int64_t decompressedSize = CustomZlibDecompress(
            compressed.data(),
            static_cast<uint64_t>(compressed.size()),
            decompressed.data(),
            static_cast<uint64_t>(decompressed.size())
        );

        if (decompressedSize < 0) {
            return {};
        }

        decompressed.resize(static_cast<size_t>(decompressedSize));
        return decompressed;
    }

    // ========================================================================
    // Utility methods
    // ========================================================================

    static int64_t getMaxCompressedSize(int64_t uncompressedSize) {
        return static_cast<int64_t>(CustomZlibGetCompressedSize(static_cast<uint64_t>(uncompressedSize)));
    }

    static bool isCompressed(const std::vector<uint8_t>& data) {
        if (data.size() < 2) {
            return false;
        }

        uint8_t cmf = data[0];
        uint8_t flg = data[1];

        if ((cmf & 0x0F) != 8) {
            return false;
        }

        uint16_t check = (static_cast<uint16_t>(cmf) << 8) | flg;
        if ((check % 31) != 0) {
            return false;
        }

        return true;
    }

    static double getCompressionRatio(const std::vector<uint8_t>& compressed,
                                      const std::vector<uint8_t>& uncompressed) {
        if (uncompressed.empty()) {
            return 0.0;
        }
        return 100.0 * static_cast<double>(compressed.size()) / static_cast<double>(uncompressed.size());
    }
};

#endif // CUSTOM_ZLIB_WRAPPER_H
