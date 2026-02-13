// =============================================================================
// DiskRecoveryAgent.h — Hardware-Level Disk Recovery Agent
// =============================================================================
// Enterprise-grade USB bridge recovery for dying WD My Book / similar devices.
// Bypasses Windows USBSTOR driver via direct SCSI pass-through (IOCTL).
//
// Features:
//   - SCSI Hammer Protocol: Retry-loop sector reads with exponential backoff
//   - Hardware Key Extraction: Vendor commands to dump AES keys from EEPROM
//   - Resume Capability: Checkpoint every N sectors (survives crashes)
//   - Bad Sector Mapping: Binary map for professional recovery services
//   - Zero Windows Caching: NO_BUFFERING + WRITE_THROUGH flags prevent hangs
//   - Sparse Image Output: Bad sectors create sparse holes (saves space)
//   - Bridge Detection: Auto-identify JMS567 / NS1066 USB-SATA controllers
//
// No exceptions. No std::function in hot path. No Qt dependency.
// Follows PatchResult pattern per copilot-instructions.md.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <thread>

// ---------------------------------------------------------------------------
// Forward declarations for PatchResult (from core layer)
// ---------------------------------------------------------------------------
#ifndef RAWRXD_PATCHRESULT_DEFINED
#define RAWRXD_PATCHRESULT_DEFINED
struct PatchResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static PatchResult ok(const char* msg = "Success") {
        PatchResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static PatchResult error(const char* msg, int code = -1) {
        PatchResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};
#endif

namespace RawrXD {
namespace Recovery {

// ---------------------------------------------------------------------------
// Recovery return codes (match ASM constants)
// ---------------------------------------------------------------------------
enum class RecoveryCode : int {
    Ok              = 0,
    Timeout         = 1,
    MediumError     = 2,
    HardwareError   = 3,
    BridgeDead      = 4,
    IllegalRequest  = 5
};

// ---------------------------------------------------------------------------
// USB Bridge types
// ---------------------------------------------------------------------------
enum class BridgeType : uint32_t {
    Unknown     = 0,
    JMS567      = 1,    // JMicron JMS567 USB 3.0 to SATA bridge
    NS1066      = 2,    // Norelsys NS1066 USB 3.0 to SATA bridge
    ASM1153E    = 3,    // ASMedia ASM1153E (future)
    VL716       = 4     // VIA VL716 (future)
};

// ---------------------------------------------------------------------------
// Recovery session state
// ---------------------------------------------------------------------------
enum class RecoveryState : uint8_t {
    Idle            = 0,
    Scanning        = 1,
    Initializing    = 2,
    ExtractingKey   = 3,
    Imaging         = 4,
    Paused          = 5,
    Completed       = 6,
    Failed          = 7,
    Aborted         = 8
};

// ---------------------------------------------------------------------------
// DriveInfo — discovered drive metadata
// ---------------------------------------------------------------------------
struct DriveInfo {
    int             driveNumber;        // PhysicalDrive index
    uint64_t        totalSectors;       // Sector count from READ CAPACITY
    uint32_t        sectorSize;         // Typically 512 or 4096
    uint64_t        totalBytes;         // totalSectors * sectorSize
    BridgeType      bridgeType;         // Detected bridge controller
    char            vendorId[9];        // From INQUIRY (8 chars + null)
    char            productId[17];      // From INQUIRY (16 chars + null)
    char            serialNumber[21];   // From INQUIRY (20 chars + null)
    bool            isEncrypted;        // Hardware encryption detected
};

// ---------------------------------------------------------------------------
// RecoveryConfig — tunable parameters
// ---------------------------------------------------------------------------
struct RecoveryConfig {
    int             maxRetries          = 100;      // Per-sector retry limit
    uint32_t        timeoutMs           = 2000;     // Per-attempt SCSI timeout
    uint32_t        sectorSize          = 4096;     // Default sector size
    uint32_t        checkpointInterval  = 1000;     // Sectors between checkpoints
    uint32_t        backoffBaseMs       = 10;       // Base backoff sleep
    uint32_t        backoffMaxMs        = 1000;     // Max backoff cap
    bool            extractKey          = true;     // Attempt AES key extraction
    bool            sparseImage         = true;     // Sparse file for bad sectors
    bool            logToConsole        = true;     // Live console output
    std::string     outputDir           = "D:\\Recovery";
    std::string     imageFileName       = "WD_Rescue";
    int             targetDriveNumber   = -1;       // -1 = auto-detect
};

// ---------------------------------------------------------------------------
// RecoveryStats — live statistics
// ---------------------------------------------------------------------------
struct RecoveryStats {
    std::atomic<uint64_t> sectorsProcessed{0};
    std::atomic<uint64_t> goodSectors{0};
    std::atomic<uint64_t> badSectors{0};
    std::atomic<uint64_t> retriesTotal{0};
    std::atomic<uint64_t> bytesWritten{0};
    std::atomic<uint64_t> currentLBA{0};
    std::atomic<uint64_t> totalSectors{0};
    std::atomic<uint32_t> percentComplete{0};
    double                elapsedSeconds  = 0.0;
    double                throughputMBps  = 0.0;
};

// ---------------------------------------------------------------------------
// RecoveryEvent — callback event for UI/logging
// ---------------------------------------------------------------------------
struct RecoveryEvent {
    enum class Type : uint8_t {
        Info,
        Progress,
        SectorGood,
        SectorBad,
        KeyExtracted,
        Checkpoint,
        Warning,
        Error,
        Complete
    };

