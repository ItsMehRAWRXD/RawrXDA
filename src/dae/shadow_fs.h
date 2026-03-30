// ============================================================================
// shadow_fs.h — DAE Shadow Filesystem (Copy-on-Write Overlay)
// ============================================================================
// Provides a Copy-on-Write virtual filesystem layered over the real workspace.
// All agent mutations operate exclusively on the overlay. Base layer files are
// never touched until an explicit Commit() call promotes validated deltas.
//
// Architecture:
//   ┌─────────────────────────────────────────────────────────────┐
//   │                      ShadowFilesystem                       │
//   │                                                             │
//   │  ┌──────────────────┐         ┌──────────────────────────┐  │
//   │  │   Base Layer      │         │     CoW Overlay          │  │
//   │  │  (workspace snap) │ ──────► │  (agent write target)    │  │
//   │  │  read-only        │         │  CreateFileMapping pages  │  │
//   │  └──────────────────┘         └──────────────────────────┘  │
//   │                                          │                   │
//   │                                          ▼                   │
//   │                               ┌──────────────────┐          │
//   │                               │  MergedView()     │          │
//   │                               │  deterministic    │          │
//   │                               │  read precedence  │          │
//   │                               └──────────────────┘          │
//   └─────────────────────────────────────────────────────────────┘
//
// Determinism Contract:
//   1. Base layer is immutable after SnapshotBase().
//   2. Overlay writes are content-hashed on entry.
//   3. Directory iteration order is always lexicographic.
//   4. Line endings are normalised to LF on read through MergedView.
//   5. Commit() is atomic — all-or-nothing promotion.
//
// Thread Safety:
//   SnapshotBase / Commit — single-owner, call from agent thread only.
//   Read / Write overlay  — guarded by internal SRW lock (readers concurrent).
//
// No exceptions. All failure paths return ShadowResult<T>.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <expected>
#include <memory>
#include <cstring>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace RawrXD {
namespace DAE {

// ============================================================================
// Error types
// ============================================================================

enum class ShadowError : uint32_t {
    None            = 0,
    PathNotFound    = 1,
    AccessDenied    = 2,
    OverlayFull     = 3,
    HashMismatch    = 4,   // Content hash changed under us — divergence
    BaseSealed      = 5,   // Attempt to write base after SnapshotBase()
    NotCommittable  = 6,   // Replay validation not yet passed
    MappingFailed   = 7,
    IoError         = 8,
};

template<typename T>
using ShadowResult = std::expected<T, ShadowError>;

// ============================================================================
// Content hash — SHA-256 truncated to 32 bytes
// ============================================================================

struct ContentHash {
    uint8_t bytes[32] = {};

    bool operator==(const ContentHash& o) const noexcept {
        return std::memcmp(bytes, o.bytes, 32) == 0;
    }
    bool operator!=(const ContentHash& o) const noexcept { return !(*this == o); }

    static ContentHash zero() noexcept { return {}; }
};

// ============================================================================
// FileEntry — one file's state in the overlay
// ============================================================================

struct FileEntry {
    std::string  path;           // Repo-relative path, forward slashes, LF-normalised
    ContentHash  baseHash;       // Hash of base layer bytes (zeroed if new file)
    ContentHash  overlayHash;    // Hash of current overlay bytes
    bool         isDeleted  = false;
    bool         isNew      = false;
    uint64_t     sizeBytes  = 0;
};

// ============================================================================
// ShadowFilesystem
// ============================================================================

class ShadowFilesystem {
public:
    ShadowFilesystem();
    ~ShadowFilesystem();

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /// Seal the base layer from the given workspace root. Must be called once
    /// before any writes. Records content hashes for all tracked files.
    ShadowResult<void> SnapshotBase(std::string_view workspaceRoot);

    /// Discard all overlay entries and reset to clean base state.
    void Reset();

    // -------------------------------------------------------------------------
    // Overlay writes (agent-facing)
    // -------------------------------------------------------------------------

    /// Write full content to a path in the overlay. Creates new file if absent.
    ShadowResult<void> Write(std::string_view path, std::string_view content);

    /// Apply a unified-diff patch to a path via the overlay.
    ShadowResult<void> Patch(std::string_view path, std::string_view unifiedDiff);

    /// Mark a path as deleted in the overlay (base is untouched).
    ShadowResult<void> Delete(std::string_view path);

    /// Move/rename a path within the overlay.
    ShadowResult<void> Move(std::string_view fromPath, std::string_view toPath);

    // -------------------------------------------------------------------------
    // Merged reads (deterministic base-or-overlay precedence)
    // -------------------------------------------------------------------------

    /// Read content through the merged view. Returns overlay content if present,
    /// otherwise streams from base. LF-normalised on return.
    ShadowResult<std::string> Read(std::string_view path) const;

    /// Check existence in merged view.
    bool Exists(std::string_view path) const;

    /// List directory entries in lexicographic order (merged view).
    ShadowResult<std::vector<std::string>> ListDir(std::string_view dirPath) const;

    // -------------------------------------------------------------------------
    // Inspection
    // -------------------------------------------------------------------------

    /// Return all overlay entries (modified, new, deleted).
    std::vector<FileEntry> DirtyEntries() const;

    /// Return the content hash of a path in the overlay; empty if unmodified.
    std::optional<ContentHash> OverlayHash(std::string_view path) const;

    // -------------------------------------------------------------------------
    // Commit — promote overlay to workspace (requires ReplayValidated == true)
    // -------------------------------------------------------------------------

    /// Mark that replay validation has passed. Required before Commit().
    void SetReplayValidated(bool val) noexcept;

    /// Atomically write all overlay deltas to the real workspace.
    /// Rolls back entirely on any failure (original files restored from base).
    ShadowResult<void> Commit();

    // -------------------------------------------------------------------------
    // Static helpers
    // -------------------------------------------------------------------------

    static ContentHash ComputeHash(std::string_view data) noexcept;
    static std::string NormaliseLineEndings(std::string_view raw);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    mutable SRWLOCK m_lock = SRWLOCK_INIT;
    bool            m_baseSealed        = false;
    bool            m_replayValidated   = false;
    std::string     m_workspaceRoot;
};

} // namespace DAE
} // namespace RawrXD
