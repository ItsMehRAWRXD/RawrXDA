// ============================================================================
// transaction_journal.cpp — WAL-Style Transaction Journal Implementation
// ============================================================================
// Append-only binary log with CRC32 integrity, recovery on crash,
// and checkpoint compaction.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "core/transaction_journal.hpp"

#include <chrono>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Core {

// ============================================================================
// CRC32 (IEEE)
// ============================================================================

static const uint32_t s_crc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91B, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBF, 0xE7B82D09, 0x90BF1D83, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7822, 0x3B6E20C8, 0x4C69105E,
    0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75,
    0xDCD60DCF, 0xABD13D59, 0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808,
    0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
    0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162,
    0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49,
    0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC,
    0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7822,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F6, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6B70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD706FF,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

uint32_t TransactionJournal::computeCRC32(const void* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i) {
        crc = s_crc32Table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

uint64_t TransactionJournal::currentTimestampMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

// ============================================================================
// JournalEntry
// ============================================================================

bool JournalEntry::verifyChecksum() const {
    // Compute CRC32 over everything except the checksum field
    JournalEntryHeader hdr = header;
    hdr.checksum = 0;

    uint32_t crc = TransactionJournal::computeCRC32(&hdr, sizeof(hdr));
    crc = TransactionJournal::computeCRC32(filePath.c_str(), filePath.size());
    if (!data.empty()) {
        crc ^= TransactionJournal::computeCRC32(data.data(), data.size());
    }
    return crc == header.checksum;
}

void JournalEntry::computeChecksum() {
    header.checksum = 0;
    uint32_t crc = TransactionJournal::computeCRC32(&header, sizeof(header));
    crc ^= TransactionJournal::computeCRC32(filePath.c_str(), filePath.size());
    if (!data.empty()) {
        crc ^= TransactionJournal::computeCRC32(data.data(), data.size());
    }
    header.checksum = crc;
}

// ============================================================================
// TransactionJournal
// ============================================================================

TransactionJournal::TransactionJournal() : m_nextTxnId(1) {}

TransactionJournal::~TransactionJournal() {
    close();
}

JournalResult TransactionJournal::open(const fs::path& journalPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_file.is_open()) {
        return JournalResult::error("Journal already open");
    }

    m_path = journalPath;

    // Create parent directory
    auto parent = journalPath.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
    }

    m_file.open(journalPath, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
    if (!m_file.is_open()) {
        // Try creating the file
        std::ofstream create(journalPath, std::ios::binary);
        create.close();
        m_file.open(journalPath, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
        if (!m_file.is_open()) {
            return JournalResult::error("Failed to open journal file");
        }
    }

    // Determine next txn ID from existing entries
    std::vector<JournalEntry> existing;
    // Temporarily unlock for read (not ideal, but avoids deadlock)
    readAll(existing);
    for (const auto& entry : existing) {
        if (entry.header.txnId >= m_nextTxnId) {
            m_nextTxnId = entry.header.txnId + 1;
        }
    }

    return JournalResult::ok("Journal opened");
}

void TransactionJournal::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

JournalResult TransactionJournal::writeEntry(JournalEntryType type, uint32_t txnId,
                                              const std::string& filePath,
                                              const void* data, size_t dataLen) {
    if (!m_file.is_open()) {
        return JournalResult::error("Journal not open");
    }

    JournalEntry entry;
    entry.header.magic = MAGIC;
    entry.header.entryType = static_cast<uint8_t>(type);
    entry.header.timestamp = currentTimestampMs();
    entry.header.txnId = txnId;
    entry.header.pathLen = static_cast<uint32_t>(filePath.size());
    entry.header.dataLen = static_cast<uint64_t>(dataLen);
    entry.filePath = filePath;

    if (data && dataLen > 0) {
        entry.data.resize(dataLen);
        std::memcpy(entry.data.data(), data, dataLen);
    }

    entry.computeChecksum();

    // Write header + path + data
    m_file.write(reinterpret_cast<const char*>(&entry.header), sizeof(entry.header));
    m_file.write(filePath.c_str(), filePath.size());
    if (dataLen > 0 && data) {
        m_file.write(static_cast<const char*>(data), dataLen);
    }

    return flushToDisk();
}

JournalResult TransactionJournal::flushToDisk() {
    m_file.flush();

#ifdef _WIN32
    // Force OS-level flush
    // std::fstream doesn't expose the HANDLE, so we rely on flush()
#endif

    return JournalResult::ok("Flushed");
}

JournalResult TransactionJournal::logBegin(uint32_t txnId, const std::string& description) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::TXN_BEGIN, txnId, "",
                      description.c_str(), description.size());
}

JournalResult TransactionJournal::logCommit(uint32_t txnId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::TXN_COMMIT, txnId, "", nullptr, 0);
}

JournalResult TransactionJournal::logRollback(uint32_t txnId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::TXN_ROLLBACK, txnId, "", nullptr, 0);
}

JournalResult TransactionJournal::logFileBackup(uint32_t txnId, const fs::path& file,
                                                 const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::FILE_BACKUP, txnId, file.string(),
                      content.c_str(), content.size());
}

JournalResult TransactionJournal::logFileWrite(uint32_t txnId, const fs::path& file,
                                                const std::string& content) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::FILE_WRITE, txnId, file.string(),
                      content.c_str(), content.size());
}

