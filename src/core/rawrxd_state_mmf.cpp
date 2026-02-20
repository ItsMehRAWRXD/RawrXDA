// ============================================================================
// rawrxd_state_mmf.cpp — Cross-Process State Synchronization Implementation
// ============================================================================
//
// Phase 36: MMF State Sync — Full Implementation
//
// Implements RawrXDStateMmf for zero-latency cross-process state broadcast
// using Win32 Memory-Mapped Files with seqlock readers and named-mutex writers.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "rawrxd_state_mmf.hpp"
#include "model_memory_hotpatch.hpp" // For PatchResult

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================================
// Singleton
// ============================================================================

RawrXDStateMmf& RawrXDStateMmf::instance() {
    static RawrXDStateMmf s_instance;
    return s_instance;
}

RawrXDStateMmf::RawrXDStateMmf()
    : m_hMapFile(nullptr)
    , m_hMutex(nullptr)
    , m_hEvent(nullptr)
    , m_mapView(nullptr)
    , m_initialized(false)
    , m_isCreator(false)
    , m_processId(0)
    , m_processType(0)
    , m_stats{}
{
    std::memset(m_processName, 0, sizeof(m_processName));
}

RawrXDStateMmf::~RawrXDStateMmf() {
    if (m_initialized) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

PatchResult RawrXDStateMmf::initialize(uint8_t processType, const char* processName) {
    if (m_initialized) {
        return PatchResult::error("RawrXDStateMmf already initialized", -1);
    }

    m_processId = GetCurrentProcessId();
    m_processType = processType;
    if (processName) {
        std::strncpy(m_processName, processName, sizeof(m_processName) - 1);
    }

    // Create or open the named mutex (writer serialization)
    m_hMutex = CreateMutexW(nullptr, FALSE, MUTEX_NAME);
    if (!m_hMutex) {
        DWORD err = GetLastError();
        char msg[128];
        std::snprintf(msg, sizeof(msg), "CreateMutex failed (error %lu)", err);
        return PatchResult::error(msg, static_cast<int>(err));
    }

    // Create or open the named event (change notification)
    m_hEvent = CreateEventW(nullptr, TRUE, FALSE, EVENT_NAME);
    if (!m_hEvent) {
        DWORD err = GetLastError();
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
        char msg[128];
        std::snprintf(msg, sizeof(msg), "CreateEvent failed (error %lu)", err);
        return PatchResult::error(msg, static_cast<int>(err));
    }

    // Create or open the memory-mapped file
    m_hMapFile = CreateFileMappingW(
        INVALID_HANDLE_VALUE,   // Backed by page file
        nullptr,                // Default security
        PAGE_READWRITE,
        0,                      // High DWORD of size
        MMF_SIZE,               // Low DWORD of size
        MMF_NAME);

    if (!m_hMapFile) {
        DWORD err = GetLastError();
        CloseHandle(m_hEvent);
        CloseHandle(m_hMutex);
        m_hEvent = nullptr;
        m_hMutex = nullptr;
        char msg[128];
        std::snprintf(msg, sizeof(msg), "CreateFileMapping failed (error %lu)", err);
        return PatchResult::error(msg, static_cast<int>(err));
    }

    m_isCreator = (GetLastError() != ERROR_ALREADY_EXISTS);

    // Map view of the file
    m_mapView = MapViewOfFile(
        m_hMapFile,
        FILE_MAP_ALL_ACCESS,
        0, 0, MMF_SIZE);

    if (!m_mapView) {
        DWORD err = GetLastError();
        CloseHandle(m_hMapFile);
        CloseHandle(m_hEvent);
        CloseHandle(m_hMutex);
        m_hMapFile = nullptr;
        m_hEvent = nullptr;
        m_hMutex = nullptr;
        char msg[128];
        std::snprintf(msg, sizeof(msg), "MapViewOfFile failed (error %lu)", err);
        return PatchResult::error(msg, static_cast<int>(err));
    }

    // Initialize header if we are the creator
    if (m_isCreator) {
        WaitForSingleObject(m_hMutex, INFINITE);

        SharedStateHeader* h = header();
        std::memset(h, 0, sizeof(SharedStateHeader));
        h->magic = 0x53445852; // 'RXDS'
        h->version = 1;
        h->headerSize = sizeof(SharedStateHeader);
        h->totalSize = MMF_SIZE;
        h->writeSequence = 0;
        h->patchCount = 0;
        h->configCount = 0;
        h->processCount = 0;
        h->eventHead = 0;
        h->totalPatchOperations = 0;
        h->totalConfigUpdates = 0;
        h->serverStartTime = GetTickCount64();

        ReleaseMutex(m_hMutex);

        OutputDebugStringA("[RawrXDStateMmf] Created new shared state region\n");
    } else {
        // Validate existing header
        const SharedStateHeader* h = header();
        if (h->magic != 0x53445852 || h->version != 1) {
            UnmapViewOfFile(m_mapView);
            CloseHandle(m_hMapFile);
            CloseHandle(m_hEvent);
            CloseHandle(m_hMutex);
            m_mapView = nullptr;
            m_hMapFile = nullptr;
            m_hEvent = nullptr;
            m_hMutex = nullptr;
            return PatchResult::error("Shared state header validation failed", -2);
        }

        OutputDebugStringA("[RawrXDStateMmf] Attached to existing shared state region\n");
    }

    // Register this process
    {
        WaitForSingleObject(m_hMutex, INFINITE);

        SharedStateHeader* h = header();
        beginWrite();

        // Find empty slot or reuse dead process slot
        int slot = -1;
        for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
            if (h->processes[i].processId == 0 || !h->processes[i].alive) {
                slot = static_cast<int>(i);
                break;
            }
        }

        if (slot >= 0) {
            h->processes[slot].processId = m_processId;
            h->processes[slot].processType = m_processType;
            h->processes[slot].alive = 1;
            h->processes[slot].lastHeartbeat = GetTickCount64();
            std::strncpy(h->processes[slot].processName, m_processName,
                         sizeof(h->processes[slot].processName) - 1);

            // Update process count
            uint32_t count = 0;
            for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
                if (h->processes[i].alive) count++;
            }
            h->processCount = count;
        }

        endWrite();
        ReleaseMutex(m_hMutex);
    }

    m_initialized = true;

    char info[256];
    std::snprintf(info, sizeof(info),
                  "[RawrXDStateMmf] Initialized — PID=%lu, type=%u, name=%s, creator=%s\n",
                  m_processId, m_processType, m_processName,
                  m_isCreator ? "yes" : "no");
    OutputDebugStringA(info);

    return PatchResult::ok("RawrXDStateMmf initialized");
}

