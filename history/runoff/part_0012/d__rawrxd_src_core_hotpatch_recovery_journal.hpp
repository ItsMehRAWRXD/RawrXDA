// ============================================================================
// hotpatch_recovery_journal.hpp — WAL-Style Recovery Journal for Hotpatches
// ============================================================================
// Persistent, crash-safe write-ahead log for every hotpatch operation across
// all three layers (Memory, Byte, Server). On crash recovery, the journal
// replays uncommitted patches or rolls them back to restore a consistent state.
//
// On-disk format:
//   [Header 32B] [Entry0] [Entry1] ... [EntryN] [Checkpoint]
//   Each entry: [EntryHeader 48B] [patchName] [backupData] [patchData]
//   Entries are checksummed (CRC32) and sequence-numbered.
//
// Recovery semantics:
//   - COMMITTED entries are already applied → skip
//   - PENDING entries → roll back (restore backup data)
//   - ORPHAN entries (no commit marker) → roll back
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"   // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <fstream>

namespace RawrXD {
namespace Recovery {

// ============================================================================
// HotpatchLayer — which layer the patch targets
// ============================================================================
enum class HotpatchLayer : uint8_t {
    Memory  = 0,
    Byte    = 1,
    Server  = 2,
};

const char* hotpatchLayerName(HotpatchLayer layer);

// ============================================================================
// JournalEntryState — lifecycle of a journal entry
// ============================================================================
enum class JournalEntryState : uint8_t {
    Pending     = 0x01,    // Written but not committed
    Committed   = 0x02,    // Successfully applied
    RolledBack  = 0x03,    // Reverted during recovery
    Orphaned    = 0x04,    // Detected during crash recovery, no commit
    Failed      = 0x05,    // Apply attempted but failed
};

// ============================================================================
// On-Disk Journal Header (file-level)
// ============================================================================
#pragma pack(push, 1)
struct JournalFileHeader {
    uint32_t    magic;              // 0x52584A4C ("RXJL")
    uint16_t    version;            // Format version (currently 1)
    uint16_t    flags;              // Reserved
    uint64_t    createdTimestamp;   // Epoch milliseconds
    uint64_t    lastSequenceId;    // Highest committed sequence
    uint32_t    entryCount;        // Total entries (including rolled-back)
    uint32_t    checksum;          // CRC32 of header (sans this field)
};

// On-Disk Journal Entry Header
struct JournalEntryHeader {
    uint32_t    magic;              // 0x52584A45 ("RXJE")
    uint64_t    sequenceId;        // Monotonic
    uint64_t    timestamp;         // Epoch milliseconds
    uint8_t     layer;             // HotpatchLayer
    uint8_t     state;             // JournalEntryState
    uint16_t    nameLen;           // Length of patch name string
    uint32_t    backupLen;         // Length of backup data (for rollback)
    uint32_t    patchLen;          // Length of patch data
    uint64_t    targetAddr;        // For Memory layer: virtual address
    uint64_t    targetOffset;      // For Byte layer: file offset
    uint32_t    checksum;          // CRC32 of (header sans checksum + name + backup + patch)
};
#pragma pack(pop)

// ============================================================================
// In-Memory Journal Entry
// ============================================================================
struct HotpatchJournalEntry {
    JournalEntryHeader header;
    std::string         patchName;
    std::vector<uint8_t> backupData;    // Original bytes for rollback
    std::vector<uint8_t> patchData;     // New bytes that were applied

    bool verifyChecksum() const;
    void computeChecksum();
};

// ============================================================================
// Recovery Report — what happened during crash recovery
// ============================================================================
struct RecoveryReport {
    uint64_t    entriesScanned;
    uint64_t    committedSkipped;
    uint64_t    pendingRolledBack;
    uint64_t    orphansDetected;
    uint64_t    rollbackFailures;
    bool        journalCorrupted;
    std::string summary;

    bool isClean() const {
        return !journalCorrupted && rollbackFailures == 0;
    }
};

// ============================================================================
// JournalStats
// ============================================================================
struct JournalStats {
    std::atomic<uint64_t> totalWrites{0};
    std::atomic<uint64_t> totalCommits{0};
    std::atomic<uint64_t> totalRollbacks{0};
    std::atomic<uint64_t> totalRecoveries{0};
    std::atomic<uint64_t> checkpointCount{0};
    uint64_t              diskBytesUsed = 0;
    uint64_t              oldestSequenceId = 0;
    uint64_t              newestSequenceId = 0;
};

// ============================================================================
// HotpatchRecoveryJournal — the crash-safe write-ahead log
// ============================================================================
class HotpatchRecoveryJournal {
public:
    static constexpr uint32_t FILE_MAGIC  = 0x52584A4C;  // "RXJL"
    static constexpr uint32_t ENTRY_MAGIC = 0x52584A45;  // "RXJE"
    static constexpr uint16_t VERSION     = 1;

    HotpatchRecoveryJournal();
    ~HotpatchRecoveryJournal();

    // Non-copyable
    HotpatchRecoveryJournal(const HotpatchRecoveryJournal&) = delete;
    HotpatchRecoveryJournal& operator=(const HotpatchRecoveryJournal&) = delete;

    // ---- Lifecycle ----
    PatchResult open(const std::string& journalPath);
    void close();
    bool isOpen() const;

    // ---- Write Operations ----
    // Log a pending hotpatch (call before applying the patch)
    PatchResult logPending(HotpatchLayer layer,
                           const std::string& patchName,
                           const void* backupData, size_t backupLen,
                           const void* patchData, size_t patchLen,
                           uint64_t targetAddr = 0,
                           uint64_t targetOffset = 0);

    // Mark a pending entry as committed (call after successful apply)
    PatchResult logCommit(uint64_t sequenceId);

    // Mark a pending entry as failed
    PatchResult logFailed(uint64_t sequenceId, const char* reason);

    // Explicitly roll back a specific entry
    PatchResult logRollback(uint64_t sequenceId);

    // ---- Recovery ----
    // Scan the journal and roll back any uncommitted patches
    RecoveryReport recover();

    // Dry-run recovery: report what would happen without applying
    RecoveryReport recoverDryRun();

    // ---- Checkpointing ----
    // Compact the journal: remove committed entries, keep recent history
    PatchResult checkpoint(size_t keepRecentCount = 100);

    // ---- Queries ----
    uint64_t lastSequenceId() const;
    std::vector<HotpatchJournalEntry> readAllEntries() const;
    std::vector<HotpatchJournalEntry> readPendingEntries() const;
    size_t entryCount() const;

    // ---- Stats ----
    JournalStats getStats() const;
    void resetStats();

    // ---- Export ----
    std::string exportJson(size_t maxEntries = 256) const;

private:
    PatchResult writeEntry(const HotpatchJournalEntry& entry);
    PatchResult updateEntryState(uint64_t sequenceId, JournalEntryState newState);
    PatchResult writeFileHeader();
    PatchResult readFileHeader(JournalFileHeader& out);
    uint32_t computeCRC32(const void* data, size_t len) const;
    PatchResult applyRollback(const HotpatchJournalEntry& entry);
    PatchResult syncToDisk();

    mutable std::mutex  m_mutex;
    std::fstream        m_file;
    std::string         m_path;
    bool                m_isOpen = false;

    // In-memory index of entries (rebuilt from disk on open)
    std::vector<HotpatchJournalEntry> m_entries;
    std::atomic<uint64_t> m_nextSequenceId{1};

    JournalStats m_stats;
};

} // namespace Recovery
} // namespace RawrXD
