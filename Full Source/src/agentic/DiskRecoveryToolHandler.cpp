// =============================================================================
// DiskRecoveryToolHandler.cpp — Agentic Tool Handler for Disk Recovery Agent
// =============================================================================
// Bridges the DiskRecoveryAgent into the RawrXD Agentic Tool Registry.
// Each static method implements one LLM-callable tool with structured
// PatchResult-style returns. No exceptions. No std::function in hot path.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "DiskRecoveryToolHandler.h"
#include <sstream>
#include <iomanip>
#include <chrono>

using RawrXD::Agent::DiskRecoveryToolHandler;
using RawrXD::Agent::ToolExecResult;
using RawrXD::Agent::AgentToolRegistry;
using RawrXD::Recovery::DiskRecoveryAgent;
using RawrXD::Recovery::RecoveryConfig;
using RawrXD::Recovery::RecoveryStats;
using RawrXD::Recovery::RecoveryState;
using RawrXD::Recovery::BridgeType;
using RawrXD::Recovery::DriveInfo;

// ---------------------------------------------------------------------------
// Singleton agent instance
// ---------------------------------------------------------------------------
DiskRecoveryAgent& DiskRecoveryToolHandler::GetAgent() {
    static DiskRecoveryAgent instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Helper: state to string
// ---------------------------------------------------------------------------
static const char* StateToString(RecoveryState state) {
    switch (state) {
        case RecoveryState::Idle:           return "idle";
        case RecoveryState::Scanning:       return "scanning";
        case RecoveryState::Initializing:   return "initializing";
        case RecoveryState::ExtractingKey:  return "extracting_key";
        case RecoveryState::Imaging:        return "imaging";
        case RecoveryState::Paused:         return "paused";
        case RecoveryState::Completed:      return "completed";
        case RecoveryState::Failed:         return "failed";
        case RecoveryState::Aborted:        return "aborted";
        default:                            return "unknown";
    }
}

static const char* BridgeToString(BridgeType bt) {
    switch (bt) {
        case BridgeType::JMS567:    return "JMicron JMS567";
        case BridgeType::NS1066:    return "Norelsys NS1066";
        case BridgeType::ASM1153E:  return "ASMedia ASM1153E";
        case BridgeType::VL716:     return "VIA VL716";
        default:                    return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// RegisterTools — Wire all disk recovery tools into the X-Macro registry
// ---------------------------------------------------------------------------
void DiskRecoveryToolHandler::RegisterTools(AgentToolRegistry& registry) {
    // The X-Macro registry uses function pointers (ToolHandler = ToolExecResult(*)(const json&)),
    // but our methods match that signature since they're static.

    // We register using lambdas that adapt to the ToolHandler signature,
    // since RegisterHandler takes std::function-compatible callables.
    registry.RegisterHandler("disk_recovery_scan",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleScan(args);
        });

    registry.RegisterHandler("disk_recovery_probe",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleProbe(args);
        });

    registry.RegisterHandler("disk_recovery_start",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleStart(args);
        });

    registry.RegisterHandler("disk_recovery_status",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleStatus(args);
        });

    registry.RegisterHandler("disk_recovery_pause",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandlePause(args);
        });

    registry.RegisterHandler("disk_recovery_abort",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleAbort(args);
        });

    registry.RegisterHandler("disk_recovery_key",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleKeyExtract(args);
        });

    registry.RegisterHandler("disk_recovery_badmap",
        [](const nlohmann::json& args) -> ToolExecResult {
            return DiskRecoveryToolHandler::HandleBadMapExport(args);
        });
}

