#include <cstddef>
#include <cstdint>
#include <vector>

// These stubs exist so small, dependency-free smoke tests can link against
// `src/gguf_loader.cpp` without pulling in optional compression backends.
// We provide a deterministic reversible codec so compression paths can still
// be validated without external libraries.

namespace {
constexpr uint32_t kHeaderMagic = 0x31524C52u; // "RLR1"

static void appendU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

static bool readU32(const std::vector<uint8_t>& in, std::size_t offset, uint32_t& out)
{
    if (offset + 4 > in.size()) {
        return false;
    }
    out = static_cast<uint32_t>(in[offset]) |
          (static_cast<uint32_t>(in[offset + 1]) << 8) |
          (static_cast<uint32_t>(in[offset + 2]) << 16) |
          (static_cast<uint32_t>(in[offset + 3]) << 24);
    return true;
}

static std::vector<uint8_t> rleEncode(const std::vector<uint8_t>& in)
{
    std::vector<uint8_t> out;
    out.reserve(in.size() + 16);
    appendU32(out, kHeaderMagic);
    appendU32(out, static_cast<uint32_t>(in.size()));

    std::size_t i = 0;
    while (i < in.size()) {
        const uint8_t value = in[i];
        uint8_t run = 1;
        while ((i + run) < in.size() && in[i + run] == value && run < 255) {
            ++run;
        }
        out.push_back(run);
        out.push_back(value);
        i += run;
    }
    return out;
}

static std::vector<uint8_t> rleDecode(const std::vector<uint8_t>& in, bool* ok)
{
    if (ok) {
        *ok = false;
    }

    uint32_t magic = 0;
    uint32_t expectedSize = 0;
    if (!readU32(in, 0, magic) || !readU32(in, 4, expectedSize) || magic != kHeaderMagic) {
        return {};
    }

    std::vector<uint8_t> out;
    out.reserve(expectedSize);

    std::size_t i = 8;
    while (i + 1 < in.size()) {
        const uint8_t run = in[i++];
        const uint8_t value = in[i++];
        if (run == 0) {
            return {};
        }
        out.insert(out.end(), run, value);
        if (out.size() > expectedSize) {
            return {};
        }
    }

    if (out.size() != expectedSize || i != in.size()) {
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return out;
}
} // namespace

namespace codec
{
std::vector<uint8_t> deflate(const std::vector<uint8_t>& in, bool* ok)
{
    if (ok) {
        *ok = true;
    }
    return rleEncode(in);
}

std::vector<uint8_t> inflate(const std::vector<uint8_t>& in, bool* ok)
{
    return rleDecode(in, ok);
}
}  // namespace codec

namespace brutal
{
std::vector<uint8_t> compress(const std::vector<uint8_t>& in)
{
    return rleEncode(in);
}

std::vector<uint8_t> compress(const void* data, std::size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    return rleEncode(std::vector<uint8_t>(p, p + size));
}

std::vector<uint8_t> decompress(const std::vector<uint8_t>& in, bool* ok)
{
    return rleDecode(in, ok);
}
}  // namespace brutal
