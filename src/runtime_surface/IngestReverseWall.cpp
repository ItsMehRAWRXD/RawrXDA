#include "rawrxd/runtime/IngestReverseWall.hpp"

#include "../logging/Logger.h"

#include <cctype>
#include <filesystem>
#include <fstream>

namespace RawrXD::Runtime {

namespace {
std::string toHex16(std::uint64_t v) {
    static const char* xd = "0123456789abcdef";
    std::string s(16, '0');
    for (int i = 15; i >= 0; --i) {
        s[static_cast<std::size_t>(i)] = xd[v & 0x0F];
        v >>= 4;
    }
    return s;
}

bool eqHexI(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}
}  // namespace

IngestReverseWall& IngestReverseWall::instance() {
    static IngestReverseWall s;
    return s;
}

std::string IngestReverseWall::fnv1a64HexOfFile(const std::wstring& path) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) {
        return {};
    }
    std::vector<std::uint8_t> buf(1 << 20);
    std::uint64_t h = 14695981039346656037ULL;
    while (f) {
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = f.gcount();
        if (got <= 0) {
            break;
        }
        for (std::streamsize i = 0; i < got; ++i) {
            h ^= buf[static_cast<std::size_t>(i)];
            h *= 1099511628211ULL;
        }
    }
    return toHex16(h);
}

std::expected<MmapRegion, std::string> IngestReverseWall::ingestFile(const std::wstring& path,
                                                                     const std::string& expectedHex64) {
    if (expectedHex64.empty()) {
        RawrXD::Logging::Logger::instance().warning(
            "[IngestWall] no expected digest — dev bypass (not for production ingest)", "RuntimeSurface");
    } else {
        const std::string got = fnv1a64HexOfFile(path);
        if (got.empty()) {
            return std::unexpected("IngestWall: cannot hash file");
        }
        if (!eqHexI(got, expectedHex64)) {
            return std::unexpected("IngestWall: digest mismatch (reverse wall)");
        }
        RawrXD::Logging::Logger::instance().info("[IngestWall] digest OK " + got, "RuntimeSurface");
    }

    auto mapped = MmapRegion::mapFileReadOnly(path);
    if (!mapped) {
        return std::unexpected(mapped.error());
    }
    return mapped;
}

}  // namespace RawrXD::Runtime
