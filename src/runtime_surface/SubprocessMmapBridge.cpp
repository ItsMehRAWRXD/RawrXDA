#include "rawrxd/runtime/SubprocessMmapBridge.hpp"

#include "../logging/Logger.h"

namespace RawrXD::Runtime {

SubprocessMmapBridge::SubprocessMmapBridge(SubprocessMmapBridge&& o) noexcept
    : mapping(o.mapping), view(o.view), sizeBytes(o.sizeBytes) {
    o.mapping = nullptr;
    o.view = nullptr;
    o.sizeBytes = 0;
}

SubprocessMmapBridge& SubprocessMmapBridge::operator=(SubprocessMmapBridge&& o) noexcept {
    if (this != &o) {
        reset();
        mapping = o.mapping;
        view = o.view;
        sizeBytes = o.sizeBytes;
        o.mapping = nullptr;
        o.view = nullptr;
        o.sizeBytes = 0;
    }
    return *this;
}

SubprocessMmapBridge::~SubprocessMmapBridge() {
    reset();
}

void SubprocessMmapBridge::reset() {
    if (view != nullptr) {
        UnmapViewOfFile(view);
        view = nullptr;
    }
    if (mapping != nullptr) {
        CloseHandle(mapping);
        mapping = nullptr;
    }
    sizeBytes = 0;
}

std::expected<SubprocessMmapBridge, std::string> SubprocessMmapBridge::createReadWriteAnonymous(
    std::size_t size) {
    if (size == 0) {
        return std::unexpected("SubprocessMmapBridge: zero size");
    }
    const DWORD szLow = static_cast<DWORD>(size & 0xFFFFFFFFu);
    const DWORD szHigh = static_cast<DWORD>((size >> 32) & 0xFFFFFFFFu);
    const HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, szHigh, szLow, nullptr);
    if (h == nullptr) {
        return std::unexpected("SubprocessMmapBridge: CreateFileMapping failed");
    }
    void* v = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (v == nullptr) {
        CloseHandle(h);
        return std::unexpected("SubprocessMmapBridge: MapViewOfFile failed");
    }
    SubprocessMmapBridge b;
    b.mapping = h;
    b.view = v;
    b.sizeBytes = size;
    RawrXD::Logging::Logger::instance().info(
        "[SubprocessMmapBridge] anonymous RW mapping created (child-duplicable HANDLE)", "RuntimeSurface");
    return b;
}

std::expected<HANDLE, std::string> SubprocessMmapBridge::duplicateMappingToProcess(
    HANDLE targetProcess, bool inheritable) const {
    if (mapping == nullptr) {
        return std::unexpected("SubprocessMmapBridge: no mapping");
    }
    HANDLE out = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), mapping, targetProcess, &out, 0, inheritable ? TRUE : FALSE,
                         DUPLICATE_SAME_ACCESS)) {
        return std::unexpected("SubprocessMmapBridge: DuplicateHandle failed");
    }
    return out;
}

}  // namespace RawrXD::Runtime
