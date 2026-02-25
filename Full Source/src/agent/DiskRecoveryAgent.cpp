// =============================================================================
// DiskRecoveryAgent.cpp — Hardware-Level Disk Recovery Agent Implementation
// =============================================================================
// Enterprise-grade USB bridge recovery for dying WD My Book / similar devices.
// Bypasses Windows USBSTOR driver via direct SCSI pass-through.
//
// No exceptions. No std::function in hot path. PatchResult pattern.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "DiskRecoveryAgent.h"
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iomanip>

// Windows-only
#include <winioctl.h>

namespace RawrXD {
namespace Recovery {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int    MAX_PHYSICAL_DRIVES     = 16;
static constexpr DWORD  OPEN_FLAGS              = FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
static constexpr int    INQUIRY_TIMEOUT_MS      = 500;
static constexpr int    INQUIRY_BUFFER_SIZE     = 64;
static constexpr size_t SECTOR_BUFFER_SIZE      = 1048576;  // 1MB
static constexpr DWORD  SPARSE_FSCTL            = 0x000900C4; // FSCTL_SET_SPARSE

// ---------------------------------------------------------------------------
// Helper: build \\.\PhysicalDriveN path
// ---------------------------------------------------------------------------
static std::string BuildDrivePath(int driveNumber) {
    char buf[64];
    snprintf(buf, sizeof(buf), "\\\\.\\PhysicalDrive%d", driveNumber);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Helper: get monotonic timestamp in seconds
// ---------------------------------------------------------------------------
static double GetTimestampSec() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now.time_since_epoch()).count();
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
DiskRecoveryAgent::DiskRecoveryAgent() {
    memset(&m_driveInfo, 0, sizeof(m_driveInfo));
    memset(m_encryptionKey, 0, sizeof(m_encryptionKey));
    memset(m_transferBuffer, 0, sizeof(m_transferBuffer));
}

DiskRecoveryAgent::~DiskRecoveryAgent() {
    Abort();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Close any open handles
    if (m_hSource != INVALID_HANDLE_VALUE) CloseHandle(m_hSource);
    if (m_hTarget != INVALID_HANDLE_VALUE) CloseHandle(m_hTarget);
    if (m_hLog    != INVALID_HANDLE_VALUE) CloseHandle(m_hLog);
    if (m_hBadMap != INVALID_HANDLE_VALUE) CloseHandle(m_hBadMap);
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
void DiskRecoveryAgent::SetConfig(const RecoveryConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

// ---------------------------------------------------------------------------
// Drive Discovery
// ---------------------------------------------------------------------------
HANDLE DiskRecoveryAgent::OpenDriveHandle(int driveNumber) {
    std::string path = BuildDrivePath(driveNumber);

    HANDLE h = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        OPEN_FLAGS,
        nullptr
    );

    return h;
}

PatchResult DiskRecoveryAgent::IdentifyBridge(HANDLE hDevice, DriveInfo& info) {
    // INQUIRY to get vendor/product strings
    uint8_t inquiryBuf[INQUIRY_BUFFER_SIZE];
    memset(inquiryBuf, 0, sizeof(inquiryBuf));

    int rc = asm_scsi_inquiry_quick(hDevice, inquiryBuf, INQUIRY_BUFFER_SIZE, INQUIRY_TIMEOUT_MS);
    if (rc != 0) {
        // Inquiry failed — might be a timeout (dying drive candidate)
        if (rc == 0x79) { // ERROR_SEM_TIMEOUT
            info.bridgeType = BridgeType::Unknown;
            return PatchResult::ok("Inquiry timed out — possible dying bridge");
        }
        return PatchResult::error("SCSI INQUIRY failed", rc);
    }

    // Parse INQUIRY response:
    //   Byte 8-15:  Vendor ID (8 chars)
    //   Byte 16-31: Product ID (16 chars)
    //   Byte 32-51: Serial Number (20 chars) — not always in standard INQUIRY
    memcpy(info.vendorId,  &inquiryBuf[8],  8);
    info.vendorId[8] = '\0';

    memcpy(info.productId, &inquiryBuf[16], 16);
    info.productId[16] = '\0';

    // Detect bridge type from vendor strings
    if (strstr(info.vendorId, "WD") || strstr(info.productId, "My Book") ||
        strstr(info.productId, "Elements") || strstr(info.productId, "easystore")) {
        // Check for specific bridge controllers
        // JMS567 is the most common in 2018-2024 WD My Books
        info.bridgeType  = BridgeType::JMS567;
        info.isEncrypted = true;
    } else if (strstr(info.vendorId, "WDC")) {
        info.bridgeType  = BridgeType::NS1066;
        info.isEncrypted = true;
    } else {
        info.bridgeType  = BridgeType::Unknown;
        info.isEncrypted = false;
    }

    return PatchResult::ok("Bridge identified");
}

PatchResult DiskRecoveryAgent::ReadCapacity(HANDLE hDevice, DriveInfo& info) {
    uint64_t totalSectors = 0;
    uint32_t sectorSize   = 0;

    int rc = asm_scsi_read_capacity(hDevice, &totalSectors, &sectorSize);
    if (rc != 0) {
        return PatchResult::error("READ CAPACITY failed", rc);
    }

    info.totalSectors = totalSectors;
    info.sectorSize   = (sectorSize > 0) ? sectorSize : m_config.sectorSize;
    info.totalBytes   = info.totalSectors * info.sectorSize;

    return PatchResult::ok("Capacity read");
}

PatchResult DiskRecoveryAgent::ScanForDyingDrives(std::vector<DriveInfo>& outDrives) {
    m_state.store(RecoveryState::Scanning);
    outDrives.clear();

    LogMessage("[*] Scanning PhysicalDrive0-%d for dying USB bridges...", MAX_PHYSICAL_DRIVES - 1);

    for (int i = 0; i < MAX_PHYSICAL_DRIVES; ++i) {
        HANDLE hDev = OpenDriveHandle(i);
        if (hDev == INVALID_HANDLE_VALUE) continue;

        DriveInfo info{};
        info.driveNumber = i;

        PatchResult idResult = IdentifyBridge(hDev, info);
        if (idResult.success) {
            // Also try to read capacity (may timeout on dying drives)
            ReadCapacity(hDev, info);
            outDrives.push_back(info);

            LogMessage("[+] PhysicalDrive%d: %s %s (Bridge: %d, Sectors: %llu)",
                       i, info.vendorId, info.productId,
                       static_cast<int>(info.bridgeType), info.totalSectors);
        }

        CloseHandle(hDev);
    }

    m_state.store(RecoveryState::Idle);

    if (outDrives.empty()) {
        return PatchResult::error("No candidate drives found");
    }

    return PatchResult::ok("Scan complete");
}

PatchResult DiskRecoveryAgent::ProbeDrive(int driveNumber, DriveInfo& outInfo) {
    HANDLE hDev = OpenDriveHandle(driveNumber);
    if (hDev == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Cannot open drive", static_cast<int>(GetLastError()));
    }

    memset(&outInfo, 0, sizeof(outInfo));
    outInfo.driveNumber = driveNumber;

    PatchResult idResult = IdentifyBridge(hDev, outInfo);
    if (!idResult.success) {
        CloseHandle(hDev);
        return idResult;
    }

    PatchResult capResult = ReadCapacity(hDev, outInfo);
    CloseHandle(hDev);

    // Capacity failure is not fatal — drive may be dying
    if (!capResult.success) {
        LogMessage("[!] READ CAPACITY failed for drive %d — estimating 2TB", driveNumber);
        outInfo.totalSectors = 488281250;  // 2TB / 4096
        outInfo.sectorSize   = m_config.sectorSize;
        outInfo.totalBytes   = outInfo.totalSectors * outInfo.sectorSize;
    }

    return PatchResult::ok("Drive probed");
}

// ---------------------------------------------------------------------------
// Key Extraction
// ---------------------------------------------------------------------------
PatchResult DiskRecoveryAgent::ExtractEncryptionKey(int driveNumber, uint8_t* keyOut32) {
    HANDLE hDev = OpenDriveHandle(driveNumber);
    if (hDev == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Cannot open drive for key extraction", static_cast<int>(GetLastError()));
    }

    // Identify bridge type first
    DriveInfo info{};
    info.driveNumber = driveNumber;
    IdentifyBridge(hDev, info);

    if (info.bridgeType == BridgeType::Unknown) {
        CloseHandle(hDev);
        return PatchResult::error("Unknown bridge — cannot extract key");
    }

    LogMessage("[*] Attempting AES key extraction from %s bridge EEPROM...",
               info.bridgeType == BridgeType::JMS567 ? "JMS567" : "NS1066");

    int rc = asm_extract_bridge_key(hDev, keyOut32, static_cast<uint32_t>(info.bridgeType));
    CloseHandle(hDev);

    if (rc != 0) {
        return PatchResult::error("Key extraction failed — bridge may not support vendor commands", rc);
    }

    // Verify key isn't all zeros or all 0xFF
    bool allZero = true, allFF = true;
    for (int i = 0; i < 32; ++i) {
        if (keyOut32[i] != 0x00) allZero = false;
        if (keyOut32[i] != 0xFF) allFF = false;
    }

    if (allZero || allFF) {
        return PatchResult::error("Extracted key is blank — EEPROM may be corrupted");
    }

    LogMessage("[+] AES-256 key extracted successfully!");
    return PatchResult::ok("Key extracted");
}

// ---------------------------------------------------------------------------
// File Creation Helpers
// ---------------------------------------------------------------------------
HANDLE DiskRecoveryAgent::CreateImageFile(const std::string& path) {
    HANDLE h = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        nullptr
    );

    if (h != INVALID_HANDLE_VALUE && m_config.sparseImage) {
        // Mark file as sparse
        DWORD bytesReturned = 0;
        DeviceIoControl(h, SPARSE_FSCTL, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
    }

    return h;
}

HANDLE DiskRecoveryAgent::CreateLogFile(const std::string& path) {
    return CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
}

HANDLE DiskRecoveryAgent::CreateBadMapFile(const std::string& path) {
    return CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
}

// ---------------------------------------------------------------------------
// Checkpoint Save / Load
// ---------------------------------------------------------------------------
PatchResult DiskRecoveryAgent::SaveCheckpoint() {
    if (m_hLog == INVALID_HANDLE_VALUE) {
        return PatchResult::error("No log file open");
    }

    // Seek to beginning
    LARGE_INTEGER zero;
    zero.QuadPart = 0;
    SetFilePointerEx(m_hLog, zero, nullptr, FILE_BEGIN);

    // Write checkpoint data as structured text
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "RAWRXD_RECOVERY_CHECKPOINT\n"
        "drive=%d\n"
        "current_lba=%llu\n"
        "total_sectors=%llu\n"
        "good_sectors=%llu\n"
        "bad_sectors=%llu\n"
        "sector_size=%u\n"
        "timestamp=%.3f\n",
        m_driveInfo.driveNumber,
        m_stats.currentLBA.load(),
        m_stats.totalSectors.load(),
        m_stats.goodSectors.load(),
        m_stats.badSectors.load(),
        m_driveInfo.sectorSize,
        GetTimestampSec()
    );

    DWORD written = 0;
    WriteFile(m_hLog, buf, static_cast<DWORD>(len), &written, nullptr);

    // Truncate after checkpoint
    SetEndOfFile(m_hLog);

    return PatchResult::ok("Checkpoint saved");
}

PatchResult DiskRecoveryAgent::LoadCheckpoint(const std::string& path) {
    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Cannot open checkpoint file");
    }

