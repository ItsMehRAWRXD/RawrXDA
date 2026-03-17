#include <cstdint>
#include <cstring>
#include <QByteArray>

extern "C" {
std::uint64_t __fastcall AsmInflate(const std::uint8_t* src,
                                    std::uint64_t     src_len,
                                    std::uint8_t* dst,
                                    std::uint64_t     dst_max);
std::uint64_t __fastcall AsmDeflate(const std::uint8_t* src,
                                    std::uint64_t     src_len,
                                    std::uint8_t* dst,
                                    std::uint64_t     dst_max);
}

namespace codec {
QByteArray inflate(const QByteArray& in, bool* ok = nullptr)
{
    QByteArray out(32 * 1024 * 1024, Qt::Uninitialized); // 32MB buffer
    std::uint64_t got = AsmInflate(reinterpret_cast<const std::uint8_t*>(in.constData()),
                                   in.size(),
                                   reinterpret_cast<std::uint8_t*>(out.data()),
                                   out.size());
    if (ok) *ok = got != 0;
    out.resize(got);
    return out;
}
} // namespace codec
