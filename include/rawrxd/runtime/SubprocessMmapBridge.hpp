#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD::Runtime {

/// Anonymous file mapping suitable for **inheritable** HANDLE duplication into a child process
/// (strict subprocess RE I/O — shared page without startup scan).
struct SubprocessMmapBridge {
    HANDLE mapping = nullptr;
    void* view = nullptr;
    std::size_t sizeBytes = 0;

    SubprocessMmapBridge() = default;
    SubprocessMmapBridge(const SubprocessMmapBridge&) = delete;
    SubprocessMmapBridge& operator=(const SubprocessMmapBridge&) = delete;
    SubprocessMmapBridge(SubprocessMmapBridge&& o) noexcept;
    SubprocessMmapBridge& operator=(SubprocessMmapBridge&& o) noexcept;
    ~SubprocessMmapBridge();

    void reset();

    [[nodiscard]] static std::expected<SubprocessMmapBridge, std::string> createReadWriteAnonymous(
        std::size_t size);

    /// Duplicate mapping handle for `targetProcess` (e.g. child) with inherit flag.
    [[nodiscard]] std::expected<HANDLE, std::string> duplicateMappingToProcess(HANDLE targetProcess,
                                                                              bool inheritable) const;
};

}  // namespace RawrXD::Runtime