PatchResult RawrXDStateMmf::shutdown() {
    if (!m_initialized) {
        return PatchResult::error("RawrXDStateMmf not initialized", -1);
    }

    // Unregister this process
    if (m_mapView && m_hMutex) {
        WaitForSingleObject(m_hMutex, 1000);

        SharedStateHeader* h = header();
        beginWrite();

        for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
            if (h->processes[i].processId == m_processId) {
                h->processes[i].alive = 0;
                h->processes[i].processId = 0;
                break;
            }
        }

        // Update process count
        uint32_t count = 0;
        for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
            if (h->processes[i].alive) count++;
        }
        h->processCount = count;

        endWrite();
        ReleaseMutex(m_hMutex);
    }

    // Unmap and close handles
    if (m_mapView) {
        UnmapViewOfFile(m_mapView);
        m_mapView = nullptr;
    }
    if (m_hMapFile) {
        CloseHandle(m_hMapFile);
        m_hMapFile = nullptr;
    }
    if (m_hEvent) {
        CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }
    if (m_hMutex) {
        CloseHandle(m_hMutex);
        m_hMutex = nullptr;
    }

    m_initialized = false;
    OutputDebugStringA("[RawrXDStateMmf] Shutdown complete\n");
    return PatchResult::ok("RawrXDStateMmf shutdown");
}

bool RawrXDStateMmf::isInitialized() const {
    return m_initialized;
}

// ============================================================================
// Seqlock Primitives
// ============================================================================

void RawrXDStateMmf::beginWrite() {
    SharedStateHeader* h = header();
    // Increment to odd (signals write in progress)
    h->writeSequence++;
    MemoryBarrier();
}

