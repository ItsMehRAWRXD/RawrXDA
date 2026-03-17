// ============================================================================
// patch_rollback_ledger.cpp — SelfPatch Rollback Safety Envelope
// ============================================================================
// WAL-journaled patch ledger with integrity checksums, deterministic rollback,
// emergency crash-safe rollback, and quarantine management.
//
// Pattern: PatchResult-style, no exceptions.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#define NOMINMAX
#include "patch_rollback_ledger.h"

#include <windows.h>
#include <cstdio>
#include <cstring>

namespace RawrXD {
namespace Patch {

// ============================================================================
// FNV-1a 64-bit (local, no external dependency in crash path)
// ============================================================================

static uint64_t fnv1a_local(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (size_t i = 0; i < len; ++i) {
        hash ^= p[i];
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

static uint64_t getCurrentMs() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10000ULL;
}

// ============================================================================
// PatchRollbackLedger
// ============================================================================

PatchRollbackLedger::PatchRollbackLedger() {
    memset(m_entries, 0, sizeof(m_entries));
}

PatchRollbackLedger::~PatchRollbackLedger() {
    shutdown();
}

PatchRollbackLedger& PatchRollbackLedger::Global() {
    static PatchRollbackLedger instance;
    return instance;
}

LedgerResult PatchRollbackLedger::initialize(const char* journalPath) {
    if (m_initialized) return LedgerResult::ok("Already initialized");

    InitializeCriticalSection(&m_cs);
    m_csInitialized = true;
    m_count = 0;
    memset(m_entries, 0, sizeof(m_entries));

    if (journalPath && journalPath[0]) {
        strncpy(m_journalPath, journalPath, sizeof(m_journalPath) - 1);
        m_journalPath[sizeof(m_journalPath) - 1] = '\0';

        m_journalHandle = CreateFileA(m_journalPath, GENERIC_WRITE | GENERIC_READ,
                                       FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                                       nullptr);
        if (m_journalHandle == INVALID_HANDLE_VALUE) {
            return LedgerResult::error("Failed to open journal file", GetLastError());
        }

        // Seek to end for appending
        SetFilePointer(m_journalHandle, 0, nullptr, FILE_END);
    }

    m_initialized = true;
    return LedgerResult::ok("Patch ledger initialized");
}

void PatchRollbackLedger::shutdown() {
    if (!m_initialized) return;

    flushJournal();

    if (m_journalHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_journalHandle);
        m_journalHandle = INVALID_HANDLE_VALUE;
    }

    if (m_csInitialized) {
        DeleteCriticalSection(&m_cs);
        m_csInitialized = false;
    }

    m_initialized = false;
}

// ============================================================================
// Patch Registration
// ============================================================================

LedgerResult PatchRollbackLedger::recordApply(uint32_t patchId, const char* name,
                                               void* targetAddress, uint32_t size,
                                               const void* newBytes, uint32_t newSize) {
    if (!m_initialized) return LedgerResult::error("Not initialized");
    if (size > 256 || newSize > 256) return LedgerResult::error("Patch too large (max 256 bytes)");

    EnterCriticalSection(&m_cs);

    // Check if already exists
    PatchEntry* existing = findEntry(patchId);
    if (existing) {
        existing->applicationCount++;
        existing->state = PatchEntry::Applied;
        existing->version.revision++;
        existing->version.timestampMs = getCurrentMs();
        // Update new bytes
        memcpy(existing->newBytes, newBytes, newSize);
        existing->newSize = newSize;
        existing->checksumAfter = fnv1a_local(newBytes, newSize);
        writeJournalEntry(*existing, "RE-APPLY");
        LeaveCriticalSection(&m_cs);
        return LedgerResult::ok("Re-applied");
    }

    if (m_count >= MAX_PATCHES) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch table full", -2);
    }

