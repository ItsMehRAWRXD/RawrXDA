// ============================================================================
// transaction_journal.hpp — WAL-Style Transaction Journal
// ============================================================================
// Write-ahead log for multi-file transaction rollback verification.
// Uses append-only file with checksummed entries.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <mutex>
#include <fstream>
#include <functional>

namespace RawrXD {
namespace Core {

// ============================================================================
// Journal Entry Types
// ============================================================================

enum class JournalEntryType : uint8_t {
    TXN_BEGIN       = 0x01,
    TXN_COMMIT      = 0x02,
    TXN_ROLLBACK    = 0x03,
    FILE_BACKUP     = 0x10,   // Original content saved
    FILE_WRITE      = 0x11,   // New content written
    FILE_DELETE     = 0x12,   // File deleted
    FILE_CREATE     = 0x13,   // New file created
    CHECKPOINT      = 0xF0,   // Compaction marker
};

// ============================================================================
// Journal Entry (on-disk format)
// ============================================================================

#pragma pack(push, 1)
struct JournalEntryHeader {
    uint32_t magic;           // 0x52585754 ("RXWT")
    uint8_t  entryType;       // JournalEntryType
    uint64_t timestamp;       // Epoch ms
    uint32_t txnId;           // Transaction ID
    uint32_t pathLen;         // Length of file path
    uint64_t dataLen;         // Length of payload data
    uint32_t checksum;        // CRC32 of (header sans checksum + path + data)
};
#pragma pack(pop)

struct JournalEntry {
    JournalEntryHeader header;
    std::string filePath;
    std::vector<uint8_t> data;   // File content or metadata

    bool verifyChecksum() const;
    void computeChecksum();
};

// ============================================================================
// Journal Result
// ============================================================================

struct JournalResult {
    bool success;
    const char* detail;
    int errorCode;

    static JournalResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static JournalResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// TransactionJournal
// ============================================================================

class TransactionJournal {
public:
    static constexpr uint32_t MAGIC = 0x52585754;  // "RXWT"

    TransactionJournal();
    ~TransactionJournal();

    // Non-copyable
    TransactionJournal(const TransactionJournal&) = delete;
    TransactionJournal& operator=(const TransactionJournal&) = delete;

    // ---- Lifecycle ----
    JournalResult open(const std::filesystem::path& journalPath);
    void close();
    bool isOpen() const { return m_file.is_open(); }

    // ---- Write Operations ----
    JournalResult logBegin(uint32_t txnId, const std::string& description);
    JournalResult logCommit(uint32_t txnId);
    JournalResult logRollback(uint32_t txnId);
    JournalResult logFileBackup(uint32_t txnId, const std::filesystem::path& file,
                                 const std::string& content);
    JournalResult logFileWrite(uint32_t txnId, const std::filesystem::path& file,
                                const std::string& content);
    JournalResult logFileCreate(uint32_t txnId, const std::filesystem::path& file);
    JournalResult logFileDelete(uint32_t txnId, const std::filesystem::path& file);

    // ---- Read / Recovery ----
    JournalResult readAll(std::vector<JournalEntry>& entries);
    JournalResult recover();   // Replay uncommitted transactions → rollback

    // ---- Maintenance ----
    JournalResult checkpoint();  // Compact journal, remove committed entries
    uint64_t fileSize() const;

    // ---- Transaction ID Generator ----
    uint32_t nextTransactionId();

    // ---- CRC32 utility (public so JournalEntry can use it) ----
    static uint32_t computeCRC32(const void* data, size_t len);
    static uint64_t currentTimestampMs();

private:
    JournalResult writeEntry(JournalEntryType type, uint32_t txnId,
                              const std::string& filePath,
                              const void* data, size_t dataLen);
    JournalResult flushToDisk();

    std::fstream m_file;
    std::filesystem::path m_path;
    mutable std::mutex m_mutex;
    uint32_t m_nextTxnId;
};

} // namespace Core
} // namespace RawrXD