void RawrXDStateMmf::endWrite() {
    SharedStateHeader* h = header();
    MemoryBarrier();
    // Increment to even (signals write complete)
    h->writeSequence++;

    // Signal change event
    if (m_hEvent) {
        PulseEvent(m_hEvent);
    }
}

bool RawrXDStateMmf::beginRead(uint64_t* outSeq) const {
    const SharedStateHeader* h = header();
    *outSeq = h->writeSequence;
    MemoryBarrier();
    // If odd, a write is in progress — caller should retry
    return (*outSeq & 1) == 0;
}

bool RawrXDStateMmf::endRead(uint64_t seq) const {
    MemoryBarrier();
    const SharedStateHeader* h = header();
    return h->writeSequence == seq;
}

// ============================================================================
// Header Access
// ============================================================================

SharedStateHeader* RawrXDStateMmf::header() {
    return static_cast<SharedStateHeader*>(m_mapView);
}

const SharedStateHeader* RawrXDStateMmf::header() const {
    return static_cast<const SharedStateHeader*>(m_mapView);
}

// ============================================================================
// Patch State
// ============================================================================

PatchResult RawrXDStateMmf::publishPatch(const MmfPatchEntry& entry) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    // Find empty slot or append
    int slot = -1;
    for (size_t i = 0; i < MMF_MAX_PATCHES; ++i) {
        if (h->patches[i].id == 0) {
            slot = static_cast<int>(i);
            break;
        }
    }

    if (slot < 0) {
        endWrite();
        ReleaseMutex(m_hMutex);
        return PatchResult::error("Patch table full", -2);
    }

    h->patches[slot] = entry;
    h->patches[slot].timestamp = GetTickCount64();
    if (static_cast<uint32_t>(slot) >= h->patchCount) {
        h->patchCount = static_cast<uint32_t>(slot) + 1;
    }
    h->totalPatchOperations++;

    endWrite();
    ReleaseMutex(m_hMutex);

    m_stats.writesPerformed++;
    return PatchResult::ok("Patch published to shared state");
}

PatchResult RawrXDStateMmf::updatePatchStatus(uint64_t patchId, uint8_t newStatus) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    bool found = false;
    for (size_t i = 0; i < MMF_MAX_PATCHES; ++i) {
        if (h->patches[i].id == patchId) {
            h->patches[i].status = newStatus;
            h->patches[i].timestamp = GetTickCount64();
            found = true;
            break;
        }
    }

    endWrite();
    ReleaseMutex(m_hMutex);

    if (!found) return PatchResult::error("Patch not found", -3);

    m_stats.writesPerformed++;
    return PatchResult::ok("Patch status updated");
}

PatchResult RawrXDStateMmf::removePatch(uint64_t patchId) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    for (size_t i = 0; i < MMF_MAX_PATCHES; ++i) {
        if (h->patches[i].id == patchId) {
            std::memset(&h->patches[i], 0, sizeof(MmfPatchEntry));
            break;
        }
    }

    endWrite();
    ReleaseMutex(m_hMutex);

    m_stats.writesPerformed++;
    return PatchResult::ok("Patch removed");
}

size_t RawrXDStateMmf::readPatches(MmfPatchEntry* outPatches, size_t maxPatches) const {
    if (!m_initialized || !outPatches) return 0;

    // Lock-free read via seqlock
    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        uint32_t count = h->patchCount;
        if (count > maxPatches) count = static_cast<uint32_t>(maxPatches);
        if (count > MMF_MAX_PATCHES) count = MMF_MAX_PATCHES;

        std::memcpy(outPatches, h->patches, count * sizeof(MmfPatchEntry));

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return count;
        }

        m_stats.seqlockRetries++;
    }

    // Fallback: acquire mutex
    WaitForSingleObject(m_hMutex, 1000);
    const SharedStateHeader* h = header();
    uint32_t count = h->patchCount;
    if (count > maxPatches) count = static_cast<uint32_t>(maxPatches);
    if (count > MMF_MAX_PATCHES) count = MMF_MAX_PATCHES;
    std::memcpy(outPatches, h->patches, count * sizeof(MmfPatchEntry));
    ReleaseMutex(m_hMutex);

    m_stats.readsPerformed++;
    return count;
}

