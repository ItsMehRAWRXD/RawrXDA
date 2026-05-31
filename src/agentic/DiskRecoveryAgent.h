// =============================================================================
// DiskRecoveryAgent.h — C++ Wrapper for RawrXD_DiskRecoveryAgent.asm
// =============================================================================
// Provides a type-safe C++ interface to the MASM64 disk recovery kernel.
// The underlying ASM exports use C-linkage (no name mangling).
//
// The recovery agent targets WD My Book hardware encryption bridges
// (JMS567/NS1066) and provides:
//   1. Physical drive scanning for dying USB bridge controllers
//   2. AES-256 encryption key extraction from bridge EEPROM
//   3. SCSI hammer-read with exponential backoff + bad sector mapping
//   4. Sparse-file imaging with resume/checkpoint support
//
// All functions follow the PatchResult pattern (return codes, no exceptions).
// =============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <functional>

namespace RawrXD {
namespace Recovery {

// ---------------------------------------------------------------------------
// Bridge controller type — matches ASM BRIDGE_* enum
// Prefixed with "Asm" to avoid ODR collision with agent/DiskRecoveryAgent.h
// which defines a richer BridgeType in the same namespace.
// ---------------------------------------------------------------------------
enum class AsmBridgeType : int {
    Unknown = 0,
    JMS567  = 1,   // JMicron JMS567 USB-SATA bridge
    NS1066  = 2    // Norelsys NS1066 USB-SATA bridge
};

// ---------------------------------------------------------------------------
// Recovery statistics snapshot (thin ASM-backed version)
// ---------------------------------------------------------------------------
struct AsmRecoveryStats {
    uint64_t goodSectors;
    uint64_t badSectors;
    uint64_t currentLBA;
    uint64_t totalSectors;

    double ProgressPercent() const {
        if (totalSectors == 0) return 0.0;
        return (static_cast<double>(currentLBA) / static_cast<double>(totalSectors)) * 100.0;
    }
};

// ---------------------------------------------------------------------------
// Recovery result (PatchResult pattern, thin ASM-backed version)
// ---------------------------------------------------------------------------
struct AsmRecoveryResult {
    bool success;
    std::string detail;
    int errorCode;

    static AsmRecoveryResult ok(const std::string& msg = "Success") {
        return {true, msg, 0};
    }
    static AsmRecoveryResult error(const std::string& msg, int code = -1) {
        return {false, msg, code};
    }
};

// ---------------------------------------------------------------------------
// C-linkage exports from RawrXD_DiskRecoveryAgent.asm
// ---------------------------------------------------------------------------
extern "C" {
    // Scan PhysicalDrive0-15 for WD bridge with dying controller.
    // Returns: drive number (0-15) or -1 if not found.
    int DiskRecovery_FindDrive();

    // Initialize recovery context with handles and sparse target file.
    // Returns: context pointer or NULL on failure.
    void* DiskRecovery_Init(int driveNum);

    // Attempt AES-256 key extraction from bridge EEPROM.
    // Returns: 1 = key extracted, 0 = failed.
    int DiskRecovery_ExtractKey(void* ctx);

    // Run the full recovery worker loop (blocks until complete/abort).
    void DiskRecovery_Run(void* ctx);

    // Thread-safe abort signal — sets context->AbortSignal = 1.
    void DiskRecovery_Abort(void* ctx);

    // Close all handles (source, target, log, bad sector map).
    void DiskRecovery_Cleanup(void* ctx);

    // Fill caller-provided stat buffer.
    void DiskRecovery_GetStats(void* ctx,
                               uint64_t* outGood,
                               uint64_t* outBad,
                               uint64_t* outCurrent,
                               uint64_t* outTotal);
}

// ---------------------------------------------------------------------------
// DiskRecoveryAsmAgent — Thin ASM-backed wrapper (used by ToolRegistry.cpp)
// Renamed from DiskRecoveryAgent to avoid ODR collision with the full
// agent/DiskRecoveryAgent.h implementation used by DiskRecoveryToolHandler.
// ---------------------------------------------------------------------------
class DiskRecoveryAsmAgent {
public:
    using ProgressCallback = std::function<void(const AsmRecoveryStats& stats)>;

    DiskRecoveryAsmAgent() = default;
    ~DiskRecoveryAsmAgent();

    // Non-copyable, movable
    DiskRecoveryAsmAgent(const DiskRecoveryAsmAgent&) = delete;
    DiskRecoveryAsmAgent& operator=(const DiskRecoveryAsmAgent&) = delete;
    DiskRecoveryAsmAgent(DiskRecoveryAsmAgent&& other) noexcept;
    DiskRecoveryAsmAgent& operator=(DiskRecoveryAsmAgent&& other) noexcept;

    // --- Discovery ---
    // Scan for dying WD drives. Returns drive number or -1.
    int FindDrive();

    // --- Initialization ---
    // Open device handles and create output files.
    AsmRecoveryResult Initialize(int driveNum);

    // --- Key Extraction ---
    // Try to pull AES-256 key from bridge EEPROM (must call before bridge dies).
    AsmRecoveryResult ExtractEncryptionKey();

    // --- Recovery ---
    // Run blocking recovery loop. Call Abort() from another thread to stop.
    AsmRecoveryResult RunRecovery();

    // --- Abort ---
    // Thread-safe abort. Sets the abort flag in the ASM context.
    void Abort();

    // --- Stats ---
    AsmRecoveryStats GetStats() const;
    AsmBridgeType GetBridgeType() const { return m_bridgeType; }
    bool IsInitialized() const { return m_ctx != nullptr; }
    bool IsKeyExtracted() const { return m_keyExtracted; }

    // --- Callbacks ---
    void SetProgressCallback(ProgressCallback cb) { m_progressCb = std::move(cb); }

private:
    void Cleanup();

    void* m_ctx = nullptr;
    AsmBridgeType m_bridgeType = AsmBridgeType::Unknown;
    bool m_keyExtracted = false;
    ProgressCallback m_progressCb;
};

} // namespace Recovery
} // namespace RawrXD
