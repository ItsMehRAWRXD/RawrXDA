#pragma once

#include "RuntimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD::Runtime {

struct MmapRegion {
    HANDLE file = INVALID_HANDLE_VALUE;
    HANDLE mapping = nullptr;
    void* view = nullptr;
    std::size_t sizeBytes = 0;

    MmapRegion() = default;
    MmapRegion(const MmapRegion&) = delete;
    MmapRegion& operator=(const MmapRegion&) = delete;

    MmapRegion(MmapRegion&& o) noexcept;
    MmapRegion& operator=(MmapRegion&& o) noexcept;
    ~MmapRegion();

    void reset();

    /// Map an existing file read-only (model weights, RE blobs). No directory scan — caller supplies path.
    [[nodiscard]] static std::expected<MmapRegion, std::string> mapFileReadOnly(const std::wstring& path);

    [[nodiscard]] const std::uint8_t* bytes() const {
        return static_cast<const std::uint8_t*>(view);
    }
    [[nodiscard]] std::size_t size() const { return sizeBytes; }
};

}  // namespace RawrXD::Runtime