size_t RawrXDStateMmf::getPatchCount() const {
    if (!m_initialized) return 0;
    const SharedStateHeader* h = header();
    return h->patchCount;
}

// ============================================================================
// Configuration State
// ============================================================================

PatchResult RawrXDStateMmf::publishConfig(const char* key, const char* value) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    if (!key || !value) return PatchResult::error("Null key or value", -2);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    // Check for existing key (update in place)
    int slot = -1;
    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
        if (h->configs[i].key[0] != '\0' && std::strcmp(h->configs[i].key, key) == 0) {
            slot = static_cast<int>(i);
            break;
        }
    }

    // If not found, find empty slot
    if (slot < 0) {
        for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
            if (h->configs[i].key[0] == '\0') {
                slot = static_cast<int>(i);
                break;
            }
        }
    }

    if (slot < 0) {
        endWrite();
        ReleaseMutex(m_hMutex);
        return PatchResult::error("Config table full", -3);
    }

    std::strncpy(h->configs[slot].key, key, MMF_MAX_STRING_LEN - 1);
    h->configs[slot].key[MMF_MAX_STRING_LEN - 1] = '\0';
    std::strncpy(h->configs[slot].value, value, MMF_MAX_STRING_LEN - 1);
    h->configs[slot].value[MMF_MAX_STRING_LEN - 1] = '\0';
    h->configs[slot].lastModified = GetTickCount64();
    h->configs[slot].modifierProcessId = m_processId;

    // Update count
    uint32_t count = 0;
    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
        if (h->configs[i].key[0] != '\0') count++;
    }
    h->configCount = count;
    h->totalConfigUpdates++;

    endWrite();
    ReleaseMutex(m_hMutex);

    m_stats.writesPerformed++;
    return PatchResult::ok("Config published");
}

PatchResult RawrXDStateMmf::removeConfig(const char* key) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);
    if (!key) return PatchResult::error("Null key", -2);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
        if (h->configs[i].key[0] != '\0' && std::strcmp(h->configs[i].key, key) == 0) {
            std::memset(&h->configs[i], 0, sizeof(MmfConfigEntry));
            break;
        }
    }

    // Update count
    uint32_t count = 0;
    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
        if (h->configs[i].key[0] != '\0') count++;
    }
    h->configCount = count;

    endWrite();
    ReleaseMutex(m_hMutex);

    m_stats.writesPerformed++;
    return PatchResult::ok("Config removed");
}

bool RawrXDStateMmf::readConfig(const char* key, char* outValue, size_t maxLen) const {
    if (!m_initialized || !key || !outValue || maxLen == 0) return false;

    // Lock-free read via seqlock
    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        bool found = false;
        for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
            if (h->configs[i].key[0] != '\0' && std::strcmp(h->configs[i].key, key) == 0) {
                std::strncpy(outValue, h->configs[i].value, maxLen - 1);
                outValue[maxLen - 1] = '\0';
                found = true;
                break;
            }
        }

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return found;
        }
        m_stats.seqlockRetries++;
    }

    // Fallback
    WaitForSingleObject(m_hMutex, 1000);
    const SharedStateHeader* h = header();
    bool found = false;
    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
        if (h->configs[i].key[0] != '\0' && std::strcmp(h->configs[i].key, key) == 0) {
            std::strncpy(outValue, h->configs[i].value, maxLen - 1);
            outValue[maxLen - 1] = '\0';
            found = true;
            break;
        }
    }
    ReleaseMutex(m_hMutex);
    m_stats.readsPerformed++;
    return found;
}

size_t RawrXDStateMmf::readAllConfigs(MmfConfigEntry* outConfigs, size_t maxConfigs) const {
    if (!m_initialized || !outConfigs) return 0;

    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        size_t count = 0;
        for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES && count < maxConfigs; ++i) {
            if (h->configs[i].key[0] != '\0') {
                outConfigs[count++] = h->configs[i];
            }
        }

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return count;
        }
        m_stats.seqlockRetries++;
    }

    // Fallback
    WaitForSingleObject(m_hMutex, 1000);
    const SharedStateHeader* h = header();
    size_t count = 0;
    for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES && count < maxConfigs; ++i) {
        if (h->configs[i].key[0] != '\0') {
            outConfigs[count++] = h->configs[i];
        }
    }
    ReleaseMutex(m_hMutex);
    m_stats.readsPerformed++;
    return count;
}

