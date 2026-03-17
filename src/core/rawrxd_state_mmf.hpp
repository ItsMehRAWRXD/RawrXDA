// ============================================================================
// rawrxd_state_mmf.hpp — Cross-Process State Synchronization via Memory-Mapped Files
// ============================================================================
//
// Phase 36: MMF State Sync — Inter-Process Consensus for 4-IDE Architecture
//
// Purpose:
//   Provides zero-latency cross-process state broadcast for patch/config/model
//   state across all RawrXD frontends: Win32, React, CLI, HTML.
//   Uses Win32 Memory-Mapped Files (MMF) with named shared memory and named
//   events for change notification.
//
// Architecture:
//   ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
//   │ Win32IDE │   │ React WS │   │ CLI Agent│   │ HTML Lite│
//   └────┬─────┘   └────┬─────┘   └────┬─────┘   └────┬─────┘
//        │              │              │              │
//        └──────────────┴──────┬───────┴──────────────┘
//                              │
//                    ┌─────────▼─────────┐
//                    │  RawrXDStateMmf   │
//                    │  (Named MMF)      │
//                    │  Global\\RawrXD_  │
//                    │  SharedState_v1   │
//                    └───────────────────┘
//
// Design Principles:
//   - PatchResult-style structured results (no exceptions)
//   - Lock-free reader path via sequence counter (seqlock)
//   - Named mutex for writer serialization
//   - Named event for change notification (poll-free)
//   - Fixed-size shared region (no dynamic alloc in shared memory)
//   - Compatible with UnifiedHotpatchManager event system
//   - No circular includes
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <atomic>
#include <mutex>
#include "patch_result.hpp"

// Forward declarations
struct HotpatchEvent;

// ============================================================================
// Shared Memory Layout — Fixed-size region visible to all processes
// ============================================================================

// Maximum counts for shared arrays
static constexpr size_t MMF_MAX_PATCHES         = 256;
static constexpr size_t MMF_MAX_CONFIG_ENTRIES   = 128;
static constexpr size_t MMF_MAX_STRING_LEN       = 256;
static constexpr size_t MMF_MAX_MODEL_NAME_LEN   = 512;
static constexpr size_t MMF_MAX_EVENTS           = 64;

// Patch state entry (shared across processes)
struct MmfPatchEntry {
    uint64_t    id;
    uint8_t     layer;              // 0=memory, 1=byte, 2=server
    uint8_t     status;             // 0=pending, 1=applied, 2=reverted, 3=failed
    uint8_t     reserved[6];
    uint64_t    timestamp;          // GetTickCount64() when applied
    char        name[MMF_MAX_STRING_LEN];
    char        detail[MMF_MAX_STRING_LEN];
    uint64_t    targetAddress;      // For memory patches
    uint64_t    patchSize;
};

// Configuration entry (shared across processes)
struct MmfConfigEntry {
    char        key[MMF_MAX_STRING_LEN];
    char        value[MMF_MAX_STRING_LEN];
    uint64_t    lastModified;       // GetTickCount64()
    uint32_t    modifierProcessId;  // PID that last wrote this
    uint32_t    reserved;
};

// Model state (shared across processes)
struct MmfModelState {
    char        modelPath[MMF_MAX_MODEL_NAME_LEN];
    char        modelName[MMF_MAX_STRING_LEN];
    uint64_t    modelSizeBytes;
    uint32_t    vocabSize;
    uint32_t    embeddingDim;
    uint32_t    numLayers;
    uint32_t    numHeads;
    uint32_t    contextSize;
    uint8_t     quantType;          // Maps to TensorType enum
    uint8_t     isLoaded;
    uint8_t     isInferring;
    uint8_t     reserved[5];
    uint64_t    tokensGenerated;
    float       tokensPerSecond;
    float       gpuMemoryUsedMB;
    float       cpuMemoryUsedMB;
    uint64_t    lastInferenceTimestamp;
};

// Memory statistics (shared across processes)
struct MmfMemoryStats {
    uint64_t    totalPhysicalBytes;
    uint64_t    availablePhysicalBytes;
    uint64_t    processWorkingSetBytes;
    uint64_t    gpuDedicatedBytes;
    uint64_t    gpuSharedBytes;
    uint64_t    tensorMemoryBytes;
    uint64_t    kvCacheBytes;
    float       memoryPressurePercent;  // 0.0-100.0
    uint64_t    lastUpdateTimestamp;
};

// Event broadcast entry (ring buffer in shared memory)
struct MmfEvent {
    uint64_t    sequenceId;
    uint64_t    timestamp;
    uint32_t    sourceProcessId;
    uint8_t     eventType;          // Maps to HotpatchEvent::Type
    uint8_t     reserved[3];
    char        detail[MMF_MAX_STRING_LEN];
};

// Process registration (each IDE instance registers itself)
static constexpr size_t MMF_MAX_PROCESSES = 16;

struct MmfProcessEntry {
    uint32_t    processId;
    uint8_t     processType;        // 0=Win32, 1=React, 2=CLI, 3=HTML, 4=other
    uint8_t     alive;              // Heartbeat flag (set by owner, cleared by watchdog)
    uint8_t     reserved[2];
    uint64_t    lastHeartbeat;      // GetTickCount64()
    char        processName[64];
};

// ============================================================================
// SharedStateHeader — Master header at offset 0 of the MMF
// ============================================================================
struct alignas(64) SharedStateHeader {
    // Magic number for validation
    uint32_t    magic;              // 'RXDS' = 0x53445852
    uint32_t    version;            // Protocol version (1)
    uint32_t    headerSize;         // sizeof(SharedStateHeader)
    uint32_t    totalSize;          // Total MMF region size

