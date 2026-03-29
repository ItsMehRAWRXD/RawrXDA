// ============================================================================
// trace_log.h — DAE Append-Only Trace Log
// ============================================================================
// Records every DAE state transition as an ordered, hash-chained event stream.
// Used for:
//   - Exact replay reconstruction
//   - Divergence detection
//   - Speculative trace similarity lookup
//
// Format:
//   Each TraceEvent contains:
//     seq          — global monotonic sequence number
//     stateBeforeHash — SHA-256 of the shadow FS dirty set before the op
//     actionDigest    — SHA-256 of (op_type || target || payload_hash)
//     stateAfterHash  — SHA-256 of the shadow FS dirty set after the op
//     prevEventHash   — SHA-256 of the previous TraceEvent (hash chain)
//
// The hash chain provides tamper evidence: any modification to a historical
// event invalidates all subsequent prevEventHash values.
//
// Thread Safety: Append is serialised; concurrent reads are safe after Close().
// No exceptions. Returns TraceResult<T>.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <expected>
#include <functional>
#include <memory>

#include "shadow_fs.h"   // ContentHash

namespace RawrXD {
namespace DAE {

// ============================================================================
// Error type
// ============================================================================

enum class TraceError : uint32_t {
    None        = 0,
    AlreadyClosed = 1,
    IoError     = 2,
    Corrupted   = 3,   // Hash chain verification failed
};

template<typename T>
using TraceResult = std::expected<T, TraceError>;

// ============================================================================
// TraceEvent
// ============================================================================

struct TraceEvent {
    uint64_t    seq             = 0;
    uint64_t    timestampUs     = 0;   // Microseconds since session start
    std::string opTypeName;            // e.g. "CreateFile"
    std::string targetPath;
    ContentHash stateBeforeHash;
    ContentHash actionDigest;
    ContentHash stateAfterHash;
    ContentHash prevEventHash;         // All-zero for seq==0
    bool        succeeded       = true;
};

// ============================================================================
// TraceLog
// ============================================================================

class TraceLog {
public:
    // -------------------------------------------------------------------------
    // Open / Close
    // -------------------------------------------------------------------------

    /// Open or create a log at the given file path.
    /// Truncates existing content (new session).
    TraceResult<void> Open(std::string_view logPath);

    /// Flush and close the file handle.
    void Close();

    bool IsOpen() const noexcept;

    // -------------------------------------------------------------------------
    // Append
    // -------------------------------------------------------------------------

    /// Append a new event. Computes prevEventHash automatically from the last
    /// appended event. Thread-safe (serialised internally).
    TraceResult<void> Append(TraceEvent event);

    // -------------------------------------------------------------------------
    // Reads (available after Close or during open for monitoring)
    // -------------------------------------------------------------------------

    /// Return all events in append order.
    TraceResult<std::vector<TraceEvent>> ReadAll() const;

    /// Return the hash of the last appended event (useful for state linking).
    ContentHash LastEventHash() const noexcept;

    uint64_t EventCount() const noexcept;

    // -------------------------------------------------------------------------
    // Integrity verification
    // -------------------------------------------------------------------------

    /// Walk every event and verify the hash chain. Returns first broken seq.
    TraceResult<void> VerifyChain() const;

    // -------------------------------------------------------------------------
    // Structural fingerprint for similarity lookup
    // -------------------------------------------------------------------------

    /// Hash of the ordered sequence of (opTypeName, succeeded) pairs.
    /// Used by the speculative replay to find similar historical traces.
    ContentHash StructuralFingerprint() const;

    // -------------------------------------------------------------------------
    // Index
    // -------------------------------------------------------------------------

    /// Build an in-memory index of successful trace fingerprints.
    /// Call after loading a historical trace for similarity search.
    void IndexForLookup();

    /// Returns true if this log's structural fingerprint matches the given one.
    bool MatchesFingerprint(const ContentHash& fp) const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// ============================================================================
// TraceIndex — session-level collection of indexed historical traces
// ============================================================================

class TraceIndex {
public:
    /// Load and index a trace file from disk.
    TraceResult<void> LoadTrace(std::string_view logPath);

    /// Find the best-matching historical trace for a given candidate graph
    /// fingerprint. Returns the path of the best match or empty if none.
    std::string FindSimilar(const ContentHash& fingerprint) const;

    size_t TracesLoaded() const noexcept;

private:
    struct Entry {
        std::string   path;
        ContentHash   fingerprint;
        bool          allSucceeded = false;
    };
    std::vector<Entry> m_entries;
};

} // namespace DAE
} // namespace RawrXD