// ============================================================================
// Model State
// ============================================================================

PatchResult RawrXDStateMmf::publishModelState(const MmfModelState& state) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();
    h->modelState = state;
    endWrite();

    ReleaseMutex(m_hMutex);
    m_stats.writesPerformed++;
    return PatchResult::ok("Model state published");
}

MmfModelState RawrXDStateMmf::readModelState() const {
    MmfModelState result;
    std::memset(&result, 0, sizeof(result));
    if (!m_initialized) return result;

    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        result = h->modelState;

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return result;
        }
        m_stats.seqlockRetries++;
    }

    // Fallback
    WaitForSingleObject(m_hMutex, 1000);
    result = header()->modelState;
    ReleaseMutex(m_hMutex);
    m_stats.readsPerformed++;
    return result;
}

// ============================================================================
// Memory Stats
// ============================================================================

PatchResult RawrXDStateMmf::publishMemoryStats(const MmfMemoryStats& stats) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();
    h->memoryStats = stats;
    h->memoryStats.lastUpdateTimestamp = GetTickCount64();
    endWrite();

    ReleaseMutex(m_hMutex);
    m_stats.writesPerformed++;
    return PatchResult::ok("Memory stats published");
}

MmfMemoryStats RawrXDStateMmf::readMemoryStats() const {
    MmfMemoryStats result;
    std::memset(&result, 0, sizeof(result));
    if (!m_initialized) return result;

    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        result = header()->memoryStats;

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return result;
        }
        m_stats.seqlockRetries++;
    }

    // Fallback
    WaitForSingleObject(m_hMutex, 1000);
    result = header()->memoryStats;
    ReleaseMutex(m_hMutex);
    m_stats.readsPerformed++;
    return result;
}

// ============================================================================
// Event Broadcast
// ============================================================================

PatchResult RawrXDStateMmf::broadcastEvent(uint8_t eventType, const char* detail) {
    if (!m_initialized) return PatchResult::error("Not initialized", -1);

    WaitForSingleObject(m_hMutex, INFINITE);

    SharedStateHeader* h = header();
    beginWrite();

    uint64_t idx = h->eventHead % MMF_MAX_EVENTS;
    h->events[idx].sequenceId = h->eventHead;
    h->events[idx].timestamp = GetTickCount64();
    h->events[idx].sourceProcessId = m_processId;
    h->events[idx].eventType = eventType;
    if (detail) {
        std::strncpy(h->events[idx].detail, detail, MMF_MAX_STRING_LEN - 1);
        h->events[idx].detail[MMF_MAX_STRING_LEN - 1] = '\0';
    } else {
        h->events[idx].detail[0] = '\0';
    }
    h->eventHead++;

    endWrite();
    ReleaseMutex(m_hMutex);

    m_stats.eventsPublished++;
    return PatchResult::ok("Event broadcast");
}

size_t RawrXDStateMmf::pollEvents(MmfEvent* outEvents, size_t maxEvents,
                                    uint64_t* lastSequence) const {
    if (!m_initialized || !outEvents || !lastSequence) return 0;

    // Lock-free read
    for (int attempt = 0; attempt < 100; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        uint64_t head = h->eventHead;
        uint64_t start = *lastSequence;

        // No new events
        if (start >= head) {
            if (endRead(seq)) {
                m_stats.readsPerformed++;
                return 0;
            }
            m_stats.seqlockRetries++;
            continue;
        }

        // Clamp to available events (ring buffer can wrap)
        if (head - start > MMF_MAX_EVENTS) {
            start = head - MMF_MAX_EVENTS;
        }

        size_t count = 0;
        for (uint64_t i = start; i < head && count < maxEvents; ++i) {
            uint64_t idx = i % MMF_MAX_EVENTS;
            outEvents[count++] = h->events[idx];
        }

        uint64_t newLast = head;

        if (endRead(seq)) {
            *lastSequence = newLast;
            m_stats.readsPerformed++;
            m_stats.eventsConsumed += count;
            return count;
        }
        m_stats.seqlockRetries++;
    }

    return 0;
}

