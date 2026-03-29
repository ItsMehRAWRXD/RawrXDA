// ============================================================================
// file_cache.h — ZCCF File Buffer Table (mmap-backed Hot File Cache)
// ============================================================================
// Maps hot workspace files into the process address space via
// CreateFileMapping / MapViewOfFile with FILE_FLAG_RANDOM_ACCESS.
// The LLM context plane references files by 32-bit FileHandles.
// Content is accessed through a view slice, not copied into strings.
//
// Architecture:
//   ┌────────────────────────────────────────────────────────┐
//   │                    FileCache                            │
//   │                                                         │
//   │  ┌──────────────────────────────────────────────────┐  │
//   │  │ Slot table [ FileEntry × kMaxSlots ]              │  │
//   │  │  slot.hFile, slot.hMapping, slot.view_base        │  │
//   │  │  slot.sizeBytes, slot.handle, slot.version        │  │
//   │  └──────────────────────────────────────────────────┘  │
//   │                                                         │
//   │  Eviction: LRU-like access stamp; Evict() unmaps oldest│
//   └────────────────────────────────────────────────────────┘
//
// Handle encoding: [ reserved:8 | slot:16 | version:8 ]
//
// Thread safety:
//   Map / Unmap / Evict — serialised by internal SRWLOCK.
//   View() — lock-free read once slot is mapped.
//
// No exceptions.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <span>
#include <expected>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace RawrXD {
namespace ZCCF {

// ============================================================================
// Handle
// ============================================================================

struct FileHandle {
    uint32_t raw = 0;

    constexpr uint16_t slot()    const noexcept { return (raw >> 8) & 0xFFFF; }
    constexpr uint8_t  version() const noexcept { return  raw       & 0xFF; }

    static FileHandle Make(uint16_t slot, uint8_t version) noexcept {
        return FileHandle{ (uint32_t(slot) << 8) | uint32_t(version) };
    }

    bool IsNull() const noexcept { return raw == 0; }
    static FileHandle Null() noexcept { return {}; }
};

// ============================================================================
// Error type
// ============================================================================

enum class FileCacheError : uint32_t {
    None          = 0,
    FileNotFound  = 1,
    MappingFailed = 2,
    CacheFull     = 3,
    StaleHandle   = 4,
    AccessDenied  = 5,
};

template<typename T>
using FileCacheResult = std::expected<T, FileCacheError>;

// ============================================================================
// FileView — zero-copy read slice into the mapped region
// ============================================================================

struct FileView {
    std::span<const char> bytes;   // Direct pointer into MapViewOfFile region
    FileHandle            handle;
    std::string           path;

    std::string_view AsStringView() const noexcept {
        return { bytes.data(), bytes.size() };
    }
};

// ============================================================================
// FileCache
// ============================================================================

class FileCache {
public:
    static constexpr uint16_t kMaxSlots = 512;

    FileCache();
    ~FileCache();

    FileCache(const FileCache&)            = delete;
    FileCache& operator=(const FileCache&) = delete;

    // -------------------------------------------------------------------------
    // Map a file into the cache
    // -------------------------------------------------------------------------

    /// Map a file by absolute path. Returns a handle for future View() calls.
    /// If already mapped, returns the existing handle (reference-counted).
    FileCacheResult<FileHandle> Map(std::string_view absPath);

    /// Release a handle reference. Slot is evictable when refcount reaches 0.
    void Release(FileHandle h) noexcept;

    // -------------------------------------------------------------------------
    // Read (lock-free once mapped)
    // -------------------------------------------------------------------------

    /// Return a zero-copy view of the mapped file content.
    /// Valid as long as the handle is held (not Released).
    FileCacheResult<FileView> View(FileHandle h) const noexcept;

    /// Path associated with a handle. Empty string if stale.
    std::string PathOf(FileHandle h) const noexcept;

    // -------------------------------------------------------------------------
    // Cache management
    // -------------------------------------------------------------------------

    /// Evict the least-recently-used zero-refcount slot. No-op if none free.
    void Evict();

    /// Force-unmap all slots (call at session end).
    void UnmapAll();

    uint16_t MappedCount()   const noexcept;
    uint16_t AvailableSlots()const noexcept;

private:
    struct Slot {
        HANDLE   hFile      = INVALID_HANDLE_VALUE;
        HANDLE   hMapping   = nullptr;
        void*    viewBase   = nullptr;
        uint64_t sizeBytes  = 0;
        uint32_t refCount   = 0;
        uint32_t accessStamp= 0;     // Coarse tick for LRU eviction
        uint8_t  version    = 0;
        bool     inUse      = false;
        char     path[512]  = {};
    };

    Slot     m_slots[kMaxSlots] = {};
    uint32_t m_tick             = 0;
    mutable SRWLOCK m_lock      = SRWLOCK_INIT;

    FileCacheResult<uint16_t> FindOrAllocSlot(std::string_view path);
};

} // namespace ZCCF
} // namespace RawrXD
