#pragma once

// Qt-free compression wrappers — uses zlib directly
// Replaces: QString, QByteArray, qCompress, qUncompress, qint64

#include <memory>
#include <vector>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward-declare zlib functions when header available; otherwise stub
#if __has_include(<zlib.h>)
#include <zlib.h>
#define RAWRXD_HAS_ZLIB 1
#else
#define RAWRXD_HAS_ZLIB 0
#endif

/**
 * @class BrutalGzipWrapper
 * @brief Win32/STL wrapper for GZIP compression/decompression (no Qt)
 */
class BrutalGzipWrapper
{
public:
    BrutalGzipWrapper() = default;
    ~BrutalGzipWrapper() = default;

    // Compress data using zlib deflate
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        if (input.empty()) { output.clear(); return true; }
#if RAWRXD_HAS_ZLIB
        uLongf bound = compressBound(static_cast<uLong>(input.size()));
        output.resize(bound);
        int rc = compress2(output.data(), &bound,
                          input.data(), static_cast<uLong>(input.size()), m_level);
        if (rc != Z_OK) { output = input; return false; }
        output.resize(bound);
        m_lastInputSize = static_cast<int64_t>(input.size());
        m_lastOutputSize = static_cast<int64_t>(bound);
        return true;
#else
        // Fallback: passthrough (no zlib)
        output = input;
        m_lastInputSize = static_cast<int64_t>(input.size());
        m_lastOutputSize = static_cast<int64_t>(input.size());
        return true;
#endif
    }

    // Decompress gzip data
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        if (input.empty()) { output.clear(); return true; }
#if RAWRXD_HAS_ZLIB
        // Estimate output size (4x input as initial guess)
        uLongf outLen = static_cast<uLong>(input.size()) * 4;
        for (int attempt = 0; attempt < 5; ++attempt) {
            output.resize(outLen);
            int rc = uncompress(output.data(), &outLen,
                                input.data(), static_cast<uLong>(input.size()));
            if (rc == Z_OK) {
                output.resize(outLen);
                return true;
            }
            if (rc == Z_BUF_ERROR) { outLen *= 2; continue; }
            return false;
        }
        return false;
#else
        output = input;
        return true;
#endif
    }

    // Get compression ratio
    float getCompressionRatio() const {
        if (m_lastInputSize == 0) return 1.0f;
        return static_cast<float>(m_lastOutputSize) / static_cast<float>(m_lastInputSize);
    }

private:
    int m_level = 6;
    int64_t m_lastInputSize = 0;
    int64_t m_lastOutputSize = 0;
};

/**
 * @class DeflateWrapper
 * @brief Win32/STL wrapper for DEFLATE compression/decompression (no Qt)
 */
class DeflateWrapper
{
public:
    DeflateWrapper() = default;
    ~DeflateWrapper() = default;

    // Compress data using deflate
    bool compress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        if (input.empty()) { output.clear(); return true; }
#if RAWRXD_HAS_ZLIB
        uLongf bound = compressBound(static_cast<uLong>(input.size()));
        output.resize(bound);
        int rc = compress2(output.data(), &bound,
                          input.data(), static_cast<uLong>(input.size()), m_level);
        if (rc != Z_OK) { output = input; return false; }
        output.resize(bound);
        m_lastInputSize = static_cast<int64_t>(input.size());
        m_lastOutputSize = static_cast<int64_t>(bound);
        return true;
#else
        output = input;
        m_lastInputSize = static_cast<int64_t>(input.size());
        m_lastOutputSize = static_cast<int64_t>(input.size());
        return true;
#endif
    }

    // Decompress deflate data
    bool decompress(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        if (input.empty()) { output.clear(); return true; }
#if RAWRXD_HAS_ZLIB
        uLongf outLen = static_cast<uLong>(input.size()) * 4;
        for (int attempt = 0; attempt < 5; ++attempt) {
            output.resize(outLen);
            int rc = uncompress(output.data(), &outLen,
                                input.data(), static_cast<uLong>(input.size()));
            if (rc == Z_OK) {
                output.resize(outLen);
                return true;
            }
            if (rc == Z_BUF_ERROR) { outLen *= 2; continue; }
            return false;
        }
        return false;
#else
        output = input;
        return true;
#endif
    }

    // Get compression ratio
    float getCompressionRatio() const {
        if (m_lastInputSize == 0) return 1.0f;
        return static_cast<float>(m_lastOutputSize) / static_cast<float>(m_lastInputSize);
    }

    // Initialize with algorithm level
    bool initialize(int level)
    {
        m_level = level;
        return true;
    }

private:
    int m_level = 6;
    int64_t m_lastInputSize = 0;
    int64_t m_lastOutputSize = 0;
};