    PatchEntry& entry = m_entries[m_count];
    memset(&entry, 0, sizeof(entry));
    entry.patchId = patchId;
    if (name) {
        strncpy(entry.name, name, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
    }
    entry.version = {1, 0, 0, getCurrentMs()};
    entry.targetAddress = targetAddress;
    entry.patchSize = size;

    // Capture original bytes for rollback
    if (targetAddress && size > 0) {
        DWORD oldProtect = 0;
        VirtualProtect(targetAddress, size, PAGE_EXECUTE_READ, &oldProtect);
        memcpy(entry.originalBytes, targetAddress, size);
        entry.originalSize = size;
        entry.checksumBefore = fnv1a_local(entry.originalBytes, size);
        VirtualProtect(targetAddress, size, oldProtect, &oldProtect);
    }

    // Store new bytes
    if (newBytes && newSize > 0) {
        memcpy(entry.newBytes, newBytes, newSize);
        entry.newSize = newSize;
        entry.checksumAfter = fnv1a_local(newBytes, newSize);
    }

    entry.state = PatchEntry::Applied;
    entry.applicationCount = 1;
    entry.lastValidatedMs = static_cast<int64_t>(getCurrentMs());

    m_count++;
    writeJournalEntry(entry, "APPLY");

    LeaveCriticalSection(&m_cs);
    return LedgerResult::ok("Patch recorded and applied");
}

LedgerResult PatchRollbackLedger::recordRollback(uint32_t patchId) {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    PatchEntry* entry = findEntry(patchId);
    if (!entry) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not found");
    }
    entry->state = PatchEntry::RolledBack;
    entry->rollbackCount++;
    writeJournalEntry(*entry, "ROLLBACK");
    LeaveCriticalSection(&m_cs);
    return LedgerResult::ok("Rollback recorded");
}

LedgerResult PatchRollbackLedger::recordFailure(uint32_t patchId, const char* reason) {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    PatchEntry* entry = findEntry(patchId);
    if (!entry) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not found");
    }
    entry->state = PatchEntry::Failed;
    entry->failureCount++;
    writeJournalEntry(*entry, reason ? reason : "FAILURE");
    LeaveCriticalSection(&m_cs);
    return LedgerResult::ok("Failure recorded");
}

LedgerResult PatchRollbackLedger::quarantine(uint32_t patchId) {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    PatchEntry* entry = findEntry(patchId);
    if (!entry) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not found");
    }
    entry->state = PatchEntry::Quarantined;
    writeJournalEntry(*entry, "QUARANTINE");
    LeaveCriticalSection(&m_cs);
    return LedgerResult::ok("Patch quarantined");
}

// ============================================================================
// Rollback
// ============================================================================

LedgerResult PatchRollbackLedger::rollback(uint32_t patchId) {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    PatchEntry* entry = findEntry(patchId);
    if (!entry) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not found");
    }
    if (entry->state != PatchEntry::Applied) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not in Applied state");
    }
    if (!entry->targetAddress || entry->originalSize == 0) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("No original bytes to restore");
    }

    // Restore original bytes
    DWORD oldProtect = 0;
    if (!VirtualProtect(entry->targetAddress, entry->originalSize,
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("VirtualProtect failed", GetLastError());
    }

    memcpy(entry->targetAddress, entry->originalBytes, entry->originalSize);
    FlushInstructionCache(GetCurrentProcess(), entry->targetAddress, entry->originalSize);

    VirtualProtect(entry->targetAddress, entry->originalSize, oldProtect, &oldProtect);

    entry->state = PatchEntry::RolledBack;
    entry->rollbackCount++;
    writeJournalEntry(*entry, "ROLLBACK-RESTORE");

    LeaveCriticalSection(&m_cs);
    return LedgerResult::ok("Patch rolled back");
}

LedgerResult PatchRollbackLedger::rollbackAll() {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    int rolled = 0;
    // Reverse order
    for (int i = m_count - 1; i >= 0; --i) {
        if (m_entries[i].state == PatchEntry::Applied) {
            LeaveCriticalSection(&m_cs);
            auto r = rollback(m_entries[i].patchId);
            EnterCriticalSection(&m_cs);
            if (r.success) rolled++;
        }
    }
    LeaveCriticalSection(&m_cs);

    char msg[64];
    snprintf(msg, sizeof(msg), "Rolled back %d patches", rolled);
    return LedgerResult::ok(msg);
}

int PatchRollbackLedger::emergencyRollbackAll() {
    // Crash-safe path — no mutex (may deadlock in crash handler)
    int rolled = 0;
    for (int i = m_count - 1; i >= 0; --i) {
        if (m_entries[i].state == PatchEntry::Applied &&
            m_entries[i].targetAddress &&
            m_entries[i].originalSize > 0) {
            DWORD oldProtect = 0;
            if (VirtualProtect(m_entries[i].targetAddress,
                               m_entries[i].originalSize,
                               PAGE_EXECUTE_READWRITE, &oldProtect)) {
                memcpy(m_entries[i].targetAddress,
                       m_entries[i].originalBytes,
                       m_entries[i].originalSize);
                FlushInstructionCache(GetCurrentProcess(),
                                      m_entries[i].targetAddress,
                                      m_entries[i].originalSize);
                VirtualProtect(m_entries[i].targetAddress,
                               m_entries[i].originalSize,
                               oldProtect, &oldProtect);
                m_entries[i].state = PatchEntry::RolledBack;
                rolled++;
            }
        }
    }
    return rolled;
}

