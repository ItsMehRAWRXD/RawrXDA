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

/// **nvUS** — NVMe-class block path opened in **user space** (volume / file handle), not kernel driver I/O.
/// Maps to `CreateFileW` on `\\.\` device or `\\?\` extended path with **unbuffered** optional flag.
class NvUsIoChannel {
public:
    [[nodiscard]] static std::expected<NvUsIoChannel, std::string> openVolumeReadOnly(const std::wstring& ntPath);

    NvUsIoChannel() = default;
    NvUsIoChannel(const NvUsIoChannel&) = delete;
    NvUsIoChannel& operator=(const NvUsIoChannel&) = delete;
    NvUsIoChannel(NvUsIoChannel&& o) noexcept;
    NvUsIoChannel& operator=(NvUsIoChannel&& o) noexcept;
    ~NvUsIoChannel();

    void close();

    [[nodiscard]] bool valid() const { return m_handle != INVALID_HANDLE_VALUE; }

    /// Raw sector-aligned read at offset (caller aligns buffer if using unbuffered).
    [[nodiscard]] std::expected<std::size_t, std::string> readAt(std::uint64_t offset, void* buffer,
                                                                std::size_t size);

private:
    explicit NvUsIoChannel(HANDLE h) : m_handle(h) {}

    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

}  // namespace RawrXD::Runtime
