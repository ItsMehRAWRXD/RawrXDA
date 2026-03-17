#pragma once
#include <cstdint>
#include <cstdlib>
#include <QtCore/QByteArray>
#include "brutal_gzip.h"

namespace brutal {

/**
 * @brief Compress QByteArray using brutal MASM stored-block gzip
 * @param in Raw input data
 * @return Compressed gzip stream (RFC 1952 compliant)
 * 
 * Ultra-fast, deterministic-size gzip compression using only stored blocks.
 * No Huffman, no LZ77 – pure memcpy speed with gzip framing.
 * Perfect for GGUF tensor caching, streaming inference, or speed-critical paths.
 */
inline QByteArray compress(const QByteArray& in)
{
    if (in.isEmpty()) return {};
    
    // Calculate maximum compressed size (header + blocks + footer)
    const size_t hdr = 10;  // gzip header
    const size_t ftr = 8;   // gzip footer (CRC32 + ISIZE)
    const size_t blockOverhead = 5;  // per 65535-byte block
    size_t numBlocks = (in.size() + 65534) / 65535;
    size_t packedSz = hdr + (numBlocks * (blockOverhead + 65535)) + ftr;
    
    void* p = std::malloc(packedSz);
    if (!p) return {};
    
    // Use our new MASM deflate function
    extern "C" size_t AsmDeflate(const void* src, size_t src_len, 
                                 void* dst, size_t dst_len);
    size_t result = AsmDeflate(
        reinterpret_cast<const void*>(in.constData()),
        in.size(),
        p,
        packedSz
    );
    
    if (result == 0) {
        std::free(p);
        return {};
    }
    packedSz = result;
    
    QByteArray out(reinterpret_cast<const char*>(p), static_cast<int>(packedSz));
    std::free(p);
    return out;
}

/**
 * @brief Compress raw buffer using brutal MASM stored-block gzip
 * @param data Raw input pointer
 * @param size Input size in bytes
 * @return Compressed gzip stream (RFC 1952 compliant)
 */
inline QByteArray compress(const void* data, std::size_t size)
{
    if (!data || size == 0) return {};
    
    // Calculate maximum compressed size (header + blocks + footer)
    const size_t hdr = 10;  // gzip header
    const size_t ftr = 8;   // gzip footer (CRC32 + ISIZE)
    const size_t blockOverhead = 5;  // per 65535-byte block
    size_t numBlocks = (size + 65534) / 65535;
    size_t packedSz = hdr + (numBlocks * (blockOverhead + 65535)) + ftr;
    
    void* p = std::malloc(packedSz);
    if (!p) return {};
    
    // Use our new MASM deflate function
    extern "C" size_t AsmDeflate(const void* src, size_t src_len, 
                                 void* dst, size_t dst_len);
    size_t result = AsmDeflate(
        data,
        size,
        p,
        packedSz
    );
    
    if (result == 0) {
        std::free(p);
        return {};
    }
    packedSz = result;
    
    QByteArray out(reinterpret_cast<const char*>(p), static_cast<int>(packedSz));
    std::free(p);
    return out;
}

/**
 * @brief Calculate worst-case compressed size for planning/allocation
 * @param rawSize Input size
 * @return Maximum possible compressed size (gzip header + stored blocks + footer)
 * 
 * Formula: header(10) + ceil(rawSize/65535)*5 + rawSize + footer(8)
 */
inline std::size_t maxCompressedSize(std::size_t rawSize)
{
    std::size_t blockCount = (rawSize + 65534) / 65535;
    return 10 + (blockCount * 5) + rawSize + 8;
}

/**
 * @brief Decompress gzip stream using MASM inflate kernel
 * @param compressed Compressed gzip data
 * @return Decompressed raw data, empty if decompression fails
 * 
 * Fast decompression using MASM-optimized inflate algorithm.
 * Handles RFC 1952 gzip format with DEFLATE stored blocks.
 */
inline QByteArray decompress(const QByteArray& compressed)
{
    if (compressed.isEmpty()) return {};
    
    // Try to use MASM inflate if available; fallback to Qt's gzip decompression
    // For stored-block gzip, we can use standard zlib
    // The brutal format is RFC 1952 compliant, so standard tools work
    
    size_t max_uncompressed = compressed.size() * 4;  // Initial guess
    void* out_buf = std::malloc(max_uncompressed);
    if (!out_buf) return {};
    
    // Use our new MASM inflate function
    extern "C" size_t AsmInflate(const void* src, size_t src_len, 
                                  void* dst, size_t dst_len);
    size_t out_len = AsmInflate(
        reinterpret_cast<const void*>(compressed.constData()),
        compressed.size(),
        out_buf,
        max_uncompressed
    );
    
    if (out_len == 0) {
        std::free(out_buf);
        return {};
    }
    
    QByteArray retval(reinterpret_cast<const char*>(out_buf), static_cast<int>(out_len));
    std::free(out_buf);
    return retval;
}

} // namespace brutal
