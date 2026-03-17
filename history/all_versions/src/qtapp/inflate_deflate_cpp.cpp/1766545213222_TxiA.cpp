#include <cstdint>
#include <cstring>
#include <QByteArray>
#include "brutal_gzip.h"

namespace codec {

// Stubbed deflate/inflate to allow builds when MASM kernels are unavailable
QByteArray deflate(const QByteArray& in, bool* ok = nullptr)
{
    if (ok) *ok = true;
    return in;  // passthrough when compression kernels are not present
}

QByteArray inflate(const QByteArray& in, bool* ok = nullptr)
{
    if (ok) *ok = true;
    return in;  // passthrough when decompression kernels are not present
}

} // namespace codec

// Global stub to satisfy legacy MASM linkage expectations
QByteArray deflate_brutal_masm(const QByteArray& in)
{
    return in;
}
