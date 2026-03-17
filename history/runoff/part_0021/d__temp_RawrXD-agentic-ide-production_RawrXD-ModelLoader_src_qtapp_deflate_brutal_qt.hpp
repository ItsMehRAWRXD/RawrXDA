// MASM-backed brutal codec Qt wrapper
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <QByteArray>
#include "brutal_gzip.h"

// MASM exports (from inflate_deflate_asm.asm)
extern "C" size_t AsmDeflate(const void* src, size_t src_len, void* dst, size_t dst_max_len);
extern "C" size_t AsmInflate(const void* src, size_t src_len, void* dst, size_t dst_max_len);

namespace brutal {

// Upper bound for gzip stored-block encoding
inline std::size_t compressBound(std::size_t len) {
    std::size_t blocks = (len + 65534) / 65535; // ceil
    return 10 + 8 + (blocks * 5) + len; // header + footer + block headers + data
}

// Compress into caller-provided buffer; returns bytes written
inline std::size_t compress(std::uint8_t* dst, std::size_t dst_max,
                            const std::uint8_t* src, std::size_t len) {
    if (!dst || !src || dst_max == 0 || len == 0) return 0;
#if defined(HAS_BRUTAL_GZIP_MASM)
    std::uint64_t out_len = 0;
    void* tmp_v = deflate_brutal_masm(static_cast<const void*>(src), static_cast<std::size_t>(len), reinterpret_cast<std::size_t*>(&out_len));
    const std::uint8_t* tmp = static_cast<const std::uint8_t*>(tmp_v);
    if (!tmp || out_len == 0) return 0;
    if (out_len > dst_max) {
        // Caller must ensure dst_max >= compressBound(len)
        // Truncate safely
        std::memcpy(dst, tmp, dst_max);
        std::free(tmp_v);
        return dst_max;
    }
    std::memcpy(dst, tmp, static_cast<std::size_t>(out_len));
    std::free(tmp_v);
    return static_cast<std::size_t>(out_len);
#else
    // Direct MASM path unavailable; use raw ASM wrapper
    std::size_t written = AsmDeflate(src, len, dst, dst_max);
    return written;
#endif
}

// Decompress gzip stream into caller buffer; returns bytes written
inline std::size_t decompress(std::uint8_t* dst, std::size_t dst_max,
                              const std::uint8_t* src, std::size_t len) {
    if (!dst || !src || dst_max == 0 || len == 0) return 0;
    return AsmInflate(src, len, dst, dst_max);
}

} // namespace brutal

// Qt-friendly overloads
namespace brutal {

inline QByteArray compress(const QByteArray& in) {
    if (in.isEmpty()) return QByteArray();
    const std::size_t in_len = static_cast<std::size_t>(in.size());
    const std::size_t bound = compressBound(in_len);
    QByteArray out;
    out.resize(static_cast<int>(bound));
    const std::size_t written = compress(reinterpret_cast<std::uint8_t*>(out.data()), bound,
                                         reinterpret_cast<const std::uint8_t*>(in.constData()), in_len);
    out.resize(static_cast<int>(written));
    return out;
}

inline QByteArray decompress(const QByteArray& in) {
    if (in.size() < 4) return QByteArray();
    const unsigned char* data = reinterpret_cast<const unsigned char*>(in.constData());
    const std::size_t len = static_cast<std::size_t>(in.size());
    // GZIP footer ISIZE is last 4 bytes (little-endian), original size modulo 2^32
    std::size_t isize = static_cast<std::size_t>(
        static_cast<uint32_t>(data[len - 4]) |
        (static_cast<uint32_t>(data[len - 3]) << 8) |
        (static_cast<uint32_t>(data[len - 2]) << 16) |
        (static_cast<uint32_t>(data[len - 1]) << 24));
    if (isize == 0) return QByteArray();
    QByteArray out;
    out.resize(static_cast<int>(isize));
    const std::size_t written = decompress(reinterpret_cast<std::uint8_t*>(out.data()), isize,
                                           reinterpret_cast<const std::uint8_t*>(data), len);
    out.resize(static_cast<int>(written));
    return out;
}

} // namespace brutal
