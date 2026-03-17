// =============================================================================
// DiskRecoveryAgent.cpp — C++ Wrapper Implementation with Observability
// =============================================================================
// Wraps the MASM64 disk recovery kernel with RAII, structured logging,
// metrics, and distributed tracing via AgenticObservability.
// =============================================================================
#include "DiskRecoveryAgent.h"

#ifdef ERROR
#undef ERROR
#endif
#include "agentic_observability.h"

#include <chrono>

using RawrXD::Recovery::DiskRecoveryAsmAgent;
using RawrXD::Recovery::RecoveryResult;
using RawrXD::Recovery::RecoveryStats;
using RawrXD::Recovery::BridgeType;

static AgenticObservability& GetObs() {
    static AgenticObservability instance;
    return instance;
}

static const char* kComponent = "DiskRecoveryAgent";

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

DiskRecoveryAsmAgent::DiskRecoveryAsmAgent(DiskRecoveryAsmAgent&& other) noexcept
    : m_ctx(other.m_ctx)
    , m_bridgeType(other.m_bridgeType)
    , m_keyExtracted(other.m_keyExtracted)
    , m_progressCb(std::move(other.m_progressCb))
{
    other.m_ctx = nullptr;
}

DiskRecoveryAsmAgent& DiskRecoveryAsmAgent::operator=(DiskRecoveryAsmAgent&& other) noexcept {
    if (this != &other) {
        Cleanup();
        m_ctx = other.m_ctx;
        m_bridgeType = other.m_bridgeType;
        m_keyExtracted = other.m_keyExtracted;
        m_progressCb = std::move(other.m_progressCb);
        other.m_ctx = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Destructor / Cleanup
// ---------------------------------------------------------------------------

DiskRecoveryAsmAgent::~DiskRecoveryAsmAgent() {
    Cleanup();
}

void DiskRecoveryAsmAgent::Cleanup() {
    if (m_ctx) {
        GetObs().logInfo(kComponent, "Cleaning up recovery context");
        DiskRecovery_Cleanup(m_ctx);
        m_ctx = nullptr;
        m_keyExtracted = false;
        m_bridgeType = BridgeType::Unknown;
    }
}

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------

int DiskRecoveryAsmAgent::FindDrive() {
    auto traceId = GetObs().startTrace("DiskRecovery_FindDrive");
    auto spanId = GetObs().startSpan("scan_physical_drives", traceId);

    GetObs().logInfo(kComponent, "Scanning PhysicalDrive0-15 for dying WD bridge...");
    auto timing = GetObs().measureDuration("disk_recovery.find_drive_ms");

    int driveNum = DiskRecovery_FindDrive();

    GetObs().endSpan(spanId, driveNum < 0, driveNum < 0 ? "No drive found" : "");

    if (driveNum >= 0) {
        GetObs().logInfo(kComponent, "Found candidate drive", {
            {"driveNumber", driveNum}
        });
        GetObs().incrementCounter("disk_recovery.drives_found");
    } else {
        GetObs().logWarn(kComponent, "No dying WD device found on any PhysicalDrive");
        GetObs().incrementCounter("disk_recovery.scan_miss");
    }

    return driveNum;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

RecoveryResult DiskRecoveryAsmAgent::Initialize(int driveNum) {
    if (m_ctx) {
        return RecoveryResult::error("Already initialized — call Cleanup first");
    }

    auto spanId = GetObs().startSpan("DiskRecovery_Init");
    GetObs().logInfo(kComponent, "Initializing recovery context", {
        {"driveNumber", driveNum}
    });
    auto timing = GetObs().measureDuration("disk_recovery.init_ms");

    void* ctx = DiskRecovery_Init(driveNum);

    if (!ctx) {
        GetObs().endSpan(spanId, true, "Init failed: could not open device handles");
        GetObs().logError(kComponent, "DiskRecovery_Init returned NULL", {
            {"driveNumber", driveNum}
        });
        GetObs().incrementCounter("disk_recovery.init_failures");
        return RecoveryResult::error("Failed to initialize recovery context for drive " +
                                      std::to_string(driveNum));
    }

    m_ctx = ctx;

    // Read initial stats to determine bridge type
    RecoveryStats stats = GetStats();
    GetObs().endSpan(spanId);
    GetObs().logInfo(kComponent, "Recovery context created", {
        {"totalSectors", static_cast<long long>(stats.totalSectors)},
        {"resumeLBA", static_cast<long long>(stats.currentLBA)}
    });
    GetObs().setGauge("disk_recovery.total_sectors", static_cast<float>(stats.totalSectors));
    GetObs().incrementCounter("disk_recovery.init_successes");

    return RecoveryResult::ok("Initialized for PhysicalDrive" + std::to_string(driveNum));
}

// ---------------------------------------------------------------------------
// Key Extraction
// ---------------------------------------------------------------------------

RecoveryResult DiskRecoveryAsmAgent::ExtractEncryptionKey() {
    if (!m_ctx) {
        return RecoveryResult::error("Not initialized");
    }

    auto spanId = GetObs().startSpan("DiskRecovery_ExtractKey");
    GetObs().logInfo(kComponent, "Attempting AES-256 key extraction from bridge EEPROM...");
    auto timing = GetObs().measureDuration("disk_recovery.key_extract_ms");

    int result = DiskRecovery_ExtractKey(m_ctx);

    if (result) {
        m_keyExtracted = true;
        GetObs().endSpan(spanId);
        GetObs().logInfo(kComponent, "AES-256 key extracted and saved to aes_key.bin");
        GetObs().incrementCounter("disk_recovery.key_extractions");
        return RecoveryResult::ok("Key extracted successfully");
    } else {
        GetObs().endSpan(spanId, true, "Key extraction failed");
        GetObs().logWarn(kComponent, "Key extraction failed — bridge may be unresponsive");
        GetObs().incrementCounter("disk_recovery.key_extraction_failures");
        return RecoveryResult::error("Hardware key extraction failed (bridge unresponsive)");
    }
}

// ---------------------------------------------------------------------------
// Recovery Loop
// ---------------------------------------------------------------------------

RecoveryResult DiskRecoveryAsmAgent::RunRecovery() {
    if (!m_ctx) {
        return RecoveryResult::error("Not initialized");
    }

    auto traceId = GetObs().startTrace("DiskRecovery_Run");
    auto spanId = GetObs().startSpan("recovery_worker", traceId);
    GetObs().logInfo(kComponent, "Starting SCSI hammer recovery loop");
    GetObs().incrementCounter("disk_recovery.runs_started");

    auto startTime = std::chrono::high_resolution_clock::now();

    // The ASM worker blocks until completion or abort
    DiskRecovery_Run(m_ctx);

    auto endTime = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(endTime - startTime).count();

    RecoveryStats finalStats = GetStats();

    GetObs().endSpan(spanId, finalStats.badSectors > 0, "");
    GetObs().logInfo(kComponent, "Recovery loop complete", {
        {"goodSectors", static_cast<long long>(finalStats.goodSectors)},
        {"badSectors", static_cast<long long>(finalStats.badSectors)},
        {"currentLBA", static_cast<long long>(finalStats.currentLBA)},
        {"totalSectors", static_cast<long long>(finalStats.totalSectors)},
        {"elapsedSeconds", elapsed},
        {"progressPercent", finalStats.ProgressPercent()}
    });

    GetObs().recordMetric("disk_recovery.elapsed_seconds", static_cast<float>(elapsed));
    GetObs().setGauge("disk_recovery.good_sectors", static_cast<float>(finalStats.goodSectors));
    GetObs().setGauge("disk_recovery.bad_sectors", static_cast<float>(finalStats.badSectors));
    GetObs().recordHistogram("disk_recovery.duration_histogram", static_cast<float>(elapsed));
    GetObs().incrementCounter("disk_recovery.runs_completed");

    std::string summary = "Recovered " + std::to_string(finalStats.goodSectors) +
                          " sectors, " + std::to_string(finalStats.badSectors) +
                          " bad, " + std::to_string(static_cast<int>(finalStats.ProgressPercent())) +
                          "% complete in " + std::to_string(static_cast<int>(elapsed)) + "s";

    return RecoveryResult::ok(summary);
}

// ---------------------------------------------------------------------------
// Abort
// ---------------------------------------------------------------------------

void DiskRecoveryAsmAgent::Abort() {
    if (m_ctx) {
        GetObs().logWarn(kComponent, "Abort signal sent to recovery worker");
        GetObs().incrementCounter("disk_recovery.aborts");
        DiskRecovery_Abort(m_ctx);
    }
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

RecoveryStats DiskRecoveryAsmAgent::GetStats() const {
    RecoveryStats stats = {};
    if (m_ctx) {
        DiskRecovery_GetStats(m_ctx,
                              &stats.goodSectors,
                              &stats.badSectors,
                              &stats.currentLBA,
                              &stats.totalSectors);
    }
    return stats;
}
