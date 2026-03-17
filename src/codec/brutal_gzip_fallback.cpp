#include "brutal_gzip.h"

#include <cstdlib>
#include <cstring>

#if __has_include(<zlib.h>)
#define RAWRXD_HAVE_ZLIB 1
#include <zlib.h>
#else
#define RAWRXD_HAVE_ZLIB 0
#endif

#if !defined(HAS_BRUTAL_GZIP_MASM) && !defined(HAS_BRUTAL_GZIP_NEON)
extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!src || len == 0) {
        return nullptr;
    }

#if RAWRXD_HAVE_ZLIB
    z_stream stream{};
    const int window_bits = 15 + 16; // gzip wrapper

    if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return nullptr;
    }

    const uLongf bound = compressBound(static_cast<uLong>(len));
    void* buffer = std::malloc(bound);
    if (!buffer) {
        deflateEnd(&stream);
        return nullptr;
    }

    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    stream.avail_in = static_cast<uInt>(len);
    stream.next_out = reinterpret_cast<Bytef*>(buffer);
    stream.avail_out = static_cast<uInt>(bound);

    const int result = deflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        deflateEnd(&stream);
        std::free(buffer);
        return nullptr;
    }

    if (out_len) {
        *out_len = static_cast<size_t>(stream.total_out);
    }

    deflateEnd(&stream);
    return buffer;
#else
    // Zlib not available: return an uncompressed copy to keep pipeline alive.
    void* buffer = std::malloc(len);
    if (!buffer) return nullptr;
    std::memcpy(buffer, src, len);
    if (out_len) *out_len = len;
    return buffer;
#endif
}
#endif
