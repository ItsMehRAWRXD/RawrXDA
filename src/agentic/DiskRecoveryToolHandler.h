// =============================================================================
// DiskRecoveryToolHandler.h — Agentic Tool Handler for Disk Recovery Agent
// =============================================================================
// Bridges the DiskRecoveryAgent into the RawrXD Agentic Tool Registry.
// Exposes recovery operations as LLM-callable tools via the X-Macro system.
//
// Tools:
//   disk_recovery_scan    — Scan for dying USB bridge drives
//   disk_recovery_probe   — Probe a specific PhysicalDrive for WD signature
//   disk_recovery_start   — Start full imaging with SCSI hammer protocol
//   disk_recovery_status  — Get recovery progress and statistics
//   disk_recovery_pause   — Pause/resume recovery
//   disk_recovery_abort   — Abort recovery session
//   disk_recovery_key     — Extract AES encryption key from bridge EEPROM
//   disk_recovery_badmap  — Export bad sector map
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// =============================================================================

#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "../agent/DiskRecoveryAgent.h"
#include "ToolRegistry.h"

namespace RawrXD {
namespace Agent {

// ============================================================================
// DiskRecoveryToolHandler — Static dispatch for recovery tools
// ============================================================================
class DiskRecoveryToolHandler {
public:
    // ---- Initialization ----
    // Call once at startup to register all disk recovery tools
    static void RegisterTools(AgentToolRegistry& registry);

    // ---- Tool Implementations (match ToolHandler signature) ----
    static ToolExecResult HandleScan(const nlohmann::json& args);
    static ToolExecResult HandleProbe(const nlohmann::json& args);
    static ToolExecResult HandleStart(const nlohmann::json& args);
    static ToolExecResult HandleStatus(const nlohmann::json& args);
    static ToolExecResult HandlePause(const nlohmann::json& args);
    static ToolExecResult HandleAbort(const nlohmann::json& args);
    static ToolExecResult HandleKeyExtract(const nlohmann::json& args);
    static ToolExecResult HandleBadMapExport(const nlohmann::json& args);

    // ---- Schema generation ----
    static nlohmann::json GetToolSchemas();

private:
    // Singleton recovery agent instance
    static Recovery::DiskRecoveryAgent& GetAgent();
};

} // namespace Agent
} // namespace RawrXD