    char buf[512];
    DWORD bytesRead = 0;
    ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
    CloseHandle(hFile);
    buf[bytesRead] = '\0';

    // Parse checkpoint
    uint64_t resumeLBA = 0;
    if (sscanf(buf, "RAWRXD_RECOVERY_CHECKPOINT\n"
               "drive=%*d\n"
               "current_lba=%llu",
               &resumeLBA) >= 1)
    {
        m_stats.currentLBA.store(resumeLBA);
        return PatchResult::ok("Checkpoint loaded");
    }

    return PatchResult::error("Invalid checkpoint format");
}

// ---------------------------------------------------------------------------
// Core Imaging Loop
// ---------------------------------------------------------------------------
PatchResult DiskRecoveryAgent::ImagingLoop(HANDLE hSource, HANDLE hTarget) {
    uint64_t currentLBA   = m_stats.currentLBA.load();
    uint64_t totalSectors = m_stats.totalSectors.load();
    uint32_t sectorSize   = m_driveInfo.sectorSize;
    auto     startTime    = std::chrono::steady_clock::now();

    while (currentLBA < totalSectors) {
        // Check abort
        if (m_abortRequested.load()) {
            m_state.store(RecoveryState::Aborted);
            SaveCheckpoint();
            return PatchResult::error("Recovery aborted by user");
        }

        // Check pause
        while (m_pauseRequested.load()) {
            m_state.store(RecoveryState::Paused);
            Sleep(100);
            if (m_abortRequested.load()) {
                m_state.store(RecoveryState::Aborted);
                SaveCheckpoint();
                return PatchResult::error("Recovery aborted during pause");
            }
        }

        m_state.store(RecoveryState::Imaging);

        // SCSI Hammer read via ASM kernel
        int rc = asm_scsi_hammer_read(
            hSource,
            currentLBA,
            m_transferBuffer,
            sectorSize,
            static_cast<uint32_t>(m_config.maxRetries),
            m_config.timeoutMs
        );

        if (rc == static_cast<int>(RecoveryCode::Ok)) {
            // Write good sector to image
            DWORD written = 0;
            WriteFile(hTarget, m_transferBuffer, sectorSize, &written, nullptr);

            m_stats.goodSectors.fetch_add(1);
            m_stats.bytesWritten.fetch_add(written);

            EmitEvent(RecoveryEvent::Type::SectorGood, currentLBA, "");

        } else {
            // Bad sector — skip in image (sparse hole), log to map
            if (m_config.sparseImage) {
                // Seek forward in target to leave sparse hole
                LARGE_INTEGER dist;
                dist.QuadPart = static_cast<LONGLONG>(sectorSize);
                SetFilePointerEx(hTarget, dist, nullptr, FILE_CURRENT);
            } else {
                // Write zeros for bad sector
                memset(m_transferBuffer, 0, sectorSize);
                DWORD written = 0;
                WriteFile(hTarget, m_transferBuffer, sectorSize, &written, nullptr);
            }

            // Record bad sector
            {
                std::lock_guard<std::mutex> lock(m_badSectorMutex);
                m_badSectors.push_back(currentLBA);
            }
            m_stats.badSectors.fetch_add(1);

            // Write to bad sector map file
            if (m_hBadMap != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(m_hBadMap, &currentLBA, sizeof(currentLBA), &written, nullptr);
            }

            EmitEvent(RecoveryEvent::Type::SectorBad, currentLBA,
                      "Code: " + std::to_string(rc));
        }

        ++currentLBA;
        m_stats.sectorsProcessed.fetch_add(1);
        m_stats.currentLBA.store(currentLBA);

        // Update percentage
        if (totalSectors > 0) {
            uint32_t pct = static_cast<uint32_t>((currentLBA * 100) / totalSectors);
            m_stats.percentComplete.store(pct);
        }

        // Checkpoint
        if ((currentLBA % m_config.checkpointInterval) == 0) {
            SaveCheckpoint();

            // Calculate throughput
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - startTime).count();
            m_stats.elapsedSeconds = elapsed;
            if (elapsed > 0) {
                double bytesTotal = static_cast<double>(m_stats.bytesWritten.load());
                m_stats.throughputMBps = (bytesTotal / (1024.0 * 1024.0)) / elapsed;
            }

            EmitEvent(RecoveryEvent::Type::Checkpoint, currentLBA,
                      "Progress: " + std::to_string(m_stats.percentComplete.load()) + "%");
        }
    }

