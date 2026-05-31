// =============================================================================
// DiskRecoveryAgent.h — C++ Wrapper for RawrXD_DiskRecoveryAgent.asm
// =============================================================================
// Provides a safe, RAII-wrapped interface to the pure MASM disk recovery
// kernel. The ASM module handles raw SCSI passthrough, hardware key
// extraction, and sector-level imaging. This wrapper adds:
//   1. Structured PatchResult returns (no exceptions)
//   2. Async execution with abort capability
//   3. Observability hooks (logging, metrics, tracing)
//   4. Thread-safe stat polling for UI integration
//
// The ASM kernel exports C-callable functions (DiskRecovery_*).
// This header declares extern "C" bindings + the C++ DiskRecoveryAgent class.
//
// Pattern: PatchResult (success/error factory), no exceptions, no Qt.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>

namespace RawrXD {
namespace Recovery {

// ---------------------------------------------------------------------------
// PatchResult — Structured result (matches project-wide pattern)
// ---------------------------------------------------------------------------
struct RecoveryResult {
    bool success;
    const char* detail;
    int errorCode;

    static RecoveryResult ok(const char* msg = "OK") {
        return {true, msg, 0};
    }
    static RecoveryResult error(const char* msg, int code = -1) {
        return {false, msg, code};
    }
};

// ---------------------------------------------------------------------------
// Recovery statistics (polled from UI thread)
// ---------------------------------------------------------------------------
struct RecoveryStats {
    uint64_t goodSectors;
    uint64_t badSectors;
    uint64_t currentLBA;
    uint64_t totalSectors;

    double progressPercent() const {
        if (totalSectors == 0) return 0.0;
        return (static_cast<double>(currentLBA) / static_cast<double>(totalSectors)) * 100.0;
    }
};

// ---------------------------------------------------------------------------
// Bridge type enum (mirrors ASM BRIDGE_* constants)
// ---------------------------------------------------------------------------
enum class BridgeType : uint32_t {
    Unknown  = 0,
    JMS567   = 1,
    NS1066   = 2
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
using ProgressCallback = std::function<void(const RecoveryStats& stats)>;
using CompleteCallback = std::function<void(RecoveryResult result, const RecoveryStats& finalStats)>;

// ---------------------------------------------------------------------------
// ASM kernel extern "C" declarations
// ---------------------------------------------------------------------------
#ifdef RAWR_HAS_MASM
extern "C" {
    int   DiskRecovery_FindDrive();
    void* DiskRecovery_Init(int driveNum);
    int   DiskRecovery_ExtractKey(void* ctx);
    void  DiskRecovery_Run(void* ctx);
    void  DiskRecovery_Abort(void* ctx);
    void  DiskRecovery_Cleanup(void* ctx);
    void  DiskRecovery_GetStats(void* ctx,
                                uint64_t* outGood,
                                uint64_t* outBad,
                                uint64_t* outCurrent,
                                uint64_t* outTotal);
}
#endif

// ---------------------------------------------------------------------------
// DiskRecoveryAgent — RAII wrapper around ASM kernel
// ---------------------------------------------------------------------------
class DiskRecoveryAgent {
public:
    DiskRecoveryAgent();
    ~DiskRecoveryAgent();

    // No copy, move only
    DiskRecoveryAgent(const DiskRecoveryAgent&) = delete;
    DiskRecoveryAgent& operator=(const DiskRecoveryAgent&) = delete;

    // -- Discovery --
    // Scan for dying WD My Book devices on PhysicalDrive0-15.
    // Returns drive number (0-15) or -1 if not found.
    int FindDyingDrive();

    // -- Initialization --
    // Open device, create output files in D:\Recovery\
    RecoveryResult Initialize(int driveNumber);

    // -- Key Extraction --
    // Attempt to dump AES-256 key from bridge EEPROM before it dies.
    RecoveryResult ExtractEncryptionKey();

    // -- Recovery --
    // Run the SCSI hammer recovery loop (blocking).
    RecoveryResult RunRecovery();

    // Run recovery in background thread with callbacks.
    void RunRecoveryAsync(ProgressCallback onProgress = nullptr,
                          CompleteCallback onComplete = nullptr);

    // -- Control --
    // Thread-safe abort signal.
    void Abort();

    // -- Status --
    bool IsRunning() const { return m_running.load(); }
    bool IsInitialized() const { return m_context != nullptr; }
    RecoveryStats GetStats() const;
    BridgeType GetBridgeType() const { return m_bridgeType; }
    bool HasEncryptionKey() const { return m_keyExtracted; }

private:
    void Cleanup();

    void* m_context = nullptr;
    int m_driveNumber = -1;
    BridgeType m_bridgeType = BridgeType::Unknown;
    bool m_keyExtracted = false;

    std::atomic<bool> m_running{false};
    std::thread m_asyncThread;
    mutable std::mutex m_mutex;
};

} // namespace Recovery
} // namespace RawrXD
