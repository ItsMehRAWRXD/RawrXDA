// ============================================================================
// patch_rollback_ledger.h — SelfPatch Rollback Safety Envelope
// ============================================================================
// Maintains a persistent patch ledger for:
//   - Version tagging every patch application
//   - Deterministic rollback map (patch ID → original bytes)
//   - Integrity checksums before and after patch
//   - Replay validation of patch impact
//   - Crash-safe journal (WAL-style)
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Patch {

// ============================================================================
// Patch Version Tag
// ============================================================================

struct PatchVersion {
    uint32_t major;
    uint32_t minor;
    uint32_t revision;
    uint64_t timestampMs;   // Epoch ms when applied
};

// ============================================================================
// Patch Entry — One entry per applied patch
// ============================================================================

struct PatchEntry {
    uint32_t    patchId;
    char        name[64];           // Human-readable name
    PatchVersion version;

    // Target
    void*       targetAddress;      // Where the patch was applied
    uint32_t    patchSize;          // Bytes modified

    // Original bytes (for rollback)
    uint8_t     originalBytes[256]; // Max 256 bytes per patch
    uint32_t    originalSize;

    // New bytes (applied patch)
    uint8_t     newBytes[256];
    uint32_t    newSize;

    // Integrity checksums (FNV-1a 64-bit)
    uint64_t    checksumBefore;     // Hash of original bytes
    uint64_t    checksumAfter;      // Hash of patched region to verify
    uint64_t    checksumValidated;  // Hash at last validation

    // State
    enum State : uint8_t {
        Pending     = 0,
        Applied     = 1,
        RolledBack  = 2,
        Failed      = 3,
        Quarantined = 4,
        Validated   = 5
    };
    State       state;

    // Impact metrics
    uint32_t    applicationCount;   // Times this patch was applied
    uint32_t    rollbackCount;      // Times rolled back
    uint32_t    failureCount;       // Times failed integrity check
    int64_t     lastValidatedMs;    // Last integrity check timestamp
};

// ============================================================================
// Ledger Result
// ============================================================================

struct LedgerResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static LedgerResult ok(const char* msg = "OK") {
        LedgerResult r;
        r.success = true;
        r.detail = msg;
        r.errorCode = 0;
        return r;
    }

    static LedgerResult error(const char* msg, int code = -1) {
        LedgerResult r;
        r.success = false;
        r.detail = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Patch Rollback Ledger
// ============================================================================

class PatchRollbackLedger {
public:
    PatchRollbackLedger();
    ~PatchRollbackLedger();

    // Singleton
    static PatchRollbackLedger& Global();

    // ---- Lifecycle ----

    /// Initialize the ledger. If journalPath is non-null, enables WAL persistence.
    LedgerResult initialize(const char* journalPath = nullptr);

    /// Shutdown and flush journal.
    void shutdown();

    // ---- Patch Registration ----

    /// Record a patch application. Captures original bytes, computes checksums.
    LedgerResult recordApply(uint32_t patchId, const char* name,
                              void* targetAddress, uint32_t size,
                              const void* newBytes, uint32_t newSize);

    /// Record a successful rollback.
    LedgerResult recordRollback(uint32_t patchId);

    /// Record a patch failure (integrity check or crash).
    LedgerResult recordFailure(uint32_t patchId, const char* reason);

    /// Quarantine a patch (prevent re-application).
    LedgerResult quarantine(uint32_t patchId);

    // ---- Rollback ----

    /// Rollback a specific patch by restoring original bytes.
    /// Uses VirtualProtect to temporarily make the region writable.
    LedgerResult rollback(uint32_t patchId);

    /// Rollback ALL applied patches in reverse order.
    LedgerResult rollbackAll();

    /// Emergency rollback — called from crash handler (minimal alloc).
    /// Returns number of patches successfully rolled back.
    int emergencyRollbackAll();

    // ---- Integrity ----

    /// Validate a patch — compare current memory against expected checksum.
    LedgerResult validate(uint32_t patchId);

    /// Validate ALL patches. Returns count of failures.
    int validateAll();

    /// Recompute checksum of patched region.
    uint64_t computeChecksum(const void* data, size_t size) const;

    // ---- Query ----

    /// Get entry for a patch ID. Returns nullptr if not found.
    const PatchEntry* getEntry(uint32_t patchId) const;

    /// Get total number of entries.
    int entryCount() const { return m_count; }

    /// Get number of currently-applied patches.
    int appliedCount() const;

    /// Get number of quarantined patches.
    int quarantinedCount() const;

    // ---- Journal ----

    /// Flush the journal to disk (WAL sync).
    LedgerResult flushJournal();

    /// Replay journal from disk to reconstruct ledger state.
    LedgerResult replayJournal(const char* path);

private:
    static constexpr int MAX_PATCHES = 256;

    PatchEntry  m_entries[MAX_PATCHES];
    int         m_count = 0;
    bool        m_initialized = false;

    // Journal file
    HANDLE      m_journalHandle = INVALID_HANDLE_VALUE;
    char        m_journalPath[260] = {};

    // Mutex for thread safety
    CRITICAL_SECTION m_cs;
    bool        m_csInitialized = false;

    // Internal helpers
    PatchEntry* findEntry(uint32_t patchId);
    LedgerResult writeJournalEntry(const PatchEntry& entry, const char* action);
};

} // namespace Patch
} // namespace RawrXD