    // Seqlock for lock-free reads
    // Writers: increment before write, write, increment after write (always even after)
    // Readers: read seq, read data, read seq again — retry if seq changed or odd
    volatile uint64_t   writeSequence;

    // Counts
    uint32_t    patchCount;
    uint32_t    configCount;
    uint32_t    processCount;
    uint32_t    reserved;

    // Event ring buffer head (monotonic, & MMF_MAX_EVENTS-1 for index)
    volatile uint64_t   eventHead;

    // Global stats
    uint64_t    totalPatchOperations;
    uint64_t    totalConfigUpdates;
    uint64_t    serverStartTime;    // GetTickCount64() when first process created MMF

    // Embedded data sections (fixed offsets from header start)
    MmfPatchEntry       patches[MMF_MAX_PATCHES];
    MmfConfigEntry      configs[MMF_MAX_CONFIG_ENTRIES];
    MmfModelState       modelState;
    MmfMemoryStats      memoryStats;
    MmfEvent            events[MMF_MAX_EVENTS];
    MmfProcessEntry     processes[MMF_MAX_PROCESSES];
};

// Compile-time size check
static_assert(sizeof(SharedStateHeader) < 1024 * 1024, 
              "SharedStateHeader must fit within 1 MB MMF region");

// ============================================================================
// RawrXDStateMmf — Cross-Process State Manager
// ============================================================================

class RawrXDStateMmf {
public:
    // Singleton access (each process gets one instance)
    static RawrXDStateMmf& instance();

    // ---- Lifecycle ----
    // Create or open the shared memory region.
    // processType: 0=Win32, 1=React, 2=CLI, 3=HTML
    PatchResult initialize(uint8_t processType, const char* processName);
    PatchResult shutdown();
    bool isInitialized() const;

    // ---- Patch State ----
    PatchResult publishPatch(const MmfPatchEntry& entry);
    PatchResult updatePatchStatus(uint64_t patchId, uint8_t newStatus);
    PatchResult removePatch(uint64_t patchId);

    // Read all patches (snapshot — lock-free via seqlock)
    // Returns number of patches copied. outPatches must have room for MMF_MAX_PATCHES.
    size_t readPatches(MmfPatchEntry* outPatches, size_t maxPatches) const;
    size_t getPatchCount() const;

    // ---- Configuration State ----
    PatchResult publishConfig(const char* key, const char* value);
    PatchResult removeConfig(const char* key);

    // Read a single config value. Returns empty string if not found.
    bool readConfig(const char* key, char* outValue, size_t maxLen) const;
    size_t readAllConfigs(MmfConfigEntry* outConfigs, size_t maxConfigs) const;

    // ---- Model State ----
    PatchResult publishModelState(const MmfModelState& state);
    MmfModelState readModelState() const;

    // ---- Memory Stats ----
    PatchResult publishMemoryStats(const MmfMemoryStats& stats);
    MmfMemoryStats readMemoryStats() const;

    // ---- Event Broadcast ----
    // Publish an event visible to all processes
    PatchResult broadcastEvent(uint8_t eventType, const char* detail);

    // Poll events newer than lastSequence. Returns number of events read.
    // Caller provides lastSequence and updates it to the newest sequence seen.
    size_t pollEvents(MmfEvent* outEvents, size_t maxEvents, uint64_t* lastSequence) const;

    // ---- Process Registry ----
    // Heartbeat — call periodically (every ~1s) to signal liveness
    void heartbeat();

    // Get all registered processes
    size_t getProcesses(MmfProcessEntry* outProcs, size_t maxProcs) const;

    // Check if any other process is alive
    bool hasOtherProcesses() const;

    // ---- Full State Snapshot (for reconnection reconciliation) ----
    // Serialize entire shared state to JSON (caller provides buffer)
    size_t serializeFullStateToJson(char* outJson, size_t maxLen) const;

    // ---- Statistics ----
    struct Stats {
        uint64_t    readsPerformed;
        uint64_t    writesPerformed;
        uint64_t    seqlockRetries;     // Number of read retries due to concurrent write
        uint64_t    eventsPublished;
        uint64_t    eventsConsumed;
    };
    Stats getStats() const;

private:
    RawrXDStateMmf();
    ~RawrXDStateMmf();
    RawrXDStateMmf(const RawrXDStateMmf&) = delete;
    RawrXDStateMmf& operator=(const RawrXDStateMmf&) = delete;

    // ---- Internal Helpers ----

    // Seqlock primitives
    void beginWrite();
    void endWrite();
    bool beginRead(uint64_t* outSeq) const;
    bool endRead(uint64_t seq) const;

    // Access the shared header (requires m_mapView != nullptr)
    SharedStateHeader* header();
    const SharedStateHeader* header() const;

    // Named object helpers
    static constexpr const wchar_t* MMF_NAME        = L"Global\\RawrXD_SharedState_v1";
    static constexpr const wchar_t* MUTEX_NAME       = L"Global\\RawrXD_StateMutex_v1";
    static constexpr const wchar_t* EVENT_NAME       = L"Global\\RawrXD_StateChanged_v1";
    static constexpr DWORD         MMF_SIZE          = 2 * 1024 * 1024; // 2 MB

    // Handles
    HANDLE      m_hMapFile;         // CreateFileMapping handle
    HANDLE      m_hMutex;           // Named mutex for writer serialization
    HANDLE      m_hEvent;           // Named event for change notification
    void*       m_mapView;          // MapViewOfFile pointer

    // Local state
    bool        m_initialized;
    bool        m_isCreator;        // True if this process created the MMF
    uint32_t    m_processId;
    uint8_t     m_processType;
    char        m_processName[64];

    // Statistics (process-local)
    mutable Stats m_stats;
};