// ---------------------------------------------------------------------------
// HandleScan — Scan PhysicalDrive0-15 for dying WD My Book devices
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleScan(const nlohmann::json& /*args*/) {
    auto& agent = GetAgent();

    std::vector<DriveInfo> drives;
    PatchResult result = agent.ScanForDyingDrives(drives);

    if (!result.success) {
        return ToolExecResult::error(std::string("Scan failed: ") + result.detail);
    }

    // Build JSON response
    nlohmann::json response;
    response["drives_found"] = drives.size();
    response["drives"] = nlohmann::json::array();

    for (const auto& d : drives) {
        nlohmann::json drv;
        drv["drive_number"] = d.driveNumber;
        drv["vendor"] = d.vendorId;
        drv["product"] = d.productId;
        drv["bridge_type"] = BridgeToString(d.bridgeType);
        drv["total_sectors"] = d.totalSectors;
        drv["sector_size"] = d.sectorSize;
        drv["encrypted"] = d.isEncrypted;

        // Human-readable size
        double gb = static_cast<double>(d.totalBytes) / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << gb << " GB";
        drv["size"] = oss.str();

        response["drives"].push_back(drv);
    }

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// HandleProbe — Probe a specific PhysicalDrive for WD bridge signature
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleProbe(const nlohmann::json& args) {
    int driveNum = args.value("drive_number", -1);
    if (driveNum < 0 || driveNum > 15) {
        return ToolExecResult::error("Invalid drive_number (must be 0-15)");
    }

    auto& agent = GetAgent();
    DriveInfo info{};
    PatchResult result = agent.ProbeDrive(driveNum, info);

    if (!result.success) {
        return ToolExecResult::error(std::string("Probe failed: ") + result.detail);
    }

    nlohmann::json response;
    response["drive_number"] = info.driveNumber;
    response["vendor"] = info.vendorId;
    response["product"] = info.productId;
    response["bridge_type"] = BridgeToString(info.bridgeType);
    response["total_sectors"] = info.totalSectors;
    response["sector_size"] = info.sectorSize;
    response["encrypted"] = info.isEncrypted;

    double gb = static_cast<double>(info.totalBytes) / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << gb << " GB";
    response["size"] = oss.str();

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// HandleStart — Start full imaging session
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleStart(const nlohmann::json& args) {
    auto& agent = GetAgent();

    // Check not already running
    RecoveryState state = agent.GetState();
    if (state == RecoveryState::Imaging || state == RecoveryState::Scanning) {
        return ToolExecResult::error("Recovery already in progress (state: " +
                                     std::string(StateToString(state)) + ")");
    }

    // Parse config overrides
    RecoveryConfig config = agent.GetConfig();

    if (args.contains("drive_number")) {
        config.targetDriveNumber = args["drive_number"].get<int>();
    }
    if (args.contains("output_dir")) {
        config.outputDir = args["output_dir"].get<std::string>();
    }
    if (args.contains("max_retries")) {
        config.maxRetries = args["max_retries"].get<int>();
    }
    if (args.contains("timeout_ms")) {
        config.timeoutMs = args["timeout_ms"].get<uint32_t>();
    }
    if (args.contains("sector_size")) {
        config.sectorSize = args["sector_size"].get<uint32_t>();
    }
    if (args.contains("extract_key")) {
        config.extractKey = args["extract_key"].get<bool>();
    }
    if (args.contains("sparse_image")) {
        config.sparseImage = args["sparse_image"].get<bool>();
    }

    agent.SetConfig(config);

    int driveNum = config.targetDriveNumber;
    if (driveNum < 0) {
        return ToolExecResult::error("No drive_number specified and auto-detect not configured. "
                                     "Run disk_recovery_scan first.");
    }

    // Start async — tool call returns immediately
    agent.StartRecoveryAsync(
        driveNum,
        nullptr,  // No per-event callback from tool context
        nullptr   // No completion callback — poll via disk_recovery_status
    );

    nlohmann::json response;
    response["status"] = "started";
    response["drive_number"] = driveNum;
    response["output_dir"] = config.outputDir;
    response["message"] = "Recovery started on PhysicalDrive" + std::to_string(driveNum) +
                          ". Use disk_recovery_status to monitor progress.";

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// HandleStatus — Get live recovery progress and statistics
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleStatus(const nlohmann::json& /*args*/) {
    auto& agent = GetAgent();
    const RecoveryStats& stats = agent.GetStats();
    RecoveryState state = agent.GetState();

    nlohmann::json response;
    response["state"] = StateToString(state);
    response["current_lba"] = stats.currentLBA.load();
    response["total_sectors"] = stats.totalSectors.load();
    response["good_sectors"] = stats.goodSectors.load();
    response["bad_sectors"] = stats.badSectors.load();
    response["sectors_processed"] = stats.sectorsProcessed.load();
    response["retries_total"] = stats.retriesTotal.load();
    response["bytes_written"] = stats.bytesWritten.load();
    response["percent_complete"] = stats.percentComplete.load();
    response["elapsed_seconds"] = stats.elapsedSeconds;
    response["throughput_mbps"] = stats.throughputMBps;

    // Human-readable summary
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "State: " << StateToString(state)
        << " | Progress: " << stats.percentComplete.load() << "%"
        << " | Good: " << stats.goodSectors.load()
        << " | Bad: " << stats.badSectors.load()
        << " | Speed: " << stats.throughputMBps << " MB/s";
    response["summary"] = oss.str();

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// HandlePause — Toggle pause/resume on active recovery
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandlePause(const nlohmann::json& args) {
    auto& agent = GetAgent();
    RecoveryState state = agent.GetState();

    bool resume = args.value("resume", false);

    if (resume) {
        if (state != RecoveryState::Paused) {
            return ToolExecResult::error("Recovery is not paused (state: " +
                                         std::string(StateToString(state)) + ")");
        }
        agent.Resume();
        return ToolExecResult::ok("{\"status\": \"resumed\"}");
    } else {
        if (state != RecoveryState::Imaging) {
            return ToolExecResult::error("Recovery is not running (state: " +
                                         std::string(StateToString(state)) + ")");
        }
        agent.Pause();
        return ToolExecResult::ok("{\"status\": \"paused\"}");
    }
}

// ---------------------------------------------------------------------------
// HandleAbort — Thread-safe abort of active recovery
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleAbort(const nlohmann::json& /*args*/) {
    auto& agent = GetAgent();
    RecoveryState state = agent.GetState();

    if (state != RecoveryState::Imaging && state != RecoveryState::Paused &&
        state != RecoveryState::Scanning) {
        return ToolExecResult::error("No active recovery to abort (state: " +
                                     std::string(StateToString(state)) + ")");
    }

    agent.Abort();

    return ToolExecResult::ok("{\"status\": \"abort_requested\", \"message\": "
                              "\"Abort signal sent. Recovery will stop at next checkpoint.\"}");
}

// ---------------------------------------------------------------------------
// HandleKeyExtract — Extract AES-256 key from bridge EEPROM
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleKeyExtract(const nlohmann::json& args) {
    int driveNum = args.value("drive_number", -1);
    if (driveNum < 0 || driveNum > 15) {
        return ToolExecResult::error("Invalid drive_number (must be 0-15)");
    }

    auto& agent = GetAgent();
    uint8_t key[32];
    PatchResult result = agent.ExtractEncryptionKey(driveNum, key);

    if (!result.success) {
        return ToolExecResult::error(std::string("Key extraction failed: ") + result.detail);
    }

    // Format key as hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < 32; ++i) {
        oss << std::setw(2) << static_cast<int>(key[i]);
    }

    nlohmann::json response;
    response["success"] = true;
    response["key_hex"] = oss.str();
    response["key_length"] = 32;
    response["algorithm"] = "AES-256";
    response["message"] = "Hardware encryption key extracted from bridge EEPROM. "
                          "Use with disk decryptor to access encrypted partitions.";

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// HandleBadMapExport — Export current bad sector list to file
// ---------------------------------------------------------------------------
ToolExecResult DiskRecoveryToolHandler::HandleBadMapExport(const nlohmann::json& args) {
    std::string path = args.value("path", "D:\\Recovery\\bad_sectors_export.txt");

    auto& agent = GetAgent();
    PatchResult result = agent.ExportBadSectorMap(path);

    if (!result.success) {
        return ToolExecResult::error(std::string("Export failed: ") + result.detail);
    }

    auto badList = agent.GetBadSectorList();

    nlohmann::json response;
    response["exported_to"] = path;
    response["bad_sector_count"] = badList.size();
    response["message"] = "Bad sector map exported with " +
                          std::to_string(badList.size()) + " entries.";

    return ToolExecResult::ok(response.dump(2));
}

// ---------------------------------------------------------------------------
// GetToolSchemas — OpenAI function-calling schemas for all disk recovery tools
// ---------------------------------------------------------------------------
nlohmann::json DiskRecoveryToolHandler::GetToolSchemas() {
    nlohmann::json schemas = nlohmann::json::array();

    // disk_recovery_scan
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_scan"},
            {"description", "Scan PhysicalDrive0-15 for dying WD My Book / USB bridge devices. "
                            "Detects JMS567, NS1066, and other USB-SATA bridge controllers."},
            {"parameters", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        }}
    });

    // disk_recovery_probe
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_probe"},
            {"description", "Probe a specific PhysicalDrive number for WD bridge signature, "
                            "capacity, and encryption status."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"drive_number", {
                        {"type", "integer"},
                        {"description", "Physical drive number (0-15)"},
                        {"minimum", 0},
                        {"maximum", 15}
                    }}
                }},
                {"required", {"drive_number"}}
            }}
        }}
    });

    // disk_recovery_start
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_start"},
            {"description", "Start a full disk imaging session with SCSI hammer protocol. "
                            "Creates a raw image file, bad sector map, and checkpoint file. "
                            "Runs asynchronously — poll with disk_recovery_status."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"drive_number", {
                        {"type", "integer"},
                        {"description", "Physical drive number to image (0-15)"},
                        {"minimum", 0},
                        {"maximum", 15}
                    }},
                    {"output_dir", {
                        {"type", "string"},
                        {"description", "Output directory for image, log, and map files"},
                        {"default", "D:\\Recovery"}
                    }},
                    {"max_retries", {
                        {"type", "integer"},
                        {"description", "Maximum retry attempts per sector"},
                        {"default", 100}
                    }},
                    {"timeout_ms", {
                        {"type", "integer"},
                        {"description", "SCSI command timeout in milliseconds"},
                        {"default", 2000}
                    }},
                    {"sector_size", {
                        {"type", "integer"},
                        {"description", "Sector size in bytes (512 or 4096)"},
                        {"default", 4096}
                    }},
                    {"extract_key", {
                        {"type", "boolean"},
                        {"description", "Attempt AES key extraction from bridge EEPROM before imaging"},
                        {"default", true}
                    }},
                    {"sparse_image", {
                        {"type", "boolean"},
                        {"description", "Use sparse file for image (bad sectors become holes)"},
                        {"default", true}
                    }}
                }},
                {"required", {"drive_number"}}
            }}
        }}
    });

    // disk_recovery_status
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_status"},
            {"description", "Get live recovery progress: state, sector counts, speed, percentage."},
            {"parameters", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        }}
    });

    // disk_recovery_pause
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_pause"},
            {"description", "Pause or resume an active disk recovery session."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"resume", {
                        {"type", "boolean"},
                        {"description", "If true, resume a paused session. If false (default), pause."},
                        {"default", false}
                    }}
                }},
                {"required", nlohmann::json::array()}
            }}
        }}
    });

    // disk_recovery_abort
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_abort"},
            {"description", "Abort an active disk recovery session. Checkpoint is saved for resume."},
            {"parameters", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        }}
    });

    // disk_recovery_key
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_key"},
            {"description", "Extract AES-256 encryption key from WD bridge EEPROM. "
                            "Must be done while bridge controller is still responsive."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"drive_number", {
                        {"type", "integer"},
                        {"description", "Physical drive number with WD bridge (0-15)"},
                        {"minimum", 0},
                        {"maximum", 15}
                    }}
                }},
                {"required", {"drive_number"}}
            }}
        }}
    });

    // disk_recovery_badmap
    schemas.push_back({
        {"type", "function"},
        {"function", {
            {"name", "disk_recovery_badmap"},
            {"description", "Export the current bad sector map to a text file for analysis."},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"path", {
                        {"type", "string"},
                        {"description", "Output file path for the bad sector map"},
                        {"default", "D:\\Recovery\\bad_sectors_export.txt"}
                    }}
                }},
                {"required", nlohmann::json::array()}
            }}
        }}
    });

    return schemas;
}