// ============================================================================
// Process Registry
// ============================================================================

void RawrXDStateMmf::heartbeat() {
    if (!m_initialized) return;

    WaitForSingleObject(m_hMutex, 100);

    SharedStateHeader* h = header();
    beginWrite();

    for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
        if (h->processes[i].processId == m_processId) {
            h->processes[i].lastHeartbeat = GetTickCount64();
            h->processes[i].alive = 1;
            break;
        }
    }

    // Clean up dead processes (no heartbeat in 10 seconds)
    uint64_t now = GetTickCount64();
    uint32_t aliveCount = 0;
    for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
        if (h->processes[i].alive) {
            if (h->processes[i].processId != m_processId &&
                (now - h->processes[i].lastHeartbeat) > 10000) {
                // Process appears dead
                // Double-check by opening the process handle
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                          h->processes[i].processId);
                if (!hProc) {
                    h->processes[i].alive = 0;
                    h->processes[i].processId = 0;
                } else {
                    DWORD exitCode = 0;
                    GetExitCodeProcess(hProc, &exitCode);
                    if (exitCode != STILL_ACTIVE) {
                        h->processes[i].alive = 0;
                        h->processes[i].processId = 0;
                    }
                    CloseHandle(hProc);
                }
            }
            if (h->processes[i].alive) aliveCount++;
        }
    }
    h->processCount = aliveCount;

    endWrite();
    ReleaseMutex(m_hMutex);
}

size_t RawrXDStateMmf::getProcesses(MmfProcessEntry* outProcs, size_t maxProcs) const {
    if (!m_initialized || !outProcs) return 0;

    for (int attempt = 0; attempt < 50; ++attempt) {
        uint64_t seq;
        if (!beginRead(&seq)) {
            m_stats.seqlockRetries++;
            YieldProcessor();
            continue;
        }

        const SharedStateHeader* h = header();
        size_t count = 0;
        for (size_t i = 0; i < MMF_MAX_PROCESSES && count < maxProcs; ++i) {
            if (h->processes[i].alive) {
                outProcs[count++] = h->processes[i];
            }
        }

        if (endRead(seq)) {
            m_stats.readsPerformed++;
            return count;
        }
        m_stats.seqlockRetries++;
    }
    return 0;
}

