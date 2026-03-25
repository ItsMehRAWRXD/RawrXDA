<<<<<<< HEAD
#pragma once
/**
 * @file deflate_brutal_qt.hpp
 * @brief MASM brutal gzip compress/decompress — STL-only (Qt-free).
 *
 * Legacy name kept for include compatibility. Uses std::vector<uint8_t> only.
 * For QByteArray-based code, use deflate_brutal_std.hpp (compress_std/compress_buf).
 */
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "brutal_gzip.h"

namespace brutal {

/**
 * @brief Compress buffer using brutal MASM stored-block gzip
 * @param in Raw input data
 * @return Compressed gzip stream (RFC 1952 compliant)
 */
inline std::vector<uint8_t> compress(const std::vector<uint8_t>& in)
{
    if (in.empty()) return {};

    size_t packedSz = 0;
    void* p = deflate_brutal_masm(in.data(), in.size(), &packedSz);
    if (!p) return {};

    std::vector<uint8_t> out(static_cast<size_t>(packedSz));
    std::memcpy(out.data(), p, packedSz);
    std::free(p);
    return out;
}

/**
 * @brief Compress raw buffer using brutal MASM stored-block gzip
 */
inline std::vector<uint8_t> compress(const void* data, std::size_t size)
{
    if (!data || size == 0) return {};

    size_t packedSz = 0;
    void* p = deflate_brutal_masm(data, size, &packedSz);
    if (!p) return {};

    std::vector<uint8_t> out(static_cast<size_t>(packedSz));
    std::memcpy(out.data(), p, packedSz);
    std::free(p);
    return out;
}

/**
 * @brief Worst-case compressed size (gzip header + stored blocks + footer)
 */
inline std::size_t maxCompressedSize(std::size_t rawSize)
{
    std::size_t blockCount = (rawSize + 65534) / 65535;
    return 10 + (blockCount * 5) + rawSize + 8;
}

/**
 * @brief Decompress gzip stream (stored-block format).
 * When HAS_BRUTAL_INFLATE_MASM is defined, uses MASM inflate; otherwise
 * requires zlib/miniz or similar — no Qt dependency.
 */
inline std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed)
{
    if (compressed.empty()) return {};

    size_t max_uncompressed = compressed.size() * 4;
    void* out_buf = std::malloc(max_uncompressed);
    if (!out_buf) return {};

    size_t out_len = 0;

#ifdef HAS_BRUTAL_INFLATE_MASM
    extern "C" int inflate_brutal_masm(const void* src, size_t src_len,
                                       void* dst, size_t dst_len, size_t* out_len);
    int result = inflate_brutal_masm(
        compressed.data(),
        compressed.size(),
        out_buf,
        max_uncompressed,
        &out_len
    );
    if (result != 0) {
        std::free(out_buf);
        return {};
    }
    std::vector<uint8_t> result_vec(out_len);
    std::memcpy(result_vec.data(), out_buf, out_len);
    std::free(out_buf);
    return result_vec;
#else
    (void)out_len;
    // No Qt qUncompress: caller must use brutal_gzip inflate or zlib/miniz.
    std::free(out_buf);
    return {};
#endif
}

} // namespace brutal
=======
#pragma once
/**
 * @file deflate_brutal_qt.hpp
 * @brief MASM brutal gzip compress/decompress — STL-only (Qt-free).
 *
 * Legacy name kept for include compatibility. Uses std::vector<uint8_t> only.
 * For QByteArray-based code, use deflate_brutal_std.hpp (compress_std/compress_buf).
 */
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <cstring>
#include "brutal_gzip.h"

namespace brutal {

/**
 * @brief Compress buffer using brutal MASM stored-block gzip
 * @param in Raw input data
 * @return Compressed gzip stream (RFC 1952 compliant)
 */
inline std::vector<uint8_t> compress(const std::vector<uint8_t>& in)
{
    if (in.empty()) return {};

    size_t packedSz = 0;
    void* p = deflate_brutal_masm(in.data(), in.size(), &packedSz);
    if (!p) return {};

    std::vector<uint8_t> out(static_cast<size_t>(packedSz));
    std::memcpy(out.data(), p, packedSz);
    std::free(p);
    return out;
}

/**
 * @brief Compress raw buffer using brutal MASM stored-block gzip
 */
inline std::vector<uint8_t> compress(const void* data, std::size_t size)
{
    if (!data || size == 0) return {};

    size_t packedSz = 0;
    void* p = deflate_brutal_masm(data, size, &packedSz);
    if (!p) return {};

    std::vector<uint8_t> out(static_cast<size_t>(packedSz));
    std::memcpy(out.data(), p, packedSz);
    std::free(p);
    return out;
}

/**
 * @brief Worst-case compressed size (gzip header + stored blocks + footer)
 */
inline std::size_t maxCompressedSize(std::size_t rawSize)
{
    std::size_t blockCount = (rawSize + 65534) / 65535;
    return 10 + (blockCount * 5) + rawSize + 8;
}

/**
 * @brief Decompress gzip stream (stored-block format).
 * When HAS_BRUTAL_INFLATE_MASM is defined, uses MASM inflate; otherwise
 * requires zlib/miniz or similar — no Qt dependency.
 */
inline std::vector<uint8_t> decompress(const std::vector<uint8_t>& compressed)
{
    if (compressed.empty()) return {};

    size_t max_uncompressed = compressed.size() * 4;
    void* out_buf = std::malloc(max_uncompressed);
    if (!out_buf) return {};

    size_t out_len = 0;

#ifdef HAS_BRUTAL_INFLATE_MASM
    extern "C" int inflate_brutal_masm(const void* src, size_t src_len,
                                       void* dst, size_t dst_len, size_t* out_len);
    int result = inflate_brutal_masm(
        compressed.data(),
        compressed.size(),
        out_buf,
        max_uncompressed,
        &out_len
    );
    if (result != 0) {
        std::free(out_buf);
        return {};
    }
    std::vector<uint8_t> result_vec(out_len);
    std::memcpy(result_vec.data(), out_buf, out_len);
    std::free(out_buf);
    return result_vec;
#else
    (void)out_len;
    // No Qt qUncompress: caller must use brutal_gzip inflate or zlib/miniz.
    std::free(out_buf);
    return {};
#endif
}

} // namespace brutal
>>>>>>> origin/main