// ============================================================================
// Integrity Validation
// ============================================================================

LedgerResult PatchRollbackLedger::validate(uint32_t patchId) {
    if (!m_initialized) return LedgerResult::error("Not initialized");

    EnterCriticalSection(&m_cs);
    PatchEntry* entry = findEntry(patchId);
    if (!entry) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Patch not found");
    }
    if (!entry->targetAddress || entry->patchSize == 0) {
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("No target to validate");
    }

    uint64_t current = computeChecksum(entry->targetAddress, entry->patchSize);
    entry->checksumValidated = current;
    entry->lastValidatedMs = static_cast<int64_t>(getCurrentMs());

    bool match = false;
    if (entry->state == PatchEntry::Applied) {
        match = (current == entry->checksumAfter);
    } else if (entry->state == PatchEntry::RolledBack) {
        match = (current == entry->checksumBefore);
    }

    if (match) {
        entry->state = PatchEntry::Validated;
        writeJournalEntry(*entry, "VALIDATED");
        LeaveCriticalSection(&m_cs);
        return LedgerResult::ok("Integrity check passed");
    } else {
        entry->failureCount++;
        writeJournalEntry(*entry, "INTEGRITY-FAIL");
        LeaveCriticalSection(&m_cs);
        return LedgerResult::error("Integrity check FAILED — checksum mismatch");
    }
}

int PatchRollbackLedger::validateAll() {
    if (!m_initialized) return -1;

    int failures = 0;
    EnterCriticalSection(&m_cs);
    for (int i = 0; i < m_count; ++i) {
        if (m_entries[i].state == PatchEntry::Applied ||
            m_entries[i].state == PatchEntry::Validated) {
            LeaveCriticalSection(&m_cs);
            auto r = validate(m_entries[i].patchId);
            EnterCriticalSection(&m_cs);
            if (!r.success) failures++;
        }
    }
    LeaveCriticalSection(&m_cs);
    return failures;
}

uint64_t PatchRollbackLedger::computeChecksum(const void* data, size_t size) const {
    return fnv1a_local(data, size);
}

// ============================================================================
// Query
// ============================================================================

const PatchEntry* PatchRollbackLedger::getEntry(uint32_t patchId) const {
    for (int i = 0; i < m_count; ++i) {
        if (m_entries[i].patchId == patchId) return &m_entries[i];
    }
    return nullptr;
}

int PatchRollbackLedger::appliedCount() const {
    int count = 0;
    for (int i = 0; i < m_count; ++i) {
        if (m_entries[i].state == PatchEntry::Applied) count++;
    }
    return count;
}

int PatchRollbackLedger::quarantinedCount() const {
    int count = 0;
    for (int i = 0; i < m_count; ++i) {
        if (m_entries[i].state == PatchEntry::Quarantined) count++;
    }
    return count;
}

// ============================================================================
// Journal I/O
// ============================================================================

LedgerResult PatchRollbackLedger::flushJournal() {
    if (m_journalHandle == INVALID_HANDLE_VALUE) return LedgerResult::ok("No journal");
    FlushFileBuffers(m_journalHandle);
    return LedgerResult::ok("Journal flushed");
}