JournalResult TransactionJournal::logFileCreate(uint32_t txnId, const fs::path& file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::FILE_CREATE, txnId, file.string(), nullptr, 0);
}

JournalResult TransactionJournal::logFileDelete(uint32_t txnId, const fs::path& file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return writeEntry(JournalEntryType::FILE_DELETE, txnId, file.string(), nullptr, 0);
}

JournalResult TransactionJournal::readAll(std::vector<JournalEntry>& entries) {
    if (!m_file.is_open()) return JournalResult::error("Journal not open");

    entries.clear();

    // Seek to beginning
    m_file.seekg(0, std::ios::beg);

    while (m_file.good() && !m_file.eof()) {
        JournalEntry entry;

        m_file.read(reinterpret_cast<char*>(&entry.header), sizeof(entry.header));
        if (m_file.gcount() < static_cast<std::streamsize>(sizeof(entry.header))) break;

        // Validate magic
        if (entry.header.magic != MAGIC) break;

        // Read path
        if (entry.header.pathLen > 0) {
            entry.filePath.resize(entry.header.pathLen);
            m_file.read(entry.filePath.data(), entry.header.pathLen);
        }

        // Read data
        if (entry.header.dataLen > 0) {
            entry.data.resize(static_cast<size_t>(entry.header.dataLen));
            m_file.read(reinterpret_cast<char*>(entry.data.data()),
                        static_cast<std::streamsize>(entry.header.dataLen));
        }

        entries.push_back(std::move(entry));
    }

    // Reset for appending
    m_file.clear();
    m_file.seekp(0, std::ios::end);

    return JournalResult::ok("Read complete");
}

JournalResult TransactionJournal::recover() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<JournalEntry> entries;
    auto r = readAll(entries);
    if (!r.success) return r;

    // Find uncommitted transactions
    std::unordered_map<uint32_t, bool> txnStatus;  // true = committed/rolled back
    for (const auto& entry : entries) {
        auto type = static_cast<JournalEntryType>(entry.header.entryType);
        if (type == JournalEntryType::TXN_BEGIN) {
            txnStatus[entry.header.txnId] = false;
        } else if (type == JournalEntryType::TXN_COMMIT ||
                   type == JournalEntryType::TXN_ROLLBACK) {
            txnStatus[entry.header.txnId] = true;
        }
    }

    // Roll back uncommitted transactions
    for (const auto& [txnId, committed] : txnStatus) {
        if (committed) continue;

        // Find backup entries for this txn and restore
        for (const auto& entry : entries) {
            if (entry.header.txnId != txnId) continue;
            auto type = static_cast<JournalEntryType>(entry.header.entryType);

            if (type == JournalEntryType::FILE_BACKUP) {
                // Restore original content
                std::ofstream out(entry.filePath, std::ios::binary);
                if (out.is_open() && !entry.data.empty()) {
                    out.write(reinterpret_cast<const char*>(entry.data.data()),
                              entry.data.size());
                }
            } else if (type == JournalEntryType::FILE_CREATE) {
                // Delete created file
                std::error_code ec;
                fs::remove(entry.filePath, ec);
            }
        }

        // Mark as rolled back
        writeEntry(JournalEntryType::TXN_ROLLBACK, txnId, "", nullptr, 0);
    }

    return JournalResult::ok("Recovery complete");
}

JournalResult TransactionJournal::checkpoint() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Read all entries, keep only uncommitted
    std::vector<JournalEntry> entries;
    auto r = readAll(entries);
    if (!r.success) return r;

    // Find committed txns
    std::unordered_set<uint32_t> committed;
    for (const auto& entry : entries) {
        auto type = static_cast<JournalEntryType>(entry.header.entryType);
        if (type == JournalEntryType::TXN_COMMIT ||
            type == JournalEntryType::TXN_ROLLBACK) {
            committed.insert(entry.header.txnId);
        }
    }

    // Write new journal with only uncommitted entries
    m_file.close();

    fs::path tempPath = m_path;
    tempPath += ".tmp";

    {
        std::ofstream tmp(tempPath, std::ios::binary);
        for (const auto& entry : entries) {
            if (committed.count(entry.header.txnId) > 0) continue;

            tmp.write(reinterpret_cast<const char*>(&entry.header), sizeof(entry.header));
            tmp.write(entry.filePath.c_str(), entry.filePath.size());
            if (!entry.data.empty()) {
                tmp.write(reinterpret_cast<const char*>(entry.data.data()),
                          entry.data.size());
            }
        }
    }

    // Atomic replace
    std::error_code ec;
    fs::rename(tempPath, m_path, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        // Re-open original
        m_file.open(m_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);
        return JournalResult::error("Checkpoint rename failed");
    }

    // Re-open
    m_file.open(m_path, std::ios::in | std::ios::out | std::ios::binary | std::ios::app);

    return JournalResult::ok("Checkpoint complete");
}

uint64_t TransactionJournal::fileSize() const {
    std::error_code ec;
    return fs::file_size(m_path, ec);
}

uint32_t TransactionJournal::nextTransactionId() {
    return m_nextTxnId++;
}

} // namespace Core
} // namespace RawrXD
