#include "codec/gzip_brutal_inflate.hpp"

namespace RawrXD::Codec {

bool gzipBrutalInflateStoredBlocks(const std::uint8_t* in, const std::size_t inLen,
                                   std::vector<std::uint8_t>& out) {
    out.clear();
    if (!in || inLen < 18) {
        return false;
    }
    if (in[0] != 0x1f || in[1] != 0x8b || in[2] != 8) {
        return false;
    }

    std::size_t pos = 10;
    const std::uint8_t flg = in[3];

    if (flg & 4) {
        if (pos + 2 > inLen) {
            return false;
        }
        const std::uint16_t xlen = static_cast<std::uint16_t>(in[pos] | (static_cast<std::uint16_t>(in[pos + 1]) << 8));
        pos += 2u + static_cast<std::size_t>(xlen);
    }
    if (flg & 8) {
        while (pos < inLen && in[pos] != 0) {
            ++pos;
        }
        if (pos >= inLen) {
            return false;
        }
        ++pos;
    }
    if (flg & 16) {
        while (pos < inLen && in[pos] != 0) {
            ++pos;
        }
        if (pos >= inLen) {
            return false;
        }
        ++pos;
    }
    if (flg & 2) {
        pos += 2;
    }

    if (pos + 8 > inLen) {
        return false;
    }
    constexpr std::size_t kTrailer = 8;
    const std::size_t deflateEnd = inLen - kTrailer;

    std::uint64_t bitbuf = 0;
    unsigned nbits = 0;

    auto need = [&](unsigned n) -> bool {
        while (nbits < n) {
            if (pos >= deflateEnd) {
                return false;
            }
            bitbuf |= static_cast<std::uint64_t>(in[pos++]) << nbits;
            nbits += 8;
        }
        return true;
    };

    auto take = [&](unsigned n) -> std::uint32_t {
        (void)need(n);
        const std::uint32_t mask = (n >= 32) ? 0xffffffffu : ((1u << n) - 1u);
        const std::uint32_t v = static_cast<std::uint32_t>(bitbuf) & mask;
        bitbuf >>= n;
        nbits -= n;
        return v;
    };

    for (;;) {
        if (!need(3)) {
            return false;
        }
        const unsigned bfinal = take(1);
        const unsigned btype = take(2);
        if (btype != 0) {
            return false;
        }

        nbits = 0;
        bitbuf = 0;

        if (pos + 4 > deflateEnd) {
            return false;
        }
        const std::uint16_t len = static_cast<std::uint16_t>(in[pos] | (static_cast<std::uint16_t>(in[pos + 1]) << 8));
        const std::uint16_t nlen =
            static_cast<std::uint16_t>(in[pos + 2] | (static_cast<std::uint16_t>(in[pos + 3]) << 8));
        pos += 4;
        if (static_cast<std::uint16_t>(len ^ nlen) != 0xffffu) {
            return false;
        }
        if (pos + len > deflateEnd) {
            return false;
        }
        out.insert(out.end(), in + pos, in + pos + len);
        pos += len;

        if (bfinal != 0) {
            break;
        }
    }

    return true;
}

}  // namespace RawrXD::Codec