    Type        type;
    uint64_t    lba;
    std::string message;
    double      timestamp;
};

using RecoveryEventCallback = std::function<void(const RecoveryEvent&)>;

// ---------------------------------------------------------------------------
// External ASM entry points (disk_recovery_scsi.asm)
// ---------------------------------------------------------------------------
extern "C" {
    // Retry-loop SCSI READ with exponential backoff
    // Returns: 0=success, error code on failure. RDX=bytes or sense key.
    int asm_scsi_hammer_read(
        HANDLE hDevice,
        uint64_t lba,
        void* buffer,
        uint32_t sectorSize,
        uint32_t maxRetries,
        uint32_t timeoutMs
    );

    // Fast SCSI INQUIRY with custom timeout
    // Returns: 0=success, GetLastError on failure
    int asm_scsi_inquiry_quick(
        HANDLE hDevice,
        void* buffer,
        uint32_t bufferSize,
        uint32_t timeoutMs
    );

    // Read drive capacity (total sectors + sector size)
    // Returns: 0=success, error code on failure
    int asm_scsi_read_capacity(
        HANDLE hDevice,
        uint64_t* totalSectors,
        uint32_t* sectorSize
    );

    // Issue REQUEST SENSE for detailed SCSI error info
    // Returns: 0=success, error code on failure
    int asm_scsi_request_sense(
        HANDLE hDevice,
        void* senseBuffer,
        uint32_t bufferSize
    );

    // Extract AES key from bridge EEPROM (vendor-specific)
    // Returns: 0=success (key in buffer), -1=failure
    int asm_extract_bridge_key(
        HANDLE hDevice,
        void* keyBuffer,        // 32 bytes output
        uint32_t bridgeType     // 1=JMS567, 2=NS1066
    );
}

// ---------------------------------------------------------------------------
// DiskRecoveryAgent — Main recovery controller
// ---------------------------------------------------------------------------
class DiskRecoveryAgent {
public:
    DiskRecoveryAgent();
    ~DiskRecoveryAgent();

    // -- Configuration --
    void SetConfig(const RecoveryConfig& config);
    const RecoveryConfig& GetConfig() const { return m_config; }

    // -- Drive Discovery --
    // Scan PhysicalDrive0-15 for dying WD-signature devices
    PatchResult ScanForDyingDrives(std::vector<DriveInfo>& outDrives);

    // Probe a specific drive number
    PatchResult ProbeDrive(int driveNumber, DriveInfo& outInfo);

    // -- Key Extraction --
    // Attempt to extract AES-256 key from bridge EEPROM before it dies
    PatchResult ExtractEncryptionKey(int driveNumber, uint8_t* keyOut32);

    // -- Recovery Operations --
    // Start full imaging session (blocking)
    PatchResult StartRecovery(int driveNumber);

    // Start recovery in background thread
    void StartRecoveryAsync(int driveNumber,
                            RecoveryEventCallback onEvent,
                            std::function<void(PatchResult)> onComplete);

    // Resume from checkpoint
    PatchResult ResumeRecovery(const std::string& checkpointFile);

    // -- Control --
    void Pause();
    void Resume();
    void Abort();

    // -- Status --
    RecoveryState GetState() const { return m_state.load(); }
    const RecoveryStats& GetStats() const { return m_stats; }
    bool IsRunning() const { return m_state.load() == RecoveryState::Imaging; }
    const DriveInfo& GetDriveInfo() const { return m_driveInfo; }

    // -- Bad Sector Map --
    std::vector<uint64_t> GetBadSectorList() const;
    PatchResult ExportBadSectorMap(const std::string& path) const;

private:
    // Internal helpers
    HANDLE OpenDriveHandle(int driveNumber);
    PatchResult IdentifyBridge(HANDLE hDevice, DriveInfo& info);
    PatchResult ReadCapacity(HANDLE hDevice, DriveInfo& info);

    // Core imaging loop
    PatchResult ImagingLoop(HANDLE hSource, HANDLE hTarget);

    // Checkpoint save/load
    PatchResult SaveCheckpoint();
    PatchResult LoadCheckpoint(const std::string& path);

    // File creation helpers
    HANDLE CreateImageFile(const std::string& path);
    HANDLE CreateLogFile(const std::string& path);
    HANDLE CreateBadMapFile(const std::string& path);

    // Console output
    void LogMessage(const char* fmt, ...);
    void EmitEvent(RecoveryEvent::Type type, uint64_t lba, const std::string& msg);

    // Configuration
    RecoveryConfig              m_config;

    // State
    std::atomic<RecoveryState>  m_state{RecoveryState::Idle};
    std::atomic<bool>           m_abortRequested{false};
    std::atomic<bool>           m_pauseRequested{false};

    // Handles
    HANDLE                      m_hSource       = INVALID_HANDLE_VALUE;
    HANDLE                      m_hTarget       = INVALID_HANDLE_VALUE;
    HANDLE                      m_hLog          = INVALID_HANDLE_VALUE;
    HANDLE                      m_hBadMap       = INVALID_HANDLE_VALUE;

    // Drive info
    DriveInfo                   m_driveInfo{};
    uint8_t                     m_encryptionKey[32]{};
    bool                        m_keyExtracted  = false;

    // Stats
    RecoveryStats               m_stats;

    // Bad sector tracking
    mutable std::mutex          m_badSectorMutex;
    std::vector<uint64_t>       m_badSectors;

    // Callback
    RecoveryEventCallback       m_eventCallback;

    // Threading
    std::thread                 m_workerThread;
    mutable std::mutex          m_mutex;

    // Transfer buffer (4KB-aligned for DMA)
    alignas(4096) uint8_t       m_transferBuffer[1048576]; // 1MB
};

} // namespace Recovery
} // namespace RawrXD
