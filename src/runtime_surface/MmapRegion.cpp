#include "rawrxd/runtime/MmapRegion.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

MmapRegion::MmapRegion(MmapRegion&& o) noexcept {
    file = o.file;
    mapping = o.mapping;
    view = o.view;
    sizeBytes = o.sizeBytes;
    o.file = INVALID_HANDLE_VALUE;
    o.mapping = nullptr;
    o.view = nullptr;
    o.sizeBytes = 0;
}

MmapRegion& MmapRegion::operator=(MmapRegion&& o) noexcept {
    if (this != &o) {
        reset();
        file = o.file;
        mapping = o.mapping;
        view = o.view;
        sizeBytes = o.sizeBytes;
        o.file = INVALID_HANDLE_VALUE;
        o.mapping = nullptr;
        o.view = nullptr;
        o.sizeBytes = 0;
    }
    return *this;
}

MmapRegion::~MmapRegion() {
    reset();
}

void MmapRegion::reset() {
    if (view != nullptr) {
        UnmapViewOfFile(view);
        view = nullptr;
    }
    if (mapping != nullptr) {
        CloseHandle(mapping);
        mapping = nullptr;
    }
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }
    sizeBytes = 0;
}

std::expected<MmapRegion, std::string> MmapRegion::mapFileReadOnly(const std::wstring& path) {
    MmapRegion out;
    out.file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
    if (out.file == INVALID_HANDLE_VALUE) {
        return std::unexpected("MmapRegion: CreateFileW failed");
    }

    LARGE_INTEGER li{};
    if (!GetFileSizeEx(out.file, &li)) {
        out.reset();
        return std::unexpected("MmapRegion: GetFileSizeEx failed");
    }
    const ULONGLONG sz = static_cast<ULONGLONG>(li.QuadPart);
    if (sz == 0) {
        out.reset();
        return std::unexpected("MmapRegion: empty file");
    }
    out.sizeBytes = static_cast<std::size_t>(sz);

    out.mapping = CreateFileMappingW(out.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (out.mapping == nullptr) {
        out.reset();
        return std::unexpected("MmapRegion: CreateFileMappingW failed");
    }

    out.view = MapViewOfFile(out.mapping, FILE_MAP_READ, 0, 0, 0);
    if (out.view == nullptr) {
        out.reset();
        return std::unexpected("MmapRegion: MapViewOfFile failed");
    }

    RawrXD::Logging::Logger::instance().info(
        "[MmapRegion] mapped " + std::to_string(out.sizeBytes) + " bytes read-only", "RuntimeSurface");
    return out;
}

}  // namespace RawrXD::Runtime