    // Final stats
    auto endTime = std::chrono::steady_clock::now();
    m_stats.elapsedSeconds = std::chrono::duration<double>(endTime - startTime).count();
    SaveCheckpoint();

    return PatchResult::ok("Imaging complete");
}

// ---------------------------------------------------------------------------
// Recovery Operations
// ---------------------------------------------------------------------------
PatchResult DiskRecoveryAgent::StartRecovery(int driveNumber) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_state.store(RecoveryState::Initializing);
    m_abortRequested.store(false);
    m_pauseRequested.store(false);

    LogMessage("=== RawrXD Disk Recovery Agent v1.0 ===");
    LogMessage("Hardware-level extraction for dying USB bridges");

    // Probe drive
    PatchResult probeResult = ProbeDrive(driveNumber, m_driveInfo);
    if (!probeResult.success) {
        m_state.store(RecoveryState::Failed);
        return probeResult;
    }

    // Open source handle
    m_hSource = OpenDriveHandle(driveNumber);
    if (m_hSource == INVALID_HANDLE_VALUE) {
        m_state.store(RecoveryState::Failed);
        return PatchResult::error("Failed to open source drive", static_cast<int>(GetLastError()));
    }

    // Create output directory
    std::error_code ec;
    std::filesystem::create_directories(m_config.outputDir, ec);

    // Create output files
    std::string imageFile = m_config.outputDir + "\\" + m_config.imageFileName + "_Drive" +
                            std::to_string(driveNumber) + ".bin";
    std::string logFile   = m_config.outputDir + "\\" + m_config.imageFileName + "_Drive" +
                            std::to_string(driveNumber) + ".checkpoint";
    std::string badFile   = m_config.outputDir + "\\" + m_config.imageFileName + "_Drive" +
                            std::to_string(driveNumber) + ".badmap";

    m_hTarget = CreateImageFile(imageFile);
    m_hLog    = CreateLogFile(logFile);
    m_hBadMap = CreateBadMapFile(badFile);

    if (m_hTarget == INVALID_HANDLE_VALUE) {
        m_state.store(RecoveryState::Failed);
        CloseHandle(m_hSource);
        m_hSource = INVALID_HANDLE_VALUE;
        return PatchResult::error("Failed to create image file");
    }

    // Set up stats (using atomic member resets since std::atomic is non-copyable)
    m_stats.sectorsProcessed.store(0);
    m_stats.goodSectors.store(0);
    m_stats.badSectors.store(0);
    m_stats.retriesTotal.store(0);
    m_stats.bytesWritten.store(0);
    m_stats.currentLBA.store(0);
    m_stats.totalSectors.store(m_driveInfo.totalSectors);
    m_stats.percentComplete.store(0);
    m_stats.elapsedSeconds = 0.0;
    m_stats.throughputMBps = 0.0;
    m_badSectors.clear();

    // Attempt key extraction first (before bridge dies)
    if (m_config.extractKey && m_driveInfo.isEncrypted) {
        m_state.store(RecoveryState::ExtractingKey);
        LogMessage("[*] Attempting hardware key extraction...");

        PatchResult keyResult = ExtractEncryptionKey(driveNumber, m_encryptionKey);
        if (keyResult.success) {
            m_keyExtracted = true;
            EmitEvent(RecoveryEvent::Type::KeyExtracted, 0, "AES-256 key extracted from bridge EEPROM");

            // Save key to separate file
            std::string keyFile = m_config.outputDir + "\\" + m_config.imageFileName + "_Drive" +
                                  std::to_string(driveNumber) + ".aeskey";
            HANDLE hKey = CreateFileA(keyFile.c_str(), GENERIC_WRITE, 0, nullptr,
                                      CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hKey != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                WriteFile(hKey, m_encryptionKey, 32, &written, nullptr);
                CloseHandle(hKey);
                LogMessage("[+] Key saved to %s", keyFile.c_str());
            }
        } else {
            LogMessage("[!] Key extraction failed: %s", keyResult.detail);
        }
    }

    // Start imaging
    LogMessage("[*] Starting SCSI hammer protocol on PhysicalDrive%d...", driveNumber);
    LogMessage("[*] Total sectors: %llu, Sector size: %u, Image: %s",
               m_driveInfo.totalSectors, m_driveInfo.sectorSize, imageFile.c_str());

    m_state.store(RecoveryState::Imaging);
    PatchResult imgResult = ImagingLoop(m_hSource, m_hTarget);

    // Cleanup handles
    CloseHandle(m_hSource);  m_hSource = INVALID_HANDLE_VALUE;
    CloseHandle(m_hTarget);  m_hTarget = INVALID_HANDLE_VALUE;
    CloseHandle(m_hLog);     m_hLog    = INVALID_HANDLE_VALUE;
    CloseHandle(m_hBadMap);  m_hBadMap = INVALID_HANDLE_VALUE;

    // Final state
    if (imgResult.success) {
        m_state.store(RecoveryState::Completed);
    }

    LogMessage("\n=== Recovery Statistics ===");
    LogMessage("Total Processed: %llu", m_stats.sectorsProcessed.load());
    LogMessage("Good Sectors:    %llu", m_stats.goodSectors.load());
    LogMessage("Bad Sectors:     %llu", m_stats.badSectors.load());
    LogMessage("Completion:      %u%%",  m_stats.percentComplete.load());
    LogMessage("Elapsed:         %.1f seconds", m_stats.elapsedSeconds);
    LogMessage("Throughput:      %.2f MB/s", m_stats.throughputMBps);

    EmitEvent(RecoveryEvent::Type::Complete, m_stats.currentLBA.load(),
              "Recovery session complete");

    return imgResult;
}

