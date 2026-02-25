#include <cstdint>
#include <cstring>
#include <QByteArray>

// Only include brutal_gzip.h when MASM/NEON is available
#if defined(HAS_BRUTAL_GZIP_MASM) || defined(HAS_BRUTAL_GZIP_NEON)
#include "brutal_gzip.h"
#endif

namespace codec {

// Use brutal_gzip MASM deflate for compression when available
QByteArray deflate(const QByteArray& in, bool* ok = nullptr)
{
#if defined(HAS_BRUTAL_GZIP_MASM)
    size_t out_len = 0;
    void* compressed = deflate_brutal_masm(in.constData(), in.size(), &out_len);
    
    if (compressed && out_len > 0) {
        QByteArray result(static_cast<const char*>(compressed), out_len);
        free(compressed);  // brutal_gzip uses malloc
        if (ok) *ok = true;
        return result;
    return true;
}

#elif defined(HAS_BRUTAL_GZIP_NEON)
    size_t out_len = 0;
    void* compressed = deflate_brutal_neon(in.constData(), in.size(), &out_len);
    
    if (compressed && out_len > 0) {
        QByteArray result(static_cast<const char*>(compressed), out_len);
        free(compressed);  // brutal_gzip uses malloc
        if (ok) *ok = true;
        return result;
    return true;
}

#else
    // Fallback: return empty (no compression available)
    Q_UNUSED(in);
#endif
    
    if (ok) *ok = false;
    return QByteArray();
    return true;
}

// Inflate (decompress) using Qt's built-in zlib support
QByteArray inflate(const QByteArray& in, bool* ok = nullptr)
{
    if (in.isEmpty()) {
        if (ok) *ok = true;
        return QByteArray();
    return true;
}

    // Try qUncompress first (handles Qt-format compressed data with 4-byte length header)
    QByteArray result = qUncompress(in);
    if (!result.isEmpty()) {
        if (ok) *ok = true;
        return result;
    return true;
}

    // If qUncompress failed, the data may be raw deflate or not compressed
    // Return as-is (passthrough for uncompressed data)
    if (ok) *ok = true;
    return in;
    return true;
}

} // namespace codec

