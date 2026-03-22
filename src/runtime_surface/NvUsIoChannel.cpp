#include "rawrxd/runtime/NvUsIoChannel.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

std::expected<NvUsIoChannel, std::string> NvUsIoChannel::openVolumeReadOnly(const std::wstring& ntPath) {
    if (ntPath.empty()) {
        return std::unexpected("NvUsIoChannel: empty path");
    }
    const HANDLE h = CreateFileW(ntPath.c_str(), GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return std::unexpected("NvUsIoChannel: CreateFile failed");
    }
    RawrXD::Logging::Logger::instance().info("[NvUsIoChannel] opened user-space volume/device path",
                                             "RuntimeSurface");
    return NvUsIoChannel(h);
}

NvUsIoChannel::NvUsIoChannel(NvUsIoChannel&& o) noexcept : m_handle(o.m_handle) {
    o.m_handle = INVALID_HANDLE_VALUE;
}

NvUsIoChannel& NvUsIoChannel::operator=(NvUsIoChannel&& o) noexcept {
    if (this != &o) {
        close();
        m_handle = o.m_handle;
        o.m_handle = INVALID_HANDLE_VALUE;
    }
    return *this;
}

NvUsIoChannel::~NvUsIoChannel() {
    close();
}

void NvUsIoChannel::close() {
    if (m_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
}

std::expected<std::size_t, std::string> NvUsIoChannel::readAt(std::uint64_t offset, void* buffer,
                                                            std::size_t size) {
    if (m_handle == INVALID_HANDLE_VALUE) {
        return std::unexpected("NvUsIoChannel: not open");
    }
    LARGE_INTEGER li{};
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(m_handle, li, nullptr, FILE_BEGIN)) {
        return std::unexpected("NvUsIoChannel: SetFilePointerEx failed");
    }
    DWORD nread = 0;
    if (!ReadFile(m_handle, buffer, static_cast<DWORD>(size), &nread, nullptr)) {
        return std::unexpected("NvUsIoChannel: ReadFile failed");
    }
    return static_cast<std::size_t>(nread);
}

}  // namespace RawrXD::Runtime
