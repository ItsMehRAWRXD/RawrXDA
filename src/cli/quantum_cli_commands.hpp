// quantum_cli_commands.hpp — CLI Interface for Quantum Agent Orchestrator
// Provides user-facing commands for multi-model, auto-adjusting, Balance/Max/Auto operation

#pragma once

#include "quantum_agent_orchestrator.hpp"
#include <string>
#include <vector>

namespace RawrXD {
namespace Quantum {
namespace CLI {

// ============================================================================
// CLI Command Handler for Quantum Operations
// ============================================================================

bool handleQuantumCommand(const std::string& input);

// Individual command handlers
void showQuantumHelp();
void showQuantumStatus();
void setQuantumMode(const std::string& mode);  // "auto", "balance", "max"
void setModelCount(int count);                 // 1-99
void setAgentCycleCount(int count);            // 1-99
void executeQuantumTask(const std::string& description);
void auditProduction(const std::string& rootPath);
void executeTopAuditItems(int count);
void showStatistics();
void configureTimeouts(const std::string& mode);  // "auto", "fixed"
void setBypassLimits(bool token, bool complexity, bool time);

// Quantum command syntax:
//   !quantum help                           — Show quantum commands
//   !quantum status                         — Show current configuration
//   !quantum mode <auto|balance|max>        — Set quality mode
//   !quantum models <1-99>                  — Set model count
//   !quantum agents <1-99>                  — Set agent cycle count
//   !quantum exec <description>             — Execute task with current settings
//   !quantum audit <path>                   — Audit codebase
//   !quantum audit-exec <N>                 — Execute top N audit items
//   !quantum stats                          — Show execution statistics
//   !quantum timeout <auto|fixed>           — Configure timeout mode
//   !quantum bypass <token|complexity|time> — Toggle bypass flags

// Example workflows:
//   !quantum mode max
//   !quantum models 8
//   !quantum agents 8
//   !quantum exec "Refactor the model loader architecture for optimal performance"
//
//   !quantum audit D:\rawrxd\src
//   !quantum audit-exec 20

} // namespace CLI
} // namespace Quantum
} // namespace RawrXD