LedgerResult PatchRollbackLedger::replayJournal(const char* path) {
    const char* replayPath = (path && path[0]) ? path : m_journalPath;
    if (!replayPath[0])
        return LedgerResult::error("No journal path specified", -1);

    HANDLE hFile = CreateFileA(replayPath, GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return LedgerResult::error("Cannot open journal file", GetLastError());

    // Read entire journal into memory (WAL journals are small)
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    if (fileSize.QuadPart > 10 * 1024 * 1024) { // 10 MB safety limit
        CloseHandle(hFile);
        return LedgerResult::error("Journal file exceeds 10 MB safety limit", -2);
    }

    size_t sz = static_cast<size_t>(fileSize.QuadPart);
    if (sz == 0) {
        CloseHandle(hFile);
        return LedgerResult::ok("Empty journal — nothing to replay");
    }

    char* buf = new char[sz + 1];
    DWORD bytesRead = 0;
    ReadFile(hFile, buf, static_cast<DWORD>(sz), &bytesRead, nullptr);
    CloseHandle(hFile);
    buf[bytesRead] = '\0';

    // Parse line by line.
    // Format: [timestamp_ms] ACTION patch=ID name=NAME state=N appCount=N rbCount=N chkBefore=HEX chkAfter=HEX
    int replayed = 0;
    char* line = buf;
    while (line && *line) {
        char* eol = strstr(line, "\r\n");
        if (!eol) eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        // Skip empty lines
        if (line[0] == '\0') {
            line = eol ? eol + (eol[1] == '\n' ? 2 : 1) : nullptr;
            continue;
        }

        // Parse: [timestamp] action patch=ID name=NAME state=N ...
        uint64_t timestamp = 0;
        char action[32] = {};
        uint32_t patchId = 0;
        char patchName[64] = {};
        int state = 0;
        uint32_t appCount = 0, rbCount = 0;
        uint64_t chkBefore = 0, chkAfter = 0;

        // Skip the [timestamp] prefix
        char* p = line;
        if (*p == '[') {
            p++;
            timestamp = strtoull(p, &p, 10);
            if (*p == ']') p++;
            while (*p == ' ') p++;
        }

        // Read action word
        int ai = 0;
        while (*p && *p != ' ' && ai < 31) action[ai++] = *p++;
        action[ai] = '\0';
        while (*p == ' ') p++;

        // Parse key=value pairs
        while (*p) {
            char key[32] = {};
            int ki = 0;
            while (*p && *p != '=' && ki < 31) key[ki++] = *p++;
            key[ki] = '\0';
            if (*p == '=') p++;

            if (strcmp(key, "patch") == 0) {
                patchId = static_cast<uint32_t>(strtoul(p, &p, 10));
            } else if (strcmp(key, "name") == 0) {
                int ni = 0;
                while (*p && *p != ' ' && ni < 63) patchName[ni++] = *p++;
                patchName[ni] = '\0';
            } else if (strcmp(key, "state") == 0) {
                state = static_cast<int>(strtol(p, &p, 10));
            } else if (strcmp(key, "appCount") == 0) {
                appCount = static_cast<uint32_t>(strtoul(p, &p, 10));
            } else if (strcmp(key, "rbCount") == 0) {
                rbCount = static_cast<uint32_t>(strtoul(p, &p, 10));
            } else if (strcmp(key, "chkBefore") == 0) {
                chkBefore = strtoull(p, &p, 16);
            } else if (strcmp(key, "chkAfter") == 0) {
                chkAfter = strtoull(p, &p, 16);
            } else {
                // Skip unknown key's value
                while (*p && *p != ' ') p++;
            }
            while (*p == ' ') p++;
        }

        // Reconstruct entry in ledger
        if (patchId > 0) {
            PatchEntry* existing = findEntry(patchId);
            if (!existing && m_count < MAX_PATCHES) {
                existing = &m_entries[m_count++];
                memset(existing, 0, sizeof(PatchEntry));
                existing->patchId = patchId;
                strncpy(existing->name, patchName, sizeof(existing->name) - 1);
            }

            if (existing) {
                existing->state = static_cast<PatchEntry::State>(state);
                existing->applicationCount = appCount;
                existing->rollbackCount = rbCount;
                existing->checksumBefore = chkBefore;
                existing->checksumAfter = chkAfter;
                existing->version.timestampMs = timestamp;
                replayed++;
            }
        }

        line = eol ? eol + 1 : nullptr;
        if (line && *line == '\n') line++; // Skip \r\n second byte
    }

    delete[] buf;

    char msg[128];
    snprintf(msg, sizeof(msg), "Replayed %d journal entries, %d patches reconstructed",
             replayed, m_count);
    return LedgerResult::ok(msg);
}

LedgerResult PatchRollbackLedger::writeJournalEntry(const PatchEntry& entry,
                                                     const char* action) {
    if (m_journalHandle == INVALID_HANDLE_VALUE) return LedgerResult::ok("No journal");

    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "[%llu] %s patch=%u name=%s state=%d appCount=%u rbCount=%u chkBefore=%016llX chkAfter=%016llX\r\n",
        (unsigned long long)getCurrentMs(), action,
        entry.patchId, entry.name, (int)entry.state,
        entry.applicationCount, entry.rollbackCount,
        (unsigned long long)entry.checksumBefore,
        (unsigned long long)entry.checksumAfter);

    DWORD written = 0;
    WriteFile(m_journalHandle, buf, (DWORD)len, &written, nullptr);
    return LedgerResult::ok();
}

// ============================================================================
// Internal
// ============================================================================

PatchEntry* PatchRollbackLedger::findEntry(uint32_t patchId) {
    for (int i = 0; i < m_count; ++i) {
        if (m_entries[i].patchId == patchId) return &m_entries[i];
    }
    return nullptr;
}

} // namespace Patch
} // namespace RawrXD