void DiskRecoveryAgent::StartRecoveryAsync(
    int driveNumber,
    RecoveryEventCallback onEvent,
    std::function<void(PatchResult)> onComplete)
{
    m_eventCallback = onEvent;

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_workerThread = std::thread([this, driveNumber, onComplete]() {
        PatchResult result = StartRecovery(driveNumber);
        if (onComplete) {
            onComplete(result);
        }
    });
}

PatchResult DiskRecoveryAgent::ResumeRecovery(const std::string& checkpointFile) {
    PatchResult loadResult = LoadCheckpoint(checkpointFile);
    if (!loadResult.success) {
        return loadResult;
    }

    int driveNum = m_driveInfo.driveNumber;
    if (driveNum < 0) {
        return PatchResult::error("Checkpoint does not contain valid drive number");
    }

    LogMessage("[*] Resuming from LBA %llu...", m_stats.currentLBA.load());
    return StartRecovery(driveNum);
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------
void DiskRecoveryAgent::Pause() {
    m_pauseRequested.store(true);
}

void DiskRecoveryAgent::Resume() {
    m_pauseRequested.store(false);
}

void DiskRecoveryAgent::Abort() {
    m_abortRequested.store(true);
}

// ---------------------------------------------------------------------------
// Bad Sector Map
// ---------------------------------------------------------------------------
std::vector<uint64_t> DiskRecoveryAgent::GetBadSectorList() const {
    std::lock_guard<std::mutex> lock(m_badSectorMutex);
    return m_badSectors;
}

PatchResult DiskRecoveryAgent::ExportBadSectorMap(const std::string& path) const {
    std::lock_guard<std::mutex> lock(m_badSectorMutex);

    HANDLE hFile = CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return PatchResult::error("Failed to create bad sector map file");
    }

    // Write header
    const char* header = "# RawrXD Bad Sector Map\n# LBA (one per line)\n";
    DWORD written = 0;
    WriteFile(hFile, header, static_cast<DWORD>(strlen(header)), &written, nullptr);

    // Write each bad LBA
    for (uint64_t lba : m_badSectors) {
        char line[32];
        int len = snprintf(line, sizeof(line), "%llu\n", lba);
        WriteFile(hFile, line, static_cast<DWORD>(len), &written, nullptr);
    }

    CloseHandle(hFile);
    return PatchResult::ok("Bad sector map exported");
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------
void DiskRecoveryAgent::LogMessage(const char* fmt, ...) {
    if (!m_config.logToConsole) return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
    va_end(args);

    if (len > 0) {
        buf[len]     = '\n';
        buf[len + 1] = '\0';

        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hStdout, buf, static_cast<DWORD>(len + 1), &written, nullptr);
        }
    }
}

void DiskRecoveryAgent::EmitEvent(RecoveryEvent::Type type, uint64_t lba, const std::string& msg) {
    if (m_eventCallback) {
        RecoveryEvent evt;
        evt.type      = type;
        evt.lba       = lba;
        evt.message   = msg;
        evt.timestamp = GetTimestampSec();
        m_eventCallback(evt);
    }
}

} // namespace Recovery
} // namespace RawrXD