bool RawrXDStateMmf::hasOtherProcesses() const {
    if (!m_initialized) return false;

    const SharedStateHeader* h = header();
    for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
        if (h->processes[i].alive && h->processes[i].processId != m_processId) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Full State Serialization (for reconnection reconciliation)
// ============================================================================

size_t RawrXDStateMmf::serializeFullStateToJson(char* outJson, size_t maxLen) const {
    if (!m_initialized || !outJson || maxLen == 0) return 0;

    // Acquire consistent snapshot under mutex
    WaitForSingleObject(m_hMutex, 1000);
    const SharedStateHeader* h = header();

    int written = 0;
    int total = 0;

    // Open JSON
    written = std::snprintf(outJson + total, maxLen - total, "{");
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    // Version info
    written = std::snprintf(outJson + total, maxLen - total,
        "\"magic\":%u,\"version\":%u,\"serverStartTime\":%llu,",
        h->magic, h->version, h->serverStartTime);
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    // Patch count
    written = std::snprintf(outJson + total, maxLen - total,
        "\"patchCount\":%u,\"patches\":[", h->patchCount);
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    for (uint32_t i = 0; i < h->patchCount && i < MMF_MAX_PATCHES; ++i) {
        const auto& p = h->patches[i];
        if (p.id == 0) continue;
        if (i > 0) {
            written = std::snprintf(outJson + total, maxLen - total, ",");
            if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
            total += written;
        }
        written = std::snprintf(outJson + total, maxLen - total,
            "{\"id\":%llu,\"layer\":%u,\"status\":%u,\"name\":\"%s\"}",
            p.id, p.layer, p.status, p.name);
        if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
        total += written;
    }

    written = std::snprintf(outJson + total, maxLen - total, "],");
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    // Config entries
    written = std::snprintf(outJson + total, maxLen - total,
        "\"configCount\":%u,\"configs\":{", h->configCount);
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    {
        bool first = true;
        for (size_t i = 0; i < MMF_MAX_CONFIG_ENTRIES; ++i) {
            if (h->configs[i].key[0] == '\0') continue;
            if (!first) {
                written = std::snprintf(outJson + total, maxLen - total, ",");
                if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
                total += written;
            }
            first = false;
            written = std::snprintf(outJson + total, maxLen - total,
                "\"%s\":\"%s\"", h->configs[i].key, h->configs[i].value);
            if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
            total += written;
        }
    }

    written = std::snprintf(outJson + total, maxLen - total, "},");
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    // Model state
    {
        const auto& m = h->modelState;
        written = std::snprintf(outJson + total, maxLen - total,
            "\"model\":{\"name\":\"%s\",\"loaded\":%s,\"inferring\":%s,"
            "\"vocabSize\":%u,\"embeddingDim\":%u,\"numLayers\":%u,"
            "\"tokensGenerated\":%llu,\"tokensPerSecond\":%.2f,"
            "\"gpuMemoryMB\":%.1f,\"cpuMemoryMB\":%.1f},",
            m.modelName,
            m.isLoaded ? "true" : "false",
            m.isInferring ? "true" : "false",
            m.vocabSize, m.embeddingDim, m.numLayers,
            m.tokensGenerated, m.tokensPerSecond,
            m.gpuMemoryUsedMB, m.cpuMemoryUsedMB);
        if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
        total += written;
    }

    // Memory stats
    {
        const auto& mem = h->memoryStats;
        written = std::snprintf(outJson + total, maxLen - total,
            "\"memory\":{\"totalPhysicalMB\":%.1f,\"availablePhysicalMB\":%.1f,"
            "\"processWorkingSetMB\":%.1f,\"gpuDedicatedMB\":%.1f,"
            "\"tensorMemoryMB\":%.1f,\"kvCacheMB\":%.1f,"
            "\"pressurePercent\":%.1f},",
            mem.totalPhysicalBytes / (1024.0 * 1024.0),
            mem.availablePhysicalBytes / (1024.0 * 1024.0),
            mem.processWorkingSetBytes / (1024.0 * 1024.0),
            mem.gpuDedicatedBytes / (1024.0 * 1024.0),
            mem.tensorMemoryBytes / (1024.0 * 1024.0),
            mem.kvCacheBytes / (1024.0 * 1024.0),
            mem.memoryPressurePercent);
        if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
        total += written;
    }

    // Process list
    written = std::snprintf(outJson + total, maxLen - total,
        "\"processCount\":%u,\"processes\":[", h->processCount);
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    {
        bool first = true;
        for (size_t i = 0; i < MMF_MAX_PROCESSES; ++i) {
            if (!h->processes[i].alive) continue;
            if (!first) {
                written = std::snprintf(outJson + total, maxLen - total, ",");
                if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
                total += written;
            }
            first = false;
            const char* typeStr = "unknown";
            switch (h->processes[i].processType) {
                case 0: typeStr = "Win32"; break;
                case 1: typeStr = "React"; break;
                case 2: typeStr = "CLI"; break;
                case 3: typeStr = "HTML"; break;
            }
            written = std::snprintf(outJson + total, maxLen - total,
                "{\"pid\":%u,\"type\":\"%s\",\"name\":\"%s\"}",
                h->processes[i].processId, typeStr, h->processes[i].processName);
            if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
            total += written;
        }
    }

    written = std::snprintf(outJson + total, maxLen - total, "],");
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

    // Global stats
    written = std::snprintf(outJson + total, maxLen - total,
        "\"totalPatchOps\":%llu,\"totalConfigUpdates\":%llu,\"eventHead\":%llu}",
        h->totalPatchOperations, h->totalConfigUpdates, h->eventHead);
    if (written < 0 || static_cast<size_t>(written) >= maxLen - total) goto done;
    total += written;

done:
    ReleaseMutex(m_hMutex);
    return static_cast<size_t>(total);
}

// ============================================================================
// Statistics
// ============================================================================

RawrXDStateMmf::Stats RawrXDStateMmf::getStats() const {
    return m_stats;
}

