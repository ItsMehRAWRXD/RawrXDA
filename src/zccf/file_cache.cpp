#include "file_cache.h"

#include <algorithm>
#include <cstring>

namespace RawrXD {
namespace ZCCF {

namespace {

void CopyPath(char* dst, size_t dstSize, std::string_view src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    size_t n = std::min(dstSize - 1, src.size());
    if (n > 0) {
        std::memcpy(dst, src.data(), n);
    }
    dst[n] = '\0';
}

} // namespace

FileCache::FileCache() = default;

FileCache::~FileCache() {
    UnmapAll();
}

FileCacheResult<uint16_t> FileCache::FindOrAllocSlot(std::string_view path) {
    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        if (m_slots[i].inUse && path == m_slots[i].path) {
            return i;
        }
    }

    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        if (!m_slots[i].inUse) {
            return i;
        }
    }

    Evict();
    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        if (!m_slots[i].inUse) {
            return i;
        }
    }

    return std::unexpected(FileCacheError::CacheFull);
}

FileCacheResult<FileHandle> FileCache::Map(std::string_view absPath) {
    AcquireSRWLockExclusive(&m_lock);
    auto guard = [&] { ReleaseSRWLockExclusive(&m_lock); };

    auto slotRes = FindOrAllocSlot(absPath);
    if (!slotRes.has_value()) {
        guard();
        return std::unexpected(slotRes.error());
    }

    uint16_t slotIdx = *slotRes;
    Slot& slot = m_slots[slotIdx];
    ++m_tick;

    if (slot.inUse && std::string_view(slot.path) == absPath) {
        ++slot.refCount;
        slot.accessStamp = m_tick;
        FileHandle h = FileHandle::Make(slotIdx, slot.version);
        guard();
        return h;
    }

    HANDLE hFile = CreateFileA(std::string(absPath).c_str(), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
                               nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        guard();
        return std::unexpected(err == ERROR_ACCESS_DENIED ? FileCacheError::AccessDenied
                                                          : FileCacheError::FileNotFound);
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(hFile, &size)) {
        CloseHandle(hFile);
        guard();
        return std::unexpected(FileCacheError::MappingFailed);
    }

    HANDLE hMap = CreateFileMappingA(hFile, nullptr, PAGE_READONLY,
                                     0, 0, nullptr);
    if (hMap == nullptr) {
        CloseHandle(hFile);
        guard();
        return std::unexpected(FileCacheError::MappingFailed);
    }

    void* view = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (view == nullptr) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        guard();
        return std::unexpected(FileCacheError::MappingFailed);
    }

    slot.hFile = hFile;
    slot.hMapping = hMap;
    slot.viewBase = view;
    slot.sizeBytes = static_cast<uint64_t>(size.QuadPart);
    slot.refCount = 1;
    slot.accessStamp = m_tick;
    slot.version = static_cast<uint8_t>(slot.version + 1);
    if (slot.version == 0) slot.version = 1;
    slot.inUse = true;
    CopyPath(slot.path, sizeof(slot.path), absPath);

    FileHandle h = FileHandle::Make(slotIdx, slot.version);
    guard();
    return h;
}

void FileCache::Release(FileHandle h) noexcept {
    if (h.IsNull() || h.slot() >= kMaxSlots) {
        return;
    }

    AcquireSRWLockExclusive(&m_lock);
    Slot& slot = m_slots[h.slot()];
    if (slot.inUse && slot.version == h.version() && slot.refCount > 0) {
        --slot.refCount;
        ++m_tick;
        slot.accessStamp = m_tick;
    }
    ReleaseSRWLockExclusive(&m_lock);
}

FileCacheResult<FileView> FileCache::View(FileHandle h) const noexcept {
    if (h.IsNull() || h.slot() >= kMaxSlots) {
        return std::unexpected(FileCacheError::StaleHandle);
    }

    AcquireSRWLockShared(&m_lock);
    auto guard = [&] { ReleaseSRWLockShared(&m_lock); };

    const Slot& slot = m_slots[h.slot()];
    if (!slot.inUse || slot.version != h.version() || slot.viewBase == nullptr) {
        guard();
        return std::unexpected(FileCacheError::StaleHandle);
    }

    FileView view;
    view.bytes = std::span<const char>(static_cast<const char*>(slot.viewBase),
                                       static_cast<size_t>(slot.sizeBytes));
    view.handle = h;
    view.path = slot.path;
    guard();
    return view;
}

std::string FileCache::PathOf(FileHandle h) const noexcept {
    if (h.IsNull() || h.slot() >= kMaxSlots) {
        return {};
    }

    AcquireSRWLockShared(&m_lock);
    const Slot& slot = m_slots[h.slot()];
    std::string path;
    if (slot.inUse && slot.version == h.version()) {
        path = slot.path;
    }
    ReleaseSRWLockShared(&m_lock);
    return path;
}

void FileCache::Evict() {
    AcquireSRWLockExclusive(&m_lock);

    uint16_t victim = kMaxSlots;
    uint32_t oldest = 0xFFFFFFFFu;
    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        const Slot& slot = m_slots[i];
        if (slot.inUse && slot.refCount == 0 && slot.accessStamp < oldest) {
            oldest = slot.accessStamp;
            victim = i;
        }
    }

    if (victim != kMaxSlots) {
        Slot& slot = m_slots[victim];
        if (slot.viewBase != nullptr) UnmapViewOfFile(slot.viewBase);
        if (slot.hMapping != nullptr) CloseHandle(slot.hMapping);
        if (slot.hFile != INVALID_HANDLE_VALUE) CloseHandle(slot.hFile);
        slot = Slot{};
    }

    ReleaseSRWLockExclusive(&m_lock);
}

void FileCache::UnmapAll() {
    AcquireSRWLockExclusive(&m_lock);
    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        Slot& slot = m_slots[i];
        if (!slot.inUse) continue;
        if (slot.viewBase != nullptr) UnmapViewOfFile(slot.viewBase);
        if (slot.hMapping != nullptr) CloseHandle(slot.hMapping);
        if (slot.hFile != INVALID_HANDLE_VALUE) CloseHandle(slot.hFile);
        slot = Slot{};
    }
    ReleaseSRWLockExclusive(&m_lock);
}

uint16_t FileCache::MappedCount() const noexcept {
    uint16_t count = 0;
    AcquireSRWLockShared(&m_lock);
    for (uint16_t i = 0; i < kMaxSlots; ++i) {
        if (m_slots[i].inUse) ++count;
    }
    ReleaseSRWLockShared(&m_lock);
    return count;
}

uint16_t FileCache::AvailableSlots() const noexcept {
    return static_cast<uint16_t>(kMaxSlots - MappedCount());
}

} // namespace ZCCF
} // namespace RawrXD
