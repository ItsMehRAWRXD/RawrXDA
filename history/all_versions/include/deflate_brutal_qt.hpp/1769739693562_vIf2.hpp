#pragma once
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <zlib.h>
#include "brutal_gzip.h"

#if !defined(QT_CORE_LIB) && !defined(QT_VERSION)
class QByteArray {
public:
    QByteArray() = default;
    QByteArray(const char* data, int len)
        : bytes_(data ? data : "", data ? data + len : data) {}

    bool isEmpty() const { return bytes_.empty(); }
    int size() const { return static_cast<int>(bytes_.size()); }
    const char* constData() const { return bytes_.empty() ? nullptr : bytes_.data(); }
    char* data() { return bytes_.empty() ? nullptr : bytes_.data(); }

private:
    std::vector<char> bytes_;
};
#endif

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
    
    std::uint64_t packedSz = 0;
    void* p = deflate_brutal_masm(
        reinterpret_cast<const void*>(in.constData()),
        in.size(),
        reinterpret_cast<size_t*>(&packedSz)
    );
    
    if (!p) return {};  // malloc failure
    
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
    
    std::uint64_t packedSz = 0;
    void* p = deflate_brutal_masm(
        data,
        size,
        reinterpret_cast<size_t*>(&packedSz)
    );
    
    if (!p) return {};
    
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
    void* out_buf = malloc(max_uncompressed);
    if (!out_buf) return {};
    
    size_t out_len = 0;
    
#ifdef HAS_BRUTAL_INFLATE_MASM
    // Use MASM inflate if available
    extern "C" int inflate_brutal_masm(const void* src, size_t src_len, 
                                       void* dst, size_t dst_len, size_t* out_len);
    int result = inflate_brutal_masm(
        reinterpret_cast<const void*>(compressed.constData()),
        compressed.size(),
        out_buf,
        max_uncompressed,
        &out_len
    );
    
    if (result != 0) {
        free(out_buf);
        return {};
    }
#else
    // Fallback: use Qt's built-in zlib (qUncompress for gzip with custom header handling)
    // Since brutal format is RFC 1952 gzip, we need to strip the gzip header/footer manually
    
    // Skip gzip header (10 bytes minimum)
    const unsigned char* data = reinterpret_cast<const unsigned char*>(compressed.constData());
    size_t data_len = compressed.size();
    
    if (data_len < 18) {  // Minimum: 10-byte header + data + 8-byte footer
        free(out_buf);
        return {};
    }
    
    // Verify gzip magic number
    if (data[0] != 0x1f || data[1] != 0x8b) {
        free(out_buf);
        return {};
    }
    
    // Skip to deflate data (skip variable-length gzip header)
    size_t header_size = 10;
    if (data[3] & 0x04) {  // FEXTRA flag
        header_size += 2 + (data[header_size] | (data[header_size + 1] << 8));
    }
    if (data[3] & 0x08) {  // FNAME flag
        while (header_size < data_len && data[header_size] != 0) header_size++;
        header_size++;
    }
    if (data[3] & 0x10) {  // FCOMMENT flag
        while (header_size < data_len && data[header_size] != 0) header_size++;
        header_size++;
    }
    if (data[3] & 0x02) {  // FHCRC flag
        header_size += 2;
    }
    
    // Extract raw deflate data (without 8-byte gzip footer)
    QByteArray deflateData(reinterpret_cast<const char*>(data + header_size), 
                           data_len - header_size - 8);
    
    // Use Qt's qUncompress (handles raw DEFLATE)
    QByteArray decompressed = qUncompress(deflateData);
    free(out_buf);
    return decompressed;
#endif
    
    QByteArray result(reinterpret_cast<const char*>(out_buf), static_cast<int>(out_len));
    free(out_buf);
    return result;
}

} // namespace brutal
